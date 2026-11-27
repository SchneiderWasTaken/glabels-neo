//  GoogleSheetsClient.hpp
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

#ifndef datasource_GoogleSheetsClient_hpp
#define datasource_GoogleSheetsClient_hpp


#include "ExternalDataFetcher.hpp"

#include <QList>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>


class QNetworkReply;


namespace glabels::datasource
{

        class GoogleAuthManager;


        struct GoogleSheetTab
        {
                qint64  sheetId{ 0 };
                QString title;
        };


        struct GoogleSpreadsheetInfo
        {
                QString               spreadsheetId;
                QString               title;
                QList<GoogleSheetTab> tabs;
        };


        ///
        /// Asynchronous Google Sheets fetcher supporting:
        ///  - OAuth 2.0 mode via Google Sheets API v4 (private & Workspace sheets)
        ///  - Public Link mode via Google Sheets CSV export (no login required for
        ///    sheets shared as "Anyone with the link can view")
        ///
        class GoogleSheetsClient : public ExternalDataFetcher
        {
                Q_OBJECT

        public:
                explicit GoogleSheetsClient( GoogleAuthManager* auth = nullptr,
                                             QObject*           parent = nullptr );
                ~GoogleSheetsClient() override;

                void fetch( const model::ExternalDataSource& source,
                            const QString&                   credentialOverride = QString() ) override;
                void cancel() override;
                bool isRunning() const override;

                /// Load spreadsheet title and tab names (requires Google Sign-In).
                void fetchSpreadsheetInfo( const QString& spreadsheetId );

                /// Endpoint overrides for unit tests.
                void setApiBaseUrlsForTests( const QString& sheetsApiBase,
                                             const QString& publicSheetsBase );

                /// Pure parsers (also tested directly).
                static bool parseSheetsValuesJson( const QByteArray& json,
                                                   bool              hasHeaderRow,
                                                   FetchResult&      out,
                                                   QString&          errorOut );
                static bool parseCsvData( const QByteArray& csv,
                                          bool              hasHeaderRow,
                                          FetchResult&      out,
                                          QString&          errorOut );

        signals:
                void spreadsheetInfoLoaded( const glabels::datasource::GoogleSpreadsheetInfo& info );
                void spreadsheetInfoFailed( const QString& errorMessage );

        private:
                void fetchViaOAuth( quint64 gen, bool retriedAfter401 );
                void fetchViaPublicCsv( quint64 gen );
                void startValuesRequest( const QString& accessToken,
                                         const QString& resolvedRange,
                                         quint64        gen,
                                         bool           retriedAfter401 );
                void fetchFirstSheetNameThenValues( const QString& accessToken,
                                                    quint64        gen,
                                                    bool           retriedAfter401 );
                void cleanupActiveReply();
                void finishWithError( const QString& message, quint64 gen );

                QNetworkRequest makeRequest( const QString& url, const QString& bearerToken = QString() ) const;
                static QString buildA1Range( const QString& sheetName, const QString& cellRange );
                static QString formatGoogleHttpError( QNetworkReply* reply, const QByteArray& body );

        private:
                GoogleAuthManager*        mAuth{ nullptr };
                QNetworkAccessManager     mNet;
                QPointer<QNetworkReply>   mActiveReply;
                QPointer<QNetworkReply>   mInfoReply;

                model::ExternalDataSource mActiveSource;
                QString                   mTokenOverride;
                QString                   mSheetsApiBase;
                QString                   mPublicSheetsBase;
                quint64                   mRequestGen{ 0 };
                bool                      mRunning{ false };
        };

}


#endif // datasource_GoogleSheetsClient_hpp
