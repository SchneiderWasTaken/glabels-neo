//  UpdateUi.hpp
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

#ifndef UpdateUi_hpp
#define UpdateUi_hpp


#include "update/UpdateService.hpp"

#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QWidget>


namespace glabels
{

        ///
        /// The user-facing side of automatic updates: the "an update is available" offer, status
        /// messages, and the Help menu entry.  All the work is done by update::UpdateService.
        ///
        class UpdateUi : public QObject
        {
                Q_OBJECT

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        private:
                UpdateUi();

        public:
                /// Create the (single) instance and connect it to the update service.
                static void init();
                static UpdateUi* instance();

                /// Add "Check for Updates..." to a window's Help menu.
                static void addToHelpMenu( QMenu* helpMenu );


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        public slots:
                /// Help -> Check for Updates...
                void checkForUpdatesRequested();

        private slots:
                void onUpdateAvailable( const update::UpdateManifest& manifest, bool userInitiated );
                void onCheckFinished( update::UpdateService::CheckResult result, bool userInitiated, const QString& message );
                void onUpdateReady( const QString& version );
                void onInstallFailed( const QString& message );
                void onDownloadProgress( qint64 received, qint64 total );


                /////////////////////////////////
                // Private
                /////////////////////////////////
        private:
                QWidget* parentWindow() const;
                void showMessage( const QString& text, int timeoutMs = 10000 ) const;
                void showOffer( const update::UpdateManifest& manifest );

                QPointer<QMessageBox> mOffer;
                bool                  mUserWaitingForCheck{false};
                bool                  mUserRequestedInstall{false};
                bool                  mUserShowsProgress{false};
        };


        ///
        /// "Updates" page of the Preferences dialog
        ///
        class UpdatePreferencesPage : public QWidget
        {
                Q_OBJECT

        public:
                explicit UpdatePreferencesPage( QWidget* parent = nullptr );

        private slots:
                void onCheckForUpdatesToggled( bool checked );
                void onAutoInstallToggled( bool checked );

        private:
                QCheckBox*   mCheckBox{nullptr};
                QCheckBox*   mAutoInstallBox{nullptr};
        };

}


#endif // UpdateUi_hpp
