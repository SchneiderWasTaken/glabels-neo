//  TestUpdateManifest.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//


#include "TestUpdateManifest.hpp"

#include "update/UpdateManifest.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


QTEST_MAIN(TestUpdateManifest)

using namespace glabels::update;


namespace
{
        const QString REPO   = "owner/project";
        const QString COMMIT = "0123456789abcdef0123456789abcdef01234567";
        const QString OTHER  = "fedcba9876543210fedcba9876543210fedcba98";
        const QString DIGEST = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

        QJsonObject validAsset( const QString& name = "glabels-neo-windows-x64.zip" )
        {
                return QJsonObject{
                        { "name", name },
                        { "url", "https://github.com/" + REPO + "/releases/download/v1.3.0/" + name },
                        { "sha256", DIGEST },
                        { "size", 12345 },
                };
        }

        QJsonObject validManifest()
        {
                return QJsonObject{
                        { "schema", 1 },
                        { "channel", "stable" },
                        { "version", "v1.3.0" },
                        { "commit", COMMIT },
                        { "published", "2026-11-22T12:00:00Z" },
                        { "notes_url", "https://github.com/" + REPO + "/releases/tag/v1.3.0" },
                        { "assets", QJsonObject{ { "windows-x86_64", validAsset() } } },
                };
        }

        QByteArray toJson( const QJsonObject& o )
        {
                return QJsonDocument( o ).toJson();
        }

        UpdateManifest parsed( const QJsonObject& o, const QString& channel = "stable" )
        {
                return UpdateManifest::parse( toJson( o ), REPO, channel ).value();
        }
}


void TestUpdateManifest::parsesValidManifest()
{
        QString error;
        auto m = UpdateManifest::parse( toJson( validManifest() ), REPO, "stable", true, &error );
        QVERIFY2( m.has_value(), qPrintable( error ) );

        QCOMPARE( m->channel, QString( "stable" ) );
        QCOMPARE( m->version, QString( "v1.3.0" ) );
        QCOMPARE( m->commit, COMMIT );
        QCOMPARE( m->id(), QString( "v1.3.0" ) );
        QCOMPARE( m->displayVersion(), QString( "v1.3.0" ) );
        QVERIFY( m->published.isValid() );
        QVERIFY( !m->notesUrl.isEmpty() );

        const UpdateAsset* a = m->assetFor( "windows-x86_64" );
        QVERIFY( a );
        QCOMPARE( a->name, QString( "glabels-neo-windows-x64.zip" ) );
        QCOMPARE( a->sha256, DIGEST );
        QCOMPARE( a->size, qint64( 12345 ) );
        QVERIFY( !m->assetFor( "linux-x86_64" ) );
}


void TestUpdateManifest::rejectsInvalidManifests_data()
{
        QTest::addColumn<QByteArray>( "json" );
        QTest::addColumn<QString>( "channel" );

        auto with = []( const QString& key, const QJsonValue& value )
        {
                QJsonObject o = validManifest();
                o[key] = value;
                return toJson( o );
        };
        auto withAsset = []( const QString& key, const QJsonValue& value )
        {
                QJsonObject a = validAsset();
                a[key] = value;
                QJsonObject o = validManifest();
                o["assets"] = QJsonObject{ { "windows-x86_64", a } };
                return toJson( o );
        };

        const QString goodName = "glabels-neo-windows-x64.zip";

        QTest::newRow( "not json" )           << QByteArray( "<html>rate limited</html>" ) << "stable";
        QTest::newRow( "not an object" )      << QByteArray( "[1,2,3]" ) << "stable";
        QTest::newRow( "future schema" )      << with( "schema", 2 ) << "stable";
        QTest::newRow( "no schema" )          << with( "schema", QJsonValue::Undefined ) << "stable";
        QTest::newRow( "other channel" )      << toJson( validManifest() ) << "edge";
        QTest::newRow( "unknown channel" )    << with( "channel", "nightly" ) << "nightly";
        QTest::newRow( "no version" )         << with( "version", "" ) << "stable";
        QTest::newRow( "tag is not version" ) << with( "version", "release-candidate" ) << "stable";
        QTest::newRow( "short commit" )       << with( "commit", "abc123" ) << "stable";
        QTest::newRow( "no assets" )          << with( "assets", QJsonObject() ) << "stable";
        QTest::newRow( "bad digest" )         << withAsset( "sha256", "XYZ" ) << "stable";
        QTest::newRow( "zero size" )          << withAsset( "size", 0 ) << "stable";
        QTest::newRow( "huge size" )          << withAsset( "size", 3.0e9 ) << "stable";
        QTest::newRow( "name with path" )     << withAsset( "name", "../evil.zip" ) << "stable";
        QTest::newRow( "hidden name" )        << withAsset( "name", ".evil.zip" ) << "stable";
        QTest::newRow( "http url" )           << withAsset( "url", "http://github.com/" + REPO + "/releases/download/v1.3.0/" + goodName ) << "stable";
        QTest::newRow( "other host" )         << withAsset( "url", "https://evil.example/" + REPO + "/releases/download/v1.3.0/" + goodName ) << "stable";
        QTest::newRow( "other repo" )         << withAsset( "url", "https://github.com/someone/else/releases/download/v1.3.0/" + goodName ) << "stable";
        QTest::newRow( "not a release url" )  << withAsset( "url", "https://github.com/" + REPO + "/archive/main.zip" ) << "stable";
        QTest::newRow( "url/name differ" )    << withAsset( "url", "https://github.com/" + REPO + "/releases/download/v1.3.0/other.zip" ) << "stable";
}


void TestUpdateManifest::rejectsInvalidManifests()
{
        QFETCH( QByteArray, json );
        QFETCH( QString, channel );

        QString error;
        auto m = UpdateManifest::parse( json, REPO, channel, true, &error );
        QVERIFY( !m.has_value() );
        QVERIFY( !error.isEmpty() );
}


void TestUpdateManifest::relaxedUrlsForTests()
{
        QJsonObject a = validAsset();
        a["url"] = "http://127.0.0.1:8080/pkg/glabels-neo-windows-x64.zip";
        QJsonObject o = validManifest();
        o["assets"] = QJsonObject{ { "windows-x86_64", a } };

        QVERIFY( !UpdateManifest::parse( toJson( o ), REPO, "stable", true ).has_value() );
        QVERIFY( UpdateManifest::parse( toJson( o ), REPO, "stable", false ).has_value() );
}


void TestUpdateManifest::compareTags()
{
        QVERIFY( glabels::update::compareTags( "v1.3.0", "v1.2.0" ) > 0 );
        QVERIFY( glabels::update::compareTags( "v1.2.0", "v1.3.0" ) < 0 );
        QCOMPARE( glabels::update::compareTags( "v1.2.0", "1.2.0" ), 0 );
        QVERIFY( glabels::update::compareTags( "v1.10.0", "v1.9.0" ) > 0 );     // numeric, not alphabetical
        QVERIFY( glabels::update::compareTags( "v2.0", "v1.99.99" ) > 0 );
        QCOMPARE( glabels::update::compareTags( "edge", "v1.0.0" ), 0 );        // unparsable: never "newer"
        QCOMPARE( glabels::update::compareTags( "", "" ), 0 );
}


void TestUpdateManifest::stableIsAvailableOnlyWhenNewer_data()
{
        QTest::addColumn<QString>( "running" );
        QTest::addColumn<bool>( "available" );

        QTest::newRow( "older" )          << "v1.2.0" << true;
        QTest::newRow( "much older" )     << "v0.9.9" << true;
        QTest::newRow( "same" )           << "v1.3.0" << false;
        QTest::newRow( "newer" )          << "v1.4.0" << false;
        QTest::newRow( "unknown tag" )    << "" << false;
        QTest::newRow( "weird tag" )      << "snapshot" << false;
}


void TestUpdateManifest::stableIsAvailableOnlyWhenNewer()
{
        QFETCH( QString, running );
        QFETCH( bool, available );

        BuildIdentity build{ "stable", running, OTHER };
        QCOMPARE( isUpdateAvailable( build, parsed( validManifest() ) ), available );
}


void TestUpdateManifest::edgeIsAvailableWhenCommitDiffers()
{
        QJsonObject o = validManifest();
        o["channel"] = "edge";
        o["version"] = "edge";
        auto m = parsed( o, "edge" );

        QCOMPARE( m.id(), COMMIT );
        QCOMPARE( m.displayVersion(), QString( "edge 0123456" ) );

        QVERIFY( isUpdateAvailable( BuildIdentity{ "edge", "", OTHER }, m ) );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", COMMIT }, m ) );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", COMMIT.toUpper() }, m ) );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", "" }, m ) );     // build does not know itself
}


void TestUpdateManifest::channelMismatchIsNeverAvailable()
{
        auto m = parsed( validManifest() );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "", "v1.0.0", OTHER }, m ) );      // self-update off
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "v1.0.0", OTHER }, m ) );  // stable manifest, edge build
}
