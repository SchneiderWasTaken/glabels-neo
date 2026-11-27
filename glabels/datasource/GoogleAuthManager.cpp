//  GoogleAuthManager.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels Neo, a modified version of gLabels-qt.
//
//  gLabels Neo is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels Neo is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels Neo.  If not, see <http://www.gnu.org/licenses/>.
//


#include "GoogleAuthManager.hpp"

#include "SecretStore.hpp"

#include "model/Settings.hpp"
#include "model/Version.hpp"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>


namespace glabels::datasource
{

        const QString GoogleAuthManager::REFRESH_TOKEN_KEY = QStringLiteral( "google:oauth:refresh_token" );

        namespace
        {
                const int HTTP_TIMEOUT_MS = 25000;

                QString envOrBuild( const char* envName, const QString& buildValue )
                {
                        const QString fromEnv = qEnvironmentVariable( envName ).trimmed();
                        return fromEnv.isEmpty() ? buildValue.trimmed() : fromEnv;
                }

                QString extractOAuthError( const QByteArray& body )
                {
                        const QJsonDocument doc = QJsonDocument::fromJson( body );
                        if ( !doc.isObject() )
                        {
                                return {};
                        }
                        const QJsonObject obj = doc.object();
                        const QString desc = obj.value( QStringLiteral( "error_description" ) ).toString().trimmed();
                        const QString err = obj.value( QStringLiteral( "error" ) ).toString().trimmed();
                        if ( !desc.isEmpty() )
                        {
                                return desc;
                        }
                        return err;
                }
        }


        GoogleAuthManager::GoogleAuthManager( QObject* parent )
                : QObject( parent ),
                  mAuthEndpoint( QStringLiteral( "https://accounts.google.com/o/oauth2/v2/auth" ) ),
                  mTokenEndpoint( QStringLiteral( "https://oauth2.googleapis.com/token" ) ),
                  mUserinfoEndpoint( QStringLiteral( "https://openidconnect.googleapis.com/v1/userinfo" ) ),
                  mRevokeEndpoint( QStringLiteral( "https://oauth2.googleapis.com/revoke" ) )
        {
        }


        GoogleAuthManager::~GoogleAuthManager()
        {
                cancelSignIn();
                cancelPicker();
                if ( mTokenReply )
                {
                        mTokenReply->abort();
                        mTokenReply->deleteLater();
                }
        }


        GoogleAuthManager* GoogleAuthManager::instance()
        {
                static GoogleAuthManager sInstance;
                return &sInstance;
        }


        QString GoogleAuthManager::clientId() const
        {
                if ( !mClientIdOverride.isEmpty() )
                {
                        return mClientIdOverride;
                }
                return envOrBuild( "GLABELS_GOOGLE_CLIENT_ID", model::Version::GOOGLE_CLIENT_ID );
        }


        QString GoogleAuthManager::clientSecret() const
        {
                if ( !mClientSecretOverride.isEmpty() )
                {
                        return mClientSecretOverride;
                }
                return envOrBuild( "GLABELS_GOOGLE_CLIENT_SECRET", model::Version::GOOGLE_CLIENT_SECRET );
        }


        QString GoogleAuthManager::pickerUrl() const
        {
                if ( !mPickerUrlOverride.isEmpty() )
                {
                        return mPickerUrlOverride;
                }
                return envOrBuild( "GLABELS_GOOGLE_PICKER_URL", model::Version::GOOGLE_PICKER_URL );
        }


        QString GoogleAuthManager::pickerApiKey() const
        {
                if ( !mPickerApiKeyOverride.isEmpty() )
                {
                        return mPickerApiKeyOverride;
                }
                return envOrBuild( "GLABELS_GOOGLE_PICKER_API_KEY", model::Version::GOOGLE_PICKER_API_KEY );
        }


        QString GoogleAuthManager::scopes() const
        {
                const QString envScopes = qEnvironmentVariable( "GLABELS_GOOGLE_SCOPES" ).trimmed();
                if ( !envScopes.isEmpty() )
                {
                        return envScopes;
                }
                // drive.file is a non-sensitive Google OAuth scope that works for ANY
                // Google user (personal Gmail or Workspace) without CASA auditing when
                // paired with Google Picker.
                return QStringLiteral( "openid email https://www.googleapis.com/auth/drive.file" );
        }


        QString GoogleAuthManager::cloudProjectNumber() const
        {
                // Google Cloud OAuth Client IDs have the form:
                //   <projectNumber>-<random>.apps.googleusercontent.com
                const QString cid = clientId();
                const QString prefix = cid.section( '-', 0, 0 );
                bool ok = false;
                prefix.toULongLong( &ok );
                return ok ? prefix : QString();
        }


        bool GoogleAuthManager::isConfigured() const
        {
                return !clientId().isEmpty();
        }


        bool GoogleAuthManager::isPickerConfigured() const
        {
                return isConfigured() && !pickerUrl().isEmpty() && !pickerApiKey().isEmpty();
        }


        bool GoogleAuthManager::isSignedIn() const
        {
                return SecretStore::hasSecret( REFRESH_TOKEN_KEY );
        }


        QString GoogleAuthManager::signedInEmail() const
        {
                if ( !mSignedInEmail.isEmpty() )
                {
                        return mSignedInEmail;
                }
                return model::Settings::googleAccountEmail();
        }


        void GoogleAuthManager::setClientCredentialsForTests( const QString& clientId,
                                                              const QString& clientSecret,
                                                              const QString& pickerUrl,
                                                              const QString& pickerApiKey )
        {
                mClientIdOverride = clientId;
                mClientSecretOverride = clientSecret;
                mPickerUrlOverride = pickerUrl;
                mPickerApiKeyOverride = pickerApiKey;
        }


        void GoogleAuthManager::setEndpointsForTests( const QString& authUrl,
                                                      const QString& tokenUrl,
                                                      const QString& userinfoUrl,
                                                      const QString& revokeUrl )
        {
                mAuthEndpoint = authUrl;
                mTokenEndpoint = tokenUrl;
                mUserinfoEndpoint = userinfoUrl;
                mRevokeEndpoint = revokeUrl;
        }


        void GoogleAuthManager::setUrlOpenerForTests( UrlOpener opener )
        {
                mUrlOpener = std::move( opener );
        }


        void GoogleAuthManager::signIn()
        {
                if ( !isConfigured() )
                {
                        emit signInFailed(
                                tr( "Google Sign-In is not configured in this build. "
                                    "Set GLABELS_GOOGLE_CLIENT_ID and GLABELS_GOOGLE_CLIENT_SECRET "
                                    "or use Public Link mode for link-shared sheets." ) );
                        return;
                }

                cancelSignIn();

                auto* server = new QTcpServer( this );
                if ( !server->listen( QHostAddress::LocalHost, 0 ) )
                {
                        const QString err = server->errorString();
                        server->deleteLater();
                        emit signInFailed( tr( "Could not start local sign-in callback listener: %1" ).arg( err ) );
                        return;
                }

                mOAuthServer = server;
                connect( server, &QTcpServer::newConnection, this, &GoogleAuthManager::onOAuthConnection );

                mPendingState = randomBase64Url( 24 );
                mPendingVerifier = randomBase64Url( 32 );
                mPendingRedirectUri = QStringLiteral( "http://127.0.0.1:%1/callback" ).arg( server->serverPort() );

                QUrl authUrl( mAuthEndpoint );
                QUrlQuery q;
                q.addQueryItem( QStringLiteral( "client_id" ), clientId() );
                q.addQueryItem( QStringLiteral( "redirect_uri" ), mPendingRedirectUri );
                q.addQueryItem( QStringLiteral( "response_type" ), QStringLiteral( "code" ) );
                q.addQueryItem( QStringLiteral( "scope" ), scopes() );
                q.addQueryItem( QStringLiteral( "state" ), mPendingState );
                q.addQueryItem( QStringLiteral( "code_challenge" ), pkceChallengeS256( mPendingVerifier ) );
                q.addQueryItem( QStringLiteral( "code_challenge_method" ), QStringLiteral( "S256" ) );
                q.addQueryItem( QStringLiteral( "access_type" ), QStringLiteral( "offline" ) );
                q.addQueryItem( QStringLiteral( "prompt" ), QStringLiteral( "consent" ) );
                authUrl.setQuery( q );

                const bool opened = mUrlOpener
                        ? mUrlOpener( authUrl )
                        : QDesktopServices::openUrl( authUrl );
                if ( !opened )
                {
                        cancelSignIn();
                        emit signInFailed( tr( "Could not open your web browser for Google Sign-In." ) );
                }
        }


        void GoogleAuthManager::cancelSignIn()
        {
                if ( mOAuthServer )
                {
                        mOAuthServer->close();
                        mOAuthServer->deleteLater();
                        mOAuthServer = nullptr;
                }
                mPendingState.clear();
                mPendingVerifier.clear();
                mPendingRedirectUri.clear();
        }


        void GoogleAuthManager::onOAuthConnection()
        {
                while ( mOAuthServer && mOAuthServer->hasPendingConnections() )
                {
                        QTcpSocket* socket = mOAuthServer->nextPendingConnection();
                        socket->setParent( this );
                        connect( socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater );
                        connect( socket, &QTcpSocket::readyRead, this, [this, socket]() {
                                const QByteArray buf = socket->property( "buf" ).toByteArray() + socket->readAll();
                                socket->setProperty( "buf", buf );
                                if ( buf.contains( "\r\n\r\n" ) )
                                {
                                        handleOAuthHttpRequest( socket, buf );
                                }
                        } );
                }
        }


        void GoogleAuthManager::handleOAuthHttpRequest( QTcpSocket* socket, const QByteArray& raw )
        {
                const int lineEnd = raw.indexOf( "\r\n" );
                const QString requestLine = QString::fromLatin1( lineEnd >= 0 ? raw.left( lineEnd ) : raw );
                const QStringList parts = requestLine.split( ' ' );
                if ( parts.size() < 2 )
                {
                        writeHtmlResponse( socket, 400, tr( "Invalid request" ), tr( "Malformed callback request." ) );
                        return;
                }

                const QUrl targetUrl( QStringLiteral( "http://127.0.0.1" ) + parts.at( 1 ) );
                if ( targetUrl.path() != QStringLiteral( "/callback" ) )
                {
                        writeHtmlResponse( socket, 404, tr( "Not found" ), tr( "Unknown callback path." ) );
                        return;
                }

                const QUrlQuery q( targetUrl );
                const QString error = q.queryItemValue( QStringLiteral( "error" ), QUrl::FullyDecoded );
                const QString state = q.queryItemValue( QStringLiteral( "state" ), QUrl::FullyDecoded );
                const QString code = q.queryItemValue( QStringLiteral( "code" ), QUrl::FullyDecoded );

                if ( !error.isEmpty() )
                {
                        writeHtmlResponse( socket, 200,
                                           tr( "Google Sign-In canceled" ),
                                           tr( "You can close this tab and return to gLabels Neo." ) );
                        cancelSignIn();
                        emit signInFailed( tr( "Google Sign-In was canceled or denied (%1)." ).arg( error ) );
                        return;
                }

                if ( state.isEmpty() || state != mPendingState || code.isEmpty() )
                {
                        writeHtmlResponse( socket, 400,
                                           tr( "Sign-in verification failed" ),
                                           tr( "The sign-in state did not match. Please try again from gLabels Neo." ) );
                        cancelSignIn();
                        emit signInFailed( tr( "Google Sign-In state verification failed. Please try again." ) );
                        return;
                }

                writeHtmlResponse( socket, 200,
                                   tr( "Signed in to gLabels Neo" ),
                                   tr( "Google Sign-In succeeded. You can close this browser tab and return to gLabels Neo." ) );

                const QString redirectUri = mPendingRedirectUri;
                const QString verifier = mPendingVerifier;
                cancelSignIn();
                mPendingVerifier = verifier;
                exchangeAuthCode( code, redirectUri );
        }


        void GoogleAuthManager::exchangeAuthCode( const QString& code, const QString& redirectUri )
        {
                QNetworkRequest req{ QUrl( mTokenEndpoint ) };
                req.setHeader( QNetworkRequest::UserAgentHeader,
                               QStringLiteral( "glabels-neo/" ) + model::Version::STRING );
                req.setHeader( QNetworkRequest::ContentTypeHeader,
                               QStringLiteral( "application/x-www-form-urlencoded" ) );
                req.setTransferTimeout( HTTP_TIMEOUT_MS );

                QUrlQuery form;
                form.addQueryItem( QStringLiteral( "client_id" ), clientId() );
                if ( !clientSecret().isEmpty() )
                {
                        form.addQueryItem( QStringLiteral( "client_secret" ), clientSecret() );
                }
                form.addQueryItem( QStringLiteral( "code" ), code );
                form.addQueryItem( QStringLiteral( "code_verifier" ), mPendingVerifier );
                form.addQueryItem( QStringLiteral( "grant_type" ), QStringLiteral( "authorization_code" ) );
                form.addQueryItem( QStringLiteral( "redirect_uri" ), redirectUri );
                mPendingVerifier.clear();

                QNetworkReply* reply = mNet.post( req, form.toString( QUrl::FullyEncoded ).toUtf8() );
                connect( reply, &QNetworkReply::finished, this, [this, reply]() {
                        const QByteArray body = reply->readAll();
                        const QNetworkReply::NetworkError err = reply->error();
                        const QString errStr = reply->errorString();
                        reply->deleteLater();

                        if ( err != QNetworkReply::NoError )
                        {
                                const QString detail = extractOAuthError( body );
                                emit signInFailed( detail.isEmpty()
                                                           ? tr( "Google token exchange failed: %1" ).arg( errStr )
                                                           : tr( "Google token exchange failed: %1" ).arg( detail ) );
                                return;
                        }

                        const QJsonObject obj = QJsonDocument::fromJson( body ).object();
                        const QString accessToken = obj.value( QStringLiteral( "access_token" ) ).toString().trimmed();
                        const QString refreshToken = obj.value( QStringLiteral( "refresh_token" ) ).toString().trimmed();
                        const int expiresIn = obj.value( QStringLiteral( "expires_in" ) ).toInt( 3600 );

                        if ( accessToken.isEmpty() )
                        {
                                emit signInFailed( tr( "Google did not return an access token." ) );
                                return;
                        }

                        mAccessToken = accessToken;
                        mAccessTokenExpiresAt = QDateTime::currentDateTimeUtc().addSecs( qMax( 60, expiresIn ) );

                        if ( !refreshToken.isEmpty() )
                        {
                                if ( !SecretStore::store( REFRESH_TOKEN_KEY, refreshToken ) )
                                {
                                        emit signInFailed(
                                                tr( "Signed in, but could not save credentials to %1." )
                                                        .arg( SecretStore::backendDescription() ) );
                                        return;
                                }
                        }

                        fetchUserEmail( accessToken );
                } );
        }


        void GoogleAuthManager::fetchUserEmail( const QString& accessToken )
        {
                QNetworkRequest req{ QUrl( mUserinfoEndpoint ) };
                req.setHeader( QNetworkRequest::UserAgentHeader,
                               QStringLiteral( "glabels-neo/" ) + model::Version::STRING );
                req.setRawHeader( "Authorization", QByteArray( "Bearer " ) + accessToken.toUtf8() );
                req.setTransferTimeout( HTTP_TIMEOUT_MS );

                QNetworkReply* reply = mNet.get( req );
                connect( reply, &QNetworkReply::finished, this, [this, reply]() {
                        const QByteArray body = reply->readAll();
                        reply->deleteLater();

                        QString email;
                        const QJsonDocument doc = QJsonDocument::fromJson( body );
                        if ( doc.isObject() )
                        {
                                email = doc.object().value( QStringLiteral( "email" ) ).toString().trimmed();
                        }
                        mSignedInEmail = email;
                        model::Settings::setGoogleAccountEmail( email );
                        emit signedIn( email );
                } );
        }


        void GoogleAuthManager::signOut()
        {
                cancelSignIn();
                cancelPicker();

                const QString refreshToken = SecretStore::retrieve( REFRESH_TOKEN_KEY );
                if ( !refreshToken.isEmpty() && !mRevokeEndpoint.isEmpty() )
                {
                        QNetworkRequest req{ QUrl( mRevokeEndpoint ) };
                        req.setHeader( QNetworkRequest::ContentTypeHeader,
                                       QStringLiteral( "application/x-www-form-urlencoded" ) );
                        req.setTransferTimeout( 10000 );
                        QUrlQuery form;
                        form.addQueryItem( QStringLiteral( "token" ), refreshToken );
                        QNetworkReply* reply = mNet.post( req, form.toString( QUrl::FullyEncoded ).toUtf8() );
                        connect( reply, &QNetworkReply::finished, reply, &QObject::deleteLater );
                }

                SecretStore::remove( REFRESH_TOKEN_KEY );
                mSignedInEmail.clear();
                model::Settings::setGoogleAccountEmail( QString() );
                mAccessToken.clear();
                mAccessTokenExpiresAt = QDateTime();
                emit signedOut();
        }


        void GoogleAuthManager::invalidateAccessToken()
        {
                mAccessToken.clear();
                mAccessTokenExpiresAt = QDateTime();
        }


        void GoogleAuthManager::ensureAccessToken( TokenCallback callback )
        {
                if ( !callback )
                {
                        return;
                }

                const QDateTime now = QDateTime::currentDateTimeUtc();
                if ( !mAccessToken.isEmpty() && mAccessTokenExpiresAt.isValid() && now.secsTo( mAccessTokenExpiresAt ) > 60 )
                {
                        callback( mAccessToken, QString() );
                        return;
                }

                if ( !isConfigured() )
                {
                        callback( QString(), tr( "Google Sign-In is not configured in this build." ) );
                        return;
                }

                if ( !isSignedIn() )
                {
                        callback( QString(), tr( "Not signed in to Google. Click Configure and sign in with Google." ) );
                        return;
                }

                mPendingTokenCallbacks.append( std::move( callback ) );
                if ( !mRefreshingToken )
                {
                        refreshAccessToken();
                }
        }


        void GoogleAuthManager::refreshAccessToken()
        {
                mRefreshingToken = true;
                const QString refreshToken = SecretStore::retrieve( REFRESH_TOKEN_KEY ).trimmed();
                if ( refreshToken.isEmpty() )
                {
                        flushPendingTokenCallbacks(
                                QString(),
                                tr( "Not signed in to Google. Click Configure and sign in with Google." ) );
                        return;
                }

                QNetworkRequest req{ QUrl( mTokenEndpoint ) };
                req.setHeader( QNetworkRequest::UserAgentHeader,
                               QStringLiteral( "glabels-neo/" ) + model::Version::STRING );
                req.setHeader( QNetworkRequest::ContentTypeHeader,
                               QStringLiteral( "application/x-www-form-urlencoded" ) );
                req.setTransferTimeout( HTTP_TIMEOUT_MS );

                QUrlQuery form;
                form.addQueryItem( QStringLiteral( "client_id" ), clientId() );
                if ( !clientSecret().isEmpty() )
                {
                        form.addQueryItem( QStringLiteral( "client_secret" ), clientSecret() );
                }
                form.addQueryItem( QStringLiteral( "refresh_token" ), refreshToken );
                form.addQueryItem( QStringLiteral( "grant_type" ), QStringLiteral( "refresh_token" ) );

                QNetworkReply* reply = mNet.post( req, form.toString( QUrl::FullyEncoded ).toUtf8() );
                mTokenReply = reply;

                connect( reply, &QNetworkReply::finished, this, [this, reply]() {
                        if ( mTokenReply == reply )
                        {
                                mTokenReply = nullptr;
                        }
                        const QByteArray body = reply->readAll();
                        const QNetworkReply::NetworkError err = reply->error();
                        const QString errStr = reply->errorString();
                        reply->deleteLater();

                        if ( err != QNetworkReply::NoError )
                        {
                                const QJsonObject errObj = QJsonDocument::fromJson( body ).object();
                                const QString errCode = errObj.value( QStringLiteral( "error" ) ).toString();
                                if ( errCode == QStringLiteral( "invalid_grant" ) )
                                {
                                        SecretStore::remove( REFRESH_TOKEN_KEY );
                                        mSignedInEmail.clear();
                                        model::Settings::setGoogleAccountEmail( QString() );
                                        invalidateAccessToken();
                                        emit signedOut();
                                        flushPendingTokenCallbacks(
                                                QString(),
                                                tr( "Your Google session expired or was revoked. Please sign in with Google again." ) );
                                        return;
                                }

                                const QString detail = extractOAuthError( body );
                                flushPendingTokenCallbacks(
                                        QString(),
                                        detail.isEmpty()
                                                ? tr( "Could not refresh Google sign-in token: %1" ).arg( errStr )
                                                : tr( "Could not refresh Google sign-in token: %1" ).arg( detail ) );
                                return;
                        }

                        const QJsonObject obj = QJsonDocument::fromJson( body ).object();
                        const QString token = obj.value( QStringLiteral( "access_token" ) ).toString().trimmed();
                        const int expiresIn = obj.value( QStringLiteral( "expires_in" ) ).toInt( 3600 );
                        const QString newRefresh = obj.value( QStringLiteral( "refresh_token" ) ).toString().trimmed();

                        if ( token.isEmpty() )
                        {
                                flushPendingTokenCallbacks( QString(), tr( "Google token refresh returned no access token." ) );
                                return;
                        }

                        if ( !newRefresh.isEmpty() )
                        {
                                SecretStore::store( REFRESH_TOKEN_KEY, newRefresh );
                        }

                        mAccessToken = token;
                        mAccessTokenExpiresAt = QDateTime::currentDateTimeUtc().addSecs( qMax( 60, expiresIn ) );
                        flushPendingTokenCallbacks( mAccessToken, QString() );
                } );
        }


        void GoogleAuthManager::flushPendingTokenCallbacks( const QString& token, const QString& error )
        {
                mRefreshingToken = false;
                auto callbacks = std::move( mPendingTokenCallbacks );
                mPendingTokenCallbacks.clear();
                for ( auto& cb : callbacks )
                {
                        cb( token, error );
                }
        }


        void GoogleAuthManager::pickSpreadsheet()
        {
                if ( !isPickerConfigured() )
                {
                        emit pickerFailed(
                                tr( "Google Picker is not configured in this build (requires GLABELS_GOOGLE_PICKER_API_KEY and GLABELS_GOOGLE_PICKER_URL)." ) );
                        return;
                }

                ensureAccessToken( [this]( const QString& token, const QString& error ) {
                        if ( !error.isEmpty() || token.isEmpty() )
                        {
                                emit pickerFailed( error.isEmpty() ? tr( "Sign in with Google first." ) : error );
                                return;
                        }

                        cancelPicker();
                        auto* server = new QTcpServer( this );
                        if ( !server->listen( QHostAddress::LocalHost, 0 ) )
                        {
                                const QString err = server->errorString();
                                server->deleteLater();
                                emit pickerFailed( tr( "Could not start local picker listener: %1" ).arg( err ) );
                                return;
                        }

                        mPickerServer = server;
                        connect( server, &QTcpServer::newConnection, this, &GoogleAuthManager::onPickerConnection );

                        mPendingPickerState = randomBase64Url( 24 );
                        const QString callbackUrl = QStringLiteral( "http://127.0.0.1:%1/picked" ).arg( server->serverPort() );

                        // Pass sensitive access_token only in the URL fragment (#) so it is
                        // never transmitted to the static hosting server.
                        QUrlQuery frag;
                        frag.addQueryItem( QStringLiteral( "access_token" ), token );
                        frag.addQueryItem( QStringLiteral( "callback" ), callbackUrl );
                        frag.addQueryItem( QStringLiteral( "state" ), mPendingPickerState );
                        frag.addQueryItem( QStringLiteral( "api_key" ), pickerApiKey() );
                        if ( !cloudProjectNumber().isEmpty() )
                        {
                                frag.addQueryItem( QStringLiteral( "app_id" ), cloudProjectNumber() );
                        }

                        QUrl url( pickerUrl() );
                        url.setFragment( frag.toString( QUrl::FullyEncoded ), QUrl::StrictMode );

                        const bool opened = mUrlOpener
                                ? mUrlOpener( url )
                                : QDesktopServices::openUrl( url );
                        if ( !opened )
                        {
                                cancelPicker();
                                emit pickerFailed( tr( "Could not open your web browser for Google Picker." ) );
                        }
                } );
        }


        void GoogleAuthManager::cancelPicker()
        {
                if ( mPickerServer )
                {
                        mPickerServer->close();
                        mPickerServer->deleteLater();
                        mPickerServer = nullptr;
                }
                mPendingPickerState.clear();
        }


        void GoogleAuthManager::onPickerConnection()
        {
                while ( mPickerServer && mPickerServer->hasPendingConnections() )
                {
                        QTcpSocket* socket = mPickerServer->nextPendingConnection();
                        socket->setParent( this );
                        connect( socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater );
                        connect( socket, &QTcpSocket::readyRead, this, [this, socket]() {
                                const QByteArray buf = socket->property( "buf" ).toByteArray() + socket->readAll();
                                socket->setProperty( "buf", buf );
                                if ( buf.contains( "\r\n\r\n" ) )
                                {
                                        handlePickerHttpRequest( socket, buf );
                                }
                        } );
                }
        }


        void GoogleAuthManager::handlePickerHttpRequest( QTcpSocket* socket, const QByteArray& raw )
        {
                const int lineEnd = raw.indexOf( "\r\n" );
                const QString requestLine = QString::fromLatin1( lineEnd >= 0 ? raw.left( lineEnd ) : raw );
                const QStringList parts = requestLine.split( ' ' );
                if ( parts.size() < 2 )
                {
                        writeHtmlResponse( socket, 400, tr( "Invalid request" ), tr( "Malformed picker callback." ) );
                        return;
                }

                const QUrl targetUrl( QStringLiteral( "http://127.0.0.1" ) + parts.at( 1 ) );
                if ( targetUrl.path() != QStringLiteral( "/picked" ) )
                {
                        writeHtmlResponse( socket, 404, tr( "Not found" ), tr( "Unknown callback path." ) );
                        return;
                }

                const QUrlQuery q( targetUrl );
                const QString state = q.queryItemValue( QStringLiteral( "state" ), QUrl::FullyDecoded );
                const QString error = q.queryItemValue( QStringLiteral( "error" ), QUrl::FullyDecoded );
                const QString id = q.queryItemValue( QStringLiteral( "id" ), QUrl::FullyDecoded ).trimmed();
                const QString name = q.queryItemValue( QStringLiteral( "name" ), QUrl::FullyDecoded ).trimmed();

                if ( state.isEmpty() || state != mPendingPickerState )
                {
                        writeHtmlResponse( socket, 400,
                                           tr( "Verification failed" ),
                                           tr( "Picker state did not match. Please try again from gLabels Neo." ) );
                        cancelPicker();
                        emit pickerFailed( tr( "Google Picker state verification failed." ) );
                        return;
                }

                if ( !error.isEmpty() || id.isEmpty() )
                {
                        writeHtmlResponse( socket, 200,
                                           tr( "No spreadsheet selected" ),
                                           tr( "You can close this tab and return to gLabels Neo." ) );
                        cancelPicker();
                        if ( error != QStringLiteral( "cancelled" ) )
                        {
                                emit pickerFailed( error.isEmpty()
                                                           ? tr( "No Google Sheet was selected." )
                                                           : tr( "Google Picker error: %1" ).arg( error ) );
                        }
                        return;
                }

                writeHtmlResponse( socket, 200,
                                   tr( "Google Sheet selected" ),
                                   tr( "Linked \"%1\". You can close this tab and return to gLabels Neo." )
                                           .arg( name.isEmpty() ? id : name ) );
                cancelPicker();
                emit spreadsheetPicked( id, name );
        }


        void GoogleAuthManager::writeHtmlResponse( QTcpSocket*    socket,
                                                   int            statusCode,
                                                   const QString& title,
                                                   const QString& body )
        {
                const QString html = QStringLiteral(
                        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                        "<title>%1</title>"
                        "<style>body{font-family:system-ui,-apple-system,sans-serif;max-width:480px;"
                        "margin:64px auto;padding:24px;color:#1f2937;line-height:1.5}"
                        "h1{font-size:1.25rem;margin:0 0 12px}</style></head>"
                        "<body><h1>%1</h1><p>%2</p></body></html>" )
                        .arg( title.toHtmlEscaped(), body.toHtmlEscaped() );
                const QByteArray payload = html.toUtf8();
                const QByteArray response =
                        "HTTP/1.1 " + QByteArray::number( statusCode )
                        + " OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
                        + QByteArray::number( payload.size() )
                        + "\r\nConnection: close\r\n\r\n" + payload;
                socket->write( response );
                socket->flush();
                socket->disconnectFromHost();
        }


        QString GoogleAuthManager::randomBase64Url( int byteCount )
        {
                QByteArray bytes( byteCount, Qt::Uninitialized );
                auto* rng = QRandomGenerator::system();
                for ( int i = 0; i < byteCount; ++i )
                {
                        bytes[i] = static_cast<char>( rng->bounded( 256 ) );
                }
                return QString::fromLatin1( bytes.toBase64( QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals ) );
        }


        QString GoogleAuthManager::pkceChallengeS256( const QString& verifier )
        {
                const QByteArray digest = QCryptographicHash::hash( verifier.toLatin1(), QCryptographicHash::Sha256 );
                return QString::fromLatin1( digest.toBase64( QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals ) );
        }

}
