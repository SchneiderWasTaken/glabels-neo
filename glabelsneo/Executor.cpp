//  Executor.cpp
//
//  Copyright (C) 2013-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//
//  The renderer configuration (simple project versus merge project, sheets, copies,
//  first position, collation, grouping) is the logic of the former glabels-batch-qt,
//  carried over unchanged in behavior.
//

#include "Executor.hpp"

#include "Terminal.hpp"

#include "merge/MemoryMerge.hpp"
#include "printing/PrintService.hpp"

#include <QDir>
#include <QVector>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <vector>


namespace
{

        QString plural( int n, const QString& one, const QString& many )
        {
                return QString( "%1 %2" ).arg( n ).arg( n == 1 ? one : many );
        }


        int editDistance( const QString& a, const QString& b )
        {
                QVector<int> previous( static_cast<int>( b.size() ) + 1 );
                QVector<int> current( static_cast<int>( b.size() ) + 1 );
                for ( int j = 0; j <= b.size(); j++ )
                {
                        previous[j] = j;
                }

                for ( int i = 1; i <= a.size(); i++ )
                {
                        current[0] = i;
                        for ( int j = 1; j <= b.size(); j++ )
                        {
                                const int cost = ( a[i-1] == b[j-1] ) ? 0 : 1;
                                current[j] = std::min( { previous[j] + 1, current[j-1] + 1, previous[j-1] + cost } );
                        }
                        std::swap( previous, current );
                }
                return previous[ b.size() ];
        }


        QString fileLabel( const QString& file )
        {
                return ( file == "-" ) ? QString( "standard input" ) : QFileInfo( file ).fileName();
        }

}


namespace glabels::cli
{

        Executor::Executor( const RunSettings& settings, Reporter& reporter, Prompter* prompter )
                : mSettings( settings ), mReporter( reporter ), mPrompter( prompter )
        {
        }


        bool Executor::succeeded() const
        {
                return mSucceeded;
        }


        bool Executor::cancelled() const
        {
                return mCancelled;
        }


        QMap<QString,QString> Executor::effectiveFields( const JobSpec&               job,
                                                         const QMap<QString,QString>& label,
                                                         bool                         forceFields )
        {
                auto overlay = []( QMap<QString,QString>& to, const QMap<QString,QString>& from ) {
                        for ( auto it = from.begin(); it != from.end(); ++it )
                        {
                                to.insert( it.key(), it.value() );
                        }
                };

                QMap<QString,QString> result = job.defaults;
                if ( forceFields )
                {
                        overlay( result, label );
                        overlay( result, job.cliFields );
                }
                else
                {
                        overlay( result, job.cliFields );
                        overlay( result, label );
                }
                return result;
        }


        QString Executor::closestName( const QString& name, const QStringList& candidates )
        {
                QString best;
                int     bestDistance = 1000;
                const int limit = std::max( 2, static_cast<int>( name.size() ) / 3 );

                for ( const QString& candidate : candidates )
                {
                        if ( candidate.compare( name, Qt::CaseInsensitive ) == 0 )
                        {
                                return candidate;
                        }

                        const int d = editDistance( name.toLower(), candidate.toLower() );
                        if ( d < bestDistance )
                        {
                                bestDistance = d;
                                best = candidate;
                        }
                }

                return ( bestDistance <= limit ) ? best : QString();
        }


        void Executor::fail( const JobSpec& job, int label, const QString& code, const QString& message, ExitCode exitCode )
        {
                Diagnostic d;
                d.code     = code;
                d.message  = message;
                d.job      = job.id;
                d.label    = label;
                d.exitCode = exitCode;
                mReporter.error( d );
        }


        void Executor::abortRun( const QList<JobSpec>& jobs, int failedIndex )
        {
                // Nothing was sent: say so for every job, naming the one that stopped the run.
                for ( int i = 0; i < jobs.size(); i++ )
                {
                        JobResult r;
                        r.id     = jobs[i].id;
                        r.index  = jobs[i].index;
                        r.status = ( i == failedIndex ) ? "failed" : "skipped";
                        r.file   = jobs[i].file;
                        mReporter.jobDone( r );
                }
        }


        bool Executor::promptForPrinter( QString& printer )
        {
                const QList<printing::PrinterEntry> printers = printing::PrintService::printers();
                if ( printers.isEmpty() )
                {
                        Diagnostic d;
                        d.code     = "printer.notfound";
                        d.message  = "no printers are installed";
                        d.exitCode = ExitPrinter;
                        mReporter.error( d );
                        return false;
                }

                QStringList names;
                int         defaultIndex = -1;
                for ( int i = 0; i < printers.size(); i++ )
                {
                        names.append( printers[i].name );
                        if ( printers[i].isDefault )
                        {
                                defaultIndex = i;
                        }
                }

                int index = 0;
                if ( !mPrompter->choose( "Which printer?", names, defaultIndex, index ) )
                {
                        mCancelled = true;
                        return false;
                }

                printer = names.at( index );
                return true;
        }


        bool Executor::promptForFields( JobSpec&                          job,
                                        RecordSet&                        rs,
                                        bool&                             have,
                                        const QList<model::FieldReference>& refs,
                                        bool&                             prompted )
        {
                if ( !mSettings.canPrompt || !mPrompter )
                {
                        return true;
                }

                QSet<QString> supplied;
                for ( const QString& key : job.defaults.keys() )   supplied.insert( key );
                for ( const QString& key : job.cliFields.keys() )  supplied.insert( key );
                for ( const LabelSpec& label : rs.labels )
                {
                        for ( const QString& key : label.fields.keys() )
                        {
                                supplied.insert( key );
                        }
                }

                // One label: ask about each field and keep the answers with that label.
                // A batch: only ask about fields nobody supplies, and treat the answer as a constant.
                const bool single = rs.labels.size() <= 1;
                bool       changedSingle = false;

                for ( const model::FieldReference& ref : refs )
                {
                        if ( ref.isVariable )
                        {
                                continue;
                        }

                        const bool missing = !ref.hasDefault && !supplied.contains( ref.name );
                        if ( !missing && !( mSettings.interactive && single ) )
                        {
                                continue;
                        }

                        QString current;
                        if ( single && !rs.labels.isEmpty() )
                        {
                                current = effectiveFields( job, rs.labels.first().fields, mSettings.forceFields ).value( ref.name );
                        }
                        else
                        {
                                current = job.cliFields.value( ref.name, job.defaults.value( ref.name ) );
                        }

                        QString answer;
                        if ( !mPrompter->ask( ref.name, current, answer ) )
                        {
                                mCancelled = true;
                                return false;
                        }
                        prompted = true;

                        if ( single )
                        {
                                if ( rs.labels.isEmpty() )
                                {
                                        rs.labels.append( LabelSpec() );
                                }
                                rs.labels.first().fields.insert( ref.name, answer );
                                // the typed value must win over --field/defaults for this label
                                job.cliFields.remove( ref.name );
                                job.defaults.remove( ref.name );
                                if ( !rs.keys.contains( ref.name ) )
                                {
                                        rs.keys.append( ref.name );
                                }
                                have = true;
                                changedSingle = true;
                        }
                        else
                        {
                                job.cliFields.insert( ref.name, answer );
                        }

                        supplied.insert( ref.name );
                }

                if ( changedSingle )
                {
                        // Remember what was typed, so a repeat run starts from it.
                        job.labels    = rs.labels;
                        job.hasLabels = true;
                        job.data.clear();
                        job.dataFormat.clear();
                }

                return true;
        }


        void Executor::checkFields( const Prepared& p, const QList<model::FieldReference>& refs, const QStringList& supplied )
        {
                const int nRecords = static_cast<int>( p.records.size() );

                QStringList refNames;
                QStringList unsatisfied;

                for ( const model::FieldReference& ref : refs )
                {
                        refNames.append( ref.name );

                        if ( ref.isVariable || ref.hasDefault )
                        {
                                continue;
                        }

                        int missing = 0;
                        int first   = -1;
                        if ( p.merged )
                        {
                                for ( int i = 0; i < nRecords; i++ )
                                {
                                        if ( !p.records[i].contains( ref.name ) )
                                        {
                                                missing++;
                                                if ( first < 0 ) first = i;
                                        }
                                }
                        }
                        else
                        {
                                missing = 1;
                        }

                        if ( missing == 0 )
                        {
                                continue;
                        }
                        unsatisfied.append( ref.name );

                        Diagnostic d;
                        d.code = "field.missing";
                        d.job  = p.spec.id;
                        if ( p.merged && nRecords > 1 )
                        {
                                d.message = QString( "the label uses field '%1' but %2 of %3 labels do not supply it; it will print blank" )
                                            .arg( ref.name ).arg( missing ).arg( nRecords );
                                d.label = first + 1;
                        }
                        else
                        {
                                d.message = QString( "the label uses field '%1' but no value was supplied; it will print blank" ).arg( ref.name );
                                d.label = -1;
                        }
                        mReporter.warning( d );
                }

                for ( const QString& key : supplied )
                {
                        if ( key.startsWith( '_' ) || refNames.contains( key ) )
                        {
                                continue;
                        }

                        // A near miss of something the label wants but did not get is the likely typo.
                        QString guess = closestName( key, unsatisfied );
                        if ( guess.isEmpty() )
                        {
                                guess = closestName( key, refNames );
                        }

                        Diagnostic d;
                        d.code    = "field.unused";
                        d.job     = p.spec.id;
                        d.message = QString( "field '%1' is not used by the label%2" )
                                    .arg( key, guess.isEmpty() ? QString() : QString( "; did you mean '%1'?" ).arg( guess ) );
                        mReporter.warning( d );
                }
        }


        bool Executor::resolveTarget( const JobSpec& job, Prepared& p )
        {
                if ( !job.printer.isEmpty() && !job.output.isEmpty() )
                {
                        fail( job, -1, "usage", "give either a printer or an output file, not both", ExitUsage );
                        return false;
                }

                if ( job.printer.isEmpty() && job.output.isEmpty() )
                {
                        fail( job, -1, "usage",
                              "no printer given: use --printer NAME (or --printer default), or --output FILE.pdf",
                              ExitUsage );
                        return false;
                }

                if ( !job.printer.isEmpty() )
                {
                        const printing::ResolvedPrinter r = printing::PrintService::resolvePrinter( job.printer );
                        switch ( r.status )
                        {
                        case printing::ResolvedPrinter::Status::Found:
                                p.printerName = r.name;
                                return true;

                        case printing::ResolvedPrinter::Status::NoDefault:
                                fail( job, -1, "printer.notfound", "there is no default printer; name one with --printer", ExitPrinter );
                                return false;

                        case printing::ResolvedPrinter::Status::Ambiguous:
                                fail( job, -1, "printer.ambiguous",
                                      QString( "'%1' matches more than one printer: %2" ).arg( job.printer, r.candidates.join( ", " ) ),
                                      ExitPrinter );
                                return false;

                        default:
                        {
                                QStringList names;
                                for ( const auto& entry : printing::PrintService::printers() )
                                {
                                        names.append( entry.name );
                                }
                                fail( job, -1, "printer.notfound",
                                      QString( "printer '%1' was not found. %2" )
                                      .arg( job.printer,
                                            names.isEmpty() ? QString( "No printers are installed." )
                                                            : QString( "Installed printers: %1" ).arg( names.join( ", " ) ) ),
                                      ExitPrinter );
                                return false;
                        }
                        }
                }

                if ( job.output != "-" )
                {
                        const QFileInfo info( job.output );
                        if ( info.isDir() )
                        {
                                fail( job, -1, "printer.failed", QString( "'%1' is a folder, not a PDF file name" ).arg( job.output ), ExitPrinter );
                                return false;
                        }
                        if ( !info.absoluteDir().exists() )
                        {
                                fail( job, -1, "printer.failed",
                                      QString( "the folder for '%1' does not exist" ).arg( job.output ), ExitPrinter );
                                return false;
                        }
                }
                return true;
        }


        bool Executor::configureRenderer( Prepared& p )
        {
                const JobSpec& job = p.spec;

                p.renderer = std::make_unique<model::PageRenderer>( p.model.get() );

                const int nLabelsPerSheet = p.model->frame()->nLabels();
                const int copies          = job.options.copies.value_or( 1 );
                const int first           = job.options.first.value_or( 1 );

                if ( first > nLabelsPerSheet )
                {
                        fail( job, -1, "usage",
                              QString( "--first %1 is beyond the %2 on a sheet" ).arg( first ).arg( plural( nLabelsPerSheet, "label", "labels" ) ),
                              ExitUsage );
                        return false;
                }

                if ( !p.merged )
                {
                        if ( job.options.sheets )
                        {
                                // Full sheets of simple items
                                p.renderer->setNCopies( *job.options.sheets * nLabelsPerSheet );
                                p.renderer->setStartItem( 0 );
                        }
                        else if ( job.options.copies )
                        {
                                // Partial sheet(s) of simple items
                                p.renderer->setNCopies( copies );
                                p.renderer->setStartItem( first - 1 );
                        }
                        else
                        {
                                // One full sheet of simple items
                                p.renderer->setNCopies( nLabelsPerSheet );
                                p.renderer->setStartItem( 0 );
                        }
                }
                else
                {
                        if ( job.options.sheets )
                        {
                                Diagnostic d;
                                d.code    = "option.ignored";
                                d.job     = job.id;
                                d.message = "--sheets only applies to a label without fields or data; it was ignored";
                                mReporter.warning( d );
                        }

                        p.renderer->setNCopies( copies );
                        p.renderer->setStartItem( first - 1 );
                        p.renderer->setIsCollated( job.options.collate.value_or( false ) );
                        p.renderer->setAreGroupsContiguous( !job.options.group.value_or( false ) );
                }

                p.renderer->setPrintOutlines( job.options.outlines.value_or( false ) );
                p.renderer->setPrintCropMarks( job.options.cropMarks.value_or( false ) );
                p.renderer->setPrintReverse( job.options.reverse.value_or( false ) );

                p.nItems = p.renderer->nItems();
                p.nPages = p.renderer->nPages();
                return true;
        }


        bool Executor::prepare( JobSpec& job, Prepared& p )
        {
                Diagnostic error;

                p.model = Inputs::loadModel( job.file, error );
                if ( !p.model )
                {
                        error.job = job.id;
                        mReporter.error( error );
                        return false;
                }

                p.model->variables().setVariables( job.variables );

                const QList<model::FieldReference> refs = model::FieldScan::scan( *p.model );

                //
                // Where do the labels' field values come from?
                //
                RecordSet rs;
                bool      have = false;

                if ( job.hasLabels )
                {
                        rs.labels = job.labels;
                        for ( const LabelSpec& label : job.labels )
                        {
                                for ( const QString& key : label.fields.keys() )
                                {
                                        if ( !rs.keys.contains( key ) )
                                        {
                                                rs.keys.append( key );
                                        }
                                }
                        }
                        have = true;
                }
                else if ( !job.data.isEmpty() )
                {
                        const QString format = Inputs::detectFormat( job.data, job.dataFormat, error );
                        if ( format.isEmpty() )
                        {
                                error.job = job.id;
                                mReporter.error( error );
                                return false;
                        }
                        if ( !Inputs::loadRecords( job.data, format, rs, error ) )
                        {
                                error.job = job.id;
                                mReporter.error( error );
                                return false;
                        }
                        have = true;
                }
                else if ( Inputs::recordsFromModelMerge( *p.model, rs ) )
                {
                        have = true;
                }

                if ( !have && ( !job.defaults.isEmpty() || !job.cliFields.isEmpty() ) )
                {
                        rs.labels.append( LabelSpec() );
                        have = true;
                }

                //
                // Ask the person for what is missing
                //
                bool prompted = false;
                if ( !promptForFields( job, rs, have, refs, prompted ) )
                {
                        return false;
                }
                if ( prompted )
                {
                        mPromptedAny = true;
                }

                if ( have && rs.labels.isEmpty() )
                {
                        fail( job, -1, "data.empty", "there are no labels to print (the data has no records)", ExitData );
                        return false;
                }

                //
                // Bind the values to the label
                //
                p.spec   = job;
                p.merged = have;

                QStringList supplied;
                auto noteKeys = [&]( const QMap<QString,QString>& map ) {
                        for ( const QString& key : map.keys() )
                        {
                                if ( !supplied.contains( key ) )
                                {
                                        supplied.append( key );
                                }
                        }
                };

                if ( have )
                {
                        QStringList                 keys = rs.keys;
                        QList<merge::Record>        records;

                        for ( const LabelSpec& label : rs.labels )
                        {
                                const QMap<QString,QString> effective = effectiveFields( job, label.fields, mSettings.forceFields );
                                noteKeys( effective );

                                merge::Record record;
                                for ( auto it = effective.begin(); it != effective.end(); ++it )
                                {
                                        record.insert( it.key(), it.value() );
                                        if ( !keys.contains( it.key() ) )
                                        {
                                                keys.append( it.key() );
                                        }
                                }

                                for ( int n = 0; n < label.copies; n++ )
                                {
                                        records.append( record );
                                        p.records.append( effective );
                                }
                        }

                        auto* memory = new merge::MemoryMerge();
                        memory->setRecords( keys, records );
                        p.model->setMerge( memory );
                }
                else
                {
                        noteKeys( job.defaults );
                        noteKeys( job.cliFields );
                }

                checkFields( p, refs, supplied );

                if ( !configureRenderer( p ) )
                {
                        return false;
                }

                if ( mSettings.maxLabels > 0 && p.nItems > mSettings.maxLabels )
                {
                        bool proceed = false;
                        if ( mSettings.canPrompt && mPrompter )
                        {
                                if ( !mPrompter->confirm( QString( "This will print %1 (the limit is %2). Continue?" )
                                                          .arg( plural( p.nItems, "label", "labels" ) ).arg( mSettings.maxLabels ),
                                                          false, proceed ) )
                                {
                                        mCancelled = true;
                                        return false;
                                }
                        }

                        if ( !proceed )
                        {
                                fail( job, -1, "data.limit",
                                      QString( "this would print %1, over the --max-labels limit of %2 (use --max-labels 0 to lift the limit)" )
                                      .arg( plural( p.nItems, "label", "labels" ) ).arg( mSettings.maxLabels ),
                                      ExitData );
                                return false;
                        }
                }

                return resolveTarget( job, p );
        }


        QString Executor::describe( const Prepared& p ) const
        {
                QString text = QString( "%1 (%2) of \"%3\" " )
                               .arg( plural( p.nItems, "label", "labels" ),
                                     plural( p.nPages, "page", "pages" ),
                                     fileLabel( p.spec.file ) );

                text += p.spec.printer.isEmpty() ? QString( "to %1" ).arg( p.spec.output )
                                                 : QString( "on \"%1\"" ).arg( p.printerName );

                if ( p.merged && p.records.size() == 1 )
                {
                    const QMap<QString,QString>& fields = p.records.first();
                    QStringList parts;
                    for ( auto it = fields.begin(); it != fields.end(); ++it )
                    {
                            if ( !it.key().startsWith( '_' ) )
                            {
                                    parts.append( QString( "%1=%2" ).arg( it.key(), it.value() ) );
                            }
                    }
                    if ( !parts.isEmpty() )
                    {
                            text += "\n  " + parts.join( "  " );
                    }
                }

                return text;
        }


        void Executor::printOne( Prepared& p )
        {
                JobResult result;
                result.id      = p.spec.id;
                result.index   = p.spec.index;
                result.file    = p.spec.file;
                result.printer = p.printerName;
                result.output  = p.spec.output;
                result.labels  = p.nItems;
                result.pages   = p.nPages;

                printing::PrintResult r;
                if ( !p.spec.printer.isEmpty() )
                {
                        r = printing::PrintService::printToPrinter( *p.renderer, p.printerName, mSettings.driver );
                }
                else if ( p.spec.output.endsWith( ".png", Qt::CaseInsensitive ) )
                {
                        r = printing::PrintService::printToImages( *p.renderer, p.spec.output, mSettings.dpi );
                }
                else
                {
                        r = printing::PrintService::printToPdf( *p.renderer, p.spec.output, Terminal::stdoutDevice() );
                }

                for ( const QString& warning : r.warnings )
                {
                        Diagnostic d;
                        d.code    = "printer.fallback";
                        d.job     = p.spec.id;
                        d.message = warning;
                        mReporter.warning( d );
                }

                if ( !r.ok )
                {
                        result.status = "failed";
                        mReporter.jobDone( result );
                        fail( p.spec, -1, "printer.failed", r.error, ExitPrinter );
                        return;
                }

                result.status = !p.spec.printer.isEmpty() ? "printed"
                              : p.spec.output.endsWith( ".png", Qt::CaseInsensitive ) ? "image" : "pdf";
                mReporter.jobDone( result );

                const QString what = QString( "%1 (%2) of \"%3\"" )
                                     .arg( plural( p.nItems, "label", "labels" ),
                                           plural( p.nPages, "page", "pages" ),
                                           fileLabel( p.spec.file ) );
                if ( p.spec.printer.isEmpty() )
                {
                        mReporter.info( QString( "Wrote %1 to %2." ).arg( what, p.spec.output == "-" ? QString( "standard output" ) : p.spec.output ) );
                }
                else
                {
                        mReporter.info( QString( "Sent %1 to \"%2\"." ).arg( what, p.printerName ) );
                }
        }


        void Executor::run( QList<JobSpec>& jobs )
        {
                mCancelled = false;
                mSucceeded = false;
                mPromptedAny = false;

                // Standard input can be read once.
                int stdinUsers = 0;
                for ( const JobSpec& job : jobs )
                {
                        stdinUsers += ( job.file == "-" ) ? 1 : 0;
                        stdinUsers += ( job.data == "-" ) ? 1 : 0;
                        if ( job.output == "-" )
                        {
                                mReporter.setStdoutBusy( true );
                        }
                }
                if ( stdinUsers > 1 )
                {
                        fail( JobSpec(), -1, "usage", "standard input can only be used for one thing (the label file or the data, not both)", ExitUsage );
                        return;
                }

                int stdoutUsers = 0;
                for ( const JobSpec& job : jobs )
                {
                        stdoutUsers += ( job.output == "-" ) ? 1 : 0;
                }
                if ( stdoutUsers > 1 )
                {
                        fail( JobSpec(), -1, "usage", "only one job can write its PDF to standard output", ExitUsage );
                        return;
                }

                //
                // Everyone needs a destination
                //
                QString chosenPrinter;
                for ( JobSpec& job : jobs )
                {
                        if ( job.printer.isEmpty() && job.output.isEmpty() && !( mSettings.canPrompt && mPrompter ) )
                        {
                                // Say so first, before any other complaint about this job.
                                fail( job, -1, "usage",
                                      "no printer given: use --printer NAME (or --printer default), or --output FILE.pdf",
                                      ExitUsage );
                                abortRun( jobs, job.index );
                                return;
                        }

                        if ( job.printer.isEmpty() && job.output.isEmpty() && mSettings.canPrompt && mPrompter )
                        {
                                if ( chosenPrinter.isEmpty() && !promptForPrinter( chosenPrinter ) )
                                {
                                        if ( mCancelled )
                                        {
                                                fail( job, -1, "cancelled", "cancelled", ExitInterrupted );
                                        }
                                        abortRun( jobs, job.index );
                                        return;
                                }
                                job.printer = chosenPrinter;
                        }
                }

                //
                // Preflight: nothing is sent unless every job checks out.
                //
                std::vector<std::unique_ptr<Prepared>> prepared;
                for ( int i = 0; i < jobs.size(); i++ )
                {
                        auto p = std::make_unique<Prepared>();
                        if ( !prepare( jobs[i], *p ) )
                        {
                                if ( mCancelled && !mReporter.hasErrors() )
                                {
                                        fail( jobs[i], -1, "cancelled", "cancelled", ExitInterrupted );
                                }
                                abortRun( jobs, i );
                                return;
                        }
                        p->spec = jobs[i];
                        prepared.push_back( std::move( p ) );
                }

                if ( mSettings.strict && mReporter.nWarnings() > 0 )
                {
                        fail( JobSpec(), -1, "data.strict",
                              QString( "%1 treated as errors (--strict); nothing was printed" )
                              .arg( plural( mReporter.nWarnings(), "warning", "warnings" ) ),
                              ExitData );
                        abortRun( jobs, -1 );
                        return;
                }

                if ( mSettings.dryRun )
                {
                        for ( const auto& p : prepared )
                        {
                                JobResult r;
                                r.id      = p->spec.id;
                                r.index   = p->spec.index;
                                r.status  = "checked";
                                r.file    = p->spec.file;
                                r.printer = p->printerName;
                                r.output  = p->spec.output;
                                r.labels  = p->nItems;
                                r.pages   = p->nPages;
                                mReporter.jobDone( r );
                                mReporter.info( "Would print " + describe( *p ) + "." );
                        }
                        mSucceeded = true;
                        return;
                }

                //
                // A person is looking: let them check before paper is spent.
                //
                if ( mSettings.canPrompt && mPrompter && !mSettings.yes && ( mSettings.interactive || mPromptedAny ) )
                {
                        for ( const auto& p : prepared )
                        {
                                mPrompter->say( "Print " + describe( *p ) );
                        }

                        bool proceed = false;
                        if ( !mPrompter->confirm( "Send to the printer?", true, proceed ) )
                        {
                                mCancelled = true;
                                fail( jobs.first(), -1, "cancelled", "cancelled", ExitInterrupted );
                                abortRun( jobs, -1 );
                                return;
                        }
                        if ( !proceed )
                        {
                                mCancelled = true;
                                fail( jobs.first(), -1, "cancelled", "cancelled; nothing was printed", ExitInterrupted );
                                abortRun( jobs, -1 );
                                return;
                        }
                }

                //
                // Print
                //
                bool allOk = true;
                for ( int i = 0; i < prepared.size(); i++ )
                {
                        const int errorsBefore = static_cast<int>( mReporter.errors().size() );
                        printOne( *prepared[i] );

                        if ( mReporter.errors().size() > errorsBefore )
                        {
                                allOk = false;
                                if ( !mSettings.continueOnError )
                                {
                                        for ( int j = i + 1; j < prepared.size(); j++ )
                                        {
                                                JobResult r;
                                                r.id     = prepared[j]->spec.id;
                                                r.index  = prepared[j]->spec.index;
                                                r.status = "skipped";
                                                r.file   = prepared[j]->spec.file;
                                                mReporter.jobDone( r );
                                        }
                                        break;
                                }
                        }
                }

                mSucceeded = allOk;
        }

}
