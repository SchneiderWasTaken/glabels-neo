//  ZplRenderer.cpp
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
//  Printer detection, profiles, ZPL packing and the raw transports live in the
//  printing library (printing/ZplPrinter), shared with the glabelsneo command line
//  tool so both drive Zebra printers identically.  This file keeps the GUI parts:
//  the preview dialog and the entry point the Fill page calls.
//

#include "ZplRenderer.hpp"

#include "model/PageRenderer.hpp"

#include <QDialog>
#include <QFileDialog>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDebug>


namespace glabels
{


        // ================================================================
        // Detection
        // ================================================================

        bool ZplRenderer::isZebraPrinter( const QString& printerName )
        {
                return printing::ZplPrinter::isZebraPrinter( printerName );
        }


        int ZplRenderer::detectDpi( const QString& printerName )
        {
                return printing::ZplPrinter::detectDpi( printerName );
        }


        // ================================================================
        // Profile management (persisted in QSettings)
        // ================================================================

        PrinterProfile ZplRenderer::loadProfile( const QString& printerName )
        {
                return printing::ZplPrinter::loadProfile( printerName );
        }


        void ZplRenderer::saveProfile( const PrinterProfile& profile )
        {
                printing::ZplPrinter::saveProfile( profile );
        }


        PrinterProfile ZplRenderer::ensureProfile( const QString& printerName )
        {
                return printing::ZplPrinter::ensureProfile( printerName );
        }


        // ================================================================
        // Single entry point
        // ================================================================

        bool ZplRenderer::print( const model::Model* model,
                                 const QString& printerName,
                                 int copies,
                                 bool showPreview )
        {
                // Non-Zebra printers: use the normal QPrinter path.
                if ( !isZebraPrinter( printerName ) )
                {
                        return sendViaQPrinter( model, printerName, copies );
                }

                // Zebra printer: use ZPL.
                PrinterProfile profile = ensureProfile( printerName );

                if ( showPreview )
                {
                        return previewDialog( model, profile, copies );
                }

                // No preview -- just send.
                QByteArray zpl = renderZpl( model, profile.dpi, copies );
                if ( zpl.isEmpty() ) return false;

                if ( printing::ZplPrinter::send( zpl, profile ) )
                {
                        return true;
                }

                // ZPL failed -- fall back to QPrinter.
                qWarning() << "ZplRenderer: ZPL failed, falling back to QPrinter";
                return sendViaQPrinter( model, printerName, copies );
        }


        // ================================================================
        // ZPL rendering
        // ================================================================

        QByteArray ZplRenderer::renderZpl( const model::Model* model,
                                           int dpi, int copies )
        {
                double wInches = model->tmplate().pageWidth().in();
                double hInches = model->tmplate().pageHeight().in();
                int widthDots  = qRound( wInches * dpi );
                int heightDots = qRound( hInches * dpi );

                if ( widthDots <= 0 || heightDots <= 0 )
                {
                        qWarning() << "ZplRenderer: invalid dimensions" << wInches << hInches;
                        return QByteArray();
                }

                // Render to ARGB32 (QPainter can't render to 1-bit reliably).
                QImage image( widthDots, heightDots, QImage::Format_ARGB32 );
                image.fill( Qt::white );

                QPainter painter( &image );
                painter.setRenderHint( QPainter::Antialiasing, true );
                painter.setRenderHint( QPainter::TextAntialiasing, true );
                double scale = static_cast<double>(dpi) / 72.0;
                painter.scale( scale, scale );

                model::Variables variables( model->constVariables() );
                variables.resetVariables();
                model->draw( &painter, false, glabels::merge::Record(), variables );
                painter.end();

                // Threshold to monochrome, pack and wrap in ZPL (shared with the command line tool).
                return printing::ZplPrinter::imageToZpl( image, copies );
        }


        QImage ZplRenderer::renderPreview( const model::Model* model, int dpi )
        {
                double wInches = model->tmplate().pageWidth().in();
                double hInches = model->tmplate().pageHeight().in();
                int w = qRound( wInches * dpi );
                int h = qRound( hInches * dpi );
                if ( w <= 0 || h <= 0 ) return QImage();

                QImage image( w, h, QImage::Format_ARGB32 );
                image.fill( Qt::white );

                QPainter painter( &image );
                painter.setRenderHint( QPainter::Antialiasing, true );
                painter.setRenderHint( QPainter::TextAntialiasing, true );
                painter.scale( static_cast<double>(dpi) / 72.0,
                               static_cast<double>(dpi) / 72.0 );

                model::Variables vars( model->constVariables() );
                vars.resetVariables();
                model->draw( &painter, false, glabels::merge::Record(), vars );
                painter.end();
                return image;
        }


        // ================================================================
        // Preview dialog
        // ================================================================

        bool ZplRenderer::previewDialog( const model::Model* model,
                                          const PrinterProfile& profile,
                                          int copies )
        {
                QImage preview = renderPreview( model, profile.dpi );
                if ( preview.isNull() ) return false;

                QDialog dlg;
                dlg.setWindowTitle( "Thermal Printer Preview" );
                dlg.resize( 650, 750 );

                auto* layout = new QVBoxLayout( &dlg );

                auto* info = new QLabel(
                    QString("Printer: %1\nResolution: %2 dpi\nLabel: %3 x %4 dots\nCopies: %5")
                        .arg(profile.name)
                        .arg(profile.dpi)
                        .arg(preview.width())
                        .arg(preview.height())
                        .arg(copies) );
                info->setWordWrap( true );
                layout->addWidget( info );

                auto* scroll = new QScrollArea( &dlg );
                auto* imgLabel = new QLabel;
                imgLabel->setPixmap( QPixmap::fromImage( preview ) );
                scroll->setWidget( imgLabel );
                scroll->setWidgetResizable( true );
                layout->addWidget( scroll, 1 );

                // Transport options
                auto* transportGroup = new QGroupBox("Connection", &dlg);
                auto* tLayout = new QVBoxLayout( transportGroup );

                auto* usbRadio = new QRadioButton("USB (raw)", transportGroup);
                auto* tcpRadio = new QRadioButton("Network (TCP)", transportGroup);
                auto* tcpHostEdit = new QLineEdit(transportGroup);
                tcpHostEdit->setPlaceholderText("192.168.1.100");
                auto* tcpPortSpin = new QSpinBox(transportGroup);
                tcpPortSpin->setRange(1, 65535);
                tcpPortSpin->setValue(profile.tcpPort);

                auto* tcpRow = new QHBoxLayout;
                tcpRow->addWidget(new QLabel("Host:"));
                tcpRow->addWidget(tcpHostEdit, 1);
                tcpRow->addWidget(new QLabel("Port:"));
                tcpRow->addWidget(tcpPortSpin);

                tLayout->addWidget(usbRadio);
                tLayout->addWidget(tcpRadio);
                tLayout->addLayout(tcpRow);
                layout->addWidget(transportGroup);

                // Restore saved transport
                if ( profile.transport == "tcp" || !profile.tcpHost.isEmpty() )
                {
                        tcpRadio->setChecked(true);
                        tcpHostEdit->setText(profile.tcpHost);
                }
                else
                {
                        usbRadio->setChecked(true);
                }

                auto* btnRow = new QHBoxLayout;
                btnRow->addStretch();
                auto* sendBtn = new QPushButton("Send to Printer", &dlg);
                auto* cancelBtn = new QPushButton("Cancel", &dlg);
                btnRow->addWidget(sendBtn);
                btnRow->addWidget(cancelBtn);
                layout->addLayout(btnRow);

                bool sent = false;

                QObject::connect(sendBtn, &QPushButton::clicked, [&]() {
                        QByteArray zpl = renderZpl(model, profile.dpi, copies);
                        if (zpl.isEmpty()) return;

                        // Save the profile with the chosen transport.
                        PrinterProfile p = profile;
                        if (usbRadio->isChecked()) {
                                p.transport = "usb";
                        } else {
                                p.transport = "tcp";
                                p.tcpHost = tcpHostEdit->text();
                                p.tcpPort = tcpPortSpin->value();
                        }
                        saveProfile(p);

                        // Try the chosen transport.
                        if (p.transport == "usb") {
                                sent = sendUsb(zpl, p.name);
                        } else if (!p.tcpHost.isEmpty()) {
                                sent = sendTcp(zpl, p.tcpHost, p.tcpPort);
                        }

                        // If the chosen transport fails, try the other.
                        if (!sent) {
                                if (p.transport == "usb" && !p.tcpHost.isEmpty()) {
                                        sent = sendTcp(zpl, p.tcpHost, p.tcpPort);
                                } else {
                                        sent = sendUsb(zpl, p.name);
                                }
                        }

                        if (sent) dlg.accept();
                });
                QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

                dlg.exec();
                return sent;
        }


        // ================================================================
        // Transports
        // ================================================================

        bool ZplRenderer::sendTcp( const QByteArray& data,
                                   const QString& host, int port )
        {
                return printing::ZplPrinter::sendTcp( data, host, port );
        }


        bool ZplRenderer::sendUsb( const QByteArray& data,
                                   const QString& printerName )
        {
                return printing::ZplPrinter::sendUsb( data, printerName );
        }


        bool ZplRenderer::sendViaQPrinter( const model::Model* model,
                                            const QString& printerName,
                                            int copies )
        {
                auto info = QPrinterInfo::printerInfo( printerName );
                if ( info.isNull() ) return false;

                QPrinter printer( info, QPrinter::HighResolution );
                printer.setColorMode( QPrinter::Color );

                model::PageRenderer renderer( model );
                renderer.setNCopies( copies );
                renderer.setStartItem( 0 );
                renderer.print( &printer );
                return true;
        }


} // namespace glabels
