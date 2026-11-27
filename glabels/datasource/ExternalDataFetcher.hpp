//  ExternalDataFetcher.hpp
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

#ifndef datasource_ExternalDataFetcher_hpp
#define datasource_ExternalDataFetcher_hpp


#include "model/ExternalDataSource.hpp"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>


namespace glabels::datasource
{

        ///
        /// Tabular result fetched from an external data source (Redash, Google Sheets, ...).
        ///
        struct FetchResult
        {
                QStringList                  headers;
                QList<QMap<QString,QString>> rows;
                QString                      title;
                QDateTime                    fetchedAt;
        };


        ///
        /// Base interface for asynchronous external data source fetchers.
        ///
        class ExternalDataFetcher : public QObject
        {
                Q_OBJECT

        public:
                explicit ExternalDataFetcher( QObject* parent = nullptr )
                        : QObject( parent )
                {
                }
                ~ExternalDataFetcher() override = default;

                virtual void fetch( const model::ExternalDataSource& source,
                                    const QString&                   credentialOverride = QString() ) = 0;
                virtual void cancel() = 0;
                virtual bool isRunning() const = 0;

        signals:
                void started();
                void progressChanged( const QString& statusMessage );
                void finished( const glabels::datasource::FetchResult& result );
                void failed( const QString& errorMessage );
        };

}


#endif // datasource_ExternalDataFetcher_hpp
