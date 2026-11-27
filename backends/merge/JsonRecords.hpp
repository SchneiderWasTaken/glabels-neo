//  JsonRecords.hpp
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

#ifndef merge_JsonRecords_hpp
#define merge_JsonRecords_hpp


#include "Record.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>


namespace glabels::merge
{

        ///
        /// Parser for merge records written as JSON.
        ///
        /// Two layouts are understood:
        ///   - an array of flat objects: [ {"name":"Blue Jeans","price":29.99}, ... ]
        ///     (a single bare object counts as one record), and
        ///   - JSON Lines: one flat object per line, blank lines ignored.
        ///
        /// Values become strings: strings as is, numbers in their shortest exact form
        /// (29.99, not 29.989999999999998; integers up to 2^53 are exact, so send longer
        /// identifiers as strings), booleans as "true"/"false", null as an empty string.
        /// Nested objects and arrays are an error, reported with their JSON path.
        ///
        /// Keys that start with an underscore are reserved for metadata and never become
        /// fields.  "_copies" (an integer >= 1) asks for that many copies of the record.
        ///
        struct JsonRecords
        {
                struct Result
                {
                        bool          ok{ false };
                        QString       error;     ///< human readable, with a line/column or JSON path when known
                        QStringList   keys;      ///< field names, in order of first appearance
                        QList<Record> records;   ///< one per object, before "_copies" expansion
                        QList<int>    copies;    ///< "_copies" of each record (1 when absent)
                };

                static Result parseArray( const QByteArray& data );
                static Result parseLines( const QByteArray& data );

                /// Parse one JSON object (shared with the glabelsneo job document reader).
                /// 'where' names the object in error messages.  Returns false and sets
                /// 'error' on failure.
                static bool readObject( const QJsonObject& object,
                                        const QString&     where,
                                        Record&            record,
                                        int&               copies,
                                        QStringList&       keys,
                                        QString&           error );

                /// Convert a scalar JSON value to a string.  False for objects and arrays.
                static bool scalarToString( const QJsonValue& value, QString& string );

                /// Strip a UTF-8 byte order mark.  Returns false (with an explanation) for
                /// UTF-16/UTF-32 input, which is what Windows PowerShell 5.1 writes by default.
                static bool normalizeEncoding( QByteArray& data, QString& error );
        };

}


#endif // merge_JsonRecords_hpp
