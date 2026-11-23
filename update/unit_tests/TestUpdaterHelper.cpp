//  TestUpdaterHelper.cpp
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


#include "TestUpdaterHelper.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <sys/stat.h>
#endif


QTEST_MAIN(TestUpdaterHelper)


namespace
{
        struct RunResult
        {
                int     exitCode{-1};
                QString status;     // "applied" / "failed" / "" (no result file)
                QString detail;
        };


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

        /// An install directory (or its staged counterpart) containing one marker file
        void makeTree( const QString& dir, const QByteArray& marker )
        {
                writeFile( dir + "/bin/version.txt", marker );
                writeFile( dir + "/lib/data.bin", marker + marker );
        }

        QString stagedOf( const QString& installed )
        {
                QFileInfo i( installed );
                return i.absolutePath() + "/." + i.fileName() + ".update";
        }

        QString backupOf( const QString& installed )
        {
                QFileInfo i( installed );
                return i.absolutePath() + "/." + i.fileName() + ".old";
        }

        QStringList unitArgs( const QStringList& installedDirs )
        {
                QStringList a;
                for ( const QString& d : installedDirs )
                {
                        a << "--unit" << QDir::toNativeSeparators( d ) << QDir::toNativeSeparators( stagedOf( d ) );
                }
                return a;
        }

        /// Run the helper to completion.  Fast retries so that failure cases do not take 20 s.
        RunResult runHelper( const QString& resultFile, const QStringList& args )
        {
                QProcess p;
                p.start( UPDATER_EXE, QStringList{ "--result", QDir::toNativeSeparators( resultFile ),
                                                   "--retries", "2", "--retry-ms", "50" } + args );
                RunResult r;
                if ( !p.waitForFinished( 60000 ) )
                {
                        p.kill();
                        return r;
                }
                r.exitCode = p.exitCode();

                QJsonObject o = QJsonDocument::fromJson( readFile( resultFile ) ).object();
                r.status = o.value( "status" ).toString();
                r.detail = o.value( "detail" ).toString();
                return r;
        }
}


void TestUpdaterHelper::swapsDirectories()
{
        QTemporaryDir tmp;
        const QString app = tmp.path() + "/glabels-neo";
        makeTree( app, "old" );
        makeTree( stagedOf( app ), "new" );

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { app } ) );

        QCOMPARE( r.exitCode, 0 );
        QCOMPARE( r.status, QString( "applied" ) );
        QCOMPARE( readFile( app + "/bin/version.txt" ), QByteArray( "new" ) );
        QCOMPARE( readFile( app + "/lib/data.bin" ), QByteArray( "newnew" ) );
        QVERIFY( !QFileInfo::exists( stagedOf( app ) ) );
        QVERIFY( !QFileInfo::exists( backupOf( app ) ) );      // previous version cleaned up
}


void TestUpdaterHelper::swapsSeveralUnitsTogether()
{
        QTemporaryDir tmp;
        const QString a = tmp.path() + "/glabels-qt.app";
        const QString b = tmp.path() + "/glabels-fill.app";
        makeTree( a, "old-a" );
        makeTree( b, "old-b" );
        makeTree( stagedOf( a ), "new-a" );
        makeTree( stagedOf( b ), "new-b" );

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { a, b } ) );

        QCOMPARE( r.exitCode, 0 );
        QCOMPARE( readFile( a + "/bin/version.txt" ), QByteArray( "new-a" ) );
        QCOMPARE( readFile( b + "/bin/version.txt" ), QByteArray( "new-b" ) );
}


void TestUpdaterHelper::nothingStagedIsNotAnError()
{
        // e.g. a second helper starting after the first one has finished the job
        QTemporaryDir tmp;
        const QString app = tmp.path() + "/glabels-neo";
        makeTree( app, "current" );

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { app } ) );

        QCOMPARE( r.exitCode, 0 );
        QCOMPARE( r.status, QString( "applied" ) );
        QCOMPARE( readFile( app + "/bin/version.txt" ), QByteArray( "current" ) );
}


void TestUpdaterHelper::rollsBackWhenALaterUnitFails()
{
#ifdef Q_OS_WIN
        QSKIP( "covered by rollsBackWhenInstalledCopyIsBusy on Windows" );
#else
        QTemporaryDir tmp;
        const QString a = tmp.path() + "/one/glabels-qt.app";
        const QString b = tmp.path() + "/two/glabels-fill.app";
        makeTree( a, "old-a" );
        makeTree( b, "old-b" );
        makeTree( stagedOf( a ), "new-a" );
        makeTree( stagedOf( b ), "new-b" );

        // the second unit cannot be renamed: its parent directory is read-only
        chmod( QFile::encodeName( tmp.path() + "/two" ).constData(), 0555 );

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { a, b } ) );

        chmod( QFile::encodeName( tmp.path() + "/two" ).constData(), 0755 );

        QCOMPARE( r.exitCode, 1 );
        QCOMPARE( r.status, QString( "failed" ) );
        QCOMPARE( readFile( a + "/bin/version.txt" ), QByteArray( "old-a" ) );    // first unit put back
        QCOMPARE( readFile( b + "/bin/version.txt" ), QByteArray( "old-b" ) );
        QCOMPARE( readFile( stagedOf( a ) + "/bin/version.txt" ), QByteArray( "new-a" ) );  // update stays available
#endif
}


void TestUpdaterHelper::rollsBackWhenInstalledCopyIsBusy()
{
#ifndef Q_OS_WIN
        QSKIP( "a directory can only be locked in place by an open file on Windows" );
#else
        QTemporaryDir tmp;
        const QString a = tmp.path() + "/glabels-qt.app";
        const QString b = tmp.path() + "/glabels-fill.app";
        makeTree( a, "old-a" );
        makeTree( b, "old-b" );
        makeTree( stagedOf( a ), "new-a" );
        makeTree( stagedOf( b ), "new-b" );

        // A running program (or a virus scanner) holds a file open without sharing it.
        const QString busyFile = QDir::toNativeSeparators( b + "/bin/version.txt" );
        HANDLE h = CreateFileW( reinterpret_cast<const wchar_t*>( busyFile.utf16() ), GENERIC_READ, 0, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
        QVERIFY( h != INVALID_HANDLE_VALUE );

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { a, b } ) );

        CloseHandle( h );

        QCOMPARE( r.exitCode, 1 );
        QCOMPARE( r.status, QString( "failed" ) );
        QCOMPARE( readFile( a + "/bin/version.txt" ), QByteArray( "old-a" ) );    // first unit was swapped, then put back
        QCOMPARE( readFile( b + "/bin/version.txt" ), QByteArray( "old-b" ) );
        QCOMPARE( readFile( stagedOf( a ) + "/bin/version.txt" ), QByteArray( "new-a" ) );
        QCOMPARE( readFile( stagedOf( b ) + "/bin/version.txt" ), QByteArray( "new-b" ) );
        QVERIFY( !QFileInfo::exists( backupOf( a ) ) );
        QVERIFY( !QFileInfo::exists( backupOf( b ) ) );

        // ... and once the file is released, the very same staged update goes through.
        RunResult again = runHelper( tmp.path() + "/result.json", unitArgs( { a, b } ) );
        QCOMPARE( again.exitCode, 0 );
        QCOMPARE( readFile( a + "/bin/version.txt" ), QByteArray( "new-a" ) );
        QCOMPARE( readFile( b + "/bin/version.txt" ), QByteArray( "new-b" ) );
#endif
}


void TestUpdaterHelper::waitsForTheApplicationToExit()
{
        QTemporaryDir tmp;
        const QString app = tmp.path() + "/glabels-neo";
        makeTree( app, "old" );
        makeTree( stagedOf( app ), "new" );

        // a stand-in "application" that runs for a couple of seconds
        QProcess application;
#ifdef Q_OS_WIN
        application.start( "cmd.exe", { "/c", "ping -n 4 127.0.0.1 >nul" } );
#else
        application.start( "/bin/sh", { "-c", "sleep 2" } );
#endif
        QVERIFY( application.waitForStarted() );

        QProcess helper;
        helper.start( UPDATER_EXE, QStringList{ "--wait-pid", QString::number( application.processId() ) }
                                   + unitArgs( { app } ) );
        QVERIFY( helper.waitForStarted() );

        // while the application runs, nothing may change
        QVERIFY( !helper.waitForFinished( 700 ) );
        QCOMPARE( readFile( app + "/bin/version.txt" ), QByteArray( "old" ) );

        // once it is gone, the swap happens
        application.waitForFinished( 20000 );
        QVERIFY( helper.waitForFinished( 20000 ) );
        QCOMPARE( helper.exitCode(), 0 );
        QCOMPARE( readFile( app + "/bin/version.txt" ), QByteArray( "new" ) );
}


void TestUpdaterHelper::restartsTheApplicationAfterwards()
{
        QTemporaryDir tmp;
        const QString app = tmp.path() + "/glabels-neo";
        makeTree( app, "old" );
        makeTree( stagedOf( app ), "new" );

        const QString marker = QDir::toNativeSeparators( tmp.path() + "/started.txt" );
#ifdef Q_OS_WIN
        QStringList relaunch{ "--relaunch", "cmd.exe", "/c", "echo", "x>" + marker };
#else
        QStringList relaunch{ "--relaunch", "/bin/sh", "-c", "echo x > " + marker };
#endif

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { app } ) + relaunch );

        QCOMPARE( r.exitCode, 0 );
        QCOMPARE( readFile( app + "/bin/version.txt" ), QByteArray( "new" ) );
        QTRY_VERIFY_WITH_TIMEOUT( QFileInfo::exists( marker ), 10000 );   // the program was started
}


void TestUpdaterHelper::restartsTheApplicationEvenWhenTheSwapFailed()
{
        // The user asked for the program: after a failed update it must still come back (the old version).
        QTemporaryDir tmp;
        const QString app = tmp.path() + "/glabels-neo";
        makeTree( app, "old" );
        makeTree( stagedOf( app ), "new" );
        QDir( app ).removeRecursively();    // installed copy vanished: the swap cannot work

        const QString marker = QDir::toNativeSeparators( tmp.path() + "/started.txt" );
#ifdef Q_OS_WIN
        QStringList relaunch{ "--relaunch", "cmd.exe", "/c", "echo", "x>" + marker };
#else
        QStringList relaunch{ "--relaunch", "/bin/sh", "-c", "echo x > " + marker };
#endif

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { app } ) + relaunch );

        QCOMPARE( r.exitCode, 1 );
        QCOMPARE( r.status, QString( "failed" ) );
        QTRY_VERIFY_WITH_TIMEOUT( QFileInfo::exists( marker ), 10000 );
}


void TestUpdaterHelper::handlesUnicodeAndSpacesInPaths()
{
        QTemporaryDir tmp;
        const QString app = tmp.path() + QString::fromUtf8( "/Programme \xC3\xBC\xC3\xB1\xC3\xAF c\xC3\xB6" "d\xC3\xA9/glabels neo" );
        makeTree( app, "old" );
        makeTree( stagedOf( app ), "new" );

        RunResult r = runHelper( tmp.path() + "/result.json", unitArgs( { app } ) );

        QCOMPARE( r.exitCode, 0 );
        QCOMPARE( readFile( app + "/bin/version.txt" ), QByteArray( "new" ) );
}


void TestUpdaterHelper::rejectsBadCommandLine()
{
        QCOMPARE( QProcess::execute( UPDATER_EXE, {} ), 2 );                                   // no --unit
        QCOMPARE( QProcess::execute( UPDATER_EXE, { "--bogus" } ), 2 );
        QCOMPARE( QProcess::execute( UPDATER_EXE, { "--unit", "only-one-path" } ), 2 );
        QCOMPARE( QProcess::execute( UPDATER_EXE, { "--unit", "a", "b", "--relaunch" } ), 2 ); // nothing to launch
}
