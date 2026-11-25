//  RibbonStatusBar.cpp
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


#include "RibbonStatusBar.hpp"

#include "RibbonButton.hpp"
#include "theme/Theme.hpp"

#include <QHBoxLayout>
#include <QPainter>
#include <QtMath>


namespace glabels::ribbon
{

        namespace
        {
                // Matches LabelEditor::zoomLevels reversed (min -> max)
                constexpr int    kNumZoomLevels = 11;
                constexpr double kZoomLevels[kNumZoomLevels] = {
                        0.33, 0.50, 0.67, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0
                };
        }


        RibbonStatusBar::RibbonStatusBar( QAction* zoomOutAction,
                                          QAction* zoomInAction,
                                          QWidget* parent )
                : QWidget( parent )
        {
                setFixedHeight( theme::Metrics::status );
                setProperty( "role", QStringLiteral( "ribbonStatusBar" ) );

                auto* layout = new QHBoxLayout( this );
                layout->setContentsMargins( 14, 0, 14, 0 );
                layout->setSpacing( 16 );

                mLeftLabel = new QLabel( tr( "Welcome" ), this );
                mLeftLabel->setProperty( "role", QStringLiteral( "statusText" ) );
                layout->addWidget( mLeftLabel, 1 );

                mSelectionLabel = new QLabel( tr( "Nothing selected" ), this );
                mSelectionLabel->setProperty( "role", QStringLiteral( "statusText" ) );
                layout->addWidget( mSelectionLabel, 0 );

                mCursorLabel = new QLabel( this );
                mCursorLabel->setProperty( "role", QStringLiteral( "statusText" ) );
                mCursorLabel->setMinimumWidth( 110 );
                mCursorLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
                layout->addWidget( mCursorLabel, 0 );

                mZoomContainer = new QWidget( this );
                auto* zl = new QHBoxLayout( mZoomContainer );
                zl->setContentsMargins( 0, 0, 0, 0 );
                zl->setSpacing( 6 );

                mZoomOutBtn = new RibbonButton( zoomOutAction,
                                                QStringLiteral( "zoom-out" ),
                                                RibbonButton::Size::IconOnly,
                                                RibbonButton::Variant::Standard,
                                                mZoomContainer );
                mZoomOutBtn->setFixedSize( 22, 22 );
                mZoomOutBtn->setFocusPolicy( Qt::NoFocus );
                zl->addWidget( mZoomOutBtn );

                mZoomSlider = new QSlider( Qt::Horizontal, mZoomContainer );
                mZoomSlider->setRange( 0, kNumZoomLevels - 1 );
                mZoomSlider->setValue( 4 ); // 100%
                mZoomSlider->setFixedWidth( 120 );
                mZoomSlider->setFocusPolicy( Qt::ClickFocus );
                mZoomSlider->setAccessibleName( tr( "Zoom" ) );
                mZoomSlider->setProperty( "role", QStringLiteral( "zoomSlider" ) );
                connect( mZoomSlider, &QSlider::valueChanged,
                         this, &RibbonStatusBar::onSliderValueChanged );
                zl->addWidget( mZoomSlider );

                mZoomInBtn = new RibbonButton( zoomInAction,
                                               QStringLiteral( "zoom-in" ),
                                               RibbonButton::Size::IconOnly,
                                               RibbonButton::Variant::Standard,
                                               mZoomContainer );
                mZoomInBtn->setFixedSize( 22, 22 );
                mZoomInBtn->setFocusPolicy( Qt::NoFocus );
                zl->addWidget( mZoomInBtn );

                mZoomPercentLabel = new QLabel( QStringLiteral( "100%" ), mZoomContainer );
                mZoomPercentLabel->setFixedWidth( 44 );
                mZoomPercentLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
                mZoomPercentLabel->setProperty( "role", QStringLiteral( "statusText" ) );
                zl->addWidget( mZoomPercentLabel );

                layout->addWidget( mZoomContainer, 0 );

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &RibbonStatusBar::onThemeChanged );
                onThemeChanged();
        }


        void RibbonStatusBar::setLabelSummary( const QString& summary )
        {
                mLabelSummary = summary;
                updateLeftText();
        }


        void RibbonStatusBar::setTemporaryMessage( const QString& message )
        {
                mTemporaryMessage = message;
                updateLeftText();
        }


        void RibbonStatusBar::setSelectionCount( int count )
        {
                if ( count <= 0 )
                {
                        mSelectionLabel->setText( tr( "Nothing selected" ) );
                }
                else if ( count == 1 )
                {
                        mSelectionLabel->setText( tr( "1 object selected" ) );
                }
                else
                {
                        mSelectionLabel->setText( tr( "%1 objects selected" ).arg( count ) );
                }
        }


        void RibbonStatusBar::setCursorPositionText( const QString& text )
        {
                mCursorLabel->setText( text );
        }


        void RibbonStatusBar::setZoomFactor( double zoom )
        {
                mZoomPercentLabel->setText( QStringLiteral( "%1%" ).arg( qRound( zoom * 100.0 ) ) );

                const int idx = zoomToSliderIndex( zoom );
                mUpdatingSlider = true;
                mLastSliderIndex = idx;
                mZoomSlider->setValue( idx );
                mUpdatingSlider = false;
        }


        void RibbonStatusBar::setEditorControlsVisible( bool visible )
        {
                mSelectionLabel->setVisible( visible );
                mCursorLabel->setVisible( visible );
                mZoomContainer->setVisible( visible );
        }


        int RibbonStatusBar::zoomToSliderIndex( double zoom )
        {
                int best = 0;
                double bestDist = qFabs( kZoomLevels[0] - zoom );
                for ( int i = 1; i < kNumZoomLevels; ++i )
                {
                        const double d = qFabs( kZoomLevels[i] - zoom );
                        if ( d < bestDist )
                        {
                                bestDist = d;
                                best = i;
                        }
                }
                return best;
        }


        void RibbonStatusBar::onSliderValueChanged( int value )
        {
                if ( mUpdatingSlider )
                {
                        return;
                }
                const int delta = value - mLastSliderIndex;
                mLastSliderIndex = value;
                if ( delta != 0 )
                {
                        emit zoomStepsRequested( delta );
                }
        }


        void RibbonStatusBar::updateLeftText()
        {
                if ( !mTemporaryMessage.isEmpty() )
                {
                        mLeftLabel->setText( mTemporaryMessage );
                }
                else if ( !mLabelSummary.isEmpty() )
                {
                        mLeftLabel->setText( mLabelSummary );
                }
                else
                {
                        mLeftLabel->setText( tr( "Welcome" ) );
                }
        }


        void RibbonStatusBar::onThemeChanged()
        {
                const auto& th  = theme::Theme::instance();
                const auto& pal = th.palette();
                const QFont f   = th.uiFont( 12, 400 );

                for ( QLabel* lbl : { mLeftLabel, mSelectionLabel, mCursorLabel, mZoomPercentLabel } )
                {
                        lbl->setFont( f );
                        QPalette p = lbl->palette();
                        p.setColor( QPalette::WindowText, pal.ink2 );
                        lbl->setPalette( p );
                }
                update();
        }


        void RibbonStatusBar::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                const auto& pal = theme::Theme::instance().palette();
                p.fillRect( rect(), pal.chrome );
                p.setPen( pal.line );
                p.drawLine( 0, 0, width(), 0 );
        }

} // namespace glabels::ribbon
