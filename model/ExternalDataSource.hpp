//  ExternalDataSource.hpp
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

#ifndef model_ExternalDataSource_hpp
#define model_ExternalDataSource_hpp


#include <QMap>
#include <QString>


namespace glabels::model
{

        ///
        /// Kind of live external data source linked to a project.
        ///
        enum class ExternalSourceKind
        {
                None,
                Redash,
                GoogleSheets
        };


        ///
        /// Authentication mode for Redash queries.
        ///
        enum class RedashAuthMode
        {
                /// Account-level User API key: can trigger fresh query execution
                /// (POST /api/queries/{id}/results) and pass query parameters.
                UserApiKey,

                /// Per-query read-only Query API key: fetches the latest cached
                /// results (GET /api/queries/{id}/results.json).
                QueryApiKey
        };


        ///
        /// Result of parsing a pasted Redash query or results URL.
        ///
        struct ParsedRedashUrl
        {
                QString        serverUrl;
                int            queryId{ 0 };
                QString        apiKey;
                RedashAuthMode authMode{ RedashAuthMode::UserApiKey };
                bool           valid{ false };
        };


        ///
        /// Authentication mode for Google Sheets.
        ///
        enum class GoogleAuthMode
        {
                /// Signed in via Google OAuth 2.0 (Sheets API v4, works for private
                /// and Workspace spreadsheets).
                OAuth,

                /// No login required for spreadsheets shared as "Anyone with the link
                /// can view" (reads CSV export via gviz endpoint).
                PublicLink
        };


        ///
        /// Result of parsing a pasted Google Sheets URL or bare spreadsheet ID.
        ///
        struct ParsedGoogleSheetsUrl
        {
                QString spreadsheetId;
                QString gid;
                bool    valid{ false };
        };


        ///
        /// Non-secret configuration describing a live external data source
        /// stored inside a .glabels project.
        ///
        /// Credentials (API keys, OAuth refresh tokens) are NEVER stored here;
        /// they are looked up in the OS credential store via credentialKey().
        ///
        struct ExternalDataSource
        {
                ExternalSourceKind    kind{ ExternalSourceKind::None };

                // Redash fields
                QString               serverUrl;
                int                   queryId{ 0 };
                RedashAuthMode        redashAuthMode{ RedashAuthMode::UserApiKey };
                int                   maxAgeSeconds{ 0 };
                QMap<QString,QString> parameters;

                // Google Sheets fields
                QString               spreadsheetId;
                QString               sheetName;
                QString               sheetGid;
                QString               cellRange;
                GoogleAuthMode        googleAuthMode{ GoogleAuthMode::OAuth };
                bool                  hasHeaderRow{ true };

                // Common display & sync options
                QString               displayName;
                QString               matchKeyField;
                bool                  refreshOnOpen{ true };
                bool                  refreshBeforePrint{ true };

                /// Label field name -> source column name.  A field without an entry is matched by
                /// name (see ColumnMatcher).  An entry with an empty column means "leave blank".
                QMap<QString,QString> columnMap;

                bool isEmpty() const;
                bool isValid() const;

                QString credentialKey() const;
                QString summaryText() const;

                bool operator==( const ExternalDataSource& other ) const = default;

                static QString kindToIdString( ExternalSourceKind kind );
                static ExternalSourceKind kindFromIdString( const QString& id );

                static QString redashAuthModeToString( RedashAuthMode mode );
                static RedashAuthMode redashAuthModeFromString( const QString& value );

                static QString googleAuthModeToString( GoogleAuthMode mode );
                static GoogleAuthMode googleAuthModeFromString( const QString& value );

                static QString normalizeServerUrl( const QString& rawUrl );
                static bool isAllowedServerUrl( const QString& url );
                static ParsedRedashUrl parseRedashUrl( const QString& urlText );
                static ParsedGoogleSheetsUrl parseGoogleSheetsUrl( const QString& urlText );
        };

}


#endif // model_ExternalDataSource_hpp
