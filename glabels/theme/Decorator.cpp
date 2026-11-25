//  Decorator.cpp
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


#include "theme/Decorator.hpp"

#include "theme/Theme.hpp"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>


namespace
{
        /// Hooked widgets carry this property so they are hooked once
        const char* hookedProperty = "_glabelsThemeHooked";

        /// A button counts as a footer button when its bottom edge is this close to the bottom
        constexpr int footerReach = 48;

        /// Band above the top of the footer buttons
        constexpr int footerPad = 12;


        ///
        /// Watches the application for widgets being polished (once per widget and style change)
        ///
        class PolishWatcher : public QObject
        {
        public:
                using QObject::QObject;

        protected:
                bool eventFilter( QObject* watched, QEvent* event ) override
                {
                        if ( event->type() != QEvent::Polish )
                        {
                                return false;
                        }

                        auto* widget = qobject_cast<QWidget*>( watched );
                        if ( !widget || widget->property( hookedProperty ).toBool() )
                        {
                                return false;
                        }

                        if ( qobject_cast<QDialog*>( widget ) || qobject_cast<QGroupBox*>( widget ) )
                        {
                                widget->setProperty( hookedProperty, true );
                                widget->installEventFilter( glabels::theme::Decorator::instance() );
                        }
                        return false;
                }
        };


        ///
        /// Is the button inside a container that has its own surface (then it is not a footer)?
        ///
        bool insideContainer( const QWidget* widget, const QWidget* dialog )
        {
                for ( const QWidget* p = widget->parentWidget(); p && p != dialog; p = p->parentWidget() )
                {
                        if ( qobject_cast<const QGroupBox*>( p ) ||
                             qobject_cast<const QTabWidget*>( p ) ||
                             qobject_cast<const QAbstractScrollArea*>( p ) ||
                             qobject_cast<const QStackedWidget*>( p ) )
                        {
                                return true;
                        }
                }
                return false;
        }
}


namespace glabels::theme
{

        ///
        /// Constructor
        ///
        Decorator::Decorator()
        {
        }


        ///
        /// Singleton
        ///
        Decorator* Decorator::instance()
        {
                static Decorator decorator;
                return &decorator;
        }


        ///
        /// Hook the application
        ///
        void Decorator::install()
        {
                static PolishWatcher* watcher = nullptr;
                if ( !watcher && qApp )
                {
                        watcher = new PolishWatcher( qApp );
                        qApp->installEventFilter( watcher );
                }
        }


        ///
        /// Footer top
        ///
        int Decorator::footerTop( const QDialog* dialog )
        {
                if ( !dialog || dialog->property( "plainFooter" ).toBool() ||
                     dialog->windowType() == Qt::Popup )
                {
                        return -1;
                }

                const int height = dialog->height();
                int top = -1;

                const auto buttons = dialog->findChildren<QPushButton*>();
                for ( const QPushButton* button : buttons )
                {
                        if ( !button->isVisibleTo( dialog ) || insideContainer( button, dialog ) )
                        {
                                continue;
                        }

                        QPoint origin = button->mapTo( dialog, QPoint( 0, 0 ) );
                        int bottom = origin.y() + button->height();
                        if ( bottom < height - footerReach || bottom > height )
                        {
                                continue;
                        }

                        top = ( top < 0 ) ? origin.y() : qMin( top, origin.y() );
                }

                if ( top < 0 )
                {
                        return -1;
                }
                return qMax( 0, top - footerPad );
        }


        ///
        /// Event filter on hooked widgets: paint before the widget paints itself
        ///
        bool Decorator::eventFilter( QObject* watched, QEvent* event )
        {
                if ( event->type() == QEvent::Paint )
                {
                        if ( auto* box = qobject_cast<QGroupBox*>( watched ) )
                        {
                                paintCard( box );
                        }
                        else if ( auto* dialog = qobject_cast<QDialog*>( watched ) )
                        {
                                paintFooter( dialog );
                        }
                }
                return false;
        }


        ///
        /// Footer band
        ///
        void Decorator::paintFooter( QDialog* dialog )
        {
                int top = footerTop( dialog );
                if ( top < 0 )
                {
                        return;
                }

                const Palette& t = Theme::instance().palette();

                QPainter painter( dialog );
                painter.fillRect( QRect( 0, top, dialog->width(), dialog->height() - top ), t.chrome );
                painter.fillRect( QRect( 0, top, dialog->width(), 1 ), t.line );
        }


        ///
        /// Card
        ///
        void Decorator::paintCard( QGroupBox* box )
        {
                if ( box->isFlat() || box->property( "plainCard" ).toBool() )
                {
                        return;
                }

                const Palette& t = Theme::instance().palette();

                QPainter painter( box );
                painter.setRenderHint( QPainter::Antialiasing );

                QRectF outer = QRectF( box->rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );
                QPainterPath shape;
                shape.addRoundedRect( outer, 10, 10 );

                painter.fillPath( shape, t.panel );

                if ( !box->title().isEmpty() )
                {
                        painter.save();
                        painter.setClipPath( shape );
                        painter.fillRect( QRectF( 0, 0, box->width(), cardHeader ), t.chrome );
                        painter.fillRect( QRectF( 0, cardHeader, box->width(), 1 ), t.line );
                        painter.restore();
                }

                QPen pen( t.line );
                pen.setWidthF( 1.0 );
                painter.setPen( pen );
                painter.setBrush( Qt::NoBrush );
                painter.drawPath( shape );
        }

}
