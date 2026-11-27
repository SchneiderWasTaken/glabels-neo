//  FieldScan.hpp
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

#ifndef model_FieldScan_hpp
#define model_FieldScan_hpp


#include <QList>
#include <QString>


namespace glabels::model
{

        // Forward references
        class Model;


        ///
        /// A field name the label refers to, as `${name}`, in text, barcode data, image file
        /// names or colors.
        ///
        struct FieldReference
        {
                QString name;
                bool    hasDefault{ false };  ///< every use supplies a `:=default` value
                bool    isVariable{ false };  ///< the file declares a user variable with this name
        };


        ///
        /// Finds the fields a label file expects to be filled in.
        ///
        class FieldScan
        {
        public:
                /// Fields referenced by the model's objects, once each, in order of first use.
                static QList<FieldReference> scan( const Model& model );
        };

}


#endif // model_FieldScan_hpp
