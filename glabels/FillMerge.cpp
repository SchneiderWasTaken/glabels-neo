//  FillMerge.cpp
//
//  Copyright (C) 2014-2026  Jaye Evins <evins@snaught.com>
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


#include "FillMerge.hpp"


namespace glabels
{


        FillMerge::FillMerge()
                : Merge()
        {
                mId = "Fill";
        }


        FillMerge::FillMerge( const FillMerge* other )
                : Merge( other ),
                  mKeys( other->mKeys ),
                  mRecords( other->mRecords )
        {
        }


        void FillMerge::setRecords( const QStringList& keys, const QList<merge::Record>& records )
        {
                mKeys = keys;
                mRecords = records;
                // Trigger the open/read/close loop to populate Merge::mRecordList.
                setSource( QString() );
        }


        FillMerge* FillMerge::clone() const
        {
                return new FillMerge( this );
        }


        QStringList FillMerge::keys() const
        {
                return mKeys;
        }


        QString FillMerge::primaryKey() const
        {
                return mKeys.isEmpty() ? QString() : mKeys.first();
        }


        void FillMerge::open()
        {
                mIndex = 0;
        }


        void FillMerge::close()
        {
        }


        merge::Record FillMerge::readNextRecord()
        {
                if ( mIndex < mRecords.size() )
                {
                        return mRecords.at( mIndex++ );
                }
                return merge::Record();
        }


} // namespace glabels
