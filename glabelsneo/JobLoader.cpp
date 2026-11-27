//  JobLoader.cpp
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

#include "JobLoader.hpp"

#include "merge/JsonRecords.hpp"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>


namespace
{

        using glabels::cli::Diagnostic;
        using glabels::cli::ExitData;
        using glabels::cli::ExitUsage;
        using glabels::merge::JsonRecords;


        Diagnostic dataError( const QString& message )
        {
                Diagnostic d;
                d.code     = "data.invalid";
                d.message  = message;
                d.exitCode = ExitData;
                return d;
        }


        Diagnostic usageError( const QString& message )
        {
                Diagnostic d;
                d.code     = "usage";
                d.message  = message;
                d.exitCode = ExitUsage;
                return d;
        }


        bool checkKeys( const QJsonObject& object, const QStringList& allowed, const QString& where, Diagnostic& error )
        {
                for ( const QString& key : object.keys() )
                {
                        if ( !allowed.contains( key ) )
                        {
                                error = dataError( QString( "%1: unknown key '%2' (allowed: %3)" )
                                                   .arg( where, key, allowed.join( ", " ) ) );
                                return false;
                        }
                }
                return true;
        }


        bool toInt( const QJsonValue& value, int minimum, int& out )
        {
                if ( value.type() != QJsonValue::Double )
                {
                        return false;
                }
                const double d = value.toDouble();
                if ( d != std::floor( d ) || d < minimum || d > 1.0e9 )
                {
                        return false;
                }
                out = static_cast<int>( d );
                return true;
        }


        QString resolvePath( const QString& baseDir, const QString& path )
        {
                if ( path.isEmpty() || path == "-" || QDir::isAbsolutePath( path ) )
                {
                        return path;
                }
                return QDir::cleanPath( QDir( baseDir ).absoluteFilePath( path ) );
        }


        bool stringValue( const QJsonObject& object, const QString& key, const QString& where, QString& out, Diagnostic& error )
        {
                if ( !object.contains( key ) )
                {
                        return true;
                }
                if ( object[key].type() != QJsonValue::String )
                {
                        error = dataError( QString( "%1.%2 must be a string" ).arg( where, key ) );
                        return false;
                }
                out = object[key].toString();
                return true;
        }

}


namespace glabels::cli
{

        void PrintOptions::overlay( const PrintOptions& other )
        {
                if ( other.copies )    copies    = other.copies;
                if ( other.sheets )    sheets    = other.sheets;
                if ( other.first )     first     = other.first;
                if ( other.collate )   collate   = other.collate;
                if ( other.group )     group     = other.group;
                if ( other.outlines )  outlines  = other.outlines;
                if ( other.cropMarks ) cropMarks = other.cropMarks;
                if ( other.reverse )   reverse   = other.reverse;
        }


        bool JobLoader::parseAssignment( const QString& text, QString& name, QString& value )
        {
                const int i = static_cast<int>( text.indexOf( '=' ) );
                if ( i <= 0 )
                {
                        return false;
                }

                name  = text.left( i );
                value = text.mid( i + 1 );

                return !name.contains( ':' ) && !name.contains( '}' ) && !name.contains( '$' );
        }


        bool JobLoader::parseStringMap( const QJsonValue&      value,
                                        const QString&         where,
                                        QMap<QString,QString>& map,
                                        Diagnostic&            error )
        {
                if ( !value.isObject() )
                {
                        error = dataError( QString( "%1 must be an object of name: value pairs" ).arg( where ) );
                        return false;
                }

                const QJsonObject object = value.toObject();
                for ( auto it = object.begin(); it != object.end(); ++it )
                {
                        if ( it.key().startsWith( '_' ) )
                        {
                                error = dataError( QString( "%1.%2: names starting with '_' are reserved" ).arg( where, it.key() ) );
                                return false;
                        }

                        QString text;
                        if ( !JsonRecords::scalarToString( it.value(), text ) )
                        {
                                error = dataError( QString( "%1.%2 must be a string, number, boolean or null" ).arg( where, it.key() ) );
                                return false;
                        }
                        map.insert( it.key(), text );
                }
                return true;
        }


        bool JobLoader::parseOptions( const QJsonObject& object,
                                      const QString&     where,
                                      PrintOptions&      options,
                                      Diagnostic&        error )
        {
                if ( !checkKeys( object, { "copies", "sheets", "first", "collate", "group", "outlines", "cropMarks", "reverse" },
                                 where, error ) )
                {
                        return false;
                }

                auto number = [&]( const char* key, int minimum, std::optional<int>& target ) {
                        if ( object.contains( key ) )
                        {
                                int n = 0;
                                if ( !toInt( object[key], minimum, n ) )
                                {
                                        error = dataError( QString( "%1.%2 must be a whole number >= %3" ).arg( where, key ).arg( minimum ) );
                                        return false;
                                }
                                target = n;
                        }
                        return true;
                };

                auto flag = [&]( const char* key, std::optional<bool>& target ) {
                        if ( object.contains( key ) )
                        {
                                if ( object[key].type() != QJsonValue::Bool )
                                {
                                        error = dataError( QString( "%1.%2 must be true or false" ).arg( where, key ) );
                                        return false;
                                }
                                target = object[key].toBool();
                        }
                        return true;
                };

                return number( "copies", 1, options.copies )
                    && number( "sheets", 1, options.sheets )
                    && number( "first",  1, options.first )
                    && flag( "collate",   options.collate )
                    && flag( "group",     options.group )
                    && flag( "outlines",  options.outlines )
                    && flag( "cropMarks", options.cropMarks )
                    && flag( "reverse",   options.reverse );
        }


        bool JobLoader::parseJob( const QJsonObject& object,
                                  const QString&     where,
                                  const QString&     baseDir,
                                  JobSpec&           job,
                                  Diagnostic&        error )
        {
                if ( !checkKeys( object,
                                 { "id", "file", "printer", "output", "options", "variables", "defaults", "labels", "data", "dataFormat" },
                                 where, error ) )
                {
                        return false;
                }

                QString file, data;
                if ( !stringValue( object, "id",         where, job.id,         error ) ||
                     !stringValue( object, "file",       where, file,           error ) ||
                     !stringValue( object, "printer",    where, job.printer,    error ) ||
                     !stringValue( object, "output",     where, job.output,     error ) ||
                     !stringValue( object, "data",       where, data,           error ) ||
                     !stringValue( object, "dataFormat", where, job.dataFormat, error ) )
                {
                        return false;
                }

                job.file   = resolvePath( baseDir, file );
                job.data   = resolvePath( baseDir, data );
                job.output = resolvePath( baseDir, job.output );

                if ( !job.printer.isEmpty() && !job.output.isEmpty() )
                {
                        error = usageError( QString( "%1: give either 'printer' or 'output', not both" ).arg( where ) );
                        return false;
                }

                if ( object.contains( "options" ) )
                {
                        if ( !object["options"].isObject() ||
                             !parseOptions( object["options"].toObject(), where + ".options", job.options, error ) )
                        {
                                if ( error.code.isEmpty() )
                                {
                                        error = dataError( QString( "%1.options must be an object" ).arg( where ) );
                                }
                                return false;
                        }
                }

                if ( object.contains( "variables" ) && !parseStringMap( object["variables"], where + ".variables", job.variables, error ) )
                {
                        return false;
                }

                if ( object.contains( "defaults" ) && !parseStringMap( object["defaults"], where + ".defaults", job.defaults, error ) )
                {
                        return false;
                }

                if ( object.contains( "labels" ) )
                {
                        if ( !data.isEmpty() )
                        {
                                error = dataError( QString( "%1: give either 'labels' or 'data', not both" ).arg( where ) );
                                return false;
                        }
                        if ( !object["labels"].isArray() )
                        {
                                error = dataError( QString( "%1.labels must be an array" ).arg( where ) );
                                return false;
                        }

                        job.hasLabels = true;
                        const QJsonArray labels = object["labels"].toArray();
                        for ( int i = 0; i < labels.size(); i++ )
                        {
                                const QString labelWhere = QString( "%1.labels[%2]" ).arg( where ).arg( i );
                                if ( !labels.at( i ).isObject() )
                                {
                                        error = dataError( labelWhere + " must be an object" );
                                        return false;
                                }

                                const QJsonObject labelObject = labels.at( i ).toObject();
                                if ( !checkKeys( labelObject, { "fields", "copies" }, labelWhere, error ) )
                                {
                                        return false;
                                }

                                LabelSpec label;
                                if ( labelObject.contains( "fields" ) &&
                                     !parseStringMap( labelObject["fields"], labelWhere + ".fields", label.fields, error ) )
                                {
                                        return false;
                                }
                                if ( labelObject.contains( "copies" ) && !toInt( labelObject["copies"], 1, label.copies ) )
                                {
                                        error = dataError( labelWhere + ".copies must be a whole number >= 1" );
                                        return false;
                                }
                                job.labels.append( label );
                        }
                }

                return true;
        }


        bool JobLoader::parseJobDocument( const QByteArray& input,
                                          const QString&    baseDir,
                                          QList<JobSpec>&   jobs,
                                          Diagnostic&       error )
        {
                QByteArray json = input;
                QString    encodingError;
                if ( !JsonRecords::normalizeEncoding( json, encodingError ) )
                {
                        error = dataError( encodingError );
                        return false;
                }

                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson( json, &parseError );
                if ( parseError.error != QJsonParseError::NoError )
                {
                        int line = 1, column = 1;
                        for ( qint64 i = 0; i < parseError.offset && i < json.size(); i++ )
                        {
                                if ( json[i] == '\n' ) { line++; column = 1; } else { column++; }
                        }
                        error = dataError( QString( "invalid JSON at line %1, column %2: %3" )
                                           .arg( line ).arg( column ).arg( parseError.errorString() ) );
                        return false;
                }

                if ( !doc.isObject() )
                {
                        error = dataError( "a job document must be a JSON object" );
                        return false;
                }

                const QJsonObject root = doc.object();

                if ( root.contains( "version" ) )
                {
                        int version = 0;
                        if ( !toInt( root["version"], 1, version ) || version != 1 )
                        {
                                error = dataError( "unsupported job document version (this tool reads version 1)" );
                                return false;
                        }
                }

                if ( root.contains( "jobs" ) )
                {
                        if ( !checkKeys( root, { "version", "jobs" }, "$", error ) )
                        {
                                return false;
                        }
                        if ( !root["jobs"].isArray() || root["jobs"].toArray().isEmpty() )
                        {
                                error = dataError( "$.jobs must be a non-empty array" );
                                return false;
                        }

                        const QJsonArray array = root["jobs"].toArray();
                        for ( int i = 0; i < array.size(); i++ )
                        {
                                const QString where = QString( "jobs[%1]" ).arg( i );
                                if ( !array.at( i ).isObject() )
                                {
                                        error = dataError( where + " must be an object" );
                                        return false;
                                }

                                JobSpec job;
                                job.index = i;
                                if ( !parseJob( array.at( i ).toObject(), where, baseDir, job, error ) )
                                {
                                        return false;
                                }
                                jobs.append( job );
                        }
                }
                else
                {
                    // One-job shorthand: the document is the job.
                        QJsonObject jobObject = root;
                        jobObject.remove( "version" );

                        JobSpec job;
                        job.index = 0;
                        if ( !parseJob( jobObject, "$", baseDir, job, error ) )
                        {
                                return false;
                        }
                        jobs.append( job );
                }

                return true;
        }


        bool JobLoader::fromCommandLine( const CliJobInput& in, JobSpec& job, Diagnostic& error )
        {
                job = JobSpec();
                job.index = 0;
                return applyOverrides( in, job, error );
        }


        bool JobLoader::applyOverrides( const CliJobInput& in, JobSpec& job, Diagnostic& error )
        {
                if ( !in.file.isEmpty() )
                {
                        job.file = in.file;
                }

                if ( !in.printer.isEmpty() || !in.output.isEmpty() )
                {
                        job.printer = in.printer;
                        job.output  = in.output;
                }

                job.options.overlay( in.options );

                for ( const QString& text : in.fields )
                {
                        QString name, value;
                        if ( !parseAssignment( text, name, value ) )
                        {
                                error = usageError( QString( "bad --field '%1': expected NAME=VALUE (a name cannot contain ':', '}' or '$')" ).arg( text ) );
                                return false;
                        }
                        if ( name.startsWith( '_' ) )
                        {
                                error = usageError( QString( "bad --field '%1': names starting with '_' are reserved" ).arg( text ) );
                                return false;
                        }
                        job.cliFields.insert( name, value );
                }

                for ( const QString& text : in.variables )
                {
                        QString name, value;
                        if ( !parseAssignment( text, name, value ) )
                        {
                                error = usageError( QString( "bad --var '%1': expected NAME=VALUE" ).arg( text ) );
                                return false;
                        }
                        job.variables.insert( name, value );
                }

                if ( !in.data.isEmpty() )
                {
                        job.data      = in.data;
                        job.labels.clear();
                        job.hasLabels = false;
                }
                if ( !in.dataFormat.isEmpty() )
                {
                        job.dataFormat = in.dataFormat;
                }

                return true;
        }

}
