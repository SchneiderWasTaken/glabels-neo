//  StartupView.hpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
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

#ifndef StartupView_hpp
#define StartupView_hpp


#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>


namespace glabels
{

        // Forward references
        class MainWindow;


        ///
        /// Startup / Welcome View Widget (SPEC 6.3, mockup s01)
        ///
        class StartupView : public QWidget
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                explicit StartupView( MainWindow* window );


                /////////////////////////////////
                // Events
                /////////////////////////////////
        protected:
                void paintEvent( QPaintEvent* event ) override;


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        private slots:
                void onNewProjectButtonClicked();
                void onOpenProjectButtonClicked();
                void onSettingsChanged();
                void onThemeChanged();


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                void loadRecentsList();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                MainWindow*  mWindow{ nullptr };
                QLabel*      mMarkLabel{ nullptr };
                QLabel*      mHeadingLabel{ nullptr };
                QLabel*      mAttributionLabel{ nullptr };
                QWidget*     mRecentCard{ nullptr };
                QVBoxLayout* mRecentRowsLayout{ nullptr };
        };

}


#endif // StartupView_hpp
