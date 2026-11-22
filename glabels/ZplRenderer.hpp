//  ZplRenderer.hpp
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


//  ZplRenderer.hpp
//
//  Graphics-mode ZPL renderer for Zebra / ZDesigner thermal printers.
//
//  Auto-detects DPI from the printer driver, tries USB raw then TCP,
//  falls back to QPrinter if ZPL fails.  Printer profiles (DPI, IP, etc.)
//  are persisted in QSettings so the user only configures once.
//

#ifndef ZplRenderer_hpp
#define ZplRenderer_hpp


#include "model/Model.hpp"

#include <QByteArray>
#include <QImage>
#include <QString>


namespace glabels
{


        ///
        /// A thermal printer profile, persisted in QSettings.
        ///
        struct PrinterProfile
        {
                QString  name;          // Windows printer name
                int      dpi{ 0 };     // 0 = auto-detect from driver
                QString  transport;    // "auto" | "usb" | "tcp"
                QString  tcpHost;      // for "tcp" transport
                int      tcpPort{ 9100 };
        };


        class ZplRenderer
        {
        public:
                /// ---- Single entry point ----

                /// Print the model.  Auto-detects Zebra printers and routes
                /// through ZPL; falls back to QPrinter for non-Zebra.
                /// Returns true if the print was sent.
                static bool print( const model::Model* model,
                                   const QString& printerName,
                                   int copies = 1,
                                   bool showPreview = true );


                /// ---- Detection ----

                /// Is this printer a Zebra / ZDesigner / ZPL-capable printer?
                static bool isZebraPrinter( const QString& printerName );

                /// Auto-detect the DPI of a printer from its driver.
                static int detectDpi( const QString& printerName );


                /// ---- Profile management (persisted in QSettings) ----

                static PrinterProfile loadProfile( const QString& printerName );
                static void saveProfile( const PrinterProfile& profile );
                static PrinterProfile ensureProfile( const QString& printerName );


                /// ---- Low-level rendering (public for testing) ----

                static QByteArray renderZpl( const model::Model* model,
                                            int dpi, int copies );
                static QImage renderPreview( const model::Model* model, int dpi );


        private:
                static bool sendUsb( const QByteArray& data,
                                     const QString& printerName );
                static bool sendTcp( const QByteArray& data,
                                     const QString& host, int port );
                static bool sendViaQPrinter( const model::Model* model,
                                             const QString& printerName,
                                             int copies );

                static bool previewDialog( const model::Model* model,
                                           const PrinterProfile& profile,
                                           int copies );
        };


}


#endif // ZplRenderer_hpp
