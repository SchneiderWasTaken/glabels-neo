//  RibbonTab.cpp
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


#include "RibbonTab.hpp"

#include "RibbonGroup.hpp"
#include "theme/Theme.hpp"

#include <QPainter>

#include <algorithm>


namespace glabels::ribbon
{

        RibbonTab::RibbonTab( TabId id, const QString& title, bool contextual, QWidget* parent )
                : QWidget( parent )
                , mId( id )
                , mTitle( title )
                , mContextual( contextual )
        {
                setFixedHeight( theme::Metrics::ribbon );
                setProperty( "role", QStringLiteral( "ribbonBody" ) );

                mLayout = new QHBoxLayout( this );
                mLayout->setContentsMargins( theme::Metrics::groupPad, 6, theme::Metrics::groupPad, 0 );
                mLayout->setSpacing( 0 );
                mLayout->setAlignment( Qt::AlignLeft | Qt::AlignTop );
                mLayout->addStretch( 1 );

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &RibbonTab::onThemeChanged );
        }


        TabId RibbonTab::id() const
        {
                return mId;
        }


        QString RibbonTab::title() const
        {
                return mTitle;
        }


        bool RibbonTab::isContextual() const
        {
                return mContextual;
        }


        RibbonGroup* RibbonTab::addGroup( const QString& title,
                                          const QString& overflowIconName,
                                          int            collapsePriority )
        {
                auto* group = new RibbonGroup( title, overflowIconName, collapsePriority, this );
                mGroups.append( group );
                // Insert before trailing stretch
                mLayout->insertWidget( mLayout->count() - 1, group );
                updateLayoutState();
                return group;
        }


        const QList<RibbonGroup*>& RibbonTab::groups() const
        {
                return mGroups;
        }


        void RibbonTab::updateLayoutState()
        {
                // Update trailing separator visibility: last visible group has no right divider.
                RibbonGroup* lastVisible = nullptr;
                QList<RibbonGroup*> visibleGroups;
                for ( RibbonGroup* g : mGroups )
                {
                        if ( !g->isHidden() )
                        {
                                lastVisible = g;
                                visibleGroups.append( g );
                        }
                }
                for ( RibbonGroup* g : visibleGroups )
                {
                        g->setShowSeparator( g != lastVisible );
                }

                // Narrow-window overflow: compute total expanded width.
                const int availW = width() - 2 * theme::Metrics::groupPad;
                if ( availW <= 0 || visibleGroups.isEmpty() )
                {
                        return;
                }

                // Order candidates by descending collapsePriority, breaking ties right-to-left (last first).
                QList<int> order;
                order.reserve( visibleGroups.size() );
                for ( int i = 0; i < visibleGroups.size(); ++i )
                {
                        order.append( i );
                }
                std::stable_sort( order.begin(), order.end(),
                                  [&visibleGroups]( int a, int b )
                                  {
                                          const int pa = visibleGroups.at( a )->collapsePriority();
                                          const int pb = visibleGroups.at( b )->collapsePriority();
                                          if ( pa != pb )
                                          {
                                                  return pa > pb;
                                          }
                                          return a > b;
                                  } );

                int neededW = 0;
                for ( RibbonGroup* g : visibleGroups )
                {
                        neededW += g->expandedWidthHint();
                }

                constexpr int collapsedGroupW = theme::Metrics::largeW + 2 * theme::Metrics::groupPad + 12;

                QList<bool> collapseFlags( visibleGroups.size(), false );
                for ( int idx : order )
                {
                        if ( neededW <= availW )
                        {
                                break;
                        }
                        const int expW = visibleGroups.at( idx )->expandedWidthHint();
                        if ( expW > collapsedGroupW )
                        {
                                collapseFlags[idx] = true;
                                neededW -= ( expW - collapsedGroupW );
                        }
                }

                for ( int i = 0; i < visibleGroups.size(); ++i )
                {
                        visibleGroups.at( i )->setCollapsed( collapseFlags.at( i ) );
                }
        }


        QList<QAction*> RibbonTab::boundActions() const
        {
                QList<QAction*> result;
                for ( RibbonGroup* g : mGroups )
                {
                        result.append( g->boundActions() );
                }
                return result;
        }


        void RibbonTab::resizeEvent( QResizeEvent* event )
        {
                QWidget::resizeEvent( event );
                updateLayoutState();
        }


        void RibbonTab::showEvent( QShowEvent* event )
        {
                QWidget::showEvent( event );
                updateLayoutState();
        }


        void RibbonTab::onThemeChanged()
        {
                update();
        }


        void RibbonTab::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                const auto& pal = theme::Theme::instance().palette();
                p.fillRect( rect(), pal.ribbon );
                p.setPen( pal.line );
                p.drawLine( 0, height() - 1, width(), height() - 1 );
        }

} // namespace glabels::ribbon
