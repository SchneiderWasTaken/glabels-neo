//  RibbonTypes.cpp
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


#include "RibbonTypes.hpp"

#include <QKeySequence>


namespace glabels::ribbon
{

        bool isContextualTab( TabId tab )
        {
                switch ( tab )
                {
                case TabId::ContextText:
                case TabId::ContextBarcode:
                case TabId::ContextImage:
                case TabId::ContextShape:
                case TabId::ContextLine:
                        return true;
                default:
                        return false;
                }
        }


        std::optional<TabId> contextualTabForSelection( SelectionKind kind )
        {
                switch ( kind )
                {
                case SelectionKind::Text:    return TabId::ContextText;
                case SelectionKind::Barcode: return TabId::ContextBarcode;
                case SelectionKind::Image:   return TabId::ContextImage;
                case SelectionKind::Box:
                case SelectionKind::Ellipse: return TabId::ContextShape;
                case SelectionKind::Line:    return TabId::ContextLine;
                case SelectionKind::None:
                case SelectionKind::Multiple:
                default:
                        return std::nullopt;
                }
        }


        std::optional<PageId> pageForTab( TabId tab, DataSubPage lastDataPage )
        {
                switch ( tab )
                {
                case TabId::Home:
                case TabId::ContextText:
                case TabId::ContextBarcode:
                case TabId::ContextImage:
                case TabId::ContextShape:
                case TabId::ContextLine:
                        return PageId::Editor;

                case TabId::Label:
                        return PageId::Properties;

                case TabId::Data:
                        switch ( lastDataPage )
                        {
                        case DataSubPage::Merge:     return PageId::Merge;
                        case DataSubPage::Variables: return PageId::Variables;
                        case DataSubPage::Fill:      return PageId::Fill;
                        }
                        return PageId::Merge;

                case TabId::Print:
                        return PageId::Print;

                case TabId::Deploy:
                        return PageId::Deploy;

                case TabId::View:
                case TabId::Help:
                        return std::nullopt;
                }
                return std::nullopt;
        }


        TabId tabForPage( PageId page )
        {
                switch ( page )
                {
                case PageId::Welcome:    return TabId::Home;
                case PageId::Editor:     return TabId::Home;
                case PageId::Properties: return TabId::Label;
                case PageId::Merge:      return TabId::Data;
                case PageId::Variables:  return TabId::Data;
                case PageId::Fill:       return TabId::Data;
                case PageId::Print:      return TabId::Print;
                case PageId::Deploy:     return TabId::Deploy;
                }
                return TabId::Home;
        }


        std::optional<DataSubPage> dataSubPageForPage( PageId page )
        {
                switch ( page )
                {
                case PageId::Merge:     return DataSubPage::Merge;
                case PageId::Variables: return DataSubPage::Variables;
                case PageId::Fill:      return DataSubPage::Fill;
                default:                return std::nullopt;
                }
        }


        bool shouldShowContextualTab( PageId currentPage, SelectionKind kind )
        {
                if ( currentPage != PageId::Editor )
                {
                        return false;
                }
                return contextualTabForSelection( kind ).has_value();
        }


        TabId resolveTabOnSelectionChange( TabId         currentTab,
                                           PageId        currentPage,
                                           SelectionKind selection,
                                           bool          activateRequested )
        {
                if ( !shouldShowContextualTab( currentPage, selection ) )
                {
                        if ( isContextualTab( currentTab ) )
                        {
                                return TabId::Home;
                        }
                        return currentTab;
                }

                const TabId ctxTab = *contextualTabForSelection( selection );
                if ( activateRequested || isContextualTab( currentTab ) )
                {
                        return ctxTab;
                }
                return currentTab;
        }


        QString stripMnemonic( const QString& text )
        {
                QString out;
                out.reserve( text.size() );
                for ( int i = 0; i < text.size(); ++i )
                {
                        if ( text.at( i ) == QLatin1Char( '&' ) )
                        {
                                if ( i + 1 < text.size() && text.at( i + 1 ) == QLatin1Char( '&' ) )
                                {
                                        out.append( QLatin1Char( '&' ) );
                                        ++i;
                                }
                                // Skip single accelerator '&'
                        }
                        else
                        {
                                out.append( text.at( i ) );
                        }
                }
                return out;
        }


        QString formatActionTooltip( const QAction* action )
        {
                if ( !action )
                {
                        return QString();
                }

                const QString title    = stripMnemonic( action->text() ).toHtmlEscaped();
                const QString shortcut = action->shortcut().toString( QKeySequence::NativeText ).toHtmlEscaped();
                const QString tip      = action->statusTip().toHtmlEscaped();

                QString html = QStringLiteral( "<b>%1</b>" ).arg( title );
                if ( !shortcut.isEmpty() )
                {
                        html += QStringLiteral( " <span style=\"font-weight:normal;\">(%1)</span>" ).arg( shortcut );
                }
                if ( !tip.isEmpty() )
                {
                        html += QStringLiteral( "<br>%1" ).arg( tip );
                }
                return html;
        }

} // namespace glabels::ribbon
