//  TestInstallLayout.cpp
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


#include "TestInstallLayout.hpp"

#include "update/InstallLayout.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>


QTEST_MAIN(TestInstallLayout)

using namespace glabels::update;


namespace
{
        void touch( const QString& path )
        {
                QDir().mkpath( QFileInfo( path ).absolutePath() );
                QFile f( path );
                QVERIFY( f.open( QIODevice::WriteOnly ) );
                f.write( "x" );
        }
}


void TestInstallLayout::windowsPackage()
{
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/Apps/glabels-neo";
        touch( root + "/bin/glabels-qt.exe" );
        touch( root + "/bin/glabels-updater.exe" );

        auto l = InstallLayout::detect( root + "/bin/glabels-qt.exe", Os::Windows );
        QVERIFY2( l.supported, qPrintable( l.reason ) );
        QCOMPARE( l.units.size(), 1 );
        QCOMPARE( l.units[0].installed, root );
        QCOMPARE( l.units[0].staged, tmp.path() + "/Apps/.glabels-neo.update" );
        QCOMPARE( l.units[0].backup, tmp.path() + "/Apps/.glabels-neo.old" );
        QCOMPARE( l.helperPath, root + "/bin/glabels-updater.exe" );
        QCOMPARE( l.relaunchPath, root + "/bin/glabels-qt.exe" );
        QCOMPARE( l.mainRel, QString( "bin/glabels-qt.exe" ) );
        QCOMPARE( l.smokeRel, QString( "bin/glabels-batch-qt.exe" ) );
}


void TestInstallLayout::linuxPackage()
{
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/opt/glabels-neo";
        touch( root + "/bin/glabels-qt" );
        touch( root + "/bin/glabels-updater" );

        auto l = InstallLayout::detect( root + "/bin/glabels-qt", Os::Linux );
        QVERIFY2( l.supported, qPrintable( l.reason ) );
        QCOMPARE( l.units[0].installed, root );
        QCOMPARE( l.relaunchPath, root + "/bin/glabels-qt" );      // no launcher script: the binary itself
        QCOMPARE( l.smokeRel, QString( "glabels-batch-qt" ) );
}


void TestInstallLayout::linuxPackageUsesLauncherScript()
{
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/glabels-neo";
        touch( root + "/bin/glabels-qt" );
        touch( root + "/bin/glabels-updater" );
        touch( root + "/glabels-neo" );     // the launcher sets LD_LIBRARY_PATH; it must be what restarts the program

        auto l = InstallLayout::detect( root + "/bin/glabels-qt", Os::Linux );
        QVERIFY( l.supported );
        QCOMPARE( l.relaunchPath, root + "/glabels-neo" );
}


void TestInstallLayout::macBundles()
{
        QTemporaryDir tmp;
        const QString apps = tmp.path() + "/Applications";
        touch( apps + "/glabels-qt.app/Contents/MacOS/glabels-qt" );
        touch( apps + "/glabels-qt.app/Contents/MacOS/glabels-updater" );
        touch( apps + "/glabels-fill.app/Contents/MacOS/glabels-fill" );

        auto l = InstallLayout::detect( apps + "/glabels-qt.app/Contents/MacOS/glabels-qt", Os::MacOs );
        QVERIFY2( l.supported, qPrintable( l.reason ) );
        QCOMPARE( l.units.size(), 2 );                                  // both apps of the dmg move together
        QCOMPARE( l.units[0].installed, apps + "/glabels-qt.app" );
        QCOMPARE( l.units[0].staged, apps + "/.glabels-qt.app.update" );
        QCOMPARE( l.units[1].installed, apps + "/glabels-fill.app" );
        QCOMPARE( l.units[1].staged, apps + "/.glabels-fill.app.update" );
        QCOMPARE( l.relaunchPath, apps + "/glabels-qt.app" );
        QCOMPARE( l.helperPath, apps + "/glabels-qt.app/Contents/MacOS/glabels-updater" );
        QCOMPARE( l.mainRel, QString( "Contents/MacOS/glabels-qt" ) );
}


void TestInstallLayout::macBundleWithoutFillApp()
{
        QTemporaryDir tmp;
        touch( tmp.path() + "/glabels-qt.app/Contents/MacOS/glabels-qt" );
        touch( tmp.path() + "/glabels-qt.app/Contents/MacOS/glabels-updater" );

        auto l = InstallLayout::detect( tmp.path() + "/glabels-qt.app/Contents/MacOS/glabels-qt", Os::MacOs );
        QVERIFY( l.supported );
        QCOMPARE( l.units.size(), 1 );
}


void TestInstallLayout::refusesWhenNotAReleasePackage()
{
        QTemporaryDir tmp;

        // a build tree: <build>/glabels/glabels-qt
        touch( tmp.path() + "/build/glabels/glabels-qt.exe" );
        auto l = InstallLayout::detect( tmp.path() + "/build/glabels/glabels-qt.exe", Os::Windows );
        QVERIFY( !l.supported );
        QVERIFY( !l.reason.isEmpty() );
        QVERIFY( l.units.isEmpty() );

        // a macOS binary that is not inside a bundle
        touch( tmp.path() + "/bin/glabels-qt" );
        QVERIFY( !InstallLayout::detect( tmp.path() + "/bin/glabels-qt", Os::MacOs ).supported );
}


void TestInstallLayout::refusesWhenHelperIsMissing()
{
        QTemporaryDir tmp;
        touch( tmp.path() + "/glabels-neo/bin/glabels-qt.exe" );     // e.g. a package from before the updater existed

        auto l = InstallLayout::detect( tmp.path() + "/glabels-neo/bin/glabels-qt.exe", Os::Windows );
        QVERIFY( !l.supported );
        QVERIFY( l.reason.contains( "helper" ) );
}


void TestInstallLayout::refusesUnsupportedOs()
{
        QVERIFY( !InstallLayout::detect( "/x/bin/glabels-qt", Os::Other ).supported );
}


void TestInstallLayout::platformKeyIsKnownForThisBuild()
{
        const QString key = platformKey();
        QVERIFY( key.isEmpty() || key == "windows-x86_64" || key == "linux-x86_64" || key == "macos-arm64" );
#if defined(Q_OS_WIN)
        QCOMPARE( currentOs(), Os::Windows );
#elif defined(Q_OS_MACOS)
        QCOMPARE( currentOs(), Os::MacOs );
#elif defined(Q_OS_LINUX)
        QCOMPARE( currentOs(), Os::Linux );
#endif
}


void TestInstallLayout::validateStaged()
{
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/glabels-neo";
        touch( root + "/bin/glabels-qt.exe" );
        touch( root + "/bin/glabels-updater.exe" );

        auto l = InstallLayout::detect( root + "/bin/glabels-qt.exe", Os::Windows );
        QVERIFY( l.supported );

        // nothing staged yet
        QVERIFY( !l.validateStaged().isEmpty() );

        // incomplete: the smoke-test program is missing
        touch( l.units[0].staged + "/bin/glabels-qt.exe" );
        touch( l.units[0].staged + "/bin/glabels-updater.exe" );
        QVERIFY( l.validateStaged().contains( "glabels-batch-qt.exe" ) );

        touch( l.units[0].staged + "/bin/glabels-batch-qt.exe" );
        QCOMPARE( l.validateStaged(), QString() );
}


void TestInstallLayout::validateStagedRejectsEscapingLinks()
{
#ifdef Q_OS_WIN
        QSKIP( "creating symbolic links needs a privilege on Windows" );
#else
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/glabels-neo";
        touch( root + "/bin/glabels-qt" );
        touch( root + "/bin/glabels-updater" );

        auto l = InstallLayout::detect( root + "/bin/glabels-qt", Os::Linux );
        QVERIFY( l.supported );

        touch( l.units[0].staged + "/bin/glabels-qt" );
        touch( l.units[0].staged + "/bin/glabels-updater" );
        touch( l.units[0].staged + "/glabels-batch-qt" );
        touch( l.units[0].staged + "/lib/libreal.so.1" );

        // a relative link inside the tree is fine (this is what shared libraries look like) ...
        QVERIFY( QFile::link( "libreal.so.1", l.units[0].staged + "/lib/libreal.so" ) );
        QCOMPARE( l.validateStaged(), QString() );

        // ... a link out of it is not
        QVERIFY( QFile::link( "/etc/passwd", l.units[0].staged + "/lib/evil" ) );
        QVERIFY( l.validateStaged().contains( "link" ) );
#endif
}
