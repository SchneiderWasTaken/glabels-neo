//  UpdateStore.hpp
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

#ifndef update_UpdateStore_hpp
#define update_UpdateStore_hpp


#include <QDateTime>
#include <QString>

#include <optional>


namespace glabels::update
{

        ///
        /// Where the updater keeps its files, and the small record of a downloaded-and-staged update.
        ///
        namespace UpdateStore
        {
                /// <per-user app data>/updates
                QString dir();

                QString downloadDir();      ///< partial and complete package downloads
                QString runDir();           ///< temporary copy of the helper while it runs
                QString pendingFile();      ///< record of a staged update
                QString resultFile();       ///< written by the helper: how the last swap went
                QString lockDir();          ///< held by the helper while it swaps
                QString logFile();

                /// Append a line to the update log (kept below ~200 KB).
                void log( const QString& message );
        }


        ///
        /// An update that has been downloaded, verified and unpacked, and waits to be swapped in.
        ///
        struct PendingUpdate
        {
                QString   id;           ///< UpdateManifest::id() of the release
                QString   version;      ///< what to show the user
                QString   channel;
                QString   sha256;       ///< digest of the package that was staged
                qint64    commitTime{0};  ///< committer time of an edge release (0 for stable)
                QDateTime stagedAt;
                int       attempts{0};  ///< times a swap has been started for it

                static std::optional<PendingUpdate> load();
                bool save() const;
                static void clear();
        };


        ///
        /// User preferences and bookkeeping of the updater (stored with the application's other settings)
        ///
        namespace UpdateSettings
        {
                /// Look for updates in the background.  Off until the user opts in (see consentAsked());
                /// "Check for Updates..." in the Help menu works regardless, because the user asked for it.
                bool autoCheck();
                void setAutoCheck( bool enabled );      ///< also records that the user has answered

                /// True once the user has been asked, or has set the preference themselves.
                bool consentAsked();
                void setConsentAsked( bool asked );

                /// Download and stage without asking first.
                bool autoInstall();
                void setAutoInstall( bool enabled );

                QDateTime lastCheck();
                void setLastCheck( const QDateTime& when );

                /// Release the user chose to ignore ("Skip this version").
                QString skippedId();
                void setSkippedId( const QString& id );

                /// Release whose installation failed here; not retried automatically.
                QString failedId();
                void setFailedId( const QString& id );

                /// "Not now": do not ask about `id` again before `until`.
                void snooze( const QString& id, const QDateTime& until );
                bool isSnoozed( const QString& id );
        }

}


#endif // update_UpdateStore_hpp
