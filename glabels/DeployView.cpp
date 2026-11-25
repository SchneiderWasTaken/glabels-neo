//  DeployView.cpp
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
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
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
                layout->setContentsMargins( 24, 20, 24, 20 );
                layout->setSpacing( 14 );

                auto* title = new QLabel( tr("Deploy"), this );
                title->setProperty( "role", QStringLiteral( "pageTitle" ) );
                layout->addWidget( title );

                auto* intro = new QLabel( tr(
                        "Build a standalone fill-and-print kiosk from the current project. "
                        "The employee gets a simplified Fill table and preview; the label layout "
                        "and locked fields are baked in."), this );
                intro->setWordWrap( true );
                intro->setProperty( "role", QStringLiteral( "caption" ) );
                layout->addWidget( intro );

                // Two side-by-side cards at natural height, top-aligned (SPEC 6.4 & s14 mockup: 640px + 340px)
                auto* cardsRow = new QHBoxLayout();
                cardsRow->setSpacing( 16 );
                cardsRow->setAlignment( Qt::AlignLeft | Qt::AlignTop );

                // Left card: Kiosk settings
                auto* settingsCard = new QGroupBox( tr("Kiosk settings"), this );
                settingsCard->setProperty( "role", QStringLiteral( "card" ) );
                settingsCard->setFixedWidth( 640 );
                settingsCard->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Maximum );
                auto* form = new QFormLayout( settingsCard );
                form->setContentsMargins( 16, 16, 16, 16 );
                form->setSpacing( 10 );

                mAppNameEdit = new QLineEdit( QStringLiteral( "gLabels Neo Fill" ), settingsCard );
                form->addRow( tr("Application name:"), mAppNameEdit );

                mLabelNameEdit = new QLineEdit( settingsCard );
                mLabelNameEdit->setPlaceholderText( tr("(e.g. \"Price Tag\")") );
                form->addRow( tr("Embedded label name:"), mLabelNameEdit );

                mFillExeEdit = new QLineEdit( settingsCard );
                mFillExeEdit->setPlaceholderText( tr("Path to kiosk base .exe") );
                QString appDir = QCoreApplication::applicationDirPath();
                QString sfxPath = appDir + "/glabels-fill-sfx.exe";
                QString dynPath = appDir + "/glabels-fill.exe";
                QString defaultFill = QFileInfo::exists( sfxPath ) ? sfxPath : dynPath;
                mFillExeEdit->setText( defaultFill );
                auto* fillBrowse = new QPushButton( tr("Browse..."), settingsCard );
                auto* fillRow = new QHBoxLayout();
                fillRow->setSpacing( 6 );
                fillRow->addWidget( mFillExeEdit );
                fillRow->addWidget( fillBrowse );
                form->addRow( tr("Base kiosk executable:"), fillRow );

                mSourcePathEdit = new QLineEdit( settingsCard );
                mSourcePathEdit->setPlaceholderText( tr("(optional: Excel/CSV to pre-fill rows)") );
                auto* sourceBrowse = new QPushButton( tr("Browse..."), settingsCard );
                auto* sourceRow = new QHBoxLayout();
                sourceRow->setSpacing( 6 );
                sourceRow->addWidget( mSourcePathEdit );
                sourceRow->addWidget( sourceBrowse );
                form->addRow( tr("Data source:"), sourceRow );

                mPrinterEdit = new QLineEdit( settingsCard );
                mPrinterEdit->setPlaceholderText( tr("(system default)") );
                auto* printerBrowse = new QPushButton( tr("Browse..."), settingsCard );
                auto* printerRow = new QHBoxLayout();
                printerRow->setSpacing( 6 );
                printerRow->addWidget( mPrinterEdit );
                printerRow->addWidget( printerBrowse );
                form->addRow( tr("Printer:"), printerRow );

                mPinEdit = new QLineEdit( settingsCard );
                mPinEdit->setEchoMode( QLineEdit::Password );
                mPinEdit->setPlaceholderText( tr("(blank = no PIN)") );
                form->addRow( tr("Admin PIN:"), mPinEdit );

                cardsRow->addWidget( settingsCard, 0, Qt::AlignTop );

                // Right card: Locked fields
                auto* lockedCard = new QGroupBox( tr("Locked fields"), this );
                lockedCard->setProperty( "role", QStringLiteral( "card" ) );
                lockedCard->setFixedWidth( 340 );
                lockedCard->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );
                auto* lockedLayout = new QVBoxLayout( lockedCard );
                lockedLayout->setContentsMargins( 16, 16, 16, 16 );
                lockedLayout->setSpacing( 8 );

                auto* lockedCaption = new QLabel(
                        tr("Pre-filled from data source; read-only to the employee."), lockedCard );
                lockedCaption->setWordWrap( true );
                lockedCaption->setProperty( "role", QStringLiteral( "caption" ) );
                lockedLayout->addWidget( lockedCaption );

                mLockedFieldsList = new QListWidget( lockedCard );
                mLockedFieldsList->setFixedHeight( 176 );
                lockedLayout->addWidget( mLockedFieldsList, 0 );

                cardsRow->addWidget( lockedCard, 0, Qt::AlignTop );
                cardsRow->addStretch( 1 );
                layout->addLayout( cardsRow, 0 );

                // Bottom status row (green status pill + detail text)
                auto* statusRow = new QHBoxLayout();
                statusRow->setSpacing( 10 );
                mStatusPill = new QLabel( tr("Ready to build"), this );
                mStatusPill->setProperty( "role", QStringLiteral( "pill" ) );
                mStatusPill->setProperty( "tone", QStringLiteral( "ok" ) );
                mStatusPill->setStyleSheet(
                        QStringLiteral( "padding: 4px 10px; border-radius: 10px; font-weight: 600;" ) );
                mStatusDetail = new QLabel(
                        tr("Use Build kiosk... on the Deploy tab to produce a single-file executable."), this );
                mStatusDetail->setProperty( "role", QStringLiteral( "caption" ) );
                statusRow->addWidget( mStatusPill, 0 );
                statusRow->addWidget( mStatusDetail, 1 );
                layout->addLayout( statusRow );
                layout->addStretch( 1 );

                // Hidden build button (triggered by the primary Build kiosk... button on the Deploy ribbon)
                mBuildButton = new QPushButton( tr("Build kiosk..."), this );
                mBuildButton->hide();

                connect( sourceBrowse, &QPushButton::clicked, this, &DeployView::onBrowseSource );
                connect( printerBrowse, &QPushButton::clicked, this, &DeployView::onBrowsePrinter );
                connect( fillBrowse, &QPushButton::clicked, this, &DeployView::onBrowseFillExe );
                connect( mFillExeEdit, &QLineEdit::textChanged, this, [this]() { updateStatusBanner(); } );
                connect( mBuildButton, &QPushButton::clicked, this, &DeployView::onBuild );

                updateStatusBanner();
        }


        void DeployView::setModel( model::Model* model )
        {
                mModel = model;
                refreshLockedFields();
                updateStatusBanner();
        }


        void DeployView::updateStatusBanner()
        {
                if ( !mStatusPill || !mStatusDetail )
                {
                        return;
                }
                const bool hasBase = QFileInfo::exists( mFillExeEdit->text() );
                if ( !mModel )
                {
                        mStatusPill->setText( tr("No project") );
                        mStatusPill->setProperty( "tone", QStringLiteral( "warn" ) );
                        mStatusDetail->setText( tr("Open or create a label project to build a kiosk.") );
                }
                else if ( !hasBase )
                {
                        mStatusPill->setText( tr("Base exe missing") );
                        mStatusPill->setProperty( "tone", QStringLiteral( "warn" ) );
                        mStatusDetail->setText( tr("Select glabels-fill.exe or glabels-fill-sfx.exe above.") );
                }
                else
                {
                        mStatusPill->setText( tr("Ready to build") );
                        mStatusPill->setProperty( "tone", QStringLiteral( "ok" ) );
                        mStatusDetail->setText(
                                tr("Use Build kiosk... on the Deploy tab to produce a single-file executable.") );
                }
                if ( auto* s = mStatusPill->style() )
                {
                        s->unpolish( mStatusPill );
                        s->polish( mStatusPill );
                }
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

                if ( mStatusPill && mStatusDetail )
                {
                        mStatusPill->setText( tr("Built") );
                        mStatusPill->setProperty( "tone", QStringLiteral( "ok" ) );
                        if ( auto* s = mStatusPill->style() )
                        {
                                s->unpolish( mStatusPill );
                                s->polish( mStatusPill );
                        }
                        mStatusDetail->setText( tr("Kiosk written to %1").arg( outPath ) );
                }
                QMessageBox::information( this, tr("Deploy"),
                                          tr("Kiosk built successfully:\n%1").arg(outPath) );
        }


} // namespace glabels
