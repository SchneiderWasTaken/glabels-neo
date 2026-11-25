//  RibbonToggleGroup.cpp
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


#include "RibbonToggleGroup.hpp"

#include "theme/Theme.hpp"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>


namespace glabels::ribbon
{

        RibbonToggleGroup::RibbonToggleGroup( QWidget* parent )
                : QWidget( parent )
        {
                setFocusPolicy( Qt::TabFocus );
                setMouseTracking( true );
                setCursor( Qt::PointingHandCursor );
                setFixedHeight( theme::Metrics::field );
                setProperty( "role", QStringLiteral( "ribbonToggleGroup" ) );

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &RibbonToggleGroup::onThemeChanged );
        }


        void RibbonToggleGroup::addOption( int            id,
                                           const QString& label,
                                           const QString& iconName,
                                           const QString& toolTip )
        {
                Item item;
                item.option.id       = id;
                item.option.label    = label;
                item.option.iconName = iconName;
                item.option.toolTip  = toolTip;
                mItems.append( item );

                if ( mCurrentIndex < 0 )
                {
                        mCurrentIndex = 0;
                }

                updateGeometry();
                update();
        }


        int RibbonToggleGroup::currentId() const
        {
                if ( mCurrentIndex >= 0 && mCurrentIndex < mItems.size() )
                {
                        return mItems.at( mCurrentIndex ).option.id;
                }
                return -1;
        }


        void RibbonToggleGroup::setCurrentId( int id )
        {
                const int idx = indexForId( id );
                if ( idx >= 0 && idx != mCurrentIndex )
                {
                        mCurrentIndex = idx;
                        update();
                }
        }


        void RibbonToggleGroup::setOptionEnabled( int id, bool enabled )
        {
                const int idx = indexForId( id );
                if ( idx >= 0 )
                {
                        mItems[idx].enabled = enabled;
                        update();
                }
        }


        int RibbonToggleGroup::indexForId( int id ) const
        {
                for ( int i = 0; i < mItems.size(); ++i )
                {
                        if ( mItems.at( i ).option.id == id )
                        {
                                return i;
                        }
                }
                return -1;
        }


        QSize RibbonToggleGroup::sizeHint() const
        {
                const auto& th = theme::Theme::instance();
                const QFontMetrics fm( th.uiFont( 12, 600 ) );

                int segW = 48;
                for ( const auto& item : mItems )
                {
                        int w = 20;
                        if ( !item.option.label.isEmpty() )
                        {
                                w += fm.horizontalAdvance( item.option.label );
                        }
                        if ( !item.option.iconName.isEmpty() )
                        {
                                w += 18;
                        }
                        segW = qMax( segW, w );
                }
                const int totalW = qMax( 1, mItems.size() ) * segW;
                return QSize( totalW, theme::Metrics::field );
        }


        QSize RibbonToggleGroup::minimumSizeHint() const
        {
                return sizeHint();
        }


        QRect RibbonToggleGroup::segmentRect( int index ) const
        {
                if ( mItems.isEmpty() )
                {
                        return QRect();
                }
                const int n = mItems.size();
                const int x0 = ( index * width() ) / n;
                const int x1 = ( ( index + 1 ) * width() ) / n;
                return QRect( x0, 0, x1 - x0, height() );
        }


        int RibbonToggleGroup::indexAt( const QPoint& pos ) const
        {
                for ( int i = 0; i < mItems.size(); ++i )
                {
                        if ( segmentRect( i ).contains( pos ) )
                        {
                                return i;
                        }
                }
                return -1;
        }


        void RibbonToggleGroup::mousePressEvent( QMouseEvent* event )
        {
                if ( event->button() == Qt::LeftButton )
                {
                        const int idx = indexAt( event->pos() );
                        if ( idx >= 0 && mItems.at( idx ).enabled && idx != mCurrentIndex )
                        {
                                mCurrentIndex = idx;
                                update();
                                emit currentIdChanged( mItems.at( idx ).option.id );
                        }
                }
                QWidget::mousePressEvent( event );
        }


        void RibbonToggleGroup::mouseMoveEvent( QMouseEvent* event )
        {
                const int idx = indexAt( event->pos() );
                if ( idx != mHoveredIndex )
                {
                        mHoveredIndex = idx;
                        if ( idx >= 0 && !mItems.at( idx ).option.toolTip.isEmpty() )
                        {
                                setToolTip( mItems.at( idx ).option.toolTip );
                        }
                        update();
                }
                QWidget::mouseMoveEvent( event );
        }


        void RibbonToggleGroup::leaveEvent( QEvent* event )
        {
                mHoveredIndex = -1;
                update();
                QWidget::leaveEvent( event );
        }


        void RibbonToggleGroup::keyPressEvent( QKeyEvent* event )
        {
                if ( mItems.isEmpty() )
                {
                        QWidget::keyPressEvent( event );
                        return;
                }

                int delta = 0;
                if ( event->key() == Qt::Key_Left )
                {
                        delta = -1;
                }
                else if ( event->key() == Qt::Key_Right )
                {
                        delta = 1;
                }

                if ( delta != 0 )
                {
                        int next = mCurrentIndex;
                        for ( int step = 0; step < mItems.size(); ++step )
                        {
                                next = ( next + delta + mItems.size() ) % mItems.size();
                                if ( mItems.at( next ).enabled )
                                {
                                        if ( next != mCurrentIndex )
                                        {
                                                mCurrentIndex = next;
                                                update();
                                                emit currentIdChanged( mItems.at( next ).option.id );
                                        }
                                        event->accept();
                                        return;
                                }
                        }
                }

                QWidget::keyPressEvent( event );
        }


        void RibbonToggleGroup::onThemeChanged()
        {
                update();
        }


        void RibbonToggleGroup::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                p.setRenderHint( QPainter::Antialiasing, true );

                const auto& th  = theme::Theme::instance();
                const auto& pal = th.palette();

                const QRectF outer = QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );
                QPainterPath clip;
                clip.addRoundedRect( outer, 6.0, 6.0 );

                // Base background
                p.fillPath( clip, pal.bg );

                p.save();
                p.setClipPath( clip );

                for ( int i = 0; i < mItems.size(); ++i )
                {
                        const QRect r = segmentRect( i );
                        const bool active  = ( i == mCurrentIndex );
                        const bool hovered = ( i == mHoveredIndex && mItems.at( i ).enabled );

                        if ( active )
                        {
                                p.fillRect( r, pal.pressed );
                        }
                        else if ( hovered )
                        {
                                p.fillRect( r, pal.hover );
                        }

                        if ( i > 0 )
                        {
                                p.setPen( pal.line2 );
                                p.drawLine( r.left(), r.top() + 1, r.left(), r.bottom() - 1 );
                        }

                        p.save();
                        if ( !mItems.at( i ).enabled )
                        {
                                p.setOpacity( 0.40 );
                        }
                        p.setFont( th.uiFont( 12, active ? 600 : 400 ) );
                        p.setPen( active ? pal.accent : pal.ink );
                        p.drawText( r, Qt::AlignCenter, mItems.at( i ).option.label );
                        p.restore();
                }

                p.restore();

                // Outer border
                p.setPen( QPen( hasFocus() ? pal.accent : pal.line2, hasFocus() ? 2.0 : 1.0 ) );
                p.setBrush( Qt::NoBrush );
                p.drawRoundedRect( outer, 6.0, 6.0 );
        }

} // namespace glabels::ribbon
