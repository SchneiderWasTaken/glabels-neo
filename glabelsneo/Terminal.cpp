//  Terminal.cpp
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

#include "Terminal.hpp"

#include <QFile>

#include <cstdio>

#if defined(Q_OS_WIN)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif


namespace
{

#if defined(Q_OS_WIN)
        ///
        /// Is this standard handle a person's terminal?  _isatty() says yes for the NUL device
        /// (so "< NUL" would look interactive), so ask for a real console instead.  Terminals that
        /// are not consoles - mintty in Git Bash and MSYS2 - hand programs a named pipe whose name
        /// says "msys-...-pty" or "cygwin-...-pty"; recognise that too.
        ///
        bool isInteractive( DWORD stdHandle )
        {
                HANDLE handle = GetStdHandle( stdHandle );
                if ( handle == nullptr || handle == INVALID_HANDLE_VALUE )
                {
                        return false;
                }

                DWORD mode = 0;
                if ( GetConsoleMode( handle, &mode ) )
                {
                        return true;
                }

                if ( GetFileType( handle ) == FILE_TYPE_PIPE )
                {
                        alignas( FILE_NAME_INFO ) char buffer[ sizeof( FILE_NAME_INFO ) + MAX_PATH * sizeof( WCHAR ) ] = {};
                        if ( GetFileInformationByHandleEx( handle, FileNameInfo, buffer, sizeof( buffer ) ) )
                        {
                                const auto* info = reinterpret_cast<const FILE_NAME_INFO*>( buffer );
                                const QString name = QString::fromWCharArray( info->FileName, static_cast<int>( info->FileNameLength / sizeof( WCHAR ) ) );
                                return ( name.contains( "msys-" ) || name.contains( "cygwin-" ) ) && name.contains( "-pty" );
                        }
                }

                return false;
        }
#endif


        QFile& outFile()
        {
                static QFile file;
                if ( !file.isOpen() )
                {
                        file.open( stdout, QIODevice::WriteOnly | QIODevice::Unbuffered );
                }
                return file;
        }


        QFile& errFile()
        {
                static QFile file;
                if ( !file.isOpen() )
                {
                        file.open( stderr, QIODevice::WriteOnly | QIODevice::Unbuffered );
                }
                return file;
        }


        QFile& inFile()
        {
                static QFile file;
                if ( !file.isOpen() )
                {
                        file.open( stdin, QIODevice::ReadOnly );
                }
                return file;
        }

}


namespace glabels::cli::Terminal
{

        void init()
        {
#if defined(Q_OS_WIN)
                // Binary mode so PDFs and JSON are not mangled by CRLF translation.
                _setmode( _fileno( stdin ),  _O_BINARY );
                _setmode( _fileno( stdout ), _O_BINARY );
                _setmode( _fileno( stderr ), _O_BINARY );

                // Only touch the code page of a real console.
                if ( isInteractive( STD_OUTPUT_HANDLE ) || isInteractive( STD_ERROR_HANDLE ) )
                {
                        SetConsoleOutputCP( CP_UTF8 );
                }
                if ( isInteractive( STD_INPUT_HANDLE ) )
                {
                        SetConsoleCP( CP_UTF8 );
                }
#endif
        }


        bool stdinIsTty()
        {
                // Test seam: lets the unit tests exercise the prompting paths without a real terminal.
                if ( qEnvironmentVariableIntValue( "GLABELSNEO_ASSUME_TTY" ) != 0 )
                {
                        return true;
                }

#if defined(Q_OS_WIN)
                return isInteractive( STD_INPUT_HANDLE );
#else
                return isatty( fileno( stdin ) ) != 0;
#endif
        }


        bool stdoutIsTty()
        {
#if defined(Q_OS_WIN)
                return isInteractive( STD_OUTPUT_HANDLE );
#else
                return isatty( fileno( stdout ) ) != 0;
#endif
        }


        QByteArray readAllStdin()
        {
                return inFile().readAll();
        }


        bool readLineStdin( QString& line )
        {
                QFile& in = inFile();
                if ( in.atEnd() )
                {
                        return false;
                }

                QByteArray bytes = in.readLine();
                if ( bytes.isEmpty() )
                {
                        return false;
                }

                while ( bytes.endsWith( '\n' ) || bytes.endsWith( '\r' ) )
                {
                        bytes.chop( 1 );
                }
                line = QString::fromUtf8( bytes );
                return true;
        }


        void writeOut( const QString& text )
        {
                outFile().write( text.toUtf8() );
        }


        void writeErr( const QString& text )
        {
                errFile().write( text.toUtf8() );
        }


        namespace
        {
                QStringList sLog;
                bool        sEcho{ false };

                void logHandler( QtMsgType type, const QMessageLogContext&, const QString& message )
                {
                        if ( type == QtDebugMsg && !sEcho )
                        {
                                return;
                        }

                        // Qt's own platform chatter is of no use to a person running a label job.
                        sLog.append( message );
                        if ( sEcho )
                        {
                                writeErr( message + "\n" );
                        }
                }
        }


        void installLogHandler( bool echo )
        {
                sEcho = echo;
                qInstallMessageHandler( logHandler );
        }


        QStringList takeLog()
        {
                QStringList log = sLog;
                sLog.clear();
                return log;
        }


        QIODevice* stdoutDevice()
        {
                return &outFile();
        }

}
