//  Prompter.hpp
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

#ifndef cli_Prompter_hpp
#define cli_Prompter_hpp


#include <QString>
#include <QStringList>


namespace glabels::cli
{

        ///
        /// Terminal prompts.  Questions go to standard error so that standard output stays
        /// clean for results; answers are read from standard input.  Every function returns
        /// false when input ends (end of file / Ctrl-D), which callers treat as "cancelled".
        ///
        /// A virtual seam lets tests script the answers.
        ///
        class Prompter
        {
        public:
                virtual ~Prompter() = default;

                /// Free text.  Pressing Enter keeps 'defaultValue'.
                virtual bool ask( const QString& question, const QString& defaultValue, QString& answer );

                /// Yes/no.  Pressing Enter takes the default.
                virtual bool confirm( const QString& question, bool defaultYes, bool& answer );

                /// Pick one of 'options' by number or by exact text.  Returns the chosen index.
                virtual bool choose( const QString& question, const QStringList& options, int defaultIndex, int& index );

                /// Tell the person something.
                virtual void say( const QString& text );

        protected:
                virtual bool readLine( QString& line );
                virtual void write( const QString& text );
        };

}


#endif // cli_Prompter_hpp
