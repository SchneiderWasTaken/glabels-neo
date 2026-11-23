//  UpdateService.hpp
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

#ifndef update_UpdateService_hpp
#define update_UpdateService_hpp


#include "InstallLayout.hpp"
#include "UpdateManifest.hpp"
#include "UpdateStore.hpp"

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <optional>


class QCryptographicHash;
class QFile;
class QNetworkReply;
class QLockFile;


namespace glabels::update
{

        ///
        /// Everything the service needs to know about its surroundings (injectable for tests).
        ///
        struct UpdateServiceConfig
        {
                BuildIdentity  build;
                InstallLayout  layout;
                QString        platformKey;
                QString        repo;
                QUrl           manifestUrl;
                bool           strictUrls{true};   ///< only tests relax this (local test server)

                /// The configuration of the running program: compiled-in channel and repository,
                /// layout derived from where the executable lives.
                static UpdateServiceConfig forThisBuild();

                /// Fixed, public location of the manifest of a channel.
                static QUrl manifestUrlFor( const QString& repo, const QString& channel );
        };


        ///
        /// Keeps the installation up to date, in the background.
        ///
        /// Life of an update:
        ///
        ///   check      GET the update manifest (a few hundred bytes) twice a day or so;
        ///   offer      tell the UI a newer release exists (or, if the user opted in, just go on);
        ///   download   fetch the package for this platform, verifying size and SHA-256;
        ///   stage      unpack it into a hidden sibling of the install directory and smoke-test it
        ///              (run it with --Version) -- all while the program is running and in use;
        ///   apply      when the program exits (or, as a fallback, the next time it starts), the
        ///              glabels-updater helper swaps the staged copy into place with directory renames.
        ///
        /// So the next launch is on the new version, and a failure at any point leaves the working
        /// install untouched.
        ///
        class UpdateService : public QObject
        {
                Q_OBJECT

        public:
                enum class State { Disabled, Idle, Checking, Available, Downloading, Installing, Ready };
                Q_ENUM( State )

                enum class CheckResult { UpToDate, UpdateAvailable, UpdateReady, Failed, Unsupported };
                Q_ENUM( CheckResult )


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                explicit UpdateService( const UpdateServiceConfig& config, QObject* parent = nullptr );
                ~UpdateService() override;

                /// Process-wide instance for the running program (created on first use).
                static UpdateService* instance();


                /////////////////////////////////
                // Properties
                /////////////////////////////////
        public:
                State state() const { return mState; }

                /// False if this build/installation cannot update itself; see unsupportedReason().
                bool isSupported() const { return mSupported; }
                QString unsupportedReason() const { return mUnsupportedReason; }

                const BuildIdentity& build() const { return mConfig.build; }
                const InstallLayout& layout() const { return mConfig.layout; }

                /// The release found by the last check that is newer than this build, if any.
                const std::optional<UpdateManifest>& availableUpdate() const { return mAvailable; }

                /// The update that is staged and waiting for the next start, if any.
                std::optional<PendingUpdate> pendingUpdate() const;

                /// Version the program was updated to since the last start, for a one-time notice.
                QString justUpdatedTo() const { return mJustUpdatedTo; }


                /////////////////////////////////
                // Actions
                /////////////////////////////////
        public slots:
                /// Begin the periodic background checks (first one shortly after start).
                void start();

                /// Ask GitHub now.  `userInitiated` checks report every outcome; automatic ones stay quiet
                /// about anything the user already declined.
                void checkNow( bool userInitiated = false );

                /// Download, verify and stage the available update.
                void install();

                /// Do not offer the available release again (until a newer one appears).
                void skipAvailable();

                /// Do not ask about the available release for a few days.
                void snoozeAvailable();


                /////////////////////////////////
                // Applying a staged update
                /////////////////////////////////
        public:
                /// Call first thing at program start.  Notices an update that was applied since the last
                /// run, and if one is staged but not yet applied hands over to the helper, which swaps it
                /// in and starts the program again with `arguments`.  Returns true if the caller must
                /// exit immediately (the helper is taking over).
                bool applyPendingAtStartup( const QStringList& arguments );

                /// Call when the program is about to exit: lets the helper swap a staged update in
                /// as soon as this process is gone.
                void applyPendingAtExit();


                /////////////////////////////////
                // Signals
                /////////////////////////////////
        signals:
                void stateChanged( State state );
                void checkFinished( CheckResult result, bool userInitiated, const QString& message );
                void updateAvailable( const UpdateManifest& manifest, bool userInitiated );
                void downloadProgress( qint64 received, qint64 total );
                void updateReady( const QString& version );
                void installFailed( const QString& message );


                /////////////////////////////////
                // Implementation
                /////////////////////////////////
        private slots:
                void onTimer();

        private:
                void setState( State state );
                void onManifestReply( QNetworkReply* reply, bool userInitiated );
                void startDownload();
                void onDownloadFinished();
                void startExtract( const QString& packagePath );
                void startSmokeTest();
                void finishReady();
                void fail( const QString& message, bool permanent );
                bool launchHelper( bool relaunch, const QStringList& relaunchArguments );
                void cleanupLeftovers();
                void discardStaged();
                bool stagedCopiesExist() const;
                int  checkIntervalSecs() const;

                UpdateServiceConfig             mConfig;
                bool                            mSupported{false};
                QString                         mUnsupportedReason;
                State                           mState{State::Disabled};
                QNetworkAccessManager           mNetwork;
                QTimer                          mTimer;
                std::optional<UpdateManifest>   mAvailable;
                QString                         mJustUpdatedTo;

                // Download/stage in progress
                std::unique_ptr<QLockFile>      mLock;
                QPointer<QNetworkReply>         mDownload;
                std::unique_ptr<QFile>             mDownloadFile;
                std::unique_ptr<QCryptographicHash> mDownloadHash;
                QString                         mDownloadPath;
        };

}


#endif // update_UpdateService_hpp
