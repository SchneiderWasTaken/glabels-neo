//  TestJobs.cpp
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

#include "TestJobs.hpp"

#include "Executor.hpp"
#include "Exit.hpp"
#include "Inputs.hpp"
#include "JobLoader.hpp"

#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/FieldScan.hpp"
#include "model/Settings.hpp"
#include "model/Variable.hpp"

#include <QDir>

using namespace glabels::cli;


namespace
{
        QByteArray doc( const char* json )
        {
                return QByteArray( json );
        }
}


void TestJobs::initTestCase()
{
        QCoreApplication::setOrganizationName( "glabels.org" );
        QCoreApplication::setApplicationName( "glabelsneo-test" );
        glabels::model::Settings::init();
        glabels::model::Db::init();
        glabels::merge::Factory::init();
}


void TestJobs::assignmentSplitsOnTheFirstEquals()
{
        QString n, v;
        QVERIFY( JobLoader::parseAssignment( "price=29.99", n, v ) );
        QCOMPARE( n, QString( "price" ) );
        QCOMPARE( v, QString( "29.99" ) );

        QVERIFY( JobLoader::parseAssignment( "formula=a=b=c", n, v ) );   // the old -D rejected this
        QCOMPARE( n, QString( "formula" ) );
        QCOMPARE( v, QString( "a=b=c" ) );

        QVERIFY( JobLoader::parseAssignment( "empty=", n, v ) );
        QCOMPARE( v, QString() );

        QVERIFY( JobLoader::parseAssignment( "name=Blue Jeans \xC3\xA9", n, v ) );
        QCOMPARE( v, QString::fromUtf8( "Blue Jeans \xC3\xA9" ) );
}


void TestJobs::assignmentRejectsBadNames()
{
        QString n, v;
        QVERIFY( !JobLoader::parseAssignment( "novalue", n, v ) );
        QVERIFY( !JobLoader::parseAssignment( "=x", n, v ) );
        QVERIFY( !JobLoader::parseAssignment( "a:b=x", n, v ) );      // ':' separates modifiers
        QVERIFY( !JobLoader::parseAssignment( "a}b=x", n, v ) );
        QVERIFY( !JobLoader::parseAssignment( "$a=x", n, v ) );
}


void TestJobs::oneJobShorthand()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( JobLoader::parseJobDocument( doc( R"({
                "version": 1, "file": "/x/jeans.glabels", "printer": "P1", "defaults": {"cur":"USD"},
                "variables": {"batch":"A7"},
                "options": {"copies": 2, "first": 3, "collate": true},
                "labels": [ {"fields":{"price":29.99,"ok":true}, "copies": 2}, {"fields":{}} ] })" ),
                "/base", jobs, e ) );
        QCOMPARE( jobs.size(), 1 );
        const JobSpec& j = jobs[0];
        QCOMPARE( j.file, QString( "/x/jeans.glabels" ) );
        QCOMPARE( j.printer, QString( "P1" ) );
        QVERIFY( j.output.isEmpty() );
        QCOMPARE( j.defaults.value( "cur" ), QString( "USD" ) );
        QCOMPARE( j.variables.value( "batch" ), QString( "A7" ) );
        QCOMPARE( *j.options.copies, 2 );
        QCOMPARE( *j.options.first, 3 );
        QCOMPARE( *j.options.collate, true );
        QVERIFY( !j.options.sheets );
        QVERIFY( j.hasLabels );
        QCOMPARE( j.labels.size(), 2 );
        QCOMPARE( j.labels[0].copies, 2 );
        QCOMPARE( j.labels[0].fields.value( "price" ), QString( "29.99" ) );
        QCOMPARE( j.labels[0].fields.value( "ok" ), QString( "true" ) );
        QCOMPARE( j.labels[1].copies, 1 );
}


void TestJobs::severalJobs()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( JobLoader::parseJobDocument( doc( R"({"jobs":[
                {"id":"a","file":"/a.glabels","printer":"P","labels":[]},
                {"id":"b","file":"/b.glabels","output":"/o.pdf","data":"/d.csv","dataFormat":"csv"} ]})" ),
                "/base", jobs, e ) );
        QCOMPARE( jobs.size(), 2 );
        QCOMPARE( jobs[0].id, QString( "a" ) );
        QCOMPARE( jobs[0].index, 0 );
        QVERIFY( jobs[0].hasLabels );
        QCOMPARE( jobs[1].index, 1 );
        QCOMPARE( jobs[1].data, QString( "/d.csv" ) );
        QCOMPARE( jobs[1].dataFormat, QString( "csv" ) );
        QVERIFY( !jobs[1].hasLabels );
}


void TestJobs::relativePathsResolveAgainstTheDocument()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        const QString base = QDir::cleanPath( QDir::tempPath() + "/docdir" );
        QVERIFY( JobLoader::parseJobDocument( doc( R"({"file":"sub/a.glabels","output":"out/a.pdf","data":"d.json"})" ),
                                              base, jobs, e ) );
        QCOMPARE( jobs[0].file,   base + "/sub/a.glabels" );
        QCOMPARE( jobs[0].output, base + "/out/a.pdf" );
        QCOMPARE( jobs[0].data,   base + "/d.json" );

        // Standard input and absolute paths are left alone.
        jobs.clear();
        QVERIFY( JobLoader::parseJobDocument( doc( R"({"file":"-","output":"-"})" ), base, jobs, e ) );
        QCOMPARE( jobs[0].file, QString( "-" ) );
        QCOMPARE( jobs[0].output, QString( "-" ) );
}


void TestJobs::unknownKeysAreErrors()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( !JobLoader::parseJobDocument( doc( R"({"file":"a","pritner":"P"})" ), "/", jobs, e ) );
        QCOMPARE( e.code, QString( "data.invalid" ) );
        QCOMPARE( e.exitCode, ExitData );
        QVERIFY2( e.message.contains( "pritner" ), qPrintable( e.message ) );

        QVERIFY( !JobLoader::parseJobDocument( doc( R"({"jobs":[{"file":"a","bogus":1}]})" ), "/", jobs, e ) );
        QVERIFY( e.message.contains( "jobs[0]" ) );

        QVERIFY( !JobLoader::parseJobDocument( doc( R"({"file":"a","labels":[{"feilds":{}}]})" ), "/", jobs, e ) );
        QVERIFY( e.message.contains( "labels[0]" ) );
}


void TestJobs::printerAndOutputAreExclusive()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( !JobLoader::parseJobDocument( doc( R"({"file":"a","printer":"P","output":"o.pdf"})" ), "/", jobs, e ) );
        QCOMPARE( e.exitCode, ExitUsage );
}


void TestJobs::labelsAndDataAreExclusive()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( !JobLoader::parseJobDocument( doc( R"({"file":"a","data":"d.csv","labels":[]})" ), "/", jobs, e ) );
        QCOMPARE( e.exitCode, ExitData );
}


void TestJobs::optionTypesAreChecked()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        for ( const char* text : { R"({"file":"a","options":{"copies":0}})", R"({"file":"a","options":{"copies":"2"}})",
                                   R"({"file":"a","options":{"copies":1.5}})", R"({"file":"a","options":{"collate":"yes"}})",
                                   R"({"file":"a","options":{"nope":1}})", R"({"file":"a","options":3})",
                                   R"({"file":"a","labels":[{"copies":0}]})", R"({"file":"a","labels":"x"})" } )
        {
                jobs.clear();
                QVERIFY2( !JobLoader::parseJobDocument( doc( text ), "/", jobs, e ), text );
                QCOMPARE( e.exitCode, ExitData );
        }
}


void TestJobs::reservedNamesAreRejected()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( !JobLoader::parseJobDocument( doc( R"({"file":"a","defaults":{"_x":"1"}})" ), "/", jobs, e ) );
        QVERIFY( e.message.contains( "reserved" ) );
}


void TestJobs::unsupportedVersionIsRejected()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( !JobLoader::parseJobDocument( doc( R"({"version":2,"file":"a"})" ), "/", jobs, e ) );
        QVERIFY( e.message.contains( "version" ) );
}


void TestJobs::badJsonSaysWhere()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( !JobLoader::parseJobDocument( doc( "{\n \"file\": \n}" ), "/", jobs, e ) );
        QVERIFY2( e.message.contains( "line 3" ), qPrintable( e.message ) );

        QVERIFY( !JobLoader::parseJobDocument( doc( "[1,2]" ), "/", jobs, e ) );
        QVERIFY( e.message.contains( "object" ) );
}


void TestJobs::overridesWin()
{
        QList<JobSpec> jobs;
        Diagnostic e;
        QVERIFY( JobLoader::parseJobDocument( doc( R"({"file":"/a.glabels","output":"o.pdf","variables":{"v":"1"},
                "options":{"copies":5,"outlines":true},"labels":[{"fields":{"a":"1"}}]})" ), "/", jobs, e ) );

        CliJobInput in;
        in.printer = "Shop Printer";
        in.fields  = { "x=1", "y=2=3" };
        in.variables = { "v=9" };
        in.options.copies = 7;
        QVERIFY( JobLoader::applyOverrides( in, jobs[0], e ) );

        QCOMPARE( jobs[0].printer, QString( "Shop Printer" ) );
        QVERIFY( jobs[0].output.isEmpty() );                       // choosing a printer replaces the document's output
        QCOMPARE( *jobs[0].options.copies, 7 );                    // flag beats document
        QCOMPARE( *jobs[0].options.outlines, true );               // untouched option stays
        QCOMPARE( jobs[0].variables.value( "v" ), QString( "9" ) );
        QCOMPARE( jobs[0].cliFields.value( "y" ), QString( "2=3" ) );
        QVERIFY( jobs[0].hasLabels );

        // --data replaces inline labels
        CliJobInput withData;
        withData.data = "d.csv";
        QVERIFY( JobLoader::applyOverrides( withData, jobs[0], e ) );
        QVERIFY( !jobs[0].hasLabels );
        QVERIFY( jobs[0].labels.isEmpty() );
        QCOMPARE( jobs[0].data, QString( "d.csv" ) );

        CliJobInput bad;
        bad.fields = { "nope" };
        QVERIFY( !JobLoader::applyOverrides( bad, jobs[0], e ) );
        QCOMPARE( e.exitCode, ExitUsage );

        CliJobInput reserved;
        reserved.fields = { "_copies=3" };
        QVERIFY( !JobLoader::applyOverrides( reserved, jobs[0], e ) );
}


void TestJobs::scanFindsFieldsAndDefaults()
{
        Diagnostic e;
        auto model = Inputs::loadModel( QString( TEST_DATA_DIR ) + "/fields.glabels", e );
        QVERIFY2( model, qPrintable( e.message ) );

        const auto refs = glabels::model::FieldScan::scan( *model );
        QCOMPARE( refs.size(), 3 );

        QStringList names;
        for ( const auto& r : refs ) names << r.name;
        QCOMPARE( names, QStringList( { "name", "price", "note" } ) );    // order of first use

        QVERIFY( !refs[0].hasDefault );
        QVERIFY( !refs[1].hasDefault );    // ${price:%.2f} has a format, not a default
        QVERIFY( refs[2].hasDefault );     // ${note:=no note}
        QVERIFY( !refs[0].isVariable );
}


void TestJobs::scanSeparatesVariables()
{
        Diagnostic e;
        auto model = Inputs::loadModel( QString( TEST_DATA_DIR ) + "/fields.glabels", e );
        QVERIFY( model );

        model->variables().addVariable( glabels::model::Variable( glabels::model::Variable::Type::STRING, "price", "1" ) );
        const auto refs = glabels::model::FieldScan::scan( *model );
        for ( const auto& r : refs )
        {
                QCOMPARE( r.isVariable, r.name == "price" );
        }
}


void TestJobs::precedenceDataFileWinsByDefault()
{
        JobSpec job;
        job.defaults  = { { "currency", "USD" }, { "price", "1" } };
        job.cliFields = { { "price", "2" }, { "extra", "x" } };
        QMap<QString,QString> label = { { "price", "3" } };

        // lowest to highest: job defaults < --field < the data file's record
        auto r = Executor::effectiveFields( job, label, false );
        QCOMPARE( r.value( "price" ), QString( "3" ) );
        QCOMPARE( r.value( "currency" ), QString( "USD" ) );
        QCOMPARE( r.value( "extra" ), QString( "x" ) );

        // a record that lacks the key falls through to --field, then the defaults
        r = Executor::effectiveFields( job, {}, false );
        QCOMPARE( r.value( "price" ), QString( "2" ) );
}


void TestJobs::forceFieldsReversesIt()
{
        JobSpec job;
        job.defaults  = { { "price", "1" } };
        job.cliFields = { { "price", "2" } };
        QMap<QString,QString> label = { { "price", "3" }, { "name", "n" } };

        auto r = Executor::effectiveFields( job, label, true );
        QCOMPARE( r.value( "price" ), QString( "2" ) );   // --field beats the record
        QCOMPARE( r.value( "name" ), QString( "n" ) );    // the rest of the record is kept
}


void TestJobs::closestNameFindsTypos()
{
        const QStringList names = { "price", "name", "note" };
        QCOMPARE( Executor::closestName( "prise", names ), QString( "price" ) );
        QCOMPARE( Executor::closestName( "Price", names ), QString( "price" ) );   // case only
        QCOMPARE( Executor::closestName( "nmae", names ), QString( "name" ) );
        QCOMPARE( Executor::closestName( "completely-different", names ), QString() );
        QCOMPARE( Executor::closestName( "x", {} ), QString() );
}


QTEST_MAIN( TestJobs )
