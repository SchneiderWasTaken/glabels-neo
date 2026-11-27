//  JsonMerge.cpp
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

#include "JsonMerge.hpp"

#include <QDebug>
#include <QFile>


namespace
{
        const QString ID_ARRAY = "Json/Array";
        const QString ID_LINES = "Json/Lines";
}


namespace glabels::merge
{

        JsonMerge::JsonMerge( const QString& id, bool lines )
                : Merge(), mLines( lines )
        {
                mId = id;
        }


        JsonMerge::JsonMerge( const JsonMerge* other )
                : Merge( other ), mLines( other->mLines ), mKeys( other->mKeys ), mRecords( other->mRecords )
        {
        }


        QStringList JsonMerge::keys() const
        {
                return mKeys;
        }


        QString JsonMerge::primaryKey() const
        {
                return mKeys.isEmpty() ? QString() : mKeys.first();
        }


        QString JsonMerge::lastError() const
        {
                return mError;
        }


        void JsonMerge::open()
        {
                mKeys.clear();
                mRecords.clear();
                mError.clear();
                mIndex = 0;

                QFile file( source() );
                if ( !file.open( QIODevice::ReadOnly ) )
                {
                        mError = QString( "cannot read '%1': %2" ).arg( source(), file.errorString() );
                        qWarning() << mError;
                        return;
                }

                const QByteArray data = file.readAll();
                const JsonRecords::Result result = mLines ? JsonRecords::parseLines( data )
                                                          : JsonRecords::parseArray( data );
                if ( !result.ok )
                {
                        mError = QString( "%1: %2" ).arg( source(), result.error );
                        qWarning() << mError;
                        return;
                }

                mKeys = result.keys;
                for ( int i = 0; i < result.records.size(); i++ )
                {
                        Record record = result.records.at( i );
                        if ( record.isEmpty() )
                        {
                                record.insert( "_", QString() );   // an empty record would end the read loop
                        }

                        for ( int n = 0; n < result.copies.at( i ); n++ )
                        {
                                mRecords.append( record );
                        }
                }
        }


        void JsonMerge::close()
        {
        }


        Record JsonMerge::readNextRecord()
        {
                if ( mIndex < mRecords.size() )
                {
                        return mRecords.at( mIndex++ );
                }
                return Record();
        }


        //
        // Json (array)
        //
        Json::Json() : JsonMerge( ID_ARRAY, false )
        {
        }

        Json::Json( const Json* other ) : JsonMerge( other )
        {
        }

        Json* Json::clone() const
        {
                return new Json( this );
        }

        QString Json::id()
        {
                return ID_ARRAY;
        }

        Merge* Json::create()
        {
                return new Json();
        }


        //
        // JsonLines
        //
        JsonLines::JsonLines() : JsonMerge( ID_LINES, true )
        {
        }

        JsonLines::JsonLines( const JsonLines* other ) : JsonMerge( other )
        {
        }

        JsonLines* JsonLines::clone() const
        {
                return new JsonLines( this );
        }

        QString JsonLines::id()
        {
                return ID_LINES;
        }

        Merge* JsonLines::create()
        {
                return new JsonLines();
        }

}
