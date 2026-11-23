//  TestUpdateService.cpp
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

//
// UpdateService against a local web server: the real check / download / verify / unpack / smoke-test
// pipeline with real archives and the system's unpacking tool.  (Swapping the result into place is the
// job of glabels-updater and is tested in TestUpdaterHelper.)
//

#include "TestUpdateService.hpp"

#include "update/UpdateService.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <memory>


QTEST_MAIN(TestUpdateService)

using namespace glabels::update;
using CheckResult = UpdateService::CheckResult;
using State       = UpdateService::State;


namespace
{
        const QString COMMIT = "0123456789abcdef0123456789abcdef01234567";
        const QString OTHER  = "fedcba9876543210fedcba9876543210fedcba98";


        ///
        /// Just enough HTTP to serve a few canned responses on localhost
        ///
        class WebServer
        {
        public:
                struct Route
                {
                        int        status{200};
                        QByteArray body;
                };

                QMap<QString,Route> routes;
                QStringList         requests;

                WebServer()
                {
                        mServer.listen( QHostAddress::LocalHost, 0 );
                        QObject::connect( &mServer, &QTcpServer::newConnection, [this]()
                        {
                                while ( QTcpSocket* s = mServer.nextPendingConnection() )
                                {
                                        QObject::connect( s, &QTcpSocket::readyRead, [this, s]() { respond( s ); } );
                                        QObject::connect( s, &QTcpSocket::disconnected, s, &QObject::deleteLater );
                                }
                        } );
                }

                bool isListening() const { return mServer.isListening(); }
                QString url( const QString& path ) const { return QString( "http://127.0.0.1:%1%2" ).arg( mServer.serverPort() ).arg( path ); }

        private:
                void respond( QTcpSocket* s )
                {
                        QByteArray request = s->readAll();
                        if ( !request.contains( "\r\n\r\n" ) )
                        {
                                return;
                        }
                        const QString path = QString::fromLatin1( request.split( ' ' ).value( 1 ) );
                        requests << path;

                        Route r = routes.value( path, Route{ 404, "not found" } );
                        s->write( QString( "HTTP/1.1 %1 X\r\nContent-Length: %2\r\nConnection: close\r\n"
                                           "Content-Type: application/octet-stream\r\n\r\n" ).arg( r.status ).arg( r.body.size() ).toLatin1() );
                        s->write( r.body );
                        s->disconnectFromHost();
                }

                QTcpServer mServer;
        };


        struct Env
        {
                std::unique_ptr<QTemporaryDir> tmp;
                std::unique_ptr<WebServer>     server;
                QString                        root;        // fake installation: <tmp>/n/glabels-neo
                int                            counter{0};
        };

        Env g;


        void writeFile( const QString& path, const QByteArray& content )
        {
                QDir().mkpath( QFileInfo( path ).absolutePath() );
                QFile f( path );
                QVERIFY( f.open( QIODevice::WriteOnly ) );
                f.write( content );
        }

        QByteArray readFile( const QString& path )
        {
                QFile f( path );
                return f.open( QIODevice::ReadOnly ) ? f.readAll() : QByteArray( "<missing>" );
        }

        QString packageName()
        {
                return currentOs() == Os::Windows ? "glabels-neo-windows-x64.zip" : "glabels-neo-linux-x86_64.tar.gz";
        }

        QString smokeFile( const QString& tree )
        {
                return currentOs() == Os::Windows ? tree + "/bin/glabels-batch-qt.exe" : tree + "/glabels-batch-qt";
        }


        /// A release package like the real ones (same layout, same archive format), made with the system's tar.
        QByteArray buildPackage( bool smokeTestWorks )
        {
                const bool win = currentOs() == Os::Windows;
                const QString work = g.tmp->path() + "/pkgwork-" + QString::number( ++g.counter );
                const QString tree = work + "/glabels-neo";

                writeFile( tree + "/bin/version.txt", "new" );
                writeFile( tree + ( win ? "/bin/glabels-qt.exe" : "/bin/glabels-qt" ), "main" );
                writeFile( tree + ( win ? "/bin/glabels-updater.exe" : "/bin/glabels-updater" ), "helper" );

                if ( smokeTestWorks )
                {
                        if ( win )
                        {
                                QFile::copy( FAKE_BATCH_EXE, smokeFile( tree ) );
                        }
                        else
                        {
                                writeFile( smokeFile( tree ), "#!/bin/sh\necho '> GLABELS NEO'\n" );
                        }
                }
                else
                {
                        writeFile( smokeFile( tree ), win ? QByteArray( "this is not a program" ) : QByteArray( "#!/bin/sh\nexit 1\n" ) );
                }
                QFile::setPermissions( smokeFile( tree ), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner );

                const QString archive = work + "/" + packageName();
                int rc;
                if ( win )
                {
                        rc = QProcess::execute( qEnvironmentVariable( "SystemRoot", "C:/Windows" ) + "/System32/tar.exe",
                                                { "-a", "-cf", archive, "-C", tree, "." } );
                }
                else
                {
                        rc = QProcess::execute( "tar", { "-czf", archive, "-C", work, "glabels-neo" } );
                }
                return rc == 0 ? readFile( archive ) : QByteArray();
        }


        QByteArray manifestFor( const QString& version, const QByteArray& package, const QString& sha256 = QString() )
        {
                const QString digest = sha256.isEmpty()
                                       ? QString::fromLatin1( QCryptographicHash::hash( package, QCryptographicHash::Sha256 ).toHex() )
                                       : sha256;
                QJsonObject asset{
                        { "name", packageName() },
                        { "url", g.server->url( "/pkg/" + packageName() ) },
                        { "sha256", digest },
                        { "size", package.size() },
                };
                return QJsonDocument( QJsonObject{
                        { "schema", 1 },
                        { "channel", "stable" },
                        { "version", version },
                        { "commit", COMMIT },
                        { "published", "2026-11-22T12:00:00Z" },
                        { "assets", QJsonObject{ { platformKey(), asset } } },
                } ).toJson();
        }


        std::unique_ptr<UpdateService> makeService( const QString& channel = "stable", const QString& tag = "v1.0.0" )
        {
                UpdateServiceConfig c;
                c.build       = BuildIdentity{ channel, tag, OTHER };
                c.platformKey = platformKey();
                c.repo        = "owner/project";
                c.manifestUrl = QUrl( g.server->url( "/manifest.json" ) );
                c.strictUrls  = false;
                c.layout      = InstallLayout::detect( g.root + ( currentOs() == Os::Windows ? "/bin/glabels-qt.exe" : "/bin/glabels-qt" ),
                                                       currentOs(), true );
                return std::make_unique<UpdateService>( c );
        }


        CheckResult resultOf( const QSignalSpy& spy, int index = 0 )
        {
                return spy.at( index ).at( 0 ).value<CheckResult>();
        }
}


///////////////////////////////////////////////////////////////////////////////////////
// Fixture
///////////////////////////////////////////////////////////////////////////////////////

void TestUpdateService::initTestCase()
{
        if ( platformKey().isEmpty() || currentOs() == Os::MacOs )
        {
                QSKIP( "no self-update package for this platform / macOS needs hdiutil" );
        }

        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-test" );
        QCoreApplication::setApplicationName( "update-test" );

        g.tmp = std::make_unique<QTemporaryDir>();
        QSettings::setDefaultFormat( QSettings::IniFormat );
        QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, g.tmp->path() + "/settings" );

        g.server = std::make_unique<WebServer>();
        QVERIFY( g.server->isListening() );
}


void TestUpdateService::init()
{
        QSettings().clear();
        UpdateSettings::setAutoCheck( true );     // most tests exercise background checks; consent has its own test
        QDir( UpdateStore::dir() ).removeRecursively();
        g.server->routes.clear();
        g.server->requests.clear();

        // A fake installation of the old version
        const bool win = currentOs() == Os::Windows;
        g.root = g.tmp->path() + "/install-" + QString::number( ++g.counter ) + "/glabels-neo";
        writeFile( g.root + "/bin/version.txt", "old" );
        writeFile( g.root + ( win ? "/bin/glabels-qt.exe" : "/bin/glabels-qt" ), "main" );
        writeFile( g.root + ( win ? "/bin/glabels-updater.exe" : "/bin/glabels-updater" ), "helper" );
}


void TestUpdateService::cleanup()
{
        QDir( UpdateStore::dir() ).removeRecursively();
}


///////////////////////////////////////////////////////////////////////////////////////
// Checking
///////////////////////////////////////////////////////////////////////////////////////

void TestUpdateService::disabledBuildDoesNothing()
{
        auto svc = makeService( "" );     // not a release build: no channel
        QVERIFY( !svc->isSupported() );
        QVERIFY( !svc->unsupportedReason().isEmpty() );
        QCOMPARE( svc->state(), State::Disabled );

        QSignalSpy spy( svc.get(), &UpdateService::checkFinished );
        svc->checkNow( true );
        QCOMPARE( spy.count(), 1 );
        QCOMPARE( resultOf( spy ), CheckResult::Unsupported );
        QVERIFY( g.server->requests.isEmpty() );          // never talks to the network
        QVERIFY( !svc->applyPendingAtStartup( {} ) );
}


void TestUpdateService::upToDateWhenManifestIsNotNewer()
{
        g.server->routes["/manifest.json"] = { 200, manifestFor( "v1.0.0", "x" ) };

        auto svc = makeService( "stable", "v1.0.0" );
        QVERIFY2( svc->isSupported(), qPrintable( svc->unsupportedReason() ) );

        QSignalSpy spy( svc.get(), &UpdateService::checkFinished );
        svc->checkNow( true );
        QVERIFY( spy.wait( 10000 ) );

        QCOMPARE( resultOf( spy ), CheckResult::UpToDate );
        QVERIFY( !svc->availableUpdate().has_value() );
        QCOMPARE( svc->state(), State::Idle );
        QVERIFY( UpdateSettings::lastCheck().isValid() );
}


void TestUpdateService::reportsBrokenOrMissingManifest()
{
        auto svc = makeService();
        QSignalSpy spy( svc.get(), &UpdateService::checkFinished );

        // no manifest published (e.g. a release from before the updater existed)
        svc->checkNow( true );
        QVERIFY( spy.wait( 10000 ) );
        QCOMPARE( resultOf( spy, 0 ), CheckResult::Failed );
        QVERIFY( !spy.at( 0 ).at( 2 ).toString().isEmpty() );

        // an HTML error page instead of a manifest
        g.server->routes["/manifest.json"] = { 200, "<html>Service Unavailable</html>" };
        svc->checkNow( true );
        QVERIFY( spy.wait( 10000 ) );
        QCOMPARE( resultOf( spy, 1 ), CheckResult::Failed );

        QCOMPARE( svc->state(), State::Idle );
        QVERIFY( !UpdateSettings::lastCheck().isValid() );     // failures do not count as "checked"
}


///////////////////////////////////////////////////////////////////////////////////////
// Installing (download, verify, unpack, smoke test)
///////////////////////////////////////////////////////////////////////////////////////

void TestUpdateService::downloadsVerifiesAndStagesAnUpdate()
{
        const QByteArray pkg = buildPackage( true );
        QVERIFY( !pkg.isEmpty() );
        g.server->routes["/manifest.json"]                 = { 200, manifestFor( "v1.1.0", pkg ) };
        g.server->routes["/pkg/" + packageName()]          = { 200, pkg };

        auto svc = makeService( "stable", "v1.0.0" );
        QSignalSpy checked( svc.get(), &UpdateService::checkFinished );
        QSignalSpy offered( svc.get(), &UpdateService::updateAvailable );
        QSignalSpy ready( svc.get(), &UpdateService::updateReady );
        QSignalSpy failed( svc.get(), &UpdateService::installFailed );

        svc->checkNow( true );
        QVERIFY( checked.wait( 10000 ) );
        QCOMPARE( resultOf( checked ), CheckResult::UpdateAvailable );
        QCOMPARE( offered.count(), 1 );
        QCOMPARE( svc->state(), State::Available );
        QVERIFY( svc->availableUpdate().has_value() );
        QCOMPARE( svc->availableUpdate()->version, QString( "v1.1.0" ) );

        svc->install();
        QVERIFY2( ready.wait( 60000 ), qPrintable( failed.isEmpty() ? QString( "timed out" ) : failed.at( 0 ).at( 0 ).toString() ) );
        QCOMPARE( failed.count(), 0 );

        QCOMPARE( svc->state(), State::Ready );
        QCOMPARE( ready.at( 0 ).at( 0 ).toString(), QString( "v1.1.0" ) );

        // staged next to the install, complete, and the running install is untouched
        const auto& unit = svc->layout().units[0];
        QCOMPARE( readFile( unit.staged + "/bin/version.txt" ), QByteArray( "new" ) );
        QCOMPARE( readFile( g.root + "/bin/version.txt" ), QByteArray( "old" ) );
        QCOMPARE( svc->layout().validateStaged(), QString() );

        auto pending = svc->pendingUpdate();
        QVERIFY( pending.has_value() );
        QCOMPARE( pending->id, QString( "v1.1.0" ) );
        QCOMPARE( pending->attempts, 0 );

        // the package file is not kept
        QVERIFY( QDir( UpdateStore::downloadDir() ).isEmpty() );

        // "next start": a new service sees the staged update and does not offer it again
        auto next = makeService( "stable", "v1.0.0" );
        QCOMPARE( next->pendingUpdate()->id, QString( "v1.1.0" ) );
        QSignalSpy nextChecked( next.get(), &UpdateService::checkFinished );
        QSignalSpy nextOffered( next.get(), &UpdateService::updateAvailable );
        next->checkNow( false );
        QVERIFY( nextChecked.wait( 10000 ) );
        QCOMPARE( resultOf( nextChecked ), CheckResult::UpdateReady );
        QCOMPARE( nextOffered.count(), 0 );
}


void TestUpdateService::rejectsACorruptDownload()
{
        const QByteArray pkg = buildPackage( true );
        QVERIFY( !pkg.isEmpty() );
        QByteArray tampered = pkg;
        tampered[tampered.size() / 2] = static_cast<char>( tampered[tampered.size() / 2] ^ 0x5a );   // same size, other content

        g.server->routes["/manifest.json"]        = { 200, manifestFor( "v1.1.0", pkg ) };
        g.server->routes["/pkg/" + packageName()] = { 200, tampered };

        auto svc = makeService();
        QSignalSpy checked( svc.get(), &UpdateService::checkFinished );
        QSignalSpy failed( svc.get(), &UpdateService::installFailed );
        QSignalSpy ready( svc.get(), &UpdateService::updateReady );

        svc->checkNow( true );
        QVERIFY( checked.wait( 10000 ) );
        svc->install();
        QVERIFY( failed.wait( 60000 ) );

        QVERIFY( failed.at( 0 ).at( 0 ).toString().contains( "checksum" ) );
        QCOMPARE( ready.count(), 0 );
        QVERIFY( !svc->pendingUpdate().has_value() );
        QVERIFY( !QFileInfo::exists( svc->layout().units[0].staged ) );        // nothing was unpacked
        QVERIFY( QDir( UpdateStore::downloadDir() ).isEmpty() );               // nor kept
        QCOMPARE( svc->state(), State::Available );                            // may try again later
        QCOMPARE( UpdateSettings::failedId(), QString() );                     // a bad transfer is not held against the release

        // truncated transfer: wrong size
        g.server->routes["/pkg/" + packageName()] = { 200, pkg.left( pkg.size() - 10 ) };
        svc->install();
        QVERIFY( failed.wait( 60000 ) );
        QCOMPARE( failed.count(), 2 );
        QVERIFY( !svc->pendingUpdate().has_value() );
}


void TestUpdateService::rejectsAPackageThatDoesNotStart()
{
        const QByteArray pkg = buildPackage( false );      // unpacks fine, but the new version cannot run
        QVERIFY( !pkg.isEmpty() );
        g.server->routes["/manifest.json"]        = { 200, manifestFor( "v1.1.0", pkg ) };
        g.server->routes["/pkg/" + packageName()] = { 200, pkg };

        auto svc = makeService();
        QSignalSpy checked( svc.get(), &UpdateService::checkFinished );
        QSignalSpy failed( svc.get(), &UpdateService::installFailed );
        QSignalSpy ready( svc.get(), &UpdateService::updateReady );

        svc->checkNow( true );
        QVERIFY( checked.wait( 10000 ) );
        svc->install();
        QVERIFY( failed.wait( 60000 ) );

        QVERIFY2( failed.at( 0 ).at( 0 ).toString().contains( "does not start" ), qPrintable( failed.at( 0 ).at( 0 ).toString() ) );
        QCOMPARE( ready.count(), 0 );
        QVERIFY( !svc->pendingUpdate().has_value() );
        QVERIFY( !QFileInfo::exists( svc->layout().units[0].staged ) );        // thrown away, not left lying around
        QCOMPARE( readFile( g.root + "/bin/version.txt" ), QByteArray( "old" ) );
        QCOMPARE( UpdateSettings::failedId(), QString( "v1.1.0" ) );           // and not retried automatically
        QCOMPARE( svc->state(), State::Idle );
}


void TestUpdateService::automaticChecksRespectSkipAndFailure()
{
        g.server->routes["/manifest.json"] = { 200, manifestFor( "v1.1.0", "x" ) };

        auto svc = makeService();
        QSignalSpy checked( svc.get(), &UpdateService::checkFinished );
        QSignalSpy offered( svc.get(), &UpdateService::updateAvailable );

        // not declined: an automatic check offers it
        svc->checkNow( false );
        QVERIFY( checked.wait( 10000 ) );
        QCOMPARE( offered.count(), 1 );
        QCOMPARE( offered.at( 0 ).at( 1 ).toBool(), false );

        // skipped / failed / snoozed: automatic checks stay quiet ...
        for ( int variant = 0; variant < 3; variant++ )
        {
                UpdateSettings::setSkippedId( variant == 0 ? "v1.1.0" : "" );
                UpdateSettings::setFailedId( variant == 1 ? "v1.1.0" : "" );
                UpdateSettings::snooze( variant == 2 ? "v1.1.0" : "other", QDateTime::currentDateTime().addDays( 1 ) );

                const int before = offered.count();
                svc->checkNow( false );
                QVERIFY( checked.wait( 10000 ) );
                QCOMPARE( offered.count(), before );
                QCOMPARE( resultOf( checked, checked.count() - 1 ), CheckResult::UpdateAvailable );   // (still reported)
        }

        // ... but a check the user asked for always shows the offer
        const int before = offered.count();
        svc->checkNow( true );
        QVERIFY( checked.wait( 10000 ) );
        QCOMPARE( offered.count(), before + 1 );

        // a *newer* release than the skipped one is offered again
        UpdateSettings::setSkippedId( "v1.0.5" );
        UpdateSettings::setFailedId( "" );
        UpdateSettings::snooze( "", QDateTime() );
        svc->checkNow( false );
        QVERIFY( checked.wait( 10000 ) );
        QCOMPARE( offered.count(), before + 2 );

        // "Skip this version" from the offer
        svc->skipAvailable();
        QCOMPARE( UpdateSettings::skippedId(), QString( "v1.1.0" ) );
        QCOMPARE( svc->state(), State::Idle );
}


void TestUpdateService::autoInstallNeedsNoOffer()
{
        const QByteArray pkg = buildPackage( true );
        QVERIFY( !pkg.isEmpty() );
        g.server->routes["/manifest.json"]        = { 200, manifestFor( "v1.1.0", pkg ) };
        g.server->routes["/pkg/" + packageName()] = { 200, pkg };
        UpdateSettings::setAutoInstall( true );

        auto svc = makeService();
        QSignalSpy offered( svc.get(), &UpdateService::updateAvailable );
        QSignalSpy ready( svc.get(), &UpdateService::updateReady );
        QSignalSpy failed( svc.get(), &UpdateService::installFailed );

        svc->checkNow( false );
        QVERIFY2( ready.wait( 60000 ), qPrintable( failed.isEmpty() ? QString( "timed out" ) : failed.at( 0 ).at( 0 ).toString() ) );

        QCOMPARE( offered.count(), 0 );                          // the user was not asked, and not disturbed
        QCOMPARE( svc->pendingUpdate()->id, QString( "v1.1.0" ) );
}


void TestUpdateService::backgroundChecksNeedConsent()
{
        g.server->routes["/manifest.json"] = { 200, manifestFor( "v1.1.0", "x" ) };
        QSettings().clear();                                  // a fresh installation: nobody has been asked

        auto svc = makeService();
        QVERIFY( svc->isSupported() );
        QVERIFY( !UpdateSettings::autoCheck() );
        QVERIFY( !UpdateSettings::consentAsked() );

        QSignalSpy asked( svc.get(), &UpdateService::consentRequired );
        QSignalSpy checked( svc.get(), &UpdateService::checkFinished );

        // the periodic timer asks (once per session) instead of checking
        QMetaObject::invokeMethod( svc.get(), "onTimer", Qt::DirectConnection );
        QMetaObject::invokeMethod( svc.get(), "onTimer", Qt::DirectConnection );
        QCOMPARE( asked.count(), 1 );

        // and nothing may reach the network without consent, however the check is requested
        svc->checkNow( false );
        QTest::qWait( 300 );
        QCOMPARE( checked.count(), 0 );
        QVERIFY( g.server->requests.isEmpty() );

        // "No" is remembered: no more questions, still no network
        UpdateSettings::setAutoCheck( false );
        QVERIFY( UpdateSettings::consentAsked() );
        auto declined = makeService();
        QSignalSpy askedAgain( declined.get(), &UpdateService::consentRequired );
        QMetaObject::invokeMethod( declined.get(), "onTimer", Qt::DirectConnection );
        QCOMPARE( askedAgain.count(), 0 );
        QVERIFY( g.server->requests.isEmpty() );

        // a check the user asks for is theirs to ask for, whatever the setting
        svc->checkNow( true );
        QVERIFY( checked.wait( 10000 ) );
        QVERIFY( !g.server->requests.isEmpty() );

        // "Yes": background checks run
        g.server->requests.clear();
        UpdateSettings::setAutoCheck( true );
        svc->checkNow( false );
        QVERIFY( checked.wait( 10000 ) );
        QVERIFY( !g.server->requests.isEmpty() );
}


///////////////////////////////////////////////////////////////////////////////////////
// Start-up bookkeeping
///////////////////////////////////////////////////////////////////////////////////////

void TestUpdateService::recognizesAnAppliedUpdateAtStartup()
{
        // The previous run staged v1.1.0, the helper swapped it in, and this process *is* v1.1.0.
        auto svc = makeService( "stable", "v1.1.0" );
        PendingUpdate p;
        p.id = "v1.1.0";
        p.version = "v1.1.0";
        p.channel = "stable";
        p.stagedAt = QDateTime::currentDateTime();
        QVERIFY( p.save() );
        QDir().mkpath( svc->layout().units[0].staged );    // leftover of the old staging area

        QVERIFY( !svc->applyPendingAtStartup( {} ) );      // nothing to do: carry on starting normally

        QCOMPARE( svc->justUpdatedTo(), QString( "v1.1.0" ) );
        QVERIFY( !PendingUpdate::load().has_value() );
        QVERIFY( !QFileInfo::exists( svc->layout().units[0].staged ) );
}


void TestUpdateService::dropsObsoleteOrBrokenPendingUpdates()
{
        PendingUpdate p;
        p.id = "v1.1.0";
        p.version = "v1.1.0";
        p.channel = "stable";
        p.stagedAt = QDateTime::currentDateTime();

        // the user installed something newer by hand in the meantime: never "update" backwards
        {
                auto svc = makeService( "stable", "v1.2.0" );
                QVERIFY( p.save() );
                QDir().mkpath( svc->layout().units[0].staged );
                QVERIFY( !svc->applyPendingAtStartup( {} ) );
                QVERIFY( svc->justUpdatedTo().isEmpty() );
                QVERIFY( !PendingUpdate::load().has_value() );
        }

        // the staged copy has disappeared (cleaned up, install moved): forget the record
        {
                auto svc = makeService( "stable", "v1.0.0" );
                QVERIFY( p.save() );
                QVERIFY( !svc->applyPendingAtStartup( {} ) );
                QVERIFY( !PendingUpdate::load().has_value() );
        }
}


void TestUpdateService::givesUpAfterRepeatedFailedSwaps()
{
        // Two swaps were attempted and the program is still the old version: stop, do not loop.
        auto svc = makeService( "stable", "v1.0.0" );
        PendingUpdate p;
        p.id = "v1.1.0";
        p.version = "v1.1.0";
        p.channel = "stable";
        p.stagedAt = QDateTime::currentDateTime();
        p.attempts = 2;
        QVERIFY( p.save() );
        QDir().mkpath( svc->layout().units[0].staged );

        QVERIFY( !svc->applyPendingAtStartup( { "label.glabels" } ) );      // -> start normally, no helper

        QVERIFY( !PendingUpdate::load().has_value() );
        QVERIFY( !QFileInfo::exists( svc->layout().units[0].staged ) );
        QCOMPARE( UpdateSettings::failedId(), QString( "v1.1.0" ) );
}
