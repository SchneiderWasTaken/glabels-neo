//  RibbonTypes.hpp
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

#ifndef ribbon_RibbonTypes_hpp
#define ribbon_RibbonTypes_hpp


#include <QAction>
#include <QString>

#include <optional>


namespace glabels::ribbon
{

        ///
        /// Ribbon tab identifiers
        ///
        enum class TabId
        {
                Home,
                Label,
                Data,
                Print,
                Deploy,
                View,
                Help,
                ContextText,
                ContextBarcode,
                ContextImage,
                ContextShape,
                ContextLine
        };


        ///
        /// Sub-page selection within the Data tab
        ///
        enum class DataSubPage
        {
                Merge,
                Variables,
                Fill
        };


        ///
        /// Stacked page identifiers matching MainWindow::mPages
        ///
        enum class PageId
        {
                Welcome    = 0,
                Editor     = 1,
                Properties = 2,
                Merge      = 3,
                Variables  = 4,
                Print      = 5,
                Fill       = 6,
                Deploy     = 7
        };


        ///
        /// Current editor selection classification
        ///
        enum class SelectionKind
        {
                None,
                Multiple,
                Text,
                Barcode,
                Image,
                Box,
                Ellipse,
                Line
        };


        ///
        /// True when tab is one of the five contextual object tabs
        ///
        bool isContextualTab( TabId tab );


        ///
        /// Return the contextual tab for a single selected object, or nullopt for None/Multiple
        ///
        std::optional<TabId> contextualTabForSelection( SelectionKind kind );


        ///
        /// Return the page to show for a given ribbon tab.
        /// View and Help return nullopt (stay on the current page).
        ///
        std::optional<PageId> pageForTab( TabId tab, DataSubPage lastDataPage );


        ///
        /// Return the ribbon tab that owns a given page.
        ///
        TabId tabForPage( PageId page );


        ///
        /// Return the DataSubPage for a Data-tab page, or nullopt for non-Data pages.
        ///
        std::optional<DataSubPage> dataSubPageForPage( PageId page );


        ///
        /// True if contextual tab should be shown given the current page and selection.
        /// Per SPEC 5.3: on Label, Data, Print, Deploy tabs no contextual tab is shown.
        ///
        bool shouldShowContextualTab( PageId currentPage, SelectionKind kind );


        ///
        /// Resolve the active tab after a selection change.
        ///
        /// - Single object + activateRequested (after creation or double-click) -> contextual tab.
        /// - Already on a contextual tab + selection becomes empty/multiple -> Home.
        /// - Already on a contextual tab + selection changes to another single type -> new contextual tab.
        /// - Otherwise keep currentTab unchanged (single click does not steal focus).
        ///
        TabId resolveTabOnSelectionChange( TabId         currentTab,
                                           PageId        currentPage,
                                           SelectionKind selection,
                                           bool          activateRequested );


        ///
        /// Strip Qt keyboard accelerator ampersands ("&Save As..." -> "Save As...", "A&&B" -> "A&B")
        ///
        QString stripMnemonic( const QString& text );


        ///
        /// Format a rich-text ribbon tooltip (SPEC 5.2, D10):
        /// bold title (without &), shortcut in parentheses, status tip underneath.
        ///
        QString formatActionTooltip( const QAction* action );

}


#endif // ribbon_RibbonTypes_hpp
