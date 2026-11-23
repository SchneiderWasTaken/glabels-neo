//  UpdateManifest.cpp
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

#include "UpdateManifest.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVersionNumber>


namespace glabels::update
{

        namespace
        {

                std::optional<UpdateManifest> fail( QString* error, const QString& message )
                {
                        if ( error )
                        {
                                *error = message;
                        }
                        return std::nullopt;
                }


                // "v1.2.0" -> 1.2.0 ; anything that does not start with a number (after an optional 'v') -> null
                QVersionNumber parseTag( const QString& tag )
                {
                        QString s = tag.trimmed();
                        if ( s.startsWith( 'v' ) || s.startsWith( 'V' ) )
                        {
                                s = s.mid( 1 );
                        }
                        if ( s.isEmpty() || !s[0].isDigit() )
                        {
                                return QVersionNumber();
                        }
                        return QVersionNumber::fromString( s );
                }


                bool isReleaseAssetUrl( const QUrl& url, const QString& repo, const QString& tag, const QString& name )
                {
                        if ( url.scheme() != "https" || url.host() != "github.com" || url.port() != -1 ||
                             !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() )
                        {
                                return false;
                        }

                        // /<owner>/<name>/releases/download/<tag>/<file>, exactly, compared as sent
                        QStringList want = repo.split( '/' );
                        want << "releases" << "download" << tag << name;

                        const QString path = url.path( QUrl::FullyEncoded );
                        if ( !path.startsWith( '/' ) )
                        {
                                return false;
                        }
                        const QStringList have = path.mid( 1 ).split( '/' );
                        if ( have != want )
                        {
                                return false;
                        }
                        for ( const QString& segment : have )
                        {
                                if ( segment.isEmpty() || segment == "." || segment == ".." || segment.contains( '%' ) )
                                {
                                        return false;
                                }
                        }
                        return true;
                }


                QString shortCommit( const QString& commit )
                {
                        return commit.left( 7 );
                }

        }


        std::optional<UpdateManifest> UpdateManifest::parse( const QByteArray& json,
                                                             const QString&    repo,
                                                             const QString&    expectedChannel,
                                                             bool              strictUrls,
                                                             QString*          error )
        {
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson( json, &parseError );
                if ( doc.isNull() || !doc.isObject() )
                {
                        return fail( error, "not a JSON object: " + parseError.errorString() );
                }
                QJsonObject root = doc.object();

                if ( root.value( "schema" ).toInt( -1 ) != SCHEMA )
                {
                        return fail( error, "unsupported manifest schema" );
                }

                UpdateManifest m;
                m.channel = root.value( "channel" ).toString();
                if ( m.channel != "stable" && m.channel != "edge" )
                {
                        return fail( error, "unknown channel '" + m.channel + "'" );
                }
                if ( m.channel != expectedChannel )
                {
                        return fail( error, "manifest is for channel '" + m.channel + "', expected '" + expectedChannel + "'" );
                }

                m.version = root.value( "version" ).toString();
                m.commit  = root.value( "commit" ).toString().toLower();
                if ( m.version.isEmpty() )
                {
                        return fail( error, "missing version" );
                }
                static const QRegularExpression commitRe( "^[0-9a-f]{40}$" );
                if ( !commitRe.match( m.commit ).hasMatch() )
                {
                        return fail( error, "missing or malformed commit" );
                }
                // The version is shown to the user, so it must look like one (no markup, no free text).
                static const QRegularExpression tagRe( "^v?[0-9]+(\\.[0-9]+){1,3}$" );
                if ( m.channel == "stable" && !tagRe.match( m.version ).hasMatch() )
                {
                        return fail( error, "stable release tag is not a version number" );
                }
                if ( m.channel == "edge" && m.version != "edge" )
                {
                        return fail( error, "an edge manifest must describe the 'edge' release" );
                }

                m.commitTime = static_cast<qint64>( root.value( "commit_time" ).toDouble( 0 ) );
                if ( m.channel == "edge" && m.commitTime <= 0 )
                {
                        return fail( error, "missing commit_time" );   // edge releases are ordered by it
                }

                m.published = QDateTime::fromString( root.value( "published" ).toString(), Qt::ISODate );
                m.notesUrl  = QUrl( root.value( "notes_url" ).toString() );
                if ( !m.notesUrl.isEmpty() && ( m.notesUrl.scheme() != "https" || m.notesUrl.host() != "github.com" ) )
                {
                        m.notesUrl.clear(); // only ever offer links that stay on GitHub
                }

                static const QRegularExpression sha256Re( "^[0-9a-f]{64}$" );
                static const QRegularExpression nameRe( "^[A-Za-z0-9._-]+$" );

                QJsonObject assets = root.value( "assets" ).toObject();
                for ( auto it = assets.begin(); it != assets.end(); ++it )
                {
                        QJsonObject o = it.value().toObject();

                        UpdateAsset a;
                        a.name   = o.value( "name" ).toString();
                        a.url    = QUrl( o.value( "url" ).toString() );
                        a.sha256 = o.value( "sha256" ).toString().toLower();
                        a.size   = static_cast<qint64>( o.value( "size" ).toDouble( 0 ) );

                        if ( !nameRe.match( a.name ).hasMatch() || a.name.startsWith( '.' ) )
                        {
                                return fail( error, "bad asset name for " + it.key() );
                        }
                        if ( !sha256Re.match( a.sha256 ).hasMatch() )
                        {
                                return fail( error, "bad digest for " + it.key() );
                        }
                        if ( a.size <= 0 || a.size > Q_INT64_C(2) * 1024 * 1024 * 1024 )
                        {
                                return fail( error, "bad size for " + it.key() );
                        }
                        // Downloads may only come from this project's release assets on github.com:
                        //   https://github.com/<repo>/releases/download/<this release's tag>/<file name>
                        // and nothing else (no port, credentials, query, fragment, dot segments or escapes).
                        if ( strictUrls && !isReleaseAssetUrl( a.url, repo, m.version, a.name ) )
                        {
                                return fail( error, "download URL for " + it.key() + " is not a release asset of " + repo );
                        }

                        m.assets.insert( it.key(), a );
                }

                if ( m.assets.isEmpty() )
                {
                        return fail( error, "no assets" );
                }

                return m;
        }


        const UpdateAsset* UpdateManifest::assetFor( const QString& platformKey ) const
        {
                auto it = assets.constFind( platformKey );
                return it == assets.constEnd() ? nullptr : &it.value();
        }


        QString UpdateManifest::id() const
        {
                return channel == "stable" ? version : commit;
        }


        QString UpdateManifest::displayVersion() const
        {
                return channel == "stable" ? version : "edge " + shortCommit( commit );
        }


        QString BuildIdentity::id() const
        {
                return channel == "stable" ? tag : commit;
        }


        QString BuildIdentity::displayVersion() const
        {
                return channel == "stable" ? tag : "edge " + shortCommit( commit );
        }


        int compareTags( const QString& a, const QString& b )
        {
                QVersionNumber va = parseTag( a );
                QVersionNumber vb = parseTag( b );
                if ( va.isNull() || vb.isNull() )
                {
                        return 0;
                }
                return QVersionNumber::compare( va, vb );
        }


        bool isUpdateAvailable( const BuildIdentity& current, const UpdateManifest& manifest )
        {
                if ( current.channel.isEmpty() || current.channel != manifest.channel )
                {
                        return false;
                }

                if ( current.channel == "stable" )
                {
                        return compareTags( manifest.version, current.tag ) > 0;
                }

                // edge: a different commit that is strictly newer than this build's
                return !current.commit.isEmpty() && manifest.commit != current.commit.toLower() &&
                       manifest.commitTime > current.commitTime;
        }

}
