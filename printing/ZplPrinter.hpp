//  ZplPrinter.hpp
//
//  Zebra / ZPL support without any user interface: printer detection, persisted
//  printer profiles, raster-to-ZPL conversion and the raw transports (USB spooler
//  RAW datatype on Windows, TCP port 9100).
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

#ifndef printing_ZplPrinter_hpp
#define printing_ZplPrinter_hpp


#include <QByteArray>
#include <QImage>
#include <QString>


namespace glabels::printing
{

        ///
        /// A thermal printer profile, persisted in QSettings.
        ///
        struct PrinterProfile
        {
                QString  name;               // Operating system printer name
                int      dpi{ 0 };           // 0 = auto-detect from driver
                QString  transport;          // "auto" | "usb" | "tcp"
                QString  tcpHost;            // for "tcp" transport
                int      tcpPort{ 9100 };
        };


        ///
        /// Zebra / ZPL helpers (static, no UI)
        ///
        class ZplPrinter
        {
        public:
                /// Is this printer a Zebra / ZDesigner / ZPL-capable printer?
                static bool isZebraPrinter( const QString& printerName );

                /// Auto-detect the DPI of a printer from its driver.
                static int detectDpi( const QString& printerName );

                /// Profile management (persisted in QSettings).
                static PrinterProfile loadProfile( const QString& printerName );
                static void saveProfile( const PrinterProfile& profile );
                static PrinterProfile ensureProfile( const QString& printerName );

                /// Convert a rendered label image to ZPL (monochrome ^GFA graphic), repeated 'copies' times.
                static QByteArray imageToZpl( const QImage& image, int copies = 1 );

                /// Convert several rendered label images to ZPL, one ^XA...^XZ block per image.
                static QByteArray imagesToZpl( const QList<QImage>& images );

                /// Send ZPL using the transport named by the profile ("auto" tries TCP, then USB).
                /// On failure returns false and (when 'error' is given) says why.
                static bool send( const QByteArray& zpl,
                                  const PrinterProfile& profile,
                                  QString* error = nullptr );

                static bool sendTcp( const QByteArray& data,
                                     const QString& host,
                                     int port,
                                     QString* error = nullptr );

                static bool sendUsb( const QByteArray& data,
                                     const QString& printerName,
                                     QString* error = nullptr );
        };

}


#endif // printing_ZplPrinter_hpp
