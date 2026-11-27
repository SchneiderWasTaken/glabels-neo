//  Inputs.hpp
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

#ifndef cli_Inputs_hpp
#define cli_Inputs_hpp


#include "JobSpec.hpp"
#include "Reporter.hpp"

#include "model/Model.hpp"

#include <QList>
#include <QStringList>

#include <memory>


namespace glabels::cli
{

        ///
        /// Records read from a data source.
        ///
        struct RecordSet
        {
                QList<LabelSpec> labels;
                QStringList      keys;      ///< field names in source order (may be empty for JSON {} records)
        };


        ///
        /// Reading label files and record sources.
        ///
        namespace Inputs
        {
                /// Load a label file ("-" = standard input).  On failure returns null and fills
                /// 'error' (code input.missing / input.invalid, exit code 3).
                std::unique_ptr<model::Model> loadModel( const QString& file, Diagnostic& error );

                /// Pick the record format: 'explicitFormat' if given, otherwise from the file extension.
                /// Returns an empty string (and fills 'error') if it cannot be told.
                QString detectFormat( const QString& source, const QString& explicitFormat, Diagnostic& error );

                /// Read records from a file ("-" = standard input).
                bool loadRecords( const QString& source,
                                  const QString& format,
                                  RecordSet&     out,
                                  Diagnostic&    error );

                /// The selected records of the merge source stored inside a label file, if it has one.
                bool recordsFromModelMerge( const model::Model& model, RecordSet& out );

                /// The formats --data-format accepts.
                QStringList knownFormats();
        }

}


#endif // cli_Inputs_hpp
