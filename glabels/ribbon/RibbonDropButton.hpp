//  RibbonDropButton.hpp
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

#ifndef ribbon_RibbonDropButton_hpp
#define ribbon_RibbonDropButton_hpp


#include "RibbonButton.hpp"

#include <QMenu>
#include <QPointer>
#include <QWidget>


namespace glabels::ribbon
{

        ///
        /// Ribbon button with a chevron that opens a QMenu or custom popup QWidget.
        ///
        class RibbonDropButton : public RibbonButton
        {
                Q_OBJECT

        public:
                explicit RibbonDropButton( const QString& label,
                                           const QString& iconName,
                                           Size           size   = Size::Large,
                                           QWidget*       parent = nullptr );

                void   setMenu( QMenu* menu );
                QMenu* menu() const;

                void     setPopupWidget( QWidget* popup );
                QWidget* popupWidget() const;

        signals:
                void aboutToShowPopup();

        protected:
                bool hasChevron() const override;

        private slots:
                void openPopup();

        private:
                QPointer<QMenu>   mMenu;
                QPointer<QWidget> mPopup;
        };

}


#endif // ribbon_RibbonDropButton_hpp
