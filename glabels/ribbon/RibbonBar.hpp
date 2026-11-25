//  RibbonBar.hpp
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

#ifndef ribbon_RibbonBar_hpp
#define ribbon_RibbonBar_hpp


#include "RibbonTypes.hpp"

#include <QAction>
#include <QList>
#include <QMap>
#include <QStackedWidget>
#include <QString>
#include <QWidget>

#include <optional>


namespace glabels::ribbon
{

        class QuickAccessBar;
        class RibbonTab;

        ///
        /// Top-level ribbon widget: tab strip (42px) + stacked tab bodies (118px).
        /// Owns the Quick Access bar, File gradient button, normal tabs, contextual tabs,
        /// Welcome mode, and collapsed/popup mode.
        ///
        class RibbonBar : public QWidget
        {
                Q_OBJECT

        public:
                explicit RibbonBar( QWidget* parent = nullptr );

                QuickAccessBar* quickAccessBar() const;

                RibbonTab* addTab( TabId id, const QString& title, bool contextual = false );
                RibbonTab* tab( TabId id ) const;

                TabId currentTab() const;
                void  setCurrentTab( TabId id, bool emitSignal = true );

                /// Show a single contextual tab at the end of the strip (or hide all with nullopt).
                void                 setVisibleContextualTab( std::optional<TabId> tab );
                std::optional<TabId> visibleContextualTab() const;

                /// Welcome mode: only File and Help tabs visible, ribbon body hidden until Help is clicked.
                bool isWelcomeMode() const;
                void setWelcomeMode( bool welcome );

                /// Collapse mode: hide inline ribbon body; clicking a tab opens it as a popup.
                bool isCollapsed() const;
                void setCollapsed( bool collapsed );
                void toggleCollapsed();

                /// Associate a page-switch QAction (e.g. Ctrl+1, Ctrl+P) with a ribbon tab.
                void registerTabAction( TabId id, QAction* action );

                /// All QActions bound across Quick Access, tab headers, and every RibbonTab.
                QList<QAction*> allBoundActions() const;

        signals:
                void tabChanged( TabId id );
                void fileButtonClicked();
                void collapsedChanged( bool collapsed );

        private slots:
                void onTabClicked( TabId id );
                void onTabDoubleClicked( TabId id );
                void onWheelStep( int delta );
                void onThemeChanged();

        private:
                void updateTabStripState();
                void updateBodyVisibility();
                void showCollapsedPopup( TabId id );
                void restoreBodyFromPopup();

                class TabStripWidget;

                TabStripWidget*          mStrip{ nullptr };
                QuickAccessBar*          mQuickAccess{ nullptr };
                QWidget*                 mBodyHost{ nullptr };
                QStackedWidget*          mBodyStack{ nullptr };
                QWidget*                 mCollapsedPopup{ nullptr };

                QMap<TabId, RibbonTab*>  mTabs;
                QMap<TabId, QAction*>    mTabActions;
                QList<TabId>             mTabOrder;
                TabId                    mCurrentTab{ TabId::Home };
                std::optional<TabId>     mVisibleContextual;
                bool                     mWelcomeMode{ true };
                bool                     mWelcomeHelpOpen{ false };
                bool                     mCollapsed{ false };
        };

}


#endif // ribbon_RibbonBar_hpp
