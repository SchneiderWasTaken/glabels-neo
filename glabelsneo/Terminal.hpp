//  Terminal.hpp
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

#ifndef cli_Terminal_hpp
#define cli_Terminal_hpp


#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QStringList>


namespace glabels::cli
{

        ///
        /// Standard streams, encoded as UTF-8 on every platform.
        ///
        namespace Terminal
        {
                /// Call once at start-up (Windows: UTF-8 console code pages).
                void init();

                bool stdinIsTty();
                bool stdoutIsTty();

                /// Everything left on standard input (binary-safe).
                QByteArray readAllStdin();

                /// One line from standard input without its line ending.  False at end of input.
                bool readLineStdin( QString& line );

                /// UTF-8 text to standard output / standard error, flushed immediately.
                void writeOut( const QString& text );
                void writeErr( const QString& text );

                /// Route Qt/library diagnostics (qWarning and friends) into a buffer instead of the
                /// terminal; with echo=true they are also written to standard error (--verbose).
                void installLogHandler( bool echo );

                /// Take (and clear) the buffered diagnostics.
                QStringList takeLog();

                /// Raw standard output, for binary payloads (PDF).
                QIODevice* stdoutDevice();
        }

}


#endif // cli_Terminal_hpp
