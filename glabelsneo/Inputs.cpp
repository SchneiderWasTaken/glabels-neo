//  Inputs.cpp
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

#include "Inputs.hpp"

#include "Terminal.hpp"

#include "merge/Factory.hpp"
#include "merge/JsonRecords.hpp"
#include "merge/Merge.hpp"
#include "model/XmlLabelParser.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryFile>


namespace
{

        using glabels::cli::Diagnostic;


        Diagnostic inputError( const QString& code, const QString& message )
        {
                Diagnostic d;
                d.code     = code;
                d.message  = message;
                d.exitCode = glabels::cli::ExitInput;
                return d;
        }


        Diagnostic dataError( const QString& code, const QString& message )
        {
                Diagnostic d;
                d.code     = code;
                d.message  = message;
                d.exitCode = glabels::cli::ExitData;
                return d;
        }


        /// Delimited-text format name -> merge backend id
        const QMap<QString,QString>& textBackends()
        {
                static const QMap<QString,QString> map = {
                        { "csv",              "Text/Comma/Line1Keys" },
                        { "csv-nokeys",       "Text/Comma" },
                        { "tsv",              "Text/Tab/Line1Keys" },
                        { "tsv-nokeys",       "Text/Tab" },
                        { "colon",            "Text/Colon/Line1Keys" },
                        { "colon-nokeys",     "Text/Colon" },
                        { "semicolon",        "Text/Semicolon/Keys" },
                        { "semicolon-nokeys", "Text/Semicolon" }
                };
                return map;
        }

}


namespace glabels::cli::Inputs
{

        QStringList knownFormats()
        {
                QStringList list = { "json", "jsonl" };
                list.append( textBackends().keys() );
                return list;
        }


        std::unique_ptr<model::Model> loadModel( const QString& file, Diagnostic& error )
        {
                if ( file.isEmpty() )
                {
                        error.code     = "usage";
                        error.message  = "no label file given";
                        error.exitCode = ExitUsage;
                        return nullptr;
                }

                QString path = file;

                // Standard input: spool to a temporary file so compressed files work too.
                QTemporaryFile spool;
                if ( file == "-" )
                {
                        const QByteArray bytes = Terminal::readAllStdin();
                        if ( bytes.isEmpty() )
                        {
                                error = inputError( "input.missing", "no label file was received on standard input" );
                                return nullptr;
                        }
                        spool.setFileTemplate( QDir::tempPath() + "/glabelsneo-XXXXXX.glabels" );
                        if ( !spool.open() || spool.write( bytes ) != bytes.size() )
                        {
                                error = inputError( "input.invalid", "cannot buffer the label file from standard input" );
                                return nullptr;
                        }
                        spool.close();
                        path = spool.fileName();
                }
                else
                {
                        const QFileInfo info( file );
                        if ( !info.exists() )
                        {
                                error = inputError( "input.missing", QString( "label file '%1' does not exist" ).arg( file ) );
                                return nullptr;
                        }
                        if ( !info.isFile() )
                        {
                                error = inputError( "input.invalid", QString( "'%1' is not a file" ).arg( file ) );
                                return nullptr;
                        }
                        if ( !info.isReadable() )
                        {
                                error = inputError( "input.invalid", QString( "label file '%1' cannot be read" ).arg( file ) );
                                return nullptr;
                        }
                }

                Terminal::takeLog();
                std::unique_ptr<model::Model> model( model::XmlLabelParser::readFile( path ) );
                if ( !model )
                {
                        QString why = Terminal::takeLog().join( "; " ).trimmed();
                        error = inputError( "input.invalid",
                                            QString( "'%1' is not a valid gLabels project file%2" )
                                            .arg( file, why.isEmpty() ? QString() : QString( " (%1)" ).arg( why ) ) );
                }
                return model;
        }


        QString detectFormat( const QString& source, const QString& explicitFormat, Diagnostic& error )
        {
                QString format = explicitFormat.toLower();
                if ( format == "ndjson" || format == "jsonlines" )
                {
                        format = "jsonl";
                }

                if ( format.isEmpty() )
                {
                        const QString suffix = QFileInfo( source ).suffix().toLower();
                        if ( suffix == "json" )                          format = "json";
                        else if ( suffix == "jsonl" || suffix == "ndjson" ) format = "jsonl";
                        else if ( suffix == "csv" )                      format = "csv";
                        else if ( suffix == "tsv" || suffix == "tab" )   format = "tsv";
                }

                if ( format.isEmpty() )
                {
                        error = dataError( "data.format",
                                           QString( "cannot tell the format of '%1'; use --data-format (%2)" )
                                           .arg( source, knownFormats().join( ", " ) ) );
                        error.exitCode = ExitUsage;
                        return QString();
                }

                if ( format != "json" && format != "jsonl" && !textBackends().contains( format ) )
                {
                        error = dataError( "data.format",
                                           QString( "unknown data format '%1' (use one of: %2)" )
                                           .arg( format, knownFormats().join( ", " ) ) );
                        error.exitCode = ExitUsage;
                        return QString();
                }

                return format;
        }


        bool loadRecords( const QString& source, const QString& format, RecordSet& out, Diagnostic& error )
        {
                QByteArray bytes;
                if ( source == "-" )
                {
                        bytes = Terminal::readAllStdin();
                }
                else
                {
                        QFile file( source );
                        if ( !file.exists() )
                        {
                                error = dataError( "data.missing", QString( "data file '%1' does not exist" ).arg( source ) );
                                return false;
                        }
                        if ( !file.open( QIODevice::ReadOnly ) )
                        {
                                error = dataError( "data.missing", QString( "data file '%1' cannot be read: %2" ).arg( source, file.errorString() ) );
                                return false;
                        }
                        bytes = file.readAll();
                }

                if ( format == "json" || format == "jsonl" )
                {
                        const merge::JsonRecords::Result result = ( format == "json" )
                                ? merge::JsonRecords::parseArray( bytes )
                                : merge::JsonRecords::parseLines( bytes );
                        if ( !result.ok )
                        {
                                error = dataError( "data.invalid", QString( "%1: %2" ).arg( source == "-" ? QString( "standard input" ) : source, result.error ) );
                                return false;
                        }

                        out.keys = result.keys;
                        for ( int i = 0; i < result.records.size(); i++ )
                        {
                                LabelSpec label;
                                label.fields = QMap<QString,QString>( result.records.at( i ) );
                                label.copies = result.copies.at( i );
                                out.labels.append( label );
                        }
                        return true;
                }

                // Delimited text: the existing merge backends read from a file path.
                QString path = source;
                QTemporaryFile spool;
                if ( source == "-" )
                {
                        spool.setFileTemplate( QDir::tempPath() + "/glabelsneo-XXXXXX.txt" );
                        if ( !spool.open() || spool.write( bytes ) != bytes.size() )
                        {
                                error = dataError( "data.invalid", "cannot buffer the data from standard input" );
                                return false;
                        }
                        spool.close();
                        path = spool.fileName();
                }

                std::unique_ptr<merge::Merge> backend( merge::Factory::createMerge( textBackends().value( format ) ) );
                if ( !backend )
                {
                        error = dataError( "data.format", QString( "no reader for data format '%1'" ).arg( format ) );
                        return false;
                }

                backend->setSource( path );
                out.keys = backend->keys();
                for ( const merge::Record& record : backend->selectedRecords() )
                {
                        LabelSpec label;
                        label.fields = QMap<QString,QString>( record );
                        out.labels.append( label );
                }
                return true;
        }


        bool recordsFromModelMerge( const model::Model& model, RecordSet& out )
        {
                const merge::Merge* merge = model.merge();
                if ( !merge || merge->keys().isEmpty() )
                {
                        return false;
                }

                out.keys = merge->keys();
                for ( const merge::Record& record : merge->selectedRecords() )
                {
                        LabelSpec label;
                        label.fields = QMap<QString,QString>( record );
                        out.labels.append( label );
                }
                return true;
        }

}
