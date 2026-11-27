//  ExternalDataSource.cpp
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


#include "ExternalDataSource.hpp"

#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>


namespace glabels::model
{

        bool ExternalDataSource::isEmpty() const
        {
                return kind == ExternalSourceKind::None;
        }


        bool ExternalDataSource::isValid() const
        {
                switch ( kind )
                {
                case ExternalSourceKind::Redash:
                        return isAllowedServerUrl( normalizeServerUrl( serverUrl ) ) && queryId > 0;

                case ExternalSourceKind::GoogleSheets:
                        return !spreadsheetId.trimmed().isEmpty();

                case ExternalSourceKind::None:
                default:
                        return false;
                }
        }


        QString ExternalDataSource::credentialKey() const
        {
                if ( kind == ExternalSourceKind::Redash )
                {
                        const QString norm = normalizeServerUrl( serverUrl );
                        if ( norm.isEmpty() )
                        {
                                return {};
                        }
                        if ( redashAuthMode == RedashAuthMode::QueryApiKey && queryId > 0 )
                        {
                                return QStringLiteral( "redash:query:%1:%2" ).arg( norm ).arg( queryId );
                        }
                        return QStringLiteral( "redash:user:%1" ).arg( norm );
                }
                if ( kind == ExternalSourceKind::GoogleSheets )
                {
                        return ( googleAuthMode == GoogleAuthMode::OAuth )
                                ? QStringLiteral( "google:oauth:refresh_token" )
                                : QString();
                }
                return {};
        }


        QString ExternalDataSource::summaryText() const
        {
                if ( kind == ExternalSourceKind::Redash )
                {
                        const QUrl parsed( normalizeServerUrl( serverUrl ) );
                        const QString host = parsed.host().isEmpty() ? serverUrl : parsed.host();
                        if ( !displayName.trimmed().isEmpty() )
                        {
                                return QStringLiteral( "Redash #%1 · %2 (%3)" )
                                        .arg( queryId )
                                        .arg( displayName.trimmed(), host );
                        }
                        return QStringLiteral( "Redash #%1 (%2)" ).arg( queryId ).arg( host );
                }
                if ( kind == ExternalSourceKind::GoogleSheets )
                {
                        const QString title = !displayName.trimmed().isEmpty()
                                ? displayName.trimmed()
                                : ( spreadsheetId.length() > 16
                                            ? spreadsheetId.left( 14 ) + QStringLiteral( "..." )
                                            : spreadsheetId );
                        QString out = QStringLiteral( "Google Sheets · %1" ).arg( title );
                        if ( !sheetName.trimmed().isEmpty() )
                        {
                                out += QStringLiteral( " (%1)" ).arg( sheetName.trimmed() );
                        }
                        if ( googleAuthMode == GoogleAuthMode::PublicLink )
                        {
                                out += QStringLiteral( " · Public" );
                        }
                        return out;
                }
                return {};
        }


        QString ExternalDataSource::kindToIdString( ExternalSourceKind kind )
        {
                switch ( kind )
                {
                case ExternalSourceKind::Redash:
                        return QStringLiteral( "redash" );
                case ExternalSourceKind::GoogleSheets:
                        return QStringLiteral( "google_sheets" );
                case ExternalSourceKind::None:
                default:
                        return QStringLiteral( "none" );
                }
        }


        ExternalSourceKind ExternalDataSource::kindFromIdString( const QString& id )
        {
                const QString lower = id.trimmed().toLower();
                if ( lower == QStringLiteral( "redash" ) )
                {
                        return ExternalSourceKind::Redash;
                }
                if ( lower == QStringLiteral( "google_sheets" ) )
                {
                        return ExternalSourceKind::GoogleSheets;
                }
                return ExternalSourceKind::None;
        }


        QString ExternalDataSource::redashAuthModeToString( RedashAuthMode mode )
        {
                return ( mode == RedashAuthMode::QueryApiKey )
                        ? QStringLiteral( "query_key" )
                        : QStringLiteral( "user_key" );
        }


        RedashAuthMode ExternalDataSource::redashAuthModeFromString( const QString& value )
        {
                if ( value.trimmed().toLower() == QStringLiteral( "query_key" ) )
                {
                        return RedashAuthMode::QueryApiKey;
                }
                return RedashAuthMode::UserApiKey;
        }


        QString ExternalDataSource::googleAuthModeToString( GoogleAuthMode mode )
        {
                return ( mode == GoogleAuthMode::PublicLink )
                        ? QStringLiteral( "public" )
                        : QStringLiteral( "oauth" );
        }


        GoogleAuthMode ExternalDataSource::googleAuthModeFromString( const QString& value )
        {
                if ( value.trimmed().toLower() == QStringLiteral( "public" ) )
                {
                        return GoogleAuthMode::PublicLink;
                }
                return GoogleAuthMode::OAuth;
        }


        QString ExternalDataSource::normalizeServerUrl( const QString& rawUrl )
        {
                QString cleaned = rawUrl.trimmed();
                if ( cleaned.isEmpty() )
                {
                        return {};
                }

                // Strip query/fragment if a user pasted a full URL into the server field.
                QUrl url( cleaned );
                if ( !url.isValid() || url.scheme().isEmpty() || url.host().isEmpty() )
                {
                        return {};
                }

                url.setQuery( QString() );
                url.setFragment( QString() );
                url.setUserInfo( QString() );

                QString out = url.toString( QUrl::FullyEncoded );
                while ( out.endsWith( '/' ) )
                {
                        out.chop( 1 );
                }
                return out;
        }


        bool ExternalDataSource::isAllowedServerUrl( const QString& urlText )
        {
                const QUrl url( urlText.trimmed() );
                if ( !url.isValid() || url.host().isEmpty() )
                {
                        return false;
                }

                const QString scheme = url.scheme().toLower();
                if ( scheme == QStringLiteral( "https" ) )
                {
                        return true;
                }

                if ( scheme == QStringLiteral( "http" ) )
                {
                        const QString host = url.host().toLower();
                        return host == QStringLiteral( "localhost" )
                                || host == QStringLiteral( "127.0.0.1" )
                                || host == QStringLiteral( "::1" );
                }

                return false;
        }


        ParsedRedashUrl ExternalDataSource::parseRedashUrl( const QString& urlText )
        {
                ParsedRedashUrl result;
                const QUrl url( urlText.trimmed() );
                if ( !url.isValid() || url.scheme().isEmpty() || url.host().isEmpty() )
                {
                        return result;
                }

                const QString path = url.path();
                // Match either:
                //   <prefix>/api/queries/<id>(/results(.json|.csv)?)?
                //   <prefix>/queries/<id>(/source)?
                static const QRegularExpression apiRx(
                        QStringLiteral( R"(^(.*)/api/queries/(\d+)(?:/results(?:\.(?:json|csv))?)?/?$)" ) );
                static const QRegularExpression webRx(
                        QStringLiteral( R"(^(.*)/queries/(\d+)(?:/.*)?$)" ) );

                QString prefix;
                int id = 0;
                bool isResultsUrl = false;

                if ( const auto m = apiRx.match( path ); m.hasMatch() )
                {
                        prefix = m.captured( 1 );
                        id = m.captured( 2 ).toInt();
                        isResultsUrl = path.contains( QStringLiteral( "/results" ) );
                }
                else if ( const auto m = webRx.match( path ); m.hasMatch() )
                {
                        prefix = m.captured( 1 );
                        id = m.captured( 2 ).toInt();
                }
                else
                {
                        return result;
                }

                if ( id <= 0 )
                {
                        return result;
                }

                QUrl server;
                server.setScheme( url.scheme() );
                server.setHost( url.host() );
                if ( url.port() != -1 )
                {
                        server.setPort( url.port() );
                }
                server.setPath( prefix );

                const QString normServer = normalizeServerUrl( server.toString() );
                if ( !isAllowedServerUrl( normServer ) )
                {
                        return result;
                }

                const QUrlQuery query( url );
                const QString key = query.queryItemValue( QStringLiteral( "api_key" ), QUrl::FullyDecoded ).trimmed();

                result.serverUrl = normServer;
                result.queryId = id;
                result.apiKey = key;
                result.authMode = ( isResultsUrl && !key.isEmpty() )
                        ? RedashAuthMode::QueryApiKey
                        : RedashAuthMode::UserApiKey;
                result.valid = true;
                return result;
        }


        ParsedGoogleSheetsUrl ExternalDataSource::parseGoogleSheetsUrl( const QString& urlText )
        {
                ParsedGoogleSheetsUrl result;
                const QString trimmed = urlText.trimmed();
                if ( trimmed.isEmpty() )
                {
                        return result;
                }

                // Standard Google Sheets URL:
                //   https://docs.google.com/spreadsheets/d/<id>/edit#gid=<gid>
                //   https://docs.google.com/spreadsheets/u/0/d/<id>/edit?gid=<gid>#gid=<gid>
                static const QRegularExpression urlRx(
                        QStringLiteral( R"(https?://docs\.google\.com/spreadsheets/(?:u/\d+/)?d/([A-Za-z0-9_-]{10,}))" ),
                        QRegularExpression::CaseInsensitiveOption );
                if ( const auto m = urlRx.match( trimmed ); m.hasMatch() )
                {
                        result.spreadsheetId = m.captured( 1 );
                        const QUrl url( trimmed );
                        const QUrlQuery query( url );
                        QString gid = query.queryItemValue( QStringLiteral( "gid" ) ).trimmed();
                        if ( gid.isEmpty() )
                        {
                                static const QRegularExpression fragGidRx( QStringLiteral( R"((?:^|&)gid=(\d+))" ) );
                                if ( const auto fm = fragGidRx.match( url.fragment() ); fm.hasMatch() )
                                {
                                        gid = fm.captured( 1 );
                                }
                        }
                        result.gid = gid;
                        result.valid = true;
                        return result;
                }

                // Bare spreadsheet ID (alphanumeric, underscore, hyphen, >= 20 chars)
                static const QRegularExpression bareIdRx( QStringLiteral( R"(^[A-Za-z0-9_-]{20,}$)" ) );
                if ( bareIdRx.match( trimmed ).hasMatch() )
                {
                        result.spreadsheetId = trimmed;
                        result.valid = true;
                }
                return result;
        }

}
