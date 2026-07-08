//  FillJobModel.cpp
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


#include "FillJobModel.hpp"


namespace glabels
{


        FillJobModel::FillJobModel( QObject* parent )
                : QAbstractTableModel( parent )
        {
        }


        void FillJobModel::setFieldNames( const QStringList& names )
        {
                beginResetModel();
                mFieldNames = names;
                endResetModel();
        }


        void FillJobModel::setLockedFields( const QStringList& fields )
        {
                mLockedFields = fields;
                // A full model reset is the only reliable way to make
                // QTableView re-query flags() for every cell.
                beginResetModel();
                endResetModel();
        }


        bool FillJobModel::isLockedField( const QString& name ) const
        {
                return mLockedFields.contains( name );
        }


        void FillJobModel::setJobs( const QList<FillJob>& jobs )
        {
                beginResetModel();
                mJobs = jobs;
                endResetModel();
        }


        void FillJobModel::appendJob()
        {
                int row = mJobs.size();
                beginInsertRows( QModelIndex(), row, row );
                mJobs.append( FillJob() );
                endInsertRows();
        }


        void FillJobModel::clear()
        {
                beginResetModel();
                mJobs.clear();
                endResetModel();
        }


        const FillJob* FillJobModel::jobAt( int row ) const
        {
                if ( row < 0 || row >= mJobs.size() )
                {
                        return nullptr;
                }
                return &mJobs.at( row );
        }


        int FillJobModel::totalQuantity() const
        {
                int total = 0;
                for ( const auto& job : mJobs )
                {
                        total += job.quantity;
                }
                return total;
        }


        int FillJobModel::checkedQuantity() const
        {
                int total = 0;
                for ( const auto& job : mJobs )
                {
                        if ( job.doPrint )
                        {
                                total += job.quantity;
                        }
                }
                return total;
        }


        int FillJobModel::checkedCount() const
        {
                int n = 0;
                for ( const auto& job : mJobs )
                {
                        if ( job.doPrint )
                        {
                                ++n;
                        }
                }
                return n;
        }


        int FillJobModel::rowCount( const QModelIndex& parent ) const
        {
                return parent.isValid() ? 0 : mJobs.size();
        }


        int FillJobModel::columnCount( const QModelIndex& parent ) const
        {
                return parent.isValid() ? 0 : ( FIRST_FIELD_COLUMN + mFieldNames.size() );
        }


        QVariant FillJobModel::headerData( int section, Qt::Orientation orientation,
                                            int role ) const
        {
                if ( role != Qt::DisplayRole )
                {
                        return {};
                }

                if ( orientation == Qt::Horizontal )
                {
                        if ( section == QUANTITY_COLUMN ) return tr( "Qty" );
                        if ( section == PRINT_COLUMN )    return tr( "Print" );
                        int fieldIndex = section - FIRST_FIELD_COLUMN;
                        if ( fieldIndex >= 0 && fieldIndex < mFieldNames.size() )
                        {
                                return mFieldNames.at( fieldIndex );
                        }
                }
                return {};
        }


        Qt::ItemFlags FillJobModel::flags( const QModelIndex& index ) const
        {
                if ( !index.isValid() )
                {
                        return Qt::NoItemFlags;
                }

                Qt::ItemFlags f = QAbstractTableModel::flags( index ) | Qt::ItemIsEnabled | Qt::ItemIsSelectable;

                if ( index.column() == PRINT_COLUMN )
                {
                        f |= Qt::ItemIsUserCheckable;
                }
                else if ( index.column() >= FIRST_FIELD_COLUMN )
                {
                        int fieldIndex = index.column() - FIRST_FIELD_COLUMN;
                        if ( fieldIndex >= 0 && fieldIndex < mFieldNames.size() )
                        {
                                if ( !isLockedField( mFieldNames.at( fieldIndex ) ) )
                                {
                                        f |= Qt::ItemIsEditable;
                                }
                        }
                }
                else
                {
                        f |= Qt::ItemIsEditable;
                }
                return f;
        }


        QVariant FillJobModel::data( const QModelIndex& index, int role ) const
        {
                if ( !index.isValid() )
                {
                        return {};
                }

                int row = index.row();
                int col = index.column();
                if ( row < 0 || row >= mJobs.size() )
                {
                        return {};
                }

                const FillJob& job = mJobs.at( row );

                if ( col == QUANTITY_COLUMN )
                {
                        if ( role == Qt::DisplayRole || role == Qt::EditRole )
                        {
                                return job.quantity;
                        }
                }
                else if ( col == PRINT_COLUMN )
                {
                        if ( role == Qt::CheckStateRole )
                        {
                                return job.doPrint ? Qt::Checked : Qt::Unchecked;
                        }
                        if ( role == Qt::DisplayRole )
                        {
                                return {};
                        }
                }
                else
                {
                        int fieldIndex = col - FIRST_FIELD_COLUMN;
                        if ( fieldIndex >= 0 && fieldIndex < mFieldNames.size() )
                        {
                                QString name = mFieldNames.at( fieldIndex );
                                if ( role == Qt::DisplayRole || role == Qt::EditRole )
                                {
                                        return job.values.value( name );
                                }
                        }
                }
                return {};
        }


        bool FillJobModel::setData( const QModelIndex& index, const QVariant& value, int role )
        {
                if ( !index.isValid() )
                {
                        return false;
                }

                int row = index.row();
                int col = index.column();
                if ( row < 0 || row >= mJobs.size() )
                {
                        return false;
                }

                FillJob& job = mJobs[row];

                if ( col == QUANTITY_COLUMN && role == Qt::EditRole )
                {
                        int q = value.toInt();
                        job.quantity = ( q < 1 ) ? 1 : q;
                        emit dataChanged( index, index, { role } );
                        return true;
                }
                if ( col == PRINT_COLUMN && role == Qt::CheckStateRole )
                {
                        job.doPrint = ( value.value<Qt::CheckState>() == Qt::Checked );
                        emit dataChanged( index, index, { role } );
                        return true;
                }
                if ( col >= FIRST_FIELD_COLUMN && role == Qt::EditRole )
                {
                        int fieldIndex = col - FIRST_FIELD_COLUMN;
                        if ( fieldIndex >= 0 && fieldIndex < mFieldNames.size() )
                        {
                                job.values.insert( mFieldNames.at( fieldIndex ), value.toString() );
                                emit dataChanged( index, index, { role } );
                                return true;
                        }
                }
                return false;
        }


        bool FillJobModel::insertRows( int row, int count, const QModelIndex& parent )
        {
                if ( parent.isValid() )
                {
                        return false;
                }
                beginInsertRows( parent, row, row + count - 1 );
                for ( int i = 0; i < count; ++i )
                {
                        mJobs.insert( row + i, FillJob() );
                }
                endInsertRows();
                return true;
        }


        bool FillJobModel::removeRows( int row, int count, const QModelIndex& parent )
        {
                if ( parent.isValid() || row < 0 || row + count > mJobs.size() )
                {
                        return false;
                }
                beginRemoveRows( parent, row, row + count - 1 );
                for ( int i = 0; i < count; ++i )
                {
                        mJobs.removeAt( row );
                }
                endRemoveRows();
                return true;
        }


} // namespace glabels
