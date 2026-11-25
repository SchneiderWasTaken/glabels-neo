//  RibbonTab.hpp
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

#ifndef ribbon_RibbonTab_hpp
#define ribbon_RibbonTab_hpp


#include "RibbonTypes.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QList>
#include <QString>
#include <QWidget>


namespace glabels::ribbon
{

        class RibbonGroup;

        ///
        /// A ribbon tab body containing a left-aligned horizontal row of RibbonGroups.
        /// Automatically collapses groups into overflow buttons on narrow windows
        /// (highest collapsePriority first, then right-to-left).
        ///
        class RibbonTab : public QWidget
        {
                Q_OBJECT

        public:
                explicit RibbonTab( TabId          id,
                                    const QString& title,
                                    bool           contextual = false,
                                    QWidget*       parent     = nullptr );

                TabId   id() const;
                QString title() const;
                bool    isContextual() const;

                RibbonGroup* addGroup( const QString& title,
                                       const QString& overflowIconName = QString(),
                                       int            collapsePriority = 0 );

                const QList<RibbonGroup*>& groups() const;

                /// Recompute right-hand separators and narrow-window overflow.
                void updateLayoutState();

                /// Collect all QActions bound to RibbonButtons across all groups in this tab.
                QList<QAction*> boundActions() const;

        protected:
                void resizeEvent( QResizeEvent* event ) override;
                void showEvent( QShowEvent* event ) override;
                void paintEvent( QPaintEvent* event ) override;

        private slots:
                void onThemeChanged();

        private:
                TabId               mId;
                QString             mTitle;
                bool                mContextual{ false };
                QHBoxLayout*        mLayout{ nullptr };
                QList<RibbonGroup*> mGroups;
        };

}


#endif // ribbon_RibbonTab_hpp
