//  main.cpp
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


//  main.cpp -- glabels-fill kiosk entry point.
//

#include "AppearanceModel.hpp"
#include "FillConfig.hpp"
#include "FillWindow.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Settings.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>


int main( int argc, char** argv )
{
        QApplication app( argc, argv );
        QCoreApplication::setApplicationName( "glabels-fill" );

        QIcon::setThemeName( "glabels-flat" );

        glabels::model::Settings::init();
        glabels::merge::Factory::init();
        glabels::barcode::Backends::init();

        // gLabels Neo look (Fusion base, theme palette and stylesheet), as in the designer.
        glabels::AppearanceModel::init();

        glabels::FillConfig cfg;
        glabels::FillConfigIO::readFromExe( QCoreApplication::applicationFilePath(), cfg );

        glabels::FillWindow w( cfg );
        w.resize( 1100, 820 );
        w.show();

        return app.exec();
}
