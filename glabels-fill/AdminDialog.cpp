//  AdminDialog.cpp
//

#include "AdminDialog.hpp"

#include "PrinterMonitor.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>


namespace glabels
{


        AdminDialog::AdminDialog( const FillConfig& cfg, QWidget* parent )
                : QDialog( parent ), mCfg( cfg )
        {
                setWindowTitle( tr("glabels Fill - Administration") );
                setModal( true );

                if ( mCfg.adminPin.isEmpty() )
                {
                        mPinOk = true;
                        buildSettingsPage();
                }
                else
                {
                        buildPinPage();
                }
        }


        void AdminDialog::buildPinPage()
        {
                auto* layout = new QVBoxLayout( this );

                auto* label = new QLabel( tr("Enter admin PIN:") );
                layout->addWidget( label );

                mPinEdit = new QLineEdit( this );
                mPinEdit->setEchoMode( QLineEdit::Password );
                layout->addWidget( mPinEdit );

                auto* btnRow = new QHBoxLayout();
                auto* okBtn = new QPushButton( tr("Unlock"), this );
                auto* cancelBtn = new QPushButton( tr("Cancel"), this );
                btnRow->addWidget( okBtn );
                btnRow->addWidget( cancelBtn );
                layout->addLayout( btnRow );

                connect( okBtn, &QPushButton::clicked, this, &AdminDialog::onVerifyPin );
                connect( cancelBtn, &QPushButton::clicked, this, &QDialog::reject );
                connect( mPinEdit, &QLineEdit::returnPressed, this, &AdminDialog::onVerifyPin );

                mPinEdit->setFocus();
        }


        void AdminDialog::buildSettingsPage()
        {
                auto* layout = new QVBoxLayout( this );

                // Label files
                layout->addWidget( new QLabel( tr("Label file(s):") ) );
                mLabelList = new QListWidget( this );
                mLabelList->addItems( mCfg.labelFiles );
                layout->addWidget( mLabelList );

                auto* labelRow = new QHBoxLayout();
                auto* addBtn = new QPushButton( tr("Add..."), this );
                auto* removeBtn = new QPushButton( tr("Remove"), this );
                labelRow->addWidget( addBtn );
                labelRow->addWidget( removeBtn );
                labelRow->addStretch();
                layout->addLayout( labelRow );

                // Printer
                auto* form = new QFormLayout();
                mPrinterEdit = new QLineEdit( mCfg.printerName, this );
                auto* printerBrowse = new QPushButton( tr("Browse..."), this );
                auto* printerRow = new QHBoxLayout();
                printerRow->addWidget( mPrinterEdit );
                printerRow->addWidget( printerBrowse );
                form->addRow( tr("Printer:"), printerRow );

                mAppNameEdit = new QLineEdit( mCfg.appName, this );
                form->addRow( tr("Application name:"), mAppNameEdit );

                mNewPinEdit = new QLineEdit( mCfg.adminPin, this );
                mNewPinEdit->setEchoMode( QLineEdit::Password );
                form->addRow( tr("Admin PIN (leave blank for none):"), mNewPinEdit );

                layout->addLayout( form );

                // Buttons
                auto* btnRow = new QHBoxLayout();
                auto* okBtn = new QPushButton( tr("Save"), this );
                auto* cancelBtn = new QPushButton( tr("Cancel"), this );
                btnRow->addStretch();
                btnRow->addWidget( okBtn );
                btnRow->addWidget( cancelBtn );
                layout->addLayout( btnRow );

                connect( addBtn, &QPushButton::clicked, this, &AdminDialog::onAddLabel );
                connect( removeBtn, &QPushButton::clicked, this, &AdminDialog::onRemoveLabel );
                connect( printerBrowse, &QPushButton::clicked, this, &AdminDialog::onBrowsePrinter );
                connect( okBtn, &QPushButton::clicked, this, &AdminDialog::onAccept );
                connect( cancelBtn, &QPushButton::clicked, this, &QDialog::reject );
        }


        void AdminDialog::onVerifyPin()
        {
                if ( mPinEdit->text() == mCfg.adminPin )
                {
                        mPinOk = true;
                        // Rebuild as the settings page.
                        QWidget::setLayout( new QVBoxLayout() ); // discard old
                        delete layout();
                        buildSettingsPage();
                }
                else
                {
                        QMessageBox::warning( this, tr("Wrong PIN"), tr("Incorrect PIN.") );
                        mPinEdit->clear();
                        mPinEdit->setFocus();
                }
        }


        void AdminDialog::onAddLabel()
        {
                QString path = QFileDialog::getOpenFileName(
                        this, tr("Select label file"), QString(),
                        tr("gLabels projects (*.glabels);;All files (*)") );
                if ( !path.isEmpty() )
                {
                        mLabelList->addItem( path );
                }
        }


        void AdminDialog::onRemoveLabel()
        {
                auto sel = mLabelList->selectedItems();
                for ( auto* item : sel )
                {
                        delete item;
                }
        }


        void AdminDialog::onBrowsePrinter()
        {
                // Reuse the full app's PrinterMonitor to enumerate installed printers.
                QStringList printers;
                // PrinterMonitor::instance() is available via glabels_lib.
                printers = PrinterMonitor::instance()->availablePrinters();
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
                        mPrinterEdit->setText(
                                chosen == tr("(System default)") ? QString() : chosen );
                }
        }


        void AdminDialog::onAccept()
        {
                mCfg.labelFiles.clear();
                for ( int i = 0; i < mLabelList->count(); ++i )
                {
                        mCfg.labelFiles << mLabelList->item( i )->text();
                }
                mCfg.printerName = mPrinterEdit->text();
                mCfg.appName = mAppNameEdit->text();
                mCfg.adminPin = mNewPinEdit->text();
                mCfg.isConfigured = true;
                accept();
        }


} // namespace glabels
