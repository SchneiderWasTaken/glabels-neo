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
                // drive.file is a non-sensitive Google OAuth scope that works for ANY
                // Google user (personal Gmail or Workspace) without CASA auditing when
                // paired with Google Picker.  It covers only the files the user picks
                // (Google offers no read-only variant; the app only ever reads).
                // Fixed in code: no environment override can widen it.
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
                return isConfigured() && !pickerApiKey().isEmpty();
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
                                                              const QString& pickerApiKey )
        {
                mClientIdOverride = clientId;
                mClientSecretOverride = clientSecret;
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

                mOAuthTimeout = new QTimer( this );
                mOAuthTimeout->setSingleShot( true );
                connect( mOAuthTimeout, &QTimer::timeout, this, [this]() {
                        cancelSignIn();
                        emit signInFailed( tr( "Google Sign-In timed out. Click \"Sign in with Google\" to try again." ) );
                } );
                mOAuthTimeout->start( mLoopbackTimeoutMs );

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
                if ( mOAuthTimeout )
                {
                        mOAuthTimeout->stop();
                        mOAuthTimeout->deleteLater();
                        mOAuthTimeout = nullptr;
                }
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
                        readLoopbackRequest( mOAuthServer->nextPendingConnection(),
                                             [this]( QTcpSocket* s, const QByteArray& raw ) {
                                                     handleOAuthHttpRequest( s, raw );
                                             } );
                }
        }


        void GoogleAuthManager::readLoopbackRequest(
                QTcpSocket* socket,
                std::function<void( QTcpSocket*, const QByteArray& )> handler )
        {
                static constexpr int MAX_HEADER_BYTES = 16 * 1024;
                static constexpr int SOCKET_TIMEOUT_MS = 10000;

                // Reparent: the server may be deleted right after it answers, and
                // the reply still has to reach the browser.
                socket->setParent( this );
                connect( socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater );
                QTimer::singleShot( SOCKET_TIMEOUT_MS, socket, [socket]() {
                        socket->abort();
                        socket->deleteLater();
                } );

                connect( socket, &QTcpSocket::readyRead, this, [socket, handler]() {
                        if ( socket->property( "handled" ).toBool() )
                        {
                                socket->readAll(); // one request per connection
                                return;
                        }
                        const QByteArray buf = socket->property( "buf" ).toByteArray() + socket->readAll();
                        if ( buf.size() > MAX_HEADER_BYTES )
                        {
                                socket->setProperty( "handled", true );
                                writeHtmlResponse( socket, 431, tr( "Request too large" ), tr( "Request header too large." ) );
                                return;
                        }
                        socket->setProperty( "buf", buf );
                        if ( buf.contains( "\r\n\r\n" ) )
                        {
                                socket->setProperty( "handled", true );
                                handler( socket, buf );
                        }
                } );
        }


        bool GoogleAuthManager::parseLoopbackRequest( const QByteArray& raw, quint16 port, QUrl& targetOut )
        {
                const int headerEnd = raw.indexOf( "\r\n\r\n" );
                const QList<QByteArray> lines = raw.left( headerEnd < 0 ? raw.size() : headerEnd ).split( '\n' );
                if ( lines.isEmpty() )
                {
                        return false;
                }

                const QList<QByteArray> parts = lines.first().trimmed().split( ' ' );
                if ( parts.size() != 3 || parts.at( 0 ) != "GET" || !parts.at( 1 ).startsWith( '/' )
                     || !parts.at( 2 ).startsWith( "HTTP/1." ) )
                {
                        return false;
                }

                // The browser sends "Host: 127.0.0.1:<port>".  A page on another
                // domain that re-points its DNS at 127.0.0.1 sends its own name.
                QByteArray host;
                for ( int i = 1; i < lines.size(); ++i )
                {
                        const QByteArray line = lines.at( i ).trimmed();
                        if ( line.left( 5 ).toLower() == "host:" )
                        {
                                if ( !host.isNull() )
                                {
                                        return false; // duplicate Host header
                                }
                                host = line.mid( 5 ).trimmed();
                        }
                }
                if ( host != "127.0.0.1" && host != "127.0.0.1:" + QByteArray::number( port ) )
                {
                        return false;
                }

                targetOut = QUrl( QStringLiteral( "http://127.0.0.1" ) + QString::fromLatin1( parts.at( 1 ) ) );
                return targetOut.isValid();
        }


        void GoogleAuthManager::handleOAuthHttpRequest( QTcpSocket* socket, const QByteArray& raw )
        {
                QUrl targetUrl;
                if ( !parseLoopbackRequest( raw, socket->localPort(), targetUrl ) )
                {
                        writeHtmlResponse( socket, 400, tr( "Invalid request" ), tr( "Malformed callback request." ) );
                        return;
                }
                if ( targetUrl.path() != QStringLiteral( "/callback" ) )
                {
                        writeHtmlResponse( socket, 404, tr( "Not found" ), tr( "Unknown callback path." ) );
                        return;
                }

                const QUrlQuery q( targetUrl );
                const QString error = q.queryItemValue( QStringLiteral( "error" ), QUrl::FullyDecoded );
                const QString state = q.queryItemValue( QStringLiteral( "state" ), QUrl::FullyDecoded );
                const QString code = q.queryItemValue( QStringLiteral( "code" ), QUrl::FullyDecoded );

                // Check state first.  A request without the right state is not from
                // Google's redirect: refuse it, but leave the real sign-in running.
                if ( mPendingState.isEmpty() || state != mPendingState )
                {
                        writeHtmlResponse( socket, 400,
                                           tr( "Sign-in verification failed" ),
                                           tr( "The sign-in state did not match. Please try again from gLabels Neo." ) );
                        return;
                }

                if ( !error.isEmpty() || code.isEmpty() )
                {
                        writeHtmlResponse( socket, 200,
                                           tr( "Google Sign-In canceled" ),
                                           tr( "You can close this tab and return to gLabels Neo." ) );
                        cancelSignIn();
                        emit signInFailed( tr( "Google Sign-In was canceled or denied (%1)." )
                                                   .arg( error.isEmpty() ? tr( "no code" ) : error ) );
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
                                tr( "Google Picker is not configured in this build (requires GLABELS_GOOGLE_PICKER_API_KEY)." ) );
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

                        mPickerTimeout = new QTimer( this );
                        mPickerTimeout->setSingleShot( true );
                        connect( mPickerTimeout, &QTimer::timeout, this, [this]() {
                                cancelPicker();
                                emit pickerFailed( tr( "Choosing a Google Sheet timed out. Click \"Pick a Google Sheet...\" to try again." ) );
                        } );
                        mPickerTimeout->start( mLoopbackTimeoutMs );

                        // The browser opens our own page on 127.0.0.1.  Its URL carries
                        // only a one-time code; the access token is written into the
                        // page body once and is never part of any URL, so it cannot
                        // land in browser history, process arguments or a web server.
                        mPendingPickerState = randomBase64Url( 24 );
                        mPendingPickerNonce = randomBase64Url( 24 );
                        mPendingPickerToken = token;
                        mPickerPageServed = false;

                        QUrl url( QStringLiteral( "http://127.0.0.1:%1/picker" ).arg( server->serverPort() ) );
                        QUrlQuery q;
                        q.addQueryItem( QStringLiteral( "n" ), mPendingPickerNonce );
                        url.setQuery( q );

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
                if ( mPickerTimeout )
                {
                        mPickerTimeout->stop();
                        mPickerTimeout->deleteLater();
                        mPickerTimeout = nullptr;
                }
                if ( mPickerServer )
                {
                        mPickerServer->close();
                        mPickerServer->deleteLater();
                        mPickerServer = nullptr;
                }
                mPendingPickerState.clear();
                mPendingPickerNonce.clear();
                mPendingPickerToken.clear();
                mPickerPageServed = false;
        }


        void GoogleAuthManager::onPickerConnection()
        {
                while ( mPickerServer && mPickerServer->hasPendingConnections() )
                {
                        readLoopbackRequest( mPickerServer->nextPendingConnection(),
                                             [this]( QTcpSocket* s, const QByteArray& raw ) {
                                                     handlePickerHttpRequest( s, raw );
                                             } );
                }
        }


        void GoogleAuthManager::handlePickerHttpRequest( QTcpSocket* socket, const QByteArray& raw )
        {
                QUrl targetUrl;
                if ( !parseLoopbackRequest( raw, socket->localPort(), targetUrl ) )
                {
                        writeHtmlResponse( socket, 400, tr( "Invalid request" ), tr( "Malformed picker request." ) );
                        return;
                }

                const QUrlQuery q( targetUrl );

                if ( targetUrl.path() == QStringLiteral( "/picker" ) )
                {
                        const QString nonce = q.queryItemValue( QStringLiteral( "n" ), QUrl::FullyDecoded );
                        if ( mPendingPickerNonce.isEmpty() || nonce != mPendingPickerNonce )
                        {
                                writeHtmlResponse( socket, 404, tr( "Link expired" ),
                                                   tr( "This link is no longer valid. Go back to gLabels Neo and click \"Pick a Google Sheet...\"." ) );
                                return;
                        }
                        if ( mPickerPageServed )
                        {
                                writeHtmlResponse( socket, 410, tr( "Already opened" ),
                                                   tr( "This page was already opened. Go back to gLabels Neo and click \"Pick a Google Sheet...\" to try again." ) );
                                return;
                        }
                        mPickerPageServed = true;
                        QByteArray csp;
                        const QByteArray page = pickerPageHtml( csp );
                        mPendingPickerToken.clear(); // served once; nothing else needs it
                        // strict-origin: Google may see "http://127.0.0.1:<port>" but
                        // never the path or the one-time code.
                        writeRawResponse( socket, 200, "text/html; charset=utf-8", page,
                                          "Content-Security-Policy: " + csp + "\r\n"
                                          "Referrer-Policy: strict-origin\r\n" );
                        return;
                }

                if ( targetUrl.path() != QStringLiteral( "/picked" ) )
                {
                        writeHtmlResponse( socket, 404, tr( "Not found" ), tr( "Unknown path." ) );
                        return;
                }

                const QString state = q.queryItemValue( QStringLiteral( "state" ), QUrl::FullyDecoded );
                const QString error = q.queryItemValue( QStringLiteral( "error" ), QUrl::FullyDecoded );
                const QString id = q.queryItemValue( QStringLiteral( "id" ), QUrl::FullyDecoded ).trimmed();
                const QString name = q.queryItemValue( QStringLiteral( "name" ), QUrl::FullyDecoded ).trimmed();

                // Wrong state: not our page.  Refuse it but keep the real picker open.
                if ( mPendingPickerState.isEmpty() || state != mPendingPickerState )
                {
                        writeHtmlResponse( socket, 400,
                                           tr( "Verification failed" ),
                                           tr( "Picker state did not match. Please try again from gLabels Neo." ) );
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


        ///
        /// The Google Picker page, served once from 127.0.0.1.  Settings go in as
        /// a JSON literal ("<" escaped so nothing can close the script tag).
        ///
        QByteArray GoogleAuthManager::pickerPageHtml( QByteArray& cspOut ) const
        {
                QJsonObject cfg;
                cfg.insert( QStringLiteral( "token" ), mPendingPickerToken );
                cfg.insert( QStringLiteral( "apiKey" ), pickerApiKey() );
                cfg.insert( QStringLiteral( "appId" ), cloudProjectNumber() );
                cfg.insert( QStringLiteral( "state" ), mPendingPickerState );
                QByteArray cfgJson = QJsonDocument( cfg ).toJson( QJsonDocument::Compact );
                cfgJson.replace( "<", "\\u003c" );

                const QByteArray nonce = randomBase64Url( 18 ).toLatin1();

                // Scripts: only our inline one and Google's.  No framing.  Google's
                // picker runs in its own docs.google.com frame.
                cspOut = "default-src 'none'; "
                         "script-src 'nonce-" + nonce + "' https://apis.google.com https://*.gstatic.com https://*.google.com; "
                         "style-src 'unsafe-inline' https://*.gstatic.com https://*.google.com; "
                         "img-src https: data:; font-src https://*.gstatic.com; "
                         "connect-src https://*.google.com https://*.googleapis.com; "
                         "frame-src https://*.google.com; "
                         "base-uri 'none'; form-action 'none'; frame-ancestors 'none'";

                QByteArray html =
                        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                        "<meta name=\"robots\" content=\"noindex\">"
                        "<title>Choose a Google Sheet</title>"
                        "<style>body{font-family:system-ui,-apple-system,sans-serif;max-width:560px;"
                        "margin:64px auto;padding:24px;color:#1f2937;line-height:1.5}"
                        "h1{font-size:1.25rem;margin:0 0 12px}button{font:inherit;padding:6px 14px}</style>"
                        "</head><body><h1>Choose a Google Sheet</h1>"
                        "<p id=\"st\">Loading Google Picker&hellip;</p>"
                        "<p><button id=\"again\" type=\"button\" hidden>Open the picker</button></p>"
                        "<script nonce=\"@NONCE@\">"
                        "(function(){'use strict';"
                        "if(window.top!==window.self){return;}"
                        "var cfg=@CFG@;"
                        "var st=document.getElementById('st'),btn=document.getElementById('again'),done=false;"
                        "function sendBack(p){if(done){return;}done=true;cfg.token='';"
                        "var q=new URLSearchParams(p);q.set('state',cfg.state);"
                        "window.location.replace('/picked?'+q.toString());}"
                        "function openPicker(){"
                        "var view=new google.picker.DocsView(google.picker.ViewId.SPREADSHEETS);"
                        "view.setIncludeFolders(true);"
                        "var b=new google.picker.PickerBuilder().addView(view)"
                        ".setOAuthToken(cfg.token).setDeveloperKey(cfg.apiKey)"
                        ".setOrigin(window.location.origin)"
                        ".setCallback(function(d){"
                        "if(d.action===google.picker.Action.PICKED&&d.docs&&d.docs.length){"
                        "var doc=d.docs[0];st.textContent='Linking '+(doc.name||'the sheet')+'\\u2026';"
                        "sendBack({id:doc.id,name:doc.name||''});}"
                        "else if(d.action===google.picker.Action.CANCEL){"
                        "st.textContent='No sheet chosen. You can close this tab.';sendBack({error:'cancelled'});}});"
                        "if(cfg.appId){b.setAppId(cfg.appId);}"
                        "b.build().setVisible(true);}"
                        "btn.onclick=openPicker;"
                        "var s=document.createElement('script');"
                        "s.src='https://apis.google.com/js/api.js';"
                        "s.onload=function(){gapi.load('picker',{callback:function(){"
                        "st.textContent='Choose the Google Sheet to link to gLabels Neo.';"
                        "btn.hidden=false;openPicker();}});};"
                        "s.onerror=function(){st.textContent='Could not load Google Picker. Check your internet connection.';};"
                        "document.head.appendChild(s);"
                        "})();"
                        "</script></body></html>";
                html.replace( "@NONCE@", nonce );
                html.replace( "@CFG@", cfgJson );
                return html;
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
                writeRawResponse( socket, statusCode, "text/html; charset=utf-8", html.toUtf8(),
                                  "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; "
                                  "frame-ancestors 'none'\r\n" );
        }


        void GoogleAuthManager::writeRawResponse( QTcpSocket*       socket,
                                                  int               statusCode,
                                                  const QByteArray& contentType,
                                                  const QByteArray& payload,
                                                  const QByteArray& extraHeaders )
        {
                QByteArray reason;
                switch ( statusCode )
                {
                case 200: reason = "OK"; break;
                case 400: reason = "Bad Request"; break;
                case 404: reason = "Not Found"; break;
                case 410: reason = "Gone"; break;
                case 431: reason = "Request Header Fields Too Large"; break;
                default:  reason = "Error"; break;
                }
                const QByteArray response =
                        "HTTP/1.1 " + QByteArray::number( statusCode ) + " " + reason + "\r\n"
                        "Content-Type: " + contentType + "\r\n"
                        "Content-Length: " + QByteArray::number( payload.size() ) + "\r\n"
                        "Cache-Control: no-store\r\n"
                        "X-Content-Type-Options: nosniff\r\n"
                        "X-Frame-Options: DENY\r\n"
                        + ( extraHeaders.contains( "Referrer-Policy:" )
                                    ? QByteArray()
                                    : QByteArray( "Referrer-Policy: no-referrer\r\n" ) )
                        + extraHeaders
                        + "Connection: close\r\n\r\n" + payload;
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
