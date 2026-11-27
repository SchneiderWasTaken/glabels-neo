//  FillView.cpp
//
//  Copyright (C) 2014-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
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


#include "FillView.hpp"

#include "PrinterMonitor.hpp"
#include "ZplRenderer.hpp"

#include "datasource/ExternalDataSourceDialog.hpp"
#include "datasource/SecretStore.hpp"

#include "model/ColumnMatcher.hpp"
#include "model/FieldBinding.hpp"
#include "model/Model.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"

#include <QDebug>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QToolButton>

#ifndef GLABELS_FILL_NO_XLSX
#include <xlsxdocument.h>
#endif


namespace
{
        const QRegularExpression fieldTokenRegex( QStringLiteral(R"(\$\{([^}:]+)[}:])") );

        // Minimal RFC-4180 CSV line parser.
        QStringList parseCsvLine( const QString& line )
        {
                QStringList fields;
                QString cur;
                bool inQuotes = false;
                for ( int i = 0; i < line.size(); ++i )
                {
                        QChar c = line.at( i );
                        if ( inQuotes )
                        {
                                if ( c == '"' )
                                {
                                        if ( i + 1 < line.size() && line.at( i + 1 ) == '"' )
                                        {
                                                cur.append( '"' );
                                                ++i;
                                        }
                                        else
                                        {
                                                inQuotes = false;
                                        }
                                }
                                else
                                {
                                        cur.append( c );
                                }
                        }
                        else
                        {
                                if ( c == '"' )
                                {
                                        inQuotes = true;
                                }
                                else if ( c == ',' )
                                {
                                        fields.append( cur );
                                        cur.clear();
                                }
                                else
                                {
                                        cur.append( c );
                                }
                        }
                }
                fields.append( cur );
                return fields;
        }

        QString csvQuote( const QString& s )
        {
                if ( s.contains( ',' ) || s.contains( '"' ) || s.contains( '\n' ) || s.contains( '\r' ) )
                {
                        QString out = s;
                        out.replace( '"', "\"\"" );
                        return "\"" + out + "\"";
                }
                return s;
        }
}


namespace glabels
{


        ///
        /// Constructor
        ///
        FillView::FillView( QWidget *parent )
                : QWidget(parent)
        {
                setupUi( this );

                titleLabel->setText( tr("Fill") );
                titleLabel->setProperty( "role", QStringLiteral( "pageTitle" ) );
                productLabel->setProperty( "role", QStringLiteral( "caption" ) );
                productLabel->setWordWrap( false );
                printDescriptionLabel->setProperty( "role", QStringLiteral( "caption" ) );
                printDescriptionLabel->setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
                addRowButton->setText( tr("Add row") );

                // Compact icons on destination combo & standalone print button
                destinationCombo->setIconSize( QSize( 16, 16 ) );
                destinationCombo->setMinimumWidth( 220 );
                printButton->setIconSize( QSize( 16, 16 ) );
                printButton->setStyleSheet( QString() );

                // Rebuild FillView layout to match s12 mockup (works cleanly in both ribbon-hosted
                // and standalone kiosk modes, keeping every .ui widget and connection intact).
                line->hide();
                rootLayout->removeItem( leftLayout );
                rootLayout->removeWidget( line );
                rootLayout->removeItem( previewLayout );
                rootLayout->setContentsMargins( 24, 18, 24, 18 );
                rootLayout->setSpacing( 0 );

                leftLayout->removeWidget( titleLabel );
                leftLayout->removeWidget( productLabel );
                leftLayout->removeItem( toolbarLayout );
                leftLayout->removeItem( viewModeLayout );
                leftLayout->removeWidget( jobsTable );
                leftLayout->removeItem( destinationLayout );
                leftLayout->removeItem( startLayout );
                leftLayout->removeWidget( printButton );
                leftLayout->removeWidget( zplButton );
                leftLayout->removeWidget( printDescriptionLabel );
                leftLayout->removeWidget( systemDialogButton );
                previewLayout->removeWidget( preview );
                previewLayout->removeItem( pageNavLayout );

                auto* mainCol = new QVBoxLayout();
                mainCol->setContentsMargins( 0, 0, 0, 0 );
                mainCol->setSpacing( 12 );

                // Header row: "Fill" + product summary on left, "Preview:" + combo on right (s12)
                auto* headerRow = new QHBoxLayout();
                headerRow->setSpacing( 12 );
                headerRow->addWidget( titleLabel, 0, Qt::AlignVCenter );
                headerRow->addWidget( productLabel, 0, Qt::AlignVCenter );
                headerRow->addStretch( 1 );
                headerRow->addWidget( viewModeLabel, 0, Qt::AlignVCenter );
                headerRow->addWidget( viewModeCombo, 0, Qt::AlignVCenter );
                mainCol->addLayout( headerRow );

                // Inline toolbar row (only visible in standalone glabels-fill kiosk mode)
                linkGoogleSheetsButton = new QToolButton( this );
                linkGoogleSheetsButton->setText( tr("Google Sheets...") );
                linkGoogleSheetsButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                linkExternalSourceButton = new QToolButton( this );
                linkExternalSourceButton->setText( tr("Redash...") );
                linkExternalSourceButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                refreshExternalSourceButton = new QToolButton( this );
                refreshExternalSourceButton->setText( tr("Refresh") );
                refreshExternalSourceButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                refreshExternalSourceButton->setEnabled( false );
                const int spacerIdx = toolbarLayout->indexOf( toolbarSpacer );
                const int insertAt = ( spacerIdx >= 0 ) ? spacerIdx : toolbarLayout->count();
                toolbarLayout->insertWidget( insertAt, linkGoogleSheetsButton );
                toolbarLayout->insertWidget( insertAt + 1, linkExternalSourceButton );
                toolbarLayout->insertWidget( insertAt + 2, refreshExternalSourceButton );
                connect( linkGoogleSheetsButton, &QToolButton::clicked,
                         this, &FillView::onLinkGoogleSheetsClicked );
                connect( linkExternalSourceButton, &QToolButton::clicked,
                         this, &FillView::onLinkExternalSourceClicked );
                connect( refreshExternalSourceButton, &QToolButton::clicked,
                         this, &FillView::onRefreshExternalSourceClicked );
                mainCol->addLayout( toolbarLayout );

                // Linked external data source banner (hidden unless a live source is attached)
                mExternalBanner = new QFrame( this );
                mExternalBanner->setProperty( "role", QStringLiteral( "card" ) );
                mExternalBanner->setFrameShape( QFrame::StyledPanel );
                auto* bannerLayout = new QHBoxLayout( mExternalBanner );
                bannerLayout->setContentsMargins( 12, 8, 12, 8 );
                bannerLayout->setSpacing( 10 );

                auto* bannerTextCol = new QVBoxLayout();
                bannerTextCol->setContentsMargins( 0, 0, 0, 0 );
                bannerTextCol->setSpacing( 2 );
                mExternalTitleLabel = new QLabel( mExternalBanner );
                mExternalTitleLabel->setStyleSheet( QStringLiteral( "font-weight: 600;" ) );
                mExternalStatusLabel = new QLabel( mExternalBanner );
                mExternalStatusLabel->setProperty( "role", QStringLiteral( "caption" ) );
                bannerTextCol->addWidget( mExternalTitleLabel );
                bannerTextCol->addWidget( mExternalStatusLabel );
                bannerLayout->addLayout( bannerTextCol, 1 );

                mBannerRefreshBtn = new QPushButton( tr("Refresh"), mExternalBanner );
                mBannerConfigureBtn = new QPushButton( tr("Configure..."), mExternalBanner );
                mBannerUnlinkBtn = new QPushButton( tr("Unlink"), mExternalBanner );
                bannerLayout->addWidget( mBannerRefreshBtn );
                bannerLayout->addWidget( mBannerConfigureBtn );
                bannerLayout->addWidget( mBannerUnlinkBtn );
                mExternalBanner->hide();
                mainCol->addWidget( mExternalBanner );

                // Strip shown when the linked source has columns but some label fields have none.
                // The choice itself is made in the Data panel of the Home tab, next to the label.
                mUnmappedStrip = new QFrame( this );
                mUnmappedStrip->setProperty( "role", QStringLiteral( "card" ) );
                mUnmappedStrip->setFrameShape( QFrame::StyledPanel );
                auto* stripLayout = new QHBoxLayout( mUnmappedStrip );
                stripLayout->setContentsMargins( 12, 8, 12, 8 );
                stripLayout->setSpacing( 10 );
                mUnmappedLabel = new QLabel( mUnmappedStrip );
                mUnmappedLabel->setWordWrap( true );
                mUnmappedBtn = new QPushButton( tr("Choose columns..."), mUnmappedStrip );
                stripLayout->addWidget( mUnmappedLabel, 1 );
                stripLayout->addWidget( mUnmappedBtn );
                mUnmappedStrip->hide();
                mainCol->addWidget( mUnmappedStrip );
                connect( mUnmappedBtn, &QPushButton::clicked, this, &FillView::mapColumnsRequested );

                connect( mBannerRefreshBtn, &QPushButton::clicked,
                         this, &FillView::onRefreshExternalSourceClicked );
                connect( mBannerConfigureBtn, &QPushButton::clicked,
                         this, &FillView::onLinkExternalSourceClicked );
                connect( mBannerUnlinkBtn, &QPushButton::clicked,
                         this, &FillView::onUnlinkExternalSourceClicked );

                connect( &mRedashClient, &datasource::ExternalDataFetcher::started,
                         this, &FillView::onExternalFetchStarted );
                connect( &mRedashClient, &datasource::ExternalDataFetcher::progressChanged,
                         this, &FillView::onExternalFetchProgress );
                connect( &mRedashClient, &datasource::ExternalDataFetcher::finished,
                         this, &FillView::onExternalFetchFinished );
                connect( &mRedashClient, &datasource::ExternalDataFetcher::failed,
                         this, &FillView::onExternalFetchFailed );

                connect( &mGoogleSheetsClient, &datasource::ExternalDataFetcher::started,
                         this, &FillView::onExternalFetchStarted );
                connect( &mGoogleSheetsClient, &datasource::ExternalDataFetcher::progressChanged,
                         this, &FillView::onExternalFetchProgress );
                connect( &mGoogleSheetsClient, &datasource::ExternalDataFetcher::finished,
                         this, &FillView::onExternalFetchFinished );
                connect( &mGoogleSheetsClient, &datasource::ExternalDataFetcher::failed,
                         this, &FillView::onExternalFetchFailed );

                // Middle row: jobs table on left, Sheet preview card on workWell on right (s12)
                auto* middleRow = new QHBoxLayout();
                middleRow->setSpacing( 16 );
                middleRow->addWidget( jobsTable, 1 );

                auto* previewCard = new QGroupBox( tr("Sheet preview"), this );
                previewCard->setProperty( "role", QStringLiteral( "card" ) );
                previewCard->setFixedWidth( 360 );
                auto* cardLayout = new QVBoxLayout( previewCard );
                cardLayout->setContentsMargins( 0, 0, 0, 0 );
                cardLayout->setSpacing( 0 );

                auto* well = new QFrame( previewCard );
                well->setProperty( "role", QStringLiteral( "workWell" ) );
                well->setAttribute( Qt::WA_StyledBackground, true );
                auto* wellLayout = new QVBoxLayout( well );
                wellLayout->setContentsMargins( 16, 16, 16, 12 );
                wellLayout->setSpacing( 8 );
                preview->setParent( well );
                preview->setMinimumSize( 280, 320 );
                preview->setProperty( "role", QStringLiteral( "workWell" ) );
                wellLayout->addWidget( preview, 1 );
                pageNavLayout->setContentsMargins( 0, 0, 0, 4 );
                wellLayout->addLayout( pageNavLayout );
                cardLayout->addWidget( well, 1 );
                middleRow->addWidget( previewCard, 0 );
                mainCol->addLayout( middleRow, 1 );

                // Bottom row: Destination + Start at position + hint (+ standalone print buttons)
                auto* footerRow = new QHBoxLayout();
                footerRow->setSpacing( 10 );
                footerRow->addWidget( destinationLabel, 0, Qt::AlignVCenter );
                footerRow->addWidget( destinationCombo, 0, Qt::AlignVCenter );
                footerRow->addSpacing( 6 );
                footerRow->addWidget( startPositionLabel, 0, Qt::AlignVCenter );
                footerRow->addWidget( startPositionSpin, 0, Qt::AlignVCenter );
                footerRow->addSpacing( 8 );
                footerRow->addWidget( printDescriptionLabel, 1, Qt::AlignVCenter );
                footerRow->addWidget( printButton, 0, Qt::AlignVCenter );
                footerRow->addWidget( zplButton, 0, Qt::AlignVCenter );
                footerRow->addWidget( systemDialogButton, 0, Qt::AlignVCenter );
                mainCol->addLayout( footerRow );

                rootLayout->addLayout( mainCol, 1 );

                jobsTable->setModel( &mJobsModel );
                jobsTable->horizontalHeader()->setStretchLastSection( true );
                jobsTable->setColumnWidth( FillJobModel::QUANTITY_COLUMN, 64 );
                jobsTable->setColumnWidth( FillJobModel::PRINT_COLUMN, 70 );
                jobsTable->setSelectionBehavior( QAbstractItemView::SelectRows );

                viewModeCombo->setCurrentIndex( 1 ); // "Checked rows" by default

                auto* printerMonitor = PrinterMonitor::instance();
                loadDestinations( printerMonitor->availablePrinters() );
                connect( printerMonitor, SIGNAL(availablePrintersChanged(QStringList)),
                         this, SLOT(onAvailablePrintersChanged(QStringList)) );

                setDestination( model::Settings::recentPrinter() );

                preview->setRenderer( &mRenderer );

                connect( &mJobsModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                         this, SLOT(onJobsDataChanged()) );
                connect( &mJobsModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
                         this, SLOT(onJobsDataChanged()) );
                connect( &mJobsModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                         this, SLOT(onJobsDataChanged()) );
                connect( jobsTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                         this, SLOT(onSelectionChanged()) );
        }


        ///
        /// Destructor -- restore the model's merge if we still hold it.
        ///
        FillView::~FillView()
        {
                restoreMerge();
        }


        ///
        /// Set Model
        ///
        void FillView::setModel( model::Model* model )
        {
                if ( mModel )
                {
                        disconnect( mModel, nullptr, this, nullptr );
                }
                mRedashClient.cancel();
                mGoogleSheetsClient.cancel();
                mLastExternalSyncAt = QDateTime();
                mLastExternalRowCount = 0;
                mLastExternalError.clear();
                mExternalAutoLoaded = false;
                mAppliedColumnMap.clear();
                mBoundFields.clear();

                mModel = model;
                mRenderer.setModel( mModel );

                if ( mModel )
                {
                        connect( mModel, SIGNAL(changed()), this, SLOT(onModelChanged()) );
                        connect( mModel, &model::Model::externalDataSourceChanged,
                                 this, &FillView::onExternalSourceChanged );
                }

                onModelChanged();

                // In kiosk mode, auto-import rows from the configured source.
                if ( !mKioskSourcePath.isEmpty() )
                {
                        QStringList header;
                        auto rows = readSourceRows( mKioskSourcePath, &header );
                        importRows( rows, header );
                }

                // Always (re)apply locked fields -- must happen after
                // refreshFields()/importRows so the model has its columns
                // and rows populated.
                mJobsModel.setLockedFields( mKioskLockedFields );

                updateExternalSourceBanner();

                if ( mModel && mModel->externalDataSource().isValid()
                     && mModel->externalDataSource().refreshOnOpen )
                {
                        const auto& src = mModel->externalDataSource();
                        const QString credKey = src.credentialKey();
                        if ( credKey.isEmpty() || datasource::SecretStore::hasSecret( credKey ) )
                        {
                                mExternalAutoLoaded = true;
                                refreshExternalSource();
                        }
                }
        }


        ///
        /// Configure kiosk source pre-fill.
        ///
        void FillView::setKioskSource( const QString& path, const QStringList& lockedFields )
        {
                mKioskSourcePath = path;
                mKioskLockedFields = lockedFields;
                mJobsModel.setLockedFields( lockedFields );
        }


        void FillView::setRibbonHosted( bool hosted )
        {
                mRibbonHosted = hosted;
                const bool showInline = !mRibbonHosted;

                addRowButton->setVisible( showInline );
                removeRowButton->setVisible( showInline );
                clearButton->setVisible( showInline );
                importCsvButton->setVisible( showInline );
                importExcelButton->setVisible( showInline );
                exportCsvButton->setVisible( showInline );
                exportExcelButton->setVisible( showInline );
                if ( linkGoogleSheetsButton )
                {
                        linkGoogleSheetsButton->setVisible( showInline );
                }
                if ( linkExternalSourceButton )
                {
                        linkExternalSourceButton->setVisible( showInline );
                }
                if ( refreshExternalSourceButton )
                {
                        refreshExternalSourceButton->setVisible( showInline );
                }
                printButton->setVisible( showInline );
                zplButton->setVisible( showInline );
                systemDialogButton->setVisible( showInline );

                updateView();
        }


        bool FillView::isRibbonHosted() const
        {
                return mRibbonHosted;
        }


        ///
        /// Read all rows from a CSV or .xlsx file (by extension).
        ///
        QList<QMap<QString,QString>> FillView::readSourceRows( const QString& path,
                                                               QStringList* headerOut ) const
        {
                QList<QMap<QString,QString>> rows;
                if ( path.isEmpty() ) return rows;

                QFileInfo info( path );
#ifndef GLABELS_FILL_NO_XLSX
                if ( info.suffix().toLower() == "xlsx" )
                {
                        QXlsx::Document xlsx( path );
                        if ( !xlsx.load() )
                        {
                                return rows;
                        }
                        QXlsx::CellRange dim = xlsx.dimension();
                        int maxRow = dim.lastRow();
                        int maxCol = dim.lastColumn();
                        if ( maxRow < 1 || maxCol < 1 ) return rows;

                        QStringList header;
                        for ( int c = 1; c <= maxCol; ++c )
                        {
                                header.append( xlsx.read( 1, c ).toString() );
                        }
                        if ( headerOut ) *headerOut = header;

                        for ( int r = 2; r <= maxRow; ++r )
                        {
                                QMap<QString,QString> row;
                                for ( int c = 1; c <= maxCol; ++c )
                                {
                                        row.insert( header.at(c-1), xlsx.read( r, c ).toString() );
                                }
                                rows.append( row );
                        }
                }
                else
#endif // GLABELS_FILL_NO_XLSX
                {
                        // CSV (and anything else as CSV)
                        QFile file( path );
                        if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
                        {
                                return rows;
                        }
                        QStringList header;
                        bool first = true;
                        QTextStream in( &file );
                        while ( !in.atEnd() )
                        {
                                QStringList cells = parseCsvLine( in.readLine() );
                                if ( first )
                                {
                                        header = cells;
                                        first = false;
                                        if ( headerOut ) *headerOut = header;
                                        continue;
                                }
                                QMap<QString,QString> row;
                                for ( int i = 0; i < cells.size() && i < header.size(); ++i )
                                {
                                        row.insert( header.at(i), cells.at(i) );
                                }
                                rows.append( row );
                        }
                }
                return rows;
        }


        ///
        /// Page became visible -- take over the model's merge.
        ///
        void FillView::showEvent( QShowEvent* event )
        {
                QWidget::showEvent( event );
                if ( mModel )
                {
                        installFillMerge();
                        onFormChanged();
                }
        }


        ///
        /// Page hidden -- restore the model's original merge.
        ///
        void FillView::hideEvent( QHideEvent* event )
        {
                QWidget::hideEvent( event );
                restoreMerge();
        }


        ///
        /// Available printers changed handler
        ///
        void FillView::onAvailablePrintersChanged( QStringList printers )
        {
                auto saved = destinationCombo->currentText();
                loadDestinations( printers );
                setDestination( saved );
        }


        ///
        /// Model changed handler
        ///
        void FillView::onModelChanged()
        {
                if ( !mModel )
                {
                        return;
                }

                startPositionSpin->setRange( 1, mModel->frame()->nLabels() );

                productLabel->setText( tr("%1 · %2 per sheet")
                                       .arg( mModel->tmplate().name() )
                                       .arg( mModel->frame()->nLabels() ) );

                refreshFields();

                onFormChanged();
        }


        ///
        /// Update the field-name set of the jobs model when it changes.
        ///
        void FillView::refreshFields()
        {
                QStringList fields = collectFieldNames();
                if ( fields != mJobsModel.fieldNames() )
                {
                        mJobsModel.setFieldNames( fields );
                        // Ensure each existing job has all field keys.
                        auto jobs = mJobsModel.jobs();
                        for ( auto& job : jobs )
                        {
                                for ( const auto& f : fields )
                                {
                                        if ( !job.values.contains( f ) )
                                        {
                                                job.values.insert( f, QString() );
                                        }
                                }
                        }
                        mJobsModel.setJobs( jobs );
                        // Re-apply locked fields after the model reset so the
                        // view re-queries flags() with the correct lock state.
                        mJobsModel.setLockedFields( mKioskLockedFields );

                        // A field the label just started using may be fed by a column we already hold.
                        reapplySourceMapping();
                }
        }


        ///
        /// Fill the table's fields again from the rows the linked source last returned, using the
        /// column map.  Qty, Print and hand-typed values stay.
        ///
        void FillView::reapplySourceMapping()
        {
                if ( !mModel || !mModel->externalDataSource().isValid() || mModel->sourceRows().isEmpty() )
                {
                        return;
                }
                mAppliedColumnMap = mModel->externalDataSource().columnMap;
                mergeExternalRows( mModel->sourceRows(),
                                   mModel->sourceColumns(),
                                   mModel->externalDataSource().matchKeyField,
                                   true );
        }


        ///
        /// Tell the model what the table holds, so the editor can show a row on the label.
        ///
        void FillView::publishFillRows()
        {
                if ( !mModel )
                {
                        return;
                }
                QList<QMap<QString,QString>> rows;
                rows.reserve( mJobsModel.jobs().size() );
                for ( const auto& job : mJobsModel.jobs() )
                {
                        rows.append( job.values );
                }
                mModel->setFillRows( rows );
        }


        ///
        /// Show or hide the strip that asks for column choices.
        ///
        void FillView::updateUnmappedStrip()
        {
                if ( !mUnmappedStrip )
                {
                        return;
                }

                QStringList names;
                if ( mModel && mModel->externalDataSource().isValid() && !mModel->sourceColumns().isEmpty() )
                {
                        for ( const auto& b : model::FieldBindings::needingColumn( model::FieldBindings::compute( *mModel ) ) )
                        {
                                names.append( b.field );
                        }
                }

                // The kiosk operator cannot fix a mapping; the person who built the label does that.
                const bool show = mRibbonHosted && !names.isEmpty();
                if ( show )
                {
                        mUnmappedLabel->setText(
                                tr( "%n label field(s) have no column: %1", nullptr, names.size() )
                                        .arg( names.join( QStringLiteral( ", " ) ) ) );
                }
                mUnmappedStrip->setVisible( show );
        }


        ///
        /// Form changed -- rebuild the in-memory merge from the visible rows
        /// and refresh the preview.
        ///
        void FillView::onFormChanged()
        {
                if ( !mModel || mBlocked )
                {
                        return;
                }

                mBlocked = true;

                if ( mInstalled )
                {
                        auto* fm = dynamic_cast<FillMerge*>( mModel->merge() );
                        if ( fm )
                        {
                                QStringList fields = mJobsModel.fieldNames();
                                QList<merge::Record> records = buildRecords( visibleJobs() );
                                fm->setRecords( fields, records ); // triggers renderer refresh
                        }
                }

                mRenderer.setStartItem( startPositionSpin->value() - 1 );
                mRenderer.setIsCollated( true );
                mRenderer.setAreGroupsContiguous( true );
                mRenderer.setNCopies( 1 );

                updateView();
                publishFillRows();
                updateUnmappedStrip();

                mBlocked = false;
        }


        ///
        /// Update the summary / page navigation.
        ///
        void FillView::updateView()
        {
                int nItems = mRenderer.nItems();
                int nPages = mRenderer.nPages();

                QString desc;
                if ( nPages <= 1 )
                {
                        desc = tr("Will print %1 item(s) on 1 page.").arg(nItems);
                }
                else
                {
                        desc = tr("Will print %1 item(s) on %2 pages.").arg(nItems).arg(nPages);
                }
                if ( mRibbonHosted )
                {
                        desc += QStringLiteral( " " ) + tr("Print and Send to Zebra are on the ribbon.");
                }
                printDescriptionLabel->setText( desc );

                pageSpin->blockSignals( true );
                pageSpin->setRange( 1, qMax( 1, nPages ) );
                pageSpin->blockSignals( false );
                nPagesLabel->setText( QString::number( nPages ) );

                bool canPrint = mJobsModel.checkedQuantity() > 0;
                printButton->setEnabled( canPrint );
                systemDialogButton->setEnabled( canPrint );
                emit printAvailabilityChanged( canPrint );

                mRenderer.setIPage( pageSpin->value() - 1 );
        }


        ///
        /// Add a row
        ///
        void FillView::onAddRowClicked()
        {
                mJobsModel.appendJob();
        }


        ///
        /// Remove selected rows
        ///
        void FillView::onRemoveRowClicked()
        {
                auto selection = jobsTable->selectionModel()->selectedRows();
                std::sort( selection.begin(), selection.end(),
                           []( const QModelIndex& a, const QModelIndex& b ){ return a.row() > b.row(); } );
                for ( const auto& idx : selection )
                {
                        mJobsModel.removeRow( idx.row() );
                }
        }


        ///
        /// Clear all rows
        ///
        void FillView::onClearClicked()
        {
                mJobsModel.clear();
                onFormChanged();
        }


        ///
        /// View mode changed
        ///
        void FillView::onViewModeChanged()
        {
                onFormChanged();
        }


        ///
        /// Table selection changed -- only relevant in "selected row" mode.
        ///
        void FillView::onSelectionChanged()
        {
                if ( viewModeCombo->currentIndex() == 0 )
                {
                        onFormChanged();
                }
        }


        ///
        /// Jobs data changed -- refresh preview.
        ///
        void FillView::onJobsDataChanged()
        {
                onFormChanged();
        }


        ///
        /// Import CSV
        ///
        void FillView::onImportCsvClicked()
        {
                QString path = QFileDialog::getOpenFileName( this, tr("Import CSV"), QString(),
                                                             tr("CSV files (*.csv);;All files (*)") );
                if ( path.isEmpty() ) return;

                QFile file( path );
                if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
                {
                        QMessageBox::warning( this, tr("Import CSV"), tr("Could not open file %1.").arg(path) );
                        return;
                }

                QList<QMap<QString,QString>> rows;
                QStringList header;
                bool first = true;
                QTextStream in( &file );
                while ( !in.atEnd() )
                {
                        QString line = in.readLine();
                        QStringList cells = parseCsvLine( line );
                        if ( first )
                        {
                                header = cells;
                                first = false;
                                continue;
                        }
                        QMap<QString,QString> row;
                        for ( int i = 0; i < cells.size() && i < header.size(); ++i )
                        {
                                row.insert( header.at(i), cells.at(i) );
                        }
                        rows.append( row );
                }

                importRows( rows, header );
        }


        ///
        /// Import Excel (.xlsx)
        ///
        void FillView::onImportExcelClicked()
        {
#ifdef GLABELS_FILL_NO_XLSX
                QMessageBox::information( this, tr("Import Excel"),
                                          tr("Excel import is not available in this build. Use CSV instead.") );
#else
                QString path = QFileDialog::getOpenFileName( this, tr("Import Excel"), QString(),
                                                             tr("Excel files (*.xlsx);;All files (*)") );
                if ( path.isEmpty() ) return;

                QStringList header;
                QList<QMap<QString,QString>> rows = readSourceRows( path, &header );
                if ( rows.isEmpty() )
                {
                        QMessageBox::warning( this, tr("Import Excel"), tr("Could not read %1.").arg(path) );
                        return;
                }
                importRows( rows, header );
#endif
        }


        ///
        /// Export CSV
        ///
        void FillView::onExportCsvClicked()
        {
                QString path = QFileDialog::getSaveFileName( this, tr("Export CSV"), QString(),
                                                             tr("CSV files (*.csv)") );
                if ( path.isEmpty() ) return;

                QFile file( path );
                if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
                {
                        QMessageBox::warning( this, tr("Export CSV"), tr("Could not write %1.").arg(path) );
                        return;
                }

                QStringList header;
                header << "Qty" << "Print" << mJobsModel.fieldNames();

                QTextStream out( &file );
                out << header.join( ',' ) << "\n";

                for ( const auto& job : mJobsModel.jobs() )
                {
                        QStringList cells;
                        cells << QString::number( job.quantity )
                              << ( job.doPrint ? "1" : "0" );
                        for ( const auto& f : mJobsModel.fieldNames() )
                        {
                                cells << csvQuote( job.values.value( f ) );
                        }
                        out << cells.join( ',' ) << "\n";
                }
        }


        ///
        /// Export Excel (.xlsx)
        ///
        void FillView::onExportExcelClicked()
        {
#ifdef GLABELS_FILL_NO_XLSX
                QMessageBox::information( this, tr("Export Excel"),
                                          tr("Excel export is not available in this build. Use CSV instead.") );
#else
                QString path = QFileDialog::getSaveFileName( this, tr("Export Excel"), QString(),
                                                             tr("Excel files (*.xlsx)") );
                if ( path.isEmpty() ) return;

                QXlsx::Document xlsx;
                xlsx.write( 1, 1, "Qty" );
                xlsx.write( 1, 2, "Print" );
                int col = 3;
                for ( const auto& f : mJobsModel.fieldNames() )
                {
                        xlsx.write( 1, col++, f );
                }

                int row = 2;
                for ( const auto& job : mJobsModel.jobs() )
                {
                        xlsx.write( row, 1, job.quantity );
                        xlsx.write( row, 2, job.doPrint ? "1" : "0" );
                        int c = 3;
                        for ( const auto& f : mJobsModel.fieldNames() )
                        {
                                xlsx.write( row, c++, job.values.value( f ) );
                        }
                        ++row;
                }

                if ( !xlsx.saveAs( path ) )
                {
                        QMessageBox::warning( this, tr("Export Excel"), tr("Could not write %1.").arg(path) );
                }
#endif
        }


        ///
        /// Print -- sends the checked rows to the printer.
        ///
        void FillView::onPrintButtonClicked()
        {
                if ( !mModel ) return;
                if ( !ensureFreshExternalDataBeforePrint() ) return;

                installFillMerge();
                auto* fm = dynamic_cast<FillMerge*>( mModel->merge() );
                if ( !fm ) return;

                mBlocked = true;
                QStringList fields = mJobsModel.fieldNames();
                QList<merge::Record> records = buildRecords( checkedJobs() );
                fm->setRecords( fields, records );
                mRenderer.setStartItem( startPositionSpin->value() - 1 );
                mRenderer.setIsCollated( true );
                mRenderer.setAreGroupsContiguous( true );
                mRenderer.setNCopies( 1 );
                mBlocked = false;

                auto printerName = destinationCombo->currentText();

                // Zebra printers: auto-route through ZPL.
                if ( ZplRenderer::isZebraPrinter( printerName ) )
                {
                        ZplRenderer::print( mModel, printerName, 1, true );
                        model::Settings::setRecentPrinter( printerName );
                        onFormChanged();
                        return;
                }

                auto printerInfo = QPrinterInfo::printerInfo( printerName );
                bool isPrinter = !printerInfo.isNull();

                if ( isPrinter )
                {
                        QPrinter printer( printerInfo, QPrinter::HighResolution );
                        printer.setColorMode( QPrinter::Color );
                        mRenderer.print( &printer );
                        model::Settings::setRecentPrinter( printerName );
                }
                else
                {
                        QPrinter printer( QPrinter::HighResolution );
                        printer.setColorMode( QPrinter::Color );
                        QString fileName = QFileDialog::getSaveFileName( this, tr("Print to file (PDF)"),
                              defaultPdf(), tr("PDF files (*.pdf);;All files (*)"), nullptr,
                              QFileDialog::DontConfirmOverwrite );
                        if ( !fileName.isEmpty() )
                        {
                                printer.setOutputFileName( fileName );
                                printer.setOutputFormat( QPrinter::PdfFormat );
                                mRenderer.print( &printer );
                        }
                }

                onFormChanged(); // restore preview to the current view mode
        }


        ///
        /// System print dialog
        ///
        void FillView::onSystemDialogButtonClicked()
        {
                if ( !mModel ) return;
                if ( !ensureFreshExternalDataBeforePrint() ) return;

                installFillMerge();
                auto* fm = dynamic_cast<FillMerge*>( mModel->merge() );
                if ( !fm ) return;

                mBlocked = true;
                fm->setRecords( mJobsModel.fieldNames(), buildRecords( checkedJobs() ) );
                mRenderer.setStartItem( startPositionSpin->value() - 1 );
                mRenderer.setIsCollated( true );
                mRenderer.setAreGroupsContiguous( true );
                mRenderer.setNCopies( 1 );
                mBlocked = false;

                auto printerInfo = QPrinterInfo::printerInfo( destinationCombo->currentText() );
                QPrinter printer( printerInfo.isNull() ? QPrinterInfo() : printerInfo,
                                  QPrinter::HighResolution );
                printer.setColorMode( QPrinter::Color );

                QPrintDialog printDialog( &printer, this );
                printDialog.setOption( QAbstractPrintDialog::PrintToFile, true );
                printDialog.setOption( QAbstractPrintDialog::PrintShowPageSize, true );

                if ( printDialog.exec() == QDialog::Accepted )
                {
                        mRenderer.print( &printer );
                        if ( !printer.printerName().isEmpty() )
                        {
                                model::Settings::setRecentPrinter( printer.printerName() );
                        }
                }

                onFormChanged();
        }


        ///
        /// Send to Zebra -- bypasses the Windows print spooler and sends
        /// ZPL directly to the printer via raw TCP or USB.
        ///
        void FillView::onZplButtonClicked()
        {
                if ( !mModel ) return;
                if ( !ensureFreshExternalDataBeforePrint() ) return;

                installFillMerge();
                auto* fm = dynamic_cast<FillMerge*>( mModel->merge() );
                if ( !fm ) return;

                mBlocked = true;
                fm->setRecords( mJobsModel.fieldNames(), buildRecords( checkedJobs() ) );
                mBlocked = false;

                auto printerName = destinationCombo->currentText();
                if ( printerName == tr("Print to file (PDF)") )
                {
                        printerName.clear();
                }

                ZplRenderer::print( mModel, printerName, 1, true );
                onFormChanged();
        }
        void FillView::loadDestinations( const QStringList& printers )
        {
                destinationCombo->blockSignals( true );
                destinationCombo->clear();
                for ( auto& name : printers )
                {
                        destinationCombo->addItem( QIcon::fromTheme( "glabels-print" ), name );
                }
                if ( destinationCombo->count() )
                {
                        destinationCombo->insertSeparator( destinationCombo->count() );
                }
                destinationCombo->addItem( QIcon::fromTheme( "glabels-file-new" ), tr("Print to file (PDF)") );
                destinationCombo->blockSignals( false );
        }


        QString FillView::defaultPdf()
        {
                return mModel ? ( mModel->dirPath() + "/" + mModel->shortName() + ".pdf" ) : "output.pdf";
        }


        void FillView::setDestination( const QString& printerName )
        {
                destinationCombo->blockSignals( true );
                auto info = QPrinterInfo::printerInfo( printerName );
                if ( !info.isNull() )
                {
                        destinationCombo->setCurrentText( printerName );
                }
                else
                {
                        auto def = QPrinterInfo::defaultPrinterName();
                        if ( !QPrinterInfo::printerInfo( def ).isNull() )
                        {
                                destinationCombo->setCurrentText( def );
                        }
                        else
                        {
                                destinationCombo->setCurrentIndex( 0 );
                        }
                }
                destinationCombo->blockSignals( false );
        }


        ///
        /// Collect field names (model variables + ${...} tokens).
        ///
        QStringList FillView::collectFieldNames() const
        {
                // Variables plus every ${...} the label uses: text, barcode data, image names, colors.
                return mModel ? model::FieldBindings::fieldNames( *mModel ) : QStringList();
        }


        ///
        /// Visible jobs according to the current view mode.
        ///
        QList<FillJob> FillView::visibleJobs() const
        {
                int mode = viewModeCombo->currentIndex();
                if ( mode == 0 )
                {
                        // Selected row
                        auto sel = jobsTable->selectionModel()->selectedRows();
                        QList<FillJob> out;
                        for ( const auto& idx : sel )
                        {
                                const FillJob* j = mJobsModel.jobAt( idx.row() );
                                if ( j ) out.append( *j );
                        }
                        return out;
                }
                if ( mode == 1 )
                {
                        return checkedJobs();
                }
                return mJobsModel.jobs();
        }


        ///
        /// Checked jobs.
        ///
        QList<FillJob> FillView::checkedJobs() const
        {
                QList<FillJob> out;
                for ( const auto& j : mJobsModel.jobs() )
                {
                        if ( j.doPrint ) out.append( j );
                }
                return out;
        }


        ///
        /// Build merge records (each job repeated `quantity` times).
        ///
        QList<merge::Record> FillView::buildRecords( const QList<FillJob>& jobs ) const
        {
                QList<merge::Record> records;
                QStringList fields = mJobsModel.fieldNames();
                for ( const auto& job : jobs )
                {
                        merge::Record r;
                        for ( const auto& f : fields )
                        {
                                r[f] = job.values.value( f );
                        }
                        r.setSelected( true );
                        for ( int i = 0; i < job.quantity; ++i )
                        {
                                records.append( r );
                        }
                }
                return records;
        }


        ///
        /// Install our FillMerge on the model, saving the original to restore later.
        ///
        void FillView::installFillMerge()
        {
                if ( mInstalled || !mModel )
                {
                        return;
                }

                merge::Merge* current = mModel->merge();
                if ( current )
                {
                        mSavedMerge = current->clone();
                }
                mModel->setMerge( new FillMerge() );
                mInstalled = true;
        }


        ///
        /// Restore the model's original merge.
        ///
        void FillView::restoreMerge()
        {
                if ( !mInstalled || !mModel )
                {
                        mSavedMerge = nullptr;
                        mInstalled = false;
                        return;
                }
                if ( mSavedMerge )
                {
                        mModel->setMerge( mSavedMerge );
                        mSavedMerge = nullptr;
                }
                mInstalled = false;
        }


        ///
        /// Apply imported rows to the jobs model.
        ///
        void FillView::importRows( const QList<QMap<QString,QString>>& rows,
                                   const QStringList& header )
        {
                QList<FillJob> jobs;
                for ( const auto& row : rows )
                {
                        FillJob job;
                        job.quantity = row.value( "Qty", "1" ).toInt();
                        if ( job.quantity < 1 ) job.quantity = 1;
                        job.doPrint = ( row.value( "Print", "1" ) != "0" );
                        for ( const auto& f : mJobsModel.fieldNames() )
                        {
                                if ( row.contains( f ) )
                                {
                                        job.values.insert( f, row.value( f ) );
                                }
                        }
                        jobs.append( job );
                }
                mJobsModel.setJobs( jobs );
                onFormChanged();
        }


        void FillView::openExternalSourceDialog( model::ExternalSourceKind preferredKind )
        {
                if ( !mModel )
                {
                        return;
                }

                datasource::ExternalDataSourceDialog dlg(
                        mModel->externalDataSource(),
                        mJobsModel.fieldNames(),
                        this,
                        preferredKind );
                if ( dlg.exec() != QDialog::Accepted )
                {
                        return;
                }

                if ( dlg.wasUnlinked() )
                {
                        onUnlinkExternalSourceClicked();
                        return;
                }

                const model::ExternalDataSource src = dlg.source();
                mModel->setExternalDataSource( src );
                mLastExternalError.clear();

                if ( dlg.hasTestedResult() )
                {
                        applyExternalFetchResult( dlg.testedResult() );
                }
                else
                {
                        refreshExternalSource();
                }
        }


        void FillView::refreshExternalSource()
        {
                onRefreshExternalSourceClicked();
        }


        void FillView::applyExternalFetchResult( const datasource::FetchResult& result )
        {
                onExternalFetchFinished( result );
        }


        datasource::ExternalDataFetcher* FillView::activeFetcher()
        {
                if ( !mModel || !mModel->externalDataSource().isValid() )
                {
                        return nullptr;
                }
                switch ( mModel->externalDataSource().kind )
                {
                case model::ExternalSourceKind::Redash:
                        return &mRedashClient;
                case model::ExternalSourceKind::GoogleSheets:
                        return &mGoogleSheetsClient;
                case model::ExternalSourceKind::None:
                default:
                        return nullptr;
                }
        }


        bool FillView::isExternalFetchRunning() const
        {
                return mRedashClient.isRunning() || mGoogleSheetsClient.isRunning();
        }


        void FillView::onLinkExternalSourceClicked()
        {
                openExternalSourceDialog( model::ExternalSourceKind::Redash );
        }


        void FillView::onLinkGoogleSheetsClicked()
        {
                openExternalSourceDialog( model::ExternalSourceKind::GoogleSheets );
        }


        void FillView::onRefreshExternalSourceClicked()
        {
                auto* fetcher = activeFetcher();
                if ( !fetcher || !mModel )
                {
                        return;
                }
                fetcher->fetch( mModel->externalDataSource() );
        }


        void FillView::onUnlinkExternalSourceClicked()
        {
                if ( !mModel )
                {
                        return;
                }
                mRedashClient.cancel();
                mGoogleSheetsClient.cancel();
                mLastExternalSyncAt = QDateTime();
                mLastExternalRowCount = 0;
                mLastExternalError.clear();
                mModel->clearExternalDataSource();
                updateExternalSourceBanner();
        }


        void FillView::onExternalSourceChanged()
        {
                updateExternalSourceBanner();

                // A column choice changed (in the Data panel, or by undo): fill the table again.
                if ( mModel && mModel->externalDataSource().columnMap != mAppliedColumnMap )
                {
                        reapplySourceMapping();
                }
                updateUnmappedStrip();
        }


        void FillView::onExternalFetchStarted()
        {
                mLastExternalError.clear();
                updateExternalSourceBanner();
        }


        void FillView::onExternalFetchProgress( const QString& status )
        {
                if ( mExternalStatusLabel )
                {
                        mExternalStatusLabel->setText( status );
                        mExternalStatusLabel->setStyleSheet( QString() );
                }
        }


        void FillView::onExternalFetchFinished( const datasource::FetchResult& result )
        {
                mLastExternalSyncAt = result.fetchedAt.isValid() ? result.fetchedAt : QDateTime::currentDateTime();
                mLastExternalRowCount = result.rows.size();
                mLastExternalError.clear();

                if ( !mModel )
                {
                        updateExternalSourceBanner();
                        return;
                }

                // Keep the columns and rows: the Data panel lists the columns, and choosing a different
                // column for a field fills the table again without asking the source a second time.
                // Columns are NOT turned into label variables; the person decides what each one feeds.
                mModel->setSourceData( result.headers, result.rows );

                mAppliedColumnMap = mModel->externalDataSource().columnMap;
                mergeExternalRows( result.rows, result.headers, mModel->externalDataSource().matchKeyField );
                updateExternalSourceBanner();
        }


        void FillView::onExternalFetchFailed( const QString& error )
        {
                mLastExternalError = error;
                updateExternalSourceBanner();
        }


        void FillView::mergeExternalRows( const QList<QMap<QString,QString>>& rows,
                                          const QStringList&                  header,
                                          const QString&                      matchKeySetting,
                                          bool                                reapply )
        {
                const QStringList labelFields = mJobsModel.fieldNames();

                // Map each label field to the source column that fills it: the column the person
                // chose (columnMap), else the same name, ignoring case, spaces and punctuation, so
                // SQL snake_case matches ${SKU}.  Looser matches are only suggested in the Data
                // panel and never applied here.
                const QMap<QString,QString> columnMap = mModel ? mModel->externalDataSource().columnMap
                                                               : QMap<QString,QString>();
                QMap<QString,QString> fieldToHeader;
                for ( const QString& field : labelFields )
                {
                        const model::ColumnMatch m = model::ColumnMatcher::match( field, header, columnMap );
                        if ( m.isApplied() )
                        {
                                fieldToHeader.insert( field, m.column );
                        }
                }

                // Fields we filled last time but no longer fill must be emptied; fields the person
                // types by hand (never filled from the source) are left alone.
                const QSet<QString> previouslyBound = mBoundFields;
                mBoundFields = QSet<QString>( fieldToHeader.keyBegin(), fieldToHeader.keyEnd() );

                // Special optional Qty / Print columns from the query
                QString qtyHeader;
                QString printHeader;
                for ( const QString& h : header )
                {
                        if ( qtyHeader.isEmpty() && h.compare( QStringLiteral( "Qty" ), Qt::CaseInsensitive ) == 0 )
                        {
                                qtyHeader = h;
                        }
                        if ( printHeader.isEmpty() && h.compare( QStringLiteral( "Print" ), Qt::CaseInsensitive ) == 0 )
                        {
                                printHeader = h;
                        }
                }

                // Resolve match key to preserve Qty, Print, and non-source/unlocked fields across refreshes.
                QString matchField;
                if ( matchKeySetting != QStringLiteral( "__replace__" ) )
                {
                        if ( !matchKeySetting.isEmpty() && fieldToHeader.contains( matchKeySetting ) )
                        {
                                matchField = matchKeySetting;
                        }
                        else if ( matchKeySetting.isEmpty() )
                        {
                                for ( const QString& locked : mJobsModel.lockedFields() )
                                {
                                        if ( fieldToHeader.contains( locked ) )
                                        {
                                                matchField = locked;
                                                break;
                                        }
                                }
                                if ( matchField.isEmpty() )
                                {
                                        for ( const QString& f : labelFields )
                                        {
                                                if ( fieldToHeader.contains( f ) )
                                                {
                                                        matchField = f;
                                                        break;
                                                }
                                        }
                                }
                        }
                }

                QMap<QString,QList<FillJob>> existingByKey;
                if ( !matchField.isEmpty() && !mJobsModel.jobs().isEmpty() )
                {
                        for ( const auto& existing : mJobsModel.jobs() )
                        {
                                const QString keyVal = existing.values.value( matchField );
                                existingByKey[ keyVal ].append( existing );
                        }
                }

                // Same rows, new mapping: keep each row's Qty, Print and typed values by position.
                const QList<FillJob> existingJobs = mJobsModel.jobs();
                const bool keepByPosition = reapply && existingJobs.size() == rows.size();

                QList<FillJob> mergedJobs;
                mergedJobs.reserve( rows.size() );
                int rowIndex = -1;
                for ( const auto& row : rows )
                {
                        ++rowIndex;
                        FillJob job;
                        bool matchedExisting = false;

                        if ( keepByPosition )
                        {
                                job = existingJobs.at( rowIndex );
                                matchedExisting = true;
                        }
                        else if ( !matchField.isEmpty() )
                        {
                                const QString incomingKey = row.value( fieldToHeader.value( matchField ) );
                                auto it = existingByKey.find( incomingKey );
                                if ( it != existingByKey.end() && !it.value().isEmpty() )
                                {
                                        job = it.value().takeFirst();
                                        matchedExisting = true;
                                }
                        }

                        if ( !matchedExisting )
                        {
                                job.quantity = qtyHeader.isEmpty() ? 1 : row.value( qtyHeader, QStringLiteral( "1" ) ).toInt();
                                if ( job.quantity < 1 )
                                {
                                        job.quantity = 1;
                                }
                                if ( !printHeader.isEmpty() )
                                {
                                        const QString pv = row.value( printHeader, QStringLiteral( "1" ) ).trimmed().toLower();
                                        job.doPrint = ( pv != QStringLiteral( "0" ) && pv != QStringLiteral( "false" ) );
                                }
                                else
                                {
                                        job.doPrint = true;
                                }
                        }

                        for ( const QString& field : labelFields )
                        {
                                if ( fieldToHeader.contains( field ) )
                                {
                                        job.values.insert( field, row.value( fieldToHeader.value( field ) ) );
                                }
                                else if ( !job.values.contains( field ) || previouslyBound.contains( field ) )
                                {
                                        job.values.insert( field, QString() );
                                }
                        }

                        mergedJobs.append( job );
                }

                mJobsModel.setJobs( mergedJobs );
                onFormChanged();
        }


        bool FillView::ensureFreshExternalDataBeforePrint()
        {
                if ( !mModel )
                {
                        return true;
                }
                const model::ExternalDataSource& src = mModel->externalDataSource();
                if ( !src.isValid() || !src.refreshBeforePrint )
                {
                        return true;
                }

                // Avoid double-fetching if the user just clicked Refresh seconds ago.
                if ( mLastExternalSyncAt.isValid()
                     && mLastExternalSyncAt.secsTo( QDateTime::currentDateTime() ) < 5 )
                {
                        return true;
                }

                auto* fetcher = activeFetcher();
                if ( !fetcher )
                {
                        return true;
                }

                bool succeeded = false;
                bool canceled = false;
                QString errorMsg;

                QEventLoop loop;
                QProgressDialog progress(
                        tr( "Refreshing latest data from %1..." ).arg( src.summaryText() ),
                        tr( "Cancel" ),
                        0, 0,
                        this );
                progress.setWindowModality( Qt::WindowModal );
                progress.setMinimumDuration( 200 );

                auto cFinish = connect( fetcher, &datasource::ExternalDataFetcher::finished, &loop,
                                        [&]( const datasource::FetchResult& ) {
                                                succeeded = true;
                                                loop.quit();
                                        } );
                auto cFail = connect( fetcher, &datasource::ExternalDataFetcher::failed, &loop,
                                      [&]( const QString& err ) {
                                              errorMsg = err;
                                              loop.quit();
                                      } );
                auto cProg = connect( fetcher, &datasource::ExternalDataFetcher::progressChanged, &progress,
                                      [&]( const QString& msg ) {
                                              progress.setLabelText( msg );
                                      } );
                auto cCancel = connect( &progress, &QProgressDialog::canceled, &loop,
                                        [&]() {
                                                canceled = true;
                                                fetcher->cancel();
                                                loop.quit();
                                        } );

                if ( !fetcher->isRunning() )
                {
                        refreshExternalSource();
                }

                // refreshExternalSource() may fail synchronously (e.g. missing credentials) before loop.exec().
                if ( fetcher->isRunning() )
                {
                        loop.exec();
                }
                else if ( !succeeded && errorMsg.isEmpty() )
                {
                        errorMsg = mLastExternalError;
                }

                disconnect( cFinish );
                disconnect( cFail );
                disconnect( cProg );
                disconnect( cCancel );
                progress.reset();

                if ( canceled )
                {
                        return false;
                }
                if ( succeeded )
                {
                        return true;
                }

                if ( mJobsModel.checkedQuantity() > 0 )
                {
                        const auto choice = QMessageBox::warning(
                                this,
                                tr( "Refresh failed" ),
                                tr( "Could not refresh latest rows from %1:\n\n%2\n\nPrint the previously loaded rows instead?" )
                                        .arg( src.summaryText(), errorMsg ),
                                QMessageBox::Yes | QMessageBox::Cancel,
                                QMessageBox::Cancel );
                        return choice == QMessageBox::Yes;
                }

                QMessageBox::warning(
                        this,
                        tr( "Refresh failed" ),
                        tr( "Could not fetch rows from %1:\n\n%2" ).arg( src.summaryText(), errorMsg ) );
                return false;
        }


        void FillView::updateExternalSourceBanner()
        {
                const bool hasSource = mModel && mModel->externalDataSource().isValid();
                const bool running = isExternalFetchRunning();

                if ( mExternalBanner )
                {
                        mExternalBanner->setVisible( hasSource );
                }
                if ( refreshExternalSourceButton )
                {
                        refreshExternalSourceButton->setEnabled( hasSource && !running );
                }
                if ( mBannerRefreshBtn )
                {
                        mBannerRefreshBtn->setEnabled( hasSource && !running );
                }

                if ( hasSource )
                {
                        const model::ExternalDataSource& src = mModel->externalDataSource();
                        const QString credKey = src.credentialKey();
                        const bool missingCred = !credKey.isEmpty() && !datasource::SecretStore::hasSecret( credKey );
                        mExternalTitleLabel->setText( src.summaryText() );

                        if ( running )
                        {
                                mExternalStatusLabel->setText( tr( "Refreshing..." ) );
                                mExternalStatusLabel->setStyleSheet( QString() );
                        }
                        else if ( !mLastExternalError.isEmpty() )
                        {
                                mExternalStatusLabel->setText( mLastExternalError );
                                mExternalStatusLabel->setStyleSheet( QStringLiteral( "color: #d9534f;" ) );
                        }
                        else if ( missingCred )
                        {
                                mExternalStatusLabel->setText(
                                        src.kind == model::ExternalSourceKind::GoogleSheets
                                                ? tr( "Not signed in to Google on this computer — click Configure to sign in." )
                                                : tr( "API key not set on this computer — click Configure to enter your key." ) );
                                mExternalStatusLabel->setStyleSheet( QString() );
                        }
                        else if ( mLastExternalSyncAt.isValid() )
                        {
                                mExternalStatusLabel->setText(
                                        tr( "Synced at %1 · %2 row(s)%3" )
                                                .arg( mLastExternalSyncAt.toString( QStringLiteral( "HH:mm:ss" ) ) )
                                                .arg( mLastExternalRowCount )
                                                .arg( src.refreshBeforePrint
                                                              ? tr( " · Auto-refreshes before printing" )
                                                              : QString() ) );
                                mExternalStatusLabel->setStyleSheet( QString() );
                        }
                        else
                        {
                                mExternalStatusLabel->setText( tr( "Not yet synced — click Refresh to load rows." ) );
                                mExternalStatusLabel->setStyleSheet( QString() );
                        }
                }

                emit externalSourceStateChanged( hasSource, running );

                bool problem = false;
                if ( hasSource && !running )
                {
                        const QString credKey = mModel->externalDataSource().credentialKey();
                        problem = !mLastExternalError.isEmpty() ||
                                  ( !credKey.isEmpty() && !datasource::SecretStore::hasSecret( credKey ) );
                }
                emit externalStatusChanged( hasSource ? mExternalTitleLabel->text() : QString(),
                                            hasSource ? mExternalStatusLabel->text() : QString(),
                                            problem,
                                            running );
        }


} // namespace glabels
