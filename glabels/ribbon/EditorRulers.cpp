//  EditorRulers.cpp
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


#include "EditorRulers.hpp"

#include "LabelEditor.hpp"
#include "model/Constants.hpp"
#include "model/Settings.hpp"
#include "theme/Theme.hpp"

#include <QEvent>
#include <QGridLayout>
#include <QPainter>
#include <QScrollBar>
#include <QtMath>


namespace glabels::ribbon
{

        class EditorRulerFrame::CornerBox : public QWidget
        {
        public:
                explicit CornerBox( QWidget* parent = nullptr ) : QWidget( parent )
                {
                        setFixedSize( theme::Metrics::ruler, theme::Metrics::ruler );
                }

        protected:
                void paintEvent( QPaintEvent* ) override
                {
                        QPainter p( this );
                        const auto& pal = theme::Theme::instance().palette();
                        p.fillRect( rect(), pal.ribbon );
                        p.setPen( pal.line );
                        p.drawLine( width() - 1, 0, width() - 1, height() );
                        p.drawLine( 0, height() - 1, width(), height() - 1 );
                }
        };


        class EditorRulerFrame::RulerStrip : public QWidget
        {
        public:
                RulerStrip( Qt::Orientation orientation,
                            QScrollArea*    scrollArea,
                            LabelEditor*    editor,
                            QWidget*        parent = nullptr )
                        : QWidget( parent )
                        , mOrientation( orientation )
                        , mScrollArea( scrollArea )
                        , mEditor( editor )
                {
                        if ( mOrientation == Qt::Horizontal )
                        {
                                setFixedHeight( theme::Metrics::ruler );
                        }
                        else
                        {
                                setFixedWidth( theme::Metrics::ruler );
                        }
                }

                void setModel( model::Model* model )
                {
                        mModel = model;
                        update();
                }

        protected:
                void paintEvent( QPaintEvent* ) override
                {
                        QPainter p( this );
                        const auto& th  = theme::Theme::instance();
                        const auto& pal = th.palette();

                        p.fillRect( rect(), pal.ribbon );
                        p.setPen( pal.line );
                        if ( mOrientation == Qt::Horizontal )
                        {
                                p.drawLine( 0, height() - 1, width(), height() - 1 );
                        }
                        else
                        {
                                p.drawLine( width() - 1, 0, width() - 1, height() );
                        }

                        if ( !mModel || !mEditor || !mScrollArea )
                        {
                                return;
                        }

                        const double scale = mEditor->zoom() * mEditor->physicalDpiX() / model::PTS_PER_INCH;
                        if ( scale <= 0.0 )
                        {
                                return;
                        }

                        const model::Units units = model::Settings::units();
                        const double ptsPerUnit = model::Distance( 1.0, units ).pt();
                        const double pxPerUnit  = ptsPerUnit * scale;
                        if ( pxPerUnit <= 0.0 )
                        {
                                return;
                        }

                        // Choose minor/major tick spacing in user units so ticks stay legible.
                        double minorStep = 1.0;
                        int    majorEvery = 10;
                        if ( units.toEnum() == model::Units::IN )
                        {
                                minorStep  = 0.125;
                                majorEvery = 8;
                        }
                        else if ( units.toEnum() == model::Units::CM )
                        {
                                minorStep  = 0.1;
                                majorEvery = 10;
                        }
                        else if ( units.toEnum() == model::Units::PT )
                        {
                                minorStep  = 10.0;
                                majorEvery = 5;
                        }

                        while ( minorStep * pxPerUnit < 4.0 )
                        {
                                minorStep *= 2.0;
                        }

                        const double x0Px = ( mEditor->width()  - mModel->w().pt() * scale ) / 2.0
                                            - mScrollArea->horizontalScrollBar()->value();
                        const double y0Px = ( mEditor->height() - mModel->h().pt() * scale ) / 2.0
                                            - mScrollArea->verticalScrollBar()->value();

                        const double originPx = ( mOrientation == Qt::Horizontal ) ? x0Px : y0Px;
                        const int    spanPx   = ( mOrientation == Qt::Horizontal ) ? width() : height();

                        const double startUnits = -originPx / pxPerUnit;
                        const double endUnits   = ( spanPx - originPx ) / pxPerUnit;
                        const int    firstTick  = qFloor( startUnits / minorStep );
                        const int    lastTick   = qCeil( endUnits / minorStep );

                        p.setFont( th.uiFont( 10, 400 ) );
                        p.setPen( pal.ink3 );

                        for ( int t = firstTick; t <= lastTick; ++t )
                        {
                                const double u   = t * minorStep;
                                const int    pos = qRound( originPx + u * pxPerUnit );
                                if ( pos < 0 || pos > spanPx )
                                {
                                        continue;
                                }

                                const bool isMajor = ( ( t % majorEvery ) == 0 );
                                const int tickLen  = isMajor ? 11 : 5;

                                if ( mOrientation == Qt::Horizontal )
                                {
                                        p.drawLine( pos, height() - tickLen, pos, height() - 1 );
                                        if ( isMajor )
                                        {
                                                const QString label = QString::number( u, 'g', 4 );
                                                p.drawText( pos + 3, 10, label );
                                        }
                                }
                                else
                                {
                                        p.drawLine( width() - tickLen, pos, width() - 1, pos );
                                        if ( isMajor )
                                        {
                                                const QString label = QString::number( u, 'g', 4 );
                                                p.drawText( 2, pos + 10, label );
                                        }
                                }
                        }
                }

        private:
                Qt::Orientation mOrientation;
                QScrollArea*    mScrollArea{ nullptr };
                LabelEditor*    mEditor{ nullptr };
                model::Model*   mModel{ nullptr };
        };


        EditorRulerFrame::EditorRulerFrame( QScrollArea* scrollArea,
                                            LabelEditor* editor,
                                            QWidget*     parent )
                : QWidget( parent )
                , mScrollArea( scrollArea )
                , mEditor( editor )
        {
                auto* grid = new QGridLayout( this );
                grid->setContentsMargins( 0, 0, 0, 0 );
                grid->setSpacing( 0 );

                mCorner          = new CornerBox( this );
                mHorizontalRuler = new RulerStrip( Qt::Horizontal, mScrollArea, mEditor, this );
                mVerticalRuler   = new RulerStrip( Qt::Vertical, mScrollArea, mEditor, this );

                grid->addWidget( mCorner,          0, 0 );
                grid->addWidget( mHorizontalRuler, 0, 1 );
                grid->addWidget( mVerticalRuler,   1, 0 );
                grid->addWidget( mScrollArea,      1, 1 );
                grid->setRowStretch( 1, 1 );
                grid->setColumnStretch( 1, 1 );

                mEditor->installEventFilter( this );
                connect( mEditor, &LabelEditor::zoomChanged, this, &EditorRulerFrame::updateRulers );
                connect( mScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
                         this, &EditorRulerFrame::updateRulers );
                connect( mScrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
                         this, &EditorRulerFrame::updateRulers );
                connect( model::Settings::instance(), &model::Settings::changed,
                         this, &EditorRulerFrame::updateRulers );
                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &EditorRulerFrame::updateRulers );
        }


        void EditorRulerFrame::setModel( model::Model* model )
        {
                if ( mModel )
                {
                        disconnect( mModel, nullptr, this, nullptr );
                }
                mModel = model;
                mHorizontalRuler->setModel( mModel );
                mVerticalRuler->setModel( mModel );
                if ( mModel )
                {
                        connect( mModel, &model::Model::sizeChanged,
                                 this, &EditorRulerFrame::updateRulers );
                }
                updateRulers();
        }


        void EditorRulerFrame::updateRulers()
        {
                mCorner->update();
                mHorizontalRuler->update();
                mVerticalRuler->update();
        }


        bool EditorRulerFrame::eventFilter( QObject* watched, QEvent* event )
        {
                if ( watched == mEditor && event->type() == QEvent::Resize )
                {
                        updateRulers();
                }
                return QWidget::eventFilter( watched, event );
        }

} // namespace glabels::ribbon
