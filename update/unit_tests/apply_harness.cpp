//  apply_harness.cpp
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
// The "running program" of TestUpdateApply: it is installed (as bin/glabels-qt) in a fake install
// directory, and does what the real program does around an update -- at start-up and at exit --
// so that the test can watch the helper swap the install out from under a real, running process.
//
//   apply_harness exit               call applyPendingAtExit() and quit
//   apply_harness startup [args...]  call applyPendingAtStartup(args); quit if told to
//

#include "update/UpdateService.hpp"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QString>

using namespace glabels::update;

int main( int argc, char** argv )
{
        QCoreApplication app( argc, argv );

        // same settings and data locations as the test that starts us
        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-test" );
        QCoreApplication::setApplicationName( "update-test" );

        const QStringList args = app.arguments().mid( 1 );
        if ( args.isEmpty() )
        {
                return 64;
        }

        UpdateServiceConfig c;
        c.build       = BuildIdentity{ "stable", "v1.0.0", QString() };
        c.platformKey = platformKey();
        c.repo        = "owner/project";
        c.manifestUrl = QUrl( "http://127.0.0.1:9/manifest.json" );
        c.strictUrls  = false;
        c.layout      = InstallLayout::detect( QCoreApplication::applicationFilePath(), currentOs(), true );

        UpdateService service( c );
        if ( !service.isSupported() )
        {
                return 65;
        }

        if ( args[0] == "exit" )
        {
                service.applyPendingAtExit();
                return 0;
        }

        // "startup": 0 = handed over to the helper (we must exit), 3 = nothing to do
        return service.applyPendingAtStartup( args.mid( 1 ) ) ? 0 : 3;
}
