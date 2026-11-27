//  PrintService.hpp
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

#ifndef printing_PrintService_hpp
#define printing_PrintService_hpp


#include "model/PageRenderer.hpp"

#include <QIODevice>
#include <QList>
#include <QString>
#include <QStringList>


namespace glabels::printing
{

        ///
        /// How a printer is driven.
        ///
        enum class Driver
        {
                Auto,    ///< Zebra printers use ZPL, everything else the system driver (same as the GUI)
                System,  ///< always the operating system's print driver
                Zpl      ///< always raw ZPL
        };


        ///
        /// An installed printer.
        ///
        struct PrinterEntry
        {
                QString name;
                bool    isDefault{ false };
                bool    isZebra{ false };     ///< would be driven through ZPL in Driver::Auto
                QString state;                ///< "idle", "active", "error", "aborted" or "unknown"
        };


        ///
        /// Outcome of looking a printer up by the name the user typed.
        ///
        struct ResolvedPrinter
        {
                enum class Status { Found, NotFound, Ambiguous, NoDefault };

                Status      status{ Status::NotFound };
                QString     name;           ///< the exact installed name, when Found
                QStringList candidates;     ///< the matching names, when Ambiguous
        };


        ///
        /// Outcome of a print.
        ///
        struct PrintResult
        {
                bool        ok{ false };
                QString     error;
                QStringList warnings;
                int         pages{ 0 };
        };


        ///
        /// Printer resolution and printing with no user interface.
        ///
        class PrintService
        {
        public:
                /// All installed printers.
                static QList<PrinterEntry> printers();

                /// Look up a printer.  "default" (any case) means the operating system default.
                /// Otherwise the name must match an installed printer exactly, or, failing that,
                /// case-insensitively and uniquely.  Never fuzzy: a wrong guess wastes label stock.
                static ResolvedPrinter resolvePrinter( const QString& requested );

                /// Would this printer be driven through ZPL for the given driver choice?
                static bool usesZpl( const QString& printerName, Driver driver );

                /// Print all pages of the renderer on an installed printer.
                static PrintResult printToPrinter( const model::PageRenderer& renderer,
                                                   const QString&             printerName,
                                                   Driver                     driver = Driver::Auto );

                /// Write all pages of the renderer to a PDF.  A file name of "-" writes the PDF to
                /// 'standardOutput' (QPrinter cannot write to a pipe, so a temporary file is used).
                static PrintResult printToPdf( const model::PageRenderer& renderer,
                                               const QString&             fileName,
                                               QIODevice*                 standardOutput = nullptr );

                /// Render every page of the renderer to PNG files at the given resolution.  One page is
                /// written to exactly fileName; several to name-1.png, name-2.png, ...
                static PrintResult printToImages( const model::PageRenderer& renderer,
                                                  const QString&             fileName,
                                                  int                        dpi );

                static QString stateToString( int printerState );
        };

}


#endif // printing_PrintService_hpp
