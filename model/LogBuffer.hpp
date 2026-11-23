//  LogBuffer.hpp
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


#ifndef model_LogBuffer_hpp
#define model_LogBuffer_hpp


#include <QString>


namespace glabels::model::LogBuffer
{

        ///
        /// Start remembering the most recent Qt log messages (warnings, errors, debug output) in
        /// memory, so a bug report can include them if the user chooses.  The previous message
        /// handler keeps working.  Nothing is written to disk or sent anywhere by this class.
        ///
        void install();


        ///
        /// Recent messages, oldest first, one per line.  Not redacted.
        ///
        QString recent();


        ///
        /// Add a message (also used by tests).
        ///
        void append( const QString& line );

}


#endif // model_LogBuffer_hpp
