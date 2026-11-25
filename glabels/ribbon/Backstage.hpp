//  Backstage.hpp
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

#ifndef ribbon_Backstage_hpp
#define ribbon_Backstage_hpp


#include <QAction>
#include <QList>
#include <QWidget>


namespace glabels::ribbon
{

        ///
        /// Full-window File backstage overlay (SPEC 6.3, mockup s02).
        /// Replaces the old File menu; all commands bind directly to MainWindow's QActions.
        ///
        class Backstage : public QWidget
        {
                Q_OBJECT

        public:
                struct Actions
                {
                        QAction*        fileNew{ nullptr };
                        QAction*        fileOpen{ nullptr };
                        QList<QAction*> fileRecent;
                        QAction*        fileSave{ nullptr };
                        QAction*        fileSaveAs{ nullptr };
                        QAction*        fileTemplateDesigner{ nullptr };
                        QAction*        editPreferences{ nullptr };
                        QAction*        fileClose{ nullptr };
                        QAction*        fileExit{ nullptr };
                };

        public:
                explicit Backstage( const Actions& actions, QWidget* parent = nullptr );

                /// Refresh recent files list and button enabled states.
                void refresh();

                /// All QActions bound in the backstage (for command-map coverage tests).
                QList<QAction*> boundActions() const;

        signals:
                void closeRequested();

        protected:
                void keyPressEvent( QKeyEvent* event ) override;
                void showEvent( QShowEvent* event ) override;
                void paintEvent( QPaintEvent* event ) override;

        private slots:
                void onThemeChanged();

        private:
                void rebuildRecentList();

                Actions         mActions;
                QWidget*        mNavPanel{ nullptr };
                QWidget*        mRecentCard{ nullptr };
                QList<QAction*> mBoundActions;
        };

}


#endif // ribbon_Backstage_hpp
