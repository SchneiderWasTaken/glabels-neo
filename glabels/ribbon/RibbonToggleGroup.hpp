//  RibbonToggleGroup.hpp
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

#ifndef ribbon_RibbonToggleGroup_hpp
#define ribbon_RibbonToggleGroup_hpp


#include <QList>
#include <QString>
#include <QWidget>


namespace glabels::ribbon
{

        ///
        /// Segmented toggle button group for mutually exclusive options
        /// (e.g. Light / Dark / System, Flat / Vector).
        ///
        class RibbonToggleGroup : public QWidget
        {
                Q_OBJECT

        public:
                struct Option
                {
                        int     id{ 0 };
                        QString label;
                        QString iconName;
                        QString toolTip;
                };

        public:
                explicit RibbonToggleGroup( QWidget* parent = nullptr );

                void addOption( int            id,
                                const QString& label,
                                const QString& iconName = QString(),
                                const QString& toolTip  = QString() );

                int  currentId() const;
                void setCurrentId( int id );

                void setOptionEnabled( int id, bool enabled );

                QSize sizeHint() const override;
                QSize minimumSizeHint() const override;

        signals:
                void currentIdChanged( int id );

        protected:
                void paintEvent( QPaintEvent* event ) override;
                void mousePressEvent( QMouseEvent* event ) override;
                void mouseMoveEvent( QMouseEvent* event ) override;
                void leaveEvent( QEvent* event ) override;
                void keyPressEvent( QKeyEvent* event ) override;

        private slots:
                void onThemeChanged();

        private:
                struct Item
                {
                        Option option;
                        bool   enabled{ true };
                };

                int   indexAt( const QPoint& pos ) const;
                QRect segmentRect( int index ) const;
                int   indexForId( int id ) const;

                QList<Item> mItems;
                int         mCurrentIndex{ -1 };
                int         mHoveredIndex{ -1 };
        };

}


#endif // ribbon_RibbonToggleGroup_hpp
