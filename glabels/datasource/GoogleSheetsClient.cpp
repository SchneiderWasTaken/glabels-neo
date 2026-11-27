//  GoogleSheetsClient.cpp
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


#include "GoogleSheetsClient.hpp"

#include "GoogleAuthManager.hpp"

#include "model/Version.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>


namespace glabels::datasource
{

        namespace
        {
                const int REQUEST_TIMEOUT_MS = 30000;

                QString extractGoogleApiError( const QByteArray& body )
                {
                        const QJsonDocument doc = QJsonDocument::fromJson( body );
                        if ( !doc.isObject() )
                        {
                                return {};
                        }
                        const QJsonObject err = doc.object().value( QStringLiteral( "error" ) ).toObject();
                        return err.value( QStringLiteral( "message" ) ).toString().trimmed();
                }

                // Full RFC-4180 CSV parser supporting quoted fields with embedded newlines and "".
                QList<QStringList> parseRfc4180Csv( const QString& text )
                {
                        QList<QStringList> rows;
                        QStringList currentRow;
                        QString cell;
                        bool inQuotes = false;
                        bool anyContentInRow = false;

                        const qsizetype n = text.size();
                        for ( qsizetype i = 0; i < n; ++i )
                        {
                                const QChar c = text.at( i );
                                if ( inQuotes )
                                {
                                        if ( c == '"' )
                                        {
                                                if ( i + 1 < n && text.at( i + 1 ) == '"' )
                                                {
                                                        cell.append( '"' );
                                                        ++i;
                                                }
                                                else
                                                {
                                                        inQuotes = false;
                                                }
                                        }
                                        else
                                        {
                                                cell.append( c );
                                        }
                                }
                                else
                                {
                                        if ( c == '"' && cell.isEmpty() )
                                        {
                                                inQuotes = true;
                                                anyContentInRow = true;
                                        }
                                        else if ( c == ',' )
                                        {
                                                currentRow.append( cell );
                                                cell.clear();
                                                anyContentInRow = true;
                                        }
                                        else if ( c == '\r' || c == '\n' )
                                        {
                                                if ( c == '\r' && i + 1 < n && text.at( i + 1 ) == '\n' )
                                                {
                                                        ++i;
                                                }
                                                if ( anyContentInRow || !cell.isEmpty() )
                                                {
                                                        currentRow.append( cell );
                                                        rows.append( currentRow );
                                                }
                                                currentRow.clear();
                                                cell.clear();
                                                anyContentInRow = false;
                                        }
                                        else
                                        {
                                                cell.append( c );
                                                anyContentInRow = true;
                                        }
                                }
                        }

                        if ( anyContentInRow || !cell.isEmpty() )
                        {
                                currentRow.append( cell );
                                rows.append( currentRow );
                        }

                        return rows;
                }

                void populateFetchResultFromGrid( const QList<QStringList>& grid,
                                                  bool                      hasHeaderRow,
                                                  FetchResult&              out )
                {
                        out = FetchResult{};
                        out.fetchedAt = QDateTime::currentDateTime();

                        if ( grid.isEmpty() )
                        {
                                return;
                        }

                        int maxCols = 0;
                        for ( const QStringList& r : grid )
                        {
                                maxCols = qMax( maxCols, static_cast<int>( r.size() ) );
                        }
                        if ( maxCols == 0 )
                        {
                                return;
                        }

                        int startRow = 0;
                        if ( hasHeaderRow )
                        {
                                const QStringList& headerRow = grid.first();
                                for ( int c = 0; c < maxCols; ++c )
                                {
                                        QString name = ( c < headerRow.size() ) ? headerRow.at( c ).trimmed() : QString();
                                        if ( name.isEmpty() )
                                        {
                                                name = QString::number( c + 1 );
                                        }
                                        out.headers.append( name );
                                }
                                startRow = 1;
                        }
                        else
                        {
                                for ( int c = 0; c < maxCols; ++c )
                                {
                                        out.headers.append( QString::number( c + 1 ) );
                                }
                        }

                        for ( int r = startRow; r < grid.size(); ++r )
                        {
                                const QStringList& cells = grid.at( r );
                                bool allEmpty = true;
                                QMap<QString,QString> rowMap;
                                for ( int c = 0; c < out.headers.size(); ++c )
                                {
                                        const QString val = ( c < cells.size() ) ? cells.at( c ) : QString();
                                        if ( !val.isEmpty() )
                                        {
                                                allEmpty = false;
                                        }
                                        rowMap.insert( out.headers.at( c ), val );
                                }
                                if ( !allEmpty )
                                {
                                        out.rows.append( rowMap );
                                }
                        }
                }
        }


        GoogleSheetsClient::GoogleSheetsClient( GoogleAuthManager* auth, QObject* parent )
                : ExternalDataFetcher( parent ),
                  mAuth( auth ? auth : GoogleAuthManager::instance() ),
                  mSheetsApiBase( QStringLiteral( "https://sheets.googleapis.com" ) ),
                  mPublicSheetsBase( QStringLiteral( "https://docs.google.com" ) )
        {
        }


        GoogleSheetsClient::~GoogleSheetsClient()
        {
                cancel();
                if ( mInfoReply )
                {
                        mInfoReply->abort();
                        mInfoReply->deleteLater();
                }
        }


        void GoogleSheetsClient::setApiBaseUrlsForTests( const QString& sheetsApiBase,
                                                         const QString& publicSheetsBase )
        {
                mSheetsApiBase = sheetsApiBase;
                mPublicSheetsBase = publicSheetsBase;
        }


        bool GoogleSheetsClient::isRunning() const
        {
                return mRunning;
        }


        void GoogleSheetsClient::cancel()
        {
                ++mRequestGen;
                cleanupActiveReply();
                mRunning = false;
        }


        void GoogleSheetsClient::cleanupActiveReply()
        {
                if ( mActiveReply )
                {
                        QNetworkReply* reply = mActiveReply;
                        mActiveReply = nullptr;
                        reply->disconnect( this );
                        reply->abort();
                        reply->deleteLater();
                }
        }


        QNetworkRequest GoogleSheetsClient::makeRequest( const QString& url, const QString& bearerToken ) const
        {
                QNetworkRequest req{ QUrl( url ) };
                req.setHeader( QNetworkRequest::UserAgentHeader,
                               QStringLiteral( "glabels-neo/" ) + model::Version::STRING );
                if ( !bearerToken.isEmpty() )
                {
                        req.setRawHeader( "Authorization", QByteArray( "Bearer " ) + bearerToken.toUtf8() );
                }
                req.setTransferTimeout( REQUEST_TIMEOUT_MS );
                req.setAttribute( QNetworkRequest::RedirectPolicyAttribute,
                                  QNetworkRequest::NoLessSafeRedirectPolicy );
                return req;
        }


        QString GoogleSheetsClient::buildA1Range( const QString& sheetName, const QString& cellRange )
        {
                const QString tab = sheetName.trimmed();
                const QString range = cellRange.trimmed();
                if ( tab.isEmpty() )
                {
                        return range;
                }
                // Quote sheet names containing spaces or special characters per A1 notation.
                QString quotedTab = tab;
                quotedTab.replace( '\'', QStringLiteral( "''" ) );
                const QString prefix = QStringLiteral( "'%1'" ).arg( quotedTab );
                return range.isEmpty() ? prefix : ( prefix + QLatin1Char( '!' ) + range );
        }


        void GoogleSheetsClient::fetch( const model::ExternalDataSource& source,
                                        const QString&                   credentialOverride )
        {
                cancel();

                const quint64 gen = ++mRequestGen;
                mActiveSource = source;
                mTokenOverride = credentialOverride.trimmed();

                if ( mActiveSource.kind != model::ExternalSourceKind::GoogleSheets || !mActiveSource.isValid() )
                {
                        emit failed( tr( "Invalid Google Sheets configuration. Provide a spreadsheet URL or ID." ) );
                        return;
                }

                mRunning = true;
                emit started();

                if ( mActiveSource.googleAuthMode == model::GoogleAuthMode::PublicLink )
                {
                        fetchViaPublicCsv( gen );
                }
                else
                {
                        fetchViaOAuth( gen, false );
                }
        }


        void GoogleSheetsClient::fetchViaOAuth( quint64 gen, bool retriedAfter401 )
        {
                auto proceed = [this, gen, retriedAfter401]( const QString& token, const QString& error ) {
                        if ( gen != mRequestGen )
                        {
                                return;
                        }
                        if ( !error.isEmpty() || token.isEmpty() )
                        {
                                finishWithError( error.isEmpty()
                                                         ? tr( "Not signed in to Google." )
                                                         : error,
                                                 gen );
                                return;
                        }

                        if ( mActiveSource.sheetName.trimmed().isEmpty() )
                        {
                                fetchFirstSheetNameThenValues( token, gen, retriedAfter401 );
                        }
                        else
                        {
                                const QString a1 = buildA1Range( mActiveSource.sheetName, mActiveSource.cellRange );
                                startValuesRequest( token, a1, gen, retriedAfter401 );
                        }
                };

                if ( !mTokenOverride.isEmpty() )
                {
                        proceed( mTokenOverride, QString() );
                        return;
                }

                emit progressChanged( tr( "Authenticating with Google..." ) );
                mAuth->ensureAccessToken( proceed );
        }


        void GoogleSheetsClient::fetchFirstSheetNameThenValues( const QString& accessToken,
                                                                quint64        gen,
                                                                bool           retriedAfter401 )
        {
                emit progressChanged( tr( "Reading Google Sheet metadata..." ) );
                const QString id = QString::fromLatin1( QUrl::toPercentEncoding( mActiveSource.spreadsheetId.trimmed() ) );
                const QString url = QStringLiteral( "%1/v4/spreadsheets/%2?fields=properties.title,sheets.properties(sheetId,title)" )
                        .arg( mSheetsApiBase, id );

                QNetworkReply* reply = mNet.get( makeRequest( url, accessToken ) );
                mActiveReply = reply;

                connect( reply, &QNetworkReply::finished, this, [this, reply, accessToken, gen, retriedAfter401]() {
                        if ( gen != mRequestGen )
                        {
                                reply->deleteLater();
                                return;
                        }
                        mActiveReply = nullptr;
                        const QByteArray body = reply->readAll();
                        const int httpStatus = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
                        const QNetworkReply::NetworkError netErr = reply->error();
                        const QString httpErr = ( netErr != QNetworkReply::NoError ) ? formatGoogleHttpError( reply, body ) : QString();
                        reply->deleteLater();

                        if ( httpStatus == 401 && !retriedAfter401 && mTokenOverride.isEmpty() )
                        {
                                mAuth->invalidateAccessToken();
                                fetchViaOAuth( gen, true );
                                return;
                        }
                        if ( netErr != QNetworkReply::NoError )
                        {
                                finishWithError( httpErr, gen );
                                return;
                        }

                        const QJsonObject root = QJsonDocument::fromJson( body ).object();
                        const QString docTitle = root.value( QStringLiteral( "properties" ) ).toObject()
                                                     .value( QStringLiteral( "title" ) ).toString().trimmed();
                        if ( mActiveSource.displayName.trimmed().isEmpty() && !docTitle.isEmpty() )
                        {
                                mActiveSource.displayName = docTitle;
                        }

                        QString chosenTab;
                        const QJsonArray sheets = root.value( QStringLiteral( "sheets" ) ).toArray();
                        for ( const QJsonValue& sv : sheets )
                        {
                                const QJsonObject props = sv.toObject().value( QStringLiteral( "properties" ) ).toObject();
                                const QString title = props.value( QStringLiteral( "title" ) ).toString();
                                const QString sid = QString::number( props.value( QStringLiteral( "sheetId" ) ).toInteger() );
                                if ( chosenTab.isEmpty() )
                                {
                                        chosenTab = title;
                                }
                                if ( !mActiveSource.sheetGid.trimmed().isEmpty() && sid == mActiveSource.sheetGid.trimmed() )
                                {
                                        chosenTab = title;
                                        break;
                                }
                        }

                        if ( chosenTab.isEmpty() )
                        {
                                finishWithError( tr( "This Google Spreadsheet has no worksheets." ), gen );
                                return;
                        }

                        const QString a1 = buildA1Range( chosenTab, mActiveSource.cellRange );
                        startValuesRequest( accessToken, a1, gen, retriedAfter401 );
                } );
        }


        void GoogleSheetsClient::startValuesRequest( const QString& accessToken,
                                                     const QString& resolvedRange,
                                                     quint64        gen,
                                                     bool           retriedAfter401 )
        {
                emit progressChanged( tr( "Loading rows from Google Sheets..." ) );
                const QString id = QString::fromLatin1( QUrl::toPercentEncoding( mActiveSource.spreadsheetId.trimmed() ) );
                const QString encRange = QString::fromLatin1( QUrl::toPercentEncoding( resolvedRange ) );
                const QString url = QStringLiteral( "%1/v4/spreadsheets/%2/values/%3?valueRenderOption=FORMATTED_VALUE" )
                        .arg( mSheetsApiBase, id, encRange );

                QNetworkReply* reply = mNet.get( makeRequest( url, accessToken ) );
                mActiveReply = reply;

                connect( reply, &QNetworkReply::finished, this, [this, reply, gen, retriedAfter401]() {
                        if ( gen != mRequestGen )
                        {
                                reply->deleteLater();
                                return;
                        }
                        mActiveReply = nullptr;
                        const QByteArray body = reply->readAll();
                        const int httpStatus = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
                        const QNetworkReply::NetworkError netErr = reply->error();
                        const QString httpErr = ( netErr != QNetworkReply::NoError ) ? formatGoogleHttpError( reply, body ) : QString();
                        reply->deleteLater();

                        if ( httpStatus == 401 && !retriedAfter401 && mTokenOverride.isEmpty() )
                        {
                                mAuth->invalidateAccessToken();
                                fetchViaOAuth( gen, true );
                                return;
                        }
                        if ( netErr != QNetworkReply::NoError )
                        {
                                finishWithError( httpErr, gen );
                                return;
                        }

                        FetchResult result;
                        QString parseErr;
                        if ( !parseSheetsValuesJson( body, mActiveSource.hasHeaderRow, result, parseErr ) )
                        {
                                finishWithError( parseErr, gen );
                                return;
                        }
                        result.title = mActiveSource.displayName;
                        mRunning = false;
                        emit finished( result );
                } );
        }


        void GoogleSheetsClient::fetchViaPublicCsv( quint64 gen )
        {
                emit progressChanged( tr( "Downloading public Google Sheet..." ) );
                const QString id = QString::fromLatin1( QUrl::toPercentEncoding( mActiveSource.spreadsheetId.trimmed() ) );
                QUrl url( QStringLiteral( "%1/spreadsheets/d/%2/gviz/tq" ).arg( mPublicSheetsBase, id ) );
                QUrlQuery q;
                q.addQueryItem( QStringLiteral( "tqx" ), QStringLiteral( "out:csv" ) );
                if ( !mActiveSource.sheetName.trimmed().isEmpty() )
                {
                        q.addQueryItem( QStringLiteral( "sheet" ), mActiveSource.sheetName.trimmed() );
                }
                else if ( !mActiveSource.sheetGid.trimmed().isEmpty() )
                {
                        q.addQueryItem( QStringLiteral( "gid" ), mActiveSource.sheetGid.trimmed() );
                }
                if ( !mActiveSource.cellRange.trimmed().isEmpty() )
                {
                        q.addQueryItem( QStringLiteral( "range" ), mActiveSource.cellRange.trimmed() );
                }
                url.setQuery( q );

                QNetworkReply* reply = mNet.get( makeRequest( url.toString() ) );
                mActiveReply = reply;

                connect( reply, &QNetworkReply::finished, this, [this, reply, gen]() {
                        if ( gen != mRequestGen )
                        {
                                reply->deleteLater();
                                return;
                        }
                        mActiveReply = nullptr;
                        const QByteArray body = reply->readAll();
                        const QString contentType = reply->header( QNetworkRequest::ContentTypeHeader ).toString().toLower();
                        const QNetworkReply::NetworkError netErr = reply->error();
                        const QString httpErr = ( netErr != QNetworkReply::NoError ) ? formatGoogleHttpError( reply, body ) : QString();
                        reply->deleteLater();

                        if ( netErr != QNetworkReply::NoError )
                        {
                                finishWithError( httpErr, gen );
                                return;
                        }

                        // Google redirects private sheets to an HTML sign-in page with HTTP 200.
                        const QByteArray trimmedStart = body.trimmed().left( 64 ).toLower();
                        if ( contentType.contains( QStringLiteral( "text/html" ) )
                             || trimmedStart.startsWith( "<!doctype html" )
                             || trimmedStart.startsWith( "<html" ) )
                        {
                                finishWithError(
                                        tr( "This Google Sheet is private. Sign in with Google to link it, "
                                            "or share it as \"Anyone with the link can view\"." ),
                                        gen );
                                return;
                        }

                        FetchResult result;
                        QString parseErr;
                        if ( !parseCsvData( body, mActiveSource.hasHeaderRow, result, parseErr ) )
                        {
                                finishWithError( parseErr, gen );
                                return;
                        }
                        result.title = mActiveSource.displayName;
                        mRunning = false;
                        emit finished( result );
                } );
        }


        void GoogleSheetsClient::fetchSpreadsheetInfo( const QString& spreadsheetId )
        {
                if ( mInfoReply )
                {
                        mInfoReply->disconnect( this );
                        mInfoReply->abort();
                        mInfoReply->deleteLater();
                        mInfoReply = nullptr;
                }

                const QString cleanId = spreadsheetId.trimmed();
                if ( cleanId.isEmpty() )
                {
                        emit spreadsheetInfoFailed( tr( "Enter a Google Sheet URL or ID first." ) );
                        return;
                }

                mAuth->ensureAccessToken( [this, cleanId]( const QString& token, const QString& error ) {
                        if ( !error.isEmpty() || token.isEmpty() )
                        {
                                emit spreadsheetInfoFailed( error.isEmpty() ? tr( "Sign in with Google first." ) : error );
                                return;
                        }

                        const QString encId = QString::fromLatin1( QUrl::toPercentEncoding( cleanId ) );
                        const QString url = QStringLiteral( "%1/v4/spreadsheets/%2?fields=properties.title,sheets.properties(sheetId,title)" )
                                .arg( mSheetsApiBase, encId );

                        QNetworkReply* reply = mNet.get( makeRequest( url, token ) );
                        mInfoReply = reply;

                        connect( reply, &QNetworkReply::finished, this, [this, reply, cleanId]() {
                                if ( mInfoReply == reply )
                                {
                                        mInfoReply = nullptr;
                                }
                                const QByteArray body = reply->readAll();
                                const QNetworkReply::NetworkError err = reply->error();
                                const QString errMsg = ( err != QNetworkReply::NoError ) ? formatGoogleHttpError( reply, body ) : QString();
                                reply->deleteLater();

                                if ( err != QNetworkReply::NoError )
                                {
                                        emit spreadsheetInfoFailed( errMsg );
                                        return;
                                }

                                const QJsonDocument doc = QJsonDocument::fromJson( body );
                                if ( !doc.isObject() )
                                {
                                        emit spreadsheetInfoFailed( tr( "Google Sheets returned an unexpected metadata response." ) );
                                        return;
                                }

                                const QJsonObject root = doc.object();
                                GoogleSpreadsheetInfo info;
                                info.spreadsheetId = cleanId;
                                info.title = root.value( QStringLiteral( "properties" ) ).toObject()
                                                 .value( QStringLiteral( "title" ) ).toString().trimmed();

                                const QJsonArray sheets = root.value( QStringLiteral( "sheets" ) ).toArray();
                                for ( const QJsonValue& sv : sheets )
                                {
                                        const QJsonObject props = sv.toObject().value( QStringLiteral( "properties" ) ).toObject();
                                        GoogleSheetTab tab;
                                        tab.sheetId = props.value( QStringLiteral( "sheetId" ) ).toInteger();
                                        tab.title = props.value( QStringLiteral( "title" ) ).toString();
                                        if ( !tab.title.isEmpty() )
                                        {
                                                info.tabs.append( tab );
                                        }
                                }

                                emit spreadsheetInfoLoaded( info );
                        } );
                } );
        }


        void GoogleSheetsClient::finishWithError( const QString& message, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        return;
                }
                cleanupActiveReply();
                mRunning = false;
                emit failed( message );
        }


        bool GoogleSheetsClient::parseSheetsValuesJson( const QByteArray& json,
                                                        bool              hasHeaderRow,
                                                        FetchResult&      out,
                                                        QString&          errorOut )
        {
                const QJsonDocument doc = QJsonDocument::fromJson( json );
                if ( !doc.isObject() )
                {
                        errorOut = tr( "Invalid JSON returned by Google Sheets API." );
                        return false;
                }

                const QJsonObject root = doc.object();
                if ( root.contains( QStringLiteral( "error" ) ) )
                {
                        const QString msg = extractGoogleApiError( json );
                        errorOut = msg.isEmpty() ? tr( "Google Sheets API returned an error." ) : msg;
                        return false;
                }

                QList<QStringList> grid;
                const QJsonArray values = root.value( QStringLiteral( "values" ) ).toArray();
                grid.reserve( values.size() );
                for ( const QJsonValue& rowVal : values )
                {
                        const QJsonArray rowArr = rowVal.toArray();
                        QStringList cells;
                        cells.reserve( rowArr.size() );
                        for ( const QJsonValue& cellVal : rowArr )
                        {
                                cells.append( cellVal.toVariant().toString() );
                        }
                        grid.append( cells );
                }

                populateFetchResultFromGrid( grid, hasHeaderRow, out );
                return true;
        }


        bool GoogleSheetsClient::parseCsvData( const QByteArray& csv,
                                               bool              hasHeaderRow,
                                               FetchResult&      out,
                                               QString&          errorOut )
        {
                Q_UNUSED( errorOut );
                QString text = QString::fromUtf8( csv );
                if ( text.startsWith( QChar( 0xFEFF ) ) )
                {
                        text.remove( 0, 1 );
                }
                const QList<QStringList> grid = parseRfc4180Csv( text );
                populateFetchResultFromGrid( grid, hasHeaderRow, out );
                return true;
        }


        QString GoogleSheetsClient::formatGoogleHttpError( QNetworkReply* reply, const QByteArray& body )
        {
                const int status = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
                const QString apiMsg = extractGoogleApiError( body );

                if ( status == 401 )
                {
                        return tr( "Google sign-in expired or unauthorized (HTTP 401). Please sign in again." );
                }
                if ( status == 403 )
                {
                        return apiMsg.isEmpty()
                                ? tr( "Permission denied by Google Sheets (HTTP 403). Use \"Pick a Google Sheet...\" to grant gLabels Neo access to this spreadsheet." )
                                : tr( "Permission denied by Google Sheets (HTTP 403): %1" ).arg( apiMsg );
                }
                if ( status == 404 )
                {
                        return tr( "Google Sheet not found (HTTP 404). Check the spreadsheet URL/ID, or use \"Pick a Google Sheet...\" if it is private." );
                }
                if ( status > 0 )
                {
                        return apiMsg.isEmpty()
                                ? tr( "Google Sheets returned HTTP %1." ).arg( status )
                                : tr( "Google Sheets returned HTTP %1: %2" ).arg( status ).arg( apiMsg );
                }
                return tr( "Could not reach Google Sheets: %1" ).arg( reply->errorString() );
        }

}
