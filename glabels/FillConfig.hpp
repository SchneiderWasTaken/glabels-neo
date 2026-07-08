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
