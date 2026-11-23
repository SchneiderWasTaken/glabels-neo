//  UpdateManifest.hpp
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

#ifndef update_UpdateManifest_hpp
#define update_UpdateManifest_hpp


#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QString>
#include <QUrl>

#include <optional>


namespace glabels::update
{

        ///
        /// One downloadable package of a release
        ///
        struct UpdateAsset
        {
                QString name;       ///< file name, e.g. "glabels-neo-windows-x64.zip"
                QUrl    url;        ///< https download location
                QString sha256;     ///< lowercase hex digest of the file
                qint64  size{0};    ///< bytes
        };


        ///
        /// What the release pipeline publishes for the updater (the "update manifest").
        ///
        /// The release workflow attaches a small JSON file to every release (stable and edge).  It names
        /// the release, the commit it was built from and, per platform, the package with its SHA-256.
        /// The application fetches it from a fixed URL -- no GitHub API, no rate limit, no parsing of
        /// GitHub's own schema.
        ///
        struct UpdateManifest
        {
                static constexpr int SCHEMA = 1;

                QString                     channel;    ///< "stable" or "edge"
                QString                     version;    ///< release tag ("v1.3.0") or "edge"
                QString                     commit;     ///< full commit hash the packages were built from
                QDateTime                   published;
                QUrl                        notesUrl;   ///< human readable release page
                QMap<QString,UpdateAsset>   assets;     ///< keyed by platform, e.g. "windows-x86_64"

                /// Parse and validate.  Returns nullopt and sets *error if the document is unusable:
                /// unknown schema, wrong channel, malformed digest, or a download URL that does not
                /// point at a release asset of `repo` on github.com over https.
                static std::optional<UpdateManifest> parse( const QByteArray& json,
                                                            const QString&    repo,
                                                            const QString&    expectedChannel,
                                                            bool              strictUrls = true,
                                                            QString*          error = nullptr );

                /// Package for a platform key (see Platform::key()), or nullptr.
                const UpdateAsset* assetFor( const QString& platformKey ) const;

                /// Identifies this release for "skip this version": the tag for stable, the commit for edge.
                QString id() const;

                /// Name to show the user: the tag for stable, a short commit for edge.
                QString displayVersion() const;
        };


        ///
        /// Description of the running build, for comparison with a manifest
        ///
        struct BuildIdentity
        {
                QString channel;    ///< "stable", "edge" or "" (self-update disabled)
                QString tag;        ///< release tag of this build ("v1.2.0"), if any
                QString commit;     ///< full commit hash of this build, if known

                /// Same notion of identity as UpdateManifest::id()
                QString id() const;
                QString displayVersion() const;
        };


        /// True if `manifest` describes a release this build should offer to install.
        ///
        ///  - stable: the manifest's tag is a strictly higher version number than this build's tag.
        ///  - edge:   the manifest was built from a different commit than this build.  (Edge only ever
        ///            moves forward, and an edge build compares itself to nothing else.)
        bool isUpdateAvailable( const BuildIdentity& current, const UpdateManifest& manifest );


        /// Compare release tags like "v1.2.0" / "1.10.3".  Returns <0, 0, >0; 0 if either is unparsable.
        int compareTags( const QString& a, const QString& b );

}


#endif // update_UpdateManifest_hpp
