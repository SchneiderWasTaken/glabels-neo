//  TemplateDesignerSidebar.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//


#include "TemplateDesignerSidebar.hpp"

#include "theme/Theme.hpp"
#include "theme/ThemeIcons.hpp"

#include <QPainter>
#include <QPainterPath>


namespace
{
        constexpr int sideWidth  = 210;
        constexpr int topPad     = 16;
        constexpr int captionH   = 28;
        constexpr int rowH       = 34;
        constexpr int rowGap     = 0;
        constexpr int hPad       = 12;
        constexpr int circle     = 18;
        constexpr int artMaxW    = 176;
}


namespace glabels
{

        TemplateDesignerSidebar::TemplateDesignerSidebar( QWidget* parent )
                : QWidget( parent )
        {
                setObjectName( QStringLiteral( "templateDesignerSidebar" ) );
                setAccessibleName( tr( "Steps" ) );
                setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
                connect( &theme::Theme::instance(), &theme::Theme::changed, this, qOverload<>( &QWidget::update ) );
        }


        void TemplateDesignerSidebar::setSteps( const QList<designer::StepItem>& steps )
        {
                mSteps = steps;

                // Screen readers: "Step 3 of 7, Page size, current"
                QStringList parts;
                for ( int i = 0; i < mSteps.size(); i++ )
                {
                        if ( mSteps.at( i ).state == designer::StepState::Current )
                        {
                                parts << tr( "Step %1 of %2, %3" ).arg( i + 1 ).arg( mSteps.size() )
                                         .arg( designer::stepTitle( mSteps.at( i ).step ) );
                        }
                }
                setAccessibleDescription( parts.join( QString() ) );
                update();
        }


        QSize TemplateDesignerSidebar::sizeHint() const
        {
                return QSize( sideWidth, topPad + captionH + 7 * ( rowH + rowGap ) + 180 );
        }


        QSize TemplateDesignerSidebar::minimumSizeHint() const
        {
                return QSize( sideWidth, topPad + captionH + 7 * ( rowH + rowGap ) + 16 );
        }


        void TemplateDesignerSidebar::paintEvent( QPaintEvent* )
        {
                const auto& th = theme::Theme::instance();
                const auto& p  = th.palette();

                QPainter painter( this );
                painter.setRenderHints( QPainter::Antialiasing | QPainter::TextAntialiasing );

                // Surface (chrome) with a hairline on the right, like the backstage navigation
                painter.fillRect( rect(), p.chrome );
                painter.fillRect( QRect( width() - 1, 0, 1, height() ), p.line );

                // Caption
                QFont caption = th.uiFont( 11, 600 );
                caption.setLetterSpacing( QFont::AbsoluteSpacing, 0.6 );
                painter.setFont( caption );
                painter.setPen( p.ink3 );
                painter.drawText( QRect( hPad + 8, topPad, width() - 2 * hPad, captionH ), Qt::AlignLeft | Qt::AlignVCenter,
                                  tr( "Steps" ).toUpper() );

                // Rows
                int y = topPad + captionH;
                for ( int i = 0; i < mSteps.size(); i++ )
                {
                        const auto& item = mSteps.at( i );
                        const bool current = ( item.state == designer::StepState::Current );
                        const bool done    = ( item.state == designer::StepState::Done );
                        QRect row( hPad, y, width() - 2 * hPad - 1, rowH );

                        if ( current )
                        {
                                QPainterPath bg;
                                bg.addRoundedRect( QRectF( row ).adjusted( 0, 2, 0, -2 ), 6, 6 );
                                painter.fillPath( bg, p.pressed );
                        }

                        QRectF dot( row.left() + 8, row.center().y() - circle / 2.0 + 0.5, circle, circle );
                        if ( done || current )
                        {
                                painter.setPen( Qt::NoPen );
                                painter.setBrush( p.accent );
                                painter.drawEllipse( dot );
                                if ( done )
                                {
                                        const int g = 12;
                                        QImage check = theme::ThemeIcons::render( QStringLiteral( "check" ), p.onGrad,
                                                                                  QSize( qRound( g * devicePixelRatioF() ), qRound( g * devicePixelRatioF() ) ) );
                                        check.setDevicePixelRatio( devicePixelRatioF() );
                                        painter.drawImage( QPointF( dot.center().x() - g / 2.0, dot.center().y() - g / 2.0 ), check );
                                }
                                else
                                {
                                        painter.setPen( p.onGrad );
                                        painter.setFont( th.uiFont( 11, 700 ) );
                                        painter.drawText( dot, Qt::AlignCenter, QString::number( i + 1 ) );
                                }
                        }
                        else
                        {
                                painter.setPen( QPen( p.line2, 1.5 ) );
                                painter.setBrush( p.panel );
                                painter.drawEllipse( dot.adjusted( 0.75, 0.75, -0.75, -0.75 ) );
                        }

                        painter.setFont( th.uiFont( 13, current ? 600 : 400 ) );
                        painter.setPen( current ? p.accent : ( done ? p.ink : p.ink2 ) );
                        painter.drawText( QRect( int( dot.right() ) + 10, row.top(), row.right() - int( dot.right() ) - 10, row.height() ),
                                          Qt::AlignLeft | Qt::AlignVCenter, designer::stepTitle( item.step ) );

                        y += rowH + rowGap;
                }

                // Illustration at the bottom, if there is room for it
                const int artW = qMin( artMaxW, width() - 2 * hPad - 8 );
                const QSize artBox( artW, artW * 160 / 200 );
                const int artTop = height() - artBox.height() - 16;
                if ( artTop > y + 8 )
                {
                        QPixmap art = theme::ThemeIcons::illustration( QStringLiteral( "designer-sidebar" ), artBox, devicePixelRatioF() );
                        painter.drawPixmap( QPoint( ( width() - artBox.width() ) / 2, artTop ), art );
                }
        }

}
