//  InstallLayout.cpp
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

#include "InstallLayout.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSysInfo>


namespace glabels::update
{

        namespace
        {

                QString tr( const char* text )
                {
                        return QCoreApplication::translate( "InstallLayout", text );
                }


                /// Can a directory be created, renamed and removed in `parentPath`?  That is exactly
                /// what swapping an install in that location needs (more reliable than asking for
                /// permission bits, which on Windows ignore ACLs).
                bool canReplaceIn( const QString& parentPath )
                {
                        QDir parent( parentPath );
                        const QString a = QString( ".glabels-update-probe-%1" ).arg( QCoreApplication::applicationPid() );
                        const QString b = a + "-renamed";

                        parent.rmdir( a );
                        parent.rmdir( b );
                        if ( !parent.mkdir( a ) )
                        {
                                return false;
                        }
                        bool ok = parent.rename( a, b );
                        parent.rmdir( ok ? b : a );
                        return ok;
                }


                InstallLayout::Unit makeUnit( const QString& installedPath )
                {
                        QFileInfo info( installedPath );
                        QString parent = info.absoluteDir().absolutePath();
                        QString name   = info.fileName();

                        InstallLayout::Unit u;
                        u.installed = info.absoluteFilePath();
                        u.staged    = parent + "/." + name + ".update";
                        u.backup    = parent + "/." + name + ".old";
                        return u;
                }

        }


        Os currentOs()
        {
#if defined(Q_OS_WIN)
                return Os::Windows;
#elif defined(Q_OS_MACOS)
                return Os::MacOs;
#elif defined(Q_OS_LINUX)
                return Os::Linux;
#else
                return Os::Other;
#endif
        }


        QString platformKey()
        {
                const QString arch = QSysInfo::buildCpuArchitecture();

                switch ( currentOs() )
                {
                case Os::Windows:
                        return arch == "x86_64" ? "windows-x86_64" : QString();
                case Os::Linux:
                        return arch == "x86_64" ? "linux-x86_64" : QString();
                case Os::MacOs:
                        return arch == "arm64" ? "macos-arm64" : QString();
                default:
                        return QString();
                }
        }


        InstallLayout InstallLayout::detect( const QString& executablePath, Os os, bool probeWritable )
        {
                InstallLayout layout;
                layout.os = os;

                auto unsupported = [&]( const QString& why )
                {
                        layout.supported = false;
                        layout.reason    = why;
                        layout.units.clear();
                        return layout;
                };

                QFileInfo exe( executablePath );
                if ( os == Os::Other )
                {
                        return unsupported( tr( QT_TRANSLATE_NOOP( "InstallLayout", "Automatic updates are not available on this operating system." ) ) );
                }

                if ( os == Os::MacOs )
                {
                        // <dir>/glabels-qt.app/Contents/MacOS/glabels-qt
                        QDir macosDir = exe.absoluteDir();
                        QDir contents = macosDir;
                        QDir bundle   = macosDir;
                        if ( macosDir.dirName() != "MacOS" || !contents.cdUp() || contents.dirName() != "Contents" ||
                             !bundle.cdUp() || !bundle.cdUp() || !bundle.dirName().endsWith( ".app" ) )
                        {
                                return unsupported( tr( QT_TRANSLATE_NOOP( "InstallLayout", "This copy of the program is not an installed application bundle." ) ) );
                        }

                        layout.helperRel = "Contents/MacOS/glabels-updater";
                        layout.mainRel   = "Contents/MacOS/" + exe.fileName();
                        layout.smokeRel  = layout.mainRel;

                        layout.units.append( makeUnit( bundle.absolutePath() ) );
                        QDir parent = bundle;
                        parent.cdUp();
                        QString fill = parent.absoluteFilePath( "glabels-fill.app" );
                        if ( bundle.dirName() != "glabels-fill.app" && QFileInfo( fill ).isDir() )
                        {
                                layout.units.append( makeUnit( fill ) );
                        }

                        layout.helperPath   = bundle.absoluteFilePath( layout.helperRel );
                        layout.relaunchPath = bundle.absolutePath();
                }
                else
                {
                        // <root>/bin/glabels-qt[.exe]
                        QDir binDir = exe.absoluteDir();
                        QDir root   = binDir;
                        if ( binDir.dirName().compare( "bin", Qt::CaseInsensitive ) != 0 || !root.cdUp() || root.isRoot() )
                        {
                                return unsupported( tr( QT_TRANSLATE_NOOP( "InstallLayout", "This copy of the program is not an installed release package." ) ) );
                        }

                        const QString exeSuffix = ( os == Os::Windows ) ? ".exe" : "";
                        layout.helperRel = "bin/glabels-updater" + exeSuffix;
                        layout.mainRel   = "bin/" + exe.fileName();
                        layout.smokeRel  = ( os == Os::Windows ) ? "bin/glabelsneo.exe" : "glabelsneo";

                        layout.units.append( makeUnit( root.absolutePath() ) );
                        layout.helperPath = root.absoluteFilePath( layout.helperRel );

                        QString launcher = root.absoluteFilePath( "glabels-neo" );
                        layout.relaunchPath = ( os == Os::Linux && QFileInfo( launcher ).isFile() ) ? launcher : exe.absoluteFilePath();
                }

                if ( !QFileInfo( layout.helperPath ).isFile() )
                {
                        return unsupported( tr( QT_TRANSLATE_NOOP( "InstallLayout", "This build does not include the update helper." ) ) );
                }

                if ( probeWritable )
                {
                        const QString problem = layout.writableProblem();
                        if ( !problem.isEmpty() )
                        {
                                return unsupported( problem );
                        }
                }

                layout.supported = true;
                return layout;
        }


        QString InstallLayout::writableProblem() const
        {
                for ( const auto& u : units )
                {
                        if ( !canReplaceIn( QFileInfo( u.installed ).absoluteDir().absolutePath() ) )
                        {
                                // TODO(installer): per-machine installs land here; see "Planned: a Windows installer" in docs/AUTO-UPDATE.md
                                return tr( QT_TRANSLATE_NOOP( "InstallLayout", "The program is installed in a location that cannot be modified without administrator rights. Move it to a folder you own to enable automatic updates." ) );
                        }
                }
                return QString();
        }


        QString InstallLayout::validateStaged() const
        {
                if ( units.isEmpty() )
                {
                        return tr( QT_TRANSLATE_NOOP( "InstallLayout", "nothing to install" ) );
                }

                for ( const auto& u : units )
                {
                        if ( !QFileInfo( u.staged ).isDir() )
                        {
                                return tr( QT_TRANSLATE_NOOP( "InstallLayout", "the unpacked update is missing" ) );
                        }
                }

                // The main program, and the helper that makes the *next* update possible.
                for ( const QString& rel : { mainRel, helperRel, smokeRel } )
                {
                        if ( !QFileInfo( units[0].staged + "/" + rel ).isFile() )
                        {
                                return tr( QT_TRANSLATE_NOOP( "InstallLayout", "the update package is incomplete (%1 is missing)" ) ).arg( rel );
                        }
                }

                // Nothing may point outside the unpacked tree.
                for ( const auto& u : units )
                {
                        const QString root = QFileInfo( u.staged ).canonicalFilePath() + "/";

                        QDirIterator it( u.staged, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                         QDirIterator::Subdirectories );
                        while ( it.hasNext() )
                        {
                                QFileInfo info( it.next() );
                                if ( info.isSymLink() )
                                {
                                        QString target = info.symLinkTarget();
                                        if ( QFileInfo( target ).isRelative() )
                                        {
                                                target = info.absolutePath() + "/" + target;
                                        }
                                        target = QDir::cleanPath( target );
                                        if ( !( target + "/" ).startsWith( root ) &&
                                             !( QFileInfo( target ).canonicalFilePath() + "/" ).startsWith( root ) )
                                        {
                                                return tr( QT_TRANSLATE_NOOP( "InstallLayout", "the update package contains a link that leaves its folder (%1)" ) ).arg( info.fileName() );
                                        }
                                }
                        }
                }

                return QString();
        }

}
