//  RibbonColorDecorator.cpp
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


#include "RibbonColorDecorator.hpp"

#include "ColorButton.hpp"
#include "theme/Theme.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QObject>
#include <QPainter>
#include <QPainterPath>


namespace glabels::ribbon
{

        namespace
        {
                class RibbonColorFilter : public QObject
                {
                public:
                        RibbonColorFilter( ColorButton* button, const QString& transparentLabel )
                                : QObject( button )
                                , mButton( button )
                                , mTransparentLabel( transparentLabel )
                        {
                                mButton->installEventFilter( this );
                                mButton->setAttribute( Qt::WA_Hover, true );
                                mButton->setCursor( Qt::PointingHandCursor );
                                mButton->setMinimumSize( 96, theme::Metrics::field );
                                mButton->setFixedHeight( theme::Metrics::field );
                                mButton->setProperty( "role", QStringLiteral( "ribbonField" ) );

                                connect( &theme::Theme::instance(), &theme::Theme::changed,
                                         mButton, QOverload<>::of( &QWidget::update ) );
                        }

                protected:
                        bool eventFilter( QObject* watched, QEvent* event ) override
                        {
                                if ( watched != mButton )
                                {
                                        return false;
                                }
                                if ( event->type() == QEvent::HoverEnter )
                                {
                                        mHovered = true;
                                        mButton->update();
                                        return false;
                                }
                                if ( event->type() == QEvent::HoverLeave )
                                {
                                        mHovered = false;
                                        mButton->update();
                                        return false;
                                }
                                if ( event->type() == QEvent::Paint )
                                {
                                        paintButton();
                                        return true;
                                }
                                return false;
                        }

                private:
                        void paintButton()
                        {
                                QPainter p( mButton );
                                p.setRenderHint( QPainter::Antialiasing, true );

                                const auto& th  = theme::Theme::instance();
                                const auto& pal = th.palette();

                                const bool enabled = mButton->isEnabled();
                                if ( !enabled )
                                {
                                        p.setOpacity( 0.40 );
                                }

                                const QRectF r = QRectF( mButton->rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );
                                const bool focused = mButton->hasFocus() && enabled;
                                const QColor bg = ( enabled && ( mButton->isDown() || mButton->isChecked() ) )
                                                  ? pal.pressed
                                                  : ( ( enabled && mHovered ) ? pal.hover : pal.bg );

                                p.setBrush( bg );
                                p.setPen( QPen( focused ? pal.accent : pal.line2, focused ? 2.0 : 1.0 ) );
                                p.drawRoundedRect( r, 6.0, 6.0 );

                                const model::ColorNode node = mButton->colorNode();
                                const int h = mButton->height();
                                const int w = mButton->width();

                                const int chevRight = w - 10;
                                int textX = 8;

                                if ( node.isField() )
                                {
                                        // Draw mono field text "${key}"
                                        const QString label = QStringLiteral( "${%1}" ).arg( node.key() );
                                        p.setFont( th.monoFont( 11 ) );
                                        p.setPen( pal.accent );
                                        const QFontMetrics fm( p.font() );
                                        const int avail = qMax( 0, chevRight - 10 - textX );
                                        p.drawText( QRect( textX, 0, avail, h ),
                                                    Qt::AlignVCenter | Qt::AlignLeft,
                                                    fm.elidedText( label, Qt::ElideRight, avail ) );
                                }
                                else
                                {
                                        const QColor c = node.color();
                                        const QRectF swatch( 7.5, ( h - 13.0 ) / 2.0, 13.0, 13.0 );

                                        if ( c.alpha() == 0 )
                                        {
                                                p.setBrush( pal.panel );
                                                p.setPen( QPen( pal.line2, 1.0 ) );
                                                p.drawRoundedRect( swatch, 3.0, 3.0 );
                                                p.setPen( QPen( pal.warn, 1.5 ) );
                                                p.drawLine( swatch.bottomLeft() + QPointF( 2.0, -2.0 ),
                                                            swatch.topRight() + QPointF( -2.0, 2.0 ) );
                                        }
                                        else
                                        {
                                                p.setBrush( c );
                                                p.setPen( QPen( pal.line2, 1.0 ) );
                                                p.drawRoundedRect( swatch, 3.0, 3.0 );
                                        }

                                        textX = 27;
                                        const QString name = friendlyColorName( c, mTransparentLabel );
                                        p.setFont( th.uiFont( 12, 400 ) );
                                        p.setPen( pal.ink );
                                        const QFontMetrics fm( p.font() );
                                        const int avail = qMax( 0, chevRight - 10 - textX );
                                        p.drawText( QRect( textX, 0, avail, h ),
                                                    Qt::AlignVCenter | Qt::AlignLeft,
                                                    fm.elidedText( name, Qt::ElideRight, avail ) );
                                }

                                // Right chevron
                                const QIcon chev = th.icon( QStringLiteral( "chevron-down" ), theme::IconRole::Normal );
                                if ( !chev.isNull() )
                                {
                                        chev.paint( &p, QRect( w - 18, ( h - 12 ) / 2, 12, 12 ) );
                                }
                                else
                                {
                                        QPainterPath cp;
                                        const qreal cx = w - 11.0;
                                        const qreal cy = h / 2.0;
                                        cp.moveTo( cx - 3.5, cy - 1.5 );
                                        cp.lineTo( cx, cy + 2.0 );
                                        cp.lineTo( cx + 3.5, cy - 1.5 );
                                        p.setPen( QPen( pal.ink2, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
                                        p.setBrush( Qt::NoBrush );
                                        p.drawPath( cp );
                                }
                        }

                        ColorButton* mButton{ nullptr };
                        QString      mTransparentLabel;
                        bool         mHovered{ false };
                };
        }


        void installRibbonColorStyle( ColorButton* button, const QString& transparentLabel )
        {
                if ( !button )
                {
                        return;
                }
                new RibbonColorFilter( button, transparentLabel );
        }


        QString friendlyColorName( const QColor& color, const QString& transparentLabel )
        {
                if ( color.alpha() == 0 )
                {
                        return transparentLabel.isEmpty() ? QObject::tr( "None" ) : transparentLabel;
                }

                const QRgb rgb = color.rgb() & 0x00FFFFFF;
                switch ( rgb )
                {
                case 0x000000:
                case 0x0F172A: return QObject::tr( "Black" );
                case 0xFFFFFF: return QObject::tr( "White" );
                case 0xEF4444:
                case 0xCC0000:
                case 0xFF0000: return QObject::tr( "Red" );
                case 0x22C55E:
                case 0x16A34A:
                case 0x00FF00:
                case 0x008000: return QObject::tr( "Green" );
                case 0x3B82F6:
                case 0x2563EB:
                case 0x0000FF: return QObject::tr( "Blue" );
                case 0x4F46E5:
                case 0x6366F1: return QObject::tr( "Indigo" );
                case 0xEDE9FE: return QObject::tr( "Lavender" );
                case 0x8B5CF6:
                case 0x7C3AED: return QObject::tr( "Violet" );
                case 0xCBD5E1:
                case 0x94A3B8: return QObject::tr( "Slate" );
                case 0x808080: return QObject::tr( "Gray" );
                case 0xF59E0B:
                case 0xFFA500: return QObject::tr( "Orange" );
                case 0xEAB308:
                case 0xFFFF00: return QObject::tr( "Yellow" );
                case 0x06B6D4:
                case 0x00FFFF: return QObject::tr( "Cyan" );
                default:       break;
                }

                return color.name( QColor::HexRgb ).toUpper();
        }

} // namespace glabels::ribbon
