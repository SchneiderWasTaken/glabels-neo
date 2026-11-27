//  FieldBinding.cpp
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

#include "FieldBinding.hpp"

#include "FieldScan.hpp"
#include "Model.hpp"
#include "Variable.hpp"

#include <QMap>
#include <QSet>
#include <algorithm>


namespace glabels::model
{

        QStringList FieldBindings::fieldNames( const Model& model )
        {
                QSet<QString> names;
                for ( const FieldReference& ref : FieldScan::scan( model ) )
                {
                        names.insert( ref.name );
                }
                for ( auto it = model.constVariables().constBegin(); it != model.constVariables().constEnd(); ++it )
                {
                        names.insert( it.key() );
                }

                QStringList list = names.values();
                list.sort();
                return list;
        }


        QList<FieldBinding> FieldBindings::compute( const Model& model, const QStringList& columns )
        {
                QMap<QString,FieldReference> refs;
                for ( const FieldReference& ref : FieldScan::scan( model ) )
                {
                        refs.insert( ref.name, ref );
                }

                const QMap<QString,QString>& columnMap = model.externalDataSource().columnMap;

                QList<FieldBinding> out;
                for ( const QString& name : fieldNames( model ) )
                {
                        FieldBinding b;
                        b.field  = name;
                        b.isUsed = refs.contains( name );
                        b.match  = ColumnMatcher::match( name, columns, columnMap );

                        if ( model.constVariables().hasVariable( name ) )
                        {
                                const Variable v = model.constVariables().value( name );
                                if ( v.increment() != Variable::Increment::NEVER )
                                {
                                        b.isComputed = true;
                                }
                                else if ( !v.initialValue().isEmpty() )
                                {
                                        b.isConstant = true;
                                }
                        }
                        if ( refs.contains( name ) && refs.value( name ).hasDefault )
                        {
                                b.isConstant = true;
                        }

                        out.append( b );
                }
                return out;
        }


        QList<FieldBinding> FieldBindings::compute( const Model& model )
        {
                return compute( model, model.sourceColumns() );
        }


        QList<FieldBinding> FieldBindings::needingColumn( const QList<FieldBinding>& bindings )
        {
                QList<FieldBinding> out;
                for ( const FieldBinding& b : bindings )
                {
                        if ( b.needsColumn() )
                        {
                                out.append( b );
                        }
                }
                return out;
        }

}
