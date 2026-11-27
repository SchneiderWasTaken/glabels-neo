//  ExternalDataSourceDialog.hpp
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

#ifndef datasource_ExternalDataSourceDialog_hpp
#define datasource_ExternalDataSourceDialog_hpp


#include "GoogleAuthManager.hpp"
#include "GoogleSheetsClient.hpp"
#include "RedashClient.hpp"

#include "model/ExternalDataSource.hpp"

#include <QDialog>
#include <QStringList>


class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QWidget;


namespace glabels::datasource
{

        ///
        /// Dialog for linking or editing a live external data source
        /// (Redash query or Google Sheet) for the Fill table.
        ///
        class ExternalDataSourceDialog : public QDialog
        {
                Q_OBJECT

        public:
                explicit ExternalDataSourceDialog(
                        const model::ExternalDataSource& initialSource,
                        const QStringList&               labelFields,
                        QWidget*                         parent = nullptr,
                        model::ExternalSourceKind        preferredKind = model::ExternalSourceKind::Redash,
                        GoogleAuthManager*               googleAuth = nullptr );

                /// Stops any sign-in or picker still waiting on 127.0.0.1.
                ~ExternalDataSourceDialog() override;

                model::ExternalDataSource source() const;
                bool wasUnlinked() const { return mUnlinked; }

                /// If the user ran "Test connection" and succeeded, returns those
                /// fetched rows so FillView can apply them immediately without a
                /// second round-trip.
                bool hasTestedResult() const { return mHasTestedResult; }
                const FetchResult& testedResult() const { return mTestedResult; }

        private slots:
                void onProviderTabChanged( int index );

                // Redash slots
                void onQuickUrlChanged( const QString& text );
                void onAuthModeChanged();
                void onCredentialTargetChanged();
                void onToggleShowKey( bool checked );
                void onAddParameter();
                void onRemoveParameter();

                // Google Sheets slots
                void onGoogleAuthModeChanged();
                void onGoogleSignInClicked();
                void onGoogleSignOutClicked();
                void onPickGoogleSheetClicked();
                void onGoogleUrlChanged( const QString& text );
                void onLoadGoogleTabsClicked();
                void updateGoogleAccountUi();

                // Shared footer slots
                void onTestConnection();
                void onUnlinkClicked();
                void onAcceptClicked();

        private:
                QWidget* buildRedashTab();
                QWidget* buildGoogleSheetsTab();

                model::ExternalDataSource buildSourceFromForm() const;
                QString effectiveRedashApiKey( const model::ExternalDataSource& src ) const;
                void setParametersInTable( const QMap<QString,QString>& params );
                QMap<QString,QString> parametersFromTable() const;
                void updateStatus( const QString& text, bool isError = false );

        private:
                model::ExternalDataSource mInitialSource;
                QStringList               mLabelFields;
                GoogleAuthManager*        mGoogleAuth{ nullptr };
                RedashClient              mRedashClient;
                GoogleSheetsClient        mGoogleSheetsClient;

                QTabWidget*   mProviderTabs{ nullptr };

                // Redash controls
                QLineEdit*    mQuickUrlEdit{ nullptr };
                QComboBox*    mServerCombo{ nullptr };
                QSpinBox*     mQueryIdSpin{ nullptr };
                QComboBox*    mAuthModeCombo{ nullptr };
                QLineEdit*    mApiKeyEdit{ nullptr };
                QPushButton*  mShowKeyBtn{ nullptr };
                QLabel*       mKeyHintLabel{ nullptr };
                QLineEdit*    mRedashDisplayNameEdit{ nullptr };
                QTableWidget* mParamsTable{ nullptr };
                QPushButton*  mAddParamBtn{ nullptr };
                QPushButton*  mRemoveParamBtn{ nullptr };

                // Google Sheets controls
                QComboBox*    mGoogleAuthModeCombo{ nullptr };
                QWidget*      mGoogleAccountBox{ nullptr };
                QLabel*       mGoogleAccountLabel{ nullptr };
                QPushButton*  mGoogleSignInBtn{ nullptr };
                QPushButton*  mGoogleSignOutBtn{ nullptr };
                QPushButton*  mGooglePickBtn{ nullptr };
                QLineEdit*    mGoogleUrlEdit{ nullptr };
                QComboBox*    mGoogleTabCombo{ nullptr };
                QPushButton*  mGoogleLoadTabsBtn{ nullptr };
                QLineEdit*    mGoogleRangeEdit{ nullptr };
                QLineEdit*    mGoogleDisplayNameEdit{ nullptr };
                QCheckBox*    mGoogleHeaderRowCheck{ nullptr };
                QString       mParsedGoogleGid;

                // Shared sync & footer controls
                QCheckBox*    mRefreshOnOpenCheck{ nullptr };
                QCheckBox*    mRefreshBeforePrintCheck{ nullptr };
                QComboBox*    mMatchKeyCombo{ nullptr };
                QPushButton*  mTestBtn{ nullptr };
                QLabel*       mStatusLabel{ nullptr };
                QPushButton*  mUnlinkBtn{ nullptr };
                QPushButton*  mSaveBtn{ nullptr };

                model::ExternalDataSource mResultSource;
                bool                      mUnlinked{ false };
                bool                      mHasTestedResult{ false };
                FetchResult               mTestedResult;
        };

}


#endif // datasource_ExternalDataSourceDialog_hpp
