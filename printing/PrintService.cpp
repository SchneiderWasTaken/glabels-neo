//  PrintService.cpp
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

#include "PrintService.hpp"

#include "ZplPrinter.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPrinter>
#include <QPrinterInfo>
#include <QTemporaryFile>

#include <cmath>


namespace glabels::printing
{

        QString PrintService::stateToString( int printerState )
        {
                switch ( printerState )
                {
                case QPrinter::Idle:    return "idle";
                case QPrinter::Active:  return "active";
                case QPrinter::Aborted: return "aborted";
                case QPrinter::Error:   return "error";
                default:                return "unknown";
                }
        }


        QList<PrinterEntry> PrintService::printers()
        {
                QList<PrinterEntry> list;

                const QString defaultName = QPrinterInfo::defaultPrinterName();
                for ( const QPrinterInfo& info : QPrinterInfo::availablePrinters() )
                {
                        PrinterEntry entry;
                        entry.name      = info.printerName();
                        entry.isDefault = ( entry.name == defaultName );
                        entry.isZebra   = ZplPrinter::isZebraPrinter( entry.name );
                        entry.state     = stateToString( info.state() );
                        list.append( entry );
                }

                return list;
        }


        ResolvedPrinter PrintService::resolvePrinter( const QString& requested )
        {
                ResolvedPrinter result;

                if ( requested.compare( "default", Qt::CaseInsensitive ) == 0 )
                {
                        const QString defaultName = QPrinterInfo::defaultPrinterName();
                        if ( defaultName.isEmpty() )
                        {
                                result.status = ResolvedPrinter::Status::NoDefault;
                        }
                        else
                        {
                                result.status = ResolvedPrinter::Status::Found;
                                result.name   = defaultName;
                        }
                        return result;
                }

                const QStringList names = QPrinterInfo::availablePrinterNames();

                if ( names.contains( requested ) )
                {
                        result.status = ResolvedPrinter::Status::Found;
                        result.name   = requested;
                        return result;
                }

                QStringList matches;
                for ( const QString& name : names )
                {
                        if ( name.compare( requested, Qt::CaseInsensitive ) == 0 )
                        {
                                matches.append( name );
                        }
                }

                if ( matches.size() == 1 )
                {
                        result.status = ResolvedPrinter::Status::Found;
                        result.name   = matches.first();
                }
                else if ( matches.size() > 1 )
                {
                        result.status     = ResolvedPrinter::Status::Ambiguous;
                        result.candidates = matches;
                }
                else
                {
                        result.status = ResolvedPrinter::Status::NotFound;
                }

                return result;
        }


        bool PrintService::usesZpl( const QString& printerName, Driver driver )
        {
                switch ( driver )
                {
                case Driver::System: return false;
                case Driver::Zpl:    return true;
                default:             return ZplPrinter::isZebraPrinter( printerName );
                }
        }


        PrintResult PrintService::printToPrinter( const model::PageRenderer& renderer,
                                                  const QString&             printerName,
                                                  Driver                     driver )
        {
                PrintResult result;
                result.pages = renderer.nPages();

                const QPrinterInfo info = QPrinterInfo::printerInfo( printerName );
                if ( info.isNull() )
                {
                        result.error = QString( "printer '%1' is not installed" ).arg( printerName );
                        return result;
                }

                if ( usesZpl( printerName, driver ) )
                {
                        const PrinterProfile profile = ZplPrinter::ensureProfile( printerName );

                        QList<QImage> images;
                        const QRectF  pageRect = renderer.pageRect();
                        const int     w = static_cast<int>( std::lround( pageRect.width()  / 72.0 * profile.dpi ) );
                        const int     h = static_cast<int>( std::lround( pageRect.height() / 72.0 * profile.dpi ) );
                        if ( w <= 0 || h <= 0 )
                        {
                                result.error = "the label has no printable size";
                                return result;
                        }

                        for ( int iPage = 0; iPage < renderer.nPages(); iPage++ )
                        {
                                QImage image( w, h, QImage::Format_ARGB32 );
                                image.fill( Qt::white );

                                QPainter painter( &image );
                                painter.setRenderHint( QPainter::Antialiasing, true );
                                painter.setRenderHint( QPainter::TextAntialiasing, true );
                                painter.scale( profile.dpi / 72.0, profile.dpi / 72.0 );
                                renderer.printPage( &painter, iPage );
                                painter.end();

                                images.append( image );
                        }

                        QString error;
                        if ( ZplPrinter::send( ZplPrinter::imagesToZpl( images ), profile, &error ) )
                        {
                                result.ok = true;
                                return result;
                        }

                        // The GUI falls back to the system driver when ZPL cannot be delivered; so do we,
                        // but say so, because the result may not look the same.
                        result.warnings.append( QString( "ZPL delivery to '%1' failed (%2); used the system print driver instead" )
                                                .arg( printerName, error ) );
                }

                QPrinter printer( info, QPrinter::HighResolution );
                if ( !printer.isValid() )
                {
                        result.error = QString( "printer '%1' is not available" ).arg( printerName );
                        return result;
                }
                printer.setColorMode( QPrinter::Color );

                renderer.print( &printer );

                if ( printer.printerState() == QPrinter::Error || printer.printerState() == QPrinter::Aborted )
                {
                        result.error = QString( "printer '%1' reported an error while printing" ).arg( printerName );
                        return result;
                }

                result.ok = true;
                return result;
        }


        PrintResult PrintService::printToImages( const model::PageRenderer& renderer,
                                                 const QString&             fileName,
                                                 int                        dpi )
        {
                PrintResult result;
                result.pages = renderer.nPages();

                const QFileInfo info( fileName );
                if ( !info.absoluteDir().exists() )
                {
                        result.error = QString( "the folder for '%1' does not exist" ).arg( fileName );
                        return result;
                }

                const QRectF pageRect = renderer.pageRect();
                const int    w = static_cast<int>( std::lround( pageRect.width()  / 72.0 * dpi ) );
                const int    h = static_cast<int>( std::lround( pageRect.height() / 72.0 * dpi ) );
                if ( w <= 0 || h <= 0 || dpi <= 0 )
                {
                        result.error = "the label has no printable size";
                        return result;
                }

                for ( int iPage = 0; iPage < renderer.nPages(); iPage++ )
                {
                        QImage image( w, h, QImage::Format_ARGB32 );
                        image.fill( Qt::white );

                        QPainter painter( &image );
                        painter.setRenderHint( QPainter::Antialiasing, true );
                        painter.setRenderHint( QPainter::TextAntialiasing, true );
                        painter.scale( dpi / 72.0, dpi / 72.0 );
                        renderer.printPage( &painter, iPage );
                        painter.end();

                        QString path = fileName;
                        if ( renderer.nPages() > 1 )
                        {
                                path = info.absolutePath() + "/" + info.completeBaseName() + "-" + QString::number( iPage + 1 ) + "." + info.suffix();
                        }

                        if ( !image.save( path, "PNG" ) )
                        {
                                result.error = QString( "cannot write '%1'" ).arg( path );
                                return result;
                        }
                }

                result.ok = true;
                return result;
        }


        PrintResult PrintService::printToPdf( const model::PageRenderer& renderer,
                                              const QString&             fileName,
                                              QIODevice*                 standardOutput )
        {
                PrintResult result;
                result.pages = renderer.nPages();

                const bool toStdout = ( fileName == "-" );

                QTemporaryFile temp;
                QString        target = fileName;
                if ( toStdout )
                {
                        temp.setFileTemplate( QDir::tempPath() + "/glabelsneo-XXXXXX.pdf" );
                        if ( !temp.open() )
                        {
                                result.error = "cannot create a temporary file for PDF output";
                                return result;
                        }
                        target = temp.fileName();
                        temp.close();
                }
                else
                {
                        const QFileInfo info( fileName );
                        if ( !info.absoluteDir().exists() )
                        {
                                result.error = QString( "the folder for '%1' does not exist" ).arg( fileName );
                                return result;
                        }
                }

                {
                        QPrinter printer( QPrinter::HighResolution );
                        printer.setColorMode( QPrinter::Color );
                        printer.setOutputFormat( QPrinter::PdfFormat );
                        printer.setOutputFileName( target );

                        renderer.print( &printer );
                }

                const QFileInfo written( target );
                if ( !written.exists() || written.size() == 0 )
                {
                        result.error = QString( "cannot write '%1'" ).arg( fileName );
                        return result;
                }

                if ( toStdout )
                {
                        QFile pdf( target );
                        if ( !pdf.open( QIODevice::ReadOnly ) || standardOutput == nullptr )
                        {
                                result.error = "cannot copy the PDF to standard output";
                                return result;
                        }
                        standardOutput->write( pdf.readAll() );
                        standardOutput->waitForBytesWritten( -1 );
                }

                result.ok = true;
                return result;
        }

}
