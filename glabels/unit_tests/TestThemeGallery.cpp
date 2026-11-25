//  TestThemeGallery.cpp
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


#include "TestThemeGallery.hpp"

#include "AboutDialog.hpp"
#include "AppearanceModel.hpp"
#include "ColorPaletteDialog.hpp"
#include "EditVariableDialog.hpp"
#include "FillConfig.hpp"
#include "MainWindow.hpp"
#include "PreferencesDialog.hpp"
#include "ReportBugDialog.hpp"
#include "SelectProductDialog.hpp"
#include "TemplateDesigner.hpp"

#include "AdminDialog.hpp"
#include "FillWindow.hpp"

#include "theme/Theme.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Settings.hpp"
#include "model/XmlLabelParser.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>

#include <functional>
#include <memory>


QTEST_MAIN(TestThemeGallery)


using namespace glabels;


namespace
{

        using Factory = std::function<QWidget*()>;


        QStringList& styleWarnings()
        {
                static QStringList list;
                return list;
        }


        void collectWarnings( QtMsgType type, const QMessageLogContext&, const QString& msg )
        {
                if ( type != QtDebugMsg && type != QtInfoMsg &&
                     ( msg.contains( "stylesheet", Qt::CaseInsensitive ) || msg.contains( "ThemeStyle" ) ) )
                {
                        styleWarnings().append( msg );
                }
        }


        QMessageBox* saveChangesBox()
        {
                // Same construction as MainWindow::closeEvent
                QString msg  = QObject::tr( "Save changes to project \"%1\" before closing?" ).arg( "Harbor & Pine product label" );
                QString info = QObject::tr( "Your changes will be lost if you don't save them." );
                auto* box = new QMessageBox( QMessageBox::Warning, QObject::tr( "Save project?" ),
                                             "<b>" + msg + "</b><p>" + info + "</p>",
                                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel );
                box->setDefaultButton( QMessageBox::Save );
                return box;
        }


        QMessageBox* consentBox()
        {
                // Same construction as UpdateUi::askForConsent
                auto* box = new QMessageBox( QMessageBox::Question, QObject::tr( "Check for Updates Automatically?" ),
                                             QObject::tr( "Should %1 look for new versions in the background?" ).arg( "gLabels Neo" ),
                                             QMessageBox::NoButton );
                box->setInformativeText( QObject::tr( "If you agree, the program asks github.com for a small file (about 1 KB) "
                                                      "every 12 hours or so." ) );
                box->addButton( QObject::tr( "Check Automatically" ), QMessageBox::AcceptRole );
                QPushButton* no = box->addButton( QObject::tr( "Don't Check" ), QMessageBox::RejectRole );
                box->setDefaultButton( no );
                return box;
        }


        QMessageBox* offerBox()
        {
                // Same construction as UpdateUi::showOffer
                auto* box = new QMessageBox( QMessageBox::Information, QObject::tr( "Update Available" ),
                                             QObject::tr( "%1 %2 is available." ).arg( "gLabels Neo", "0.9.1" ),
                                             QMessageBox::NoButton );
                box->setTextFormat( Qt::RichText );
                box->setInformativeText( QObject::tr( "You are using 0.9.0. The update downloads quietly in the background." ) +
                                         "<br><br><a href=\"https://example.invalid\">" + QObject::tr( "What's new" ) + "</a>" );
                QPushButton* install = box->addButton( QObject::tr( "Install in Background" ), QMessageBox::AcceptRole );
                box->addButton( QObject::tr( "Not Now" ), QMessageBox::RejectRole );
                box->addButton( QObject::tr( "Skip This Version" ), QMessageBox::ActionRole );
                box->setDefaultButton( install );
                box->setCheckBox( new QCheckBox( QObject::tr( "Install future updates automatically" ), box ) );
                return box;
        }


        FillConfig kioskConfig()
        {
                FillConfig cfg;
                cfg.appName = QStringLiteral( "Harbor & Pine Labeler" );
                cfg.labelFiles << QStringLiteral( GLABELS_SOURCE_DIR "/test-data/simple-shapes.glabels" );
                return cfg;
        }


        MainWindow* mainWindow( bool withProject )
        {
                auto* window = new MainWindow();
                if ( withProject )
                {
                        window->setModel( model::XmlLabelParser::readFile(
                                QStringLiteral( GLABELS_SOURCE_DIR "/test-data/simple-shapes.glabels" ) ) );
                }
                return window;
        }


        void setScheme( model::Settings::ColorScheme scheme )
        {
                // Through the stored setting, exactly as the Preferences dialog does
                model::Settings::setColorScheme( scheme );
                QCoreApplication::processEvents();
        }
}


void TestThemeGallery::initTestCase()
{
        // A private settings scope: never touch the user's real preferences.
        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-neo-tests" );
        QCoreApplication::setApplicationName( "TestThemeGallery" );

        model::Settings::init();
        model::Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();

        qInstallMessageHandler( collectWarnings );

        // model::Model::canPaste() expects clipboard data; the offscreen platform starts empty.
        QApplication::clipboard()->setText( QStringLiteral( "gallery" ) );
}


void TestThemeGallery::dialogs_data()
{
        QTest::addColumn<QString>( "name" );
        QTest::addColumn<QSize>( "size" );

        QTest::newRow( "d01" ) << "d01-select-product"    << QSize( 1080, 660 );
        QTest::newRow( "d02" ) << "d02-preferences"       << QSize();
        QTest::newRow( "d03" ) << "d03-template-designer" << QSize( 920, 620 );
        QTest::newRow( "d05" ) << "d05-edit-variable"     << QSize();
        QTest::newRow( "d06" ) << "d06-about"             << QSize();
        QTest::newRow( "d07" ) << "d07-report-problem"    << QSize( 680, 700 );
        QTest::newRow( "d08a" ) << "d08-update-consent"   << QSize();
        QTest::newRow( "d08b" ) << "d08-update-available" << QSize();
        QTest::newRow( "d09" ) << "d09-save-changes"      << QSize();
        QTest::newRow( "d10" ) << "d10-color-palette"     << QSize();
        QTest::newRow( "d11a" ) << "d11-kiosk"            << QSize( 900, 640 );
        QTest::newRow( "d11b" ) << "d11-kiosk-admin-pin"  << QSize();

        // Not a dialog: the main window as it is today, to catch stylesheet regressions there
        QTest::newRow( "main-welcome" ) << "main-window-welcome" << QSize( 1440, 900 );
        QTest::newRow( "main-editor" )  << "main-window-editor"  << QSize( 1440, 900 );
}


void TestThemeGallery::dialogs()
{
        QFETCH( QString, name );
        QFETCH( QSize, size );

        const QHash<QString, Factory> factories {
                { "d01-select-product",    [] { return new SelectProductDialog(); } },
                { "d02-preferences",       [] { return new PreferencesDialog(); } },
                { "d03-template-designer", [] { return new TemplateDesigner(); } },
                { "d05-edit-variable",     [] { return new EditVariableDialog(); } },
                { "d06-about",             [] { return new AboutDialog(); } },
                { "d07-report-problem",    [] { return new ReportBugDialog( nullptr ); } },
                { "d08-update-consent",    [] { return consentBox(); } },
                { "d08-update-available",  [] { return offerBox(); } },
                { "d09-save-changes",      [] { return saveChangesBox(); } },
                { "d10-color-palette",     [] { return new ColorPaletteDialog( "Default", Qt::black, Qt::black ); } },
                { "d11-kiosk",             [] { return new FillWindow( kioskConfig() ); } },
                { "d11-kiosk-admin-pin",   [] { return new AdminDialog( kioskConfig() ); } },
                { "main-window-welcome",   [] { return mainWindow( false ); } },
                { "main-window-editor",    [] { return mainWindow( true ); } },
        };
        QVERIFY( factories.contains( name ) );

        const QString outDir = qEnvironmentVariable( "GLABELS_THEME_SCREENSHOTS" );
        if ( !outDir.isEmpty() )
        {
                QDir().mkpath( outDir );
        }

        for ( auto scheme : { model::Settings::LIGHT_COLOR_SCHEME, model::Settings::DARK_COLOR_SCHEME } )
        {
                setScheme( scheme );
                const bool dark = ( scheme == model::Settings::DARK_COLOR_SCHEME );
                QCOMPARE( theme::Theme::instance().mode(), dark ? theme::Mode::Dark : theme::Mode::Light );

                styleWarnings().clear();

                std::unique_ptr<QWidget> widget( factories.value( name )() );
                if ( size.isValid() )
                {
                        widget->resize( size );
                }
                widget->show();
                QVERIFY( QTest::qWaitForWindowExposed( widget.get() ) );
                QCoreApplication::processEvents();

                QImage image = widget->grab().toImage();
                QVERIFY( !image.isNull() );

                if ( !outDir.isEmpty() )
                {
                        QString base = outDir + "/" + name + ( dark ? "-dark" : "-light" );
                        QVERIFY( image.save( base + ".png" ) );

                        // Every page of a tabbed dialog
                        if ( auto* tabs = widget->findChild<QTabWidget*>() )
                        {
                                for ( int i = 1; i < tabs->count(); i++ )
                                {
                                        tabs->setCurrentIndex( i );
                                        QCoreApplication::processEvents();
                                        widget->grab().toImage().save( base + QString( "-tab%1.png" ).arg( i ) );
                                }
                        }
                }

                QVERIFY2( styleWarnings().isEmpty(), qPrintable( styleWarnings().join( "\n" ) ) );
        }

        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
}


void TestThemeGallery::switchesWithoutRestart()
{
        // An open window follows Light > Dark > Light without being recreated.
        setScheme( model::Settings::LIGHT_COLOR_SCHEME );

        PreferencesDialog dialog;
        dialog.show();
        QVERIFY( QTest::qWaitForWindowExposed( &dialog ) );

        const QColor lightPanel = theme::Theme::lightPalette().panel;
        const QColor darkPanel  = theme::Theme::darkPalette().panel;

        QCOMPARE( dialog.grab().toImage().pixelColor( 3, 3 ), lightPanel );

        setScheme( model::Settings::DARK_COLOR_SCHEME );
        QCOMPARE( AppearanceModel::style(), AppearanceModel::STYLE_DARK );
        QCOMPARE( dialog.grab().toImage().pixelColor( 3, 3 ), darkPanel );

        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
        QCOMPARE( AppearanceModel::style(), AppearanceModel::STYLE_LIGHT );
        QCOMPARE( dialog.grab().toImage().pixelColor( 3, 3 ), lightPanel );

        // Flat / Vector icon style, in both schemes, without a restart (legacy icons, SPEC Q1)
        model::Settings::setIconStyle( model::Settings::VECTOR_ICON_STYLE );
        QCoreApplication::processEvents();
        QCOMPARE( QIcon::themeName(), QString( "glabels-vector" ) );
        setScheme( model::Settings::DARK_COLOR_SCHEME );
        QCOMPARE( QIcon::themeName(), QString( "glabels-vector-dark" ) );
        model::Settings::setIconStyle( model::Settings::FLAT_ICON_STYLE );
        QCoreApplication::processEvents();
        QCOMPARE( QIcon::themeName(), QString( "glabels-flat-dark" ) );
        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
        QCOMPARE( QIcon::themeName(), QString( "glabels-flat" ) );

        // System is offered (Qt 6.8) and resolves through the theme
        QVERIFY( AppearanceModel::supportsSystemColorScheme() );
        setScheme( model::Settings::SYSTEM_COLOR_SCHEME );
        auto expected = theme::Theme::systemPrefersDark() ? AppearanceModel::STYLE_DARK : AppearanceModel::STYLE_LIGHT;
        QCOMPARE( AppearanceModel::style(), expected );

        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
}
