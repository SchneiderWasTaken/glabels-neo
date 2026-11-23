//  LogBuffer.cpp
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


#include "LogBuffer.hpp"

#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QtLogging>

#include <cstdio>


namespace glabels::model::LogBuffer
{

        namespace
        {
                const int MAX_LINES = 300;
                const int MAX_LINE_LENGTH = 400;

                QMutex            mutex;
                QStringList       lines;
                QtMessageHandler  previousHandler = nullptr;
                bool              installed = false;

                void handler( QtMsgType type, const QMessageLogContext& context, const QString& message )
                {
                        const QString formatted = qFormatLogMessage( type, context, message );
                        append( formatted );

                        if ( previousHandler )
                        {
                                previousHandler( type, context, message );
                        }
                        else
                        {
                                // No custom handler was installed before ours: keep the default
                                // behaviour of writing to stderr.
                                fputs( qPrintable( formatted ), stderr );
                                fputc( '\n', stderr );
                                fflush( stderr );
                        }
                }
        }


        void install()
        {
                QMutexLocker lock( &mutex );
                if ( !installed )
                {
                        previousHandler = qInstallMessageHandler( handler );
                        installed = true;
                }
        }


        void append( const QString& line )
        {
                QMutexLocker lock( &mutex );
                lines.append( line.left( MAX_LINE_LENGTH ) );
                while ( lines.size() > MAX_LINES )
                {
                        lines.removeFirst();
                }
        }


        QString recent()
        {
                QMutexLocker lock( &mutex );
                return lines.join( '\n' );
        }

}
