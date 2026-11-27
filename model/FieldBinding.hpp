//  FieldBinding.hpp
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

#ifndef model_FieldBinding_hpp
#define model_FieldBinding_hpp


#include "ColumnMatcher.hpp"

#include <QList>
#include <QString>
#include <QStringList>


namespace glabels::model
{

        class Model;


        ///
        /// One field of a label and the source column that fills it.
        ///
        struct FieldBinding
        {
                QString     field;
                ColumnMatch match;
                bool        isComputed{ false };  ///< a variable that counts or steps itself; no column feeds it
                bool        isConstant{ false };  ///< a variable with a fixed value, or a field with a default
                bool        isUsed{ false };      ///< some object on the label refers to it

                /// The person still has to say which column fills this field.
                bool needsColumn() const
                {
                        return !isComputed && !isConstant &&
                               !match.isApplied() &&
                               match.kind != ColumnMatch::Kind::Blank;
                }
        };


        ///
        /// Works out, for a label and the columns of its data source, which column fills which field.
        ///
        class FieldBindings
        {
        public:
                /// Every field of the label (variables and `${...}` references), sorted by name.
                static QList<FieldBinding> compute( const Model& model, const QStringList& columns );

                /// Same, using the columns the model's linked source last returned.
                static QList<FieldBinding> compute( const Model& model );

                /// Fields that no column fills yet.
                static QList<FieldBinding> needingColumn( const QList<FieldBinding>& bindings );

                /// Field names only.
                static QStringList fieldNames( const Model& model );
        };

}


#endif // model_FieldBinding_hpp
