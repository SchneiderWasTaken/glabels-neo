//  FieldScan.cpp
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

#include "FieldScan.hpp"

#include "ColorNode.hpp"
#include "Model.hpp"
#include "ModelObject.hpp"
#include "RawText.hpp"
#include "SubstitutionField.hpp"
#include "TextNode.hpp"

#include <QMap>


namespace
{

        using glabels::model::FieldReference;


        void note( QList<FieldReference>& list, const QString& name, bool hasDefault )
        {
                if ( name.isEmpty() )
                {
                        return;
                }

                for ( FieldReference& ref : list )
                {
                        if ( ref.name == name )
                        {
                                // A name is only "defaulted" if every use of it is.
                                ref.hasDefault = ref.hasDefault && hasDefault;
                                return;
                        }
                }

                FieldReference ref;
                ref.name       = name;
                ref.hasDefault = hasDefault;
                list.append( ref );
        }


        void noteText( QList<FieldReference>& list, const QString& text )
        {
                if ( text.isEmpty() )
                {
                        return;
                }

                const glabels::model::RawText raw( text );
                for ( const auto& field : raw.fields() )
                {
                        note( list, field.fieldName(), !field.defaultValue().isEmpty() );
                }
        }


        void noteColor( QList<FieldReference>& list, const glabels::model::ColorNode& node )
        {
                if ( node.isField() )
                {
                        note( list, node.key(), false );
                }
        }

}


namespace glabels::model
{

        QList<FieldReference> FieldScan::scan( const Model& model )
        {
                QList<FieldReference> list;

                for ( const ModelObject* object : model.objectList() )
                {
                        noteText( list, object->text() );
                        noteText( list, object->bcData() );

                        const TextNode filename = object->filenameNode();
                        if ( filename.isField() )
                        {
                                note( list, filename.data(), false );
                        }

                        noteColor( list, object->textColorNode() );
                        noteColor( list, object->lineColorNode() );
                        noteColor( list, object->fillColorNode() );
                        noteColor( list, object->bcColorNode() );
                        noteColor( list, object->shadowColorNode() );
                }

                for ( FieldReference& ref : list )
                {
                        ref.isVariable = model.constVariables().hasVariable( ref.name );
                }

                return list;
        }

}
