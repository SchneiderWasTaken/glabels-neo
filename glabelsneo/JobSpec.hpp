//  JobSpec.hpp
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

#ifndef cli_JobSpec_hpp
#define cli_JobSpec_hpp


#include "printing/PrintService.hpp"

#include <QList>
#include <QMap>
#include <QString>

#include <optional>


namespace glabels::cli
{

        ///
        /// Quantity and layout options.  Unset means "use the default", so command-line
        /// flags can override a job document one option at a time.
        ///
        struct PrintOptions
        {
                std::optional<int>  copies;
                std::optional<int>  sheets;
                std::optional<int>  first;
                std::optional<bool> collate;
                std::optional<bool> group;
                std::optional<bool> outlines;
                std::optional<bool> cropMarks;
                std::optional<bool> reverse;

                /// Overlay the options that 'other' sets.
                void overlay( const PrintOptions& other );
        };


        ///
        /// One label to print: its field values and how many copies.
        ///
        struct LabelSpec
        {
                QMap<QString,QString> fields;
                int                   copies{ 1 };
        };


        ///
        /// Everything needed to print one label file.  Command-line flags and JSON job
        /// documents both produce these, so they cannot behave differently.
        ///
        struct JobSpec
        {
                QString id;                          ///< caller's correlation id (optional)
                int     index{ 0 };                  ///< position in the run, 0-based

                QString file;                        ///< label file, or "-" for standard input
                QString printer;                     ///< printer name or "default" ...
                QString output;                      ///< ... or a PDF file ("-" = standard output)

                PrintOptions          options;
                QMap<QString,QString> variables;     ///< user variable initial values
                QMap<QString,QString> defaults;      ///< job document: fields every label starts from
                QMap<QString,QString> cliFields;     ///< --field values

                QList<LabelSpec> labels;             ///< records given inline
                bool             hasLabels{ false }; ///< "labels" was present (even if empty)
                QString          data;               ///< records file ("-" = standard input)
                QString          dataFormat;         ///< json, jsonl, csv, ... (empty = by extension)

                /// "job 2" or the id, for messages.
                QString name() const
                {
                        return id.isEmpty() ? QString( "%1" ).arg( index + 1 ) : id;
                }
        };

}


#endif // cli_JobSpec_hpp
