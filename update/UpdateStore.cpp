//  UpdateStore.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//

#include "UpdateStore.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>


namespace glabels::update
{

        namespace UpdateStore
        {

                QString dir()
                {
                        return QStandardPaths::writableLocation( QStandardPaths::AppLocalDataLocation ) + "/updates";
                }

                QString downloadDir() { return dir() + "/download"; }
                QString runDir()      { return dir() + "/run"; }
                QString pendingFile() { return dir() + "/pending.json"; }
                QString resultFile()  { return dir() + "/last-apply.json"; }
                QString lockDir()     { return dir() + "/apply.lock"; }
                QString logFile()     { return dir() + "/update.log"; }


                void log( const QString& message )
                {
                        QDir().mkpath( dir() );

                        QFile f( logFile() );
                        if ( f.size() > 200 * 1024 )
                        {
                                f.remove();
                        }
                        if ( f.open( QIODevice::Append | QIODevice::Text ) )
                        {
                                QTextStream( &f ) << QDateTime::currentDateTime().toString( "yyyy-MM-dd HH:mm:ss" )
                                                  << " [app] " << message << "\n";
                        }
                }

        }


        std::optional<PendingUpdate> PendingUpdate::load()
        {
                QFile f( UpdateStore::pendingFile() );
                if ( !f.open( QIODevice::ReadOnly ) )
                {
                        return std::nullopt;
                }

                QJsonObject o = QJsonDocument::fromJson( f.readAll() ).object();
                PendingUpdate p;
                p.id       = o.value( "id" ).toString();
                p.version  = o.value( "version" ).toString();
                p.channel  = o.value( "channel" ).toString();
                p.sha256   = o.value( "sha256" ).toString();
                p.commitTime = static_cast<qint64>( o.value( "commitTime" ).toDouble( 0 ) );
                p.stagedAt = QDateTime::fromString( o.value( "stagedAt" ).toString(), Qt::ISODate );
                p.attempts = o.value( "attempts" ).toInt( 0 );

                if ( p.id.isEmpty() )
                {
                        return std::nullopt;
                }
                return p;
        }


        bool PendingUpdate::save() const
        {
                QDir().mkpath( UpdateStore::dir() );

                QJsonObject o;
                o["id"]       = id;
                o["version"]  = version;
                o["channel"]  = channel;
                o["sha256"]   = sha256;
                o["commitTime"] = commitTime;
                o["stagedAt"] = stagedAt.toString( Qt::ISODate );
                o["attempts"] = attempts;

                QSaveFile f( UpdateStore::pendingFile() );
                if ( !f.open( QIODevice::WriteOnly ) )
                {
                        return false;
                }
                f.write( QJsonDocument( o ).toJson() );
                return f.commit();
        }


        void PendingUpdate::clear()
        {
                QFile::remove( UpdateStore::pendingFile() );
        }


        namespace UpdateSettings
        {

                namespace
                {
                        // Same store as the rest of the application's settings (organization and
                        // application name are set in main()), own group.
                        template <typename T>
                        T get( const QString& key, const T& fallback )
                        {
                                QSettings s;
                                s.beginGroup( "Updates" );
                                return s.value( key, QVariant::fromValue( fallback ) ).template value<T>();
                        }

                        void set( const QString& key, const QVariant& value )
                        {
                                QSettings s;
                                s.beginGroup( "Updates" );
                                s.setValue( key, value );
                        }
                }

                // Background checks contact github.com, so they are off until the user has said yes.
                bool autoCheck()                    { return get<bool>( "autoCheck", false ); }
                void setAutoCheck( bool enabled )   { set( "autoCheck", enabled ); set( "consentAsked", true ); }

                bool consentAsked()                 { return get<bool>( "consentAsked", false ); }
                void setConsentAsked( bool asked )  { set( "consentAsked", asked ); }

                bool autoInstall()                  { return get<bool>( "autoInstall", false ); }
                void setAutoInstall( bool enabled ) { set( "autoInstall", enabled ); }

                QDateTime lastCheck()                       { return get<QDateTime>( "lastCheck", QDateTime() ); }
                void setLastCheck( const QDateTime& when )  { set( "lastCheck", when ); }

                QString skippedId()                 { return get<QString>( "skippedId", QString() ); }
                void setSkippedId( const QString& id ) { set( "skippedId", id ); }

                QString failedId()                  { return get<QString>( "failedId", QString() ); }
                void setFailedId( const QString& id ) { set( "failedId", id ); }

                void snooze( const QString& id, const QDateTime& until )
                {
                        set( "snoozedId", id );
                        set( "snoozedUntil", until );
                }

                bool isSnoozed( const QString& id )
                {
                        return get<QString>( "snoozedId", QString() ) == id &&
                               get<QDateTime>( "snoozedUntil", QDateTime() ) > QDateTime::currentDateTime();
                }

        }

}
