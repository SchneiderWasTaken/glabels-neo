//  main.cpp -- glabels-fill kiosk entry point.
//

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

        glabels::FillConfig cfg;
        glabels::FillConfigIO::readFromExe( QCoreApplication::applicationFilePath(), cfg );

        glabels::FillWindow w( cfg );
        w.resize( 1100, 820 );
        w.show();

        return app.exec();
}
