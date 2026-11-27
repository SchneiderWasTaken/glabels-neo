//  MemoryMerge.cpp
//
//  Copyright (C) 2014-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//

#include "MemoryMerge.hpp"


namespace glabels::merge
{

        MemoryMerge::MemoryMerge()
                : Merge()
        {
                mId = "Memory";
        }


        MemoryMerge::MemoryMerge( const QString& id )
                : Merge()
        {
                mId = id;
        }


        MemoryMerge::MemoryMerge( const MemoryMerge* other )
                : Merge( other ),
                  mKeys( other->mKeys ),
                  mRecords( other->mRecords )
        {
        }


        void MemoryMerge::setRecords( const QStringList& keys, const QList<Record>& records )
        {
                mKeys    = keys;
                mRecords = records;

                // An empty record is the end-of-data marker of the read loop.
                for ( Record& record : mRecords )
                {
                        if ( record.isEmpty() )
                        {
                                record.insert( "_", QString() );
                        }
                }

                // Trigger the open/read/close loop to populate Merge::mRecordList.
                setSource( QString() );
        }


        MemoryMerge* MemoryMerge::clone() const
        {
                return new MemoryMerge( this );
        }


        QStringList MemoryMerge::keys() const
        {
                return mKeys;
        }


        QString MemoryMerge::primaryKey() const
        {
                return mKeys.isEmpty() ? QString() : mKeys.first();
        }


        void MemoryMerge::open()
        {
                mIndex = 0;
        }


        void MemoryMerge::close()
        {
        }


        Record MemoryMerge::readNextRecord()
        {
                if ( mIndex < mRecords.size() )
                {
                        return mRecords.at( mIndex++ );
                }
                return Record();
        }

}
