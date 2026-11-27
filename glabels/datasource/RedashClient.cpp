//  RedashClient.cpp
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


#include "RedashClient.hpp"

#include "SecretStore.hpp"

#include "model/Version.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <cmath>


namespace glabels::datasource
{

        namespace
        {
                const int REQUEST_TIMEOUT_MS = 30000;

                QString jsonValueToCellString( const QJsonValue& v )
                {
                        if ( v.isNull() || v.isUndefined() )
                        {
                                return {};
                        }
                        if ( v.isString() )
                        {
                                return v.toString();
                        }
                        if ( v.isBool() )
                        {
                                return v.toBool() ? QStringLiteral( "true" ) : QStringLiteral( "false" );
                        }
                        if ( v.isDouble() )
                        {
                                const double d = v.toDouble();
                                if ( std::isfinite( d )
                                     && std::floor( d ) == d
                                     && std::abs( d ) < 9007199254740992.0 )
                                {
                                        return QString::number( static_cast<qint64>( d ) );
                                }
                                return QString::number( d, 'g', 15 );
                        }
                        if ( v.isArray() )
                        {
                                return QString::fromUtf8( QJsonDocument( v.toArray() ).toJson( QJsonDocument::Compact ) );
                        }
                        if ( v.isObject() )
                        {
                                return QString::fromUtf8( QJsonDocument( v.toObject() ).toJson( QJsonDocument::Compact ) );
                        }
                        return {};
                }

                QString extractRedashErrorMessage( const QByteArray& body )
                {
                        const QJsonDocument doc = QJsonDocument::fromJson( body );
                        if ( !doc.isObject() )
                        {
                                return {};
                        }
                        const QJsonObject obj = doc.object();
                        if ( obj.contains( QStringLiteral( "message" ) ) )
                        {
                                return obj.value( QStringLiteral( "message" ) ).toString().trimmed();
                        }
                        if ( obj.contains( QStringLiteral( "error" ) ) )
                        {
                                return obj.value( QStringLiteral( "error" ) ).toString().trimmed();
                        }
                        return {};
                }
        }


        RedashClient::RedashClient( QObject* parent )
                : ExternalDataFetcher( parent )
        {
                mPollTimer.setSingleShot( true );
        }


        RedashClient::~RedashClient()
        {
                cancel();
                if ( mInfoReply )
                {
                        mInfoReply->abort();
                        mInfoReply->deleteLater();
                }
        }


        void RedashClient::setPollIntervalMs( int ms )
        {
                mPollIntervalMs = qMax( 10, ms );
        }


        void RedashClient::setMaxPollTimeMs( int ms )
        {
                mMaxPollTimeMs = qMax( 100, ms );
        }


        bool RedashClient::isRunning() const
        {
                return mRunning;
        }


        void RedashClient::cancel()
        {
                ++mRequestGen;
                cleanupActiveRequest();
                mRunning = false;
        }


        void RedashClient::cleanupActiveRequest()
        {
                mPollTimer.stop();
                mPollTimer.disconnect( this );
                if ( mActiveReply )
                {
                        QNetworkReply* reply = mActiveReply;
                        mActiveReply = nullptr;
                        reply->disconnect( this );
                        reply->abort();
                        reply->deleteLater();
                }
                mActiveJobId.clear();
        }


        QNetworkRequest RedashClient::makeRequest( const QString& url, const QString& apiKey ) const
        {
                QNetworkRequest req{ QUrl( url ) };
                req.setHeader( QNetworkRequest::UserAgentHeader,
                               QStringLiteral( "glabels-neo/" ) + model::Version::STRING );
                req.setRawHeader( "Accept", "application/json" );
                if ( !apiKey.isEmpty() )
                {
                        req.setRawHeader( "Authorization", QByteArray( "Key " ) + apiKey.toUtf8() );
                }
                req.setTransferTimeout( REQUEST_TIMEOUT_MS );
                req.setAttribute( QNetworkRequest::RedirectPolicyAttribute,
                                  QNetworkRequest::NoLessSafeRedirectPolicy );
                return req;
        }


        void RedashClient::fetch( const model::ExternalDataSource& source,
                                  const QString&                   credentialOverride )
        {
                cancel();

                const quint64 gen = ++mRequestGen;
                mActiveSource = source;
                mActiveSource.serverUrl = model::ExternalDataSource::normalizeServerUrl( source.serverUrl );

                if ( mActiveSource.kind != model::ExternalSourceKind::Redash || !mActiveSource.isValid() )
                {
                        emit failed( tr( "Invalid Redash configuration. Check the server URL (HTTPS required) and query ID." ) );
                        return;
                }

                mActiveApiKey = credentialOverride.trimmed();
                if ( mActiveApiKey.isEmpty() )
                {
                        mActiveApiKey = SecretStore::retrieve( mActiveSource.credentialKey() ).trimmed();
                }

                if ( mActiveApiKey.isEmpty() )
                {
                        emit failed( tr( "No Redash API key is saved for this source. Open Configure to enter your API key." ) );
                        return;
                }

                mRunning = true;
                mPollStartedMs = QDateTime::currentMSecsSinceEpoch();
                emit started();

                const QString base = mActiveSource.serverUrl;
                const int qid = mActiveSource.queryId;

                if ( mActiveSource.redashAuthMode == model::RedashAuthMode::QueryApiKey )
                {
                        emit progressChanged( tr( "Fetching cached results for Redash query #%1..." ).arg( qid ) );
                        const QString url = QStringLiteral( "%1/api/queries/%2/results.json" ).arg( base ).arg( qid );
                        QNetworkReply* reply = mNet.get( makeRequest( url, mActiveApiKey ) );
                        mActiveReply = reply;
                        connect( reply, &QNetworkReply::finished, this, [this, reply, gen]() {
                                handleResultsReply( reply, gen );
                        } );
                }
                else
                {
                        emit progressChanged( tr( "Running Redash query #%1..." ).arg( qid ) );
                        const QString url = QStringLiteral( "%1/api/queries/%2/results" ).arg( base ).arg( qid );
                        QNetworkRequest req = makeRequest( url, mActiveApiKey );
                        req.setHeader( QNetworkRequest::ContentTypeHeader, QStringLiteral( "application/json" ) );

                        QJsonObject payload;
                        payload.insert( QStringLiteral( "max_age" ), qMax( 0, mActiveSource.maxAgeSeconds ) );
                        if ( !mActiveSource.parameters.isEmpty() )
                        {
                                QJsonObject params;
                                for ( auto it = mActiveSource.parameters.constBegin(); it != mActiveSource.parameters.constEnd(); ++it )
                                {
                                        params.insert( it.key(), it.value() );
                                }
                                payload.insert( QStringLiteral( "parameters" ), params );
                        }

                        QNetworkReply* reply = mNet.post( req, QJsonDocument( payload ).toJson( QJsonDocument::Compact ) );
                        mActiveReply = reply;
                        connect( reply, &QNetworkReply::finished, this, [this, reply, gen]() {
                                handleResultsReply( reply, gen );
                        } );
                }
        }


        void RedashClient::handleResultsReply( QNetworkReply* reply, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        reply->deleteLater();
                        return;
                }
                mActiveReply = nullptr;
                const QByteArray body = reply->readAll();
                const QNetworkReply::NetworkError netErr = reply->error();
                const QString httpErr = ( netErr != QNetworkReply::NoError ) ? formatHttpError( reply, body ) : QString();
                reply->deleteLater();

                if ( netErr != QNetworkReply::NoError )
                {
                        finishWithError( httpErr, gen );
                        return;
                }

                const QJsonDocument doc = QJsonDocument::fromJson( body );
                if ( !doc.isObject() )
                {
                        finishWithError( tr( "Redash returned an unexpected non-JSON response." ), gen );
                        return;
                }

                const QJsonObject root = doc.object();
                if ( root.contains( QStringLiteral( "query_result" ) ) )
                {
                        FetchResult result;
                        QString parseErr;
                        if ( !parseQueryResultJson( body, result, parseErr ) )
                        {
                                finishWithError( parseErr, gen );
                                return;
                        }
                        result.title = mActiveSource.displayName;
                        mRunning = false;
                        emit finished( result );
                        return;
                }

                if ( root.contains( QStringLiteral( "job" ) ) )
                {
                        const QJsonObject job = root.value( QStringLiteral( "job" ) ).toObject();
                        const int status = job.value( QStringLiteral( "status" ) ).toInt();
                        if ( status == 3 )
                        {
                                const qint64 resultId = job.value( QStringLiteral( "query_result_id" ) ).toInteger();
                                if ( resultId > 0 )
                                {
                                        fetchQueryResultById( resultId, gen );
                                        return;
                                }
                        }
                        else if ( status == 4 || status == 5 )
                        {
                                const QString jobErr = job.value( QStringLiteral( "error" ) ).toString().trimmed();
                                finishWithError( jobErr.isEmpty() ? tr( "Redash query execution failed." ) : jobErr, gen );
                                return;
                        }

                        const QString jobId = job.value( QStringLiteral( "id" ) ).toString().trimmed();
                        if ( jobId.isEmpty() )
                        {
                                finishWithError( tr( "Redash returned a job without a job ID." ), gen );
                                return;
                        }

                        emit progressChanged( tr( "Waiting for Redash query #%1..." ).arg( mActiveSource.queryId ) );
                        scheduleJobPoll( jobId, gen );
                        return;
                }

                const QString msg = extractRedashErrorMessage( body );
                finishWithError( msg.isEmpty()
                                         ? tr( "Redash response did not contain query results or a job." )
                                         : msg,
                                 gen );
        }


        void RedashClient::scheduleJobPoll( const QString& jobId, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        return;
                }
                const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - mPollStartedMs;
                if ( elapsed >= mMaxPollTimeMs )
                {
                        finishWithError( tr( "Timed out waiting for Redash query #%1 to finish." ).arg( mActiveSource.queryId ), gen );
                        return;
                }

                mActiveJobId = jobId;
                mPollTimer.disconnect( this );
                connect( &mPollTimer, &QTimer::timeout, this, [this, jobId, gen]() {
                        pollJob( jobId, gen );
                } );
                mPollTimer.start( mPollIntervalMs );
        }


        void RedashClient::pollJob( const QString& jobId, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        return;
                }

                const QString url = QStringLiteral( "%1/api/jobs/%2" )
                        .arg( mActiveSource.serverUrl, QString::fromLatin1( QUrl::toPercentEncoding( jobId ) ) );
                QNetworkReply* reply = mNet.get( makeRequest( url, mActiveApiKey ) );
                mActiveReply = reply;
                connect( reply, &QNetworkReply::finished, this, [this, reply, jobId, gen]() {
                        handleJobReply( reply, jobId, gen );
                } );
        }


        void RedashClient::handleJobReply( QNetworkReply* reply, const QString& jobId, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        reply->deleteLater();
                        return;
                }
                mActiveReply = nullptr;
                const QByteArray body = reply->readAll();
                const QNetworkReply::NetworkError netErr = reply->error();
                const QString httpErr = ( netErr != QNetworkReply::NoError ) ? formatHttpError( reply, body ) : QString();
                reply->deleteLater();

                if ( netErr != QNetworkReply::NoError )
                {
                        finishWithError( httpErr, gen );
                        return;
                }

                const QJsonDocument doc = QJsonDocument::fromJson( body );
                if ( !doc.isObject() || !doc.object().contains( QStringLiteral( "job" ) ) )
                {
                        finishWithError( tr( "Redash returned an unexpected job status payload." ), gen );
                        return;
                }

                const QJsonObject job = doc.object().value( QStringLiteral( "job" ) ).toObject();
                const int status = job.value( QStringLiteral( "status" ) ).toInt();

                if ( status == 1 || status == 2 )
                {
                        emit progressChanged( status == 2
                                              ? tr( "Executing Redash query #%1..." ).arg( mActiveSource.queryId )
                                              : tr( "Redash query #%1 is queued..." ).arg( mActiveSource.queryId ) );
                        scheduleJobPoll( jobId, gen );
                }
                else if ( status == 3 )
                {
                        const qint64 resultId = job.value( QStringLiteral( "query_result_id" ) ).toInteger();
                        if ( resultId <= 0 )
                        {
                                finishWithError( tr( "Redash job succeeded but did not provide a result ID." ), gen );
                                return;
                        }
                        emit progressChanged( tr( "Downloading Redash query results..." ) );
                        fetchQueryResultById( resultId, gen );
                }
                else
                {
                        const QString jobErr = job.value( QStringLiteral( "error" ) ).toString().trimmed();
                        finishWithError( jobErr.isEmpty()
                                                 ? tr( "Redash query execution failed (status %1)." ).arg( status )
                                                 : jobErr,
                                         gen );
                }
        }


        void RedashClient::fetchQueryResultById( qint64 queryResultId, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        return;
                }
                const QString url = QStringLiteral( "%1/api/query_results/%2.json" )
                        .arg( mActiveSource.serverUrl )
                        .arg( queryResultId );
                QNetworkReply* reply = mNet.get( makeRequest( url, mActiveApiKey ) );
                mActiveReply = reply;
                connect( reply, &QNetworkReply::finished, this, [this, reply, gen]() {
                        handleQueryResultByIdReply( reply, gen );
                } );
        }


        void RedashClient::handleQueryResultByIdReply( QNetworkReply* reply, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        reply->deleteLater();
                        return;
                }
                mActiveReply = nullptr;
                const QByteArray body = reply->readAll();
                const QNetworkReply::NetworkError netErr = reply->error();
                const QString httpErr = ( netErr != QNetworkReply::NoError ) ? formatHttpError( reply, body ) : QString();
                reply->deleteLater();

                if ( netErr != QNetworkReply::NoError )
                {
                        finishWithError( httpErr, gen );
                        return;
                }

                FetchResult result;
                QString parseErr;
                if ( !parseQueryResultJson( body, result, parseErr ) )
                {
                        finishWithError( parseErr, gen );
                        return;
                }
                result.title = mActiveSource.displayName;
                mRunning = false;
                emit finished( result );
        }


        void RedashClient::finishWithError( const QString& message, quint64 gen )
        {
                if ( gen != mRequestGen )
                {
                        return;
                }
                cleanupActiveRequest();
                mRunning = false;
                emit failed( message );
        }


        void RedashClient::fetchQueryInfo( const QString& serverUrl,
                                           int            queryId,
                                           const QString& apiKey )
        {
                if ( mInfoReply )
                {
                        mInfoReply->disconnect( this );
                        mInfoReply->abort();
                        mInfoReply->deleteLater();
                        mInfoReply = nullptr;
                }

                const QString norm = model::ExternalDataSource::normalizeServerUrl( serverUrl );
                if ( !model::ExternalDataSource::isAllowedServerUrl( norm ) || queryId <= 0 )
                {
                        emit queryInfoFailed( tr( "Enter a valid HTTPS Redash server URL and query ID." ) );
                        return;
                }

                const QString url = QStringLiteral( "%1/api/queries/%2" ).arg( norm ).arg( queryId );
                QNetworkReply* reply = mNet.get( makeRequest( url, apiKey.trimmed() ) );
                mInfoReply = reply;

                connect( reply, &QNetworkReply::finished, this, [this, reply, queryId]() {
                        if ( mInfoReply == reply )
                        {
                                mInfoReply = nullptr;
                        }
                        const QByteArray body = reply->readAll();
                        const QNetworkReply::NetworkError err = reply->error();
                        const QString errText = ( err != QNetworkReply::NoError ) ? formatHttpError( reply, body ) : QString();
                        reply->deleteLater();

                        if ( err != QNetworkReply::NoError )
                        {
                                emit queryInfoFailed( errText );
                                return;
                        }

                        const QJsonDocument doc = QJsonDocument::fromJson( body );
                        if ( !doc.isObject() )
                        {
                                emit queryInfoFailed( tr( "Redash returned an unexpected query metadata response." ) );
                                return;
                        }

                        const QJsonObject obj = doc.object();
                        RedashQueryInfo info;
                        info.queryId = obj.value( QStringLiteral( "id" ) ).toInt( queryId );
                        info.name = obj.value( QStringLiteral( "name" ) ).toString().trimmed();

                        const QJsonArray params = obj.value( QStringLiteral( "options" ) )
                                                     .toObject()
                                                     .value( QStringLiteral( "parameters" ) )
                                                     .toArray();
                        for ( const QJsonValue& pv : params )
                        {
                                const QJsonObject p = pv.toObject();
                                const QString pName = p.value( QStringLiteral( "name" ) ).toString().trimmed();
                                if ( !pName.isEmpty() )
                                {
                                        info.defaultParameters.insert(
                                                pName,
                                                jsonValueToCellString( p.value( QStringLiteral( "value" ) ) ) );
                                }
                        }

                        emit queryInfoLoaded( info );
                } );
        }


        bool RedashClient::parseQueryResultJson( const QByteArray& json,
                                                 FetchResult&      out,
                                                 QString&          errorOut )
        {
                const QJsonDocument doc = QJsonDocument::fromJson( json );
                if ( !doc.isObject() )
                {
                        errorOut = tr( "Invalid JSON in Redash query result." );
                        return false;
                }

                const QJsonObject root = doc.object();
                const QJsonObject qr = root.contains( QStringLiteral( "query_result" ) )
                        ? root.value( QStringLiteral( "query_result" ) ).toObject()
                        : root;

                const QJsonObject data = qr.value( QStringLiteral( "data" ) ).toObject();
                if ( data.isEmpty() || !data.contains( QStringLiteral( "rows" ) ) )
                {
                        errorOut = tr( "Redash response is missing query_result.data.rows." );
                        return false;
                }

                out = FetchResult{};
                out.fetchedAt = QDateTime::currentDateTime();

                const QJsonArray columns = data.value( QStringLiteral( "columns" ) ).toArray();
                for ( const QJsonValue& colVal : columns )
                {
                        const QJsonObject col = colVal.toObject();
                        QString colName = col.value( QStringLiteral( "name" ) ).toString();
                        if ( colName.isEmpty() )
                        {
                                colName = col.value( QStringLiteral( "friendly_name" ) ).toString();
                        }
                        if ( !colName.isEmpty() && !out.headers.contains( colName ) )
                        {
                                out.headers.append( colName );
                        }
                }

                const QJsonArray rows = data.value( QStringLiteral( "rows" ) ).toArray();
                out.rows.reserve( rows.size() );
                for ( const QJsonValue& rowVal : rows )
                {
                        const QJsonObject rowObj = rowVal.toObject();
                        QMap<QString,QString> rowMap;

                        if ( out.headers.isEmpty() )
                        {
                                // Fallback if a Redash proxy omitted the columns array.
                                const QStringList keys = rowObj.keys();
                                for ( const QString& k : keys )
                                {
                                        out.headers.append( k );
                                }
                        }

                        for ( const QString& col : out.headers )
                        {
                                rowMap.insert( col, jsonValueToCellString( rowObj.value( col ) ) );
                        }
                        out.rows.append( rowMap );
                }

                return true;
        }


        QString RedashClient::formatHttpError( QNetworkReply* reply, const QByteArray& body )
        {
                const int httpStatus = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
                const QString apiMsg = extractRedashErrorMessage( body );

                if ( httpStatus == 401 )
                {
                        return tr( "Redash rejected the API key (HTTP 401). Check that the key is valid and active." );
                }
                if ( httpStatus == 403 )
                {
                        return apiMsg.isEmpty()
                                ? tr( "Access denied by Redash (HTTP 403). Use a User API key if the query has parameters or requires fresh execution." )
                                : tr( "Access denied by Redash (HTTP 403): %1" ).arg( apiMsg );
                }
                if ( httpStatus == 404 )
                {
                        return apiMsg.isEmpty()
                                ? tr( "Redash query or endpoint not found (HTTP 404). Check the server URL and query ID." )
                                : tr( "Redash query not found (HTTP 404): %1" ).arg( apiMsg );
                }
                if ( httpStatus > 0 )
                {
                        return apiMsg.isEmpty()
                                ? tr( "Redash returned HTTP %1." ).arg( httpStatus )
                                : tr( "Redash returned HTTP %1: %2" ).arg( httpStatus ).arg( apiMsg );
                }
                return tr( "Could not reach Redash: %1" ).arg( reply->errorString() );
        }

}
