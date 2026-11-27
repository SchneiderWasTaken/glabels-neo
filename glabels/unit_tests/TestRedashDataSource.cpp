//  TestRedashDataSource.cpp
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


#include "TestRedashDataSource.hpp"

#include "FillJobModel.hpp"
#include "FillView.hpp"
#include "datasource/RedashClient.hpp"
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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>


QTEST_MAIN( TestRedashDataSource )


using namespace glabels;
using namespace glabels::datasource;
using namespace glabels::model;


namespace
{

        ///
        /// Minimal local mock Redash HTTP server supporting:
        ///  - POST /api/queries/{id}/results (returns job or direct query_result)
        ///  - GET  /api/jobs/{jobId}         (progresses queued -> started -> success/failure)
        ///  - GET  /api/query_results/{id}.json
        ///  - GET  /api/queries/{id}/results.json
        ///  - GET  /api/queries/{id}
        ///
        class MockRedashServer : public QTcpServer
        {
        public:
                QString     expectedUserKey{ QStringLiteral( "user-secret-key" ) };
                QString     expectedQueryKey{ QStringLiteral( "query-secret-key" ) };
                bool        useJobPolling{ true };
                bool        failJob{ false };
                int         forceStatusCode{ 0 };
                int         jobPollCount{ 0 };
                QJsonObject lastPostBody;
                QString     lastAuthHeader;
                QByteArray  resultPayload;

                explicit MockRedashServer( QObject* parent = nullptr )
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
                        lastAuthHeader = auth;
                        int status = 404;
                        QByteArray payload = R"({"message":"Not found"})";

                        if ( forceStatusCode > 0 )
                        {
                                status = forceStatusCode;
                                payload = R"({"message":"Forced error"})";
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "POST /api/queries/42/results " ) ) )
                        {
                                if ( auth != QStringLiteral( "Key " ) + expectedUserKey )
                                {
                                        status = 401;
                                        payload = R"({"message":"Unauthorized"})";
                                }
                                else
                                {
                                        lastPostBody = QJsonDocument::fromJson( body ).object();
                                        status = 200;
                                        if ( useJobPolling )
                                        {
                                                jobPollCount = 0;
                                                payload = R"({"job":{"id":"job-1","status":1}})";
                                        }
                                        else
                                        {
                                                payload = resultPayload;
                                        }
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /api/jobs/job-1 " ) ) )
                        {
                                if ( auth != QStringLiteral( "Key " ) + expectedUserKey )
                                {
                                        status = 401;
                                }
                                else
                                {
                                        status = 200;
                                        ++jobPollCount;
                                        if ( failJob )
                                        {
                                                payload = R"({"job":{"id":"job-1","status":4,"error":"syntax error near SELECT"}})";
                                        }
                                        else if ( jobPollCount == 1 )
                                        {
                                                payload = R"({"job":{"id":"job-1","status":2}})";
                                        }
                                        else
                                        {
                                                payload = R"({"job":{"id":"job-1","status":3,"query_result_id":777}})";
                                        }
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /api/query_results/777.json " ) ) )
                        {
                                if ( auth != QStringLiteral( "Key " ) + expectedUserKey )
                                {
                                        status = 401;
                                }
                                else
                                {
                                        status = 200;
                                        payload = resultPayload;
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /api/queries/7/results.json " ) ) )
                        {
                                if ( auth != QStringLiteral( "Key " ) + expectedQueryKey )
                                {
                                        status = 401;
                                        payload = R"({"message":"Invalid query API key"})";
                                }
                                else
                                {
                                        status = 200;
                                        payload = resultPayload;
                                }
                        }
                        else if ( reqLine.startsWith( QStringLiteral( "GET /api/queries/42 " ) ) )
                        {
                                if ( auth != QStringLiteral( "Key " ) + expectedUserKey )
                                {
                                        status = 401;
                                }
                                else
                                {
                                        status = 200;
                                        payload = R"({
                                                "id": 42,
                                                "name": "Daily Inventory Labels",
                                                "options": {
                                                        "parameters": [
                                                                {"name": "store", "value": "104"},
                                                                {"name": "active", "value": true}
                                                        ]
                                                }
                                        })";
                                }
                        }

                        const QByteArray reply = "HTTP/1.1 " + QByteArray::number( status )
                                + " OK\r\nContent-Type: application/json\r\nContent-Length: "
                                + QByteArray::number( payload.size() )
                                + "\r\nConnection: close\r\n\r\n" + payload;
                        s->write( reply );
                        s->disconnectFromHost();
                }
        };

}


void TestRedashDataSource::initTestCase()
{
        Settings::init();
        Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        SecretStore::setMemoryModeForTests( true );
}


void TestRedashDataSource::cleanupTestCase()
{
        SecretStore::setMemoryModeForTests( false );
}


void TestRedashDataSource::cleanup()
{
        SecretStore::clearMemoryStoreForTests();
}


void TestRedashDataSource::parseRedashUrls()
{
        // Standard query editor URL
        {
                const auto p = ExternalDataSource::parseRedashUrl(
                        QStringLiteral( "https://redash.company.com/queries/42/source#123" ) );
                QVERIFY( p.valid );
                QCOMPARE( p.serverUrl, QStringLiteral( "https://redash.company.com" ) );
                QCOMPARE( p.queryId, 42 );
                QCOMPARE( p.authMode, RedashAuthMode::UserApiKey );
                QVERIFY( p.apiKey.isEmpty() );
        }

        // Subpath deployment
        {
                const auto p = ExternalDataSource::parseRedashUrl(
                        QStringLiteral( "https://bi.company.com/redash/queries/99" ) );
                QVERIFY( p.valid );
                QCOMPARE( p.serverUrl, QStringLiteral( "https://bi.company.com/redash" ) );
                QCOMPARE( p.queryId, 99 );
        }

        // Results JSON URL with query API key
        {
                const auto p = ExternalDataSource::parseRedashUrl(
                        QStringLiteral( "https://redash.company.com/api/queries/7/results.json?api_key=qkey-12345" ) );
                QVERIFY( p.valid );
                QCOMPARE( p.serverUrl, QStringLiteral( "https://redash.company.com" ) );
                QCOMPARE( p.queryId, 7 );
                QCOMPARE( p.authMode, RedashAuthMode::QueryApiKey );
                QCOMPARE( p.apiKey, QStringLiteral( "qkey-12345" ) );
        }

        // Plain HTTP rejected for non-localhost, accepted for 127.0.0.1
        QVERIFY( !ExternalDataSource::isAllowedServerUrl( QStringLiteral( "http://redash.company.com" ) ) );
        QVERIFY( !ExternalDataSource::parseRedashUrl( QStringLiteral( "http://redash.company.com/queries/42" ) ).valid );
        QVERIFY( ExternalDataSource::isAllowedServerUrl( QStringLiteral( "http://127.0.0.1:5000" ) ) );
        QVERIFY( ExternalDataSource::isAllowedServerUrl( QStringLiteral( "http://localhost:5000" ) ) );

        // Credential key scoping
        ExternalDataSource userSrc;
        userSrc.kind = ExternalSourceKind::Redash;
        userSrc.serverUrl = QStringLiteral( "https://redash.company.com/" );
        userSrc.queryId = 42;
        userSrc.redashAuthMode = RedashAuthMode::UserApiKey;
        QCOMPARE( userSrc.credentialKey(), QStringLiteral( "redash:user:https://redash.company.com" ) );

        ExternalDataSource querySrc = userSrc;
        querySrc.redashAuthMode = RedashAuthMode::QueryApiKey;
        QCOMPARE( querySrc.credentialKey(), QStringLiteral( "redash:query:https://redash.company.com:42" ) );
}


void TestRedashDataSource::xmlRoundTripWithoutSecrets()
{
        Model model;
        auto templates = Db::templates();
        QVERIFY( !templates.isEmpty() );
        model.setTmplate( templates.first() );

        ExternalDataSource src;
        src.kind = ExternalSourceKind::Redash;
        src.serverUrl = QStringLiteral( "https://redash.company.com" );
        src.queryId = 42;
        src.redashAuthMode = RedashAuthMode::UserApiKey;
        src.maxAgeSeconds = 300;
        src.displayName = QStringLiteral( "Daily SKUs" );
        src.matchKeyField = QStringLiteral( "SKU" );
        src.refreshOnOpen = true;
        src.refreshBeforePrint = false;
        src.parameters.insert( QStringLiteral( "store" ), QStringLiteral( "104" ) );

        // Store a secret in SecretStore and verify it never leaks into XML
        const QString secret = QStringLiteral( "super-secret-redash-key-999" );
        QVERIFY( SecretStore::store( src.credentialKey(), secret ) );

        model.setExternalDataSource( src );

        QByteArray xml;
        XmlLabelCreator::writeBuffer( &model, xml );
        QVERIFY( !xml.isEmpty() );
        QVERIFY( !xml.contains( secret.toUtf8() ) );
        QVERIFY( xml.contains( "ExternalDataSource" ) );

        std::unique_ptr<Model> parsed( XmlLabelParser::readBuffer( xml ) );
        QVERIFY( parsed != nullptr );
        QCOMPARE( parsed->externalDataSource(), src );
}


void TestRedashDataSource::parseQueryResultJson()
{
        const QByteArray payload = R"({
                "query_result": {
                        "id": 777,
                        "data": {
                                "columns": [
                                        {"name": "sku", "friendly_name": "SKU", "type": "string"},
                                        {"name": "upc", "friendly_name": "UPC", "type": "integer"},
                                        {"name": "price", "friendly_name": "Price", "type": "float"},
                                        {"name": "in_stock", "friendly_name": "In Stock", "type": "boolean"},
                                        {"name": "notes", "friendly_name": "Notes", "type": "string"}
                                ],
                                "rows": [
                                        {"sku": "ABC-1", "upc": 123456789012, "price": 19.95, "in_stock": true, "notes": null}
                                ]
                        }
                }
        })";

        FetchResult res;
        QString err;
        QVERIFY( RedashClient::parseQueryResultJson( payload, res, err ) );
        QVERIFY( err.isEmpty() );

        // Column order matches data.columns (not alphabetical)
        QCOMPARE( res.headers, QStringList( { "sku", "upc", "price", "in_stock", "notes" } ) );
        QCOMPARE( res.rows.size(), 1 );
        QCOMPARE( res.rows[0].value( "sku" ), QStringLiteral( "ABC-1" ) );
        QCOMPARE( res.rows[0].value( "upc" ), QStringLiteral( "123456789012" ) );
        QCOMPARE( res.rows[0].value( "price" ), QStringLiteral( "19.95" ) );
        QCOMPARE( res.rows[0].value( "in_stock" ), QStringLiteral( "true" ) );
        QCOMPARE( res.rows[0].value( "notes" ), QString() );

        // Invalid JSON fails cleanly
        FetchResult badRes;
        QString badErr;
        QVERIFY( !RedashClient::parseQueryResultJson( QByteArray( "not json" ), badRes, badErr ) );
        QVERIFY( !badErr.isEmpty() );
}


void TestRedashDataSource::userApiKeyJobPollingAndParameters()
{
        MockRedashServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );
        server.resultPayload = R"({
                "query_result": {
                        "data": {
                                "columns": [{"name": "SKU"}, {"name": "Name"}, {"name": "Price"}],
                                "rows": [
                                        {"SKU": "A-100", "Name": "Widget", "Price": 9.99},
                                        {"SKU": "B-200", "Name": "Gadget", "Price": 14.5}
                                ]
                        }
                }
        })";

        ExternalDataSource src;
        src.kind = ExternalSourceKind::Redash;
        src.serverUrl = server.baseUrl();
        src.queryId = 42;
        src.redashAuthMode = RedashAuthMode::UserApiKey;
        src.maxAgeSeconds = 0;
        src.parameters.insert( QStringLiteral( "store" ), QStringLiteral( "104" ) );

        QVERIFY( SecretStore::store( src.credentialKey(), QStringLiteral( "user-secret-key" ) ) );

        RedashClient client;
        client.setPollIntervalMs( 15 );

        QSignalSpy finishedSpy( &client, &ExternalDataFetcher::finished );
        QSignalSpy failedSpy( &client, &ExternalDataFetcher::failed );

        client.fetch( src );
        QTRY_COMPARE_WITH_TIMEOUT( finishedSpy.count(), 1, 5000 );
        QCOMPARE( failedSpy.count(), 0 );
        QCOMPARE( server.jobPollCount, 2 );
        QCOMPARE( server.lastPostBody.value( QStringLiteral( "max_age" ) ).toInt(), 0 );
        QCOMPARE( server.lastPostBody.value( QStringLiteral( "parameters" ) ).toObject().value( QStringLiteral( "store" ) ).toString(),
                  QStringLiteral( "104" ) );

        const auto res = finishedSpy.at( 0 ).at( 0 ).value<FetchResult>();
        QCOMPARE( res.headers, QStringList( { "SKU", "Name", "Price" } ) );
        QCOMPARE( res.rows.size(), 2 );
        QCOMPARE( res.rows[0].value( "Name" ), QStringLiteral( "Widget" ) );
        QCOMPARE( res.rows[1].value( "Price" ), QStringLiteral( "14.5" ) );
}


void TestRedashDataSource::queryApiKeyCachedResults()
{
        MockRedashServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );
        server.resultPayload = R"({
                "query_result": {
                        "data": {
                                "columns": [{"name": "SKU"}, {"name": "Qty"}],
                                "rows": [{"SKU": "C-300", "Qty": 3}]
                        }
                }
        })";

        ExternalDataSource src;
        src.kind = ExternalSourceKind::Redash;
        src.serverUrl = server.baseUrl();
        src.queryId = 7;
        src.redashAuthMode = RedashAuthMode::QueryApiKey;
        src.maxAgeSeconds = -1;

        QVERIFY( SecretStore::store( src.credentialKey(), QStringLiteral( "query-secret-key" ) ) );

        RedashClient client;
        QSignalSpy finishedSpy( &client, &ExternalDataFetcher::finished );
        QSignalSpy failedSpy( &client, &ExternalDataFetcher::failed );

        client.fetch( src );
        QTRY_COMPARE_WITH_TIMEOUT( finishedSpy.count(), 1, 5000 );
        QCOMPARE( failedSpy.count(), 0 );

        const auto res = finishedSpy.at( 0 ).at( 0 ).value<FetchResult>();
        QCOMPARE( res.rows.size(), 1 );
        QCOMPARE( res.rows[0].value( "SKU" ), QStringLiteral( "C-300" ) );
        QCOMPARE( res.rows[0].value( "Qty" ), QStringLiteral( "3" ) );
}


void TestRedashDataSource::httpErrorsAndJobFailure()
{
        MockRedashServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );

        ExternalDataSource src;
        src.kind = ExternalSourceKind::Redash;
        src.serverUrl = server.baseUrl();
        src.queryId = 42;
        src.redashAuthMode = RedashAuthMode::UserApiKey;

        // 1. Wrong API key -> HTTP 401
        RedashClient client;
        client.setPollIntervalMs( 15 );
        QSignalSpy failedSpy( &client, &ExternalDataFetcher::failed );

        client.fetch( src, QStringLiteral( "wrong-key" ) );
        QTRY_COMPARE_WITH_TIMEOUT( failedSpy.count(), 1, 5000 );
        QVERIFY( failedSpy.at( 0 ).at( 0 ).toString().contains( QStringLiteral( "401" ) ) );

        // 2. Job failure (status 4) surfaces SQL error
        failedSpy.clear();
        server.failJob = true;
        client.fetch( src, QStringLiteral( "user-secret-key" ) );
        QTRY_COMPARE_WITH_TIMEOUT( failedSpy.count(), 1, 5000 );
        QVERIFY( failedSpy.at( 0 ).at( 0 ).toString().contains( QStringLiteral( "syntax error" ) ) );
}


void TestRedashDataSource::fetchQueryInfo()
{
        MockRedashServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );

        RedashClient client;
        QSignalSpy infoSpy( &client, &RedashClient::queryInfoLoaded );
        QSignalSpy failSpy( &client, &RedashClient::queryInfoFailed );

        client.fetchQueryInfo( server.baseUrl(), 42, QStringLiteral( "user-secret-key" ) );
        QTRY_COMPARE_WITH_TIMEOUT( infoSpy.count(), 1, 5000 );
        QCOMPARE( failSpy.count(), 0 );

        const auto info = infoSpy.at( 0 ).at( 0 ).value<RedashQueryInfo>();
        QCOMPARE( info.queryId, 42 );
        QCOMPARE( info.name, QStringLiteral( "Daily Inventory Labels" ) );
        QCOMPARE( info.defaultParameters.value( QStringLiteral( "store" ) ), QStringLiteral( "104" ) );
        QCOMPARE( info.defaultParameters.value( QStringLiteral( "active" ) ), QStringLiteral( "true" ) );
}


void TestRedashDataSource::fillViewEndToEndRefreshAndPreserveEdits()
{
        MockRedashServer server;
        QVERIFY( server.listen( QHostAddress::LocalHost, 0 ) );
        server.useJobPolling = false;
        server.resultPayload = R"({
                "query_result": {
                        "data": {
                                "columns": [{"name": "sku"}, {"name": "Name"}, {"name": "Price"}],
                                "rows": [
                                        {"sku": "A-100", "Name": "Widget", "Price": 9.99},
                                        {"sku": "B-200", "Name": "Gadget", "Price": 14.50}
                                ]
                        }
                }
        })";

        Model model;
        auto templates = Db::templates();
        QVERIFY( !templates.isEmpty() );
        model.setTmplate( templates.first() );

        auto* textObj = new ModelTextObject();
        textObj->setText( QStringLiteral( "${SKU}\n${Name}\n${Price}\n${Note}" ) );
        model.addObject( textObj );

        ExternalDataSource src;
        src.kind = ExternalSourceKind::Redash;
        src.serverUrl = server.baseUrl();
        src.queryId = 42;
        src.redashAuthMode = RedashAuthMode::UserApiKey;
        src.refreshOnOpen = true;
        src.refreshBeforePrint = true;
        src.matchKeyField = QStringLiteral( "SKU" );
        QVERIFY( SecretStore::store( src.credentialKey(), QStringLiteral( "user-secret-key" ) ) );
        model.setExternalDataSource( src );

        FillView fillView;
        fillView.redashClient()->setPollIntervalMs( 15 );
        fillView.setModel( &model );

        auto* tableModel = qobject_cast<FillJobModel*>( fillView.jobsTable->model() );
        QVERIFY( tableModel != nullptr );

        // Auto-refresh on setModel populates 2 rows (case-insensitive "sku" -> "${SKU}")
        QTRY_COMPARE_WITH_TIMEOUT( tableModel->rowCount(), 2, 5000 );
        QCOMPARE( tableModel->jobAt( 0 )->values.value( QStringLiteral( "SKU" ) ), QStringLiteral( "A-100" ) );
        QCOMPARE( tableModel->jobAt( 0 )->values.value( QStringLiteral( "Price" ) ), QStringLiteral( "9.99" ) );

        // User edits row 0 Qty -> 5, types "Rush" into unlocked/non-source Note column,
        // and unchecks row 1 Print.
        const int noteCol = FillJobModel::FIRST_FIELD_COLUMN + tableModel->fieldNames().indexOf( QStringLiteral( "Note" ) );
        QVERIFY( noteCol >= FillJobModel::FIRST_FIELD_COLUMN );
        QVERIFY( tableModel->setData( tableModel->index( 0, FillJobModel::QUANTITY_COLUMN ), 5, Qt::EditRole ) );
        QVERIFY( tableModel->setData( tableModel->index( 0, noteCol ), QStringLiteral( "Rush" ), Qt::EditRole ) );
        QVERIFY( tableModel->setData( tableModel->index( 1, FillJobModel::PRINT_COLUMN ), Qt::Unchecked, Qt::CheckStateRole ) );

        // Redash now returns updated price for A-100 plus a brand new row C-300
        server.resultPayload = R"({
                "query_result": {
                        "data": {
                                "columns": [{"name": "sku"}, {"name": "Name"}, {"name": "Price"}],
                                "rows": [
                                        {"sku": "A-100", "Name": "Widget Pro", "Price": 12.50},
                                        {"sku": "B-200", "Name": "Gadget", "Price": 14.50},
                                        {"sku": "C-300", "Name": "Doohickey", "Price": 4.25}
                                ]
                        }
                }
        })";

        fillView.refreshExternalSource();
        QTRY_COMPARE_WITH_TIMEOUT( tableModel->rowCount(), 3, 5000 );

        // Row 0 refreshed Name + Price, preserved Qty=5 and Note="Rush"
        QCOMPARE( tableModel->jobAt( 0 )->values.value( QStringLiteral( "Name" ) ), QStringLiteral( "Widget Pro" ) );
        QCOMPARE( tableModel->jobAt( 0 )->values.value( QStringLiteral( "Price" ) ), QStringLiteral( "12.5" ) );
        QCOMPARE( tableModel->jobAt( 0 )->quantity, 5 );
        QCOMPARE( tableModel->jobAt( 0 )->values.value( QStringLiteral( "Note" ) ), QStringLiteral( "Rush" ) );

        // Row 1 preserved Print=false
        QCOMPARE( tableModel->jobAt( 1 )->doPrint, false );

        // Row 2 added with default Qty=1, Print=true
        QCOMPARE( tableModel->jobAt( 2 )->values.value( QStringLiteral( "SKU" ) ), QStringLiteral( "C-300" ) );
        QCOMPARE( tableModel->jobAt( 2 )->quantity, 1 );
        QCOMPARE( tableModel->jobAt( 2 )->doPrint, true );
}
