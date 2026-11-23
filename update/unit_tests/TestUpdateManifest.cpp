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
#include <QCryptographicHash>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>


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

        // The edge release is always called "edge", so are the download URLs under it.
        QJsonObject edgeManifest()
        {
                QJsonObject a = validAsset();
                a["url"] = "https://github.com/" + REPO + "/releases/download/edge/" + a["name"].toString();

                QJsonObject o = validManifest();
                o["channel"] = "edge";
                o["version"] = "edge";
                o["assets"]  = QJsonObject{ { "windows-x86_64", a } };
                return o;
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
        QJsonObject o = edgeManifest();
        o["commit_time"] = 2000;
        auto m = parsed( o, "edge" );

        QCOMPARE( m.id(), COMMIT );
        QCOMPARE( m.displayVersion(), QString( "edge 0123456" ) );

        QVERIFY( isUpdateAvailable( BuildIdentity{ "edge", "", OTHER, 1000 }, m ) );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", COMMIT, 1000 }, m ) );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", COMMIT.toUpper() }, m ) );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", "" }, m ) );     // build does not know itself
}

void TestUpdateManifest::edgeNeverMovesBackwards()
{
        QJsonObject o = edgeManifest();
        o["commit_time"] = 2000;
        auto m = parsed( o, "edge" );
        QCOMPARE( m.commitTime, qint64( 2000 ) );

        QVERIFY(  isUpdateAvailable( BuildIdentity{ "edge", "", OTHER, 1999 }, m ) );    // older build: update
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", OTHER, 2000 }, m ) );    // same time, other commit: no
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", OTHER, 2001 }, m ) );    // newer build: stale or replayed manifest
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "", COMMIT, 1000 }, m ) );   // same commit: nothing to do
        QVERIFY(  isUpdateAvailable( BuildIdentity{ "edge", "", OTHER, 0 }, m ) );       // build time unknown
}


void TestUpdateManifest::edgeManifestNeedsACommitTime()
{
        QJsonObject o = edgeManifest();

        QVERIFY( !UpdateManifest::parse( toJson( o ), REPO, "edge" ).has_value() );      // none
        o["commit_time"] = 0;
        QVERIFY( !UpdateManifest::parse( toJson( o ), REPO, "edge" ).has_value() );
        o["commit_time"] = 1700000000;
        QVERIFY( UpdateManifest::parse( toJson( o ), REPO, "edge" ).has_value() );

        o["version"] = "v1.3.0";                                                          // edge is only ever "edge"
        QVERIFY( !UpdateManifest::parse( toJson( o ), REPO, "edge" ).has_value() );
}


void TestUpdateManifest::rejectsSneakyDownloadUrls_data()
{
        QTest::addColumn<QString>( "url" );

        const QString good = "https://github.com/" + REPO + "/releases/download/v1.3.0/glabels-neo-windows-x64.zip";
        QTest::newRow( "dot dot" )        << "https://github.com/" + REPO + "/releases/download/v1.3.0/../../../../other/repo/releases/download/v1.3.0/glabels-neo-windows-x64.zip";
        QTest::newRow( "escaped dots" )   << "https://github.com/" + REPO + "/releases/download/%2e%2e/v1.3.0/glabels-neo-windows-x64.zip";
        QTest::newRow( "escaped slash" )  << "https://github.com/" + REPO + "/releases/download/v1.3.0%2f/glabels-neo-windows-x64.zip";
        QTest::newRow( "other tag" )      << "https://github.com/" + REPO + "/releases/download/v9.9.9/glabels-neo-windows-x64.zip";
        QTest::newRow( "extra segment" )  << "https://github.com/" + REPO + "/releases/download/v1.3.0/x/glabels-neo-windows-x64.zip";
        QTest::newRow( "empty segment" )  << "https://github.com/" + REPO + "/releases/download//glabels-neo-windows-x64.zip";
        QTest::newRow( "query" )          << good + "?token=1";
        QTest::newRow( "fragment" )       << good + "#x";
        QTest::newRow( "port" )           << "https://github.com:8443/" + REPO + "/releases/download/v1.3.0/glabels-neo-windows-x64.zip";
        QTest::newRow( "credentials" )    << "https://user:pw@github.com/" + REPO + "/releases/download/v1.3.0/glabels-neo-windows-x64.zip";
        QTest::newRow( "look-alike host" ) << "https://github.com.evil.example/" + REPO + "/releases/download/v1.3.0/glabels-neo-windows-x64.zip";
}


void TestUpdateManifest::rejectsSneakyDownloadUrls()
{
        QFETCH( QString, url );

        QJsonObject a = validAsset();
        a["url"] = url;
        QJsonObject o = validManifest();
        o["assets"] = QJsonObject{ { "windows-x86_64", a } };
        QVERIFY2( !UpdateManifest::parse( toJson( o ), REPO, "stable" ).has_value(), qPrintable( url ) );

        QVERIFY( UpdateManifest::parse( toJson( validManifest() ), REPO, "stable" ).has_value() );   // the plain URL is fine
}


void TestUpdateManifest::versionMustLookLikeAVersion()
{
        // The version is shown to the user, so markup or free text in it is refused outright.
        for ( const QString& bad : { "v1.3.0<a href=\"https://evil.example\">click</a>", "v1.3.0 (security update!)", "v1", "1.2.3.4.5", "v1.3.0\n" } )
        {
                QJsonObject o = validManifest();
                o["version"] = bad;
                QVERIFY2( !UpdateManifest::parse( toJson( o ), REPO, "stable" ).has_value(), qPrintable( bad ) );
        }
        for ( const QString& good : { "v1.3.0", "1.3", "v10.20.30.40" } )
        {
                QJsonObject a = validAsset();
                a["url"] = "https://github.com/" + REPO + "/releases/download/" + good + "/glabels-neo-windows-x64.zip";
                QJsonObject o = validManifest();
                o["version"] = good;
                o["assets"] = QJsonObject{ { "windows-x86_64", a } };
                QVERIFY2( UpdateManifest::parse( toJson( o ), REPO, "stable" ).has_value(), qPrintable( good ) );
        }
}



void TestUpdateManifest::channelMismatchIsNeverAvailable()
{
        auto m = parsed( validManifest() );
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "", "v1.0.0", OTHER }, m ) );      // self-update off
        QVERIFY( !isUpdateAvailable( BuildIdentity{ "edge", "v1.0.0", OTHER }, m ) );  // stable manifest, edge build
}


void TestUpdateManifest::parsesWhatTheReleaseScriptProduces()
{
        // The contract between the release workflow (scripts/make-update-manifest.sh) and this parser.
        QString bash = qEnvironmentVariable( "GLABELS_TEST_BASH" );
#ifndef Q_OS_WIN
        if ( bash.isEmpty() )
        {
                bash = QStandardPaths::findExecutable( "bash" );   // (on Windows "bash" may be the WSL stub)
        }
#endif
        if ( bash.isEmpty() || QStandardPaths::findExecutable( "jq" ).isEmpty() )
        {
                QSKIP( "needs bash and jq" );
        }

        QTemporaryDir dist;
        QVERIFY( dist.isValid() );
        const QByteArray zip( 4321, 'z' );
        const QByteArray tgz( 8765, 't' );
        for ( auto [name, content] : { std::pair<QString,QByteArray>{ "glabels-neo-windows-x64.zip", zip },
                                       std::pair<QString,QByteArray>{ "glabels-neo-linux-x86_64.tar.gz", tgz } } )
        {
                QFile f( dist.path() + "/" + name );
                QVERIFY( f.open( QIODevice::WriteOnly ) );
                f.write( content );
        }

        QProcess script;
        script.start( bash, { MAKE_MANIFEST_SCRIPT, dist.path(), "stable", "v1.3.0", COMMIT, "SchneiderWasTaken/glabels-neo", "1700000000" } );
        QVERIFY( script.waitForFinished( 30000 ) );
        QVERIFY2( script.exitCode() == 0, qPrintable( script.readAllStandardError() ) );

        QFile out( dist.path() + "/glabels-neo-update-manifest.json" );
        QVERIFY( out.open( QIODevice::ReadOnly ) );

        QString error;
        auto m = UpdateManifest::parse( out.readAll(), "SchneiderWasTaken/glabels-neo", "stable", true, &error );
        QVERIFY2( m.has_value(), qPrintable( error ) );

        QCOMPARE( m->version, QString( "v1.3.0" ) );
        QCOMPARE( m->commit, COMMIT );
        QCOMPARE( m->assets.size(), 2 );                         // no macOS package was given
        QVERIFY( !m->assetFor( "macos-arm64" ) );

        const UpdateAsset* win = m->assetFor( "windows-x86_64" );
        QVERIFY( win );
        QCOMPARE( win->size, qint64( zip.size() ) );
        QCOMPARE( win->sha256, QString::fromLatin1( QCryptographicHash::hash( zip, QCryptographicHash::Sha256 ).toHex() ) );
        QCOMPARE( win->url.toString(), QString( "https://github.com/SchneiderWasTaken/glabels-neo/releases/download/v1.3.0/glabels-neo-windows-x64.zip" ) );

        // The same for an edge release, which is ordered by the commit time the script records
        QProcess edge;
        edge.start( bash, { MAKE_MANIFEST_SCRIPT, dist.path(), "edge", "edge", COMMIT, "SchneiderWasTaken/glabels-neo", "1700000000" } );
        QVERIFY( edge.waitForFinished( 30000 ) );
        QVERIFY2( edge.exitCode() == 0, qPrintable( edge.readAllStandardError() ) );

        QFile edgeOut( dist.path() + "/glabels-neo-update-manifest.json" );
        QVERIFY( edgeOut.open( QIODevice::ReadOnly ) );
        auto e = UpdateManifest::parse( edgeOut.readAll(), "SchneiderWasTaken/glabels-neo", "edge", true, &error );
        QVERIFY2( e.has_value(), qPrintable( error ) );
        QCOMPARE( e->commitTime, qint64( 1700000000 ) );
        QCOMPARE( e->assetFor( "windows-x86_64" )->url.path(), QString( "/SchneiderWasTaken/glabels-neo/releases/download/edge/glabels-neo-windows-x64.zip" ) );

        // and without a commit time it asks git, which does not know this made-up commit: refuse rather than guess
        QProcess noTime;
        noTime.start( bash, { MAKE_MANIFEST_SCRIPT, dist.path(), "edge", "edge", COMMIT, "SchneiderWasTaken/glabels-neo" } );
        QVERIFY( noTime.waitForFinished( 30000 ) );
        QVERIFY( noTime.exitCode() != 0 );
}
