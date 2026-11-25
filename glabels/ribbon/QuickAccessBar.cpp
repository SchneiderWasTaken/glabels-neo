//  QuickAccessBar.cpp
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


#include "QuickAccessBar.hpp"

#include "RibbonButton.hpp"
#include "theme/Theme.hpp"

#include <QPainter>


namespace glabels::ribbon
{

        QuickAccessBar::QuickAccessBar( QWidget* parent )
                : QWidget( parent )
        {
                setProperty( "role", QStringLiteral( "ribbonQuickAccess" ) );

                mLayout = new QHBoxLayout( this );
                mLayout->setContentsMargins( 0, 0, 11, 0 );
                mLayout->setSpacing( 1 );
                mLayout->setAlignment( Qt::AlignVCenter );

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &QuickAccessBar::onThemeChanged );
        }


        RibbonButton* QuickAccessBar::addAction( QAction* action, const QString& iconName )
        {
                auto* btn = new RibbonButton( action,
                                              iconName,
                                              RibbonButton::Size::IconOnly,
                                              RibbonButton::Variant::Standard,
                                              this );
                mLayout->addWidget( btn );
                mButtons.append( btn );
                return btn;
        }


        QList<QAction*> QuickAccessBar::boundActions() const
        {
                QList<QAction*> result;
                for ( RibbonButton* btn : mButtons )
                {
                        if ( QAction* act = btn->action() )
                        {
                                result.append( act );
                        }
                }
                return result;
        }


        void QuickAccessBar::onThemeChanged()
        {
                update();
        }


        void QuickAccessBar::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                const auto& pal = theme::Theme::instance().palette();
                p.setPen( pal.line2 );
                p.drawLine( width() - 1, 7, width() - 1, height() - 7 );
        }

} // namespace glabels::ribbon
