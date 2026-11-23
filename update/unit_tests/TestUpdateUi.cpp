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

#include "UpdateUi.hpp"

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
        const auto boxes = page.findChildren<QCheckBox*>();
        QCOMPARE( boxes.size(), 2 );
        QCheckBox* check   = boxes[0];
        QCheckBox* install = boxes[1];

        // defaults: look for updates, but ask before installing
        QVERIFY( check->isChecked() );
        QVERIFY( !install->isChecked() );
        QVERIFY( update::UpdateSettings::autoCheck() );
        QVERIFY( !update::UpdateSettings::autoInstall() );

        install->setChecked( true );
        QVERIFY( update::UpdateSettings::autoInstall() );

        check->setChecked( false );
        QVERIFY( !update::UpdateSettings::autoCheck() );
        QVERIFY( !install->isEnabled() );        // nothing to install without checking

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
