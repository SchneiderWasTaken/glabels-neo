//  ZplRenderer.cpp
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
#include <QSettings>
#include <QSpinBox>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winspool.h>
#endif


namespace glabels
{


        // ================================================================
        // Detection
        // ================================================================

        bool ZplRenderer::isZebraPrinter( const QString& printerName )
        {
                if ( printerName.isEmpty() ) return false;
                QString name = printerName.toLower();

                // Name-based detection — covers current and historical Zebra models.
                static const QStringList patterns = {
                        "zebra", "zdesigner", "zpl", "gk420", "gk888", "gk", "zd4",
                        "zd6", "zt2", "zt4", "zt5", "zp5", "imx", "tc", "rx",
                        "203dpi", "300dpi", "600dpi"
                };
                for ( const auto& p : patterns )
                {
                        if ( name.contains( p ) ) return true;
                }

                // Driver-based detection via QPrinterInfo.
                auto info = QPrinterInfo::printerInfo( printerName );
                if ( !info.isNull() )
                {
                        QString make = info.makeAndModel().toLower();
                        if ( make.contains( "zebra" ) || make.contains( "zdesigner" ) ||
                             make.contains( "zpl" ) )
                        {
                                return true;
                        }

                        // Zebra drivers report exactly 1 page size and specific DPIs.
                        if ( info.supportedPageSizes().size() == 1 )
                        {
                                int dpi = detectDpi(printerName);
                                if ( dpi == 203 || dpi == 300 || dpi == 600 )
                                {
                                        // Likely a thermal printer with a fixed DPI.
                                        QString pageSizeName =
                                                info.supportedPageSizes().first().name().toLower();
                                        if ( pageSizeName.contains( "user" ) ||
                                             pageSizeName.contains( "custom" ) )
                                        {
                                                return true;
                                        }
                                }
                        }
                }
                return false;
        }


        int ZplRenderer::detectDpi( const QString& printerName )
        {
                // Try QPrinterInfo's resolution.
                auto info = QPrinterInfo::printerInfo( printerName );
                if ( !info.isNull() )
                {
                        // QPrinterInfo doesn't expose resolution directly, but
                        // constructing a QPrinter and reading resolution() works.
                        QPrinter printer( info, QPrinter::HighResolution );
                        int dpi = printer.resolution();
                        if ( dpi > 0 ) return dpi;
                }
                // Default: 203 dpi is the most common for entry-level Zebra.
                return 203;
        }


        // ================================================================
        // Profile management (persisted in QSettings)
        // ================================================================

        PrinterProfile ZplRenderer::loadProfile( const QString& printerName )
        {
                QSettings s;
                s.beginGroup( "ThermalPrinters" );
                s.beginGroup( printerName );
                PrinterProfile p;
                p.name      = printerName;
                p.dpi       = s.value( "dpi", 0 ).toInt();
                p.transport = s.value( "transport", "auto" ).toString();
                p.tcpHost   = s.value( "tcpHost" ).toString();
                p.tcpPort   = s.value( "tcpPort", 9100 ).toInt();
                s.endGroup();
                s.endGroup();
                return p;
        }


        void ZplRenderer::saveProfile( const PrinterProfile& profile )
        {
                QSettings s;
                s.beginGroup( "ThermalPrinters" );
                s.beginGroup( profile.name );
                s.setValue( "dpi", profile.dpi );
                s.setValue( "transport", profile.transport );
                s.setValue( "tcpHost", profile.tcpHost );
                s.setValue( "tcpPort", profile.tcpPort );
                s.endGroup();
                s.endGroup();
        }


        PrinterProfile ZplRenderer::ensureProfile( const QString& printerName )
        {
                PrinterProfile p = loadProfile( printerName );
                if ( p.dpi == 0 )
                {
                        p.dpi = detectDpi( printerName );
                        saveProfile( p );
                }
                return p;
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

                // No preview — just send.
                QByteArray zpl = renderZpl( model, profile.dpi, copies );
                if ( zpl.isEmpty() ) return false;

                if ( profile.transport == "tcp" || profile.transport == "auto" )
                {
                        if ( !profile.tcpHost.isEmpty() )
                        {
                                if ( sendTcp( zpl, profile.tcpHost, profile.tcpPort ) )
                                {
                                        return true;
                                }
                        }
                }

                if ( profile.transport == "usb" || profile.transport == "auto" )
                {
                        if ( sendUsb( zpl, printerName ) )
                        {
                                return true;
                        }
                }

                // ZPL failed — fall back to QPrinter.
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

                // Threshold to monochrome and pack for ZPL (1 = black, MSB-first).
                int bytesPerRow = ( widthDots + 7 ) / 8;
                QByteArray packed( heightDots * bytesPerRow, 0 );

                for ( int y = 0; y < heightDots; ++y )
                {
                        const QRgb* scanline =
                                reinterpret_cast<const QRgb*>( image.constScanLine( y ) );
                        for ( int x = 0; x < widthDots; ++x )
                        {
                                QRgb px = scanline[x];
                                int lum = ( qRed(px)*30 + qGreen(px)*59 + qBlue(px)*11 ) / 100;
                                if ( lum < 128 )
                                {
                                        int byteIdx = y * bytesPerRow + x / 8;
                                        int bitIdx  = 7 - ( x % 8 );
                                        packed[byteIdx] |= ( 1 << bitIdx );
                                }
                        }
                }

                // Hex-encode.
                int totalBytes = heightDots * bytesPerRow;
                static const char hex[] = "0123456789ABCDEF";
                QByteArray hexData;
                hexData.reserve( totalBytes * 2 );
                for ( int i = 0; i < totalBytes; ++i )
                {
                        unsigned char b = static_cast<unsigned char>( packed[i] );
                        hexData.append( hex[(b >> 4) & 0x0F] );
                        hexData.append( hex[b & 0x0F] );
                }

                // Build ZPL.
                QByteArray zpl;
                zpl.reserve( hexData.size() + 128 );
                for ( int i = 0; i < copies; ++i )
                {
                        zpl.append( "^XA" );
                        zpl.append( "^MNN" );
                        zpl.append( "^MTD" );
                        zpl.append( "^PW" + QByteArray::number(widthDots) );
                        zpl.append( "^LL" + QByteArray::number(heightDots) );
                        zpl.append( "^FO0,0" );
                        zpl.append( "^GFA," );
                        zpl.append( QByteArray::number(totalBytes) + "," );
                        zpl.append( QByteArray::number(totalBytes) + "," );
                        zpl.append( QByteArray::number(bytesPerRow) + "," );
                        zpl.append( hexData );
                        zpl.append( "^FS" );
                        zpl.append( "^XZ" );
                }
                return zpl;
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
                QTcpSocket socket;
                socket.connectToHost( host, port );
                if ( !socket.waitForConnected( 5000 ) )
                {
                        qWarning() << "ZPL TCP connect failed:" << socket.errorString();
                        return false;
                }
                socket.write( data );
                if ( !socket.waitForBytesWritten( 10000 ) )
                {
                        qWarning() << "ZPL TCP write failed:" << socket.errorString();
                        return false;
                }
                socket.disconnectFromHost();
                return true;
        }


        bool ZplRenderer::sendUsb( const QByteArray& data,
                                   const QString& printerName )
        {
#ifdef Q_OS_WIN
                HANDLE hPrinter = nullptr;
                if ( !OpenPrinterA( const_cast<char*>(printerName.toLocal8Bit().constData()),
                                    &hPrinter, nullptr ) )
                {
                        qWarning() << "ZPL OpenPrinter failed for" << printerName;
                        return false;
                }

                DOC_INFO_1A docInfo = {};
                docInfo.pDocName = const_cast<char*>("gLabels ZPL");
                docInfo.pDatatype = const_cast<char*>("RAW");

                if ( !StartDocPrinterA(hPrinter, 1, reinterpret_cast<BYTE*>(&docInfo)) )
                {
                        ClosePrinter(hPrinter);
                        return false;
                }
                StartPagePrinter(hPrinter);

                DWORD written = 0;
                bool ok = WritePrinter(hPrinter,
                                       const_cast<char*>(data.constData()),
                                       data.size(), &written) != 0;
                ok = ok && (int)written == data.size();

                EndPagePrinter(hPrinter);
                EndDocPrinter(hPrinter);
                ClosePrinter(hPrinter);
                return ok;
#else
                Q_UNUSED(data); Q_UNUSED(printerName);
                return false;
#endif
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
