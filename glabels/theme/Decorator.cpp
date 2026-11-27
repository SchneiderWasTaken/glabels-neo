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

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QComboBox>
#include <QMenu>
#include <QStyledItemDelegate>
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
                        if ( !widget )
                        {
                                return false;
                        }

                        // Popups are fixed on every polish (style or stylesheet changes re-polish).
                        if ( auto* combo = qobject_cast<QComboBox*>( widget ) )
                        {
                                glabels::theme::Decorator::polishCombo( combo );
                        }
                        else if ( auto* view = qobject_cast<QAbstractItemView*>( widget ) )
                        {
                                glabels::theme::Decorator::polishPopupView( view );
                        }
                        if ( widget->isWindow() )
                        {
                                glabels::theme::Decorator::polishPopupWindow( widget );
                        }

                        if ( widget->property( hookedProperty ).toBool() )
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


        ///
        /// Lowest bottom edge (in dialog coordinates) of any visible widget that sits
        /// strictly above the footer button row.  Recurses only through ancestors of the
        /// footer buttons (e.g. QDialogButtonBox) so scrolled descendants inside content
        /// containers cannot skew the measurement.
        ///
        int maxContentBottom( const QWidget*                    parent,
                              const QDialog*                    dialog,
                              const QList<const QPushButton*>&  footerButtons,
                              int                               buttonTop )
        {
                int maxBottom = -1;

                for ( QObject* child : parent->children() )
                {
                        const auto* widget = qobject_cast<const QWidget*>( child );
                        if ( !widget || !widget->isVisibleTo( dialog ) )
                        {
                                continue;
                        }

                        bool isFooterBtn = false;
                        bool holdsFooterBtn = false;
                        for ( const QPushButton* btn : footerButtons )
                        {
                                if ( widget == btn )
                                {
                                        isFooterBtn = true;
                                        break;
                                }
                                if ( widget->isAncestorOf( btn ) )
                                {
                                        holdsFooterBtn = true;
                                }
                        }

                        if ( isFooterBtn )
                        {
                                continue;
                        }
                        if ( holdsFooterBtn )
                        {
                                maxBottom = qMax( maxBottom, maxContentBottom( widget, dialog, footerButtons, buttonTop ) );
                                continue;
                        }

                        const QPoint origin = widget->mapTo( dialog, QPoint( 0, 0 ) );
                        const int bottom = origin.y() + widget->height();
                        if ( origin.y() < buttonTop && bottom <= buttonTop )
                        {
                                maxBottom = qMax( maxBottom, bottom );
                        }
                }

                return maxBottom;
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
        /// Combo boxes (POPUP-AUDIT.md)
        ///
        void Decorator::polishCombo( QComboBox* combo )
        {
                // Lists of up to 15 items open without scrolling.  Only Qt's default is raised, so a
                // combo that chose its own limit keeps it.
                if ( combo->maxVisibleItems() == 10 )
                {
                        combo->setMaxVisibleItems( popupRows );
                }

                // Qt's own combo delegates are QItemDelegates: they ignore the stylesheet's item
                // rules, so the popup was sized for one row height and drawn with another.  A
                // QStyledItemDelegate sizes and paints rows from the same rules.  Custom delegates
                // (the font list, ...) are kept.
                const QString delegate = combo->itemDelegate() ? combo->itemDelegate()->metaObject()->className() : QString();
                if ( delegate == QLatin1String( "QComboBoxDelegate" ) || delegate == QLatin1String( "QComboMenuDelegate" ) )
                {
                        combo->setItemDelegate( new QStyledItemDelegate( combo ) );
                }

                polishPopupView( combo->view() );
        }


        ///
        /// List views that live in a popup window (combo and completer popups)
        ///
        void Decorator::polishPopupView( QAbstractItemView* view )
        {
                if ( !view )
                {
                        return;
                }
                QWidget* window = view->window();
                const bool inPopup = window && ( window->windowType() == Qt::Popup || window->windowType() == Qt::ToolTip );
                if ( inPopup && !view->property( "popupList" ).toBool() )
                {
                        // Selected by the stylesheet: square corners and no extra padding, because the
                        // popup window around the list is a rectangle (rounded corners leave pixels that
                        // nothing paints, which Windows shows black).
                        view->setProperty( "popupList", true );
                        view->style()->unpolish( view );
                        view->style()->polish( view );
                }
        }


        ///
        /// Top-level popup windows (custom popovers)
        ///
        void Decorator::polishPopupWindow( QWidget* window )
        {
                if ( window->windowType() != Qt::Popup ||
                     window->testAttribute( Qt::WA_TranslucentBackground ) ||
                     qobject_cast<QMenu*>( window ) )
                {
                        return;
                }

                // A frameless popup that paints a rounded panel itself leaves its corners unpainted.
                // Filling the window background first guarantees every pixel is painted.
                if ( !window->autoFillBackground() )
                {
                        window->setAutoFillBackground( true );
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
                int buttonTop = -1;
                QList<const QPushButton*> footerButtons;

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

                        footerButtons.append( button );
                        buttonTop = ( buttonTop < 0 ) ? origin.y() : qMin( buttonTop, origin.y() );
                }

                if ( buttonTop < 0 )
                {
                        return -1;
                }

                // Never paint the footer band behind content sitting above the buttons:
                // when the gap is smaller than 2 * footerPad, centre the divider in the gap.
                const int contentBottom = maxContentBottom( dialog, dialog, footerButtons, buttonTop );
                if ( contentBottom >= 0 )
                {
                        const int gap = buttonTop - contentBottom;
                        if ( gap < 2 * footerPad )
                        {
                                return qMax( 0, contentBottom + gap / 2 );
                        }
                }

                return qMax( 0, buttonTop - footerPad );
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
