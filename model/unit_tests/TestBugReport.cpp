//  TestBugReport.cpp
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


#include "TestBugReport.hpp"

#include "model/BugReport.hpp"
#include "model/LogBuffer.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>


QTEST_MAIN(TestBugReport)


using namespace glabels::model;


void TestBugReport::leadingZeroBits()
{
        QCOMPARE( BugReport::leadingZeroBits( QByteArray::fromHex( "ff00" ) ), 0 );
        QCOMPARE( BugReport::leadingZeroBits( QByteArray::fromHex( "7f00" ) ), 1 );
        QCOMPARE( BugReport::leadingZeroBits( QByteArray::fromHex( "0100" ) ), 7 );
        QCOMPARE( BugReport::leadingZeroBits( QByteArray::fromHex( "0080" ) ), 8 );
        QCOMPARE( BugReport::leadingZeroBits( QByteArray::fromHex( "000f" ) ), 12 );
        QCOMPARE( BugReport::leadingZeroBits( QByteArray::fromHex( "0000" ) ), 16 );
}


void TestBugReport::proofOfWork()
{
        // Same rule the relay verifies: SHA-256( challenge + ":" + counter ) has `bits` leading zero bits.
        const QString challenge = "abc.def";
        const QString counter = BugReport::solveProofOfWork( challenge, 12 );
        QVERIFY( !counter.isEmpty() );
        QVERIFY( counter.length() <= 15 );

        const QByteArray digest =
                QCryptographicHash::hash( ( challenge + ":" + counter ).toUtf8(), QCryptographicHash::Sha256 );
        QVERIFY( BugReport::leadingZeroBits( digest ) >= 12 );
}


void TestBugReport::proofOfWorkCancel()
{
        QVERIFY( BugReport::solveProofOfWork( "x", 40, []() { return false; } ).isEmpty() );
        QVERIFY( BugReport::solveProofOfWork( "x", 40, {}, 1000 ).isEmpty() );
}


void TestBugReport::redactHomeAndEmail()
{
        const QString home = QDir::homePath();
        const QString text = QString( "file %1/labels/a.glabels, mail me at someone@example.com, printer 192.168.1.20" ).arg( home );
        const QString out = BugReport::redact( text );

        QVERIFY( !out.contains( home ) );
        QVERIFY( out.contains( "~/labels/a.glabels" ) );
        QVERIFY( !out.contains( "someone@example.com" ) );
        QVERIFY( out.contains( "<email>" ) );
        QVERIFY( !out.contains( "192.168.1.20" ) );
        QVERIFY( out.contains( "<ip>" ) );
}


void TestBugReport::json()
{
        QList<BugReport::Attachment> files;
        files.append( { "log", "log.txt", "line1" } );

        const QByteArray body = BugReport::toJson( "chal", "42", "sum", "desc text", "diag", files );
        const QJsonObject o = QJsonDocument::fromJson( body ).object();

        QCOMPARE( o["schema"].toInt(), 1 );
        QCOMPARE( o["pow"].toObject()["challenge"].toString(), QString( "chal" ) );
        QCOMPARE( o["pow"].toObject()["counter"].toString(), QString( "42" ) );
        QCOMPARE( o["summary"].toString(), QString( "sum" ) );
        QCOMPARE( o["description"].toString(), QString( "desc text" ) );
        QCOMPARE( o["diagnostics"].toString(), QString( "diag" ) );
        QCOMPARE( o["attachments"].toArray().size(), 1 );
        QCOMPARE( o["attachments"].toArray()[0].toObject()["kind"].toString(), QString( "log" ) );
}


void TestBugReport::prefilledUrl()
{
        const QUrl url = BugReport::prefilledIssueUrl( "Crash", QString( "x" ).repeated( 20000 ) );
        QVERIFY( url.toEncoded().length() <= 7000 );

        const QUrlQuery query( url );
        QCOMPARE( query.queryItemValue( "template" ), QString( "bug_report.yml" ) );
        QVERIFY( query.queryItemValue( "title" ).contains( "Crash" ) );
        QVERIFY( query.queryItemValue( "what" ).contains( "[truncated]" ) );
        QVERIFY( url.path().endsWith( "/issues/new" ) );
}


void TestBugReport::logBuffer()
{
        LogBuffer::append( "first" );
        for ( int i = 0; i < 400; i++ )
        {
                LogBuffer::append( QString( "line %1" ).arg( i ) );
        }
        const QString recent = LogBuffer::recent();
        QVERIFY( !recent.contains( "first" ) );
        QVERIFY( recent.contains( "line 399" ) );
        QVERIFY( recent.split( '\n' ).size() <= 300 );
}
