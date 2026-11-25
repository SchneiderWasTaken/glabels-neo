//  TestThemePopups.cpp
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


#include "TestThemePopups.hpp"

#include "AppearanceModel.hpp"
#include "BarcodeMenu.hpp"
#include "ColorPaletteDialog.hpp"
#include "EditVariableDialog.hpp"
#include "MainWindow.hpp"
#include "PreferencesDialog.hpp"

#include "ribbon/RibbonBar.hpp"
#include "ribbon/RibbonTypes.hpp"
#include "ribbon/ShadowPopover.hpp"
#include "theme/Theme.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Model.hpp"
#include "model/Settings.hpp"
#include "model/XmlLabelParser.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QDir>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolTip>
#include <QVBoxLayout>

#include <memory>


QTEST_MAIN(TestThemePopups)


using namespace glabels;


namespace
{

        void settle()
        {
                for ( int i = 0; i < 6; i++ )
                {
                        QCoreApplication::processEvents();
                }
        }


        void setScheme( model::Settings::ColorScheme scheme )
        {
                model::Settings::setColorScheme( scheme );
                settle();
        }


        QString shotDir()
        {
                const QString base = qEnvironmentVariable( "GLABELS_THEME_SCREENSHOTS" );
                if ( base.isEmpty() )
                {
                        return QString();
                }
                QString dir = base + "/popups";
                const QString tag = qEnvironmentVariable( "GLABELS_POPUP_TAG" );
                if ( !tag.isEmpty() )
                {
                        dir += "/" + tag;
                }
                QDir().mkpath( dir );
                return dir;
        }


        bool isBlack( const QColor& c )
        {
                return c.red() < 24 && c.green() < 24 && c.blue() < 24;
        }


        ///
        /// Pixels of a popup window that nothing paints (empty list = none).
        ///
        /// The window is rendered with its children but without the window background onto a
        /// magenta sentinel.  Whatever stays magenta is not painted by the popup itself: on
        /// Windows that area of a frameless popup comes out black (the owner's black corner).
        /// The rendering is saved for POPUP-AUDIT.md.
        ///
        QStringList blackCorners( QWidget* window, const QString& name )
        {
                const QColor sentinel( 255, 0, 255 );
                QImage image( window->size(), QImage::Format_ARGB32 );
                image.fill( sentinel );
                window->render( &image, QPoint(), QRegion(), QWidget::DrawChildren );

                const QString dir = shotDir();
                if ( !dir.isEmpty() )
                {
                        QString file = QString( name ).replace( QRegularExpression( "[^A-Za-z0-9_-]" ), "_" );
                        image.save( dir + "/" + file + ".png" );
                }

                const int w = image.width() - 1;
                const int h = image.height() - 1;
                QStringList bad;
                const QPoint corners[] = { { 0, 0 }, { w, 0 }, { 0, h }, { w, h }, { 1, 1 }, { w - 1, 1 }, { 1, h - 1 }, { w - 1, h - 1 } };
                for ( const QPoint& p : corners )
                {
                        QColor c = image.pixelColor( p );
                        if ( c == sentinel || isBlack( c ) )
                        {
                                bad << QString( "%1: corner pixel (%2,%3) not painted" ).arg( name ).arg( p.x() ).arg( p.y() );
                        }
                }
                return bad;
        }


        struct ComboReport
        {
                int  items{ 0 };
                int  fullyVisible{ 0 };
                bool scrolls{ false };
                QStringList blackCorners;
        };


        ComboReport measureCombo( QComboBox* combo, const QString& name, bool corners )
        {
                ComboReport report;
                report.items = combo->count();

                combo->showPopup();
                settle();

                QAbstractItemView* view = combo->view();
                QWidget* container = view->window();
                const QRect viewport = view->viewport()->rect();

                for ( int i = 0; i < combo->count(); i++ )
                {
                        QModelIndex index = combo->model()->index( i, combo->modelColumn(), combo->rootModelIndex() );
                        QRect r = view->visualRect( index );
                        if ( !r.isEmpty() && r.top() >= viewport.top() && r.bottom() <= viewport.bottom() )
                        {
                                report.fullyVisible++;
                        }
                }

                report.scrolls = view->verticalScrollBar()->maximum() > 0;
                for ( QWidget* child : container->findChildren<QWidget*>() )
                {
                        if ( QString( child->metaObject()->className() ).contains( "Scroller" ) && child->isVisible() )
                        {
                                report.scrolls = true;
                        }
                }

                if ( corners )
                {
                        report.blackCorners = blackCorners( container, name );
                }

                combo->hidePopup();
                settle();
                return report;
        }


        ///
        /// The 15-row rule: up to 15 items all visible without scrolling, more scroll with 15 shown
        ///
        QString checkRule( const ComboReport& r, const QString& name )
        {
                const int expected = qMin( r.items, 15 );
                if ( r.fullyVisible < expected )
                {
                        return QString( "%1: %2 items, only %3 fully visible" ).arg( name ).arg( r.items ).arg( r.fullyVisible );
                }
                if ( r.items <= 15 && r.scrolls )
                {
                        return QString( "%1: %2 items but the popup scrolls" ).arg( name ).arg( r.items );
                }
                return QString();
        }


        QString writeProject( const QString& dir )
        {
                QString path = dir + "/popups.glabels";
                QFile file( path );
                if ( file.open( QIODevice::WriteOnly | QIODevice::Text ) )
                {
                        file.write(
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
                                " color=\"0xff\" align=\"left\" valign=\"top\" wrap=\"word\" line_spacing=\"1\" auto_shrink=\"false\""
                                " shadow=\"false\" lock_aspect_ratio=\"false\"><p>Popup audit</p></Object-text>\n"
                                "  </Objects>\n"
                                "  <Data/>\n"
                                "</Glabels-document>\n" );
                }
                return path;
        }
}


void TestThemePopups::initTestCase()
{
        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-neo-tests" );
        QCoreApplication::setApplicationName( "TestThemePopups" );

        // Measure the final geometry, not an animation frame.
        QApplication::setEffectEnabled( Qt::UI_AnimateCombo, false );
        QApplication::setEffectEnabled( Qt::UI_AnimateMenu, false );
        QApplication::setEffectEnabled( Qt::UI_FadeMenu, false );
        QApplication::setEffectEnabled( Qt::UI_AnimateTooltip, false );
        QApplication::setEffectEnabled( Qt::UI_FadeTooltip, false );

        model::Settings::init();
        model::Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();

        // model::Model::canPaste() expects clipboard data; the offscreen platform starts empty.
        QApplication::clipboard()->setText( QStringLiteral( "popups" ) );
}


void TestThemePopups::cleanup()
{
        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
}


void TestThemePopups::comboShowsEveryItem_data()
{
        QTest::addColumn<int>( "count" );
        QTest::addColumn<bool>( "dark" );
        QTest::addColumn<bool>( "editable" );

        for ( bool dark : { false, true } )
        {
                for ( int n : { 1, 2, 3, 4, 5, 8, 10, 11, 12, 15, 16, 30 } )
                {
                        QTest::addRow( "%s-%d", dark ? "dark" : "light", n ) << n << dark << false;
                }
                QTest::addRow( "%s-editable-4", dark ? "dark" : "light" ) << 4 << dark << true;
        }
}


void TestThemePopups::comboShowsEveryItem()
{
        QFETCH( int, count );
        QFETCH( bool, dark );
        QFETCH( bool, editable );

        setScheme( dark ? model::Settings::DARK_COLOR_SCHEME : model::Settings::LIGHT_COLOR_SCHEME );

        QWidget host;
        auto* layout = new QVBoxLayout( &host );
        auto* combo = new QComboBox;
        combo->setEditable( editable );
        for ( int i = 0; i < count; i++ )
        {
                combo->addItem( i == 3 ? QString( "Per page" ) : QString( "Item %1" ).arg( i + 1 ) );
        }
        layout->addWidget( combo );
        host.resize( 300, 120 );
        host.move( 200, 200 );
        host.show();
        QVERIFY( QTest::qWaitForWindowExposed( &host ) );

        const QString name = QString( "combo-%1-%2%3" ).arg( dark ? "dark" : "light" ).arg( count ).arg( editable ? "-editable" : "" );
        ComboReport r = measureCombo( combo, name, !dark );

        QString broken = checkRule( r, name );
        QVERIFY2( broken.isEmpty(), qPrintable( broken ) );
        QVERIFY2( r.blackCorners.isEmpty(), qPrintable( r.blackCorners.join( "\n" ) ) );
}


void TestThemePopups::comboInEveryDialogAndPage()
{
        // Every visible combo box of the real dialogs and pages, light and dark.
        QTemporaryDir dir;
        QVERIFY( dir.isValid() );
        const QString project = writeProject( dir.path() );

        QStringList failures;
        int measured = 0;

        for ( auto scheme : { model::Settings::LIGHT_COLOR_SCHEME, model::Settings::DARK_COLOR_SCHEME } )
        {
                setScheme( scheme );
                const bool light = ( scheme == model::Settings::LIGHT_COLOR_SCHEME );
                const QString mode = light ? "light" : "dark";

                auto checkAll = [&]( QWidget* root, const QString& where )
                {
                        for ( QComboBox* combo : root->findChildren<QComboBox*>() )
                        {
                                if ( !combo->isVisible() || !combo->isEnabled() || combo->count() == 0 )
                                {
                                        continue;
                                }
                                const QString name = QString( "%1-%2-%3" ).arg( mode, where,
                                        combo->objectName().isEmpty() ? QString::number( measured ) : combo->objectName() );
                                ComboReport r = measureCombo( combo, name, light );
                                measured++;
                                QString broken = checkRule( r, name );
                                if ( !broken.isEmpty() )
                                {
                                        failures << broken;
                                }
                                failures << r.blackCorners;
                        }
                };

                {
                        EditVariableDialog dialog;
                        dialog.show();
                        QVERIFY( QTest::qWaitForWindowExposed( &dialog ) );
                        checkAll( &dialog, "edit-variable" );
                }
                {
                        PreferencesDialog dialog;
                        dialog.show();
                        QVERIFY( QTest::qWaitForWindowExposed( &dialog ) );
                        checkAll( &dialog, "preferences" );
                }
                {
                        auto* labelModel = model::XmlLabelParser::readFile( project );
                        QVERIFY( labelModel );
                        MainWindow window;
                        window.setModel( labelModel );
                        window.resize( 1440, 900 );
                        window.show();
                        QVERIFY( QTest::qWaitForWindowExposed( &window ) );
                        auto* bar = window.ribbonBar();

                        labelModel->selectObject( labelModel->objectList().first() );
                        settle();
                        bar->setCurrentTab( ribbon::TabId::ContextText );
                        settle();
                        checkAll( &window, "text-tools" );

                        labelModel->unselectAll();
                        const struct { ribbon::TabId tab; const char* slot; const char* name; } pages[] = {
                                { ribbon::TabId::Label,  nullptr,                 "label" },
                                { ribbon::TabId::Data,   "fileShowMergePage",     "merge" },
                                { ribbon::TabId::Data,   "fileShowFillPage",      "fill" },
                                { ribbon::TabId::Print,  nullptr,                 "print" },
                                { ribbon::TabId::Deploy, nullptr,                 "deploy" },
                                { ribbon::TabId::View,   nullptr,                 "view" },
                        };
                        for ( const auto& page : pages )
                        {
                                bar->setCurrentTab( page.tab );
                                if ( page.slot )
                                {
                                        QMetaObject::invokeMethod( &window, page.slot );
                                }
                                settle();
                                checkAll( &window, page.name );
                        }
                }
        }

        QVERIFY2( measured > 10, qPrintable( QString::number( measured ) ) );
        QVERIFY2( failures.isEmpty(), qPrintable( failures.join( "\n" ) ) );
}


void TestThemePopups::menusAndSubmenus()
{
        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
        QStringList failures;

        auto checkMenu = [&]( QMenu* menu, const QString& name )
        {
                const QRect inside = menu->rect();
                for ( QAction* action : menu->actions() )
                {
                        if ( action->isVisible() && !inside.contains( menu->actionGeometry( action ) ) )
                        {
                                failures << QString( "%1: '%2' not fully inside the menu" ).arg( name, action->text() );
                        }
                }
                failures << blackCorners( menu, name );
        };

        // Context menu shape: commands, separator, submenus
        QMenu context;
        context.addAction( "Cut" );
        context.addAction( "Copy" );
        context.addAction( "Paste" );
        context.addAction( "Delete" );
        context.addSeparator();
        QMenu* order = context.addMenu( "Order" );
        order->addAction( "Bring to front" );
        order->addAction( "Send to back" );
        QAction* checked = order->addAction( "Checked item" );
        checked->setCheckable( true );
        checked->setChecked( true );
        context.popup( QPoint( 200, 200 ) );
        settle();
        checkMenu( &context, "menu-context" );
        order->popup( QPoint( 400, 220 ) );
        settle();
        checkMenu( order, "menu-submenu" );
        order->hide();
        context.hide();

        // Barcode style menu (sections and submenus from the backends)
        BarcodeMenu barcode;
        barcode.popup( QPoint( 200, 100 ) );
        settle();
        checkMenu( &barcode, "menu-barcode" );
        barcode.hide();

        QVERIFY2( failures.isEmpty(), qPrintable( failures.join( "\n" ) ) );
}


void TestThemePopups::customPopups()
{
        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
        QStringList failures;

        // Colour palette popup (every colour button)
        {
                ColorPaletteDialog palette( "Default", Qt::black, Qt::black );
                palette.move( 200, 200 );
                palette.show();
                QVERIFY( QTest::qWaitForWindowExposed( &palette ) );
                settle();
                failures << blackCorners( &palette, "popup-color-palette" );
        }

        // Shadow popover (Effects > Shadow)
        {
                auto* group = new QGroupBox( "Enable shadow" );
                group->setCheckable( true );
                ribbon::ShadowPopover::Controls controls;
                controls.enableGroup = group;
                controls.xSpin       = new QDoubleSpinBox;
                controls.ySpin       = new QDoubleSpinBox;
                controls.colorButton = new QPushButton( "Black" );
                controls.opacitySpin = new QDoubleSpinBox;
                ribbon::ShadowPopover popover( controls );
                popover.move( 200, 200 );
                popover.show();
                QVERIFY( QTest::qWaitForWindowExposed( &popover ) );
                settle();
                failures << blackCorners( &popover, "popup-shadow" );
        }

        // Any frameless popup that paints a rounded panel itself (ribbon group overflow, ...)
        {
                class RoundedPopup : public QWidget
                {
                public:
                        RoundedPopup() : QWidget( nullptr, Qt::Popup | Qt::FramelessWindowHint ) { resize( 220, 120 ); }
                protected:
                        void paintEvent( QPaintEvent* ) override
                        {
                                QPainter p( this );
                                p.setRenderHint( QPainter::Antialiasing );
                                p.setBrush( Qt::white );
                                p.drawRoundedRect( QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 ), 10, 10 );
                        }
                };
                RoundedPopup popup;
                popup.move( 200, 200 );
                popup.show();
                QVERIFY( QTest::qWaitForWindowExposed( &popup ) );
                settle();
                // The theme guard: a frameless popup always fills its background first.
                QVERIFY( popup.autoFillBackground() );
                failures << blackCorners( &popup, "popup-generic-rounded" );
        }

        QVERIFY2( failures.isEmpty(), qPrintable( failures.join( "\n" ) ) );
}


void TestThemePopups::toolTipAndCompleter()
{
        setScheme( model::Settings::LIGHT_COLOR_SCHEME );
        QStringList failures;

        QWidget host;
        auto* layout = new QVBoxLayout( &host );
        auto* edit = new QLineEdit;
        layout->addWidget( edit );
        host.resize( 300, 100 );
        host.move( 200, 200 );
        host.show();
        QVERIFY( QTest::qWaitForWindowExposed( &host ) );

        // Tooltip (D10)
        QToolTip::showText( QPoint( 260, 260 ), "<b>Undo Create Barcode</b> (Ctrl+Z)<br>Undo the last change.", edit );
        settle();
        QWidget* tip = nullptr;
        for ( QWidget* w : QApplication::topLevelWidgets() )
        {
                if ( w->isVisible() && QString( w->metaObject()->className() ) == "QTipLabel" )
                {
                        tip = w;
                }
        }
        QVERIFY( tip );
        failures << blackCorners( tip, "tooltip" );
        QToolTip::hideText();

        // Completer popup: a list view in a popup window, like the combo one
        auto* completer = new QCompleter( QStringList { "Per item", "Per copy", "Per page", "Per sheet", "Per row" }, edit );
        edit->setCompleter( completer );
        completer->setCompletionPrefix( "Per" );
        completer->complete();
        settle();
        QAbstractItemView* popup = completer->popup();
        QVERIFY( popup->isVisible() );
        int visible = 0;
        for ( int i = 0; i < completer->completionCount(); i++ )
        {
                QRect r = popup->visualRect( popup->model()->index( i, 0 ) );
                if ( !r.isEmpty() && r.top() >= 0 && r.bottom() <= popup->viewport()->rect().bottom() )
                {
                        visible++;
                }
        }
        if ( visible < 5 )
        {
                failures << QString( "completer: only %1 of 5 rows visible" ).arg( visible );
        }
        failures << blackCorners( popup, "completer" );
        popup->hide();

        QVERIFY2( failures.isEmpty(), qPrintable( failures.join( "\n" ) ) );
}
