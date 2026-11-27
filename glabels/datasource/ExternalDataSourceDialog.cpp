//  ExternalDataSourceDialog.cpp
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


#include "ExternalDataSourceDialog.hpp"

#include "SecretStore.hpp"

#include "model/Settings.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>


namespace glabels::datasource
{

        namespace
        {
                enum ProviderTab
                {
                        TAB_REDASH = 0,
                        TAB_GOOGLE_SHEETS = 1
                };

                enum RedashAuthOption
                {
                        AUTH_USER_FRESH = 0,
                        AUTH_USER_CACHED_5M = 1,
                        AUTH_QUERY_CACHED = 2
                };

                enum GoogleAuthOption
                {
                        GOOGLE_OAUTH = 0,
                        GOOGLE_PUBLIC_LINK = 1
                };
        }


        ExternalDataSourceDialog::ExternalDataSourceDialog( const model::ExternalDataSource& initialSource,
                                                            const QStringList&               labelFields,
                                                            QWidget*                         parent,
                                                            model::ExternalSourceKind        preferredKind,
                                                            GoogleAuthManager*               googleAuth )
                : QDialog( parent ),
                  mInitialSource( initialSource ),
                  mLabelFields( labelFields ),
                  mGoogleAuth( googleAuth ? googleAuth : GoogleAuthManager::instance() ),
                  mRedashClient( this ),
                  mGoogleSheetsClient( mGoogleAuth, this )
        {
                setWindowTitle( tr( "Link external data source" ) );
                setMinimumWidth( 580 );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( 20, 18, 20, 18 );
                root->setSpacing( 12 );

                auto* intro = new QLabel(
                        tr( "Link a live Redash query or Google Sheet to the Fill table. "
                            "Credentials are stored in your system credential store and never written to .glabels files." ),
                        this );
                intro->setWordWrap( true );
                intro->setProperty( "role", QStringLiteral( "caption" ) );
                root->addWidget( intro );

                // Provider tabs
                mProviderTabs = new QTabWidget( this );
                mProviderTabs->addTab( buildRedashTab(), tr( "Redash" ) );
                mProviderTabs->addTab( buildGoogleSheetsTab(), tr( "Google Sheets" ) );
                root->addWidget( mProviderTabs );

                // Shared refresh & row-matching options
                auto* syncCard = new QGroupBox( tr( "Refresh options" ), this );
                syncCard->setProperty( "role", QStringLiteral( "card" ) );
                auto* syncLayout = new QVBoxLayout( syncCard );
                syncLayout->setContentsMargins( 14, 12, 14, 12 );
                syncLayout->setSpacing( 6 );

                mRefreshOnOpenCheck = new QCheckBox( tr( "Refresh automatically when opening this project" ), syncCard );
                mRefreshBeforePrintCheck = new QCheckBox( tr( "Refresh latest rows before printing" ), syncCard );
                syncLayout->addWidget( mRefreshOnOpenCheck );
                syncLayout->addWidget( mRefreshBeforePrintCheck );

                auto* matchRow = new QHBoxLayout();
                matchRow->addWidget( new QLabel( tr( "Preserve Qty / Print by matching column:" ), syncCard ) );
                mMatchKeyCombo = new QComboBox( syncCard );
                mMatchKeyCombo->addItem( tr( "(Auto — first matching label field)" ), QString() );
                mMatchKeyCombo->addItem( tr( "(Replace all rows on refresh)" ), QStringLiteral( "__replace__" ) );
                for ( const QString& field : mLabelFields )
                {
                        mMatchKeyCombo->addItem( field, field );
                }
                matchRow->addWidget( mMatchKeyCombo, 1 );
                syncLayout->addLayout( matchRow );
                root->addWidget( syncCard );

                // Test & status bar
                auto* testRow = new QHBoxLayout();
                testRow->setSpacing( 10 );
                mTestBtn = new QPushButton( tr( "Test connection" ), this );
                mStatusLabel = new QLabel( this );
                mStatusLabel->setWordWrap( true );
                mStatusLabel->setProperty( "role", QStringLiteral( "caption" ) );
                testRow->addWidget( mTestBtn );
                testRow->addWidget( mStatusLabel, 1 );
                root->addLayout( testRow );

                // Dialog footer
                auto* footer = new QHBoxLayout();
                footer->setContentsMargins( 0, 12, 0, 0 );
                mUnlinkBtn = new QPushButton( tr( "Unlink source" ), this );
                mUnlinkBtn->setVisible( initialSource.isValid() );
                footer->addWidget( mUnlinkBtn );
                footer->addStretch( 1 );

                auto* cancelBtn = new QPushButton( tr( "Cancel" ), this );
                mSaveBtn = new QPushButton(
                        initialSource.isValid() ? tr( "Save && Refresh" ) : tr( "Link && Load" ),
                        this );
                mSaveBtn->setDefault( true );
                footer->addWidget( cancelBtn );
                footer->addWidget( mSaveBtn );
                root->addLayout( footer );

                // Populate initial state
                mRefreshOnOpenCheck->setChecked( initialSource.isValid() ? initialSource.refreshOnOpen : true );
                mRefreshBeforePrintCheck->setChecked( initialSource.isValid() ? initialSource.refreshBeforePrint : true );
                if ( initialSource.isValid() )
                {
                        const int matchIdx = mMatchKeyCombo->findData( initialSource.matchKeyField );
                        if ( matchIdx >= 0 )
                        {
                                mMatchKeyCombo->setCurrentIndex( matchIdx );
                        }
                }

                if ( initialSource.kind == model::ExternalSourceKind::Redash )
                {
                        mServerCombo->setCurrentText( initialSource.serverUrl );
                        if ( initialSource.queryId > 0 )
                        {
                                mQueryIdSpin->setValue( initialSource.queryId );
                        }
                        if ( initialSource.redashAuthMode == model::RedashAuthMode::QueryApiKey )
                        {
                                mAuthModeCombo->setCurrentIndex( AUTH_QUERY_CACHED );
                        }
                        else if ( initialSource.maxAgeSeconds > 0 )
                        {
                                mAuthModeCombo->setCurrentIndex( AUTH_USER_CACHED_5M );
                        }
                        else
                        {
                                mAuthModeCombo->setCurrentIndex( AUTH_USER_FRESH );
                        }
                        mRedashDisplayNameEdit->setText( initialSource.displayName );
                        setParametersInTable( initialSource.parameters );
                        mProviderTabs->setCurrentIndex( TAB_REDASH );
                }
                else if ( initialSource.kind == model::ExternalSourceKind::GoogleSheets )
                {
                        mGoogleAuthModeCombo->setCurrentIndex(
                                initialSource.googleAuthMode == model::GoogleAuthMode::PublicLink
                                        ? GOOGLE_PUBLIC_LINK
                                        : GOOGLE_OAUTH );
                        // Show a normal sheet link rather than the bare ID.
                        QString link = QStringLiteral( "https://docs.google.com/spreadsheets/d/%1/edit" )
                                               .arg( initialSource.spreadsheetId );
                        if ( !initialSource.sheetGid.isEmpty() )
                        {
                                link += QStringLiteral( "#gid=" ) + initialSource.sheetGid;
                        }
                        mGoogleUrlEdit->setText( link );
                        mParsedGoogleGid = initialSource.sheetGid;
                        if ( !initialSource.sheetName.isEmpty() )
                        {
                                mGoogleTabCombo->addItem( initialSource.sheetName );
                                mGoogleTabCombo->setCurrentText( initialSource.sheetName );
                        }
                        mGoogleRangeEdit->setText( initialSource.cellRange );
                        mGoogleDisplayNameEdit->setText( initialSource.displayName );
                        mGoogleHeaderRowCheck->setChecked( initialSource.hasHeaderRow );
                        mProviderTabs->setCurrentIndex( TAB_GOOGLE_SHEETS );
                }
                else
                {
                        if ( mServerCombo->count() == 0 )
                        {
                                mServerCombo->setCurrentText( QString() );
                        }
                        mProviderTabs->setCurrentIndex(
                                preferredKind == model::ExternalSourceKind::GoogleSheets
                                        ? TAB_GOOGLE_SHEETS
                                        : TAB_REDASH );
                }

                // Wire shared signals
                connect( mProviderTabs, &QTabWidget::currentChanged,
                         this, &ExternalDataSourceDialog::onProviderTabChanged );
                connect( mTestBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onTestConnection );
                connect( mUnlinkBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onUnlinkClicked );
                connect( cancelBtn, &QPushButton::clicked, this, &QDialog::reject );
                connect( mSaveBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onAcceptClicked );

                auto wireFetcher = [this]( ExternalDataFetcher* fetcher ) {
                        connect( fetcher, &ExternalDataFetcher::progressChanged, this, [this]( const QString& msg ) {
                                updateStatus( msg, false );
                        } );
                        connect( fetcher, &ExternalDataFetcher::finished, this, [this]( const FetchResult& result ) {
                                mTestBtn->setEnabled( true );
                                mHasTestedResult = true;
                                mTestedResult = result;
                                const QString colsPreview = result.headers.mid( 0, 5 ).join( QStringLiteral( ", " ) )
                                        + ( result.headers.size() > 5 ? QStringLiteral( "..." ) : QString() );
                                updateStatus(
                                        tr( "Connected: %1 row(s), %2 column(s) (%3)." )
                                                .arg( result.rows.size() )
                                                .arg( result.headers.size() )
                                                .arg( colsPreview ),
                                        false );
                        } );
                        connect( fetcher, &ExternalDataFetcher::failed, this, [this]( const QString& err ) {
                                mTestBtn->setEnabled( true );
                                mHasTestedResult = false;
                                updateStatus( err, true );
                        } );
                };
                wireFetcher( &mRedashClient );
                wireFetcher( &mGoogleSheetsClient );

                connect( &mRedashClient, &RedashClient::queryInfoLoaded, this, [this]( const RedashQueryInfo& info ) {
                        if ( mRedashDisplayNameEdit->text().trimmed().isEmpty() && !info.name.isEmpty() )
                        {
                                mRedashDisplayNameEdit->setText( info.name );
                        }
                        if ( mParamsTable->rowCount() == 0 && !info.defaultParameters.isEmpty() )
                        {
                                setParametersInTable( info.defaultParameters );
                        }
                } );

                connect( &mGoogleSheetsClient, &GoogleSheetsClient::spreadsheetInfoLoaded, this,
                         [this]( const GoogleSpreadsheetInfo& info ) {
                                 mGoogleLoadTabsBtn->setEnabled( true );
                                 if ( mGoogleDisplayNameEdit->text().trimmed().isEmpty() && !info.title.isEmpty() )
                                 {
                                         mGoogleDisplayNameEdit->setText( info.title );
                                 }
                                 const QString prev = mGoogleTabCombo->currentText().trimmed();
                                 mGoogleTabCombo->clear();
                                 for ( const auto& tab : info.tabs )
                                 {
                                         mGoogleTabCombo->addItem( tab.title );
                                 }
                                 if ( !prev.isEmpty() )
                                 {
                                         mGoogleTabCombo->setCurrentText( prev );
                                 }
                                 else if ( !mParsedGoogleGid.isEmpty() )
                                 {
                                         for ( const auto& tab : info.tabs )
                                         {
                                                 if ( QString::number( tab.sheetId ) == mParsedGoogleGid )
                                                 {
                                                         mGoogleTabCombo->setCurrentText( tab.title );
                                                         break;
                                                 }
                                         }
                                 }
                                 updateStatus(
                                         tr( "Loaded \"%1\" (%2 tab(s))." )
                                                 .arg( info.title.isEmpty() ? tr( "the sheet" ) : info.title )
                                                 .arg( info.tabs.size() ),
                                         false );
                         } );
                connect( &mGoogleSheetsClient, &GoogleSheetsClient::spreadsheetInfoFailed, this,
                         [this]( const QString& err ) {
                                 mGoogleLoadTabsBtn->setEnabled( true );
                                 updateStatus( err, true );
                         } );

                connect( mGoogleAuth, &GoogleAuthManager::signedIn, this, [this]( const QString& email ) {
                        updateGoogleAccountUi();
                        updateStatus(
                                email.isEmpty()
                                        ? tr( "Signed in to Google." )
                                        : tr( "Signed in to Google as %1." ).arg( email ),
                                false );
                } );
                connect( mGoogleAuth, &GoogleAuthManager::signInFailed, this, [this]( const QString& err ) {
                        updateGoogleAccountUi();
                        updateStatus( err, true );
                } );
                connect( mGoogleAuth, &GoogleAuthManager::signedOut, this, [this]() {
                        updateGoogleAccountUi();
                        updateStatus( tr( "Signed out of Google." ), false );
                } );
                connect( mGoogleAuth, &GoogleAuthManager::spreadsheetPicked, this,
                         [this]( const QString& id, const QString& title ) {
                                 mGoogleUrlEdit->setText(
                                         QStringLiteral( "https://docs.google.com/spreadsheets/d/%1/edit" ).arg( id ) );
                                 if ( !title.isEmpty() )
                                 {
                                         mGoogleDisplayNameEdit->setText( title );
                                 }
                                 mParsedGoogleGid.clear();
                                 mHasTestedResult = false;
                                 updateStatus( tr( "Selected \"%1\". Loading tabs..." ).arg( title.isEmpty() ? tr( "the sheet" ) : title ), false );
                                 onLoadGoogleTabsClicked();
                         } );
                connect( mGoogleAuth, &GoogleAuthManager::pickerFailed, this, [this]( const QString& err ) {
                        updateStatus( err, true );
                } );

                onAuthModeChanged();
                onCredentialTargetChanged();
                onGoogleAuthModeChanged();
                updateGoogleAccountUi();
        }


        QWidget* ExternalDataSourceDialog::buildRedashTab()
        {
                auto* page = new QWidget( this );
                auto* layout = new QVBoxLayout( page );
                layout->setContentsMargins( 10, 12, 10, 10 );
                layout->setSpacing( 10 );

                auto* form = new QFormLayout();
                form->setSpacing( 8 );

                mQuickUrlEdit = new QLineEdit( page );
                mQuickUrlEdit->setPlaceholderText(
                        tr( "Paste a Redash query or results URL (e.g. https://redash.company.com/queries/42)" ) );
                form->addRow( tr( "Paste URL:" ), mQuickUrlEdit );

                mServerCombo = new QComboBox( page );
                mServerCombo->setEditable( true );
                mServerCombo->addItems( model::Settings::recentRedashServers() );
                if ( mServerCombo->lineEdit() )
                {
                        mServerCombo->lineEdit()->setPlaceholderText( QStringLiteral( "https://redash.example.com" ) );
                }
                form->addRow( tr( "Server URL:" ), mServerCombo );

                mQueryIdSpin = new QSpinBox( page );
                mQueryIdSpin->setRange( 1, 99999999 );
                mQueryIdSpin->setValue( mInitialSource.queryId > 0 ? mInitialSource.queryId : 1 );
                form->addRow( tr( "Query ID:" ), mQueryIdSpin );

                mAuthModeCombo = new QComboBox( page );
                mAuthModeCombo->addItem( tr( "User API key — run fresh query (live)" ), AUTH_USER_FRESH );
                mAuthModeCombo->addItem( tr( "User API key — allow cached results (5 min)" ), AUTH_USER_CACHED_5M );
                mAuthModeCombo->addItem( tr( "Query API key — latest cached results" ), AUTH_QUERY_CACHED );
                form->addRow( tr( "API key type:" ), mAuthModeCombo );

                auto* keyRow = new QHBoxLayout();
                keyRow->setSpacing( 6 );
                mApiKeyEdit = new QLineEdit( page );
                mApiKeyEdit->setEchoMode( QLineEdit::Password );
                mShowKeyBtn = new QPushButton( tr( "Show" ), page );
                mShowKeyBtn->setCheckable( true );
                mShowKeyBtn->setFixedWidth( 64 );
                keyRow->addWidget( mApiKeyEdit, 1 );
                keyRow->addWidget( mShowKeyBtn );
                form->addRow( tr( "API key:" ), keyRow );

                mKeyHintLabel = new QLabel( page );
                mKeyHintLabel->setWordWrap( true );
                mKeyHintLabel->setProperty( "role", QStringLiteral( "caption" ) );
                form->addRow( QString(), mKeyHintLabel );

                mRedashDisplayNameEdit = new QLineEdit( page );
                mRedashDisplayNameEdit->setPlaceholderText( tr( "(optional — auto-filled on test)" ) );
                form->addRow( tr( "Label / name:" ), mRedashDisplayNameEdit );

                layout->addLayout( form );

                auto* paramsLabel = new QLabel( tr( "Query parameters (optional):" ), page );
                layout->addWidget( paramsLabel );

                mParamsTable = new QTableWidget( 0, 2, page );
                mParamsTable->setHorizontalHeaderLabels( { tr( "Parameter" ), tr( "Value" ) } );
                mParamsTable->horizontalHeader()->setStretchLastSection( true );
                mParamsTable->verticalHeader()->setVisible( false );
                mParamsTable->setSelectionBehavior( QAbstractItemView::SelectRows );
                mParamsTable->setMaximumHeight( 96 );
                layout->addWidget( mParamsTable );

                auto* paramBtnRow = new QHBoxLayout();
                mAddParamBtn = new QPushButton( tr( "Add parameter" ), page );
                mRemoveParamBtn = new QPushButton( tr( "Remove" ), page );
                paramBtnRow->addWidget( mAddParamBtn );
                paramBtnRow->addWidget( mRemoveParamBtn );
                paramBtnRow->addStretch( 1 );
                layout->addLayout( paramBtnRow );

                connect( mQuickUrlEdit, &QLineEdit::textChanged, this, &ExternalDataSourceDialog::onQuickUrlChanged );
                connect( mServerCombo, &QComboBox::currentTextChanged, this, &ExternalDataSourceDialog::onCredentialTargetChanged );
                connect( mQueryIdSpin, qOverload<int>( &QSpinBox::valueChanged ), this, &ExternalDataSourceDialog::onCredentialTargetChanged );
                connect( mAuthModeCombo, qOverload<int>( &QComboBox::currentIndexChanged ), this, &ExternalDataSourceDialog::onAuthModeChanged );
                connect( mShowKeyBtn, &QPushButton::toggled, this, &ExternalDataSourceDialog::onToggleShowKey );
                connect( mAddParamBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onAddParameter );
                connect( mRemoveParamBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onRemoveParameter );

                return page;
        }


        QWidget* ExternalDataSourceDialog::buildGoogleSheetsTab()
        {
                auto* page = new QWidget( this );
                auto* layout = new QVBoxLayout( page );
                layout->setContentsMargins( 10, 12, 10, 10 );
                layout->setSpacing( 10 );

                auto* form = new QFormLayout();
                form->setSpacing( 8 );

                mGoogleAuthModeCombo = new QComboBox( page );
                mGoogleAuthModeCombo->addItem( tr( "Sign in with Google (private & Workspace sheets)" ), GOOGLE_OAUTH );
                mGoogleAuthModeCombo->addItem( tr( "Public link (Anyone with the link can view — no login)" ), GOOGLE_PUBLIC_LINK );
                form->addRow( tr( "Access:" ), mGoogleAuthModeCombo );

                mGoogleAccountBox = new QWidget( page );
                auto* acctLayout = new QHBoxLayout( mGoogleAccountBox );
                acctLayout->setContentsMargins( 0, 0, 0, 0 );
                acctLayout->setSpacing( 6 );
                mGoogleAccountLabel = new QLabel( mGoogleAccountBox );
                mGoogleAccountLabel->setWordWrap( true );
                mGoogleAccountLabel->setProperty( "role", QStringLiteral( "caption" ) );
                mGoogleSignInBtn = new QPushButton( tr( "Sign in with Google" ), mGoogleAccountBox );
                mGooglePickBtn = new QPushButton( tr( "Pick a Google Sheet..." ), mGoogleAccountBox );
                mGoogleSignOutBtn = new QPushButton( tr( "Sign out" ), mGoogleAccountBox );
                acctLayout->addWidget( mGoogleAccountLabel, 1 );
                acctLayout->addWidget( mGoogleSignInBtn );
                acctLayout->addWidget( mGooglePickBtn );
                acctLayout->addWidget( mGoogleSignOutBtn );
                form->addRow( tr( "Google account:" ), mGoogleAccountBox );

                // Signed-in users choose a sheet with the picker and see its name;
                // public-link users paste the sheet's link.  The raw spreadsheet ID
                // only shows under "Show debug details".
                mGoogleSheetNameLabel = new QLabel( page );
                mGoogleSheetNameLabel->setWordWrap( true );
                form->addRow( tr( "Sheet:" ), mGoogleSheetNameLabel );

                mGoogleUrlEdit = new QLineEdit( page );
                mGoogleUrlEdit->setPlaceholderText(
                        tr( "Paste the sheet's link (https://docs.google.com/spreadsheets/d/...)" ) );
                form->addRow( tr( "Sheet link:" ), mGoogleUrlEdit );

                auto* tabRow = new QHBoxLayout();
                tabRow->setSpacing( 6 );
                mGoogleTabCombo = new QComboBox( page );
                mGoogleTabCombo->setEditable( true );
                if ( mGoogleTabCombo->lineEdit() )
                {
                        mGoogleTabCombo->lineEdit()->setPlaceholderText( tr( "(blank = first sheet or #gid from URL)" ) );
                }
                mGoogleLoadTabsBtn = new QPushButton( tr( "Load tabs" ), page );
                tabRow->addWidget( mGoogleTabCombo, 1 );
                tabRow->addWidget( mGoogleLoadTabsBtn );
                form->addRow( tr( "Worksheet tab:" ), tabRow );

                mGoogleDisplayNameEdit = new QLineEdit( page );
                mGoogleDisplayNameEdit->setPlaceholderText( tr( "(optional — auto-filled when picked or loaded)" ) );
                form->addRow( tr( "Label / name:" ), mGoogleDisplayNameEdit );

                layout->addLayout( form );
                mGoogleForm = form;

                // Debug details: spreadsheet ID, cell range and header-row option.
                // Defaults (whole tab, first row = column names) suit nearly everyone.
                mGoogleDebugCheck = new QCheckBox( tr( "Show debug details" ), page );
                layout->addWidget( mGoogleDebugCheck );

                mGoogleDebugBox = new QWidget( page );
                auto* debugForm = new QFormLayout( mGoogleDebugBox );
                debugForm->setContentsMargins( 0, 0, 0, 0 );
                debugForm->setSpacing( 8 );

                mGoogleIdEdit = new QLineEdit( mGoogleDebugBox );
                mGoogleIdEdit->setReadOnly( true );
                debugForm->addRow( tr( "Spreadsheet ID:" ), mGoogleIdEdit );

                mGoogleRangeEdit = new QLineEdit( mGoogleDebugBox );
                mGoogleRangeEdit->setPlaceholderText( tr( "(optional A1 range, e.g. A1:F — blank = whole tab)" ) );
                debugForm->addRow( tr( "Cell range:" ), mGoogleRangeEdit );

                mGoogleHeaderRowCheck = new QCheckBox( tr( "First row contains column headers" ), mGoogleDebugBox );
                mGoogleHeaderRowCheck->setChecked( true );
                debugForm->addRow( QString(), mGoogleHeaderRowCheck );

                mGoogleDebugBox->setVisible( false );
                layout->addWidget( mGoogleDebugBox );
                layout->addStretch( 1 );

                connect( mGoogleDebugCheck, &QCheckBox::toggled, mGoogleDebugBox, &QWidget::setVisible );
                connect( mGoogleDisplayNameEdit, &QLineEdit::textChanged,
                         this, [this]() { updateGoogleSheetUi(); } );

                connect( mGoogleAuthModeCombo, qOverload<int>( &QComboBox::currentIndexChanged ),
                         this, &ExternalDataSourceDialog::onGoogleAuthModeChanged );
                connect( mGoogleSignInBtn, &QPushButton::clicked,
                         this, &ExternalDataSourceDialog::onGoogleSignInClicked );
                connect( mGoogleSignOutBtn, &QPushButton::clicked,
                         this, &ExternalDataSourceDialog::onGoogleSignOutClicked );
                connect( mGooglePickBtn, &QPushButton::clicked,
                         this, &ExternalDataSourceDialog::onPickGoogleSheetClicked );
                connect( mGoogleUrlEdit, &QLineEdit::textChanged,
                         this, &ExternalDataSourceDialog::onGoogleUrlChanged );
                connect( mGoogleLoadTabsBtn, &QPushButton::clicked,
                         this, &ExternalDataSourceDialog::onLoadGoogleTabsClicked );

                return page;
        }


        ExternalDataSourceDialog::~ExternalDataSourceDialog()
        {
                if ( mGoogleAuth )
                {
                        mGoogleAuth->cancelSignIn();
                        mGoogleAuth->cancelPicker();
                }
        }


        model::ExternalDataSource ExternalDataSourceDialog::source() const
        {
                return mResultSource;
        }


        void ExternalDataSourceDialog::onProviderTabChanged( int )
        {
                mHasTestedResult = false;
                updateStatus( QString(), false );
        }


        void ExternalDataSourceDialog::onQuickUrlChanged( const QString& text )
        {
                const auto parsed = model::ExternalDataSource::parseRedashUrl( text );
                if ( !parsed.valid )
                {
                        return;
                }

                mServerCombo->setCurrentText( parsed.serverUrl );
                mQueryIdSpin->setValue( parsed.queryId );
                if ( parsed.authMode == model::RedashAuthMode::QueryApiKey )
                {
                        mAuthModeCombo->setCurrentIndex( AUTH_QUERY_CACHED );
                }
                if ( !parsed.apiKey.isEmpty() )
                {
                        mApiKeyEdit->setText( parsed.apiKey );
                }
                mHasTestedResult = false;
                updateStatus( tr( "Parsed Redash query #%1 from URL." ).arg( parsed.queryId ), false );
        }


        void ExternalDataSourceDialog::onAuthModeChanged()
        {
                const bool isQueryKey = ( mAuthModeCombo->currentData().toInt() == AUTH_QUERY_CACHED );
                mParamsTable->setEnabled( !isQueryKey );
                mAddParamBtn->setEnabled( !isQueryKey );
                mRemoveParamBtn->setEnabled( !isQueryKey );
                mParamsTable->setToolTip( isQueryKey
                                                  ? tr( "Redash requires a User API key for parameterized queries." )
                                                  : QString() );
                onCredentialTargetChanged();
        }


        void ExternalDataSourceDialog::onCredentialTargetChanged()
        {
                mHasTestedResult = false;
                if ( mProviderTabs->currentIndex() != TAB_REDASH )
                {
                        return;
                }

                const model::ExternalDataSource draft = buildSourceFromForm();
                const bool hasSaved = draft.isValid() && SecretStore::hasSecret( draft.credentialKey() );

                if ( hasSaved )
                {
                        mApiKeyEdit->setPlaceholderText(
                                tr( "Saved API key will be used — enter a new key to replace" ) );
                }
                else
                {
                        mApiKeyEdit->setPlaceholderText(
                                draft.redashAuthMode == model::RedashAuthMode::QueryApiKey
                                        ? tr( "Paste query API key" )
                                        : tr( "Paste user API key from Redash profile" ) );
                }

                mKeyHintLabel->setText(
                        tr( "API keys are stored in %1 and never written to .glabels files." )
                                .arg( SecretStore::backendDescription() ) );
        }


        void ExternalDataSourceDialog::onToggleShowKey( bool checked )
        {
                mApiKeyEdit->setEchoMode( checked ? QLineEdit::Normal : QLineEdit::Password );
                mShowKeyBtn->setText( checked ? tr( "Hide" ) : tr( "Show" ) );
        }


        void ExternalDataSourceDialog::onAddParameter()
        {
                const int row = mParamsTable->rowCount();
                mParamsTable->insertRow( row );
                mParamsTable->setItem( row, 0, new QTableWidgetItem() );
                mParamsTable->setItem( row, 1, new QTableWidgetItem() );
                mParamsTable->setCurrentCell( row, 0 );
                mParamsTable->editItem( mParamsTable->item( row, 0 ) );
        }


        void ExternalDataSourceDialog::onRemoveParameter()
        {
                const int row = mParamsTable->currentRow();
                if ( row >= 0 )
                {
                        mParamsTable->removeRow( row );
                }
        }


        void ExternalDataSourceDialog::onGoogleAuthModeChanged()
        {
                mHasTestedResult = false;
                const bool isOAuth = ( mGoogleAuthModeCombo->currentData().toInt() == GOOGLE_OAUTH );
                mGoogleAccountBox->setVisible( isOAuth );
                mGoogleLoadTabsBtn->setVisible( isOAuth );
                updateGoogleAccountUi();
                updateGoogleSheetUi();
        }


        ///
        /// Signed-in mode shows the picked sheet's name; public-link mode shows
        /// the link box.  The bare spreadsheet ID is only in the debug details.
        ///
        void ExternalDataSourceDialog::updateGoogleSheetUi()
        {
                if ( !mGoogleForm || !mGoogleSheetNameLabel || !mGoogleUrlEdit || !mGoogleAuthModeCombo )
                {
                        return;
                }
                const bool isOAuth = ( mGoogleAuthModeCombo->currentData().toInt() == GOOGLE_OAUTH );
                const auto parsed = model::ExternalDataSource::parseGoogleSheetsUrl( mGoogleUrlEdit->text() );

                QString name = mGoogleDisplayNameEdit->text().trimmed();
                if ( !parsed.valid )
                {
                        name = tr( "None chosen yet. Click \"Pick a Google Sheet...\"." );
                }
                else if ( name.isEmpty() )
                {
                        name = tr( "Google Sheet (name not loaded yet)" );
                }
                mGoogleSheetNameLabel->setText( name );

                mGoogleForm->setRowVisible( mGoogleSheetNameLabel, isOAuth );
                mGoogleForm->setRowVisible( mGoogleUrlEdit, !isOAuth );

                if ( mGoogleIdEdit )
                {
                        mGoogleIdEdit->setText( parsed.valid ? parsed.spreadsheetId : QString() );
                }
        }


        void ExternalDataSourceDialog::updateGoogleAccountUi()
        {
                const bool configured = mGoogleAuth->isConfigured();
                const bool signedIn = mGoogleAuth->isSignedIn();
                const QString email = mGoogleAuth->signedInEmail();

                if ( !configured )
                {
                        mGoogleAccountLabel->setText(
                                tr( "OAuth not configured in this build — use Public link mode or set GLABELS_GOOGLE_CLIENT_ID." ) );
                        mGoogleSignInBtn->setVisible( true );
                        mGoogleSignInBtn->setEnabled( false );
                        mGooglePickBtn->setVisible( false );
                        mGoogleSignOutBtn->setVisible( false );
                        mGoogleLoadTabsBtn->setEnabled( false );
                        return;
                }

                if ( signedIn )
                {
                        mGoogleAccountLabel->setText(
                                email.isEmpty()
                                        ? tr( "Signed in to Google" )
                                        : tr( "Signed in as %1" ).arg( email ) );
                        mGoogleSignInBtn->setVisible( false );
                        mGooglePickBtn->setVisible( true );
                        mGooglePickBtn->setEnabled( mGoogleAuth->isPickerConfigured() );
                        mGooglePickBtn->setToolTip(
                                mGoogleAuth->isPickerConfigured()
                                        ? tr( "Choose a Google Sheet from your Google Drive (grants read access via drive.file)." )
                                        : tr( "Google Picker requires GLABELS_GOOGLE_PICKER_API_KEY." ) );
                        mGoogleSignOutBtn->setVisible( true );
                        mGoogleLoadTabsBtn->setEnabled( true );
                }
                else
                {
                        mGoogleAccountLabel->setText( tr( "Not signed in" ) );
                        mGoogleSignInBtn->setVisible( true );
                        mGoogleSignInBtn->setEnabled( true );
                        mGooglePickBtn->setVisible( false );
                        mGoogleSignOutBtn->setVisible( false );
                        mGoogleLoadTabsBtn->setEnabled( false );
                }
        }


        void ExternalDataSourceDialog::onGoogleSignInClicked()
        {
                updateStatus( tr( "Opening your browser for Google Sign-In..." ), false );
                mGoogleAuth->signIn();
        }


        void ExternalDataSourceDialog::onGoogleSignOutClicked()
        {
                mGoogleAuth->signOut();
        }


        void ExternalDataSourceDialog::onPickGoogleSheetClicked()
        {
                updateStatus( tr( "Opening Google Sheet Picker in your browser..." ), false );
                mGoogleAuth->pickSpreadsheet();
        }


        void ExternalDataSourceDialog::onGoogleUrlChanged( const QString& text )
        {
                mHasTestedResult = false;
                const auto parsed = model::ExternalDataSource::parseGoogleSheetsUrl( text );
                if ( parsed.valid )
                {
                        mParsedGoogleGid = parsed.gid;
                }
                updateGoogleSheetUi();
        }


        void ExternalDataSourceDialog::onLoadGoogleTabsClicked()
        {
                const auto parsed = model::ExternalDataSource::parseGoogleSheetsUrl( mGoogleUrlEdit->text() );
                if ( !parsed.valid )
                {
                        updateStatus( tr( "Pick a Google Sheet or paste its link first." ), true );
                        return;
                }
                mGoogleLoadTabsBtn->setEnabled( false );
                updateStatus( tr( "Loading worksheet tabs from Google Sheets..." ), false );
                mGoogleSheetsClient.fetchSpreadsheetInfo( parsed.spreadsheetId );
        }


        void ExternalDataSourceDialog::onTestConnection()
        {
                const model::ExternalDataSource draft = buildSourceFromForm();

                if ( draft.kind == model::ExternalSourceKind::Redash )
                {
                        if ( !draft.isValid() )
                        {
                                updateStatus( tr( "Enter a valid HTTPS Redash server URL (or http://localhost) and query ID." ), true );
                                return;
                        }

                        const QString key = effectiveRedashApiKey( draft );
                        if ( key.isEmpty() )
                        {
                                updateStatus( tr( "Enter a Redash API key first." ), true );
                                return;
                        }

                        mTestBtn->setEnabled( false );
                        mHasTestedResult = false;
                        updateStatus( tr( "Connecting to Redash..." ), false );

                        if ( draft.redashAuthMode == model::RedashAuthMode::UserApiKey )
                        {
                                mRedashClient.fetchQueryInfo( draft.serverUrl, draft.queryId, key );
                        }
                        mRedashClient.fetch( draft, key );
                }
                else if ( draft.kind == model::ExternalSourceKind::GoogleSheets )
                {
                        if ( !draft.isValid() )
                        {
                                updateStatus( tr( "Pick a Google Sheet or paste its link first." ), true );
                                return;
                        }
                        if ( draft.googleAuthMode == model::GoogleAuthMode::OAuth && !mGoogleAuth->isSignedIn() )
                        {
                                updateStatus( tr( "Sign in with Google first, or switch Access to Public link." ), true );
                                return;
                        }

                        mTestBtn->setEnabled( false );
                        mHasTestedResult = false;
                        updateStatus( tr( "Connecting to Google Sheets..." ), false );
                        mGoogleSheetsClient.fetch( draft );
                }
        }


        void ExternalDataSourceDialog::onUnlinkClicked()
        {
                mRedashClient.cancel();
                mGoogleSheetsClient.cancel();
                mUnlinked = true;
                mResultSource = model::ExternalDataSource();
                accept();
        }


        void ExternalDataSourceDialog::onAcceptClicked()
        {
                const model::ExternalDataSource draft = buildSourceFromForm();

                if ( draft.kind == model::ExternalSourceKind::Redash )
                {
                        if ( !draft.isValid() )
                        {
                                QMessageBox::warning(
                                        this,
                                        tr( "Invalid Redash source" ),
                                        tr( "Please enter a valid HTTPS Redash server URL (http://localhost is allowed for local testing) and a positive query ID." ) );
                                return;
                        }

                        const QString typedKey = mApiKeyEdit->text().trimmed();
                        const QString key = effectiveRedashApiKey( draft );
                        if ( key.isEmpty() )
                        {
                                QMessageBox::warning(
                                        this,
                                        tr( "API key required" ),
                                        tr( "Please enter a Redash API key. It will be stored in %1." )
                                                .arg( SecretStore::backendDescription() ) );
                                return;
                        }

                        if ( !typedKey.isEmpty() && !SecretStore::store( draft.credentialKey(), typedKey ) )
                        {
                                QMessageBox::warning(
                                        this,
                                        tr( "Could not store API key" ),
                                        tr( "Failed to save the API key to %1." )
                                                .arg( SecretStore::backendDescription() ) );
                                return;
                        }

                        model::Settings::addRecentRedashServer( draft.serverUrl );
                }
                else if ( draft.kind == model::ExternalSourceKind::GoogleSheets )
                {
                        if ( !draft.isValid() )
                        {
                                QMessageBox::warning(
                                        this,
                                        tr( "Invalid Google Sheet" ),
                                        tr( "Please pick a Google Sheet or paste its link." ) );
                                return;
                        }
                        if ( draft.googleAuthMode == model::GoogleAuthMode::OAuth && !mGoogleAuth->isSignedIn() )
                        {
                                QMessageBox::warning(
                                        this,
                                        tr( "Google Sign-In required" ),
                                        tr( "Please sign in with Google first, or switch Access to Public link for link-shared sheets." ) );
                                return;
                        }
                }

                mRedashClient.cancel();
                mGoogleSheetsClient.cancel();
                mUnlinked = false;
                mResultSource = draft;
                accept();
        }


        model::ExternalDataSource ExternalDataSourceDialog::buildSourceFromForm() const
        {
                model::ExternalDataSource src;
                src.refreshOnOpen = mRefreshOnOpenCheck->isChecked();
                src.refreshBeforePrint = mRefreshBeforePrintCheck->isChecked();
                src.matchKeyField = mMatchKeyCombo->currentData().toString();

                if ( mProviderTabs->currentIndex() == TAB_GOOGLE_SHEETS )
                {
                        src.kind = model::ExternalSourceKind::GoogleSheets;
                        const auto parsed = model::ExternalDataSource::parseGoogleSheetsUrl( mGoogleUrlEdit->text() );
                        src.spreadsheetId = parsed.valid ? parsed.spreadsheetId : QString();
                        src.sheetGid = parsed.valid && !parsed.gid.isEmpty() ? parsed.gid : mParsedGoogleGid;
                        src.sheetName = mGoogleTabCombo->currentText().trimmed();
                        src.cellRange = mGoogleRangeEdit->text().trimmed();
                        src.displayName = mGoogleDisplayNameEdit->text().trimmed();
                        src.hasHeaderRow = mGoogleHeaderRowCheck->isChecked();
                        src.googleAuthMode = ( mGoogleAuthModeCombo->currentData().toInt() == GOOGLE_PUBLIC_LINK )
                                ? model::GoogleAuthMode::PublicLink
                                : model::GoogleAuthMode::OAuth;
                        return src;
                }

                src.kind = model::ExternalSourceKind::Redash;
                src.serverUrl = model::ExternalDataSource::normalizeServerUrl( mServerCombo->currentText() );
                src.queryId = mQueryIdSpin->value();
                src.displayName = mRedashDisplayNameEdit->text().trimmed();

                const int authOption = mAuthModeCombo->currentData().toInt();
                if ( authOption == AUTH_QUERY_CACHED )
                {
                        src.redashAuthMode = model::RedashAuthMode::QueryApiKey;
                        src.maxAgeSeconds = -1;
                }
                else
                {
                        src.redashAuthMode = model::RedashAuthMode::UserApiKey;
                        src.maxAgeSeconds = ( authOption == AUTH_USER_CACHED_5M ) ? 300 : 0;
                        src.parameters = parametersFromTable();
                }

                return src;
        }


        QString ExternalDataSourceDialog::effectiveRedashApiKey( const model::ExternalDataSource& src ) const
        {
                const QString typed = mApiKeyEdit->text().trimmed();
                if ( !typed.isEmpty() )
                {
                        return typed;
                }
                return SecretStore::retrieve( src.credentialKey() ).trimmed();
        }


        void ExternalDataSourceDialog::setParametersInTable( const QMap<QString,QString>& params )
        {
                mParamsTable->setRowCount( 0 );
                for ( auto it = params.constBegin(); it != params.constEnd(); ++it )
                {
                        const int row = mParamsTable->rowCount();
                        mParamsTable->insertRow( row );
                        mParamsTable->setItem( row, 0, new QTableWidgetItem( it.key() ) );
                        mParamsTable->setItem( row, 1, new QTableWidgetItem( it.value() ) );
                }
        }


        QMap<QString,QString> ExternalDataSourceDialog::parametersFromTable() const
        {
                QMap<QString,QString> params;
                for ( int r = 0; r < mParamsTable->rowCount(); ++r )
                {
                        auto* keyItem = mParamsTable->item( r, 0 );
                        auto* valItem = mParamsTable->item( r, 1 );
                        const QString k = keyItem ? keyItem->text().trimmed() : QString();
                        if ( !k.isEmpty() )
                        {
                                params.insert( k, valItem ? valItem->text() : QString() );
                        }
                }
                return params;
        }


        void ExternalDataSourceDialog::updateStatus( const QString& text, bool isError )
        {
                mStatusLabel->setText( text );
                mStatusLabel->setStyleSheet( isError ? QStringLiteral( "color: #d9534f;" ) : QString() );
        }

}
