//  RibbonGroup.hpp
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

#ifndef ribbon_RibbonGroup_hpp
#define ribbon_RibbonGroup_hpp


#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QString>
#include <QWidget>


namespace glabels::ribbon
{

        class RibbonDropButton;

        ///
        /// A named group inside a RibbonTab.
        /// Contains a 90px body area + 22px centered caption, 8px horizontal padding,
        /// and a 1px right divider line (unless it is the last visible group).
        /// Supports collapsing into a single overflow drop-down button on narrow windows.
        ///
        class RibbonGroup : public QWidget
        {
                Q_OBJECT

        public:
                explicit RibbonGroup( const QString& title,
                                      const QString& overflowIconName = QString(),
                                      int            collapsePriority = 0,
                                      QWidget*       parent           = nullptr );

                QString      title() const;
                QHBoxLayout* bodyLayout() const;
                QWidget*     bodyWidget() const;

                void addWidget( QWidget* widget, Qt::Alignment alignment = Qt::AlignTop );
                void addLayout( QLayout* layout );

                int  collapsePriority() const;
                void setCollapsePriority( int priority );

                bool isCollapsed() const;
                void setCollapsed( bool collapsed );

                int  expandedWidthHint() const;

                void setShowSeparator( bool show );

                /// Collect all QActions bound to RibbonButtons inside this group.
                QList<QAction*> boundActions() const;

        protected:
                void paintEvent( QPaintEvent* event ) override;
                bool eventFilter( QObject* watched, QEvent* event ) override;

        private slots:
                void onThemeChanged();
                void onOverflowAboutToShow();

        private:
                void restoreBodyFromPopup();

                QString           mTitle;
                QString           mOverflowIconName;
                int               mCollapsePriority{ 0 };
                bool              mCollapsed{ false };
                bool              mShowSeparator{ true };

                QWidget*          mBodyContainer{ nullptr };
                QWidget*          mBody{ nullptr };
                QHBoxLayout*      mBodyLayout{ nullptr };
                QLabel*           mNameLabel{ nullptr };

                RibbonDropButton* mOverflowButton{ nullptr };
                QWidget*          mOverflowPopup{ nullptr };
        };

}


#endif // ribbon_RibbonGroup_hpp
