//  TestRecords.cpp
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

#include "TestRecords.hpp"

#include "merge/Factory.hpp"
#include "merge/JsonMerge.hpp"
#include "merge/JsonRecords.hpp"
#include "merge/MemoryMerge.hpp"

#include <QFile>
#include <QTemporaryDir>

using glabels::merge::JsonRecords;
using glabels::merge::Record;


namespace
{
        QByteArray bytes( const char* text )
        {
                return QByteArray( text );
        }
}


void TestRecords::initTestCase()
{
        glabels::merge::Factory::init();
}


void TestRecords::arrayOfObjects()
{
        auto r = JsonRecords::parseArray( bytes( R"([{"name":"Blue Jeans","price":"29.99"},{"name":"Tee","size":"L"}])" ) );
        QVERIFY( r.ok );
        QCOMPARE( r.records.size(), 2 );
        QCOMPARE( r.records[0]["name"], QString( "Blue Jeans" ) );
        QCOMPARE( r.records[1]["size"], QString( "L" ) );
        QCOMPARE( r.keys, QStringList( { "name", "price", "size" } ) );   // first-seen order
        QCOMPARE( r.copies, QList<int>( { 1, 1 } ) );
}


void TestRecords::singleObjectIsOneRecord()
{
        auto r = JsonRecords::parseArray( bytes( R"({"a":"1"})" ) );
        QVERIFY( r.ok );
        QCOMPARE( r.records.size(), 1 );
}


void TestRecords::jsonLinesSkipBlankLines()
{
        auto r = JsonRecords::parseLines( bytes( "{\"a\":\"1\"}\n\n  \r\n{\"a\":\"2\",\"b\":\"x\"}\n" ) );
        QVERIFY( r.ok );
        QCOMPARE( r.records.size(), 2 );
        QCOMPARE( r.records[1]["b"], QString( "x" ) );
}


void TestRecords::numbersKeepTheirShortestForm()
{
        auto r = JsonRecords::parseArray( bytes( R"([{"a":29.99,"b":12,"c":1e3,"d":0.1,"e":-5,"f":4006381333931}])" ) );
        QVERIFY( r.ok );
        QCOMPARE( r.records[0]["a"], QString( "29.99" ) );     // not 29.989999999999998
        QCOMPARE( r.records[0]["b"], QString( "12" ) );
        QCOMPARE( r.records[0]["c"], QString( "1000" ) );
        QCOMPARE( r.records[0]["d"], QString( "0.1" ) );
        QCOMPARE( r.records[0]["e"], QString( "-5" ) );
        QCOMPARE( r.records[0]["f"], QString( "4006381333931" ) );   // an EAN-13 survives as digits
}


void TestRecords::booleansAndNull()
{
        auto r = JsonRecords::parseArray( bytes( R"([{"a":true,"b":false,"c":null}])" ) );
        QVERIFY( r.ok );
        QCOMPARE( r.records[0]["a"], QString( "true" ) );
        QCOMPARE( r.records[0]["b"], QString( "false" ) );
        QVERIFY( r.records[0].contains( "c" ) );
        QCOMPARE( r.records[0]["c"], QString() );
}


void TestRecords::nestedValuesReportTheirPath()
{
        auto r = JsonRecords::parseArray( bytes( R"([{"a":"1"},{"x":{"y":1}}])" ) );
        QVERIFY( !r.ok );
        QVERIFY2( r.error.contains( "$[1].x" ), qPrintable( r.error ) );
        QVERIFY( r.error.contains( "object" ) );

        r = JsonRecords::parseArray( bytes( R"([{"x":[1,2]}])" ) );
        QVERIFY( !r.ok );
        QVERIFY( r.error.contains( "array" ) );

        r = JsonRecords::parseLines( bytes( "{\"a\":1}\n{\"b\":{\"c\":2}}\n" ) );
        QVERIFY( !r.ok );
        QVERIFY2( r.error.contains( "line 2" ), qPrintable( r.error ) );

        r = JsonRecords::parseArray( bytes( R"(["just a string"])" ) );
        QVERIFY( !r.ok );
        QVERIFY( r.error.contains( "not an object" ) );
}


void TestRecords::copiesAndReservedKeys()
{
        auto r = JsonRecords::parseArray( bytes( R"([{"a":"1","_copies":3,"_note":"ignored"}])" ) );
        QVERIFY( r.ok );
        QCOMPARE( r.copies, QList<int>( { 3 } ) );
        QVERIFY( !r.records[0].contains( "_copies" ) );
        QVERIFY( !r.records[0].contains( "_note" ) );
        QCOMPARE( r.keys, QStringList( { "a" } ) );
}


void TestRecords::badCopiesIsAnError()
{
        for ( const char* text : { R"([{"_copies":0}])", R"([{"_copies":-2}])", R"([{"_copies":"many"}])", R"([{"_copies":true}])", R"([{"_copies":1.5}])" } )
        {
                auto r = JsonRecords::parseArray( bytes( text ) );
                QVERIFY2( !r.ok, text );
                QVERIFY2( r.error.contains( "_copies" ), text );
        }
}


void TestRecords::utf8BomIsStripped()
{
        auto r = JsonRecords::parseArray( QByteArray( "\xEF\xBB\xBF" ) + bytes( R"([{"a":"1"}])" ) );
        QVERIFY2( r.ok, qPrintable( r.error ) );
        QCOMPARE( r.records.size(), 1 );
}


void TestRecords::utf16IsExplained()
{
        // What Windows PowerShell 5.1 writes for "> file": UTF-16 LE with a byte order mark.
        QByteArray utf16 = QByteArray( "\xFF\xFE" ) + QByteArray( "[\0{\0}\0]\0", 8 );
        auto r = JsonRecords::parseArray( utf16 );
        QVERIFY( !r.ok );
        QVERIFY2( r.error.contains( "UTF-16" ), qPrintable( r.error ) );
        QVERIFY( r.error.contains( "utf8" ) );

        // No byte order mark, but NUL bytes give it away.
        r = JsonRecords::parseArray( QByteArray( "[\0{\0}\0]\0", 8 ) );
        QVERIFY( !r.ok );
        QVERIFY( r.error.contains( "UTF-16" ) );
}


void TestRecords::syntaxErrorsSaySayWhere()
{
        auto r = JsonRecords::parseArray( bytes( "[\n  {\"a\": 1},\n  {\"a\": }\n]" ) );
        QVERIFY( !r.ok );
        QVERIFY2( r.error.contains( "line 3" ), qPrintable( r.error ) );
}


void TestRecords::emptyArrayIsFine()
{
        auto r = JsonRecords::parseArray( bytes( "[]" ) );
        QVERIFY( r.ok );
        QVERIFY( r.records.isEmpty() );
}


void TestRecords::memoryMergeKeepsEmptyRecords()
{
        // An empty record is the end-of-data marker of Merge's read loop; a label with no
        // fields must not cut the run short.
        glabels::merge::MemoryMerge merge;
        QList<Record> records;
        Record a; a.insert( "x", "1" );
        Record empty;
        Record c; c.insert( "x", "3" );
        records << a << empty << c;

        merge.setRecords( { "x" }, records );
        QCOMPARE( merge.recordList().size(), 3 );
        QCOMPARE( merge.nSelectedRecords(), 3 );
        QCOMPARE( merge.keys(), QStringList( { "x" } ) );
        QCOMPARE( merge.primaryKey(), QString( "x" ) );

        std::unique_ptr<glabels::merge::MemoryMerge> copy( merge.clone() );
        QCOMPARE( copy->recordList().size(), 3 );
        QCOMPARE( copy->keys(), merge.keys() );
}


void TestRecords::jsonMergeBackendReadsAFile()
{
        QTemporaryDir dir;
        QVERIFY( dir.isValid() );
        QFile file( dir.filePath( "r.json" ) );
        QVERIFY( file.open( QIODevice::WriteOnly ) );
        file.write( R"([{"name":"A","price":1.5},{"name":"B"}])" );
        file.close();

        std::unique_ptr<glabels::merge::Merge> merge( glabels::merge::Factory::createMerge( glabels::merge::Json::id() ) );
        QVERIFY( merge );
        merge->setSource( file.fileName() );
        QCOMPARE( merge->recordList().size(), 2 );
        QCOMPARE( merge->keys(), QStringList( { "name", "price" } ) );
        QCOMPARE( merge->recordList()[0]["price"], QString( "1.5" ) );
}


void TestRecords::jsonMergeExpandsCopies()
{
        QTemporaryDir dir;
        QVERIFY( dir.isValid() );
        QFile file( dir.filePath( "r.jsonl" ) );
        QVERIFY( file.open( QIODevice::WriteOnly ) );
        file.write( "{\"a\":\"1\",\"_copies\":3}\n{\"a\":\"2\"}\n" );
        file.close();

        std::unique_ptr<glabels::merge::Merge> merge( glabels::merge::Factory::createMerge( glabels::merge::JsonLines::id() ) );
        QVERIFY( merge );
        merge->setSource( file.fileName() );
        QCOMPARE( merge->recordList().size(), 4 );   // 3 copies + 1
}


void TestRecords::jsonBackendsAreRegistered()
{
        // Appended after the existing backends, so saved indices keep their meaning.
        const QStringList names = glabels::merge::Factory::nameList();
        QVERIFY( names.contains( "JSON: array of records" ) );
        QVERIFY( names.contains( "JSON Lines: one record per line" ) );
        QCOMPARE( names.first(), QString( "None" ) );
}


QTEST_MAIN( TestRecords )
