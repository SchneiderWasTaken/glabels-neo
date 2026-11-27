//  JsonRecords.cpp
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

#include "JsonRecords.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLocale>

#include <cmath>


namespace
{

        using glabels::merge::JsonRecords;


        ///
        /// 1-based line and column of a byte offset
        ///
        QString lineAndColumn( const QByteArray& data, qint64 offset )
        {
                int line = 1;
                int column = 1;
                for ( qint64 i = 0; i < offset && i < data.size(); i++ )
                {
                        if ( data[i] == '\n' )
                        {
                                line++;
                                column = 1;
                        }
                        else
                        {
                                column++;
                        }
                }
                return QString( "line %1, column %2" ).arg( line ).arg( column );
        }


        void addResult( JsonRecords::Result& result, const glabels::merge::Record& record, int copies, const QStringList& keys )
        {
                for ( const QString& key : keys )
                {
                        if ( !result.keys.contains( key ) )
                        {
                                result.keys.append( key );
                        }
                }
                result.records.append( record );
                result.copies.append( copies );
        }

}


namespace glabels::merge
{

        bool JsonRecords::normalizeEncoding( QByteArray& data, QString& error )
        {
                if ( data.startsWith( "\xEF\xBB\xBF" ) )
                {
                        data.remove( 0, 3 );
                        return true;
                }

                if ( data.startsWith( "\xFF\xFE" ) || data.startsWith( "\xFE\xFF" ) )
                {
                        error = "the data is UTF-16 (a byte order mark FF FE / FE FF); JSON must be UTF-8.  "
                                "In Windows PowerShell 5.1 use '| Out-File -Encoding utf8' (or PowerShell 7).";
                        return false;
                }

                // UTF-16 without a mark still shows as NUL bytes between ASCII characters.
                if ( data.size() >= 2 && data.left( 4 ).contains( '\0' ) )
                {
                        error = "the data looks like UTF-16 or UTF-32 (NUL bytes); JSON must be UTF-8.";
                        return false;
                }

                return true;
        }


        bool JsonRecords::scalarToString( const QJsonValue& value, QString& string )
        {
                switch ( value.type() )
                {
                case QJsonValue::String:
                        string = value.toString();
                        return true;

                case QJsonValue::Double:
                {
                        const double d = value.toDouble();
                        if ( std::isfinite( d ) && d == std::floor( d ) && std::fabs( d ) < 9007199254740992.0 )
                        {
                                string = QString::number( static_cast<qlonglong>( d ) );
                        }
                        else
                        {
                                string = QLocale::c().toString( d, 'g', QLocale::FloatingPointShortest );
                        }
                        return true;
                }

                case QJsonValue::Bool:
                        string = value.toBool() ? "true" : "false";
                        return true;

                case QJsonValue::Null:
                        string.clear();
                        return true;

                default:
                        return false;
                }
        }


        bool JsonRecords::readObject( const QJsonObject& object,
                                      const QString&     where,
                                      Record&            record,
                                      int&               copies,
                                      QStringList&       keys,
                                      QString&           error )
        {
                copies = 1;

                for ( auto it = object.begin(); it != object.end(); ++it )
                {
                        const QString key = it.key();

                        if ( key == "_copies" )
                        {
                                QString text;
                                if ( !scalarToString( it.value(), text ) || it.value().type() == QJsonValue::Bool )
                                {
                                        error = QString( "%1._copies must be a whole number >= 1" ).arg( where );
                                        return false;
                                }
                                bool ok = false;
                                const int n = text.toInt( &ok );
                                if ( !ok || n < 1 )
                                {
                                        error = QString( "%1._copies must be a whole number >= 1" ).arg( where );
                                        return false;
                                }
                                copies = n;
                                continue;
                        }

                        if ( key.startsWith( '_' ) )
                        {
                                continue;  // reserved for metadata
                        }

                        QString text;
                        if ( !scalarToString( it.value(), text ) )
                        {
                                error = QString( "%1.%2 is %3; field values must be strings, numbers, booleans or null" )
                                        .arg( where, key, it.value().isArray() ? "an array" : "an object" );
                                return false;
                        }

                        record.insert( key, text );
                        if ( !keys.contains( key ) )
                        {
                                keys.append( key );
                        }
                }

                return true;
        }


        JsonRecords::Result JsonRecords::parseArray( const QByteArray& input )
        {
                Result result;
                QByteArray data = input;

                if ( !normalizeEncoding( data, result.error ) )
                {
                        return result;
                }

                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson( data, &parseError );
                if ( parseError.error != QJsonParseError::NoError )
                {
                        result.error = QString( "invalid JSON at %1: %2" )
                                .arg( lineAndColumn( data, parseError.offset ), parseError.errorString() );
                        return result;
                }

                QJsonArray array;
                if ( doc.isArray() )
                {
                        array = doc.array();
                }
                else if ( doc.isObject() )
                {
                        array.append( doc.object() );
                }
                else
                {
                        result.error = "expected a JSON array of objects";
                        return result;
                }

                for ( int i = 0; i < array.size(); i++ )
                {
                        const QString where = QString( "$[%1]" ).arg( i );

                        if ( !array.at( i ).isObject() )
                        {
                                result.error = QString( "%1 is not an object" ).arg( where );
                                return result;
                        }

                        Record      record;
                        QStringList keys;
                        int         copies = 1;
                        if ( !readObject( array.at( i ).toObject(), where, record, copies, keys, result.error ) )
                        {
                                return result;
                        }
                        addResult( result, record, copies, keys );
                }

                result.ok = true;
                return result;
        }


        JsonRecords::Result JsonRecords::parseLines( const QByteArray& input )
        {
                Result result;
                QByteArray data = input;

                if ( !normalizeEncoding( data, result.error ) )
                {
                        return result;
                }

                const QList<QByteArray> lines = data.split( '\n' );
                for ( int i = 0; i < lines.size(); i++ )
                {
                        const QByteArray line = lines.at( i ).trimmed();
                        if ( line.isEmpty() )
                        {
                                continue;
                        }

                        const QString where = QString( "line %1" ).arg( i + 1 );

                        QJsonParseError parseError;
                        const QJsonDocument doc = QJsonDocument::fromJson( line, &parseError );
                        if ( parseError.error != QJsonParseError::NoError )
                        {
                                result.error = QString( "invalid JSON on %1, column %2: %3" )
                                        .arg( where ).arg( parseError.offset + 1 ).arg( parseError.errorString() );
                                return result;
                        }
                        if ( !doc.isObject() )
                        {
                                result.error = QString( "%1 is not a JSON object" ).arg( where );
                                return result;
                        }

                        Record      record;
                        QStringList keys;
                        int         copies = 1;
                        if ( !readObject( doc.object(), where, record, copies, keys, result.error ) )
                        {
                                return result;
                        }
                        addResult( result, record, copies, keys );
                }

                result.ok = true;
                return result;
        }

}
