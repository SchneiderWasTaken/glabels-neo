//  TestCli.cpp
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

#include "TestCli.hpp"

#include "Cli.hpp"
#include "Exit.hpp"
#include "Prompter.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Settings.hpp"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

using namespace glabels::cli;


namespace
{

        const QString LABEL = QString( TEST_DATA_DIR ) + "/fields.glabels";
        const QString CSV   = QString( TEST_DATA_DIR ) + "/items.csv";
        const QString JSONL = QString( TEST_DATA_DIR ) + "/items.jsonl";
        const QString PLAIN = QString( TEST_DATA_DIR ) + "/../../../test-data/simple-text-liberation-sans.glabels";


        ///
        /// Answers prompts from a script and records what was asked.
        ///
        class ScriptedPrompter : public Prompter
        {
        public:
                explicit ScriptedPrompter( const QStringList& answers ) : mAnswers( answers ) {}

                QString asked() const { return mWritten; }
                int     remaining() const { return static_cast<int>( mAnswers.size() ); }

        protected:
                bool readLine( QString& line ) override
                {
                        if ( mAnswers.isEmpty() )
                        {
                                return false;      // end of input
                        }
                        line = mAnswers.takeFirst();
                        return true;
                }

                void write( const QString& text ) override
                {
                        mWritten += text;
                }

        private:
                QStringList mAnswers;
                QString     mWritten;
        };


        QStringList withNoInput( QStringList args )
        {
                args << "--no-input";
                return args;
        }


        bool startsWithPdf( const QString& path )
        {
                QFile f( path );
                return f.open( QIODevice::ReadOnly ) && f.read( 4 ) == "%PDF";
        }


        struct Run
        {
                int        exitCode{ -1 };
                QByteArray out;
                QByteArray err;
        };


        Run runProcess( const QString& exe, const QStringList& args, const QByteArray& input = QByteArray() )
        {
                QProcess p;
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.insert( "GLABELSNEO_NO_INPUT", "1" );
#if defined(Q_OS_LINUX)
                env.insert( "QT_QPA_PLATFORM", "offscreen" );
#endif
                p.setProcessEnvironment( env );
                p.start( exe, args );
                if ( !p.waitForStarted( 20000 ) )
                {
                        return Run();
                }
                if ( !input.isEmpty() )
                {
                        p.write( input );
                }
                p.closeWriteChannel();
                p.waitForFinished( 120000 );

                Run r;
                r.exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -2;
                r.out      = p.readAllStandardOutput();
                r.err      = p.readAllStandardError();
                return r;
        }

}


void TestCli::initTestCase()
{
        QCoreApplication::setOrganizationName( "glabels.org" );
        QCoreApplication::setApplicationName( "glabelsneo-test" );
        glabels::model::Settings::init();
        glabels::model::Db::init();
        glabels::merge::Factory::init();
        glabels::barcode::Backends::init();
}


void TestCli::printFieldsToPdf()
{
        QTemporaryDir dir;
        const QString out = dir.filePath( "a.pdf" );
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-F", "price=29.99", "-F", "name=Blue Jeans", "-o", out } ) ), 0 );
        QVERIFY( startsWithPdf( out ) );
        QVERIFY( QFileInfo( out ).size() > 1000 );
}


void TestCli::needsATarget()
{
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-F", "price=1", "-F", "name=x" } ) ), static_cast<int>( ExitUsage ) );
}


void TestCli::rejectsBothTargets()
{
        QTemporaryDir dir;
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-p", "default", "-o", dir.filePath( "x.pdf" ) } ) ), static_cast<int>( ExitUsage ) );
}


void TestCli::unknownPrinterIsAPrinterError()
{
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-F", "price=1", "-F", "name=x", "-p", "No Such Printer 12345" } ) ),
                  static_cast<int>( ExitPrinter ) );
}


void TestCli::missingLabelFileIsAnInputError()
{
        QTemporaryDir dir;
        QCOMPARE( runCli( withNoInput( { "print", dir.filePath( "nope.glabels" ), "-o", dir.filePath( "x.pdf" ) } ) ),
                  static_cast<int>( ExitInput ) );
        QVERIFY( !QFileInfo::exists( dir.filePath( "x.pdf" ) ) );
}


void TestCli::notALabelFileIsAnInputError()
{
        QTemporaryDir dir;
        QFile f( dir.filePath( "junk.glabels" ) );
        QVERIFY( f.open( QIODevice::WriteOnly ) );
        f.write( "this is not xml" );
        f.close();
        QCOMPARE( runCli( withNoInput( { "print", f.fileName(), "-o", dir.filePath( "x.pdf" ) } ) ), static_cast<int>( ExitInput ) );
}


void TestCli::strictTurnsWarningsIntoErrors()
{
        QTemporaryDir dir;
        const QString out = dir.filePath( "s.pdf" );

        // "prise" is a typo for "price": one warning for the unused field, one for the blank price.
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-F", "prise=1", "-F", "name=x", "-o", out, "--strict" } ) ),
                  static_cast<int>( ExitData ) );
        QVERIFY( !QFileInfo::exists( out ) );
}


void TestCli::warningsAloneDoNotFail()
{
        QTemporaryDir dir;
        const QString out = dir.filePath( "w.pdf" );
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-F", "prise=1", "-F", "name=x", "-o", out } ) ), 0 );
        QVERIFY( startsWithPdf( out ) );   // blank price, printed anyway
}


void TestCli::dryRunWritesNothing()
{
        QTemporaryDir dir;
        const QString out = dir.filePath( "d.pdf" );
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-F", "price=1", "-F", "name=x", "-o", out, "--dry-run" } ) ), 0 );
        QVERIFY( !QFileInfo::exists( out ) );

        // ...but it still validates.
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-o", out, "--dry-run", "--data", dir.filePath( "missing.csv" ) } ) ),
                  static_cast<int>( ExitData ) );
}


void TestCli::maxLabelsStopsFloods()
{
        QTemporaryDir dir;
        const QString out = dir.filePath( "m.pdf" );
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "--data", CSV, "-o", out, "--max-labels", "1" } ) ), static_cast<int>( ExitData ) );
        QVERIFY( !QFileInfo::exists( out ) );
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "--data", CSV, "-o", out, "--max-labels", "0" } ) ), 0 );
        QVERIFY( QFileInfo::exists( out ) );
}


void TestCli::perRecordCopiesBecomePages()
{
        // items.jsonl: one record, then one with "_copies": 3  ->  4 labels; the label is a roll, so 4 pages.
        QTemporaryDir dir;
        const Run r = runProcess( GLABELSNEO_EXE, { "print", LABEL, "--data", JSONL, "-o", dir.filePath( "p.pdf" ), "--format", "json" } );
        QCOMPARE( r.exitCode, 0 );
        const QJsonObject o = QJsonDocument::fromJson( r.out ).object();
        QVERIFY( o["ok"].toBool() );
        QCOMPARE( o["jobs"].toArray().at( 0 ).toObject()["labels"].toInt(), 4 );
        QCOMPARE( o["jobs"].toArray().at( 0 ).toObject()["pages"].toInt(), 4 );

        // --copies multiplies on top
        const Run r2 = runProcess( GLABELSNEO_EXE, { "print", LABEL, "--data", JSONL, "-o", dir.filePath( "p2.pdf" ), "-n", "2", "--format", "json" } );
        QCOMPARE( QJsonDocument::fromJson( r2.out ).object()["jobs"].toArray().at( 0 ).toObject()["labels"].toInt(), 8 );
}


void TestCli::pngOutputNamesPages()
{
        QTemporaryDir dir;
        QCOMPARE( runCli( withNoInput( { "print", LABEL, "-F", "price=1", "-F", "name=x", "-o", dir.filePath( "one.png" ), "--dpi", "50" } ) ), 0 );
        QVERIFY( QFileInfo::exists( dir.filePath( "one.png" ) ) );

        QCOMPARE( runCli( withNoInput( { "print", LABEL, "--data", JSONL, "-o", dir.filePath( "many.png" ), "--dpi", "50" } ) ), 0 );
        QVERIFY( !QFileInfo::exists( dir.filePath( "many.png" ) ) );
        for ( int i = 1; i <= 4; i++ )
        {
                QVERIFY( QFileInfo::exists( dir.filePath( QString( "many-%1.png" ).arg( i ) ) ) );
        }
        QVERIFY( !QFileInfo::exists( dir.filePath( "many-5.png" ) ) );

        QImage image( dir.filePath( "one.png" ) );
        QVERIFY( !image.isNull() );
        QVERIFY( image.width() > 0 && image.height() > image.width() );   // a portrait roll label
}


void TestCli::firstBeyondTheSheetIsAUsageError()
{
        QTemporaryDir dir;
        // The roll label holds one label per "sheet".
        QCOMPARE( runCli( withNoInput( { "print", PLAIN, "-o", dir.filePath( "f.pdf" ), "-n", "1", "--first", "2" } ) ),
                  static_cast<int>( ExitUsage ) );
}


void TestCli::stdinCanBeUsedOnce()
{
        QTemporaryDir dir;
        QCOMPARE( runCli( withNoInput( { "print", "-", "--data", "-", "--data-format", "json", "-o", dir.filePath( "x.pdf" ) } ) ),
                  static_cast<int>( ExitUsage ) );
}


void TestCli::badNumbersAreUsageErrors()
{
        QTemporaryDir dir;
        const QString out = dir.filePath( "n.pdf" );
        for ( const QString& bad : { "0", "-3", "abc", "1.5" } )
        {
                QCOMPARE( runCli( withNoInput( { "print", PLAIN, "-o", out, "-n", bad } ) ), static_cast<int>( ExitUsage ) );
        }
        QCOMPARE( runCli( withNoInput( { "print", PLAIN, "-o", out, "--max-labels", "-1" } ) ), static_cast<int>( ExitUsage ) );
        QCOMPARE( runCli( withNoInput( { "print", PLAIN, "-o", out, "--driver", "magic" } ) ), static_cast<int>( ExitUsage ) );
        QCOMPARE( runCli( withNoInput( { "print", PLAIN, "-o", out, "--format", "xml" } ) ), static_cast<int>( ExitUsage ) );
        QVERIFY( !QFileInfo::exists( out ) );
}


void TestCli::unknownOptionsAreUsageErrors()
{
        QCOMPARE( runCli( withNoInput( { "print", PLAIN, "--bogus" } ) ), static_cast<int>( ExitUsage ) );
        QCOMPARE( runCli( { "frobnicate" } ), static_cast<int>( ExitUsage ) );
        QCOMPARE( runCli( { "-F" } ), static_cast<int>( ExitUsage ) );
        QCOMPARE( runCli( withNoInput( { "print", PLAIN, "second.glabels", "-o", "x.pdf" } ) ), static_cast<int>( ExitUsage ) );
        QCOMPARE( runCli( withNoInput( { "print", PLAIN, "-o", "x.pdf", "-F", "no-equals-sign" } ) ), static_cast<int>( ExitUsage ) );
}


void TestCli::jobDocumentRunsEveryJob()
{
        QTemporaryDir dir;
        QFile job( dir.filePath( "job.json" ) );
        QVERIFY( job.open( QIODevice::WriteOnly ) );
        job.write( QString( R"({"version":1,"jobs":[
            {"id":"a","file":"%1","output":"a.pdf","labels":[{"fields":{"name":"A","price":"1"},"copies":2},{"fields":{"name":"B","price":"2"}}]},
            {"id":"b","file":"%1","output":"b.pdf","data":"%2"}
        ]})" ).arg( LABEL, CSV ).toUtf8() );
        job.close();

        QCOMPARE( runCli( withNoInput( { "print", "--job", job.fileName() } ) ), 0 );
        QVERIFY( startsWithPdf( dir.filePath( "a.pdf" ) ) );   // resolved next to the job document
        QVERIFY( startsWithPdf( dir.filePath( "b.pdf" ) ) );
}


void TestCli::jobDocumentIsAllOrNothing()
{
        QTemporaryDir dir;
        QFile job( dir.filePath( "job.json" ) );
        QVERIFY( job.open( QIODevice::WriteOnly ) );
        job.write( QString( R"({"jobs":[
            {"file":"%1","output":"first.pdf","labels":[{"fields":{"name":"A","price":"1"}}]},
            {"file":"does-not-exist.glabels","output":"second.pdf"}
        ]})" ).arg( LABEL ).toUtf8() );
        job.close();

        // The second job is broken, so the first must not be sent either.
        QCOMPARE( runCli( withNoInput( { "print", "--job", job.fileName() } ) ), static_cast<int>( ExitInput ) );
        QVERIFY( !QFileInfo::exists( dir.filePath( "first.pdf" ) ) );
        QVERIFY( !QFileInfo::exists( dir.filePath( "second.pdf" ) ) );
}


void TestCli::commandLineOverridesTheJobDocument()
{
        QTemporaryDir dir;
        QFile job( dir.filePath( "job.json" ) );
        QVERIFY( job.open( QIODevice::WriteOnly ) );
        job.write( QString( R"({"file":"%1","output":"from-doc.pdf","labels":[{"fields":{"name":"A","price":"1"}}]})" ).arg( LABEL ).toUtf8() );
        job.close();

        const QString redirected = dir.filePath( "from-flag.pdf" );
        QCOMPARE( runCli( withNoInput( { "print", "--job", job.fileName(), "-o", redirected } ) ), 0 );
        QVERIFY( startsWithPdf( redirected ) );
        QVERIFY( !QFileInfo::exists( dir.filePath( "from-doc.pdf" ) ) );
}


void TestCli::promptsForMissingFields()
{
        // A person at a terminal gets asked for what is missing, then asked to confirm.
        qputenv( "GLABELSNEO_ASSUME_TTY", "1" );
        QTemporaryDir dir;
        const QString out = dir.filePath( "pr.png" );

        ScriptedPrompter prompter( { "Corduroy", "49.5", "y" } );
        QCOMPARE( runCli( { "print", LABEL, "-o", out, "--dpi", "40" }, &prompter ), 0 );
        qunsetenv( "GLABELSNEO_ASSUME_TTY" );

        QVERIFY( QFileInfo::exists( out ) );
        QVERIFY2( prompter.asked().contains( "name" ), qPrintable( prompter.asked() ) );
        QVERIFY( prompter.asked().contains( "price" ) );
        QVERIFY( !prompter.asked().contains( "note" ) );              // it has a default, so it is not "missing"
        QVERIFY( prompter.asked().contains( "name=Corduroy" ) );      // the summary shows what will print
        QCOMPARE( prompter.remaining(), 0 );
}


void TestCli::endOfInputCancels()
{
        qputenv( "GLABELSNEO_ASSUME_TTY", "1" );
        QTemporaryDir dir;
        ScriptedPrompter prompter( {} );
        QCOMPARE( runCli( { "print", LABEL, "-o", dir.filePath( "c.pdf" ) }, &prompter ), static_cast<int>( ExitInterrupted ) );
        qunsetenv( "GLABELSNEO_ASSUME_TTY" );
        QVERIFY( !QFileInfo::exists( dir.filePath( "c.pdf" ) ) );
}


void TestCli::forcedInteractiveAsksForEverything()
{
        qputenv( "GLABELSNEO_ASSUME_TTY", "1" );
        QTemporaryDir dir;
        const QString out = dir.filePath( "i.png" );

        // name: typed; price: Enter keeps the --field value; note: typed; confirm; then "another?" -> no.
        ScriptedPrompter prompter( { "Typed Name", "", "Typed Note", "y", "n" } );
        QCOMPARE( runCli( { "print", LABEL, "-o", out, "-i", "-F", "price=7", "--dpi", "40" }, &prompter ), 0 );
        qunsetenv( "GLABELSNEO_ASSUME_TTY" );

        QVERIFY( QFileInfo::exists( out ) );
        QVERIFY2( prompter.asked().contains( "price [7]" ), qPrintable( prompter.asked() ) );   // default shown
        QVERIFY( prompter.asked().contains( "note" ) );                                         // asked although it has a default
        QVERIFY( prompter.asked().contains( "price=7" ) );
        QVERIFY( prompter.asked().contains( "name=Typed Name" ) );
        QVERIFY( prompter.asked().contains( "another label" ) );
        QCOMPARE( prompter.remaining(), 0 );
}


void TestCli::answeringNoCancelsTheJob()
{
        qputenv( "GLABELSNEO_ASSUME_TTY", "1" );
        QTemporaryDir dir;
        ScriptedPrompter prompter( { "x", "1", "n" } );
        QCOMPARE( runCli( { "print", LABEL, "-o", dir.filePath( "no.pdf" ) }, &prompter ), static_cast<int>( ExitInterrupted ) );
        qunsetenv( "GLABELSNEO_ASSUME_TTY" );
        QVERIFY( !QFileInfo::exists( dir.filePath( "no.pdf" ) ) );
}


void TestCli::versionContract()
{
        // The self-updater of installed copies runs "<tool> --Version" on every downloaded package
        // and requires "GLABELS NEO" in the answer (see update/UpdateService.cpp).
        const Run r = runProcess( GLABELSNEO_EXE, { "--Version" } );
        QCOMPARE( r.exitCode, 0 );
        QVERIFY2( r.out.contains( "GLABELS NEO" ), r.out.constData() );

        const Run shortVersion = runProcess( GLABELSNEO_EXE, { "--version" } );
        QCOMPARE( shortVersion.exitCode, 0 );
        QVERIFY( shortVersion.out.startsWith( "glabelsneo " ) );

        const Run viaCommand = runProcess( GLABELSNEO_EXE, { "version", "--format", "json" } );
        QCOMPARE( viaCommand.exitCode, 0 );
        QCOMPARE( QJsonDocument::fromJson( viaCommand.out ).object()["name"].toString(), QString( "glabelsneo" ) );
}


void TestCli::legacyVersionContract()
{
        // Installed copies still require this program in every package, and run it with --Version.
        const Run r = runProcess( GLABELS_BATCH_EXE, { "--Version" } );
        QCOMPARE( r.exitCode, 0 );
        QVERIFY2( r.out.contains( "GLABELS NEO" ), r.out.constData() );
        QVERIFY( !r.err.contains( "deprecated" ) );    // silent: it is the updater's smoke test
}


void TestCli::jsonResultShape()
{
        QTemporaryDir dir;
        const Run r = runProcess( GLABELSNEO_EXE, { "print", LABEL, "-F", "prise=1", "-F", "name=x", "-o", dir.filePath( "j.pdf" ), "--format", "json" } );
        QCOMPARE( r.exitCode, 0 );

        // One JSON object, nothing else, on standard output.
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson( r.out, &parseError );
        QCOMPARE( parseError.error, QJsonParseError::NoError );
        QVERIFY( doc.isObject() );

        const QJsonObject o = doc.object();
        QVERIFY( o["ok"].toBool() );
        QVERIFY( !o["dryRun"].toBool() );
        QCOMPARE( o["exitCode"].toInt(), 0 );
        QVERIFY( o["errors"].toArray().isEmpty() );
        QCOMPARE( o["jobs"].toArray().size(), 1 );
        QCOMPARE( o["jobs"].toArray().at( 0 ).toObject()["status"].toString(), QString( "pdf" ) );

        // The typo and the missing field are reported as stable codes.
        QStringList codes;
        for ( const QJsonValue& w : o["warnings"].toArray() )
        {
            codes << w.toObject()["code"].toString();
        }
        QVERIFY2( codes.contains( "field.unused" ), qPrintable( codes.join( "," ) ) );
        QVERIFY( codes.contains( "field.missing" ) );
        QVERIFY( !r.err.contains( "warning" ) );    // JSON mode keeps standard error quiet too
}


void TestCli::jsonUsageError()
{
        const Run r = runProcess( GLABELSNEO_EXE, { "print", LABEL, "--format", "json" } );    // no target
        QCOMPARE( r.exitCode, static_cast<int>( ExitUsage ) );
        const QJsonObject o = QJsonDocument::fromJson( r.out ).object();
        QVERIFY( !o["ok"].toBool() );
        QCOMPARE( o["exitCode"].toInt(), static_cast<int>( ExitUsage ) );
        QCOMPARE( o["errors"].toArray().at( 0 ).toObject()["code"].toString(), QString( "usage" ) );

        const Run unknown = runProcess( GLABELSNEO_EXE, { "nonsense", "--format=json" } );
        QCOMPARE( unknown.exitCode, static_cast<int>( ExitUsage ) );
        QVERIFY( !QJsonDocument::fromJson( unknown.out ).object().isEmpty() );
}


void TestCli::pdfOnStdoutIsClean()
{
        const Run r = runProcess( GLABELSNEO_EXE, { "print", LABEL, "-F", "price=1", "-F", "name=x", "-o", "-" } );
        QCOMPARE( r.exitCode, 0 );
        QVERIFY( r.out.startsWith( "%PDF" ) );
        QVERIFY( r.out.contains( "%%EOF" ) );
        QVERIFY( !r.out.contains( "Wrote" ) );          // the human summary went to standard error
        QVERIFY( r.err.contains( "standard output" ) );

        const Run json = runProcess( GLABELSNEO_EXE, { "print", LABEL, "-o", "-", "--format", "json" } );
        QCOMPARE( json.exitCode, static_cast<int>( ExitUsage ) );
}


void TestCli::labelFromStdin()
{
        QFile f( LABEL );
        QVERIFY( f.open( QIODevice::ReadOnly ) );
        QTemporaryDir dir;
        const Run r = runProcess( GLABELSNEO_EXE, { "print", "-", "-F", "price=5", "-F", "name=Piped", "-o", dir.filePath( "s.pdf" ), "--format", "json" }, f.readAll() );
        QCOMPARE( r.exitCode, 0 );
        QVERIFY( startsWithPdf( dir.filePath( "s.pdf" ) ) );
        QVERIFY( QJsonDocument::fromJson( r.out ).object()["ok"].toBool() );
}


void TestCli::inspectReportsFields()
{
        const Run r = runProcess( GLABELSNEO_EXE, { "inspect", LABEL, "--format", "json" } );
        QCOMPARE( r.exitCode, 0 );
        const QJsonObject o = QJsonDocument::fromJson( r.out ).object();
        QVERIFY( o["ok"].toBool() );
        QCOMPARE( o["template"].toObject()["part"].toString(), QString( "DK-1201" ) );
        QCOMPARE( o["label"].toObject()["labelsPerSheet"].toInt(), 1 );

        const QJsonArray fields = o["fields"].toArray();
        QCOMPARE( fields.size(), 3 );
        QCOMPARE( fields.at( 0 ).toObject()["name"].toString(), QString( "name" ) );
        QVERIFY( fields.at( 0 ).toObject()["required"].toBool() );
        QCOMPARE( fields.at( 2 ).toObject()["name"].toString(), QString( "note" ) );
        QVERIFY( !fields.at( 2 ).toObject()["required"].toBool() );

        const Run text = runProcess( GLABELSNEO_EXE, { "inspect", LABEL } );
        QCOMPARE( text.exitCode, 0 );
        QVERIFY( text.out.contains( "price" ) );

        const Run missing = runProcess( GLABELSNEO_EXE, { "inspect", "/no/such/file.glabels", "--format", "json" } );
        QCOMPARE( missing.exitCode, static_cast<int>( ExitInput ) );
}


void TestCli::printersListsAsJson()
{
        const Run r = runProcess( GLABELSNEO_EXE, { "printers", "--format", "json" } );
        QCOMPARE( r.exitCode, 0 );
        const QJsonObject o = QJsonDocument::fromJson( r.out ).object();
        QVERIFY( o["ok"].toBool() );
        QVERIFY( o["printers"].isArray() );    // may be empty on a build machine

        const Run check = runProcess( GLABELSNEO_EXE, { "printers", "--check", "No Such Printer 12345", "--format", "json" } );
        QCOMPARE( check.exitCode, static_cast<int>( ExitPrinter ) );
        QCOMPARE( QJsonDocument::fromJson( check.out ).object()["errors"].toArray().at( 0 ).toObject()["code"].toString(),
                  QString( "printer.notfound" ) );
}


void TestCli::helpAndUnknownCommand()
{
        const Run help = runProcess( GLABELSNEO_EXE, { "--help" } );
        QCOMPARE( help.exitCode, 0 );
        QVERIFY( help.out.contains( "print" ) && help.out.contains( "inspect" ) && help.out.contains( "printers" ) );

        const Run printHelp = runProcess( GLABELSNEO_EXE, { "print", "--help" } );
        QCOMPARE( printHelp.exitCode, 0 );
        QVERIFY2( printHelp.out.contains( "Usage: glabelsneo print" ), printHelp.out.constData() );
        QVERIFY( printHelp.out.contains( "--field" ) && printHelp.out.contains( "--printer" ) && printHelp.out.contains( "--job" ) );

        const Run viaHelp = runProcess( GLABELSNEO_EXE, { "help", "print" } );
        QCOMPARE( viaHelp.exitCode, 0 );
        QVERIFY( viaHelp.out.contains( "--dry-run" ) );

        const Run none = runProcess( GLABELSNEO_EXE, {} );
        QCOMPARE( none.exitCode, static_cast<int>( ExitUsage ) );
}


void TestCli::legacyToolStillPrints()
{
        // glabels-batch-qt keeps working for one release, with a deprecation notice on standard error.
        QTemporaryDir dir;
        const Run r = runProcess( GLABELS_BATCH_EXE, { "-o", dir.filePath( "legacy.pdf" ), PLAIN } );
        QCOMPARE( r.exitCode, 0 );
        QVERIFY( startsWithPdf( dir.filePath( "legacy.pdf" ) ) );
        QVERIFY2( r.err.contains( "deprecated" ), r.err.constData() );
        QVERIFY( r.err.contains( "glabelsneo print" ) );
}


void TestCli::schemaMatchesTheLoader()
{
        const Run r = runProcess( GLABELSNEO_EXE, { "schema" } );
        QCOMPARE( r.exitCode, 0 );

        QJsonParseError error;
        const QJsonObject schema = QJsonDocument::fromJson( r.out, &error ).object();
        QCOMPARE( error.error, QJsonParseError::NoError );
        QCOMPARE( schema["$schema"].toString(), QString( "https://json-schema.org/draft/2020-12/schema" ) );

        // The schema must list exactly the keys the loader accepts (it rejects all others).
        const QJsonObject defs = schema["$defs"].toObject();
        QStringList job = defs["job"].toObject()["properties"].toObject().keys();
        job.sort();
        QCOMPARE( job, QStringList( { "data", "dataFormat", "defaults", "file", "id", "labels", "options", "output", "printer", "variables", "version" } ) );

        QStringList options = defs["options"].toObject()["properties"].toObject().keys();
        options.sort();
        QCOMPARE( options, QStringList( { "collate", "copies", "cropMarks", "first", "group", "outlines", "reverse", "sheets" } ) );

        QStringList label = defs["label"].toObject()["properties"].toObject().keys();
        label.sort();
        QCOMPARE( label, QStringList( { "copies", "fields" } ) );

        // Every data format the schema names is one the tool really accepts.
        QStringList formats;
        for ( const QJsonValue& v : defs["job"].toObject()["properties"].toObject()["dataFormat"].toObject()["enum"].toArray() )
        {
                formats << v.toString();
        }
        QVERIFY( formats.contains( "json" ) && formats.contains( "jsonl" ) && formats.contains( "csv-nokeys" ) );

        // ...and the same document the schema describes is accepted by the tool.
        QTemporaryDir dir;
        QFile doc( dir.filePath( "d.json" ) );
        QVERIFY( doc.open( QIODevice::WriteOnly ) );
        doc.write( QString( R"({"version":1,"file":"%1","output":"o.png","dataFormat":"json","data":"%2/items.json"})" )
                   .arg( LABEL, QString( TEST_DATA_DIR ) ).toUtf8() );
        doc.close();
        QCOMPARE( runProcess( GLABELSNEO_EXE, { "print", "--job", doc.fileName(), "--dry-run" } ).exitCode, 0 );
}


QTEST_MAIN( TestCli )
