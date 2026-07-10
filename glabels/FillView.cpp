//  FillView.cpp
//
//  Copyright (C) 2014-2026  Jaye Evins <evins@snaught.com>
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

#include "model/Model.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QTextStream>

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

                titleLabel->setText( QString( "<span style='font-size:18pt;'>%1</span>" ).arg( tr("Fill") ) );

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
                mModel = model;
                mRenderer.setModel( mModel );

                connect( mModel, SIGNAL(changed()), this, SLOT(onModelChanged()) );

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

                productLabel->setText( tr("Product: %1  (%2 per sheet)")
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
                }
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

                mBlocked = false;
        }


        ///
        /// Update the summary / page navigation.
        ///
        void FillView::updateView()
        {
                int nItems = mRenderer.nItems();
                int nPages = mRenderer.nPages();

                if ( nPages <= 1 )
                {
                        printDescriptionLabel->setText( tr("Will print %1 item(s) on 1 page.").arg(nItems) );
                }
                else
                {
                        printDescriptionLabel->setText( tr("Will print %1 item(s) on %2 pages.").arg(nItems).arg(nPages) );
                }

                pageSpin->blockSignals( true );
                pageSpin->setRange( 1, qMax( 1, nPages ) );
                pageSpin->blockSignals( false );
                nPagesLabel->setText( QString::number( nPages ) );

                bool canPrint = mJobsModel.checkedQuantity() > 0;
                printButton->setEnabled( canPrint );
                systemDialogButton->setEnabled( canPrint );

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
                QSet<QString> names;
                if ( mModel )
                {
                        for ( const auto& var : mModel->constVariables() )
                        {
                                names.insert( var.name() );
                        }
                        for ( auto* obj : mModel->objectList() )
                        {
                                auto* textObj = dynamic_cast<model::ModelTextObject*>( obj );
                                if ( textObj )
                                {
                                        auto it = fieldTokenRegex.globalMatch( textObj->text() );
                                        while ( it.hasNext() )
                                        {
                                                names.insert( it.next().captured(1).trimmed() );
                                        }
                                }
                        }
                }
                QStringList list = names.values();
                list.sort();
                return list;
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


} // namespace glabels
