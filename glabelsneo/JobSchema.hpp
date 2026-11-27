//  JobSchema.hpp
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

#ifndef cli_JobSchema_hpp
#define cli_JobSchema_hpp


#include <QString>


namespace glabels::cli
{

        ///
        /// JSON Schema (draft 2020-12) of the job document read by `glabelsneo print --job`,
        /// so programs can validate a document before sending it.  Keep in step with JobLoader.
        ///
        QString jobSchema();

}


#endif // cli_JobSchema_hpp
