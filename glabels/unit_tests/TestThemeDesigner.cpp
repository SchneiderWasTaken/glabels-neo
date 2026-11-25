//  TestThemeDesigner.cpp
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


#include "TestThemeDesigner.hpp"

#include "AppearanceModel.hpp"
#include "TemplateDesigner.hpp"
#include "TemplateDesignerSidebar.hpp"
#include "TemplateDesignerSteps.hpp"

#include "theme/Theme.hpp"
#include "theme/ThemeIcons.hpp"
#include "theme/ThemeScheme.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Settings.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QCommandLinkButton>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSettings>
#include <QStandardPaths>


QTEST_MAIN(TestThemeDesigner)


using namespace glabels;
using namespace glabels::designer;


namespace
{
        using S  = Step;
        using St = StepState;

        QList<StepItem> items( std::initializer_list<std::pair<Step, StepState>> list )
        {
                QList<StepItem> out;
                for ( const auto& p : list )
                {
                        out.append( { p.first, p.second } );
                }
                return out;
        }


        QString describe( const QList<StepItem>& list )
        {
                QStringList parts;
                for ( const auto& i : list )
                {
                        const char* state = i.state == St::Done ? "done" : i.state == St::Current ? "CURRENT" : "todo";
                        parts << QString( "%1:%2" ).arg( stepTitle( i.step ), state );
                }
                return parts.join( ", " );
        }


        void settle()
        {
                for ( int i = 0; i < 6; i++ )
                {
                        QCoreApplication::processEvents();
                }
        }
}


void TestThemeDesigner::initTestCase()
{
        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-neo-tests" );
        QCoreApplication::setApplicationName( "TestThemeDesigner" );
        QSettings().clear();

        model::Settings::init();
        model::Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();

        QCOMPARE( theme::Theme::instance().scheme(), QString( "graphite" ) );
}


void TestThemeDesigner::stepForEveryPage()
{
        QVERIFY( stepForPage( IntroPageId ) == S::Start );
        QVERIFY( stepForPage( PathPageId ) == S::Start );
        QVERIFY( stepForPage( ContinuousPageId ) == S::Start );
        QVERIFY( stepForPage( NamePageId ) == S::Name );
        QVERIFY( stepForPage( PageSizePageId ) == S::PageSize );
        QVERIFY( stepForPage( ShapePageId ) == S::Shape );
        for ( int id : { RectPageId, RoundPageId, EllipsePageId, CdPageId, ThermalPageId } )
        {
                QVERIFY( stepForPage( id ) == S::Size );
        }
        for ( int id : { NLayoutsPageId, OneLayoutPageId, TwoLayoutPageId } )
        {
                QVERIFY( stepForPage( id ) == S::Layout );
        }
        QVERIFY( stepForPage( ApplyPageId ) == S::Save );
}


void TestThemeDesigner::stepListOnStart()
{
        auto list = stepList( { IntroPageId }, IntroPageId, false );
        QVERIFY2( list == items( { { S::Start, St::Current }, { S::Name, St::Todo }, { S::PageSize, St::Todo }, { S::Shape, St::Todo },
                                   { S::Size, St::Todo }, { S::Layout, St::Todo }, { S::Save, St::Todo } } ), qPrintable( describe( list ) ) );
        // Before the wizard has shown anything (no history yet)
        QCOMPARE( stepList( {}, IntroPageId, false ).size(), 7 );
}


void TestThemeDesigner::stepListSheetPath()
{
        // Sheet product: Intro, Name, Page size, Shape, Rect, Number of layouts, One layout, Save
        const QList<int> path = { IntroPageId, NamePageId, PageSizePageId, ShapePageId, RectPageId, NLayoutsPageId, OneLayoutPageId, ApplyPageId };

        auto atSize = stepList( path.mid( 0, 5 ), RectPageId, false );
        QVERIFY2( atSize == items( { { S::Start, St::Done }, { S::Name, St::Done }, { S::PageSize, St::Done }, { S::Shape, St::Done },
                                     { S::Size, St::Current }, { S::Layout, St::Todo }, { S::Save, St::Todo } } ),
                  qPrintable( describe( atSize ) ) );

        // Number of layouts and One layout are both the Layout step
        auto atOneLayout = stepList( path.mid( 0, 7 ), OneLayoutPageId, false );
        QVERIFY( atOneLayout.at( 5 ) == StepItem( { S::Layout, St::Current } ) );
        QVERIFY( atOneLayout.at( 4 ) == StepItem( { S::Size, St::Done } ) );

        auto atSave = stepList( path, ApplyPageId, false );
        for ( int i = 0; i < 6; i++ )
        {
                QVERIFY( atSave.at( i ).state == St::Done );
        }
        QVERIFY( atSave.last() == StepItem( { S::Save, St::Current } ) );
}


void TestThemeDesigner::stepListRollSkipsNumberOfLayouts()
{
        // Roll page size: Rect goes straight to One layout; the Layout step is still one step
        const QList<int> path = { IntroPageId, NamePageId, PageSizePageId, ShapePageId, RectPageId, OneLayoutPageId };
        auto list = stepList( path, OneLayoutPageId, false );
        QCOMPARE( list.size(), 7 );
        QVERIFY( list.at( 5 ) == StepItem( { S::Layout, St::Current } ) );
        QVERIFY( list.at( 6 ) == StepItem( { S::Save, St::Todo } ) );
}


void TestThemeDesigner::stepListAfterBack()
{
        // QWizard drops pages from visitedIds() on Back: Shape -> Back -> Page size
        auto list = stepList( { IntroPageId, NamePageId, PageSizePageId }, PageSizePageId, false );
        QVERIFY2( list == items( { { S::Start, St::Done }, { S::Name, St::Done }, { S::PageSize, St::Current }, { S::Shape, St::Todo },
                                   { S::Size, St::Todo }, { S::Layout, St::Todo }, { S::Save, St::Todo } } ),
                  qPrintable( describe( list ) ) );
}


void TestThemeDesigner::stepListThermalPath()
{
        // New thermal / roll label: Intro, Name, Thermal (label size), Save
        auto atSize = stepList( { IntroPageId, NamePageId, ThermalPageId }, ThermalPageId, true );
        QVERIFY2( atSize == items( { { S::Start, St::Done }, { S::Name, St::Done }, { S::Size, St::Current }, { S::Save, St::Todo } } ),
                  qPrintable( describe( atSize ) ) );
        auto atName = stepList( { IntroPageId, NamePageId }, NamePageId, true );
        QCOMPARE( atName.size(), 4 );
        QVERIFY( atName.at( 1 ) == StepItem( { S::Name, St::Current } ) );
}


void TestThemeDesigner::stepListStaleThermalFlagOnStart()
{
        // Back on Start after choosing thermal: the path is open again, all steps are shown
        auto list = stepList( { IntroPageId }, IntroPageId, true );
        QCOMPARE( list.size(), 7 );
        QVERIFY( list.first() == StepItem( { S::Start, St::Current } ) );
}


void TestThemeDesigner::stepListEditing()
{
        // editTemplate() starts on the Name page: no Start step
        auto list = stepList( { NamePageId }, NamePageId, false );
        QVERIFY2( list == items( { { S::Name, St::Current }, { S::PageSize, St::Todo }, { S::Shape, St::Todo },
                                   { S::Size, St::Todo }, { S::Layout, St::Todo }, { S::Save, St::Todo } } ),
                  qPrintable( describe( list ) ) );
        auto later = stepList( { NamePageId, PageSizePageId, ShapePageId, EllipsePageId }, EllipsePageId, false );
        QCOMPARE( later.size(), 6 );
        QVERIFY( later.at( 3 ) == StepItem( { S::Size, St::Current } ) );
}


void TestThemeDesigner::stepListUnsupportedCopy()
{
        // Copying a path or continuous template shows an explanation that leads back to Start
        auto list = stepList( { IntroPageId, PathPageId }, PathPageId, false );
        QCOMPARE( list.size(), 7 );
        QVERIFY( list.first() == StepItem( { S::Start, St::Current } ) );
        QVERIFY( stepList( { IntroPageId, ContinuousPageId }, ContinuousPageId, false ).first().state == St::Current );
}


void TestThemeDesigner::illustrationsFollowThePalette()
{
        for ( const char* name : { "designer-sidebar", "size-rect", "size-round", "size-ellipse", "size-cd", "layout-one", "layout-two" } )
        {
                for ( const auto& scheme : theme::ThemeSchemes::all() )
                {
                        for ( theme::Mode mode : { theme::Mode::Light, theme::Mode::Dark } )
                        {
                                const auto& p = scheme.palette( mode );
                                const QByteArray svg = theme::ThemeIcons::illustrationSvg( name, p );
                                QVERIFY2( !svg.isEmpty(), name );
                                // No sentinel survives; the tokens are in
                                for ( const char* sentinel : { "#010203", "#040506", "#070809", "#0A0B0C", "#101112" } )
                                {
                                        QVERIFY2( !svg.contains( sentinel ), qPrintable( QString( "%1 %2" ).arg( name, sentinel ) ) );
                                }
                                QVERIFY( svg.contains( p.ink2.name().toLatin1() ) );
                                QVERIFY( svg.contains( p.accent.name().toLatin1() ) || QByteArray( name ) == "layout-one" );
                                // No gradient in the artwork
                                QVERIFY( !svg.contains( "Gradient" ) );
                        }
                }
        }

        // HiDPI: rendered at the device pixel ratio
        QPixmap pm = theme::ThemeIcons::illustration( "designer-sidebar", QSize( 176, 141 ), 2.0 );
        QVERIFY( !pm.isNull() );
        QCOMPARE( pm.devicePixelRatio(), 2.0 );
        QVERIFY( pm.width() >= 350 );

        // The numbers of the size diagrams match the numbered fields of the pages
        const struct { const char* art; int count; } numbered[] = {
                { "size-rect", 7 }, { "size-round", 3 }, { "size-ellipse", 4 }, { "size-cd", 6 } };
        for ( const auto& n : numbered )
        {
                const QByteArray svg = theme::ThemeIcons::illustrationSvg( n.art, theme::Theme::instance().palette() );
                for ( int i = 1; i <= n.count; i++ )
                {
                        QVERIFY2( svg.contains( QByteArray( ">" ) + QByteArray::number( i ) + "</text>" ), qPrintable( QString( "%1 #%2" ).arg( n.art ).arg( i ) ) );
                }
                QVERIFY( !svg.contains( QByteArray( ">" ) + QByteArray::number( n.count + 1 ) + "</text>" ) );
        }
}


void TestThemeDesigner::wizardWalk()
{
        QString outDir = qEnvironmentVariable( "GLABELS_THEME_SCREENSHOTS" );
        if ( !outDir.isEmpty() )
        {
                outDir += "/designer";
                QDir().mkpath( outDir );
        }

        for ( auto scheme : { model::Settings::LIGHT_COLOR_SCHEME, model::Settings::DARK_COLOR_SCHEME } )
        {
                model::Settings::setColorScheme( scheme );
                settle();
                const QString mode = scheme == model::Settings::DARK_COLOR_SCHEME ? "dark" : "light";

                TemplateDesigner wizard;
                wizard.resize( 920, 620 );
                wizard.show();
                QVERIFY( QTest::qWaitForWindowExposed( &wizard ) );
                settle();

                auto* sidebar = wizard.findChild<TemplateDesignerSidebar*>();
                QVERIFY( sidebar );
                QCOMPARE( wizard.sideWidget(), static_cast<QWidget*>( sidebar ) );

                auto save = [&]( const QString& name )
                {
                        settle();
                        QImage image = wizard.grab().toImage();
                        QVERIFY( !image.isNull() );
                        if ( !outDir.isEmpty() )
                        {
                                QVERIFY( image.save( QString( "%1/%2-%3.png" ).arg( outDir, name, mode ) ) );
                        }
                };
                auto steps = [&]() { return describe( sidebar->steps() ); };
                auto currentStep = [&]() -> Step
                {
                        for ( const auto& i : sidebar->steps() )
                        {
                                if ( i.state == St::Current ) return i.step;
                        }
                        return Step::Start;
                };
                auto radio = [&]( int pageId, const char* name ) { return wizard.page( pageId )->findChild<QRadioButton*>( name ); };

                // Start
                QCOMPARE( wizard.currentId(), int( IntroPageId ) );
                QCOMPARE( sidebar->steps().size(), 7 );
                QVERIFY( currentStep() == S::Start );
                auto* intro = wizard.currentPage();
                for ( const char* card : { "copyButton", "newButton", "thermalButton" } )
                {
                        auto* button = intro->findChild<QCommandLinkButton*>( card );
                        QVERIFY( button );
                        QVERIFY( !button->icon().isNull() );
                }
                save( "01-start" );

                // New product
                intro->findChild<QCommandLinkButton*>( "newButton" )->click();
                settle();
                QCOMPARE( wizard.currentId(), int( NamePageId ) );
                QVERIFY( currentStep() == S::Name );
                QVERIFY( sidebar->steps().first().state == St::Done );
                wizard.currentPage()->findChild<QLineEdit*>( "brandEntry" )->setText( "Gallery" );
                wizard.currentPage()->findChild<QLineEdit*>( "partEntry" )->setText( "G-0001" );
                wizard.currentPage()->findChild<QLineEdit*>( "descriptionEntry" )->setText( "Shipping labels" );
                save( "02-name" );

                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( PageSizePageId ) );
                QVERIFY( currentStep() == S::PageSize );
                save( "03-page-size" );

                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( ShapePageId ) );
                save( "04-shape" );

                // Every shape's size page, back to Shape in between (Back must update the steps)
                const struct { const char* radio; int page; const char* name; } shapes[] = {
                        { "roundRadio",   RoundPageId,   "05-size-round" },
                        { "ellipseRadio", EllipsePageId, "05-size-ellipse" },
                        { "cdRadio",      CdPageId,      "05-size-cd" },
                        { "rectRadio",    RectPageId,    "05-size-rect" },
                };
                for ( const auto& shape : shapes )
                {
                        QVERIFY2( radio( ShapePageId, shape.radio ), shape.radio );
                        radio( ShapePageId, shape.radio )->setChecked( true );
                        wizard.next(); settle();
                        QCOMPARE( wizard.currentId(), shape.page );
                        QVERIFY( currentStep() == S::Size );
                        QVERIFY2( sidebar->steps().at( 3 ).state == St::Done, qPrintable( steps() ) );
                        save( shape.name );
                        if ( shape.page != RectPageId )
                        {
                                wizard.back(); settle();
                                QCOMPARE( wizard.currentId(), int( ShapePageId ) );
                                QVERIFY2( currentStep() == S::Shape && sidebar->steps().at( 4 ).state == St::Todo, qPrintable( steps() ) );
                        }
                }

                // Layouts: number of layouts, then one layout; then two layouts
                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( NLayoutsPageId ) );
                QVERIFY( currentStep() == S::Layout );
                save( "06-layouts" );
                radio( NLayoutsPageId, "oneLayoutRadio" )->setChecked( true );
                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( OneLayoutPageId ) );
                QVERIFY( currentStep() == S::Layout );
                save( "07-layout-one" );
                wizard.back(); settle();
                radio( NLayoutsPageId, "twoLayoutsRadio" )->setChecked( true );
                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( TwoLayoutPageId ) );
                save( "07-layout-two" );

                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( ApplyPageId ) );
                QVERIFY( currentStep() == S::Save );
                for ( int i = 0; i < 6; i++ )
                {
                        QVERIFY2( sidebar->steps().at( i ).state == St::Done, qPrintable( steps() ) );
                }
                // Standard buttons: Save is the default (filled accent)
                auto* finish = wizard.button( QWizard::FinishButton );
                QVERIFY( finish->isVisible() );
                save( "08-save" );

                // Back to Start, then the thermal / roll path: Start, Name, Size, Save
                wizard.restart(); settle();
                QCOMPARE( sidebar->steps().size(), 7 );
                wizard.currentPage()->findChild<QCommandLinkButton*>( "thermalButton" )->click();
                settle();
                QCOMPARE( wizard.currentId(), int( NamePageId ) );
                QCOMPARE( sidebar->steps().size(), 4 );
                wizard.currentPage()->findChild<QLineEdit*>( "brandEntry" )->setText( "Gallery" );
                wizard.currentPage()->findChild<QLineEdit*>( "partEntry" )->setText( "G-0002" );
                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( ThermalPageId ) );
                QVERIFY2( describe( sidebar->steps() ) == "Start:done, Name:done, Size:CURRENT, Save:todo", qPrintable( steps() ) );
                save( "05-size-thermal" );
                wizard.next(); settle();
                QCOMPARE( wizard.currentId(), int( ApplyPageId ) );
                save( "08-save-thermal" );

                // Back from Save to Start resets the path
                wizard.back(); wizard.back(); wizard.back(); settle();
                QCOMPARE( wizard.currentId(), int( IntroPageId ) );
                QCOMPARE( sidebar->steps().size(), 7 );

                wizard.reject();
        }

        model::Settings::setColorScheme( model::Settings::LIGHT_COLOR_SCHEME );
        settle();
}
