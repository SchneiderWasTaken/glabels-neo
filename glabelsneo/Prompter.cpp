//  Prompter.cpp
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

#include "Prompter.hpp"

#include "Terminal.hpp"


namespace glabels::cli
{

        bool Prompter::readLine( QString& line )
        {
                return Terminal::readLineStdin( line );
        }


        void Prompter::write( const QString& text )
        {
                Terminal::writeErr( text );
        }


        void Prompter::say( const QString& text )
        {
                write( text + "\n" );
        }


        bool Prompter::ask( const QString& question, const QString& defaultValue, QString& answer )
        {
                if ( defaultValue.isEmpty() )
                {
                        write( QString( "%1: " ).arg( question ) );
                }
                else
                {
                        write( QString( "%1 [%2]: " ).arg( question, defaultValue ) );
                }

                QString line;
                if ( !readLine( line ) )
                {
                        write( "\n" );
                        return false;
                }

                answer = line.isEmpty() ? defaultValue : line;
                return true;
        }


        bool Prompter::confirm( const QString& question, bool defaultYes, bool& answer )
        {
                for ( ;; )
                {
                        write( QString( "%1 %2 " ).arg( question, defaultYes ? "[Y/n]" : "[y/N]" ) );

                        QString line;
                        if ( !readLine( line ) )
                        {
                                write( "\n" );
                                return false;
                        }

                        const QString reply = line.trimmed().toLower();
                        if ( reply.isEmpty() )
                        {
                                answer = defaultYes;
                                return true;
                        }
                        if ( reply == "y" || reply == "yes" )
                        {
                                answer = true;
                                return true;
                        }
                        if ( reply == "n" || reply == "no" )
                        {
                                answer = false;
                                return true;
                        }

                        say( "Please answer y or n." );
                }
        }


        bool Prompter::choose( const QString& question, const QStringList& options, int defaultIndex, int& index )
        {
                say( question );
                for ( int i = 0; i < options.size(); i++ )
                {
                        say( QString( "  %1) %2%3" ).arg( i + 1 ).arg( options.at( i ), i == defaultIndex ? "  (default)" : "" ) );
                }

                for ( ;; )
                {
                        write( defaultIndex >= 0 ? QString( "Choose [%1]: " ).arg( defaultIndex + 1 ) : QString( "Choose: " ) );

                        QString line;
                        if ( !readLine( line ) )
                        {
                                write( "\n" );
                                return false;
                        }

                        const QString reply = line.trimmed();
                        if ( reply.isEmpty() && defaultIndex >= 0 )
                        {
                                index = defaultIndex;
                                return true;
                        }

                        bool ok = false;
                        const int n = reply.toInt( &ok );
                        if ( ok && n >= 1 && n <= options.size() )
                        {
                                index = n - 1;
                                return true;
                        }

                        const int byName = static_cast<int>( options.indexOf( reply ) );
                        if ( byName >= 0 )
                        {
                                index = byName;
                                return true;
                        }

                        say( "Not one of the choices." );
                }
        }

}
