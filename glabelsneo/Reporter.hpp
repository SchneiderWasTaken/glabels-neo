//  Reporter.hpp
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

#ifndef cli_Reporter_hpp
#define cli_Reporter_hpp


#include "Exit.hpp"

#include <QJsonObject>
#include <QList>
#include <QString>


namespace glabels::cli
{

        ///
        /// A warning or error.  'code' is a stable identifier (part of the machine interface);
        /// 'message' is for people and may change.
        ///
        struct Diagnostic
        {
                QString code;
                QString message;
                QString job;          ///< job id, or empty
                int     label{ -1 };  ///< 1-based label number within the job, or -1
                ExitCode exitCode{ ExitInternal };  ///< errors only
        };


        ///
        /// What happened to one job.
        ///
        struct JobResult
        {
                QString id;
                int     index{ 0 };
                QString status;     ///< "printed", "pdf", "checked", "failed", "skipped"
                QString file;
                QString printer;
                QString output;
                int     labels{ 0 };
                int     pages{ 0 };
        };


        ///
        /// Collects everything a run produces and writes it either as readable text or as
        /// one JSON object.  Results go to standard output, diagnostics to standard error
        /// (text mode); JSON mode writes only the JSON object to standard output.
        ///
        class Reporter
        {
        public:
                Reporter( bool json, bool quiet, bool verbose );

                /// A PDF is going to standard output, so no human text may share it.
                void setStdoutBusy( bool busy );

                void warning( const Diagnostic& d );
                void error( const Diagnostic& d );
                void jobDone( const JobResult& r );

                /// Progress/information line (text mode only; hidden by --quiet).
                void info( const QString& text );

                /// Developer diagnostics (--verbose only), always to standard error.
                void debug( const QString& text );

                bool hasErrors() const;
                int  nWarnings() const;
                const QList<Diagnostic>& warnings() const;
                const QList<Diagnostic>& errors() const;

                /// Write the final report and return the process exit code.
                int finish( bool dryRun, bool continueOnError );

                static QJsonObject toJson( const Diagnostic& d );

        private:
                bool mJson;
                bool mQuiet;
                bool mVerbose;
                bool mStdoutBusy{ false };

                QList<Diagnostic> mWarnings;
                QList<Diagnostic> mErrors;
                QList<JobResult>  mJobs;
        };

}


#endif // cli_Reporter_hpp
