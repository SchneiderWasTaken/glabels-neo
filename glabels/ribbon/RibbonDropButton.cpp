//  RibbonDropButton.cpp
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


#include "RibbonDropButton.hpp"

#include <QApplication>
#include <QScreen>


namespace glabels::ribbon
{

        RibbonDropButton::RibbonDropButton( const QString& label,
                                            const QString& iconName,
                                            Size           size,
                                            QWidget*       parent )
                : RibbonButton( size, Variant::Standard, parent )
        {
                setText( label );
                setAccessibleName( label );
                setIconName( iconName );

                connect( this, &QAbstractButton::clicked, this, &RibbonDropButton::openPopup );
        }


        void RibbonDropButton::setMenu( QMenu* menu )
        {
                mMenu = menu;
        }


        QMenu* RibbonDropButton::menu() const
        {
                return mMenu;
        }


        void RibbonDropButton::setPopupWidget( QWidget* popup )
        {
                mPopup = popup;
        }


        QWidget* RibbonDropButton::popupWidget() const
        {
                return mPopup;
        }


        bool RibbonDropButton::hasChevron() const
        {
                return true;
        }


        void RibbonDropButton::openPopup()
        {
                emit aboutToShowPopup();

                QPoint pos = mapToGlobal( QPoint( 0, height() + 2 ) );

                if ( mMenu )
                {
                        mMenu->popup( pos );
                        return;
                }

                if ( mPopup )
                {
                        mPopup->adjustSize();
                        if ( QScreen* screen = QApplication::screenAt( pos ) )
                        {
                                const QRect avail = screen->availableGeometry();
                                if ( pos.x() + mPopup->width() > avail.right() )
                                {
                                        pos.setX( qMax( avail.left(), avail.right() - mPopup->width() ) );
                                }
                                if ( pos.y() + mPopup->height() > avail.bottom() )
                                {
                                        pos.setY( mapToGlobal( QPoint( 0, 0 ) ).y() - mPopup->height() - 2 );
                                }
                        }
                        mPopup->move( pos );
                        mPopup->show();
                        mPopup->raise();
                        mPopup->activateWindow();
                }
        }

} // namespace glabels::ribbon
