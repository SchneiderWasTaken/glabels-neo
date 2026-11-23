//  TestUpdateApply.cpp
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
// The last mile with real processes: a program installed in a directory has an update staged next to
// it, and quits (or starts).  The helper -- started by the service from a copy outside the install,
// waiting for the program to exit -- must replace the very directory the program was running from and
// bring the new version up.
//

#include "TestUpdateApply.hpp"

#include "update/UpdateService.hpp"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>


QTEST_MAIN(TestUpdateApply)

using namespace glabels::update;


namespace
{
        const bool WIN = ( currentOs() == Os::Windows );

        QTemporaryDir* gTmp = nullptr;
        QString        gRoot;       // the installation the harness runs from
        QString        gStaged;     // its staged update


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

        QString exeName( const QString& base ) { return WIN ? base + ".exe" : base; }
}


void TestUpdateApply::initTestCase()
{
        if ( platformKey().isEmpty() || currentOs() == Os::MacOs )
        {
                QSKIP( "no self-update package for this platform / macOS bundles are covered by the helper test" );
        }

        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-test" );
        QCoreApplication::setApplicationName( "update-test" );

        gTmp = new QTemporaryDir();
        QSettings::setDefaultFormat( QSettings::IniFormat );
        QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, gTmp->path() + "/settings" );
}


void TestUpdateApply::init()
{
        QDir( UpdateStore::dir() ).removeRecursively();

        // The old version: the harness is its main program, so it really is a running exe inside the directory
        // that is going to be replaced.
        static int n = 0;
        gRoot = gTmp->path() + "/install-" + QString::number( ++n ) + "/glabels-neo";
        QVERIFY( QDir().mkpath( gRoot + "/bin" ) );
        QVERIFY( QFile::copy( HARNESS_EXE, gRoot + "/bin/" + exeName( "glabels-qt" ) ) );
        QVERIFY( QFile::copy( UPDATER_EXE, gRoot + "/bin/" + exeName( "glabels-updater" ) ) );
        writeFile( gRoot + "/bin/version.txt", "old" );

        // The new version, staged: its main program records when it is started.
        gStaged = QFileInfo( gRoot ).absolutePath() + "/.glabels-neo.update";
        QVERIFY( QDir().mkpath( gStaged + "/bin" ) );
        QVERIFY( QFile::copy( FAKE_PROGRAM_EXE, gStaged + "/bin/" + exeName( "glabels-qt" ) ) );
        QVERIFY( QFile::copy( UPDATER_EXE, gStaged + "/bin/" + exeName( "glabels-updater" ) ) );
        writeFile( gStaged + "/bin/version.txt", "new" );

        PendingUpdate p;
        p.id       = "v1.1.0";
        p.version  = "v1.1.0";
        p.channel  = "stable";
        p.stagedAt = QDateTime::currentDateTime();
        QVERIFY( p.save() );
}


void TestUpdateApply::appliesAtExitSoTheNextLaunchIsNew()
{
        QProcess program;
        program.start( gRoot + "/bin/" + exeName( "glabels-qt" ), { "exit" } );
        QVERIFY( program.waitForFinished( 60000 ) );
        QCOMPARE( program.exitCode(), 0 );

        // the helper runs on its own once the program has gone
        QTRY_COMPARE_WITH_TIMEOUT( readFile( gRoot + "/bin/version.txt" ), QByteArray( "new" ), 30000 );

        QVERIFY( !QFileInfo::exists( gStaged ) );
        // the old version is deleted after the swap, which can take a moment on a slow disk
        QTRY_VERIFY_WITH_TIMEOUT( !QFileInfo::exists( QFileInfo( gRoot ).absolutePath() + "/.glabels-neo.old" ), 30000 );
        QVERIFY( QFileInfo::exists( gRoot + "/bin/" + exeName( "glabels-updater" ) ) );

        // "exit" only swaps: it does not start anything
        QTest::qWait( 500 );
        QVERIFY( !QFileInfo::exists( gRoot + "/bin/started.txt" ) );

        // and the record shows one attempt, so a swap that never works cannot loop forever
        QVERIFY( !PendingUpdate::load().has_value() || PendingUpdate::load()->attempts == 1 );
}


void TestUpdateApply::appliesAtStartupAndRestartsWithTheSameArguments()
{
        // e.g. the previous run was killed before it could apply the update; the user now opens a file
        QProcess program;
        program.start( gRoot + "/bin/" + exeName( "glabels-qt" ), { "startup", "label one.glabels", "two.glabels" } );
        QVERIFY( program.waitForFinished( 60000 ) );
        QCOMPARE( program.exitCode(), 0 );                     // "I handed over; exit now"

        QTRY_COMPARE_WITH_TIMEOUT( readFile( gRoot + "/bin/version.txt" ), QByteArray( "new" ), 30000 );

        // the new version was started, with the arguments the old one had been given
        QTRY_VERIFY_WITH_TIMEOUT( QFileInfo::exists( gRoot + "/bin/started.txt" ), 30000 );
        QCOMPARE( readFile( gRoot + "/bin/started.txt" ), QByteArray( "label one.glabels|two.glabels" ) );
        QVERIFY( !QFileInfo::exists( gStaged ) );
}


void TestUpdateApply::leavesTheInstallAloneWithoutAStagedUpdate()
{
        PendingUpdate::clear();
        QDir( gStaged ).removeRecursively();

        QProcess program;
        program.start( gRoot + "/bin/" + exeName( "glabels-qt" ), { "startup" } );
        QVERIFY( program.waitForFinished( 60000 ) );
        QCOMPARE( program.exitCode(), 3 );                     // nothing to do: carry on starting normally

        program.start( gRoot + "/bin/" + exeName( "glabels-qt" ), { "exit" } );
        QVERIFY( program.waitForFinished( 60000 ) );
        QTest::qWait( 1000 );
        QCOMPARE( readFile( gRoot + "/bin/version.txt" ), QByteArray( "old" ) );
}
