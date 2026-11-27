//  Executor.hpp
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

#ifndef cli_Executor_hpp
#define cli_Executor_hpp


#include "Inputs.hpp"
#include "JobSpec.hpp"
#include "Prompter.hpp"
#include "Reporter.hpp"

#include "model/FieldScan.hpp"
#include "model/Model.hpp"
#include "model/PageRenderer.hpp"

#include <QList>
#include <QMap>

#include <memory>


namespace glabels::cli
{

        ///
        /// Run-wide switches (the global and behavior flags of `glabelsneo print`).
        ///
        struct RunSettings
        {
                bool interactive{ false };      ///< -i: ask for every field
                bool canPrompt{ false };        ///< a person is at a terminal and prompts are allowed
                bool yes{ false };              ///< -y: do not ask for confirmation
                bool dryRun{ false };
                bool forceFields{ false };      ///< --field beats the data file
                bool strict{ false };           ///< warnings are errors
                bool continueOnError{ false };
                int  maxLabels{ 1000 };         ///< 0 = no limit
                int  dpi{ 150 };                ///< resolution of PNG output
                printing::Driver driver{ printing::Driver::Auto };
        };


        ///
        /// Runs print jobs: preflights every job first (nothing is sent unless all of them
        /// check out), then prints them in order.
        ///
        class Executor
        {
        public:
                Executor( const RunSettings& settings, Reporter& reporter, Prompter* prompter = nullptr );

                /// Run the jobs.  Anything the person types at a prompt is written back into the
                /// jobs, so a caller can run them again with those values as the new defaults.
                /// Results and diagnostics are in the Reporter.
                void run( QList<JobSpec>& jobs );

                /// Did the last run send (or, with --dry-run, check) everything without error?
                bool succeeded() const;

                /// Were the last run's prompts answered (as opposed to cancelled at end of input)?
                bool cancelled() const;

                /// Effective field values of a label: job defaults, --field, and the label's own
                /// values, in the precedence the settings ask for.
                static QMap<QString,QString> effectiveFields( const JobSpec&                 job,
                                                              const QMap<QString,QString>&   label,
                                                              bool                           forceFields );

                /// Closest candidate to 'name' (edit distance, or case difference), or empty.
                static QString closestName( const QString& name, const QStringList& candidates );

        private:
                struct Prepared
                {
                        JobSpec                              spec;
                        std::unique_ptr<model::Model>        model;
                        std::unique_ptr<model::PageRenderer> renderer;
                        QString                              printerName;    ///< resolved installed name
                        QList<QMap<QString,QString>>         records;        ///< effective, copies expanded
                        bool                                 merged{ false };
                        int                                  nItems{ 0 };
                        int                                  nPages{ 0 };
                };

                bool prepare( JobSpec& job, Prepared& p );
                bool promptForFields( JobSpec& job, RecordSet& rs, bool& have, const QList<model::FieldReference>& refs, bool& prompted );
                bool promptForPrinter( QString& printer );
                bool resolveTarget( const JobSpec& job, Prepared& p );
                void checkFields( const Prepared& p, const QList<model::FieldReference>& refs, const QStringList& supplied );
                bool configureRenderer( Prepared& p );
                QString describe( const Prepared& p ) const;
                void printOne( Prepared& p );
                void abortRun( const QList<JobSpec>& jobs, int failedIndex );
                void fail( const JobSpec& job, int label, const QString& code, const QString& message, ExitCode exitCode );

                RunSettings  mSettings;
                Reporter&    mReporter;
                Prompter*    mPrompter;
                bool         mCancelled{ false };
                bool         mSucceeded{ false };
                bool         mPromptedAny{ false };
        };

}


#endif // cli_Executor_hpp
