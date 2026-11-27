//  JsonMerge.hpp
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

#ifndef merge_JsonMerge_hpp
#define merge_JsonMerge_hpp


#include "Merge.hpp"
#include "JsonRecords.hpp"


namespace glabels::merge
{

        ///
        /// JSON merge backends: a file holding an array of flat objects (Json), or one flat
        /// object per line (JsonLines).  See JsonRecords for the value rules and the
        /// "_copies" metadata key.
        ///
        class JsonMerge : public Merge
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        protected:
                JsonMerge( const QString& id, bool lines );
                JsonMerge( const JsonMerge* other );
                ~JsonMerge() override = default;


                /////////////////////////////////
                // Implementation of virtual methods
                /////////////////////////////////
        public:
                QStringList keys() const override;
                QString primaryKey() const override;

                /// Why the last attempt to read the source failed (empty when it did not)
                QString lastError() const;
        protected:
                void open() override;
                void close() override;
                Record readNextRecord() override;


                /////////////////////////////////
                // Private Data
                /////////////////////////////////
        private:
                bool          mLines;
                QStringList   mKeys;
                QList<Record> mRecords;   // "_copies" already expanded
                int           mIndex{ 0 };
                QString       mError;
        };


        ///
        /// JSON array of records
        ///
        class Json : public JsonMerge
        {
                Q_OBJECT
        private:
                Json();
                Json( const Json* other );
        public:
                Json* clone() const override;
                static QString id();
                static Merge* create();
        };


        ///
        /// JSON Lines (one record per line)
        ///
        class JsonLines : public JsonMerge
        {
                Q_OBJECT
        private:
                JsonLines();
                JsonLines( const JsonLines* other );
        public:
                JsonLines* clone() const override;
                static QString id();
                static Merge* create();
        };

}


#endif // merge_JsonMerge_hpp
