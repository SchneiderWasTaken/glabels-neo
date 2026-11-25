//  TestRibbon.cpp
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


#include "TestRibbon.hpp"

#include "AppearanceModel.hpp"
#include "FillView.hpp"
#include "MainWindow.hpp"
#include "PrintView.hpp"
#include "PropertiesView.hpp"
#include "VariablesView.hpp"
#include "ribbon/Backstage.hpp"
#include "ribbon/ContentPanel.hpp"
#include "ribbon/RibbonBar.hpp"
#include "ribbon/RibbonButton.hpp"
#include "ribbon/RibbonColorDecorator.hpp"
#include "ribbon/RibbonGroup.hpp"
#include "ribbon/RibbonTab.hpp"
#include "ribbon/RibbonTypes.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Model.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"
#include "model/Template.hpp"

#include <QAction>
#include <QMenuBar>
#include <QSet>


using namespace glabels;
using namespace glabels::ribbon;


void TestRibbon::initTestCase()
{
        model::Settings::init();
        model::Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();
}


void TestRibbon::tabPageMapping()
{
        QCOMPARE( pageForTab( TabId::Home, DataSubPage::Merge ), std::optional<PageId>( PageId::Editor ) );
        QCOMPARE( pageForTab( TabId::Label, DataSubPage::Merge ), std::optional<PageId>( PageId::Properties ) );
        QCOMPARE( pageForTab( TabId::Data, DataSubPage::Merge ), std::optional<PageId>( PageId::Merge ) );
        QCOMPARE( pageForTab( TabId::Data, DataSubPage::Variables ), std::optional<PageId>( PageId::Variables ) );
        QCOMPARE( pageForTab( TabId::Data, DataSubPage::Fill ), std::optional<PageId>( PageId::Fill ) );
        QCOMPARE( pageForTab( TabId::Print, DataSubPage::Merge ), std::optional<PageId>( PageId::Print ) );
        QCOMPARE( pageForTab( TabId::Deploy, DataSubPage::Merge ), std::optional<PageId>( PageId::Deploy ) );

        // View and Help stay on the current page (SPEC 3.2)
        QVERIFY( !pageForTab( TabId::View, DataSubPage::Merge ).has_value() );
        QVERIFY( !pageForTab( TabId::Help, DataSubPage::Merge ).has_value() );

        // Contextual tabs map back to Editor
        QCOMPARE( pageForTab( TabId::ContextText, DataSubPage::Merge ), std::optional<PageId>( PageId::Editor ) );
        QCOMPARE( pageForTab( TabId::ContextBarcode, DataSubPage::Merge ), std::optional<PageId>( PageId::Editor ) );
        QCOMPARE( pageForTab( TabId::ContextImage, DataSubPage::Merge ), std::optional<PageId>( PageId::Editor ) );
        QCOMPARE( pageForTab( TabId::ContextShape, DataSubPage::Merge ), std::optional<PageId>( PageId::Editor ) );
        QCOMPARE( pageForTab( TabId::ContextLine, DataSubPage::Merge ), std::optional<PageId>( PageId::Editor ) );

        // Page -> Tab mapping
        QCOMPARE( tabForPage( PageId::Editor ), TabId::Home );
        QCOMPARE( tabForPage( PageId::Properties ), TabId::Label );
        QCOMPARE( tabForPage( PageId::Merge ), TabId::Data );
        QCOMPARE( tabForPage( PageId::Variables ), TabId::Data );
        QCOMPARE( tabForPage( PageId::Fill ), TabId::Data );
        QCOMPARE( tabForPage( PageId::Print ), TabId::Print );
        QCOMPARE( tabForPage( PageId::Deploy ), TabId::Deploy );

        // Data sub-page extraction
        QCOMPARE( dataSubPageForPage( PageId::Merge ), std::optional<DataSubPage>( DataSubPage::Merge ) );
        QCOMPARE( dataSubPageForPage( PageId::Variables ), std::optional<DataSubPage>( DataSubPage::Variables ) );
        QCOMPARE( dataSubPageForPage( PageId::Fill ), std::optional<DataSubPage>( DataSubPage::Fill ) );
        QVERIFY( !dataSubPageForPage( PageId::Editor ).has_value() );
        QVERIFY( !dataSubPageForPage( PageId::Print ).has_value() );
}


void TestRibbon::contextualTabVisibilityAndActivation()
{
        // Selection -> contextual tab mapping
        QCOMPARE( contextualTabForSelection( SelectionKind::Text ), std::optional<TabId>( TabId::ContextText ) );
        QCOMPARE( contextualTabForSelection( SelectionKind::Barcode ), std::optional<TabId>( TabId::ContextBarcode ) );
        QCOMPARE( contextualTabForSelection( SelectionKind::Image ), std::optional<TabId>( TabId::ContextImage ) );
        QCOMPARE( contextualTabForSelection( SelectionKind::Box ), std::optional<TabId>( TabId::ContextShape ) );
        QCOMPARE( contextualTabForSelection( SelectionKind::Ellipse ), std::optional<TabId>( TabId::ContextShape ) );
        QCOMPARE( contextualTabForSelection( SelectionKind::Line ), std::optional<TabId>( TabId::ContextLine ) );
        QVERIFY( !contextualTabForSelection( SelectionKind::None ).has_value() );
        QVERIFY( !contextualTabForSelection( SelectionKind::Multiple ).has_value() );

        // Visibility rules (SPEC 5.3)
        QVERIFY( shouldShowContextualTab( PageId::Editor, SelectionKind::Text ) );
        QVERIFY( shouldShowContextualTab( PageId::Editor, SelectionKind::Barcode ) );
        QVERIFY( !shouldShowContextualTab( PageId::Editor, SelectionKind::None ) );
        QVERIFY( !shouldShowContextualTab( PageId::Editor, SelectionKind::Multiple ) );
        QVERIFY( !shouldShowContextualTab( PageId::Welcome, SelectionKind::Text ) );
        QVERIFY( !shouldShowContextualTab( PageId::Properties, SelectionKind::Text ) );
        QVERIFY( !shouldShowContextualTab( PageId::Merge, SelectionKind::Text ) );
        QVERIFY( !shouldShowContextualTab( PageId::Print, SelectionKind::Text ) );
        QVERIFY( !shouldShowContextualTab( PageId::Deploy, SelectionKind::Text ) );

        // Single-clicking an object on Home does NOT steal tab focus
        QCOMPARE( resolveTabOnSelectionChange( TabId::Home, PageId::Editor, SelectionKind::Text, false ),
                  TabId::Home );

        // Creating or double-clicking an object DOES activate its contextual tab
        QCOMPARE( resolveTabOnSelectionChange( TabId::Home, PageId::Editor, SelectionKind::Text, true ),
                  TabId::ContextText );
        QCOMPARE( resolveTabOnSelectionChange( TabId::Home, PageId::Editor, SelectionKind::Barcode, true ),
                  TabId::ContextBarcode );

        // Switching selection type while already on a contextual tab follows the new type
        QCOMPARE( resolveTabOnSelectionChange( TabId::ContextText, PageId::Editor, SelectionKind::Barcode, false ),
                  TabId::ContextBarcode );
        QCOMPARE( resolveTabOnSelectionChange( TabId::ContextBarcode, PageId::Editor, SelectionKind::Box, false ),
                  TabId::ContextShape );

        // Deselecting or selecting multiple while on a contextual tab falls back to Home
        QCOMPARE( resolveTabOnSelectionChange( TabId::ContextText, PageId::Editor, SelectionKind::None, false ),
                  TabId::Home );
        QCOMPARE( resolveTabOnSelectionChange( TabId::ContextShape, PageId::Editor, SelectionKind::Multiple, false ),
                  TabId::Home );

        // Leaving Editor while on a contextual tab falls back to that page's tab
        QCOMPARE( resolveTabOnSelectionChange( TabId::ContextText, PageId::Print, SelectionKind::Text, false ),
                  TabId::Print );
}


void TestRibbon::tooltipAndMnemonicFormatting()
{
        QCOMPARE( stripMnemonic( QStringLiteral( "&Save" ) ), QStringLiteral( "Save" ) );
        QCOMPARE( stripMnemonic( QStringLiteral( "Save &As..." ) ), QStringLiteral( "Save As..." ) );
        QCOMPARE( stripMnemonic( QStringLiteral( "Line && Fill" ) ), QStringLiteral( "Line & Fill" ) );

        QAction act( QStringLiteral( "&Save" ) );
        act.setShortcut( QKeySequence( Qt::CTRL | Qt::Key_S ) );
        act.setStatusTip( QStringLiteral( "Save current project" ) );

        const QString tip = formatActionTooltip( &act );
        QVERIFY( tip.contains( QStringLiteral( "Save" ) ) );
        QVERIFY( tip.contains( QStringLiteral( "Save current project" ) ) );
        QVERIFY( !tip.contains( QStringLiteral( "&Save" ) ) );
}


void TestRibbon::mainWindowCommandMapCoverage()
{
        MainWindow win;

        // Menu bar is removed (SPEC 3.1)
        QVERIFY( win.menuBar()->actions().isEmpty() );

        // Welcome mode initially
        QVERIFY( win.ribbonBar()->isWelcomeMode() );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Welcome ) );

        // Every QAction registered in MainWindow must appear in the ribbon or backstage
        const QList<QAction*> allActions = win.allWindowActions();
        QVERIFY( !allActions.isEmpty() );

        QSet<QAction*> covered;
        for ( QAction* a : win.ribbonBar()->allBoundActions() )
        {
                covered.insert( a );
        }
        for ( QAction* a : win.backstage()->boundActions() )
        {
                covered.insert( a );
        }

        QStringList missing;
        for ( QAction* a : allActions )
        {
                if ( !covered.contains( a ) )
                {
                        missing.append( a->text() );
                }
        }
        QVERIFY2( missing.isEmpty(),
                  qPrintable( QStringLiteral( "Unbound MainWindow actions: %1" ).arg( missing.join( ", " ) ) ) );

        // Hard rule: Barcode contextual tab has NO Effects/Shadow group; Text/Image/Shape/Line DO
        auto hasGroupNamed = [&win]( TabId tabId, const QString& title ) -> bool
        {
                RibbonTab* t = win.ribbonBar()->tab( tabId );
                if ( !t ) return false;
                for ( RibbonGroup* g : t->groups() )
                {
                        if ( g->title() == title ) return true;
                }
                return false;
        };

        QVERIFY( !hasGroupNamed( TabId::ContextBarcode, QStringLiteral( "Effects" ) ) );
        QVERIFY( hasGroupNamed( TabId::ContextText, QStringLiteral( "Effects" ) ) );
        QVERIFY( hasGroupNamed( TabId::ContextImage, QStringLiteral( "Effects" ) ) );
        QVERIFY( hasGroupNamed( TabId::ContextShape, QStringLiteral( "Effects" ) ) );
        QVERIFY( hasGroupNamed( TabId::ContextLine, QStringLiteral( "Effects" ) ) );
}


void TestRibbon::mainWindowTabPageSyncAndSelection()
{
        MainWindow win;

        auto* model = new model::Model();
        const auto& templates = model::Db::templates();
        if ( !templates.isEmpty() )
        {
                model->setTmplate( templates.first() );
        }
        win.setModel( model );

        QVERIFY( !win.ribbonBar()->isWelcomeMode() );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Editor ) );
        QCOMPARE( win.ribbonBar()->currentTab(), TabId::Home );

        // Tab -> page sync
        win.ribbonBar()->setCurrentTab( TabId::Label, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Properties ) );

        // View and Help stay on the current page (Properties)
        win.ribbonBar()->setCurrentTab( TabId::View, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Properties ) );
        win.ribbonBar()->setCurrentTab( TabId::Help, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Properties ) );

        // Print & Deploy
        win.ribbonBar()->setCurrentTab( TabId::Print, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Print ) );
        win.ribbonBar()->setCurrentTab( TabId::Deploy, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Deploy ) );

        // Data remembers last sub-page, and the active Data page toggle is ENABLED and CHECKED
        win.ribbonBar()->setCurrentTab( TabId::Data, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Merge ) );

        auto findActionByShortcut = [&]( Qt::Key key ) -> QAction*
        {
                for ( QAction* a : win.allWindowActions() )
                {
                        if ( a->shortcut() == QKeySequence( Qt::CTRL | key ) )
                        {
                                return a;
                        }
                }
                return nullptr;
        };
        QAction* mergeAct = findActionByShortcut( Qt::Key_3 );
        QAction* varsAct  = findActionByShortcut( Qt::Key_4 );
        QAction* fillAct  = findActionByShortcut( Qt::Key_5 );
        QVERIFY( mergeAct && varsAct && fillAct );
        QVERIFY( mergeAct->isEnabled() );
        QVERIFY( mergeAct->isChecked() );
        QVERIFY( !varsAct->isChecked() );

        // Trigger Variables action (Ctrl+4) and verify Data remembers it and marks it checked+enabled
        varsAct->trigger();
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Variables ) );
        QCOMPARE( win.ribbonBar()->currentTab(), TabId::Data );
        QVERIFY( varsAct->isEnabled() );
        QVERIFY( varsAct->isChecked() );
        QVERIFY( !mergeAct->isChecked() );

        win.ribbonBar()->setCurrentTab( TabId::Home, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Editor ) );

        win.ribbonBar()->setCurrentTab( TabId::Data, true );
        QCOMPARE( win.pageStack()->currentIndex(), static_cast<int>( PageId::Variables ) );

        // Back to Editor and test contextual tab visibility on object selection
        win.ribbonBar()->setCurrentTab( TabId::Home, true );

        auto* textObj = new model::ModelTextObject();
        auto* bcObj   = new model::ModelBarcodeObject();
        model->addObject( textObj );
        model->addObject( bcObj );

        model->unselectAll();
        QVERIFY( !win.ribbonBar()->visibleContextualTab().has_value() );

        model->selectObject( textObj );
        QCOMPARE( win.ribbonBar()->visibleContextualTab(), std::optional<TabId>( TabId::ContextText ) );
        // Single selection does not steal focus from Home
        QCOMPARE( win.ribbonBar()->currentTab(), TabId::Home );

        // User switches to Text contextual tab, then selects Barcode -> follows to ContextBarcode
        win.ribbonBar()->setCurrentTab( TabId::ContextText, true );
        model->unselectAll();
        model->selectObject( bcObj );
        QCOMPARE( win.ribbonBar()->visibleContextualTab(), std::optional<TabId>( TabId::ContextBarcode ) );
        QCOMPARE( win.ribbonBar()->currentTab(), TabId::ContextBarcode );

        // Selecting both hides contextual tab and returns to Home
        model->selectAll();
        QVERIFY( !win.ribbonBar()->visibleContextualTab().has_value() );
        QCOMPARE( win.ribbonBar()->currentTab(), TabId::Home );

        // Collapse ribbon toggle
        QVERIFY( !win.ribbonBar()->isCollapsed() );
        win.ribbonBar()->toggleCollapsed();
        QVERIFY( win.ribbonBar()->isCollapsed() );
        win.ribbonBar()->toggleCollapsed();
        QVERIFY( !win.ribbonBar()->isCollapsed() );
}


void TestRibbon::fillViewRibbonHostedVsStandalone()
{
        // Default standalone mode for glabels-fill kiosk keeps inline toolbar & output buttons
        FillView standalone;
        QVERIFY( !standalone.isRibbonHosted() );
        QVERIFY( !standalone.addRowButton->isHidden() );
        QVERIFY( !standalone.printButton->isHidden() );
        QVERIFY( !standalone.zplButton->isHidden() );
        QVERIFY( !standalone.systemDialogButton->isHidden() );

        // Ribbon-hosted mode hides redundant inline toolbar & output buttons
        standalone.setRibbonHosted( true );
        QVERIFY( standalone.isRibbonHosted() );
        QVERIFY( standalone.addRowButton->isHidden() );
        QVERIFY( standalone.printButton->isHidden() );
        QVERIFY( standalone.zplButton->isHidden() );
        QVERIFY( standalone.systemDialogButton->isHidden() );

        // Color decorator friendly names
        QCOMPARE( friendlyColorName( QColor( 0, 0, 0 ) ), QStringLiteral( "Black" ) );
        QCOMPARE( friendlyColorName( QColor( 255, 255, 255 ) ), QStringLiteral( "White" ) );
        QCOMPARE( friendlyColorName( QColor( 0x4F, 0x46, 0xE5 ) ), QStringLiteral( "Indigo" ) );
        QCOMPARE( friendlyColorName( QColor( 0, 0, 0, 0 ), QStringLiteral( "No fill" ) ), QStringLiteral( "No fill" ) );

        // Page visual-review invariants: sentence case, Variables intro caption, workWell preview cards
        PropertiesView propsView;
        QCOMPARE( propsView.similarProductsGroupBox->title(), QStringLiteral( "Similar products" ) );
        QCOMPARE( propsView.parametersGroupBox->title(), QStringLiteral( "Adjustable parameters" ) );
        QVERIFY( !propsView.findChildren<QFrame*>().isEmpty() );

        VariablesView varsView;
        bool foundVarsIntro = false;
        for ( QLabel* lbl : varsView.findChildren<QLabel*>() )
        {
                if ( lbl->text().contains( QStringLiteral( "${name}" ) ) )
                {
                        foundVarsIntro = true;
                        break;
                }
        }
        QVERIFY( foundVarsIntro );

        PrintView printView;
        bool foundPrintWorkWell = false;
        for ( QFrame* f : printView.findChildren<QFrame*>() )
        {
                if ( f->property( "role" ).toString() == QStringLiteral( "workWell" ) )
                {
                        foundPrintWorkWell = true;
                        break;
                }
        }
        QVERIFY( foundPrintWorkWell );
}


QTEST_MAIN( TestRibbon )
