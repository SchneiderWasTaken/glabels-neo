//  FillView.hpp
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

#ifndef FillView_hpp
#define FillView_hpp


#include "FillJobModel.hpp"
#include "FillMerge.hpp"

#include "datasource/GoogleSheetsClient.hpp"
#include "datasource/RedashClient.hpp"

#include "ui_FillView.h"

#include "model/ExternalDataSource.hpp"
#include "model/Model.hpp"
#include "model/PageRenderer.hpp"
#include "merge/Merge.hpp"

#include <QDateTime>
#include <QList>
#include <QStringList>


class QFrame;
class QLabel;
class QPushButton;
class QToolButton;


namespace glabels
{


        ///
        /// Fill View Widget
        ///
        /// An interactive fill-and-print page.  The left side is a table of
        /// "fill jobs" -- one row per batch -- with a Quantity column, a
        /// Print checkbox column, and one column per label field (model
        /// variables and ${...} placeholders discovered in the label text).
        /// The right side is a live sheet preview that reflects the
        /// selected view mode (selected row / checked rows / all rows).
        ///
        /// CSV and Excel (.xlsx) import/export round-trip the jobs table.
        ///
        class FillView : public QWidget, public Ui_FillView
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                FillView( QWidget *parent = nullptr );
                virtual ~FillView();


                /////////////////////////////////
                // Public methods
                /////////////////////////////////
        public:
                void setModel( model::Model* model );

                // Kiosk mode: pre-fill locked columns from an Excel/CSV source
                // and make those columns read-only.  Empty path = disabled.
                void setKioskSource( const QString& path, const QStringList& lockedFields );

                /// When true (hosted inside MainWindow under Data > Fill), row/IO/output
                /// actions live on the ribbon instead of inline toolbars.
                /// Defaults to false so standalone glabels-fill kiosk keeps its inline controls.
                void setRibbonHosted( bool hosted );
                bool isRibbonHosted() const;

                // External data source actions (mirrored on the Data > Fill ribbon)
                QToolButton* linkExternalSourceButton{ nullptr };
                QToolButton* linkGoogleSheetsButton{ nullptr };
                QToolButton* refreshExternalSourceButton{ nullptr };

                void openExternalSourceDialog(
                        model::ExternalSourceKind preferredKind = model::ExternalSourceKind::Redash );
                void refreshExternalSource();
                void applyExternalFetchResult( const datasource::FetchResult& result );
                datasource::RedashClient* redashClient() { return &mRedashClient; }
                datasource::GoogleSheetsClient* googleSheetsClient() { return &mGoogleSheetsClient; }

        signals:
                void printAvailabilityChanged( bool canPrint );
                void externalSourceStateChanged( bool hasSource, bool isRefreshing );


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        private slots:
                void onAvailablePrintersChanged( QStringList printers );
                void onModelChanged();
                void onFormChanged();
                void updateView();

                void onAddRowClicked();
                void onRemoveRowClicked();
                void onClearClicked();

                void onImportCsvClicked();
                void onImportExcelClicked();
                void onExportCsvClicked();
                void onExportExcelClicked();

                void onViewModeChanged();
                void onSelectionChanged();
                void onJobsDataChanged();

                void onPrintButtonClicked();
                void onSystemDialogButtonClicked();
                void onZplButtonClicked();

                void onLinkExternalSourceClicked();
                void onLinkGoogleSheetsClicked();
                void onRefreshExternalSourceClicked();
                void onUnlinkExternalSourceClicked();
                void onExternalSourceChanged();
                void onExternalFetchStarted();
                void onExternalFetchProgress( const QString& status );
                void onExternalFetchFinished( const glabels::datasource::FetchResult& result );
                void onExternalFetchFailed( const QString& error );


                /////////////////////////////////
                // Protected (page visibility)
                /////////////////////////////////
        protected:
                void showEvent( QShowEvent* event ) override;
                void hideEvent( QHideEvent* event ) override;


                /////////////////////////////////
                // Private methods
                /////////////////////////////////
        private:
                void loadDestinations( const QStringList& printers );
                QString defaultPdf();
                void setDestination( const QString& printerName );

                QStringList collectFieldNames() const;
                void refreshFields();

                // Read all rows from a CSV or .xlsx source (by extension).
                QList<QMap<QString,QString>> readSourceRows( const QString& path,
                                                             QStringList* headerOut = nullptr ) const;

                QList<FillJob> visibleJobs() const;
                QList<merge::Record> buildRecords( const QList<FillJob>& jobs ) const;
                QList<FillJob> checkedJobs() const;

                void installFillMerge();
                void restoreMerge();

                void importRows( const QList<QMap<QString,QString>>& rows,
                                 const QStringList& header );
                void mergeExternalRows( const QList<QMap<QString,QString>>& rows,
                                        const QStringList&                  header,
                                        const QString&                      matchKeySetting );
                bool ensureFreshExternalDataBeforePrint();
                void updateExternalSourceBanner();
                datasource::ExternalDataFetcher* activeFetcher();
                bool isExternalFetchRunning() const;
                QList<QMap<QString,QString>> exportRows( QStringList* headerOut = nullptr ) const;


                /////////////////////////////////
                // Private Data
                /////////////////////////////////
        private:
                model::Model*                  mModel{ nullptr };
                model::PageRenderer            mRenderer;
                FillJobModel                   mJobsModel;
                FillMerge                      mFillMerge;
                datasource::RedashClient       mRedashClient;
                datasource::GoogleSheetsClient mGoogleSheetsClient;

                QString                  mKioskSourcePath;
                QStringList              mKioskLockedFields;

                QFrame*                  mExternalBanner{ nullptr };
                QLabel*                  mExternalTitleLabel{ nullptr };
                QLabel*                  mExternalStatusLabel{ nullptr };
                QPushButton*             mBannerRefreshBtn{ nullptr };
                QPushButton*             mBannerConfigureBtn{ nullptr };
                QPushButton*             mBannerUnlinkBtn{ nullptr };

                QDateTime                mLastExternalSyncAt;
                int                      mLastExternalRowCount{ 0 };
                QString                  mLastExternalError;
                bool                     mExternalAutoLoaded{ false };

                // The merge the model had before we took over (restored on hide).
                merge::Merge*            mSavedMerge{ nullptr };
                bool                     mBlocked{ false };
                bool                     mInstalled{ false };
                bool                     mRibbonHosted{ false };
        };


}


#endif // FillView_hpp
