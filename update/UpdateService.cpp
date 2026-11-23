//  UpdateService.cpp
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

#include "UpdateService.hpp"

#include "UpdateConfig.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QThreadPool>

#include <atomic>
#include <functional>


namespace glabels::update
{

        namespace
        {

                /// A manifest is about 1 KB; anything larger is not one.
                constexpr qint64 MAX_MANIFEST_BYTES = 64 * 1024;

                /// A stable release is offered to automatic checks only after it has been out this long (see
                /// onManifestReply), which leaves time to pull a bad release.  "Check for Updates" skips the wait.
                constexpr qint64 STABLE_SOAK_HOURS = 24;

                QString tr( const char* text )
                {
                        return QCoreApplication::translate( "UpdateService", text );
                }


                //
                // Running external commands one after the other, asynchronously.
                //
                struct Command
                {
                        QString             program;
                        QStringList         arguments;
                        int                 timeoutMs{10 * 60 * 1000};
                        QProcessEnvironment environment;
                };

                using Done = std::function<void( bool ok, const QString& error, const QString& output )>;

                struct Sequence
                {
                        QObject*        context{nullptr};
                        QList<Command>  steps;
                        QList<Command>  cleanup;     // always run, after the steps (e.g. unmount)
                        Done            done;
                        int             index{0};
                        bool            cleaning{false};
                        bool            failed{false};
                        QString         error;
                        QString         output;
                };


                // The bundled-library search paths of the running program must not leak into system tools.
                QProcessEnvironment cleanEnvironment()
                {
                        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                        for ( const char* name : { "LD_LIBRARY_PATH", "QT_PLUGIN_PATH", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH" } )
                        {
                                env.remove( name );
                        }
                        return env;
                }


                void runNext( const std::shared_ptr<Sequence>& s )
                {
                        QList<Command>& list = s->cleaning ? s->cleanup : s->steps;

                        if ( s->index >= list.size() || ( !s->cleaning && s->failed ) )
                        {
                                if ( !s->cleaning )
                                {
                                        s->cleaning = true;
                                        s->index    = 0;
                                        runNext( s );
                                        return;
                                }
                                s->done( !s->failed, s->error, s->output );
                                return;
                        }

                        const Command c = list[s->index++];
                        const bool cleaning = s->cleaning;

                        auto* p = new QProcess( s->context );
                        p->setProcessEnvironment( c.environment.isEmpty() ? cleanEnvironment() : c.environment );
                        p->setProcessChannelMode( QProcess::MergedChannels );

                        auto timedOut = std::make_shared<bool>( false );
                        auto handled  = std::make_shared<bool>( false );

                        auto conclude = [s, p, c, cleaning, timedOut, handled]( bool ok, const QString& why )
                        {
                                if ( *handled )
                                {
                                        return;
                                }
                                *handled = true;

                                QString out = QString::fromLocal8Bit( p->readAllStandardOutput() );
                                p->deleteLater();

                                if ( !cleaning )
                                {
                                        s->output = out;
                                        if ( !ok )
                                        {
                                                s->failed = true;
                                                s->error  = why.isEmpty() ? tr( QT_TRANSLATE_NOOP( "UpdateService", "%1 failed" ) ).arg( QFileInfo( c.program ).fileName() ) : why;
                                                if ( !out.trimmed().isEmpty() )
                                                {
                                                        s->error += ": " + out.trimmed().left( 400 );
                                                }
                                        }
                                }
                                runNext( s );
                        };

                        QObject::connect( p, &QProcess::finished, s->context,
                                          [p, conclude, timedOut]( int code, QProcess::ExitStatus status )
                                          {
                                                  if ( *timedOut )
                                                  {
                                                          conclude( false, tr( QT_TRANSLATE_NOOP( "UpdateService", "timed out" ) ) );
                                                  }
                                                  else
                                                  {
                                                          conclude( status == QProcess::NormalExit && code == 0,
                                                                    status == QProcess::NormalExit ? tr( QT_TRANSLATE_NOOP( "UpdateService", "exited with code %1" ) ).arg( code ) : tr( QT_TRANSLATE_NOOP( "UpdateService", "crashed" ) ) );
                                                  }
                                          } );

                        QObject::connect( p, &QProcess::errorOccurred, s->context,
                                          [p, conclude]( QProcess::ProcessError error )
                                          {
                                                  if ( error == QProcess::FailedToStart )
                                                  {
                                                          conclude( false, tr( QT_TRANSLATE_NOOP( "UpdateService", "could not be started" ) ) );
                                                  }
                                          } );

                        QTimer::singleShot( c.timeoutMs, p,
                                            [p, timedOut]()
                                            {
                                                    if ( p->state() != QProcess::NotRunning )
                                                    {
                                                            *timedOut = true;
                                                            p->kill();
                                                    }
                                            } );

                        p->start( c.program, c.arguments );
                }


                void runSequence( QObject* context, const QList<Command>& steps, const QList<Command>& cleanup, const Done& done )
                {
                        auto s = std::make_shared<Sequence>();
                        s->context = context;
                        s->steps   = steps;
                        s->cleanup = cleanup;
                        s->done    = done;
                        runNext( s );
                }


                /// Remove a directory tree without making the user interface wait for it.  An unpacked update has
                /// thousands of files, and deleting them (with a virus scanner looking at each) can take seconds.
                /// The tree is first renamed out of the way, which is instant on the same volume, so its old name
                /// is free again at once; the files are then deleted on a worker thread.
                void removeTreeAsync( const QString& path, bool alreadyAside = false )
                {
                        static std::atomic<int> counter{0};

                        QFileInfo info( path );
                        if ( !info.exists() && !info.isSymLink() )
                        {
                                return;
                        }
                        if ( info.isSymLink() || !info.isDir() )
                        {
                                QFile::remove( path );
                                return;
                        }

                        QString target = path;
                        if ( !alreadyAside )
                        {
                                const QString aside = info.absolutePath() + "/." + info.fileName() + ".trash-" +
                                                      QString::number( QCoreApplication::applicationPid() ) + "-" + QString::number( ++counter );
                                if ( QDir().rename( path, aside ) )
                                {
                                        target = aside;
                                }
                        }
                        QThreadPool::globalInstance()->start( [target]() { QDir( target ).removeRecursively(); } );
                }


                /// Leftovers of removeTreeAsync() from a run that ended before the deletion did.
                void removeTrashBesides( const QString& installedPath )
                {
                        const QFileInfo info( installedPath );
                        QDir parent( info.absolutePath() );
                        const QStringList stale = parent.entryList( { "." + info.fileName() + ".trash-*" },
                                                                    QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden );
                        for ( const QString& name : stale )
                        {
                                removeTreeAsync( parent.absoluteFilePath( name ), true );
                        }
                }


                void removeFilesIn( const QString& dirPath )
                {
                        QDir dir( dirPath );
                        for ( const QString& name : dir.entryList( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden ) )
                        {
                                QString path = dir.absoluteFilePath( name );
                                if ( QFileInfo( path ).isDir() && !QFileInfo( path ).isSymLink() )
                                {
                                        QDir( path ).removeRecursively();
                                }
                                else
                                {
                                        QFile::remove( path );
                                }
                        }
                }

        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // Configuration
        ///////////////////////////////////////////////////////////////////////////////////////

        QUrl UpdateServiceConfig::manifestUrlFor( const QString& repo, const QString& channel )
        {
                const QString file = "glabels-neo-update-manifest.json";

                if ( channel == "stable" )
                {
                        // GitHub redirects "latest" to the newest non-prerelease release.
                        return QUrl( QString( "https://github.com/%1/releases/latest/download/%2" ).arg( repo, file ) );
                }
                if ( channel == "edge" )
                {
                        return QUrl( QString( "https://github.com/%1/releases/download/edge/%2" ).arg( repo, file ) );
                }
                return QUrl();
        }


        UpdateServiceConfig UpdateServiceConfig::forThisBuild()
        {
                UpdateServiceConfig c;
                c.build.channel = Config::CHANNEL;
                c.build.tag     = Config::TAG;
                c.build.commit  = Config::COMMIT;
                c.build.commitTime = Config::COMMIT_TIME;
                c.repo          = Config::REPO;
                c.platformKey   = glabels::update::platformKey();
                c.manifestUrl   = manifestUrlFor( c.repo, c.build.channel );
                // No writes to the install location here: that waits until the user has asked for, or agreed to, checks
                // (see UpdateService::verifyWritable).
                c.layout        = InstallLayout::detect( QCoreApplication::applicationFilePath(), currentOs(), false );
                return c;
        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // Life cycle
        ///////////////////////////////////////////////////////////////////////////////////////

        UpdateService::UpdateService( const UpdateServiceConfig& config, QObject* parent )
                : QObject( parent ), mConfig( config )
        {
                if ( mConfig.build.channel.isEmpty() )
                {
                        mUnsupportedReason = tr( QT_TRANSLATE_NOOP( "UpdateService", "This build was not produced by the release pipeline, so it does not update itself." ) );
                }
                else if ( mConfig.platformKey.isEmpty() )
                {
                        mUnsupportedReason = tr( QT_TRANSLATE_NOOP( "UpdateService", "There is no update package for this platform." ) );
                }
                else if ( !mConfig.manifestUrl.isValid() )
                {
                        mUnsupportedReason = tr( QT_TRANSLATE_NOOP( "UpdateService", "No update source is configured." ) );
                }
                else if ( !mConfig.layout.supported )
                {
                        mUnsupportedReason = mConfig.layout.reason;
                }
                mSupported = mUnsupportedReason.isEmpty();
                mState     = mSupported ? State::Idle : State::Disabled;

                QNetworkProxyFactory::setUseSystemConfiguration( true );
                mNetwork.setRedirectPolicy( QNetworkRequest::UserVerifiedRedirectPolicy );   // see watchRedirects()

                mTimer.setInterval( 30 * 60 * 1000 );
                connect( &mTimer, &QTimer::timeout, this, &UpdateService::onTimer );
        }


        UpdateService::~UpdateService() = default;


        UpdateService* UpdateService::instance()
        {
                static UpdateService* sInstance = new UpdateService( UpdateServiceConfig::forThisBuild(), QCoreApplication::instance() );
                return sInstance;
        }


        void UpdateService::setState( State state )
        {
                if ( mState != state )
                {
                        mState = state;
                        emit stateChanged( state );
                }
        }


        std::optional<PendingUpdate> UpdateService::pendingUpdate() const
        {
                auto p = PendingUpdate::load();
                if ( p && stagedCopiesExist() )
                {
                        return p;
                }
                return std::nullopt;
        }


        bool UpdateService::stagedCopiesExist() const
        {
                if ( mConfig.layout.units.isEmpty() )
                {
                        return false;
                }
                for ( const auto& u : mConfig.layout.units )
                {
                        if ( !QFileInfo( u.staged ).isDir() )
                        {
                                return false;
                        }
                }
                return true;
        }


        int UpdateService::checkIntervalSecs() const
        {
                // Edge moves with every merge; stable releases are rare.
                return ( mConfig.build.channel == "edge" ? 6 : 12 ) * 3600;
        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // Scheduling
        ///////////////////////////////////////////////////////////////////////////////////////

        void UpdateService::start()
        {
                if ( !mSupported )
                {
                        return;
                }

                cleanupLeftovers();

                // The first check waits a little so that it never competes with start-up; after that, a
                // cheap timer decides whether the (persisted) interval since the last check has passed.
                QTimer::singleShot( 20 * 1000, this, &UpdateService::onTimer );
                mTimer.start();
        }


        void UpdateService::onTimer()
        {
                if ( !mSupported )
                {
                        return;
                }
                if ( !UpdateSettings::autoCheck() )
                {
                        // Never contact the network on the user's behalf before they have agreed to it.
                        // Ask once per session until they have answered.
                        if ( !UpdateSettings::consentAsked() && !mConsentRequested )
                        {
                                mConsentRequested = true;
                                emit consentRequired();
                        }
                        return;
                }
                if ( mState != State::Idle && mState != State::Available )
                {
                        return;
                }
                if ( pendingUpdate() )
                {
                        return;  // already waiting for a restart; nothing more to do
                }

                const QDateTime last = UpdateSettings::lastCheck();
                const QDateTime now  = QDateTime::currentDateTime();
                if ( last.isValid() && last <= now && last.secsTo( now ) < checkIntervalSecs() )
                {
                        return;
                }

                checkNow( false );
        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // Checking
        ///////////////////////////////////////////////////////////////////////////////////////

        bool UpdateService::verifyWritable()
        {
                if ( mWritableChecked )
                {
                        return mSupported;
                }
                mWritableChecked = true;

                const QString problem = mConfig.layout.writableProblem();
                if ( !problem.isEmpty() )
                {
                        mUnsupportedReason = problem;
                        mSupported         = false;
                        setState( State::Disabled );
                        UpdateStore::log( "not updating: " + problem );
                }
                return mSupported;
        }


        void UpdateService::watchRedirects( QNetworkReply* reply )
        {
                // Only follow redirects that stay on GitHub (github.com hands release downloads to its asset
                // hosts, *.githubusercontent.com).  The package is protected by its digest, but the manifest
                // that carries the digest is whatever its final redirect target says.
                connect( reply, &QNetworkReply::redirected, this,
                         [this, reply]( const QUrl& url )
                         {
                                 const QString host = url.host().toLower();
                                 const bool allowed = !mConfig.strictUrls ||
                                                      ( url.scheme() == "https" &&
                                                        ( host == "github.com" || host.endsWith( ".githubusercontent.com" ) ) );
                                 if ( allowed )
                                 {
                                         emit reply->redirectAllowed();
                                 }
                                 else
                                 {
                                         UpdateStore::log( "refusing redirect to " + url.host() );
                                         reply->abort();
                                 }
                         } );
        }


        void UpdateService::checkNow( bool userInitiated )
        {
                if ( !mSupported )
                {
                        emit checkFinished( CheckResult::Unsupported, userInitiated, mUnsupportedReason );
                        return;
                }
                if ( !userInitiated && !UpdateSettings::autoCheck() )
                {
                        return;  // background checks need the user's consent (see onTimer)
                }
                if ( mState != State::Idle && mState != State::Available )
                {
                        return;  // already busy: checking, downloading, or waiting for a restart
                }

                if ( !verifyWritable() )
                {
                        emit checkFinished( CheckResult::Unsupported, userInitiated, mUnsupportedReason );
                        return;
                }

                setState( State::Checking );

                QNetworkRequest request( mConfig.manifestUrl );
                request.setHeader( QNetworkRequest::UserAgentHeader, "glabels-neo-updater" );
                request.setAttribute( QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork );
                request.setTransferTimeout( 30 * 1000 );

                QNetworkReply* reply = mNetwork.get( request );
                watchRedirects( reply );

                // A manifest is about 1 KB: stop reading a response that is not one instead of buffering it.
                connect( reply, &QNetworkReply::downloadProgress, reply,
                         [reply]( qint64 received, qint64 )
                         {
                                 if ( received > MAX_MANIFEST_BYTES )
                                 {
                                         reply->abort();
                                 }
                         } );
                connect( reply, &QNetworkReply::finished, this, [this, reply, userInitiated]() { onManifestReply( reply, userInitiated ); } );
        }


        void UpdateService::onManifestReply( QNetworkReply* reply, bool userInitiated )
        {
                reply->deleteLater();
                setState( State::Idle );

                auto failed = [&]( const QString& why )
                {
                        UpdateStore::log( "check failed: " + why );
                        emit checkFinished( CheckResult::Failed, userInitiated, why );
                };

                const int status = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
                if ( reply->error() != QNetworkReply::NoError )
                {
                        failed( status == 404 ? tr( QT_TRANSLATE_NOOP( "UpdateService", "The latest release does not carry update information." ) )
                                              : reply->errorString() );
                        return;
                }

                QByteArray body = reply->read( MAX_MANIFEST_BYTES + 1 );
                if ( body.size() > MAX_MANIFEST_BYTES )
                {
                        failed( tr( QT_TRANSLATE_NOOP( "UpdateService", "The update information is too large." ) ) );
                        return;
                }

                QString parseError;
                auto manifest = UpdateManifest::parse( body, mConfig.repo, mConfig.build.channel, mConfig.strictUrls, &parseError );
                if ( !manifest )
                {
                        failed( tr( QT_TRANSLATE_NOOP( "UpdateService", "The update information is not valid: %1" ) ).arg( parseError ) );
                        return;
                }

                UpdateSettings::setLastCheck( QDateTime::currentDateTime() );

                if ( !isUpdateAvailable( mConfig.build, *manifest ) )
                {
                        mAvailable.reset();
                        UpdateStore::log( "check: up to date (" + mConfig.build.displayVersion() + ")" );
                        emit checkFinished( CheckResult::UpToDate, userInitiated, QString() );
                        return;
                }

                // Soak time: a stable release is not offered automatically until it has been out for a day, so that
                // pulling a bad release (deleting it, or its manifest) stops the rollout before it reaches anyone.
                // A check the user asked for is not held back.
                if ( !userInitiated && manifest->channel == "stable" && manifest->published.isValid() &&
                     manifest->published.secsTo( QDateTime::currentDateTimeUtc() ) < STABLE_SOAK_HOURS * 3600 )
                {
                        mAvailable.reset();
                        UpdateStore::log( "check: " + manifest->displayVersion() + " is newer but still in its soak period" );
                        emit checkFinished( CheckResult::UpToDate, userInitiated, QString() );
                        return;
                }

                const QString id = manifest->id();
                UpdateStore::log( "check: " + manifest->displayVersion() + " is available" );

                // Already downloaded and staged: nothing to offer, just waiting for a restart.
                if ( auto p = pendingUpdate(); p && p->id == id )
                {
                        mAvailable.reset();
                        setState( State::Ready );
                        emit checkFinished( CheckResult::UpdateReady, userInitiated, p->version );
                        return;
                }

                mAvailable = manifest;
                setState( State::Available );
                emit checkFinished( CheckResult::UpdateAvailable, userInitiated, manifest->displayVersion() );

                const bool declined = UpdateSettings::skippedId() == id || UpdateSettings::failedId() == id;
                if ( userInitiated )
                {
                        // The user asked; the UI shows the offer (even for a release skipped earlier).
                        emit updateAvailable( *manifest, true );
                }
                else if ( !declined )
                {
                        if ( UpdateSettings::autoInstall() )
                        {
                                install();
                        }
                        else if ( !UpdateSettings::isSnoozed( id ) )
                        {
                                emit updateAvailable( *manifest, false );
                        }
                }
        }


        void UpdateService::skipAvailable()
        {
                if ( mAvailable )
                {
                        UpdateSettings::setSkippedId( mAvailable->id() );
                        mAvailable.reset();
                        setState( State::Idle );
                }
        }


        void UpdateService::snoozeAvailable()
        {
                if ( mAvailable )
                {
                        UpdateSettings::snooze( mAvailable->id(), QDateTime::currentDateTime().addDays( 3 ) );
                }
        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // Downloading
        ///////////////////////////////////////////////////////////////////////////////////////

        void UpdateService::install()
        {
                if ( mState != State::Available || !mAvailable )
                {
                        return;
                }

                if ( !mAvailable->assetFor( mConfig.platformKey ) )
                {
                        UpdateStore::log( "no package for " + mConfig.platformKey );
                        emit installFailed( tr( QT_TRANSLATE_NOOP( "UpdateService", "The release has no package for this platform." ) ) );
                        return;
                }

                QDir().mkpath( UpdateStore::dir() );
                mLock = std::make_unique<QLockFile>( UpdateStore::dir() + "/update.lock" );
                mLock->setStaleLockTime( 15 * 60 * 1000 );
                if ( !mLock->tryLock( 0 ) )
                {
                        mLock.reset();
                        UpdateStore::log( "install: another instance is working on an update" );
                        emit installFailed( tr( QT_TRANSLATE_NOOP( "UpdateService", "Another window of the program is already installing an update." ) ) );
                        return;
                }

                // A new download replaces whatever was staged before.
                discardStaged();
                PendingUpdate::clear();

                setState( State::Downloading );
                startDownload();
        }


        void UpdateService::startDownload()
        {
                const UpdateAsset& asset = *mAvailable->assetFor( mConfig.platformKey );

                QDir().mkpath( UpdateStore::downloadDir() );
                removeFilesIn( UpdateStore::downloadDir() );

                mDownloadPath = UpdateStore::downloadDir() + "/" + asset.name;
                mDownloadFile = std::make_unique<QFile>( mDownloadPath + ".part" );
                if ( !mDownloadFile->open( QIODevice::WriteOnly | QIODevice::Truncate ) )
                {
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "Cannot write the download: %1" ) ).arg( mDownloadFile->errorString() ), false );
                        return;
                }
                mDownloadHash = std::make_unique<QCryptographicHash>( QCryptographicHash::Sha256 );

                UpdateStore::log( "download: " + asset.url.toString() );

                QNetworkRequest request( asset.url );
                request.setHeader( QNetworkRequest::UserAgentHeader, "glabels-neo-updater" );
                request.setTransferTimeout( 60 * 1000 );   // no data for a minute

                mDownload = mNetwork.get( request );
                QNetworkReply* reply = mDownload;
                watchRedirects( reply );

                connect( reply, &QNetworkReply::readyRead, this,
                         [this, reply]()
                         {
                                 if ( mState != State::Downloading || !mDownloadFile )
                                 {
                                         return;
                                 }
                                 const QByteArray chunk = reply->readAll();
                                 mDownloadHash->addData( chunk );
                                 mDownloadFile->write( chunk );

                                 // Never take more than the manifest promised.
                                 if ( mDownloadFile->size() > mAvailable->assetFor( mConfig.platformKey )->size )
                                 {
                                         reply->abort();
                                 }
                         } );

                connect( reply, &QNetworkReply::downloadProgress, this,
                         [this]( qint64 received, qint64 total )
                         {
                                 if ( mState == State::Downloading )
                                 {
                                         emit downloadProgress( received, total > 0 ? total : mAvailable->assetFor( mConfig.platformKey )->size );
                                 }
                         } );

                connect( reply, &QNetworkReply::finished, this, &UpdateService::onDownloadFinished );
        }


        void UpdateService::onDownloadFinished()
        {
                QNetworkReply* reply = mDownload;
                if ( !reply || mState != State::Downloading )
                {
                        return;
                }
                reply->deleteLater();

                const UpdateAsset asset = *mAvailable->assetFor( mConfig.platformKey );

                // Whatever arrived after the last readyRead (normally nothing).
                const QByteArray rest = reply->readAll();
                if ( !rest.isEmpty() )
                {
                        mDownloadHash->addData( rest );
                        mDownloadFile->write( rest );
                }
                mDownloadFile->close();

                const int status = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
                if ( reply->error() != QNetworkReply::NoError )
                {
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "Download failed: %1" ) ).arg( reply->errorString() ), false );
                        return;
                }
                if ( status != 200 )
                {
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "Download failed: HTTP status %1" ) ).arg( status ), false );
                        return;
                }

                if ( QFileInfo( mDownloadFile->fileName() ).size() != asset.size )
                {
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "The downloaded file has the wrong size." ) ), false );
                        return;
                }
                if ( QString::fromLatin1( mDownloadHash->result().toHex() ) != asset.sha256 )
                {
                        // Not marked permanent: the "edge" package may have been replaced by a newer
                        // build between reading the manifest and downloading it.
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "The downloaded file is corrupt (checksum mismatch)." ) ), false );
                        return;
                }

                QFile::remove( mDownloadPath );
                if ( !QFile::rename( mDownloadFile->fileName(), mDownloadPath ) )
                {
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "Cannot store the download." ) ), false );
                        return;
                }
                mDownloadFile.reset();
                mDownloadHash.reset();

                UpdateStore::log( "download: verified " + asset.name );
                setState( State::Installing );
                startExtract( mDownloadPath );
        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // Staging
        ///////////////////////////////////////////////////////////////////////////////////////

        void UpdateService::startExtract( const QString& packagePath )
        {
                const InstallLayout& layout = mConfig.layout;

                discardStaged();

                QList<Command> steps;
                QList<Command> cleanup;

                switch ( layout.os )
                {
                case Os::Windows:
                {
                        // bsdtar ships with Windows 10 and later and reads zip archives (GNU tar, which
                        // might come first on PATH, does not).  It refuses paths that escape the target.
                        QDir().mkpath( layout.units[0].staged );
                        steps << Command{ qEnvironmentVariable( "SystemRoot", "C:/Windows" ) + "/System32/tar.exe",
                                          { "-xf", QDir::toNativeSeparators( packagePath ),
                                            "-C", QDir::toNativeSeparators( layout.units[0].staged ) } };
                        break;
                }

                case Os::Linux:
                {
                        QDir().mkpath( layout.units[0].staged );
                        // Absolute path: do not let a directory earlier on PATH choose the program that unpacks the download.
                        const QString tar = QFileInfo::exists( "/usr/bin/tar" ) ? "/usr/bin/tar" : "/bin/tar";
                        steps << Command{ tar, { "-xzf", packagePath, "-C", layout.units[0].staged,
                                                   "--strip-components=1", "--no-same-owner" } };
                        break;
                }

                case Os::MacOs:
                {
                        // Mount the disk image privately, copy the application bundles out, unmount.
                        const QString mount = UpdateStore::dir() + "/mount";
                        QDir( mount ).removeRecursively();
                        QDir().mkpath( mount );

                        steps << Command{ "/usr/bin/hdiutil", { "attach", "-nobrowse", "-readonly", "-noverify",
                                                                "-noautoopen", "-mountpoint", mount, packagePath } };
                        for ( const auto& u : layout.units )
                        {
                                steps << Command{ "/usr/bin/ditto", { mount + "/" + QFileInfo( u.installed ).fileName(), u.staged } };
                        }
                        cleanup << Command{ "/usr/bin/hdiutil", { "detach", mount, "-force" } };
                        break;
                }

                default:
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "Automatic updates are not available on this operating system." ) ), true );
                        return;
                }

                UpdateStore::log( "stage: unpacking " + QFileInfo( packagePath ).fileName() );
                runSequence( this, steps, cleanup,
                             [this, os = layout.os]( bool ok, const QString& error, const QString& )
                             {
                                     if ( os == Os::MacOs )
                                     {
                                             QDir( UpdateStore::dir() + "/mount" ).removeRecursively();
                                     }

                                     if ( !ok )
                                     {
                                             fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "Unpacking the update failed: %1" ) ).arg( error ), true );
                                             return;
                                     }

                                     const QString problem = mConfig.layout.validateStaged();
                                     if ( !problem.isEmpty() )
                                     {
                                             fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "The update is not usable: %1" ) ).arg( problem ), true );
                                             return;
                                     }

                                     startSmokeTest();
                             } );
        }


        void UpdateService::startSmokeTest()
        {
                // Run the new version far enough to know it starts: right libraries, right plugins,
                // right architecture.  A broken build is thrown away instead of being installed.
                QProcessEnvironment env = cleanEnvironment();
                env.insert( "QT_QPA_PLATFORM", "offscreen" );

                Command c{ mConfig.layout.units[0].staged + "/" + mConfig.layout.smokeRel, { "--Version" }, 60 * 1000, env };

                runSequence( this, { c }, {},
                             [this]( bool ok, const QString& error, const QString& output )
                             {
                                     if ( !ok )
                                     {
                                             fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "The new version does not start: %1" ) ).arg( error ), true );
                                             return;
                                     }
                                     if ( !output.contains( "GLABELS NEO" ) )
                                     {
                                             fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "The new version gave an unexpected answer." ) ), true );
                                             return;
                                     }
                                     finishReady();
                             } );
        }


        void UpdateService::finishReady()
        {
                const UpdateAsset& asset = *mAvailable->assetFor( mConfig.platformKey );

                PendingUpdate pending;
                pending.id       = mAvailable->id();
                pending.version  = mAvailable->displayVersion();
                pending.channel  = mAvailable->channel;
                pending.sha256   = asset.sha256;
                pending.commitTime = mAvailable->commitTime;
                pending.stagedAt = QDateTime::currentDateTime();
                if ( !pending.save() )
                {
                        fail( tr( QT_TRANSLATE_NOOP( "UpdateService", "Cannot record the staged update." ) ), false );
                        return;
                }

                removeFilesIn( UpdateStore::downloadDir() );
                UpdateStore::log( "stage: " + pending.version + " is ready; it will be applied when the program exits" );

                const QString version = pending.version;
                mAvailable.reset();
                mLock.reset();
                setState( State::Ready );
                emit updateReady( version );
        }


        void UpdateService::fail( const QString& message, bool permanent )
        {
                UpdateStore::log( "install failed: " + message );

                if ( mDownload )
                {
                        mDownload->disconnect( this );
                        mDownload->abort();
                        mDownload->deleteLater();
                }
                mDownloadFile.reset();
                mDownloadHash.reset();

                discardStaged();
                PendingUpdate::clear();
                removeFilesIn( UpdateStore::downloadDir() );

                if ( permanent && mAvailable )
                {
                        UpdateSettings::setFailedId( mAvailable->id() );   // do not retry the same release automatically
                        mAvailable.reset();
                }

                mLock.reset();
                setState( mAvailable ? State::Available : State::Idle );
                emit installFailed( message );
        }


        void UpdateService::discardStaged()
        {
                for ( const auto& u : mConfig.layout.units )
                {
                        removeTreeAsync( u.staged );
                }
        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // Applying
        ///////////////////////////////////////////////////////////////////////////////////////

        bool UpdateService::applyPendingAtStartup( const QStringList& arguments )
        {
                if ( !mSupported )
                {
                    return false;
                }

                // How did the last swap go?  (The helper has no UI; it leaves a note.)
                if ( QFile f( UpdateStore::resultFile() ); f.open( QIODevice::ReadOnly ) )
                {
                        UpdateStore::log( "helper reported: " + QString::fromUtf8( f.readAll() ).trimmed() );
                        f.close();
                        f.remove();
                }

                auto pending = PendingUpdate::load();
                if ( !pending )
                {
                        return false;
                }

                // Running the staged release (or something newer): the swap happened.
                const bool stable = mConfig.build.channel == "stable";
                const bool applied = ( pending->id == mConfig.build.id() );
                const bool edge = mConfig.build.channel == "edge";
                const bool obsolete = ( stable && compareTags( pending->id, mConfig.build.tag ) <= 0 ) ||
                                      ( edge && pending->commitTime <= mConfig.build.commitTime );   // never move back to an older build
                if ( applied || obsolete )
                {
                        if ( applied )
                        {
                                mJustUpdatedTo = pending->version;
                                UpdateStore::log( "now running " + pending->version );
                        }
                        PendingUpdate::clear();
                        discardStaged();
                        return false;
                }

                if ( !stagedCopiesExist() )
                {
                        PendingUpdate::clear();
                        return false;
                }

                return launchHelper( true, arguments );
        }


        void UpdateService::applyPendingAtExit()
        {
                if ( !mSupported || mState == State::Downloading || mState == State::Installing )
                {
                        return;
                }

                auto pending = PendingUpdate::load();
                if ( pending && pending->id != mConfig.build.id() && stagedCopiesExist() )
                {
                        launchHelper( false, {} );
                }
        }


        bool UpdateService::launchHelper( bool relaunch, const QStringList& relaunchArguments )
        {
                auto pending = PendingUpdate::load();
                if ( !pending )
                {
                        return false;
                }

                // A swap that keeps failing must not turn into a restart loop.
                if ( pending->attempts >= 2 )
                {
                        UpdateStore::log( "giving up on " + pending->version + " after repeated failures" );
                        UpdateSettings::setFailedId( pending->id );
                        discardStaged();
                        PendingUpdate::clear();
                        return false;
                }
                pending->attempts++;
                pending->save();

                // Run a copy of the helper from outside the install directory, which is about to be replaced.
                QDir().mkpath( UpdateStore::runDir() );
                removeFilesIn( UpdateStore::runDir() );

                const QString suffix = QFileInfo( mConfig.layout.helperPath ).suffix();
                const QString copy = UpdateStore::runDir() + "/glabels-updater-" + QString::number( QCoreApplication::applicationPid() )
                                     + ( suffix.isEmpty() ? QString() : "." + suffix );
                if ( !QFile::copy( mConfig.layout.helperPath, copy ) )
                {
                        UpdateStore::log( "cannot copy the helper to " + copy );
                        return false;
                }
                QFile::setPermissions( copy, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                             QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                             QFileDevice::ReadOther | QFileDevice::ExeOther );

                QStringList args;
                args << "--wait-pid" << QString::number( QCoreApplication::applicationPid() )
                     << "--result" << QDir::toNativeSeparators( UpdateStore::resultFile() )
                     << "--log"    << QDir::toNativeSeparators( UpdateStore::logFile() )
                     << "--lock"   << QDir::toNativeSeparators( UpdateStore::lockDir() );
                for ( const auto& u : mConfig.layout.units )
                {
                        args << "--unit" << QDir::toNativeSeparators( u.installed ) << QDir::toNativeSeparators( u.staged );
                }
                if ( relaunch )
                {
                        args << "--relaunch" << QDir::toNativeSeparators( mConfig.layout.relaunchPath ) << relaunchArguments;
                }

                UpdateStore::log( QString( "apply: starting helper (%1)" ).arg( relaunch ? "restart" : "after exit" ) );

                QProcess helper;
                helper.setProgram( copy );
                helper.setArguments( args );
                helper.setProcessEnvironment( QProcessEnvironment::systemEnvironment() );
                if ( !helper.startDetached() )
                {
                        UpdateStore::log( "cannot start the helper: " + helper.errorString() );
                        return false;
                }
                return true;
        }


        void UpdateService::cleanupLeftovers()
        {
                if ( mState != State::Idle || PendingUpdate::load() )
                {
                        return;
                }

                // Only if no other instance is in the middle of an update.
                QDir().mkpath( UpdateStore::dir() );
                QLockFile lock( UpdateStore::dir() + "/update.lock" );
                lock.setStaleLockTime( 15 * 60 * 1000 );
                if ( !lock.tryLock( 0 ) )
                {
                        return;
                }

                for ( const auto& u : mConfig.layout.units )
                {
                        removeTreeAsync( u.staged );
                        // The previous version, if the helper could not delete it -- but never while the install itself
                        // is missing: then the backup is the only copy of the program.
                        if ( QFileInfo::exists( u.installed ) )
                        {
                                removeTreeAsync( u.backup );
                        }
                        removeTrashBesides( u.installed );
                }
                removeFilesIn( UpdateStore::downloadDir() );
                removeFilesIn( UpdateStore::runDir() );
        }

}
