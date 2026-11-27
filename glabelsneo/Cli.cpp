//  Cli.cpp
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

#include "Cli.hpp"

#include "Executor.hpp"
#include "Inputs.hpp"
#include "JobLoader.hpp"
#include "JobSchema.hpp"
#include "Reporter.hpp"
#include "Terminal.hpp"

#include "model/Branding.hpp"
#include "model/FieldScan.hpp"
#include "model/Version.hpp"
#include "printing/PrintService.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>


namespace
{

        using namespace glabels::cli;
        namespace model    = glabels::model;
        namespace printing = glabels::printing;


        const QStringList COMMANDS = { "print", "inspect", "printers", "schema", "version", "help" };


        QString generalHelp()
        {
                return
                        "glabelsneo - print gLabels label files from the command line\n"
                        "\n"
                        "Usage:\n"
                        "  glabelsneo <command> [options] [arguments]\n"
                        "\n"
                        "Commands:\n"
                        "  print      Print a label file to a printer, or to a PDF\n"
                        "  inspect    Show a label file's size, fields and variables\n"
                        "  printers   List the installed printers\n"
                        "  version    Show version information\n"
                        "  help       Show help for a command\n"
                        "\n"
                        "Run 'glabelsneo <command> --help' for the options of a command.\n"
                        "\n"
                        "Examples:\n"
                        "  glabelsneo print jeans.glabels --printer default -F price=29.99 -F \"name=Blue Jeans\"\n"
                        "  glabelsneo print tags.glabels --printer \"Brother QL-800\" --data items.csv\n"
                        "  glabelsneo print --job orders.json --printer default --format json\n"
                        "  glabelsneo print jeans.glabels --output preview.pdf -F price=29.99\n"
                        "  glabelsneo inspect jeans.glabels\n"
                        "\n"
                        "Exit codes: 0 ok, 2 usage, 3 label file, 4 data, 5 printer, 6 partial failure,\n"
                        "            130 cancelled.\n";
        }


        struct Globals
        {
                bool json{ false };
                bool quiet{ false };
                bool verbose{ false };
                bool noInput{ false };
        };


        bool wantsJson( const QStringList& args )
        {
                for ( int i = 0; i < args.size(); i++ )
                {
                        if ( args[i] == "--format=json" )
                        {
                                return true;
                        }
                        if ( args[i] == "--format" && i + 1 < args.size() && args[i+1] == "json" )
                        {
                                return true;
                        }
                }
                return false;
        }


        int usageFailure( const QString& message, const QString& command, bool json )
        {
                if ( json )
                {
                        Diagnostic d;
                        d.code     = "usage";
                        d.message  = message;
                        d.exitCode = ExitUsage;

                        QJsonObject root;
                        root["ok"]       = false;
                        root["warnings"] = QJsonArray();
                        root["jobs"]     = QJsonArray();
                        root["errors"]   = QJsonArray( { Reporter::toJson( d ) } );
                        root["exitCode"] = static_cast<int>( ExitUsage );
                        Terminal::writeOut( QString::fromUtf8( QJsonDocument( root ).toJson( QJsonDocument::Compact ) ) + "\n" );
                }
                else
                {
                        Terminal::writeErr( QString( "glabelsneo: error: %1\nTry 'glabelsneo %2--help' for usage.\n" )
                                            .arg( message, command.isEmpty() ? QString() : command + " " ) );
                }
                return ExitUsage;
        }


        void addGlobalOptions( QCommandLineParser& parser )
        {
                parser.addOptions( {
                        { { "q", "quiet" },   "Print errors only." },
                        { { "v", "verbose" }, "Show diagnostics on standard error." },
                        { "no-input",         "Never prompt, even at a terminal. (Environment: GLABELSNEO_NO_INPUT=1)" },
                        { "format",           "Output format on standard output: text (default) or json.", "text|json", "text" }
                } );
        }


        bool readGlobals( const QCommandLineParser& parser, Globals& g, QString& error )
        {
                g.quiet   = parser.isSet( "quiet" );
                g.verbose = parser.isSet( "verbose" );
                g.noInput = parser.isSet( "no-input" ) || qEnvironmentVariableIntValue( "GLABELSNEO_NO_INPUT" ) != 0;

                const QString format = parser.value( "format" ).toLower();
                if ( format != "text" && format != "json" )
                {
                        error = QString( "unknown --format '%1' (use text or json)" ).arg( parser.value( "format" ) );
                        return false;
                }
                g.json = ( format == "json" );
                return true;
        }


        QString commandHelp( QCommandLineParser& parser, const QString& command, const QString& synopsis, const QString& description )
        {
                QString text = parser.helpText();
                text.replace( QRegularExpression( "^Usage: [^\\n]*" ), QString( "Usage: glabelsneo %1 %2" ).arg( command, synopsis ) );
                return description + "\n\n" + text;
        }


        bool toInteger( const QCommandLineParser& parser, const QString& name, int minimum, std::optional<int>& target, QString& error )
        {
                if ( !parser.isSet( name ) )
                {
                        return true;
                }

                bool ok = false;
                const int n = parser.value( name ).toInt( &ok );
                if ( !ok || n < minimum )
                {
                        error = QString( "--%1 must be a whole number >= %2 (got '%3')" ).arg( name ).arg( minimum ).arg( parser.value( name ) );
                        return false;
                }
                target = n;
                return true;
        }


        //
        // print
        //
        int runPrint( const QStringList& args, Prompter* injected )
        {
                QCommandLineParser parser;
                parser.setApplicationDescription( "Print a label file." );
                const QCommandLineOption helpOption = parser.addHelpOption();

                parser.addOptions( {
                        { { "p", "printer" },   "Print on this printer. Use \"default\" for the system default printer. "
                                                "Either --printer or --output is required.", "name" },
                        { { "o", "output" },    "Write a PDF instead of printing (\"-\" = standard output), or PNG image(s) if the name ends in .png.", "file" },
                        { { "F", "field" },     "Set a merge field for the label (repeatable). Spaces need quotes.", "NAME=VALUE" },
                        { "var",                "Set the initial value of a user variable (repeatable).", "NAME=VALUE" },
                        { "data",               "Records for a batch: a file, or \"-\" for standard input.", "source" },
                        { "data-format",        "Format of --data: json, jsonl, csv, tsv, colon, semicolon "
                                                "(add -nokeys for files without a header row). Default: from the file name.", "format" },
                        { "job",                "Run a JSON job document (\"-\" = standard input). Command-line options override it.", "file" },
                        { { "n", "copies" },    "Copies of each label (default 1).", "n" },
                        { "sheets",             "Full sheets of a label without fields or data.", "n" },
                        { "first",              "Start at label position <n> on the first sheet (default 1).", "n" },
                        { "collate",            "Collate copies of merged labels." },
                        { "group",              "Start each merge group on a new page." },
                        { "outlines",           "Print label outlines." },
                        { "crop-marks",         "Print crop marks." },
                        { "reverse",            "Print in reverse (mirror image)." },
                        { { "i", "interactive" }, "Prompt for every field, then confirm." },
                        { { "y", "yes" },       "Do not ask for confirmation." },
                        { "dry-run",            "Check everything and report, but do not print." },
                        { "force-fields",       "Make --field values override the data file's values." },
                        { "strict",             "Treat warnings (unused or missing fields) as errors." },
                        { "max-labels",         "Refuse to print more than <n> labels (default 1000; 0 = no limit).", "n", "1000" },
                        { "continue-on-error",  "In a run of several jobs, keep going after one fails." },
                        { "dpi",                "Resolution of PNG output (--output file.png), default 150.", "n", "150" },
                        { "driver",             "How to talk to the printer: auto (Zebra printers use ZPL), system or zpl.", "auto|system|zpl", "auto" }
                } );
                addGlobalOptions( parser );
                parser.addPositionalArgument( "file", "The gLabels label file to print (\"-\" = standard input).", "[FILE]" );

                QStringList full = args;
                full.prepend( "glabelsneo" );
                const bool json = wantsJson( args );

                if ( !parser.parse( full ) )
                {
                        return usageFailure( parser.errorText(), "print", json );
                }
                if ( parser.isSet( helpOption ) )
                {
                        Terminal::writeOut( commandHelp( parser, "print", "[options] [FILE]",
                                                         "Print a label file to a printer or a PDF.\n"
                                                         "One label: -F name=value for each field the label uses. Many labels: --data or --job." ) );
                        return ExitOk;
                }

                Globals globals;
                QString problem;
                if ( !readGlobals( parser, globals, problem ) )
                {
                        return usageFailure( problem, "print", json );
                }

                Terminal::installLogHandler( globals.verbose );
                Reporter reporter( globals.json, globals.quiet, globals.verbose );

                auto usage = [&]( const QString& message ) {
                        Diagnostic d;
                        d.code     = "usage";
                        d.message  = message;
                        d.exitCode = ExitUsage;
                        reporter.error( d );
                        return reporter.finish( false, false );
                };

                //
                // Options -> RunSettings and the per-job overrides
                //
                RunSettings settings;
                settings.interactive     = parser.isSet( "interactive" );
                settings.yes             = parser.isSet( "yes" );
                settings.dryRun          = parser.isSet( "dry-run" );
                settings.forceFields     = parser.isSet( "force-fields" );
                settings.strict          = parser.isSet( "strict" );
                settings.continueOnError = parser.isSet( "continue-on-error" );

                std::optional<int> maxLabels;
                if ( !toInteger( parser, "max-labels", 0, maxLabels, problem ) )
                {
                        return usage( problem );
                }
                settings.maxLabels = maxLabels.value_or( 1000 );

                std::optional<int> dpi;
                if ( !toInteger( parser, "dpi", 1, dpi, problem ) )
                {
                        return usage( problem );
                }
                settings.dpi = dpi.value_or( 150 );

                const QString driver = parser.value( "driver" ).toLower();
                if ( driver == "auto" )        settings.driver = printing::Driver::Auto;
                else if ( driver == "system" ) settings.driver = printing::Driver::System;
                else if ( driver == "zpl" )    settings.driver = printing::Driver::Zpl;
                else return usage( QString( "unknown --driver '%1' (use auto, system or zpl)" ).arg( parser.value( "driver" ) ) );

                CliJobInput in;
                in.printer    = parser.value( "printer" );
                in.output     = parser.value( "output" );
                in.fields     = parser.values( "field" );
                in.variables  = parser.values( "var" );
                in.data       = parser.value( "data" );
                in.dataFormat = parser.value( "data-format" );

                if ( !toInteger( parser, "copies", 1, in.options.copies, problem ) ||
                     !toInteger( parser, "sheets", 1, in.options.sheets, problem ) ||
                     !toInteger( parser, "first",  1, in.options.first,  problem ) )
                {
                        return usage( problem );
                }
                if ( parser.isSet( "collate" ) )    in.options.collate   = true;
                if ( parser.isSet( "group" ) )      in.options.group     = true;
                if ( parser.isSet( "outlines" ) )   in.options.outlines  = true;
                if ( parser.isSet( "crop-marks" ) ) in.options.cropMarks = true;
                if ( parser.isSet( "reverse" ) )    in.options.reverse   = true;

                if ( !in.printer.isEmpty() && !in.output.isEmpty() )
                {
                        return usage( "give either --printer or --output, not both" );
                }

                const QStringList positional = parser.positionalArguments();
                if ( positional.size() > 1 )
                {
                        return usage( QString( "only one label file can be given (got %1); to print several use --job" ).arg( positional.size() ) );
                }
                if ( !positional.isEmpty() )
                {
                        in.file = positional.first();
                }

                //
                // Jobs
                //
                QList<JobSpec> jobs;
                Diagnostic     error;

                if ( parser.isSet( "job" ) )
                {
                        const QString jobFile = parser.value( "job" );
                        QByteArray    bytes;
                        QString       baseDir = QDir::currentPath();

                        if ( jobFile == "-" )
                        {
                                bytes = Terminal::readAllStdin();
                        }
                        else
                        {
                                QFile file( jobFile );
                                if ( !file.open( QIODevice::ReadOnly ) )
                                {
                                        Diagnostic d;
                                        d.code     = "input.missing";
                                        d.message  = QString( "job file '%1' cannot be read: %2" ).arg( jobFile, file.errorString() );
                                        d.exitCode = ExitInput;
                                        reporter.error( d );
                                        return reporter.finish( false, false );
                                }
                                bytes   = file.readAll();
                                baseDir = QFileInfo( jobFile ).absolutePath();
                        }

                        if ( !JobLoader::parseJobDocument( bytes, baseDir, jobs, error ) )
                        {
                                reporter.error( error );
                                return reporter.finish( false, false );
                        }

                        if ( !in.file.isEmpty() && jobs.size() > 1 )
                        {
                                return usage( "a label file on the command line cannot override a job document with several jobs" );
                        }

                        for ( JobSpec& job : jobs )
                        {
                                if ( !JobLoader::applyOverrides( in, job, error ) )
                                {
                                        reporter.error( error );
                                        return reporter.finish( false, false );
                                }
                        }
                }
                else
                {
                        JobSpec job;
                        if ( !JobLoader::fromCommandLine( in, job, error ) )
                        {
                                reporter.error( error );
                                return reporter.finish( false, false );
                        }
                        jobs.append( job );
                }

                for ( int i = 0; i < jobs.size(); i++ )
                {
                        jobs[i].index = i;
                }

                //
                // Prompting happens only when a person is there to answer
                //
                bool stdinUsed = ( parser.value( "job" ) == "-" );
                bool pdfOnStdout = false;
                for ( const JobSpec& job : jobs )
                {
                        stdinUsed   = stdinUsed || job.file == "-" || job.data == "-";
                        pdfOnStdout = pdfOnStdout || job.output == "-";
                }

                if ( globals.json && pdfOnStdout )
                {
                        return usage( "--format json cannot be combined with a PDF on standard output (--output -)" );
                }

                const bool atTerminal = Terminal::stdinIsTty() && !stdinUsed;
                if ( settings.interactive && ( !atTerminal || globals.noInput ) )
                {
                        return usage( globals.noInput ? "--interactive conflicts with --no-input"
                                                      : "--interactive needs a terminal on standard input (it cannot be combined with '-' inputs or a pipe)" );
                }
                settings.canPrompt = atTerminal && !globals.noInput && ( !globals.json || settings.interactive );

                Prompter  defaultPrompter;
                Prompter* prompter = injected ? injected : &defaultPrompter;

                Executor executor( settings, reporter, prompter );
                executor.run( jobs );

                // Shop-counter workflow: another label with different values.
                if ( settings.interactive && settings.canPrompt && jobs.size() == 1 && jobs.first().hasLabels )
                {
                        while ( executor.succeeded() && !settings.dryRun )
                        {
                                bool again = false;
                                if ( !prompter->confirm( "Print another label with different values?", false, again ) || !again )
                                {
                                        break;
                                }
                                executor.run( jobs );
                        }
                }

                return reporter.finish( settings.dryRun, settings.continueOnError );
        }


        //
        // inspect
        //
        int runInspect( const QStringList& args )
        {
                QCommandLineParser parser;
                const QCommandLineOption helpOption = parser.addHelpOption();
                addGlobalOptions( parser );
                parser.addPositionalArgument( "file", "The gLabels label file to inspect (\"-\" = standard input).", "FILE" );

                QStringList full = args;
                full.prepend( "glabelsneo" );
                const bool json = wantsJson( args );

                if ( !parser.parse( full ) )
                {
                        return usageFailure( parser.errorText(), "inspect", json );
                }
                if ( parser.isSet( helpOption ) )
                {
                        Terminal::writeOut( commandHelp( parser, "inspect", "[options] FILE",
                                                         "Show a label file's template, label size, merge source, user variables\n"
                                                         "and the fields it expects to be filled in." ) );
                        return ExitOk;
                }

                Globals globals;
                QString problem;
                if ( !readGlobals( parser, globals, problem ) )
                {
                        return usageFailure( problem, "inspect", json );
                }
                if ( parser.positionalArguments().size() != 1 )
                {
                        return usageFailure( "inspect needs exactly one label file", "inspect", globals.json );
                }

                Terminal::installLogHandler( globals.verbose );
                Reporter reporter( globals.json, globals.quiet, globals.verbose );

                const QString file = parser.positionalArguments().first();
                Diagnostic error;
                auto labelModel = Inputs::loadModel( file, error );
                if ( !labelModel )
                {
                        reporter.error( error );
                        return reporter.finish( false, false );
                }

                const auto refs  = model::FieldScan::scan( *labelModel );
                const auto* tmpl = &labelModel->tmplate();
                const auto* frame = labelModel->frame();

                QJsonObject root;
                root["ok"]   = true;
                root["file"] = file;

                QJsonObject t;
                t["brand"]       = tmpl->brand();
                t["part"]        = tmpl->part();
                t["description"] = tmpl->description();
                t["pageWidthPt"]  = tmpl->pageWidth().pt();
                t["pageHeightPt"] = tmpl->pageHeight().pt();
                root["template"] = t;

                QJsonObject l;
                l["widthPt"]        = frame->w().pt();
                l["heightPt"]       = frame->h().pt();
                l["labelsPerSheet"] = frame->nLabels();
                l["layout"]         = frame->layoutDescription();
                root["label"] = l;

                QJsonObject m;
                m["id"]     = labelModel->merge()->id();
                m["source"] = labelModel->merge()->source();
                m["keys"]   = QJsonArray::fromStringList( labelModel->merge()->keys() );
                root["merge"] = m;

                QJsonArray variables;
                for ( const auto& variable : labelModel->constVariables() )
                {
                        QJsonObject v;
                        v["name"]      = variable.name();
                        v["type"]      = model::Variable::typeToIdString( variable.type() );
                        v["increment"] = model::Variable::incrementToIdString( variable.increment() );
                        v["initial"]   = variable.initialValue();
                        variables.append( v );
                }
                root["variables"] = variables;

                QJsonArray fields;
                for ( const auto& ref : refs )
                {
                        QJsonObject f;
                        f["name"]       = ref.name;
                        f["required"]   = !ref.hasDefault && !ref.isVariable;
                        f["hasDefault"] = ref.hasDefault;
                        f["isVariable"] = ref.isVariable;
                        fields.append( f );
                }
                root["fields"] = fields;

                if ( globals.json )
                {
                        Terminal::writeOut( QString::fromUtf8( QJsonDocument( root ).toJson( QJsonDocument::Compact ) ) + "\n" );
                        return ExitOk;
                }

                QString text;
                text += QString( "File:       %1\n" ).arg( file );
                text += QString( "Template:   %1 %2 - %3\n" ).arg( tmpl->brand(), tmpl->part(), tmpl->description() );
                text += QString( "Label size: %1 x %2 pt (%3 x %4 in), %5 per sheet\n" )
                        .arg( frame->w().pt(), 0, 'f', 1 ).arg( frame->h().pt(), 0, 'f', 1 )
                        .arg( frame->w().in(), 0, 'f', 2 ).arg( frame->h().in(), 0, 'f', 2 )
                        .arg( frame->nLabels() );
                text += QString( "Merge:      %1\n" ).arg( labelModel->merge()->source().isEmpty() ? labelModel->merge()->id()
                                                                                              : labelModel->merge()->id() + " " + labelModel->merge()->source() );

                text += "Fields:\n";
                if ( refs.isEmpty() )
                {
                        text += "  (none: the label has no ${...} fields)\n";
                }
                for ( const auto& ref : refs )
                {
                        QString note = ref.isVariable ? "user variable" : ( ref.hasDefault ? "has a default" : "required" );
                        text += QString( "  %1  (%2)\n" ).arg( ref.name, note );
                }

                if ( !labelModel->constVariables().isEmpty() )
                {
                        text += "Variables:\n";
                        for ( const auto& variable : labelModel->constVariables() )
                        {
                                text += QString( "  %1  %2, starts at \"%3\", increments %4\n" )
                                        .arg( variable.name(),
                                              model::Variable::typeToIdString( variable.type() ),
                                              variable.initialValue(),
                                              model::Variable::incrementToIdString( variable.increment() ) );
                        }
                }

                Terminal::writeOut( text );
                return ExitOk;
        }


        //
        // printers
        //
        int runPrinters( const QStringList& args )
        {
                QCommandLineParser parser;
                const QCommandLineOption helpOption = parser.addHelpOption();
                parser.addOption( { "check", "Exit 0 if this printer name resolves, 5 if not.", "name" } );
                addGlobalOptions( parser );

                QStringList full = args;
                full.prepend( "glabelsneo" );
                const bool json = wantsJson( args );

                if ( !parser.parse( full ) )
                {
                        return usageFailure( parser.errorText(), "printers", json );
                }
                if ( parser.isSet( helpOption ) )
                {
                        Terminal::writeOut( commandHelp( parser, "printers", "[options]", "List the installed printers (the default one is marked)." ) );
                        return ExitOk;
                }

                Globals globals;
                QString problem;
                if ( !readGlobals( parser, globals, problem ) )
                {
                        return usageFailure( problem, "printers", json );
                }

                Terminal::installLogHandler( globals.verbose );

                if ( parser.isSet( "check" ) )
                {
                        Reporter reporter( globals.json, globals.quiet, globals.verbose );
                        const auto r = printing::PrintService::resolvePrinter( parser.value( "check" ) );
                        if ( r.status == printing::ResolvedPrinter::Status::Found )
                        {
                                JobResult result;
                                result.status  = "checked";
                                result.printer = r.name;
                                reporter.jobDone( result );
                                reporter.info( QString( "Printer \"%1\" is available." ).arg( r.name ) );
                        }
                        else
                        {
                                Diagnostic d;
                                d.exitCode = ExitPrinter;
                                if ( r.status == printing::ResolvedPrinter::Status::Ambiguous )
                                {
                                        d.code    = "printer.ambiguous";
                                        d.message = QString( "'%1' matches more than one printer: %2" ).arg( parser.value( "check" ), r.candidates.join( ", " ) );
                                }
                                else
                                {
                                        d.code    = "printer.notfound";
                                        d.message = QString( "printer '%1' was not found" ).arg( parser.value( "check" ) );
                                }
                                reporter.error( d );
                        }
                        return reporter.finish( true, false );
                }

                const QList<printing::PrinterEntry> printers = printing::PrintService::printers();

                if ( globals.json )
                {
                        QJsonArray list;
                        for ( const auto& p : printers )
                        {
                                QJsonObject o;
                                o["name"]    = p.name;
                                o["default"] = p.isDefault;
                                o["kind"]    = p.isZebra ? "zebra-zpl" : "system";
                                o["state"]   = p.state;
                                list.append( o );
                        }
                        QJsonObject root;
                        root["ok"]       = true;
                        root["printers"] = list;
                        Terminal::writeOut( QString::fromUtf8( QJsonDocument( root ).toJson( QJsonDocument::Compact ) ) + "\n" );
                        return ExitOk;
                }

                if ( printers.isEmpty() )
                {
                        Terminal::writeOut( "No printers are installed.\n" );
                        return ExitOk;
                }

                QString text;
                for ( const auto& p : printers )
                {
                        text += QString( "%1 %2  [%3%4]\n" ).arg( p.isDefault ? "*" : " ", p.name, p.state, p.isZebra ? ", zpl" : "" );
                }
                text += "\n* = default printer\n";
                Terminal::writeOut( text );
                return ExitOk;
        }


        //
        // schema
        //
        int runSchema( const QStringList& args )
        {
                QCommandLineParser parser;
                const QCommandLineOption helpOption = parser.addHelpOption();

                QStringList full = args;
                full.prepend( "glabelsneo" );

                if ( !parser.parse( full ) )
                {
                        return usageFailure( parser.errorText(), "schema", wantsJson( args ) );
                }
                if ( parser.isSet( helpOption ) )
                {
                        Terminal::writeOut( commandHelp( parser, "schema", "",
                                                         "Print the JSON Schema (draft 2020-12) of the job documents read by\n"
                                                         "'glabelsneo print --job', so a program can validate one before sending it." ) );
                        return ExitOk;
                }
                if ( !parser.positionalArguments().isEmpty() )
                {
                        return usageFailure( "schema takes no arguments", "schema", false );
                }

                Terminal::writeOut( jobSchema() );
                return ExitOk;
        }


        //
        // version
        //
        int runVersion( const QStringList& args )
        {
                QCommandLineParser parser;
                const QCommandLineOption helpOption = parser.addHelpOption();
                addGlobalOptions( parser );

                QStringList full = args;
                full.prepend( "glabelsneo" );
                const bool json = wantsJson( args );

                if ( !parser.parse( full ) )
                {
                        return usageFailure( parser.errorText(), "version", json );
                }
                if ( parser.isSet( helpOption ) )
                {
                        Terminal::writeOut( commandHelp( parser, "version", "[options]", "Show version information (--verbose for build and system details)." ) );
                        return ExitOk;
                }

                Globals globals;
                QString problem;
                if ( !readGlobals( parser, globals, problem ) )
                {
                        return usageFailure( problem, "version", json );
                }

                if ( globals.json )
                {
                        QJsonObject root;
                        root["ok"]      = true;
                        root["name"]    = "glabelsneo";
                        root["version"] = model::Version::STRING;
                        root["build"]   = model::Version::LONG_STRING;
                        Terminal::writeOut( QString::fromUtf8( QJsonDocument( root ).toJson( QJsonDocument::Compact ) ) + "\n" );
                }
                else if ( globals.verbose )
                {
                        Terminal::writeOut( model::Version::details() + "\n" );
                }
                else
                {
                        Terminal::writeOut( QString( "glabelsneo %1\n" ).arg( model::Version::STRING ) );
                }
                return ExitOk;
        }

}


namespace glabels::cli
{

        int runCli( const QStringList& arguments, Prompter* prompter )
        {
                // Find the command: the first argument that is not an option.  Anything before it is a
                // global option and is handed on to the command's own parser.
                int commandIndex = -1;
                for ( int i = 0; i < arguments.size(); i++ )
                {
                        if ( !arguments[i].startsWith( '-' ) )
                        {
                                commandIndex = i;
                                break;
                        }
                }

                // No command: only the version and help flags make sense.
                if ( commandIndex < 0 )
                {
                        for ( const QString& arg : arguments )
                        {
                                if ( arg == "-V" || arg == "--Version" )
                                {
                                        // Detailed version; the self-updater runs this to smoke-test a download.
                                        Terminal::writeOut( model::Version::details() + "\n" );
                                        return ExitOk;
                                }
                                if ( arg == "--version" )
                                {
                                        Terminal::writeOut( QString( "glabelsneo %1\n" ).arg( model::Version::STRING ) );
                                        return ExitOk;
                                }
                                if ( arg == "-h" || arg == "--help" )
                                {
                                        Terminal::writeOut( generalHelp() );
                                        return ExitOk;
                                }
                        }

                        if ( arguments.isEmpty() )
                        {
                                Terminal::writeErr( generalHelp() );
                                return ExitUsage;
                        }

                        return usageFailure( QString( "unknown option '%1'" ).arg( arguments.first() ), QString(), wantsJson( arguments ) );
                }

                const QString command = arguments[commandIndex];
                QStringList   rest    = arguments.mid( 0, commandIndex ) + arguments.mid( commandIndex + 1 );

                if ( command == "print" )    return runPrint( rest, prompter );
                if ( command == "inspect" )  return runInspect( rest );
                if ( command == "printers" ) return runPrinters( rest );
                if ( command == "schema" )   return runSchema( rest );
                if ( command == "version" )  return runVersion( rest );

                if ( command == "help" )
                {
                        if ( rest.isEmpty() )
                        {
                                Terminal::writeOut( generalHelp() );
                                return ExitOk;
                        }
                        const QString topic = rest.first();
                        if ( !COMMANDS.contains( topic ) )
                        {
                                return usageFailure( QString( "no help for unknown command '%1'" ).arg( topic ), QString(), false );
                        }
                        return runCli( { topic, "--help" }, prompter );
                }

                return usageFailure( QString( "unknown command '%1' (commands: %2)" ).arg( command, COMMANDS.join( ", " ) ), QString(), wantsJson( arguments ) );
        }

}
