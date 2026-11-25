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

#include "ribbon/RibbonBar.hpp"
#include "ribbon/RibbonTypes.hpp"
#include "theme/Theme.hpp"
#include "theme/ThemeScheme.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Model.hpp"
#include "model/ModelObject.hpp"
#include "model/Settings.hpp"
#include "model/XmlLabelParser.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QIcon>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
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


        ///
        /// A project with one object of every kind (SPEC 6.2): text, barcode, image, box, ellipse, line
        ///
        QString writeGalleryProject( const QString& dir )
        {
                const QString image = QStringLiteral( GLABELS_SOURCE_DIR "/glabels/images/glabels-logo.png" );
                const QString xml = QStringLiteral(
                        "<?xml version=\"1.0\"?>\n"
                        "<Glabels-document version=\"4.0\">\n"
                        "  <Template brand=\"Avery\" description=\"Name badge labels\" part=\"5095\" size=\"US-Letter\">\n"
                        "    <Label-rectangle y_waste=\"4.5pt\" height=\"168pt\" id=\"0\" round=\"13.5pt\" width=\"243pt\" x_waste=\"4.5pt\">\n"
                        "      <Markup-margin size=\"4.5pt\"/>\n"
                        "      <Layout dy=\"180pt\" y0=\"42pt\" nx=\"2\" x0=\"49.5pt\" ny=\"4\" dx=\"270pt\"/>\n"
                        "    </Label-rectangle>\n"
                        "  </Template>\n"
                        "  <Objects id=\"0\" rotate=\"false\">\n"
                        "    <Object-text x=\"14pt\" y=\"10pt\" w=\"150pt\" h=\"30pt\" a0=\"1\" a1=\"0\" a2=\"0\" a3=\"1\" a4=\"0\" a5=\"0\""
                        " font_family=\"Segoe UI\" font_size=\"18\" font_weight=\"bold\" font_italic=\"false\" font_underline=\"false\""
                        " color=\"0x0f172aff\" align=\"left\" valign=\"top\" wrap=\"word\" line_spacing=\"1\" auto_shrink=\"false\""
                        " shadow=\"false\" lock_aspect_ratio=\"false\"><p>Ethiopia Guji</p></Object-text>\n"
                        "    <Object-image x=\"178pt\" y=\"10pt\" w=\"50pt\" h=\"50pt\" a0=\"1\" a1=\"0\" a2=\"0\" a3=\"1\" a4=\"0\" a5=\"0\""
                        " src=\"%1\" shadow=\"false\" lock_aspect_ratio=\"true\"/>\n"
                        "    <Object-box x=\"14pt\" y=\"48pt\" w=\"40pt\" h=\"30pt\" a0=\"1\" a1=\"0\" a2=\"0\" a3=\"1\" a4=\"0\" a5=\"0\""
                        " line_width=\"1pt\" line_color=\"0x3f3f46ff\" fill_color=\"0xe4e4e7ff\" shadow=\"false\" lock_aspect_ratio=\"false\"/>\n"
                        "    <Object-ellipse x=\"64pt\" y=\"48pt\" w=\"30pt\" h=\"30pt\" a0=\"1\" a1=\"0\" a2=\"0\" a3=\"1\" a4=\"0\" a5=\"0\""
                        " line_width=\"1pt\" line_color=\"0x3f3f46ff\" fill_color=\"0xe4e4e7ff\" shadow=\"false\" lock_aspect_ratio=\"false\"/>\n"
                        "    <Object-line x=\"14pt\" y=\"88pt\" dx=\"214pt\" dy=\"0pt\" a0=\"1\" a1=\"0\" a2=\"0\" a3=\"1\" a4=\"0\" a5=\"0\""
                        " line_width=\"1pt\" line_color=\"0xcbd5e1ff\" shadow=\"false\"/>\n"
                        "    <Object-barcode x=\"14pt\" y=\"98pt\" w=\"214pt\" h=\"50pt\" a0=\"1\" a1=\"0\" a2=\"0\" a3=\"1\" a4=\"0\" a5=\"0\""
                        " backend=\"\" style=\"code39\" data=\"0123456789\" text=\"true\" checksum=\"true\" color=\"0xff\" lock_aspect_ratio=\"false\"/>\n"
                        "  </Objects>\n"
                        "  <Data/>\n"
                        "</Glabels-document>\n" ).arg( image );

                QString path = dir + QStringLiteral( "/Gallery label.glabels" );
                QFile file( path );
                if ( file.open( QIODevice::WriteOnly | QIODevice::Text ) )
                {
                        QTextStream( &file ) << xml;
                }
                return path;
        }


        ///
        /// The first object of a kind in the model
        ///
        model::ModelObject* firstObject( model::Model* model, const char* className )
        {
                for ( auto* object : model->objectList() )
                {
                        if ( QString( object->metaObject()->className() ).endsWith( QString( className ) ) )
                        {
                                return object;
                        }
                }
                return nullptr;
        }


        void settle()
        {
                for ( int i = 0; i < 5; i++ )
                {
                        QCoreApplication::processEvents();
                }
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

        // Start from no saved settings (this scope belongs to the test only), so defaults are tested.
        QSettings().clear();

        model::Settings::init();
        model::Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();

        qInstallMessageHandler( collectWarnings );

        // A user with no saved choice gets the default scheme, Graphite (round 4).
        QCOMPARE( model::Settings::themeScheme(), QString( "graphite" ) );
        QCOMPARE( theme::Theme::instance().scheme(), QString( "graphite" ) );

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

        const QColor lightPanel = theme::Theme::paletteFor( theme::Mode::Light ).panel;
        const QColor darkPanel  = theme::Theme::paletteFor( theme::Mode::Dark ).panel;

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


void TestThemeGallery::mainWindowScreens()
{
        // The real main window at 1440 x 900 (mockups s01..s20), every tab and page, light and dark.
        QTemporaryDir dir;
        QVERIFY( dir.isValid() );
        const QString project = writeGalleryProject( dir.path() );

        const QString outDir = qEnvironmentVariable( "GLABELS_THEME_SCREENSHOTS" );
        if ( !outDir.isEmpty() )
        {
                QDir().mkpath( outDir );
        }

        using ribbon::TabId;

        for ( auto scheme : { model::Settings::LIGHT_COLOR_SCHEME, model::Settings::DARK_COLOR_SCHEME } )
        {
                setScheme( scheme );
                const QString suffix = ( scheme == model::Settings::DARK_COLOR_SCHEME ) ? "-dark" : "-light";
                styleWarnings().clear();

                auto save = [&]( QWidget* window, const QString& name )
                {
                        settle();
                        QImage image = window->grab().toImage();
                        QVERIFY( !image.isNull() );
                        if ( !outDir.isEmpty() )
                        {
                                QVERIFY( image.save( outDir + "/" + name + suffix + ".png" ) );
                        }
                };

                // s01: no project
                {
                        std::unique_ptr<MainWindow> welcome( new MainWindow() );
                        welcome->resize( 1440, 900 );
                        welcome->show();
                        QVERIFY( QTest::qWaitForWindowExposed( welcome.get() ) );
                        save( welcome.get(), "s01-welcome" );
                }

                auto* labelModel = model::XmlLabelParser::readFile( project );
                QVERIFY( labelModel );
                QCOMPARE( labelModel->objectList().size(), 6 );

                std::unique_ptr<MainWindow> window( new MainWindow() );
                window->setModel( labelModel );
                window->resize( 1440, 900 );
                window->show();
                QVERIFY( QTest::qWaitForWindowExposed( window.get() ) );
                auto* bar = window->ribbonBar();
                QVERIFY( bar );

                // s02: File backstage
                QVERIFY( QMetaObject::invokeMethod( window.get(), "onFileButtonClicked" ) );
                save( window.get(), "s02-file-backstage" );
                QVERIFY( QMetaObject::invokeMethod( window.get(), "onBackstageCloseRequested" ) );

                // s03: Home, nothing selected
                labelModel->unselectAll();
                bar->setCurrentTab( TabId::Home );
                save( window.get(), "s03-home" );

                // s04..s08: contextual tabs with a matching object selected
                const struct { const char* cls; TabId tab; const char* name; } contexts[] = {
                        { "ModelTextObject",    TabId::ContextText,    "s04-text-tools" },
                        { "ModelBarcodeObject", TabId::ContextBarcode, "s05-barcode-tools" },
                        { "ModelImageObject",   TabId::ContextImage,   "s06-image-tools" },
                        { "ModelBoxObject",     TabId::ContextShape,   "s07-shape-tools" },
                        { "ModelEllipseObject", TabId::ContextShape,   "s07-shape-tools-ellipse" },
                        { "ModelLineObject",    TabId::ContextLine,    "s08-line-tools" },
                };
                for ( const auto& c : contexts )
                {
                        auto* object = firstObject( labelModel, c.cls );
                        QVERIFY2( object, c.cls );
                        bar->setCurrentTab( TabId::Home );
                        labelModel->unselectAll();
                        labelModel->selectObject( object );
                        settle();
                        QVERIFY2( bar->visibleContextualTab() == c.tab, c.name );
                        bar->setCurrentTab( c.tab );
                        save( window.get(), c.name );
                }
                labelModel->unselectAll();

                // Pages
                bar->setCurrentTab( TabId::Label );
                save( window.get(), "s09-label-tab" );
                QVERIFY( QMetaObject::invokeMethod( window.get(), "fileShowMergePage" ) );
                save( window.get(), "s10-data-merge" );
                QVERIFY( QMetaObject::invokeMethod( window.get(), "fileShowVariablesPage" ) );
                save( window.get(), "s11-data-variables" );
                QVERIFY( QMetaObject::invokeMethod( window.get(), "fileShowFillPage" ) );
                save( window.get(), "s12-data-fill" );
                bar->setCurrentTab( TabId::Print );
                save( window.get(), "s13-print" );
                bar->setCurrentTab( TabId::Deploy );
                save( window.get(), "s14-deploy" );
                bar->setCurrentTab( TabId::Home );
                bar->setCurrentTab( TabId::View );
                save( window.get(), "s15-view-tab" );
                bar->setCurrentTab( TabId::Help );
                save( window.get(), "s16-help-tab" );

                QVERIFY2( styleWarnings().isEmpty(), qPrintable( styleWarnings().join( "\n" ) ) );
        }

        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
}


void TestThemeGallery::themeSchemes()
{
        // Every colour scheme x Light/Dark on the real main window and Preferences, applied live
        // through the stored setting, exactly as the Theme picker does (THEMES.md).
        QTemporaryDir dir;
        QVERIFY( dir.isValid() );
        const QString project = writeGalleryProject( dir.path() );

        QString outDir = qEnvironmentVariable( "GLABELS_THEME_SCREENSHOTS" );
        if ( !outDir.isEmpty() )
        {
                outDir += "/themes";
                QDir().mkpath( outDir );
        }

        using ribbon::TabId;
        QMap<QString, QImage> textTools;   // "<scheme>-<mode>" -> Text tools capture

        for ( const auto& scheme : theme::ThemeSchemes::all() )
        {
                model::Settings::setThemeScheme( scheme.id );
                for ( auto mode : { model::Settings::LIGHT_COLOR_SCHEME, model::Settings::DARK_COLOR_SCHEME } )
                {
                        setScheme( mode );
                        const bool dark = ( mode == model::Settings::DARK_COLOR_SCHEME );
                        const QString prefix = scheme.id + ( dark ? "-dark-" : "-light-" );

                        QCOMPARE( theme::Theme::instance().scheme(), scheme.id );
                        QCOMPARE( theme::Theme::instance().palette().accent, scheme.palette( dark ? theme::Mode::Dark : theme::Mode::Light ).accent );
                        styleWarnings().clear();

                        auto save = [&]( QWidget* window, const QString& screen ) -> QImage
                        {
                                settle();
                                QImage image = window->grab().toImage();
                                if ( !outDir.isEmpty() )
                                {
                                        image.save( outDir + "/" + prefix + screen + ".png" );
                                }
                                return image;
                        };

                        auto* labelModel = model::XmlLabelParser::readFile( project );
                        QVERIFY( labelModel );
                        std::unique_ptr<MainWindow> window( new MainWindow() );
                        window->setModel( labelModel );
                        window->resize( 1440, 900 );
                        window->show();
                        QVERIFY( QTest::qWaitForWindowExposed( window.get() ) );
                        auto* bar = window->ribbonBar();

                        labelModel->unselectAll();
                        bar->setCurrentTab( TabId::Home );
                        save( window.get(), "home" );

                        labelModel->selectObject( firstObject( labelModel, "ModelTextObject" ) );
                        settle();
                        bar->setCurrentTab( TabId::ContextText );
                        textTools.insert( scheme.id + ( dark ? "-dark" : "-light" ), save( window.get(), "text-tools" ) );
                        labelModel->unselectAll();

                        QVERIFY( QMetaObject::invokeMethod( window.get(), "fileShowFillPage" ) );
                        save( window.get(), "data-fill" );
                        bar->setCurrentTab( TabId::Print );
                        save( window.get(), "print" );
                        window.reset();

                        PreferencesDialog prefs;
                        prefs.show();
                        QVERIFY( QTest::qWaitForWindowExposed( &prefs ) );
                        if ( auto* tabs = prefs.findChild<QTabWidget*>() )
                        {
                                for ( int i = 0; i < tabs->count(); i++ )
                                {
                                        if ( tabs->widget( i )->findChild<QComboBox*>( "appearanceThemeCombo" ) )
                                        {
                                                tabs->setCurrentIndex( i );
                                        }
                                }
                        }
                        auto* picker = prefs.findChild<QComboBox*>( "appearanceThemeCombo" );
                        QVERIFY( picker );
                        QCOMPARE( picker->count(), theme::ThemeSchemes::all().size() );
                        QCOMPARE( picker->currentData().toString(), scheme.id );
                        save( &prefs, "preferences" );

                        QVERIFY2( styleWarnings().isEmpty(), qPrintable( scheme.id + ": " + styleWarnings().join( "\n" ) ) );
                }
        }

        // A corrupt saved id falls back to Graphite; an explicit "neo" keeps Neo.
        model::Settings::setThemeScheme( "corrupt\x7f" );
        settle();
        QCOMPARE( theme::Theme::instance().scheme(), QString( "graphite" ) );
        model::Settings::setThemeScheme( "neo" );
        settle();
        QCOMPARE( theme::Theme::instance().scheme(), QString( "neo" ) );

        // The picker drives the setting, live: choose Signal in Preferences.
        {
                PreferencesDialog prefs;
                prefs.show();
                QVERIFY( QTest::qWaitForWindowExposed( &prefs ) );
                auto* picker = prefs.findChild<QComboBox*>( "appearanceThemeCombo" );
                QVERIFY( picker );
                picker->setCurrentIndex( picker->findData( "signal" ) );
                settle();
                QCOMPARE( model::Settings::themeScheme(), QString( "signal" ) );
                QCOMPARE( theme::Theme::instance().scheme(), QString( "signal" ) );
                const auto& signal = theme::ThemeSchemes::find( "signal" );
                QCOMPARE( theme::Theme::instance().palette().accent, signal.palette( theme::Theme::instance().mode() ).accent );
                QVERIFY( qApp->styleSheet().contains( theme::Theme::instance().palette().accent.name() ) );
        }

        // Overview: Text tools of every scheme, light row group on top, dark below, labelled.
        if ( !outDir.isEmpty() )
        {
                const auto& all = theme::ThemeSchemes::all();
                const int cols = 3;
                const QSize tile( 720, 450 );
                const int labelH = 34;
                const int rowsPerMode = ( all.size() + cols - 1 ) / cols;
                QImage overview( cols * tile.width(), 2 * rowsPerMode * ( tile.height() + labelH ) + 48, QImage::Format_RGB32 );
                overview.fill( QColor( "#d4d4d8" ) );
                QPainter painter( &overview );
                painter.setRenderHint( QPainter::SmoothPixmapTransform );
                QFont font = painter.font();
                font.setPixelSize( 18 );
                font.setBold( true );
                painter.setFont( font );
                painter.setPen( QColor( "#18181b" ) );
                painter.drawText( QRect( 0, 0, overview.width(), 24 ), Qt::AlignCenter, "Light" );
                for ( int m = 0; m < 2; m++ )
                {
                        const int top = 24 + m * ( rowsPerMode * ( tile.height() + labelH ) + 24 );
                        if ( m == 1 )
                        {
                                painter.drawText( QRect( 0, top - 24, overview.width(), 24 ), Qt::AlignCenter, "Dark" );
                        }
                        for ( int i = 0; i < all.size(); i++ )
                        {
                                const QString key = all.at( i ).id + ( m == 0 ? "-light" : "-dark" );
                                QRect target( ( i % cols ) * tile.width(), top + ( i / cols ) * ( tile.height() + labelH ), tile.width(), tile.height() );
                                painter.drawImage( target.adjusted( 4, 4, -4, -4 ), textTools.value( key ) );
                                painter.drawText( QRect( target.left(), target.bottom(), target.width(), labelH ), Qt::AlignCenter,
                                                  all.at( i ).displayName() + ( m == 0 ? "  (light)" : "  (dark)" ) );
                        }
                }
                painter.end();
                QVERIFY( overview.save( outDir + "/overview.png" ) );
        }

        model::Settings::setThemeScheme( theme::ThemeSchemes::defaultId() );
        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
}
