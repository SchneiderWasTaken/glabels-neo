//  TestGoogleSheetsDataSource.cpp
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


#include "TestGoogleSheetsDataSource.hpp"

#include "FillJobModel.hpp"
#include "FillView.hpp"
#include "datasource/GoogleAuthManager.hpp"
#include "datasource/GoogleSheetsClient.hpp"
#include "datasource/SecretStore.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/ExternalDataSource.hpp"
#include "model/Model.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"
#include "model/XmlLabelCreator.hpp"
#include "model/XmlLabelParser.hpp"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>


QTEST_MAIN( TestGoogleSheetsDataSource )


using namespace glabels;
using namespace glabels::datasource;
using namespace glabels::model;


namespace
{

        ///
        /// Local mock Google OAuth + Sheets API v4 + Public CSV server.
        ///
        class MockGoogleServer : public QTcpServer
        {
        public:
                QString    expectedChallenge;
                QString    activeAccessToken{ QStringLiteral( "access-token-1" ) };
                int        tokenExchangeCount{ 0 };
                int        tokenRefreshCount{ 0 };
                int        revokeCount{ 0 };
                bool       failFirstValuesWith401{ false };
                bool       privatePublicSheetHtml{ false };
                QByteArray csvBody;
                QByteArray valuesJson;

                explicit MockGoogleServer( QObject* parent = nullptr )
                        : QTcpServer( parent )
                {
                        connect( this, &QTcpServer::newConnection, this, [this]() {
                                while ( hasPendingConnections() )
                                {
                                        QTcpSocket* s = nextPendingConnection();
                                        connect( s, &QTcpSocket::readyRead, this, [this, s]() { onReadyRead( s ); } );
                                        connect( s, &QTcpSocket::disconnected, s, &QObject::deleteLater );
                                }
                        } );
                }

                QString baseUrl() const
                {
                        return QStringLiteral( "http://127.0.0.1:%1" ).arg( serverPort() );
                }

        private:
                void onReadyRead( QTcpSocket* s )
                {
                        const QByteArray data = s->property( "buf" ).toByteArray() + s->readAll();
                        s->setProperty( "buf", data );

                        const int headerEnd = data.indexOf( "\r\n\r\n" );
                        if ( headerEnd < 0 )
                        {
                                return;
                        }

                        const QByteArray head = data.left( headerEnd );
                        int contentLength = 0;
                        QString auth;
                        for ( const QByteArray& line : head.split( '\n' ) )
                        {
                                const QString text = QString::fromLatin1( line ).trimmed();
                                if ( text.startsWith( QStringLiteral( "content-length:" ), Qt::CaseInsensitive ) )
                                {
                                        contentLength = text.mid( 15 ).trimmed().toInt();
                                }
                                else if ( text.startsWith( QStringLiteral( "authorization:" ), Qt::CaseInsensitive ) )
                                {
                                        auth = text.mid( 14 ).trimmed();
                                }
                        }

                        const QByteArray body = data.mid( headerEnd + 4 );
                        if ( body.size() < contentLength )
                        {
                                return;
                        }
                        s->setProperty( "buf", QByteArray() );

                        const QString reqLine = QString::fromLatin1( head.left( head.indexOf( '\r' ) ) );
                        respond( s, reqLine, auth, body.left( contentLength ) );
                }

                void respond( QTcpSocket* s, const QString& reqLine, const QString& auth, const QByteArray& body )
                {
                        int status = 404;
                        QByteArray contentType = "application/json";
                        QByteArray payload = R"({"error":{"message":"Not found"}})";

                        if ( reqLine.startsWith( QStringLiteral( "POST /token " ) ) )
                        {
                                const QUrlQuery form( QString::fromUtf8( body ) );
                                const QString grantType = form.queryItemValue( QStringLiteral( "grant_type" ), QUrl::FullyDecoded );
                                if ( grantType == QStringLiteral( "authorization_code" ) )
                                {
                                        const QString verifier = form.queryItemValue( QStringLiteral( "code_verifier" ), QUrl::FullyDecoded );
                                        const QByteArray digest = QCryptographicHash::hash( verifier.toLatin1(), QCryptographicHash::Sha256 );
                                        const QString calcChallenge = QString::fromLatin1(
                                                digest.toBase64( QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals ) );

                                        if ( !expectedChallenge.isEmpty() && calcChallenge != expectedChallenge )
                                        {
                                                status = 400;
                                                payload = R"({"error":"invalid_grant","error_description":"PKCE verifier mismatch"})";
                                        }
                                        else
                                        {
                                                ++tokenExchangeCount;
                                                activeAccessToken = QStringLiteral( "access-token-1" );
                                                status = 200;
                                                payload = R"({"access_token":"access-token-1","refresh_token":"refresh-token-xyz","expires_in":3600})";
                                        }
                                }
                                else if ( grantType == QStringLiteral( "refresh_token" ) )
                                {
                                        ++tokenRefreshCount;
                                        activeAccessToken = QStringLiteral( "access-token-refreshed" );
                                        status = 200;
                                        payload = R"({"access_token":"access-token-refreshed","expires_in":3600})";
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /userinfo " ) ) )
                        {
                                if ( auth == QStringLiteral( "Bearer " ) + activeAccessToken )
                                {
                                        status = 200;
                                        payload = R"({"email":"alice@company.com"})";
                                }
                                else
                                {
                                        status = 401;
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "POST /revoke " ) ) )
                        {
                                ++revokeCount;
                                status = 200;
                                payload = "{}";
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /v4/spreadsheets/sheet123?" ) ) )
                        {
                                if ( auth != QStringLiteral( "Bearer " ) + activeAccessToken )
                                {
                                        status = 401;
                                }
                                else
                                {
                                        status = 200;
                                        payload = R"({
                                                "properties": {"title": "Warehouse Labels"},
                                                "sheets": [
                                                        {"properties": {"sheetId": 0, "title": "Summary"}},
                                                        {"properties": {"sheetId": 98765, "title": "Print Queue"}}
                                                ]
                                        })";
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /v4/spreadsheets/sheet123/values/" ) ) )
                        {
                                if ( failFirstValuesWith401 )
                                {
                                        failFirstValuesWith401 = false;
                                        status = 401;
                                        payload = R"({"error":{"message":"Token expired"}})";
                                }
                                else if ( auth != QStringLiteral( "Bearer " ) + activeAccessToken )
                                {
                                        status = 401;
                                }
                                else
                                {
                                        status = 200;
                                        payload = valuesJson;
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /spreadsheets/d/pubsheet123/gviz/tq" ) ) )
                        {
                                status = 200;
                                if ( privatePublicSheetHtml )
                                {
                                        contentType = "text/html; charset=utf-8";
                                        payload = "<!DOCTYPE html><html><body>Sign in - Google Accounts</body></html>";
                                }
                                else
                                {
                                        contentType = "text/csv; charset=utf-8";
                                        payload = csvBody;
                                }
                        }

                        const QByteArray reply = "HTTP/1.1 " + QByteArray::number( status )
                                + " OK\r\nContent-Type: " + contentType + "\r\nContent-Length: "
                                + QByteArray::number( payload.size() )
                                + "\r\nConnection: close\r\n\r\n" + payload;
                        s->write( reply );
                        s->disconnectFromHost();
                }
        };

        void sendLocalHttpGet( quint16 port, const QString& pathAndQuery )
        {
                QTcpSocket sock;
                QByteArray resp;
                bool connected = false;
                bool done = false;

                QObject::connect( &sock, &QTcpSocket::connected, &sock, [&]() {
                        connected = true;
                        const QByteArray req = "GET " + pathAndQuery.toUtf8()
                                + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
                        sock.write( req );
                        sock.flush();
                } );
                QObject::connect( &sock, &QTcpSocket::readyRead, &sock, [&]() {
                        resp.append( sock.readAll() );
                        if ( resp.contains( "\r\n\r\n" ) )
                        {
                                done = true;
                        }
                } );
                QObject::connect( &sock, &QTcpSocket::disconnected, &sock, [&]() {
                        resp.append( sock.readAll() );
                        done = true;
                } );

                sock.connectToHost( QHostAddress::LocalHost, port );
                QTRY_VERIFY_WITH_TIMEOUT( connected && done, 5000 );
                QVERIFY( resp.startsWith( "HTTP/1.1 200" ) );
        }

}


void TestGoogleSheetsDataSource::initTestCase()
{
        QCoreApplication::setOrganizationName( QStringLiteral( "glabels.org" ) );
        QCoreApplication::setApplicationName( QStringLiteral( "glabels-test-gsheets" ) );
        Settings::init();
        Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        SecretStore::setMemoryModeForTests( true );
}


void TestGoogleSheetsDataSource::cleanupTestCase()
{
        SecretStore::setMemoryModeForTests( false );
}


void TestGoogleSheetsDataSource::cleanup()
{
        SecretStore::clearMemoryStoreForTests();
        Settings::setGoogleAccountEmail( QString() );
}


void TestGoogleSheetsDataSource::parseGoogleSheetsUrls()
{
        {
                const auto p = ExternalDataSource::parseGoogleSheetsUrl(
                        QStringLiteral( "https://docs.google.com/spreadsheets/d/1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms/edit#gid=98765" ) );
                QVERIFY( p.valid );
                QCOMPARE( p.spreadsheetId, QStringLiteral( "1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms" ) );
                QCOMPARE( p.gid, QStringLiteral( "98765" ) );
        }

        {
                const auto p = ExternalDataSource::parseGoogleSheetsUrl(
                        QStringLiteral( "https://docs.google.com/spreadsheets/u/1/d/abcDEF1234567890_-xy/edit?gid=42#gid=42" ) );
                QVERIFY( p.valid );
                QCOMPARE( p.spreadsheetId, QStringLiteral( "abcDEF1234567890_-xy" ) );
                QCOMPARE( p.gid, QStringLiteral( "42" ) );
        }

        {
                const auto p = ExternalDataSource::parseGoogleSheetsUrl(
                        QStringLiteral( "1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms" ) );
                QVERIFY( p.valid );
                QCOMPARE( p.spreadsheetId, QStringLiteral( "1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms" ) );
                QVERIFY( p.gid.isEmpty() );
        }

        QVERIFY( !ExternalDataSource::parseGoogleSheetsUrl( QStringLiteral( "https://example.com/not-a-sheet" ) ).valid );
        QVERIFY( !ExternalDataSource::parseGoogleSheetsUrl( QStringLiteral( "short" ) ).valid );
}


void TestGoogleSheetsDataSource::xmlRoundTripWithoutSecrets()
{
        Model model;
        auto templates = Db::templates();
        QVERIFY( !templates.isEmpty() );
        model.setTmplate( templates.first() );

        ExternalDataSource src;
        src.kind = ExternalSourceKind::GoogleSheets;
        src.spreadsheetId = QStringLiteral( "1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms" );
        src.sheetName = QStringLiteral( "Print Queue" );
        src.sheetGid = QStringLiteral( "98765" );
        src.cellRange = QStringLiteral( "A1:E" );
        src.googleAuthMode = GoogleAuthMode::OAuth;
        src.hasHeaderRow = true;
        src.displayName = QStringLiteral( "Warehouse Labels" );
        src.matchKeyField = QStringLiteral( "SKU" );
        src.refreshOnOpen = true;
        src.refreshBeforePrint = true;

        const QString secretToken = QStringLiteral( "ya29-secret-refresh-token-do-not-save" );
        QVERIFY( SecretStore::store( src.credentialKey(), secretToken ) );

        model.setExternalDataSource( src );

        QByteArray xml;
        XmlLabelCreator::writeBuffer( &model, xml );
        QVERIFY( !xml.isEmpty() );
        QVERIFY( !xml.contains( secretToken.toUtf8() ) );
        QVERIFY( xml.contains( "google_sheets" ) );

        std::unique_ptr<Model> parsed( XmlLabelParser::readBuffer( xml ) );
        QVERIFY( parsed != nullptr );
        QCOMPARE( parsed->externalDataSource(), src );
}


void TestGoogleSheetsDataSource::parseSheetsValuesJsonAndCsv()
{
        // Google Sheets API v4 values JSON (jagged row + trailing blank row)
        const QByteArray valuesPayload = R"({
                "range": "'Print Queue'!A1:C4",
                "values": [
                        ["SKU", "Name", "Price"],
                        ["A-100", "Widget", "$9.99"],
                        ["B-200", "Gadget"],
                        ["", "", ""]
                ]
        })";

        FetchResult res;
        QString err;
        QVERIFY( GoogleSheetsClient::parseSheetsValuesJson( valuesPayload, true, res, err ) );
        QCOMPARE( res.headers, QStringList( { "SKU", "Name", "Price" } ) );
        QCOMPARE( res.rows.size(), 2 );
        QCOMPARE( res.rows[0].value( "Price" ), QStringLiteral( "$9.99" ) );
        QCOMPARE( res.rows[1].value( "Price" ), QString() );

        // RFC-4180 CSV with BOM, quoted commas, escaped quotes, embedded newline
        const QByteArray csvPayload = "\xEF\xBB\xBFSKU,Name,Note\r\n"
                                      "A-1,\"Widget, \"\"Pro\"\"\",\"Line 1\nLine 2\"\r\n";
        FetchResult csvRes;
        QVERIFY( GoogleSheetsClient::parseCsvData( csvPayload, true, csvRes, err ) );
        QCOMPARE( csvRes.headers, QStringList( { "SKU", "Name", "Note" } ) );
        QCOMPARE( csvRes.rows.size(), 1 );
        QCOMPARE( csvRes.rows[0].value( "Name" ), QStringLiteral( "Widget, \"Pro\"" ) );
        QCOMPARE( csvRes.rows[0].value( "Note" ), QStringLiteral( "Line 1\nLine 2" ) );
}


void TestGoogleSheetsDataSource::oauthSignInRefreshAndPickerFlow()
{
        MockGoogleServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );

        GoogleAuthManager auth;
        auth.setClientCredentialsForTests(
                QStringLiteral( "1234567890-abc.apps.googleusercontent.com" ),
                QStringLiteral( "desktop-secret" ),
                QStringLiteral( "https://example.github.io/glabels-neo/picker/" ),
                QStringLiteral( "picker-api-key-1" ) );
        auth.setEndpointsForTests(
                server.baseUrl() + QStringLiteral( "/auth" ),
                server.baseUrl() + QStringLiteral( "/token" ),
                server.baseUrl() + QStringLiteral( "/userinfo" ),
                server.baseUrl() + QStringLiteral( "/revoke" ) );

        QCOMPARE( auth.cloudProjectNumber(), QStringLiteral( "1234567890" ) );
        QVERIFY( auth.isConfigured() );
        QVERIFY( auth.isPickerConfigured() );

        QUrl capturedAuthUrl;
        auth.setUrlOpenerForTests( [&]( const QUrl& u ) {
                capturedAuthUrl = u;
                return true;
        } );

        QSignalSpy signedInSpy( &auth, &GoogleAuthManager::signedIn );
        QSignalSpy failedSpy( &auth, &GoogleAuthManager::signInFailed );

        auth.signIn();
        QVERIFY( capturedAuthUrl.isValid() );

        const QUrlQuery authQuery( capturedAuthUrl );
        QCOMPARE( authQuery.queryItemValue( QStringLiteral( "code_challenge_method" ) ), QStringLiteral( "S256" ) );
        QVERIFY( authQuery.queryItemValue( QStringLiteral( "scope" ), QUrl::FullyDecoded ).contains( QStringLiteral( "drive.file" ) ) );
        const QString state = authQuery.queryItemValue( QStringLiteral( "state" ), QUrl::FullyDecoded );
        const QUrl redirectUri( authQuery.queryItemValue( QStringLiteral( "redirect_uri" ), QUrl::FullyDecoded ) );
        server.expectedChallenge = authQuery.queryItemValue( QStringLiteral( "code_challenge" ), QUrl::FullyDecoded );

        // Simulate browser redirect back to local loopback server
        sendLocalHttpGet( static_cast<quint16>( redirectUri.port() ),
                          QStringLiteral( "/callback?code=auth-code-123&state=%1" ).arg( state ) );

        QTRY_COMPARE_WITH_TIMEOUT( signedInSpy.count(), 1, 5000 );
        QCOMPARE( failedSpy.count(), 0 );
        QCOMPARE( signedInSpy.at( 0 ).at( 0 ).toString(), QStringLiteral( "alice@company.com" ) );
        QVERIFY( auth.isSignedIn() );
        QCOMPARE( auth.signedInEmail(), QStringLiteral( "alice@company.com" ) );
        QCOMPARE( SecretStore::retrieve( GoogleAuthManager::REFRESH_TOKEN_KEY ), QStringLiteral( "refresh-token-xyz" ) );

        // Invalidate in-memory access token and verify silent refresh
        auth.invalidateAccessToken();
        QString refreshedToken;
        QString refreshErr;
        auth.ensureAccessToken( [&]( const QString& tok, const QString& err ) {
                refreshedToken = tok;
                refreshErr = err;
        } );
        QTRY_VERIFY_WITH_TIMEOUT( !refreshedToken.isEmpty() || !refreshErr.isEmpty(), 5000 );
        QVERIFY( refreshErr.isEmpty() );
        QCOMPARE( refreshedToken, QStringLiteral( "access-token-refreshed" ) );
        QCOMPARE( server.tokenRefreshCount, 1 );

        // Test Google Picker loopback flow
        QUrl capturedPickerUrl;
        auth.setUrlOpenerForTests( [&]( const QUrl& u ) {
                capturedPickerUrl = u;
                return true;
        } );
        QSignalSpy pickedSpy( &auth, &GoogleAuthManager::spreadsheetPicked );
        auth.pickSpreadsheet();
        QTRY_VERIFY_WITH_TIMEOUT( capturedPickerUrl.isValid(), 5000 );

        // Access token is in the URL fragment, NEVER the query string
        QVERIFY( capturedPickerUrl.query().isEmpty() );
        const QUrlQuery frag( capturedPickerUrl.fragment() );
        QCOMPARE( frag.queryItemValue( QStringLiteral( "access_token" ), QUrl::FullyDecoded ),
                  QStringLiteral( "access-token-refreshed" ) );
        QCOMPARE( frag.queryItemValue( QStringLiteral( "app_id" ) ), QStringLiteral( "1234567890" ) );
        const QUrl pickerCb( frag.queryItemValue( QStringLiteral( "callback" ), QUrl::FullyDecoded ) );
        const QString pickerState = frag.queryItemValue( QStringLiteral( "state" ), QUrl::FullyDecoded );

        sendLocalHttpGet( static_cast<quint16>( pickerCb.port() ),
                          QStringLiteral( "/picked?id=sheet123&name=Warehouse%20Labels&state=%1" ).arg( pickerState ) );

        QTRY_COMPARE_WITH_TIMEOUT( pickedSpy.count(), 1, 5000 );
        QCOMPARE( pickedSpy.at( 0 ).at( 0 ).toString(), QStringLiteral( "sheet123" ) );
        QCOMPARE( pickedSpy.at( 0 ).at( 1 ).toString(), QStringLiteral( "Warehouse Labels" ) );

        // Sign out clears SecretStore + email and revokes token
        auth.signOut();
        QVERIFY( !auth.isSignedIn() );
        QVERIFY( auth.signedInEmail().isEmpty() );
        QTRY_COMPARE_WITH_TIMEOUT( server.revokeCount, 1, 5000 );
}


void TestGoogleSheetsDataSource::sheetsApiV4FetchAndRetry401()
{
        MockGoogleServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );
        server.failFirstValuesWith401 = true;
        server.valuesJson = R"({
                "values": [
                        ["SKU", "Name", "Price"],
                        ["S-1", "Label Roll", "$15.00"]
                ]
        })";

        GoogleAuthManager auth;
        auth.setClientCredentialsForTests( QStringLiteral( "123-cid" ), QStringLiteral( "sec" ) );
        auth.setEndpointsForTests(
                server.baseUrl() + QStringLiteral( "/auth" ),
                server.baseUrl() + QStringLiteral( "/token" ),
                server.baseUrl() + QStringLiteral( "/userinfo" ),
                server.baseUrl() + QStringLiteral( "/revoke" ) );
        QVERIFY( SecretStore::store( GoogleAuthManager::REFRESH_TOKEN_KEY, QStringLiteral( "refresh-token-xyz" ) ) );

        GoogleSheetsClient client( &auth );
        client.setApiBaseUrlsForTests( server.baseUrl(), server.baseUrl() );

        // Fetch tab metadata
        QSignalSpy infoSpy( &client, &GoogleSheetsClient::spreadsheetInfoLoaded );
        client.fetchSpreadsheetInfo( QStringLiteral( "sheet123" ) );
        QTRY_COMPARE_WITH_TIMEOUT( infoSpy.count(), 1, 5000 );
        const auto info = infoSpy.at( 0 ).at( 0 ).value<GoogleSpreadsheetInfo>();
        QCOMPARE( info.title, QStringLiteral( "Warehouse Labels" ) );
        QCOMPARE( info.tabs.size(), 2 );
        QCOMPARE( info.tabs[1].title, QStringLiteral( "Print Queue" ) );

        // Fetch values resolving #gid=98765 -> "Print Queue", retrying automatically after 401
        ExternalDataSource src;
        src.kind = ExternalSourceKind::GoogleSheets;
        src.spreadsheetId = QStringLiteral( "sheet123" );
        src.sheetGid = QStringLiteral( "98765" );
        src.googleAuthMode = GoogleAuthMode::OAuth;
        src.hasHeaderRow = true;

        QSignalSpy finishedSpy( &client, &ExternalDataFetcher::finished );
        QSignalSpy failedSpy( &client, &ExternalDataFetcher::failed );
        client.fetch( src );

        QTRY_COMPARE_WITH_TIMEOUT( finishedSpy.count(), 1, 5000 );
        QCOMPARE( failedSpy.count(), 0 );
        QVERIFY( server.tokenRefreshCount >= 2 );

        const auto res = finishedSpy.at( 0 ).at( 0 ).value<FetchResult>();
        QCOMPARE( res.rows.size(), 1 );
        QCOMPARE( res.rows[0].value( "SKU" ), QStringLiteral( "S-1" ) );
        QCOMPARE( res.rows[0].value( "Price" ), QStringLiteral( "$15.00" ) );
}


void TestGoogleSheetsDataSource::publicLinkCsvFetchAndPrivateDetection()
{
        MockGoogleServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );
        server.csvBody = "SKU,Name\nP-10,Thermal Sticker\nP-20,Shipping Tag\n";

        GoogleSheetsClient client;
        client.setApiBaseUrlsForTests( server.baseUrl(), server.baseUrl() );

        ExternalDataSource src;
        src.kind = ExternalSourceKind::GoogleSheets;
        src.spreadsheetId = QStringLiteral( "pubsheet123" );
        src.googleAuthMode = GoogleAuthMode::PublicLink;
        src.hasHeaderRow = true;

        QSignalSpy finishedSpy( &client, &ExternalDataFetcher::finished );
        QSignalSpy failedSpy( &client, &ExternalDataFetcher::failed );

        client.fetch( src );
        QTRY_COMPARE_WITH_TIMEOUT( finishedSpy.count(), 1, 5000 );
        QCOMPARE( failedSpy.count(), 0 );
        const auto res = finishedSpy.at( 0 ).at( 0 ).value<FetchResult>();
        QCOMPARE( res.rows.size(), 2 );
        QCOMPARE( res.rows[1].value( "Name" ), QStringLiteral( "Shipping Tag" ) );

        // Private sheet redirected to HTML login page gives a clear actionable error
        server.privatePublicSheetHtml = true;
        client.fetch( src );
        QTRY_COMPARE_WITH_TIMEOUT( failedSpy.count(), 1, 5000 );
        QVERIFY( failedSpy.at( 0 ).at( 0 ).toString().contains( QStringLiteral( "private" ), Qt::CaseInsensitive ) );
}


void TestGoogleSheetsDataSource::fillViewGoogleSheetsEndToEndRefresh()
{
        MockGoogleServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );
        server.csvBody = "SKU,Name,Price\nA-1,Apple,$1.00\nB-2,Banana,$0.50\n";

        Model model;
        auto templates = Db::templates();
        QVERIFY( !templates.isEmpty() );
        model.setTmplate( templates.first() );

        auto* textObj = new ModelTextObject();
        textObj->setText( QStringLiteral( "${SKU}\n${Name}\n${Price}\n${Lot}" ) );
        model.addObject( textObj );

        ExternalDataSource src;
        src.kind = ExternalSourceKind::GoogleSheets;
        src.spreadsheetId = QStringLiteral( "pubsheet123" );
        src.googleAuthMode = GoogleAuthMode::PublicLink;
        src.refreshOnOpen = true;
        src.refreshBeforePrint = true;
        src.matchKeyField = QStringLiteral( "SKU" );
        model.setExternalDataSource( src );

        FillView fillView;
        fillView.googleSheetsClient()->setApiBaseUrlsForTests( server.baseUrl(), server.baseUrl() );
        fillView.setModel( &model );

        auto* tableModel = qobject_cast<FillJobModel*>( fillView.jobsTable->model() );
        QVERIFY( tableModel != nullptr );
        QTRY_COMPARE_WITH_TIMEOUT( tableModel->rowCount(), 2, 5000 );

        // Edit row 0 Qty + unlocked Lot field
        const int lotCol = FillJobModel::FIRST_FIELD_COLUMN + tableModel->fieldNames().indexOf( QStringLiteral( "Lot" ) );
        QVERIFY( tableModel->setData( tableModel->index( 0, FillJobModel::QUANTITY_COLUMN ), 4, Qt::EditRole ) );
        QVERIFY( tableModel->setData( tableModel->index( 0, lotCol ), QStringLiteral( "LOT-9" ), Qt::EditRole ) );

        // Sheet updates price for A-1
        server.csvBody = "SKU,Name,Price\nA-1,Honeycrisp Apple,$1.49\nB-2,Banana,$0.50\n";
        fillView.refreshExternalSource();

        QTRY_COMPARE_WITH_TIMEOUT(
                tableModel->jobAt( 0 )->values.value( QStringLiteral( "Price" ) ),
                QStringLiteral( "$1.49" ),
                5000 );
        QCOMPARE( tableModel->jobAt( 0 )->values.value( QStringLiteral( "Name" ) ), QStringLiteral( "Honeycrisp Apple" ) );
        QCOMPARE( tableModel->jobAt( 0 )->quantity, 4 );
        QCOMPARE( tableModel->jobAt( 0 )->values.value( QStringLiteral( "Lot" ) ), QStringLiteral( "LOT-9" ) );
}
