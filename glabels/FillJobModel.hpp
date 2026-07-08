//  FillJobModel.hpp
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

#ifndef FillJobModel_hpp
#define FillJobModel_hpp


#include <QAbstractTableModel>
#include <QMap>
#include <QStringList>


namespace glabels
{


        ///
        /// A single fill job: a print quantity, a field-name -> value map,
        /// and a "print" flag so the user can stage rows for the next run
        /// without losing them.
        ///
        struct FillJob
        {
                int                   quantity{ 1 };
                bool                  doPrint{ true };
                QMap<QString,QString> values;
        };


        ///
        /// Table model backing the Fill page's jobs table.
        ///
        /// Column layout:
        ///   0  Quantity        (editable spin)
        ///   1  Print           (checkbox)
        ///   2..N  field names  (editable text)
        ///
        class FillJobModel : public QAbstractTableModel
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                explicit FillJobModel( QObject* parent = nullptr );


                /////////////////////////////////
                // Column indices
                /////////////////////////////////
        public:
                static const int QUANTITY_COLUMN = 0;
                static const int PRINT_COLUMN    = 1;
                static const int FIRST_FIELD_COLUMN = 2;


                /////////////////////////////////
                // Fields
                /////////////////////////////////
        public:
                QStringList fieldNames() const { return mFieldNames; }
                void setFieldNames( const QStringList& names );

                // Fields that are read-only to the employee (pre-filled from a
                // source).  Used by the kiosk to lock columns.
                QStringList lockedFields() const { return mLockedFields; }
                void setLockedFields( const QStringList& fields );
                bool isLockedField( const QString& name ) const;


                /////////////////////////////////
                // Jobs
                /////////////////////////////////
        public:
                const QList<FillJob>& jobs() const { return mJobs; }
                void setJobs( const QList<FillJob>& jobs );

                void appendJob();
                void clear();

                const FillJob* jobAt( int row ) const;

                int totalQuantity() const;
                int checkedQuantity() const;
                int checkedCount() const;


                /////////////////////////////////
                // QAbstractTableModel
                /////////////////////////////////
        public:
                int rowCount( const QModelIndex& parent = QModelIndex() ) const override;
                int columnCount( const QModelIndex& parent = QModelIndex() ) const override;

                QVariant headerData( int section, Qt::Orientation orientation,
                                     int role = Qt::DisplayRole ) const override;

                Qt::ItemFlags flags( const QModelIndex& index ) const override;

                QVariant data( const QModelIndex& index,
                               int role = Qt::DisplayRole ) const override;
                bool setData( const QModelIndex& index, const QVariant& value,
                              int role = Qt::EditRole ) override;

                bool insertRows( int row, int count,
                                 const QModelIndex& parent = QModelIndex() ) override;
                bool removeRows( int row, int count,
                                 const QModelIndex& parent = QModelIndex() ) override;


                /////////////////////////////////
                // Private Data
                /////////////////////////////////
        private:
                QStringList    mFieldNames;
                QStringList    mLockedFields;
                QList<FillJob> mJobs;
        };


}


#endif // FillJobModel_hpp
