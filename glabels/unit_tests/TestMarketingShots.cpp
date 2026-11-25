//  TestMarketingShots.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels Neo.
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


#include "TestMarketingShots.hpp"

#include "AppearanceModel.hpp"
#include "FillView.hpp"
#include "MainWindow.hpp"
#include "SelectProductDialog.hpp"
#include "TemplateDesigner.hpp"
#include "TemplatePicker.hpp"

#include "ribbon/RibbonBar.hpp"
#include "ribbon/RibbonTypes.hpp"
#include "theme/Theme.hpp"
#include "theme/ThemeScheme.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Model.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"
#include "model/XmlLabelParser.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QLineEdit>
#include <QTabWidget>
#include <QSettings>
#include <QStandardPaths>

#include <memory>


QTEST_MAIN(TestMarketingShots)


using namespace glabels;


namespace
{
        const QString SAMPLES = QStringLiteral( GLABELS_SOURCE_DIR "/docs/assets/samples/" );


        void settle()
        {
                for ( int i = 0; i < 8; i++ )
                {
                        QCoreApplication::processEvents();
                }
        }


        void setMode( bool dark )
        {
                model::Settings::setColorScheme( dark ? model::Settings::DARK_COLOR_SCHEME
                                                      : model::Settings::LIGHT_COLOR_SCHEME );
                settle();
        }


        QString outDir()
        {
                return qEnvironmentVariable( "GLABELS_MARKETING_SHOTS" );
        }


        bool save( QWidget* widget, const QString& name )
        {
                settle();
                QImage image = widget->grab().toImage();
                return !image.isNull() && image.save( outDir() + "/" + name + ".png" );
        }


        model::ModelObject* textObjectContaining( model::Model* model, const QString& needle )
        {
                for ( auto* object : model->objectList() )
                {
                        if ( auto* text = qobject_cast<model::ModelTextObject*>( object ) )
                        {
                                if ( text->text().contains( needle ) )
                                {
                                        return text;
                                }
                        }
                }
                return nullptr;
        }


        ///
        /// A main window showing a sample project
        ///
        std::unique_ptr<MainWindow> openWindow( const QString& project, model::Model** modelOut )
        {
                auto* model = model::XmlLabelParser::readFile( SAMPLES + project );
                if ( !model )
                {
                        return nullptr;
                }
                std::unique_ptr<MainWindow> window( new MainWindow() );
                window->setModel( model );
                window->resize( 1440, 900 );
                window->show();
                if ( !QTest::qWaitForWindowExposed( window.get() ) )
                {
                        return nullptr;
                }
                *modelOut = model;
                return window;
        }
}


void TestMarketingShots::initTestCase()
{
        if ( outDir().isEmpty() )
        {
                QSKIP( "GLABELS_MARKETING_SHOTS is not set" );
        }

        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-neo-tests" );
        QCoreApplication::setApplicationName( "TestMarketingShots" );
        QSettings().clear();

        model::Settings::init();
        model::Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();
        QApplication::clipboard()->setText( QStringLiteral( "shots" ) );

        QDir().mkpath( outDir() );
}


void TestMarketingShots::screens()
{
        using ribbon::TabId;

        for ( bool dark : { false, true } )
        {
                setMode( dark );
                const QString mode = dark ? "dark" : "light";

                // Welcome page
                {
                        std::unique_ptr<MainWindow> welcome( new MainWindow() );
                        welcome->resize( 1440, 900 );
                        welcome->show();
                        QVERIFY( QTest::qWaitForWindowExposed( welcome.get() ) );
                        QVERIFY( save( welcome.get(), "welcome-" + mode ) );
                }

                // The editor with a shipping label, the name selected so the Text tab is showing
                {
                        model::Model* model = nullptr;
                        auto window = openWindow( "shipping-label.glabels", &model );
                        QVERIFY( window );
                        model->unselectAll();
                        model->selectObject( textObjectContaining( model, "Maya" ) );
                        settle();
                        window->ribbonBar()->setCurrentTab( TabId::ContextText );
                        QVERIFY( save( window.get(), "editor-text-" + mode ) );

                        model->unselectAll();
                        window->ribbonBar()->setCurrentTab( TabId::Home );
                        QVERIFY( save( window.get(), "editor-home-" + mode ) );
                }

                // A round product label
                {
                        model::Model* model = nullptr;
                        auto window = openWindow( "product-label.glabels", &model );
                        QVERIFY( window );
                        model->unselectAll();
                        settle();
                        QVERIFY( save( window.get(), "editor-round-" + mode ) );
                }

                // The Fill page with a week of orders
                {
                        model::Model* model = nullptr;
                        auto window = openWindow( "shipping-label-merge.glabels", &model );
                        QVERIFY( window );
                        auto* fill = window->findChild<FillView*>();
                        QVERIFY( fill );
                        fill->setKioskSource( SAMPLES + "shipping-orders.csv", {} );
                        fill->setModel( model );
                        QVERIFY( QMetaObject::invokeMethod( window.get(), "fileShowFillPage" ) );
                        QVERIFY( save( window.get(), "fill-" + mode ) );
                }

                // Product browser
                {
                        SelectProductDialog dialog;
                        dialog.resize( 1080, 660 );
                        dialog.show();
                        QVERIFY( QTest::qWaitForWindowExposed( &dialog ) );
                        if ( auto* tabs = dialog.findChild<QTabWidget*>() )
                        {
                                tabs->setCurrentIndex( 0 );
                        }
                        if ( auto* search = dialog.findChild<QLineEdit*>( "searchEntry" ) )
                        {
                                search->setText( "Avery 5" );
                        }
                        settle();
                        if ( auto* picker = dialog.findChild<TemplatePicker*>() )
                        {
                                picker->selectionModel()->select( picker->model()->index( 0, 0 ), QItemSelectionModel::ClearAndSelect );
                        }
                        QVERIFY( save( &dialog, "templates-" + mode ) );
                }

                // Template designer
                {
                        TemplateDesigner designer;
                        designer.resize( 920, 620 );
                        designer.show();
                        QVERIFY( QTest::qWaitForWindowExposed( &designer ) );
                        QVERIFY( save( &designer, "designer-" + mode ) );
                }
        }
        setMode( false );
}


void TestMarketingShots::schemes()
{
        using ribbon::TabId;

        for ( const auto& scheme : theme::ThemeSchemes::all() )
        {
                model::Settings::setThemeScheme( scheme.id );
                for ( bool dark : { false, true } )
                {
                        setMode( dark );
                        model::Model* model = nullptr;
                        auto window = openWindow( "shipping-label.glabels", &model );
                        QVERIFY( window );
                        model->unselectAll();
                        model->selectObject( textObjectContaining( model, "Maya" ) );
                        settle();
                        window->ribbonBar()->setCurrentTab( TabId::ContextText );
                        QVERIFY( save( window.get(), QString( "theme-%1-%2" ).arg( scheme.id, dark ? "dark" : "light" ) ) );
                }
        }

        model::Settings::setThemeScheme( "graphite" );
        setMode( false );
}
