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
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>


namespace glabels::datasource
{

        namespace
        {
                enum AuthModeOption
                {
                        AUTH_USER_FRESH = 0,
                        AUTH_USER_CACHED_5M = 1,
                        AUTH_QUERY_CACHED = 2
                };
        }


        ExternalDataSourceDialog::ExternalDataSourceDialog( const model::ExternalDataSource& initialSource,
                                                            const QStringList&               labelFields,
                                                            QWidget*                         parent )
                : QDialog( parent ),
                  mInitialSource( initialSource ),
                  mLabelFields( labelFields ),
                  mClient( this )
        {
                setWindowTitle( tr( "Link Redash query" ) );
                setMinimumWidth( 560 );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( 20, 18, 20, 18 );
                root->setSpacing( 12 );

                auto* intro = new QLabel(
                        tr( "Connect a Redash query directly to the Fill table. "
                            "Paste a query URL or enter your Redash server and query ID below." ),
                        this );
                intro->setWordWrap( true );
                intro->setProperty( "role", QStringLiteral( "caption" ) );
                root->addWidget( intro );

                // Connection card
                auto* connCard = new QGroupBox( tr( "Redash connection" ), this );
                connCard->setProperty( "role", QStringLiteral( "card" ) );
                auto* form = new QFormLayout( connCard );
                form->setContentsMargins( 14, 14, 14, 14 );
                form->setSpacing( 8 );

                mQuickUrlEdit = new QLineEdit( connCard );
                mQuickUrlEdit->setPlaceholderText(
                        tr( "Paste a Redash query or results URL (e.g. https://redash.company.com/queries/42)" ) );
                form->addRow( tr( "Paste URL:" ), mQuickUrlEdit );

                mServerCombo = new QComboBox( connCard );
                mServerCombo->setEditable( true );
                mServerCombo->addItems( model::Settings::recentRedashServers() );
                if ( mServerCombo->lineEdit() )
                {
                        mServerCombo->lineEdit()->setPlaceholderText( QStringLiteral( "https://redash.example.com" ) );
                }
                form->addRow( tr( "Server URL:" ), mServerCombo );

                mQueryIdSpin = new QSpinBox( connCard );
                mQueryIdSpin->setRange( 1, 99999999 );
                mQueryIdSpin->setValue( initialSource.queryId > 0 ? initialSource.queryId : 1 );
                form->addRow( tr( "Query ID:" ), mQueryIdSpin );

                mAuthModeCombo = new QComboBox( connCard );
                mAuthModeCombo->addItem( tr( "User API key — run fresh query (live)" ), AUTH_USER_FRESH );
                mAuthModeCombo->addItem( tr( "User API key — allow cached results (5 min)" ), AUTH_USER_CACHED_5M );
                mAuthModeCombo->addItem( tr( "Query API key — latest cached results" ), AUTH_QUERY_CACHED );
                form->addRow( tr( "API key type:" ), mAuthModeCombo );

                auto* keyRow = new QHBoxLayout();
                keyRow->setSpacing( 6 );
                mApiKeyEdit = new QLineEdit( connCard );
                mApiKeyEdit->setEchoMode( QLineEdit::Password );
                mShowKeyBtn = new QPushButton( tr( "Show" ), connCard );
                mShowKeyBtn->setCheckable( true );
                mShowKeyBtn->setFixedWidth( 64 );
                keyRow->addWidget( mApiKeyEdit, 1 );
                keyRow->addWidget( mShowKeyBtn );
                form->addRow( tr( "API key:" ), keyRow );

                mKeyHintLabel = new QLabel( connCard );
                mKeyHintLabel->setWordWrap( true );
                mKeyHintLabel->setProperty( "role", QStringLiteral( "caption" ) );
                form->addRow( QString(), mKeyHintLabel );

                mDisplayNameEdit = new QLineEdit( connCard );
                mDisplayNameEdit->setPlaceholderText( tr( "(optional — auto-filled on test)" ) );
                form->addRow( tr( "Label / name:" ), mDisplayNameEdit );

                root->addWidget( connCard );

                // Parameters card
                auto* paramsCard = new QGroupBox( tr( "Query parameters (optional)" ), this );
                paramsCard->setProperty( "role", QStringLiteral( "card" ) );
                auto* paramsLayout = new QVBoxLayout( paramsCard );
                paramsLayout->setContentsMargins( 14, 12, 14, 12 );
                paramsLayout->setSpacing( 6 );

                mParamsTable = new QTableWidget( 0, 2, paramsCard );
                mParamsTable->setHorizontalHeaderLabels( { tr( "Parameter" ), tr( "Value" ) } );
                mParamsTable->horizontalHeader()->setStretchLastSection( true );
                mParamsTable->verticalHeader()->setVisible( false );
                mParamsTable->setSelectionBehavior( QAbstractItemView::SelectRows );
                mParamsTable->setMaximumHeight( 110 );
                paramsLayout->addWidget( mParamsTable );

                auto* paramBtnRow = new QHBoxLayout();
                mAddParamBtn = new QPushButton( tr( "Add parameter" ), paramsCard );
                mRemoveParamBtn = new QPushButton( tr( "Remove" ), paramsCard );
                paramBtnRow->addWidget( mAddParamBtn );
                paramBtnRow->addWidget( mRemoveParamBtn );
                paramBtnRow->addStretch( 1 );
                paramsLayout->addLayout( paramBtnRow );

                root->addWidget( paramsCard );

                // Refresh & row-matching options
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

                // Dialog footer buttons
                auto* footer = new QHBoxLayout();
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

                // Populate initial values
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
                        mDisplayNameEdit->setText( initialSource.displayName );
                        setParametersInTable( initialSource.parameters );
                        mRefreshOnOpenCheck->setChecked( initialSource.refreshOnOpen );
                        mRefreshBeforePrintCheck->setChecked( initialSource.refreshBeforePrint );

                        const int matchIdx = mMatchKeyCombo->findData( initialSource.matchKeyField );
                        if ( matchIdx >= 0 )
                        {
                                mMatchKeyCombo->setCurrentIndex( matchIdx );
                        }
                }
                else
                {
                        mRefreshOnOpenCheck->setChecked( true );
                        mRefreshBeforePrintCheck->setChecked( true );
                        if ( mServerCombo->count() == 0 )
                        {
                                mServerCombo->setCurrentText( QString() );
                        }
                }

                // Wire signals
                connect( mQuickUrlEdit, &QLineEdit::textChanged, this, &ExternalDataSourceDialog::onQuickUrlChanged );
                connect( mServerCombo, &QComboBox::currentTextChanged, this, &ExternalDataSourceDialog::onCredentialTargetChanged );
                connect( mQueryIdSpin, qOverload<int>( &QSpinBox::valueChanged ), this, &ExternalDataSourceDialog::onCredentialTargetChanged );
                connect( mAuthModeCombo, qOverload<int>( &QComboBox::currentIndexChanged ), this, &ExternalDataSourceDialog::onAuthModeChanged );
                connect( mShowKeyBtn, &QPushButton::toggled, this, &ExternalDataSourceDialog::onToggleShowKey );
                connect( mAddParamBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onAddParameter );
                connect( mRemoveParamBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onRemoveParameter );
                connect( mTestBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onTestConnection );
                connect( mUnlinkBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onUnlinkClicked );
                connect( cancelBtn, &QPushButton::clicked, this, &QDialog::reject );
                connect( mSaveBtn, &QPushButton::clicked, this, &ExternalDataSourceDialog::onAcceptClicked );

                connect( &mClient, &ExternalDataFetcher::progressChanged, this, [this]( const QString& msg ) {
                        updateStatus( msg, false );
                } );
                connect( &mClient, &ExternalDataFetcher::finished, this, [this]( const FetchResult& result ) {
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
                connect( &mClient, &ExternalDataFetcher::failed, this, [this]( const QString& err ) {
                        mTestBtn->setEnabled( true );
                        mHasTestedResult = false;
                        updateStatus( err, true );
                } );
                connect( &mClient, &RedashClient::queryInfoLoaded, this, [this]( const RedashQueryInfo& info ) {
                        if ( mDisplayNameEdit->text().trimmed().isEmpty() && !info.name.isEmpty() )
                        {
                                mDisplayNameEdit->setText( info.name );
                        }
                        if ( mParamsTable->rowCount() == 0 && !info.defaultParameters.isEmpty() )
                        {
                                setParametersInTable( info.defaultParameters );
                        }
                } );

                onAuthModeChanged();
                onCredentialTargetChanged();
        }


        model::ExternalDataSource ExternalDataSourceDialog::source() const
        {
                return mResultSource;
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
                if ( isQueryKey )
                {
                        mParamsTable->setToolTip( tr( "Redash requires a User API key for parameterized queries." ) );
                }
                else
                {
                        mParamsTable->setToolTip( QString() );
                }
                onCredentialTargetChanged();
        }


        void ExternalDataSourceDialog::onCredentialTargetChanged()
        {
                mHasTestedResult = false;
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


        void ExternalDataSourceDialog::onTestConnection()
        {
                const model::ExternalDataSource draft = buildSourceFromForm();
                if ( !draft.isValid() )
                {
                        updateStatus( tr( "Enter a valid HTTPS Redash server URL (or http://localhost) and query ID." ), true );
                        return;
                }

                const QString key = effectiveApiKey( draft );
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
                        mClient.fetchQueryInfo( draft.serverUrl, draft.queryId, key );
                }
                mClient.fetch( draft, key );
        }


        void ExternalDataSourceDialog::onUnlinkClicked()
        {
                mClient.cancel();
                mUnlinked = true;
                mResultSource = model::ExternalDataSource();
                accept();
        }


        void ExternalDataSourceDialog::onAcceptClicked()
        {
                const model::ExternalDataSource draft = buildSourceFromForm();
                if ( !draft.isValid() )
                {
                        QMessageBox::warning(
                                this,
                                tr( "Invalid Redash source" ),
                                tr( "Please enter a valid HTTPS Redash server URL (http://localhost is allowed for local testing) and a positive query ID." ) );
                        return;
                }

                const QString typedKey = mApiKeyEdit->text().trimmed();
                const QString key = effectiveApiKey( draft );
                if ( key.isEmpty() )
                {
                        QMessageBox::warning(
                                this,
                                tr( "API key required" ),
                                tr( "Please enter a Redash API key. It will be stored in %1." )
                                        .arg( SecretStore::backendDescription() ) );
                        return;
                }

                if ( !typedKey.isEmpty() )
                {
                        if ( !SecretStore::store( draft.credentialKey(), typedKey ) )
                        {
                                QMessageBox::warning(
                                        this,
                                        tr( "Could not store API key" ),
                                        tr( "Failed to save the API key to %1." )
                                                .arg( SecretStore::backendDescription() ) );
                                return;
                        }
                }

                model::Settings::addRecentRedashServer( draft.serverUrl );

                mClient.cancel();
                mUnlinked = false;
                mResultSource = draft;
                accept();
        }


        model::ExternalDataSource ExternalDataSourceDialog::buildSourceFromForm() const
        {
                model::ExternalDataSource src;
                src.kind = model::ExternalSourceKind::Redash;
                src.serverUrl = model::ExternalDataSource::normalizeServerUrl( mServerCombo->currentText() );
                src.queryId = mQueryIdSpin->value();
                src.displayName = mDisplayNameEdit->text().trimmed();
                src.refreshOnOpen = mRefreshOnOpenCheck->isChecked();
                src.refreshBeforePrint = mRefreshBeforePrintCheck->isChecked();
                src.matchKeyField = mMatchKeyCombo->currentData().toString();

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


        QString ExternalDataSourceDialog::effectiveApiKey( const model::ExternalDataSource& src ) const
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
