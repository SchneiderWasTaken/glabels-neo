//  FillConfig.hpp
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


//  FillConfig.hpp
//
//  Configuration + appended-trailer persistence for the glabels-fill kiosk.
//

#ifndef FillConfig_hpp
#define FillConfig_hpp


#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>


namespace glabels
{


        ///
        /// A label baked into the kiosk exe (serialized .glabels XML).
        ///
        struct EmbeddedLabel
        {
                QString   name;       // display name (e.g. "Price Tag")
                QByteArray xml;       // serialized project XML
        };


        struct FillConfig
        {
                // Embedded labels (preferred when non-empty).  If multiple,
                // the employee picks from a dropdown.
                QList<EmbeddedLabel> embeddedLabels;

                // Fallback: external file paths (used when no embedded labels).
                QStringList          labelFiles;

                // Admin PIN (empty = no PIN required).
                QString              adminPin;

                // Locked printer name (empty = use system default).
                QString              printerName;

                // Branding.
                QString              appName{ "glabels Fill" };

                // Excel/CSV source path for pre-filling locked columns.
                QString              sourcePath;

                // Fields pre-filled from the source and read-only to the employee.
                QStringList          lockedFields;

                bool                 isConfigured{ false };

                bool hasLabels() const
                {
                        return !embeddedLabels.isEmpty() || !labelFiles.isEmpty();
                }
        };


        namespace FillConfigIO
        {
                extern const char* MAGIC;
                extern const int   MAGIC_LEN;

                bool readFromExe( const QString& exePath, FillConfig& cfg );
                bool writeToExe( const QString& exePath, const FillConfig& cfg );
        }


}


#endif // FillConfig_hpp
