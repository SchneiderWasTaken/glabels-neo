//  RedashClient.hpp
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

#ifndef datasource_RedashClient_hpp
#define datasource_RedashClient_hpp


#include "ExternalDataFetcher.hpp"

#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>


class QNetworkReply;


namespace glabels::datasource
{

        ///
        /// Metadata discovered from GET /api/queries/{id}.
        ///
        struct RedashQueryInfo
        {
                int                   queryId{ 0 };
                QString               name;
                QMap<QString,QString> defaultParameters;
        };


        ///
        /// Asynchronous Redash API client.
        ///
        /// Supports:
        ///  - User API key: POST /api/queries/{id}/results with max_age + parameters,
        ///    polling GET /api/jobs/{jobId} until completion, then fetching
        ///    GET /api/query_results/{resultId}.json.
        ///  - Query API key: GET /api/queries/{id}/results.json for latest cached results.
        ///
        class RedashClient : public ExternalDataFetcher
        {
                Q_OBJECT

        public:
                explicit RedashClient( QObject* parent = nullptr );
                ~RedashClient() override;

                void fetch( const model::ExternalDataSource& source,
                            const QString&                   credentialOverride = QString() ) override;
                void cancel() override;
                bool isRunning() const override;

                /// Fetch query title and parameter names (User API key).
                void fetchQueryInfo( const QString& serverUrl,
                                     int            queryId,
                                     const QString& apiKey );

                /// Tune polling interval and timeouts (primarily for tests).
                void setPollIntervalMs( int ms );
                void setMaxPollTimeMs( int ms );

                /// Parse a Redash query_result JSON payload into headers + rows.
                static bool parseQueryResultJson( const QByteArray& json,
                                                  FetchResult&      out,
                                                  QString&          errorOut );

        signals:
                void queryInfoLoaded( const glabels::datasource::RedashQueryInfo& info );
                void queryInfoFailed( const QString& errorMessage );

        private:
                QNetworkRequest makeRequest( const QString& url, const QString& apiKey ) const;
                void handleResultsReply( QNetworkReply* reply, quint64 gen );
                void scheduleJobPoll( const QString& jobId, quint64 gen );
                void pollJob( const QString& jobId, quint64 gen );
                void handleJobReply( QNetworkReply* reply, const QString& jobId, quint64 gen );
                void fetchQueryResultById( qint64 queryResultId, quint64 gen );
                void handleQueryResultByIdReply( QNetworkReply* reply, quint64 gen );
                void finishWithError( const QString& message, quint64 gen );
                void cleanupActiveRequest();

                static QString formatHttpError( QNetworkReply* reply, const QByteArray& body );

        private:
                QNetworkAccessManager   mNet;
                QPointer<QNetworkReply> mActiveReply;
                QPointer<QNetworkReply> mInfoReply;
                QTimer                  mPollTimer;

                model::ExternalDataSource mActiveSource;
                QString                   mActiveApiKey;
                QString                   mActiveJobId;
                qint64                    mPollStartedMs{ 0 };
                int                       mPollIntervalMs{ 500 };
                int                       mMaxPollTimeMs{ 60000 };
                quint64                   mRequestGen{ 0 };
                bool                      mRunning{ false };
        };

}


#endif // datasource_RedashClient_hpp
