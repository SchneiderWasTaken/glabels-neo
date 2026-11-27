//  GoogleAuthManager.hpp
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

#ifndef datasource_GoogleAuthManager_hpp
#define datasource_GoogleAuthManager_hpp


#include <QDateTime>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <functional>


class QNetworkReply;
class QTcpServer;
class QTcpSocket;


namespace glabels::datasource
{

        ///
        /// Manages Google OAuth 2.0 authentication (RFC 8252 loopback + PKCE)
        /// and the local Google Picker page for linking private or shared
        /// Google Sheets from any personal Gmail or Google Workspace account.
        ///
        /// Refresh tokens are persisted only in SecretStore (OS credential store).
        /// Short-lived access tokens are kept in memory only.
        ///
        class GoogleAuthManager : public QObject
        {
                Q_OBJECT

        public:
                using TokenCallback = std::function<void( const QString& accessToken, const QString& error )>;
                using UrlOpener = std::function<bool( const QUrl& url )>;

                static const QString REFRESH_TOKEN_KEY;

                explicit GoogleAuthManager( QObject* parent = nullptr );
                ~GoogleAuthManager() override;

                /// Shared application-wide instance.
                static GoogleAuthManager* instance();

                /// Effective OAuth configuration (env override > compile-time Version::GOOGLE_*).
                /// The picker page is always served by the app itself on 127.0.0.1, so
                /// there is no picker URL to configure.
                QString clientId() const;
                QString clientSecret() const;
                QString pickerApiKey() const;
                QString scopes() const;
                QString cloudProjectNumber() const;

                bool isConfigured() const;
                bool isPickerConfigured() const;
                bool isSignedIn() const;
                QString signedInEmail() const;

                /// Start interactive "Sign in with Google" in the user's system browser.
                void signIn();
                void cancelSignIn();

                /// Revoke and remove saved refresh token + cached account info.
                void signOut();

                /// Obtain a fresh valid access token (from memory cache or via refresh_token).
                void ensureAccessToken( TokenCallback callback );
                void invalidateAccessToken();

                /// Serve the Google Picker page from 127.0.0.1 and open it in the user's browser.
                /// The access token is only in that one-time page, never in a URL.
                /// (drive.file grants per-file access to the sheets the user picks.)
                void pickSpreadsheet();
                void cancelPicker();

                /// Test / configuration overrides.
                void setClientCredentialsForTests( const QString& clientId,
                                                   const QString& clientSecret,
                                                   const QString& pickerApiKey = QString() );
                void setEndpointsForTests( const QString& authUrl,
                                           const QString& tokenUrl,
                                           const QString& userinfoUrl,
                                           const QString& revokeUrl );
                void setUrlOpenerForTests( UrlOpener opener );
                void setLoopbackTimeoutForTests( int ms ) { mLoopbackTimeoutMs = ms; }

                /// How long a sign-in or picker listener waits for the browser.
                static constexpr int LOOPBACK_TIMEOUT_MS = 5 * 60 * 1000;

        signals:
                void signedIn( const QString& email );
                void signInFailed( const QString& errorMessage );
                void signedOut();
                void spreadsheetPicked( const QString& spreadsheetId, const QString& title );
                void pickerFailed( const QString& errorMessage );

        private:
                void onOAuthConnection();
                void handleOAuthHttpRequest( QTcpSocket* socket, const QByteArray& raw );
                void exchangeAuthCode( const QString& code, const QString& redirectUri );
                void fetchUserEmail( const QString& accessToken );
                void refreshAccessToken();
                void flushPendingTokenCallbacks( const QString& token, const QString& error );

                void onPickerConnection();
                void handlePickerHttpRequest( QTcpSocket* socket, const QByteArray& raw );
                QByteArray pickerPageHtml( QByteArray& cspOut ) const;

                /// Read one HTTP request header from a loopback socket (size-capped,
                /// 10 s per socket) and hand it to `handler` exactly once.
                void readLoopbackRequest( QTcpSocket* socket,
                                          std::function<void( QTcpSocket*, const QByteArray& )> handler );

                /// Parse "GET <path> HTTP/1.1" and check the Host header names this
                /// listener (blocks DNS-rebinding pages).  Returns false on anything else.
                static bool parseLoopbackRequest( const QByteArray& raw,
                                                  quint16           port,
                                                  QUrl&             targetOut );

                static void writeHtmlResponse( QTcpSocket*    socket,
                                               int            statusCode,
                                               const QString& title,
                                               const QString& body );
                static void writeRawResponse( QTcpSocket*       socket,
                                              int               statusCode,
                                              const QByteArray& contentType,
                                              const QByteArray& payload,
                                              const QByteArray& extraHeaders = QByteArray() );
                static QString randomBase64Url( int byteCount );
                static QString pkceChallengeS256( const QString& verifier );

        private:
                QNetworkAccessManager    mNet;
                QPointer<QTcpServer>     mOAuthServer;
                QPointer<QTcpServer>     mPickerServer;
                QPointer<QNetworkReply>  mTokenReply;

                QString                  mPendingState;
                QString                  mPendingVerifier;
                QString                  mPendingRedirectUri;
                QString                  mPendingPickerState;   // posted back with the result
                QString                  mPendingPickerNonce;   // one-time key for GET /picker
                QString                  mPendingPickerToken;   // embedded once in the page
                bool                     mPickerPageServed{ false };
                QPointer<QTimer>         mOAuthTimeout;
                QPointer<QTimer>         mPickerTimeout;
                int                      mLoopbackTimeoutMs{ LOOPBACK_TIMEOUT_MS };

                QString                  mAccessToken;
                QDateTime                mAccessTokenExpiresAt;
                QString                  mSignedInEmail;
                bool                     mRefreshingToken{ false };
                QList<TokenCallback>     mPendingTokenCallbacks;

                // Optional test/runtime overrides
                QString                  mClientIdOverride;
                QString                  mClientSecretOverride;
                QString                  mPickerApiKeyOverride;
                QString                  mAuthEndpoint;
                QString                  mTokenEndpoint;
                QString                  mUserinfoEndpoint;
                QString                  mRevokeEndpoint;
                UrlOpener                mUrlOpener;
        };

}


#endif // datasource_GoogleAuthManager_hpp
