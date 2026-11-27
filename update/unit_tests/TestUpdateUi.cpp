//  TestUpdateUi.cpp
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

//
// The user-facing pieces of automatic updates, run offscreen.  A test binary is never an installed release
// package, so the service is "unsupported" here: this checks that the UI says so instead of misbehaving.
//

#include "TestUpdateUi.hpp"

#include "AboutDialog.hpp"
#include "UpdateUi.hpp"

#include "model/Branding.hpp"
#include "model/Version.hpp"
#include "update/UpdateConfig.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>


QTEST_MAIN(TestUpdateUi)

using namespace glabels;


void TestUpdateUi::initTestCase()
{
        QStandardPaths::setTestModeEnabled( true );
        QCoreApplication::setOrganizationName( "glabels-test" );
        QCoreApplication::setApplicationName( "update-ui-test" );

        static QTemporaryDir settingsDir;
        QSettings::setDefaultFormat( QSettings::IniFormat );
        QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, settingsDir.path() );
        QSettings().clear();

        QVERIFY( !update::UpdateService::instance()->isSupported() );
}


void TestUpdateUi::helpMenuEntryExplainsWhyUpdatesAreUnavailable()
{
        QMenu menu;
        menu.addAction( "About" );
        UpdateUi::addToHelpMenu( &menu );

        QList<QAction*> actions = menu.actions();
        QCOMPARE( actions.size(), 3 );
        QVERIFY( actions[1]->isSeparator() );
        QVERIFY( actions[2]->text().contains( "Updates" ) );

        // Triggering it opens a message box (modal): look for it, check what it says, close it.
        bool sawMessage = false;
        QTimer::singleShot( 200, [&]()
        {
                if ( auto* box = qobject_cast<QMessageBox*>( QApplication::activeModalWidget() ) )
                {
                        sawMessage = box->text().contains( update::UpdateService::instance()->unsupportedReason() );
                        box->close();
                }
        } );
        actions[2]->trigger();
        QVERIFY( sawMessage );
}


void TestUpdateUi::preferencesPageIsDisabledWhenUpdatesAreUnavailable()
{
        UpdatePreferencesPage page;

        auto* group = page.findChild<QGroupBox*>();
        QVERIFY( group );
        QVERIFY( !group->isEnabled() );

        bool mentionsReason = false;
        for ( QLabel* label : page.findChildren<QLabel*>() )
        {
                mentionsReason = mentionsReason || label->text().contains( update::UpdateService::instance()->unsupportedReason() );
        }
        QVERIFY( mentionsReason );
}


void TestUpdateUi::preferencesPageStoresTheChoices()
{
        QSettings().clear();

        UpdatePreferencesPage page;
        auto* group = page.findChild<QGroupBox*>();
        QVERIFY( group );
        const auto boxes = page.findChildren<QCheckBox*>();
        QCOMPARE( boxes.size(), 2 );
        QCheckBox* check   = boxes[0];
        QCheckBox* install = boxes[1];

        // defaults: nothing happens in the background until the user has agreed
        QVERIFY( !check->isChecked() );
        QVERIFY( !install->isChecked() );
        QVERIFY( !install->isEnabledTo( group ) );
        QVERIFY( !update::UpdateSettings::autoCheck() );
        QVERIFY( !update::UpdateSettings::consentAsked() );
        QVERIFY( !update::UpdateSettings::autoInstall() );

        check->setChecked( true );
        QVERIFY( update::UpdateSettings::autoCheck() );
        QVERIFY( update::UpdateSettings::consentAsked() );      // setting it here answers the question
        QVERIFY( install->isEnabledTo( group ) );

        install->setChecked( true );
        QVERIFY( update::UpdateSettings::autoInstall() );

        check->setChecked( false );
        QVERIFY( !update::UpdateSettings::autoCheck() );
        QVERIFY( !install->isEnabledTo( group ) );        // nothing to install without checking

        // a new page shows what was stored
        UpdatePreferencesPage again;
        const auto boxes2 = again.findChildren<QCheckBox*>();
        QVERIFY( !boxes2[0]->isChecked() );
        QVERIFY( boxes2[1]->isChecked() );
}


void TestUpdateUi::offerDialogHasTheThreeChoices()
{
        QSettings().clear();

        update::UpdateManifest manifest;
        manifest.channel = "stable";
        manifest.version = "v9.9.9";
        manifest.commit  = "0123456789abcdef0123456789abcdef01234567";
        manifest.notesUrl = QUrl( "https://github.com/owner/project/releases/tag/v9.9.9" );

        // what the service does when an automatic check finds something newer
        UpdateUi::init();
        emit update::UpdateService::instance()->updateAvailable( manifest, false );

        QMessageBox* box = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT( ( box = qobject_cast<QMessageBox*>( QApplication::activeModalWidget() ) ) != nullptr, 5000 );

        QVERIFY( box->text().contains( "v9.9.9" ) );
        QVERIFY( box->informativeText().contains( "releases/tag/v9.9.9" ) );     // link to what is new

        QStringList labels;
        QPushButton* install = nullptr;
        for ( QAbstractButton* b : box->buttons() )
        {
                labels << b->text();
                if ( b->text() == "Install in Background" )
                {
                        install = qobject_cast<QPushButton*>( b );
                }
        }
        QVERIFY2( labels.contains( "Install in Background" ) && labels.contains( "Not Now" ) && labels.contains( "Skip This Version" ),
                  qPrintable( labels.join( "," ) ) );
        QVERIFY( install );

        // a second offer while one is showing must not stack another dialog
        emit update::UpdateService::instance()->updateAvailable( manifest, false );
        QCOMPARE( QApplication::topLevelWidgets().size() > 0, true );
        int boxes = 0;
        for ( QWidget* w : QApplication::topLevelWidgets() )
        {
                boxes += qobject_cast<QMessageBox*>( w ) && w->isVisible() ? 1 : 0;
        }
        QCOMPARE( boxes, 1 );

        // "Install future updates automatically" + "Install in Background" is remembered
        QVERIFY( box->checkBox() );
        QVERIFY( !update::UpdateSettings::autoInstall() );
        box->checkBox()->setChecked( true );
        install->click();
        QTRY_VERIFY_WITH_TIMEOUT( update::UpdateSettings::autoInstall(), 5000 );
}


namespace
{
        // Looks the box up by its text: Qt ignores the window title of a QMessageBox on macOS.
        QMessageBox* waitForBox( const QString& text )
        {
                for ( int waited = 0; waited < 5000; waited += 50 )
                {
                        auto* box = qobject_cast<QMessageBox*>( QApplication::activeModalWidget() );
                        if ( box && box->text().contains( text ) )
                        {
                                return box;
                        }
                        QTest::qWait( 50 );
                }
                return nullptr;
        }

        QPushButton* buttonLabelled( QMessageBox* box, const QString& text )
        {
                for ( QAbstractButton* b : box->buttons() )
                {
                        if ( b->text() == text )
                        {
                                return qobject_cast<QPushButton*>( b );
                        }
                }
                return nullptr;
        }
}


void TestUpdateUi::consentDialogAsksOnceAndStoresTheAnswer()
{
        UpdateUi::init();
        auto* service = update::UpdateService::instance();

        // --- "Don't Check" (also what Escape means) ---
        QSettings().clear();
        QVERIFY( !update::UpdateSettings::consentAsked() );
        emit service->consentRequired();

        QMessageBox* box = waitForBox( "look for new versions in the background" );
        QVERIFY( box );
        QVERIFY( box->informativeText().contains( "github.com" ) );        // says who is contacted
        QVERIFY( box->informativeText().contains( "Preferences" ) );        // says how to change it
        QPushButton* yes = buttonLabelled( box, "Check Automatically" );
        QPushButton* no  = buttonLabelled( box, "Don't Check" );
        QVERIFY( yes && no );
        QVERIFY( box->defaultButton() == no );                             // the safe answer is the default

        emit service->consentRequired();                                   // a second request must not stack a dialog
        int boxes = 0;
        for ( QWidget* w : QApplication::topLevelWidgets() )
        {
                boxes += qobject_cast<QMessageBox*>( w ) && w->isVisible() ? 1 : 0;
        }
        QCOMPARE( boxes, 1 );

        box->close();                                                      // closing without answering = no
        QTRY_VERIFY_WITH_TIMEOUT( update::UpdateSettings::consentAsked(), 5000 );
        QVERIFY( !update::UpdateSettings::autoCheck() );

        // --- "Check Automatically" ---
        QSettings().clear();
        emit service->consentRequired();
        box = waitForBox( "look for new versions in the background" );
        QVERIFY( box );
        QVERIFY( box->checkBox() );
        QVERIFY( !box->checkBox()->isChecked() );                          // installing unasked is never pre-selected
        buttonLabelled( box, "Check Automatically" )->click();
        QTRY_VERIFY_WITH_TIMEOUT( update::UpdateSettings::autoCheck(), 5000 );
        QVERIFY( update::UpdateSettings::consentAsked() );
        QVERIFY( !update::UpdateSettings::autoInstall() );                 // checking is not installing

        // --- "Check Automatically" and install without asking (a separate, explicit tick) ---
        QSettings().clear();
        emit service->consentRequired();
        box = waitForBox( "look for new versions in the background" );
        QVERIFY( box );
        box->checkBox()->setChecked( true );
        buttonLabelled( box, "Check Automatically" )->click();
        QTRY_VERIFY_WITH_TIMEOUT( update::UpdateSettings::autoInstall(), 5000 );
        QVERIFY( update::UpdateSettings::autoCheck() );

        // --- ticking the box but declining does not turn anything on ---
        QSettings().clear();
        emit service->consentRequired();
        box = waitForBox( "look for new versions in the background" );
        QVERIFY( box );
        box->checkBox()->setChecked( true );
        buttonLabelled( box, "Don't Check" )->click();
        QTRY_VERIFY_WITH_TIMEOUT( update::UpdateSettings::consentAsked(), 5000 );
        QVERIFY( !update::UpdateSettings::autoCheck() );
        QVERIFY( !update::UpdateSettings::autoInstall() );
}


void TestUpdateUi::manifestTextIsShownNotInterpreted()
{
        QSettings().clear();
        UpdateUi::init();

        update::UpdateManifest manifest;
        manifest.channel  = "stable";
        manifest.version  = "1.0<a href=\"https://evil.example\">click</a>";      // (the parser refuses this; the dialog does not trust it anyway)
        manifest.commit   = "0123456789abcdef0123456789abcdef01234567";
        manifest.notesUrl = QUrl( "https://github.com/owner/project/releases/tag/x\"><b>y" );

        emit update::UpdateService::instance()->updateAvailable( manifest, false );
        QMessageBox* box = waitForBox( "is available" );
        QVERIFY( box );

        QVERIFY2( !box->text().contains( "<a" ), qPrintable( box->text() ) );
        QVERIFY2( box->text().contains( "&lt;a" ), qPrintable( box->text() ) );
        QVERIFY2( !box->informativeText().contains( "<b>" ), qPrintable( box->informativeText() ) );

        for ( QAbstractButton* b : box->buttons() )
        {
                if ( b->text() == "Not Now" )
                {
                        b->click();
                }
        }
        QTest::qWait( 50 );
}


void TestUpdateUi::aboutDialogShowsReleaseVersionAndAttribution()
{
        AboutDialog about;
        const QString text = about.aboutLabel->text();

        // the attribution to the upstream project is always there
        QVERIFY( text.contains( model::Branding::ATTRIBUTION ) );
        QVERIFY( text.contains( model::Branding::UPSTREAM_URL ) );

        // the version names the GitHub release this build belongs to, and links to it
        QVERIFY( text.contains( "Version" ) );
        QVERIFY( text.contains( model::Version::LONG_STRING ) );
        if ( !update::Config::TAG.isEmpty() )
        {
                QVERIFY( text.contains( update::Config::TAG ) );
                QVERIFY( text.contains( "https://github.com/" + update::Config::REPO + "/releases/tag/" ) );
        }
        else if ( model::Version::IS_DEV_BUILD )
        {
                // built from the dev branch: says so, links to the rolling "dev" prerelease, never updates itself
                QVERIFY( text.contains( "/releases/tag/dev" ) );
                QVERIFY( text.contains( update::Config::COMMIT.left( 7 ) ) );
                QVERIFY( text.contains( "Development build" ) );
                QVERIFY( update::Config::CHANNEL.isEmpty() );
        }
        else if ( update::Config::CHANNEL == "edge" )
        {
                QVERIFY( text.contains( "/releases/tag/edge" ) );
                QVERIFY( text.contains( update::Config::COMMIT.left( 7 ) ) );
        }
        else
        {
                QVERIFY( text.contains( "development build" ) );        // not claimed to be a GitHub release
        }

        // only dev builds carry the development notice
        QCOMPARE( text.contains( "Development build" ), model::Version::IS_DEV_BUILD );
}
