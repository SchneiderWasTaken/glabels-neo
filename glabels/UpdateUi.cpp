//  UpdateUi.cpp
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

#include "UpdateUi.hpp"

#include "model/Branding.hpp"

#include <QAction>
#include <QApplication>
#include <QGroupBox>
#include <QMainWindow>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>


namespace glabels
{

        using update::UpdateService;
        namespace UpdateSettings = update::UpdateSettings;


        ///////////////////////////////////////////////////////////////////////////////////////
        // UpdateUi
        ///////////////////////////////////////////////////////////////////////////////////////

        UpdateUi::UpdateUi()
                : QObject( QCoreApplication::instance() )
        {
                auto* service = UpdateService::instance();

                connect( service, &UpdateService::consentRequired, this, &UpdateUi::onConsentRequired );
                connect( service, &UpdateService::updateAvailable, this, &UpdateUi::onUpdateAvailable );
                connect( service, &UpdateService::checkFinished,   this, &UpdateUi::onCheckFinished );
                connect( service, &UpdateService::updateReady,     this, &UpdateUi::onUpdateReady );
                connect( service, &UpdateService::installFailed,   this, &UpdateUi::onInstallFailed );
                connect( service, &UpdateService::downloadProgress, this, &UpdateUi::onDownloadProgress );
        }


        void UpdateUi::init()
        {
                auto* ui = instance();

                // One-time notice after an update has been applied.
                const QString version = UpdateService::instance()->justUpdatedTo();
                if ( !version.isEmpty() )
                {
                        QTimer::singleShot( 2000, ui, [ui, version]()
                        {
                                ui->showMessage( tr( "%1 was updated to %2." ).arg( model::Branding::DISPLAY_NAME, version ) );
                        } );
                }
        }


        namespace
        {

                /// A message box that shows its text exactly as given.  (The static QMessageBox functions treat text
                /// that looks like HTML as HTML, and some of what the updater reports comes from the downloaded manifest.)
                void showPlainBox( QMessageBox::Icon icon, QWidget* parent, const QString& title, const QString& text )
                {
                        QMessageBox box( icon, title, text, QMessageBox::Ok, parent );
                        box.setTextFormat( Qt::PlainText );
                        box.exec();
                }

        }


        UpdateUi* UpdateUi::instance()
        {
                static UpdateUi* sInstance = new UpdateUi();
                return sInstance;
        }


        void UpdateUi::addToHelpMenu( QMenu* helpMenu )
        {
                auto* action = new QAction( tr( "Check for &Updates..." ), helpMenu );
                action->setStatusTip( tr( "Look for a newer version of the program" ) );
                connect( action, &QAction::triggered, instance(), &UpdateUi::checkForUpdatesRequested );
                helpMenu->addSeparator();
                helpMenu->addAction( action );
        }


        QWidget* UpdateUi::parentWindow() const
        {
                if ( QWidget* w = QApplication::activeWindow() )
                {
                        return w;
                }
                for ( QWidget* w : QApplication::topLevelWidgets() )
                {
                        if ( qobject_cast<QMainWindow*>( w ) && w->isVisible() )
                        {
                                return w;
                        }
                }
                return nullptr;
        }


        void UpdateUi::showMessage( const QString& text, int timeoutMs ) const
        {
                if ( auto* window = qobject_cast<QMainWindow*>( parentWindow() ) )
                {
                        window->statusBar()->showMessage( text, timeoutMs );
                }
        }


        ///
        /// Help -> Check for Updates...
        ///
        void UpdateUi::checkForUpdatesRequested()
        {
                auto* service = UpdateService::instance();

                if ( !service->isSupported() )
                {
                        showPlainBox( QMessageBox::Information, parentWindow(), tr( "Check for Updates" ), service->unsupportedReason() );
                        return;
                }

                if ( auto pending = service->pendingUpdate() )
                {
                        showPlainBox( QMessageBox::Information, parentWindow(), tr( "Check for Updates" ),
                                                  tr( "%1 has already been downloaded. It will be installed the next time you start %2." )
                                                  .arg( pending->version, model::Branding::DISPLAY_NAME ) );
                        return;
                }

                if ( service->state() == UpdateService::State::Downloading || service->state() == UpdateService::State::Installing )
                {
                        mUserRequestedInstall = true;   // report the outcome of the running download
                        mUserShowsProgress    = true;
                        showMessage( tr( "An update is already being downloaded." ), 5000 );
                        return;
                }

                mUserWaitingForCheck = true;
                showMessage( tr( "Checking for updates..." ), 5000 );
                service->checkNow( true );
        }


        void UpdateUi::onCheckFinished( UpdateService::CheckResult result, bool userInitiated, const QString& message )
        {
                // Only report back if the user is waiting for an answer (also when their request joined a check
                // that the background timer had just started).
                Q_UNUSED( userInitiated )
                if ( !mUserWaitingForCheck )
                {
                        return;
                }
                mUserWaitingForCheck = false;

                const QString title = tr( "Check for Updates" );
                switch ( result )
                {
                case UpdateService::CheckResult::UpToDate:
                        showPlainBox( QMessageBox::Information, parentWindow(), title,
                                                  tr( "You have the latest version of %1 (%2)." )
                                                  .arg( model::Branding::DISPLAY_NAME,
                                                        UpdateService::instance()->build().displayVersion() ) );
                        break;

                case UpdateService::CheckResult::UpdateReady:
                        showPlainBox( QMessageBox::Information, parentWindow(), title,
                                                  tr( "%1 has already been downloaded. It will be installed the next time you start %2." )
                                                  .arg( message, model::Branding::DISPLAY_NAME ) );
                        break;

                case UpdateService::CheckResult::Failed:
                        showPlainBox( QMessageBox::Warning, parentWindow(), title,
                                              tr( "Could not check for updates.\n\n%1" ).arg( message ) );
                        break;

                case UpdateService::CheckResult::Unsupported:
                        showPlainBox( QMessageBox::Information, parentWindow(), title, message );
                        break;

                case UpdateService::CheckResult::UpdateAvailable:
                        break;   // the offer is shown by onUpdateAvailable()
                }
        }


        void UpdateUi::onConsentRequired()
        {
                if ( mConsent && mConsent->isVisible() )
                {
                        return;   // already asking
                }

                auto* service = UpdateService::instance();

                auto* box = new QMessageBox( QMessageBox::Question, tr( "Check for Updates Automatically?" ),
                                             tr( "Should %1 look for new versions in the background?" ).arg( model::Branding::DISPLAY_NAME ),
                                             QMessageBox::NoButton, parentWindow() );
                box->setAttribute( Qt::WA_DeleteOnClose );
                box->setInformativeText(
                        tr( "If you agree, the program asks github.com for a small file (about 1 KB) every %1 hours or so, "
                            "and GitHub sees your IP address as with any download. Nothing else is sent, and there is no "
                            "account or identifier.\n\n"
                            "Unless you tick the box below, nothing is downloaded or installed until you say so. You can change this at any time in "
                            "Edit > Preferences > Updates, or use Help > Check for Updates to look once." )
                        .arg( service->build().channel == "edge" ? 6 : 12 ) );

                QPushButton* yes = box->addButton( tr( "Check Automatically" ), QMessageBox::AcceptRole );
                QPushButton* no  = box->addButton( tr( "Don't Check" ), QMessageBox::RejectRole );
                box->setDefaultButton( no );     // the privacy-preserving answer is the default
                box->setEscapeButton( no );

                // Installing without asking is a separate, explicit choice that is off unless ticked here
                // (or later in Preferences).  It only counts together with "Check Automatically".
                auto* install = new QCheckBox( tr( "Also download and install updates without asking each time" ), box );
                install->setChecked( false );
                box->setCheckBox( install );

                mConsent = box;
                connect( box, &QMessageBox::finished, this,
                         [box, yes, install]()
                         {
                                 // Closing the box any other way than "Check Automatically" is a no.
                                 const bool agreed = box->clickedButton() == yes;
                                 UpdateSettings::setAutoCheck( agreed );
                                 UpdateSettings::setAutoInstall( agreed && install->isChecked() );
                                 if ( agreed )
                                 {
                                         UpdateService::instance()->checkNow( false );
                                 }
                         } );

                box->open();   // does not block the main window
        }


        void UpdateUi::onUpdateAvailable( const update::UpdateManifest& manifest, bool userInitiated )
        {
                Q_UNUSED( userInitiated )

                if ( mOffer )
                {
                        return;   // already asking
                }
                showOffer( manifest );
        }


        void UpdateUi::showOffer( const update::UpdateManifest& manifest )
        {
                auto* service = UpdateService::instance();

                // The box shows rich text (for the link), and some of what goes in it comes from the downloaded manifest:
                // escape everything that is not ours, so that it can only ever be shown, never be markup.
                QString details = tr( "You are using %1." ).arg( service->build().displayVersion().toHtmlEscaped() ) + " " +
                                  tr( "The update downloads quietly in the background while you work and is installed "
                                      "when you close the program, so the next start is on the new version." );
                if ( !manifest.notesUrl.isEmpty() )
                {
                        details += QString( "<br><br><a href=\"%1\">%2</a>" )
                                   .arg( QString::fromLatin1( manifest.notesUrl.toEncoded() ).toHtmlEscaped(), tr( "What's new" ).toHtmlEscaped() );
                }

                auto* box = new QMessageBox( QMessageBox::Information, tr( "Update Available" ),
                                             tr( "%1 %2 is available." ).arg( QString( model::Branding::DISPLAY_NAME ).toHtmlEscaped(),
                                                                                     manifest.displayVersion().toHtmlEscaped() ),
                                             QMessageBox::NoButton, parentWindow() );
                box->setAttribute( Qt::WA_DeleteOnClose );
                box->setTextFormat( Qt::RichText );
                box->setInformativeText( details );

                QPushButton* install = box->addButton( tr( "Install in Background" ), QMessageBox::AcceptRole );
                QPushButton* later   = box->addButton( tr( "Not Now" ), QMessageBox::RejectRole );
                QPushButton* skip    = box->addButton( tr( "Skip This Version" ), QMessageBox::ActionRole );
                box->setDefaultButton( install );
                box->setEscapeButton( later );

                auto* always = new QCheckBox( tr( "Install future updates automatically" ), box );
                box->setCheckBox( always );

                mOffer = box;
                connect( box, &QMessageBox::finished, this,
                         [this, box, install, skip, always]()
                         {
                                 auto* clicked = box->clickedButton();
                                 auto* service = UpdateService::instance();

                                 if ( clicked == install )
                                 {
                                         if ( always->isChecked() )
                                         {
                                                 UpdateSettings::setAutoInstall( true );
                                         }
                                         mUserRequestedInstall = true;
                                         mUserShowsProgress    = true;
                                         showMessage( tr( "Downloading the update in the background..." ), 5000 );
                                         service->install();
                                 }
                                 else if ( clicked == skip )
                                 {
                                         service->skipAvailable();
                                 }
                                 else
                                 {
                                         service->snoozeAvailable();
                                 }
                         } );

                box->open();   // does not block the main window
        }


        void UpdateUi::onDownloadProgress( qint64 received, qint64 total )
        {
                if ( mUserShowsProgress && total > 0 )
                {
                        showMessage( tr( "Downloading update: %1%" ).arg( received * 100 / total ), 2000 );
                }
        }


        void UpdateUi::onUpdateReady( const QString& version )
        {
                // Quiet by design: the work is done, nothing for the user to do.
                showMessage( tr( "%1 is ready and will be installed when you close %2." )
                             .arg( version, model::Branding::DISPLAY_NAME ), 15000 );
                mUserRequestedInstall = false;
                mUserShowsProgress    = false;
        }


        void UpdateUi::onInstallFailed( const QString& message )
        {
                // Automatic installs fail silently (and are logged); one the user asked for reports back.
                if ( mUserRequestedInstall )
                {
                        showPlainBox( QMessageBox::Warning, parentWindow(), tr( "Update" ),
                                              tr( "The update could not be installed.\n\n%1\n\nYour current version is unchanged." ).arg( message ) );
                }
                mUserRequestedInstall = false;
                mUserShowsProgress    = false;
        }


        ///////////////////////////////////////////////////////////////////////////////////////
        // UpdatePreferencesPage
        ///////////////////////////////////////////////////////////////////////////////////////

        UpdatePreferencesPage::UpdatePreferencesPage( QWidget* parent )
                : QWidget( parent )
        {
                auto* service = UpdateService::instance();

                auto* group = new QGroupBox( tr( "Automatic updates" ), this );
                auto* groupLayout = new QVBoxLayout( group );

                mCheckBox = new QCheckBox( tr( "Check for updates automatically" ), group );
                mCheckBox->setChecked( UpdateSettings::autoCheck() );
                groupLayout->addWidget( mCheckBox );

                mAutoInstallBox = new QCheckBox( tr( "Download and install updates in the background without asking" ), group );
                mAutoInstallBox->setChecked( UpdateSettings::autoInstall() );
                mAutoInstallBox->setEnabled( mCheckBox->isChecked() );
                groupLayout->addWidget( mAutoInstallBox );

                auto* info = new QLabel( group );
                info->setWordWrap( true );
                if ( service->isSupported() )
                {
                        QString channel = service->build().channel == "edge"
                                          ? tr( "newest build of the main branch" )
                                          : tr( "stable releases" );
                        info->setText( tr( "Installed version: %1\nFollowing: %2\n\n"
                                           "Updates are installed when you close the program, so the next start is on the new version. "
                                           "Use Help > Check for Updates to look right now." )
                                       .arg( service->build().displayVersion(), channel ) );
                }
                else
                {
                        info->setText( tr( "Automatic updates are not available for this installation.\n\n%1" )
                                       .arg( service->unsupportedReason() ) );
                        group->setEnabled( false );
                }
                groupLayout->addWidget( info );

                auto* layout = new QVBoxLayout( this );
                layout->addWidget( group );
                layout->addStretch();

                connect( mCheckBox, &QCheckBox::toggled, this, &UpdatePreferencesPage::onCheckForUpdatesToggled );
                connect( mAutoInstallBox, &QCheckBox::toggled, this, &UpdatePreferencesPage::onAutoInstallToggled );
        }


        void UpdatePreferencesPage::onCheckForUpdatesToggled( bool checked )
        {
                UpdateSettings::setAutoCheck( checked );
                mAutoInstallBox->setEnabled( checked );
        }


        void UpdatePreferencesPage::onAutoInstallToggled( bool checked )
        {
                UpdateSettings::setAutoInstall( checked );
        }

}
