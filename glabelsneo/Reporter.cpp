//  Reporter.cpp
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

#include "Reporter.hpp"

#include "Terminal.hpp"

#include <QJsonArray>
#include <QJsonDocument>


namespace
{

        QString context( const glabels::cli::Diagnostic& d )
        {
                QString where;
                if ( !d.job.isEmpty() )
                {
                        where += QString( " [job %1" ).arg( d.job );
                        if ( d.label > 0 )
                        {
                                where += QString( ", label %1" ).arg( d.label );
                        }
                        where += "]";
                }
                else if ( d.label > 0 )
                {
                        where += QString( " [label %1]" ).arg( d.label );
                }
                return where;
        }

}


namespace glabels::cli
{

        Reporter::Reporter( bool json, bool quiet, bool verbose )
                : mJson( json ), mQuiet( quiet ), mVerbose( verbose )
        {
        }


        void Reporter::setStdoutBusy( bool busy )
        {
                mStdoutBusy = busy;
        }


        void Reporter::warning( const Diagnostic& d )
        {
                mWarnings.append( d );
                if ( !mJson && !mQuiet )
                {
                        Terminal::writeErr( QString( "glabelsneo: warning: %1%2\n" ).arg( d.message, context( d ) ) );
                }
        }


        void Reporter::error( const Diagnostic& d )
        {
                mErrors.append( d );
                if ( !mJson )
                {
                        Terminal::writeErr( QString( "glabelsneo: error: %1%2\n" ).arg( d.message, context( d ) ) );
                }
        }


        void Reporter::jobDone( const JobResult& r )
        {
                mJobs.append( r );
        }


        void Reporter::info( const QString& text )
        {
                if ( mJson || mQuiet )
                {
                        return;
                }

                if ( mStdoutBusy )
                {
                        Terminal::writeErr( text + "\n" );
                }
                else
                {
                        Terminal::writeOut( text + "\n" );
                }
        }


        void Reporter::debug( const QString& text )
        {
                if ( mVerbose )
                {
                        Terminal::writeErr( "glabelsneo: " + text + "\n" );
                }
        }


        bool Reporter::hasErrors() const
        {
                return !mErrors.isEmpty();
        }


        int Reporter::nWarnings() const
        {
                return static_cast<int>( mWarnings.size() );
        }


        const QList<Diagnostic>& Reporter::warnings() const
        {
                return mWarnings;
        }


        const QList<Diagnostic>& Reporter::errors() const
        {
                return mErrors;
        }


        QJsonObject Reporter::toJson( const Diagnostic& d )
        {
                QJsonObject o;
                o["code"]    = d.code;
                o["message"] = d.message;
                if ( !d.job.isEmpty() )
                {
                        o["job"] = d.job;
                }
                if ( d.label > 0 )
                {
                        o["label"] = d.label;
                }
                return o;
        }


        int Reporter::finish( bool dryRun, bool continueOnError )
        {
                int exitCode = ExitOk;

                if ( !mErrors.isEmpty() )
                {
                        bool anyPrinted = false;
                        for ( const JobResult& r : mJobs )
                        {
                                if ( r.status == "printed" || r.status == "pdf" || r.status == "image" )
                                {
                                        anyPrinted = true;
                                }
                        }

                        exitCode = ( continueOnError && anyPrinted ) ? static_cast<int>( ExitPartial )
                                                                     : static_cast<int>( mErrors.first().exitCode );
                }

                if ( mJson )
                {
                        QJsonObject root;
                        root["ok"]     = mErrors.isEmpty();
                        root["dryRun"] = dryRun;

                        QJsonArray warnings;
                        for ( const Diagnostic& d : mWarnings )
                        {
                                warnings.append( toJson( d ) );
                        }
                        root["warnings"] = warnings;

                        QJsonArray jobs;
                        for ( const JobResult& r : mJobs )
                        {
                                QJsonObject o;
                                if ( !r.id.isEmpty() )
                                {
                                        o["id"] = r.id;
                                }
                                o["index"]  = r.index;
                                o["status"] = r.status;
                                if ( !r.file.isEmpty() )
                                {
                                        o["file"] = r.file;
                                }
                                if ( !r.printer.isEmpty() )
                                {
                                        o["printer"] = r.printer;
                                }
                                if ( !r.output.isEmpty() )
                                {
                                        o["output"] = r.output;
                                }
                                o["labels"] = r.labels;
                                o["pages"]  = r.pages;
                                jobs.append( o );
                        }
                        root["jobs"] = jobs;

                        QJsonArray errors;
                        for ( const Diagnostic& d : mErrors )
                        {
                                errors.append( toJson( d ) );
                        }
                        root["errors"] = errors;
                        root["exitCode"] = exitCode;

                        Terminal::writeOut( QString::fromUtf8( QJsonDocument( root ).toJson( QJsonDocument::Compact ) ) + "\n" );
                }

                return exitCode;
        }

}
