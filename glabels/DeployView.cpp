//  DeployView.cpp
//

#include "DeployView.hpp"

#include "FillConfig.hpp"
#include "PrinterMonitor.hpp"

#include "model/Model.hpp"
#include "model/ModelTextObject.hpp"
#include "model/XmlLabelCreator.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>


namespace
{
        const QRegularExpression fieldTokenRegex( QStringLiteral(R"(\$\{([^}:]+)[}:])") );
}


namespace glabels
{


        DeployView::DeployView( QWidget* parent )
                : QWidget( parent )
        {
                auto* layout = new QVBoxLayout( this );
                layout->setContentsMargins( 12, 12, 12, 12 );
                layout->setSpacing( 9 );

                auto* title = new QLabel(
                        QString( "<span style='font-size:18pt;'>%1</span>" ).arg( tr("Deploy") ) );
                layout->addWidget( title );

                auto* intro = new QLabel( tr(
                        "Build a standalone fill-and-print kiosk from the current project. "
                        "The employee gets a simplified Fill table + preview; the label layout "
                        "and locked fields are baked in.") );
                intro->setWordWrap( true );
                layout->addWidget( intro );

                auto* form = new QFormLayout();

                mAppNameEdit = new QLineEdit( "glabels Fill", this );
                form->addRow( tr("Application name:"), mAppNameEdit );

                mPinEdit = new QLineEdit( this );
                mPinEdit->setEchoMode( QLineEdit::Password );
                mPinEdit->setPlaceholderText( tr("(blank = no PIN)") );
                form->addRow( tr("Admin PIN:"), mPinEdit );

                mPrinterEdit = new QLineEdit( this );
                mPrinterEdit->setPlaceholderText( tr("(system default)") );
                auto* printerBrowse = new QPushButton( tr("Browse..."), this );
                auto* printerRow = new QHBoxLayout();
                printerRow->addWidget( mPrinterEdit );
                printerRow->addWidget( printerBrowse );
                form->addRow( tr("Printer:"), printerRow );

                mSourcePathEdit = new QLineEdit( this );
                mSourcePathEdit->setPlaceholderText( tr("(optional: Excel/CSV to pre-fill rows)") );
                auto* sourceBrowse = new QPushButton( tr("Browse..."), this );
                auto* sourceRow = new QHBoxLayout();
                sourceRow->addWidget( mSourcePathEdit );
                sourceRow->addWidget( sourceBrowse );
                form->addRow( tr("Data source:"), sourceRow );

                mLabelNameEdit = new QLineEdit( this );
                mLabelNameEdit->setPlaceholderText( tr("(e.g. \"Price Tag\")") );
                form->addRow( tr("Embedded label name:"), mLabelNameEdit );

                layout->addLayout( form );

                // Locked fields
                layout->addWidget( new QLabel( tr("Locked fields (pre-filled from source, read-only to employee):") ) );
                mLockedFieldsList = new QListWidget( this );
                mLockedFieldsList->setMaximumHeight( 120 );
                layout->addWidget( mLockedFieldsList );

                // Kiosk exe source
                auto* fillExeForm = new QFormLayout();
                mFillExeEdit = new QLineEdit( this );
                mFillExeEdit->setPlaceholderText( tr("Path to glabels-fill.exe") );
                // Default: look next to this exe.
                QString defaultFill = QCoreApplication::applicationDirPath() + "/glabels-fill.exe";
                mFillExeEdit->setText( defaultFill );
                auto* fillBrowse = new QPushButton( tr("Browse..."), this );
                auto* fillRow = new QHBoxLayout();
                fillRow->addWidget( mFillExeEdit );
                fillRow->addWidget( fillBrowse );
                fillExeForm->addRow( tr("Base kiosk executable:"), fillRow );
                layout->addLayout( fillExeForm );

                // Build button
                auto* btnRow = new QHBoxLayout();
                btnRow->addStretch();
                mBuildButton = new QPushButton( tr("Build kiosk..."), this );
                mBuildButton->setIcon( QIcon::fromTheme( "glabels-fill" ) );
                mBuildButton->setIconSize( QSize(32,32) );
                mBuildButton->setStyleSheet( "padding:6px;" );
                btnRow->addWidget( mBuildButton );
                layout->addLayout( btnRow );

                layout->addStretch();

                connect( sourceBrowse, &QPushButton::clicked, this, &DeployView::onBrowseSource );
                connect( printerBrowse, &QPushButton::clicked, this, &DeployView::onBrowsePrinter );
                connect( fillBrowse, &QPushButton::clicked, this, &DeployView::onBrowseFillExe );
                connect( mBuildButton, &QPushButton::clicked, this, &DeployView::onBuild );
        }


        void DeployView::setModel( model::Model* model )
        {
                mModel = model;
                refreshLockedFields();
        }


        QStringList DeployView::collectFieldNames() const
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


        void DeployView::refreshLockedFields()
        {
                mLockedFieldsList->clear();
                for ( const auto& f : collectFieldNames() )
                {
                        auto* item = new QListWidgetItem( f, mLockedFieldsList );
                        item->setCheckState( Qt::Unchecked );
                }
        }


        void DeployView::onBrowseSource()
        {
                QString path = QFileDialog::getOpenFileName( this, tr("Select data source"), QString(),
                              tr("Excel/CSV (*.xlsx *.csv);;All files (*)") );
                if ( !path.isEmpty() ) mSourcePathEdit->setText( path );
        }


        void DeployView::onBrowsePrinter()
        {
                QStringList printers = PrinterMonitor::instance()->availablePrinters();
                printers.prepend( tr("(System default)") );

                QDialog dlg( this );
                dlg.setWindowTitle( tr("Select printer") );
                auto* l = new QVBoxLayout( &dlg );
                auto* combo = new QComboBox( &dlg );
                combo->addItems( printers );
                l->addWidget( combo );
                auto* ok = new QPushButton( tr("OK"), &dlg );
                l->addWidget( ok );
                connect( ok, &QPushButton::clicked, &dlg, &QDialog::accept );
                if ( dlg.exec() == QDialog::Accepted )
                {
                        QString chosen = combo->currentText();
                        mPrinterEdit->setText( chosen == tr("(System default)") ? QString() : chosen );
                }
        }


        void DeployView::onBrowseFillExe()
        {
                QString path = QFileDialog::getOpenFileName( this, tr("Select glabels-fill.exe"), QString(),
                              tr("Executables (*.exe);;All files (*)") );
                if ( !path.isEmpty() ) mFillExeEdit->setText( path );
        }


        void DeployView::onBuild()
        {
                if ( !mModel )
                {
                        QMessageBox::warning( this, tr("Deploy"), tr("No project is open.") );
                        return;
                }

                QString baseExe = mFillExeEdit->text();
                if ( !QFileInfo::exists( baseExe ) )
                {
                        QMessageBox::warning( this, tr("Deploy"),
                                              tr("Base kiosk executable not found:\n%1\n\n"
                                                 "Build the glabels-fill target first.").arg(baseExe) );
                        return;
                }

                QString outPath = QFileDialog::getSaveFileName( this, tr("Save kiosk as"), QString(),
                                  tr("Executable (*.exe)") );
                if ( outPath.isEmpty() ) return;

                // Serialize the current project.
                QByteArray labelXml;
                model::XmlLabelCreator::writeBuffer( mModel, labelXml );

                // Assemble config.
                FillConfig cfg;
                cfg.appName = mAppNameEdit->text();
                cfg.adminPin = mPinEdit->text();
                cfg.printerName = mPrinterEdit->text();
                cfg.sourcePath = mSourcePathEdit->text();

                QStringList locked;
                for ( int i = 0; i < mLockedFieldsList->count(); ++i )
                {
                        auto* item = mLockedFieldsList->item( i );
                        if ( item->checkState() == Qt::Checked )
                        {
                                locked << item->text();
                        }
                }
                cfg.lockedFields = locked;

                EmbeddedLabel lbl;
                lbl.name = mLabelNameEdit->text().isEmpty()
                           ? mModel->shortName()
                           : mLabelNameEdit->text();
                lbl.xml = labelXml;
                cfg.embeddedLabels.append( lbl );
                cfg.isConfigured = true;

                // Copy the base exe to the output path.
                if ( !QFile::copy( baseExe, outPath ) )
                {
                        QMessageBox::warning( this, tr("Deploy"),
                                              tr("Could not copy %1 to %2.").arg(baseExe, outPath) );
                        return;
                }

                // Write the config trailer onto the copy.
                if ( !FillConfigIO::writeToExe( outPath, cfg ) )
                {
                        QMessageBox::warning( this, tr("Deploy"),
                                              tr("Could not write config to %1.").arg(outPath) );
                        return;
                }

                QMessageBox::information( this, tr("Deploy"),
                                          tr("Kiosk built successfully:\n%1").arg(outPath) );
        }


} // namespace glabels
