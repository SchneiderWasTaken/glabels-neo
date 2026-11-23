//  TestReportBugDialog.cpp
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


#include "TestReportBugDialog.hpp"

#include "ReportBugDialog.hpp"

#include "model/BugReport.hpp"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>


QTEST_MAIN(TestReportBugDialog)


using namespace glabels;


namespace
{

        ///
        /// A stand-in for the report relay that speaks the same protocol and checks what the dialog sends:
        /// user agent, schema, and a valid proof of work.
        ///
        class MockRelay : public QTcpServer
        {
        public:
                QList<QJsonObject> reports;
                int                reportStatus  = 201;
                int                challengeBits = 8;

                explicit MockRelay( QObject* parent = nullptr ) : QTcpServer( parent )
                {
                        connect( this, &QTcpServer::newConnection, this,
                                 [this]()
                                 {
                                         while ( hasPendingConnections() )
                                         {
                                                 QTcpSocket* s = nextPendingConnection();
                                                 connect( s, &QTcpSocket::readyRead, this, [this, s]() { onReadyRead( s ); } );
                                                 connect( s, &QTcpSocket::disconnected, s, &QObject::deleteLater );
                                         }
                                 } );
                }

        private:
                void onReadyRead( QTcpSocket* s )
                {
                        const QByteArray data = s->property( "buf" ).toByteArray() + s->readAll();
                        s->setProperty( "buf", data );

                        const int headerEnd = data.indexOf( "\r\n\r\n" );
                        if ( headerEnd < 0 )
                        {
                                return;
                        }

                        const QByteArray head = data.left( headerEnd );
                        int length = 0;
                        QString userAgent;
                        for ( const QByteArray& line : head.split( '\n' ) )
                        {
                                const QString text = QString::fromLatin1( line ).trimmed();
                                if ( text.startsWith( "content-length:", Qt::CaseInsensitive ) )
                                {
                                        length = text.mid( 15 ).trimmed().toInt();
                                }
                                else if ( text.startsWith( "user-agent:", Qt::CaseInsensitive ) )
                                {
                                        userAgent = text.mid( 11 ).trimmed();
                                }
                        }

                        const QByteArray body = data.mid( headerEnd + 4 );
                        if ( body.size() < length )
                        {
                                return;
                        }
                        s->setProperty( "buf", QByteArray() );

                        respond( s, QString::fromLatin1( head.left( head.indexOf( '\r' ) ) ), userAgent, body.left( length ) );
                }

                void respond( QTcpSocket* s, const QString& requestLine, const QString& userAgent, const QByteArray& body )
                {
                        int        status = 404;
                        QByteArray payload = "{}";

                        if ( requestLine.startsWith( "GET /v1/challenge" ) )
                        {
                                status  = 200;
                                payload = QJsonDocument( QJsonObject{ { "challenge", "mock-challenge" }, { "bits", challengeBits } } )
                                                  .toJson( QJsonDocument::Compact );
                        }
                        else if ( requestLine.startsWith( "POST /v1/reports" ) )
                        {
                                const QJsonObject o = QJsonDocument::fromJson( body ).object();
                                const QString challenge = o["pow"].toObject()["challenge"].toString();
                                const QString counter   = o["pow"].toObject()["counter"].toString();
                                const QByteArray digest = QCryptographicHash::hash( ( challenge + ":" + counter ).toUtf8(),
                                                                                    QCryptographicHash::Sha256 );

                                const bool valid = challenge == "mock-challenge" &&
                                                   model::BugReport::leadingZeroBits( digest ) >= challengeBits &&
                                                   o["schema"].toInt() == 1 &&
                                                   userAgent.startsWith( "glabels-neo/" );
                                if ( !valid )
                                {
                                        status  = 400;
                                        payload = R"({"ok":false,"error":"rejected"})";
                                }
                                else
                                {
                                        reports.append( o );
                                        status  = reportStatus;
                                        payload = ( status == 201 )
                                                ? QByteArray( R"({"ok":true,"number":123,"url":"https://github.com/o/r/issues/123"})" )
                                                : QByteArray( R"({"ok":false,"error":"limit"})" );
                                }
                        }

                        const QByteArray reply = "HTTP/1.1 " + QByteArray::number( status ) +
                                                 " X\r\nContent-Type: application/json\r\nContent-Length: " +
                                                 QByteArray::number( payload.size() ) + "\r\nConnection: close\r\n\r\n" + payload;
                        s->write( reply );
                        s->disconnectFromHost();
                }
        };


        QPushButton* buttonWithText( QWidget* w, const QString& text )
        {
                for ( QPushButton* b : w->findChildren<QPushButton*>() )
                {
                        if ( b->text().contains( text ) )
                        {
                                return b;
                        }
                }
                return nullptr;
        }


        QPlainTextEdit* editableText( QWidget* w )
        {
                for ( QPlainTextEdit* e : w->findChildren<QPlainTextEdit*>() )
                {
                        if ( !e->isReadOnly() )
                        {
                                return e;
                        }
                }
                return nullptr;
        }


        bool anyLabelContains( QWidget* w, const QString& text )
        {
                for ( QLabel* l : w->findChildren<QLabel*>() )
                {
                        if ( l->text().contains( text ) )
                        {
                                return true;
                        }
                }
                return false;
        }


        const QString DESCRIPTION = "It crashes when I press print on a two column sheet.";

}


void TestReportBugDialog::sendsAReportThroughTheRelay()
{
        MockRelay relay;
        QVERIFY( relay.listen( QHostAddress::LocalHost ) );

        ReportBugDialog dialog( nullptr, nullptr, QString( "http://127.0.0.1:%1" ).arg( relay.serverPort() ) );
        dialog.show();

        QPushButton*    send        = buttonWithText( &dialog, "Send report" );
        QPlainTextEdit* description = editableText( &dialog );
        QLineEdit*      summary     = dialog.findChild<QLineEdit*>();
        QVERIFY( send );
        QVERIFY( description );
        QVERIFY( summary );
        QVERIFY( send->isVisibleTo( &dialog ) );

        // Nothing can be sent until there is a description.
        QVERIFY( !send->isEnabled() );
        summary->setText( "Crash when printing" );
        description->setPlainText( DESCRIPTION );
        QVERIFY( send->isEnabled() );

        // Nothing has reached the relay before the button is pressed.
        QTest::qWait( 200 );
        QCOMPARE( relay.reports.size(), 0 );

        QTest::mouseClick( send, Qt::LeftButton );

        QTRY_COMPARE_WITH_TIMEOUT( relay.reports.size(), 1, 20000 );
        QTRY_VERIFY_WITH_TIMEOUT( anyLabelContains( &dialog, "#123" ), 10000 );

        const QJsonObject r = relay.reports[0];
        QCOMPARE( r["summary"].toString(), QString( "Crash when printing" ) );
        QCOMPARE( r["description"].toString(), DESCRIPTION );
        QVERIFY( !r["diagnostics"].toString().isEmpty() );
        QVERIFY( !r["version"].toString().isEmpty() );
        QCOMPARE( r["attachments"].toArray().size(), 0 );   // the label and the log are opt-in

        // done: it cannot be sent twice
        QVERIFY( !send->isEnabled() );
}


void TestReportBugDialog::saysSoWhenTheRelayIsDown()
{
        // nothing listens on port 1
        ReportBugDialog dialog( nullptr, nullptr, "http://127.0.0.1:1" );
        dialog.show();

        QPushButton* send = buttonWithText( &dialog, "Send report" );
        QVERIFY( send );
        editableText( &dialog )->setPlainText( DESCRIPTION );

        QTest::mouseClick( send, Qt::LeftButton );
        QTRY_VERIFY_WITH_TIMEOUT( anyLabelContains( &dialog, "Could not reach" ), 30000 );

        // the browser route stays available
        QPushButton* browser = buttonWithText( &dialog, "GitHub" );
        QVERIFY( browser );
        QVERIFY( browser->isEnabled() );
}


void TestReportBugDialog::refusesToSendOverPlainHttpToARemoteHost()
{
        ReportBugDialog dialog( nullptr, nullptr, "http://example.com" );

        QPushButton* send = buttonWithText( &dialog, "Send report" );
        QVERIFY( send );
        QVERIFY( send->isHidden() );   // only the browser option is offered
}


void TestReportBugDialog::showsTheRateLimitMessage()
{
        MockRelay relay;
        relay.reportStatus = 429;
        QVERIFY( relay.listen( QHostAddress::LocalHost ) );

        ReportBugDialog dialog( nullptr, nullptr, QString( "http://127.0.0.1:%1" ).arg( relay.serverPort() ) );
        dialog.show();

        editableText( &dialog )->setPlainText( DESCRIPTION );
        QTest::mouseClick( buttonWithText( &dialog, "Send report" ), Qt::LeftButton );

        QTRY_VERIFY_WITH_TIMEOUT( anyLabelContains( &dialog, "Too many reports" ), 20000 );
}
