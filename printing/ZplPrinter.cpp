//  ZplPrinter.cpp
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

#include "ZplPrinter.hpp"

#include <QDebug>
#include <QPageSize>
#include <QPrinter>
#include <QPrinterInfo>
#include <QSettings>
#include <QStringList>
#include <QTcpSocket>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winspool.h>
#endif


namespace
{

        ///
        /// One ^XA ... ^XZ block carrying the image as a monochrome ^GFA graphic
        ///
        QByteArray zplBlock( const QImage& source )
        {
                // QPainter output is thresholded here, so make sure we can read ARGB pixels.
                const QImage image = source.convertToFormat( QImage::Format_ARGB32 );

                const int widthDots  = image.width();
                const int heightDots = image.height();
                if ( widthDots <= 0 || heightDots <= 0 )
                {
                        return QByteArray();
                }

                // Threshold to monochrome and pack for ZPL (1 = black, MSB-first).
                const int bytesPerRow = ( widthDots + 7 ) / 8;
                const int totalBytes  = heightDots * bytesPerRow;
                QByteArray packed( totalBytes, 0 );

                for ( int y = 0; y < heightDots; ++y )
                {
                        const QRgb* scanline = reinterpret_cast<const QRgb*>( image.constScanLine( y ) );
                        for ( int x = 0; x < widthDots; ++x )
                        {
                                const QRgb px  = scanline[x];
                                const int  lum = ( qRed(px)*30 + qGreen(px)*59 + qBlue(px)*11 ) / 100;
                                if ( lum < 128 )
                                {
                                        packed[ y*bytesPerRow + x/8 ] |= static_cast<char>( 1 << ( 7 - ( x % 8 ) ) );
                                }
                        }
                }

                // Hex-encode.
                static const char hex[] = "0123456789ABCDEF";
                QByteArray hexData;
                hexData.reserve( totalBytes * 2 );
                for ( int i = 0; i < totalBytes; ++i )
                {
                        const unsigned char b = static_cast<unsigned char>( packed[i] );
                        hexData.append( hex[(b >> 4) & 0x0F] );
                        hexData.append( hex[b & 0x0F] );
                }

                QByteArray zpl;
                zpl.reserve( hexData.size() + 128 );
                zpl.append( "^XA" );
                zpl.append( "^MNN" );
                zpl.append( "^MTD" );
                zpl.append( "^PW" + QByteArray::number( widthDots ) );
                zpl.append( "^LL" + QByteArray::number( heightDots ) );
                zpl.append( "^FO0,0" );
                zpl.append( "^GFA," );
                zpl.append( QByteArray::number( totalBytes ) + "," );
                zpl.append( QByteArray::number( totalBytes ) + "," );
                zpl.append( QByteArray::number( bytesPerRow ) + "," );
                zpl.append( hexData );
                zpl.append( "^FS" );
                zpl.append( "^XZ" );
                return zpl;
        }


        void setError( QString* error, const QString& message )
        {
                if ( error )
                {
                        *error = message;
                }
        }

}


namespace glabels::printing
{

        // ================================================================
        // Detection
        // ================================================================

        bool ZplPrinter::isZebraPrinter( const QString& printerName )
        {
                if ( printerName.isEmpty() ) return false;
                QString name = printerName.toLower();

                // Name-based detection -- covers current and historical Zebra models.
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
                                int dpi = detectDpi( printerName );
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


        int ZplPrinter::detectDpi( const QString& printerName )
        {
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

        PrinterProfile ZplPrinter::loadProfile( const QString& printerName )
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


        void ZplPrinter::saveProfile( const PrinterProfile& profile )
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


        PrinterProfile ZplPrinter::ensureProfile( const QString& printerName )
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
        // ZPL generation
        // ================================================================

        QByteArray ZplPrinter::imageToZpl( const QImage& image, int copies )
        {
                const QByteArray block = zplBlock( image );
                if ( block.isEmpty() )
                {
                        return QByteArray();
                }

                QByteArray zpl;
                zpl.reserve( block.size() * qMax( copies, 1 ) );
                for ( int i = 0; i < copies; ++i )
                {
                        zpl.append( block );
                }
                return zpl;
        }


        QByteArray ZplPrinter::imagesToZpl( const QList<QImage>& images )
        {
                QByteArray zpl;
                for ( const QImage& image : images )
                {
                        zpl.append( zplBlock( image ) );
                }
                return zpl;
        }


        // ================================================================
        // Transports
        // ================================================================

        bool ZplPrinter::send( const QByteArray& zpl, const PrinterProfile& profile, QString* error )
        {
                QString lastError;

                if ( profile.transport == "tcp" || profile.transport == "auto" )
                {
                        if ( !profile.tcpHost.isEmpty() )
                        {
                                if ( sendTcp( zpl, profile.tcpHost, profile.tcpPort, &lastError ) )
                                {
                                        return true;
                                }
                        }
                        else if ( profile.transport == "tcp" )
                        {
                                lastError = "no network address is configured for this printer";
                        }
                }

                if ( profile.transport == "usb" || profile.transport == "auto" )
                {
                        if ( sendUsb( zpl, profile.name, &lastError ) )
                        {
                                return true;
                        }
                }

                setError( error, lastError.isEmpty() ? QString( "ZPL transport failed" ) : lastError );
                return false;
        }


        bool ZplPrinter::sendTcp( const QByteArray& data, const QString& host, int port, QString* error )
        {
                QTcpSocket socket;
                socket.connectToHost( host, port );
                if ( !socket.waitForConnected( 5000 ) )
                {
                        qWarning() << "ZPL TCP connect failed:" << socket.errorString();
                        setError( error, QString( "cannot connect to %1:%2 (%3)" ).arg( host ).arg( port ).arg( socket.errorString() ) );
                        return false;
                }
                socket.write( data );
                if ( !socket.waitForBytesWritten( 10000 ) )
                {
                        qWarning() << "ZPL TCP write failed:" << socket.errorString();
                        setError( error, QString( "writing to %1:%2 failed (%3)" ).arg( host ).arg( port ).arg( socket.errorString() ) );
                        return false;
                }
                socket.disconnectFromHost();
                if ( socket.state() != QAbstractSocket::UnconnectedState )
                {
                        socket.waitForDisconnected( 5000 );
                }
                return true;
        }


        bool ZplPrinter::sendUsb( const QByteArray& data, const QString& printerName, QString* error )
        {
#ifdef Q_OS_WIN
                HANDLE hPrinter = nullptr;
                if ( !OpenPrinterA( const_cast<char*>( printerName.toLocal8Bit().constData() ),
                                    &hPrinter, nullptr ) )
                {
                        qWarning() << "ZPL OpenPrinter failed for" << printerName;
                        setError( error, QString( "cannot open printer '%1'" ).arg( printerName ) );
                        return false;
                }

                DOC_INFO_1A docInfo = {};
                docInfo.pDocName = const_cast<char*>( "gLabels Neo ZPL" );
                docInfo.pDatatype = const_cast<char*>( "RAW" );

                if ( !StartDocPrinterA( hPrinter, 1, reinterpret_cast<BYTE*>( &docInfo ) ) )
                {
                        ClosePrinter( hPrinter );
                        setError( error, QString( "printer '%1' refused the job" ).arg( printerName ) );
                        return false;
                }
                StartPagePrinter( hPrinter );

                DWORD written = 0;
                bool ok = WritePrinter( hPrinter,
                                        const_cast<char*>( data.constData() ),
                                        static_cast<DWORD>( data.size() ), &written ) != 0;
                ok = ok && static_cast<int>( written ) == data.size();

                EndPagePrinter( hPrinter );
                EndDocPrinter( hPrinter );
                ClosePrinter( hPrinter );

                if ( !ok )
                {
                        setError( error, QString( "writing to printer '%1' failed" ).arg( printerName ) );
                }
                return ok;
#else
                Q_UNUSED( data );
                Q_UNUSED( printerName );
                setError( error, QString( "raw USB printing is only available on Windows" ) );
                return false;
#endif
        }

}
