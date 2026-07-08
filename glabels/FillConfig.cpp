//  FillConfig.cpp
//

#include "FillConfig.hpp"

#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QProcessEnvironment>


namespace glabels
{


        const char* FillConfigIO::MAGIC = "GLBLFILL";
        const int   FillConfigIO::MAGIC_LEN = 8;


        namespace
        {
                const quint32 FORMAT_VERSION = 2;

                QByteArray serialize( const FillConfig& cfg )
                {
                        QByteArray ba;
                        QDataStream s( &ba, QIODevice::WriteOnly );
                        s.setVersion( QDataStream::Qt_6_0 );
                        s << FORMAT_VERSION;
                        s << cfg.adminPin;
                        s << cfg.printerName;
                        s << cfg.appName;
                        s << cfg.sourcePath;
                        s << cfg.lockedFields;
                        s << cfg.isConfigured;
                        // Embedded labels.  Cast count to quint32 -- qsizetype
                        // would serialize as 8 bytes and mismatch the reader.
                        s << static_cast<quint32>( cfg.embeddedLabels.size() );
                        for ( const auto& lbl : cfg.embeddedLabels )
                        {
                                s << lbl.name << lbl.xml;
                        }
                        s << cfg.labelFiles;
                        return ba;
                }

                bool deserialize( const QByteArray& ba, FillConfig& cfg )
                {
                        QDataStream s( ba );
                        s.setVersion( QDataStream::Qt_6_0 );
                        quint32 version = 0;
                        s >> version;
                        if ( version != FORMAT_VERSION )
                        {
                                return false;
                        }
                        s >> cfg.adminPin;
                        s >> cfg.printerName;
                        s >> cfg.appName;
                        s >> cfg.sourcePath;
                        s >> cfg.lockedFields;
                        s >> cfg.isConfigured;
                        int nLabels = 0;
                        s >> nLabels;
                        for ( int i = 0; i < nLabels; ++i )
                        {
                                EmbeddedLabel lbl;
                                s >> lbl.name >> lbl.xml;
                                cfg.embeddedLabels.append( lbl );
                        }
                        s >> cfg.labelFiles;
                        return s.status() == QDataStream::Ok;
                }
        }


        bool FillConfigIO::readFromExe( const QString& exePath, FillConfig& cfg )
        {
                /* When running inside an SFX wrapper, the trailer is on the
                 * SFX exe, not the extracted temp copy.  The stub sets
                 * GLABELS_FILL_SFX_PATH to point back to itself. */
                QString path = exePath;
                QString sfxPath = QProcessEnvironment::systemEnvironment().value( "GLABELS_FILL_SFX_PATH" );
                if ( !sfxPath.isEmpty() )
                {
                        path = sfxPath;
                }

                QFile f( path );
                if ( !f.open( QIODevice::ReadOnly ) )
                {
                        return false;
                }

                qint64 size = f.size();
                const qint64 trailerMin = sizeof(qint32) + MAGIC_LEN;
                if ( size < trailerMin )
                {
                        return false;
                }

                f.seek( size - MAGIC_LEN );
                QByteArray magic = f.read( MAGIC_LEN );
                if ( magic != QByteArray( MAGIC, MAGIC_LEN ) )
                {
                        return false;
                }

                f.seek( size - MAGIC_LEN - sizeof(qint32) );
                qint32 payloadLen = 0;
                if ( f.read( reinterpret_cast<char*>(&payloadLen), sizeof(qint32) )
                     != sizeof(qint32) )
                {
                        return false;
                }

                qint64 payloadPos = size - MAGIC_LEN - sizeof(qint32) - payloadLen;
                if ( payloadPos < 0 )
                {
                        return false;
                }

                f.seek( payloadPos );
                QByteArray payload = f.read( payloadLen );
                if ( payload.size() != payloadLen )
                {
                        return false;
                }

                return deserialize( payload, cfg );
        }


        bool FillConfigIO::writeToExe( const QString& exePath, const FillConfig& cfg )
        {
                QString path = exePath;
                QString sfxPath = QProcessEnvironment::systemEnvironment().value( "GLABELS_FILL_SFX_PATH" );
                if ( !sfxPath.isEmpty() )
                {
                        path = sfxPath;
                }

                QFile f( path );
                if ( !f.open( QIODevice::ReadWrite ) )
                {
                        return false;
                }

                qint64 size = f.size();
                f.seek( size - MAGIC_LEN );
                QByteArray magic = f.read( MAGIC_LEN );
                if ( magic == QByteArray( MAGIC, MAGIC_LEN ) )
                {
                        f.seek( size - MAGIC_LEN - sizeof(qint32) );
                        qint32 oldLen = 0;
                        if ( f.read( reinterpret_cast<char*>(&oldLen), sizeof(qint32) )
                             == sizeof(qint32) )
                        {
                                size = size - MAGIC_LEN - sizeof(qint32) - oldLen;
                        }
                }

                QByteArray payload = serialize( cfg );
                f.seek( size );
                f.resize( size );

                qint32 payloadLen = payload.size();
                f.write( payload.constData(), payloadLen );
                f.write( reinterpret_cast<const char*>(&payloadLen), sizeof(qint32) );
                f.write( MAGIC, MAGIC_LEN );

                f.close();
                return true;
        }


} // namespace glabels
