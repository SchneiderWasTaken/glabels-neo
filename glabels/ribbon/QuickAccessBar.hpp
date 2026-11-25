//  QuickAccessBar.hpp
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

#ifndef ribbon_QuickAccessBar_hpp
#define ribbon_QuickAccessBar_hpp


#include <QAction>
#include <QHBoxLayout>
#include <QList>
#include <QString>
#include <QWidget>


namespace glabels::ribbon
{

        class RibbonButton;

        ///
        /// Quick Access toolbar at the left of the ribbon tab strip:
        /// New, Open, Save, Undo, Redo. Hideable via View > Show > Quick Access toolbar.
        ///
        class QuickAccessBar : public QWidget
        {
                Q_OBJECT

        public:
                explicit QuickAccessBar( QWidget* parent = nullptr );

                RibbonButton* addAction( QAction* action, const QString& iconName );

                QList<QAction*> boundActions() const;

        protected:
                void paintEvent( QPaintEvent* event ) override;

        private slots:
                void onThemeChanged();

        private:
                QHBoxLayout*         mLayout{ nullptr };
                QList<RibbonButton*> mButtons;
        };

}


#endif // ribbon_QuickAccessBar_hpp
