//  BugReport.cpp
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


#include "BugReport.hpp"

#include "Version.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrlQuery>


namespace glabels::model::BugReport
{

        namespace
        {
                const int MAX_URL_LENGTH = 6500;

                void replaceAll( QString& text, const QString& needle, const QString& with )
                {
                        if ( needle.length() >= 3 )
                        {
                                text.replace( needle, with, Qt::CaseInsensitive );
                        }
                }
        }


        QString redactPaths( const QString& text )
        {
                QString result = text;

                // Home directory first (it contains the user name), in both separator styles.
                const QString home = QDir::homePath();
                if ( home.length() >= 3 )
                {
                        replaceAll( result, home, "~" );
                        replaceAll( result, QDir::toNativeSeparators( home ), "~" );
                }

                for ( const char* var : { "USERNAME", "USER", "LOGNAME" } )
                {
                        replaceAll( result, qEnvironmentVariable( var ), "<user>" );
                }
                replaceAll( result, QSysInfo::machineHostName(), "<host>" );

                return result;
        }


        QString redact( const QString& text )
        {
                QString result = redactPaths( text );

                static const QRegularExpression email( R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})" );
                result.replace( email, "<email>" );

                static const QRegularExpression ipv4( R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)" );
                result.replace( ipv4, "<ip>" );

                return result;
        }


        QString diagnostics()
        {
                return redact( Version::details() );
        }


        int leadingZeroBits( const QByteArray& digest )
        {
                int n = 0;
                for ( char c : digest )
                {
                        const auto b = static_cast<unsigned char>( c );
                        if ( b == 0 )
                        {
                                n += 8;
                                continue;
                        }
                        for ( int bit = 7; bit >= 0 && !( b & ( 1u << bit ) ); bit-- )
                        {
                                n++;
                        }
                        break;
                }
                return n;
        }


        QString solveProofOfWork( const QString&               challenge,
                                  int                          bits,
                                  const std::function<bool()>& keepGoing,
                                  quint64                      maxIterations )
        {
                const QByteArray prefix = challenge.toUtf8() + ':';

                QCryptographicHash hash( QCryptographicHash::Sha256 );
                for ( quint64 counter = 0; counter < maxIterations; counter++ )
                {
                        if ( keepGoing && ( counter % 2048 == 0 ) && !keepGoing() )
                        {
                                return QString();
                        }

                        hash.reset();
                        hash.addData( prefix );
                        hash.addData( QByteArray::number( counter ) );
                        if ( leadingZeroBits( hash.result() ) >= bits )
                        {
                                return QString::number( counter );
                        }
                }
                return QString();
        }


        QByteArray toJson( const QString&           challenge,
                           const QString&           counter,
                           const QString&           summary,
                           const QString&           description,
                           const QString&           diagnosticsText,
                           const QList<Attachment>& attachments )
        {
                QJsonArray files;
                for ( const Attachment& a : attachments )
                {
                        files.append( QJsonObject{ { "kind", a.kind }, { "name", a.name }, { "text", a.text } } );
                }

                QJsonObject root{
                        { "schema",      1 },
                        { "pow",         QJsonObject{ { "challenge", challenge }, { "counter", counter } } },
                        { "summary",     summary },
                        { "description", description },
                        { "diagnostics", diagnosticsText },
                        { "version",     Version::STRING },
                        { "attachments", files } };

                return QJsonDocument( root ).toJson( QJsonDocument::Compact );
        }


        QUrl prefilledIssueUrl( const QString& summary, const QString& description )
        {
                const QUrl base( Version::BUG_WEBSITE + "/new" );

                auto build = [&]( const QString& what )
                {
                        QUrlQuery query;
                        query.addQueryItem( "template", "bug_report.yml" );
                        query.addQueryItem( "title",    "[Bug]: " + summary );
                        query.addQueryItem( "version",  Version::STRING );
                        query.addQueryItem( "os",       redact( QSysInfo::prettyProductName() ) );
                        query.addQueryItem( "what",     what );
                        QUrl url = base;
                        url.setQuery( query );
                        return url;
                };

                QString what = description;
                QUrl url = build( what );
                while ( url.toEncoded().length() > MAX_URL_LENGTH && what.length() > 20 )
                {
                        what.truncate( what.length() * 3 / 4 );
                        url = build( what + QStringLiteral( " [truncated]" ) );
                }
                return url;
        }

}
