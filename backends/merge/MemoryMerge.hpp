//  MemoryMerge.hpp
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

#ifndef merge_MemoryMerge_hpp
#define merge_MemoryMerge_hpp


#include "Merge.hpp"
#include "Record.hpp"

#include <QList>
#include <QStringList>


namespace glabels::merge
{

        ///
        /// In-memory merge backend.
        ///
        /// Holds a list of records handed over by the caller (the GUI's fill table, or the
        /// glabelsneo command line tool) and serves them to Merge::setSource()'s read loop.
        /// Not registered in the Factory: it has no file or source of its own.
        ///
        class MemoryMerge : public Merge
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                MemoryMerge();
                MemoryMerge( const MemoryMerge* other );
                ~MemoryMerge() override = default;
        protected:
                explicit MemoryMerge( const QString& id );


                /////////////////////////////////
                // Records
                /////////////////////////////////
        public:
                /// Replace the in-memory record set and re-run the read loop so that
                /// Merge::recordList() / selectedRecords() reflect it.  A record with no
                /// fields would end the read loop early, so such records get a placeholder
                /// field.
                void setRecords( const QStringList& keys, const QList<Record>& records );


                /////////////////////////////////
                // Object duplication
                /////////////////////////////////
        public:
                MemoryMerge* clone() const override;


                /////////////////////////////////
                // Implementation of virtual methods
                /////////////////////////////////
        public:
                QStringList keys() const override;
                QString primaryKey() const override;
        protected:
                void open() override;
                void close() override;
                Record readNextRecord() override;


                /////////////////////////////////
                // Private Data
                /////////////////////////////////
        private:
                QStringList   mKeys;
                QList<Record> mRecords;
                int           mIndex{ 0 };
        };

}


#endif // merge_MemoryMerge_hpp
