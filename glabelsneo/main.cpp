//  main.cpp
//
//  Copyright (C) 2013-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
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
//  glabelsneo: the command line front end.  It grew out of glabels-batch-qt (still
//  shipped, see legacy_main.cpp) and keeps its subsystem start-up and translators.
//


#include "Cli.hpp"
#include "Terminal.hpp"

#include "model/Db.hpp"
#include "model/FileUtil.hpp"
#include "model/Settings.hpp"
#include "model/Version.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"

#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>


int main( int argc, char **argv )
{
#if defined(Q_OS_LINUX)
        // Qt needs a platform plugin for fonts.  With no display (a server, a container, CI)
        // use the offscreen one instead of failing; no xvfb required.
        if ( qEnvironmentVariableIsEmpty( "QT_QPA_PLATFORM" ) &&
             qEnvironmentVariableIsEmpty( "DISPLAY" ) &&
             qEnvironmentVariableIsEmpty( "WAYLAND_DISPLAY" ) )
        {
                qputenv( "QT_QPA_PLATFORM", "offscreen" );
        }
#endif

        QGuiApplication app( argc, argv );

        // The same identity as the GUI, so both share preferences, user templates and
        // printer profiles (for example the Zebra connection settings).
        QCoreApplication::setOrganizationName( "glabels.org" );
        QCoreApplication::setOrganizationDomain( "glabels.org" );
        QCoreApplication::setApplicationName( glabels::model::Version::APPLICATION_NAME );
        QCoreApplication::setApplicationVersion( glabels::model::Version::LONG_STRING );

        glabels::cli::Terminal::init();

        const QStringList arguments = app.arguments().mid( 1 );

        // Library warnings are buffered rather than spilled onto the terminal (they are shown
        // with --verbose, or folded into an error message when they explain one).
        glabels::cli::Terminal::installLogHandler( arguments.contains( "-v" ) || arguments.contains( "--verbose" ) );

        //
        // Setup translators
        //
        QLocale locale = QLocale::system();
        QString qtTranslationsDir = QLibraryInfo::path( QLibraryInfo::TranslationsPath );
        QString myTranslationsDir = glabels::model::FileUtil::translationsDir().canonicalPath();

        QTranslator qtTranslator;
        if ( qtTranslator.load( locale, "qt", "_", qtTranslationsDir ) )
        {
                app.installTranslator( &qtTranslator );
        }

        QTranslator glabelsTranslator;
        if ( glabelsTranslator.load( locale, "glabels", "_", myTranslationsDir ) )
        {
                app.installTranslator( &glabelsTranslator );
        }

        QTranslator templatesTranslator;
        if ( templatesTranslator.load( locale, "templates", "_", myTranslationsDir ) )
        {
                app.installTranslator( &templatesTranslator );
        }

        //
        // Initialize subsystems
        //
        glabels::model::Settings::init();
        glabels::model::Db::init();
        glabels::merge::Factory::init();
        glabels::barcode::Backends::init();

        return glabels::cli::runCli( arguments );
}
