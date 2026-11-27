//  SecretStore.hpp
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

#ifndef datasource_SecretStore_hpp
#define datasource_SecretStore_hpp


#include <QString>


namespace glabels::datasource
{

        ///
        /// Cross-platform local credential store for external data source
        /// secrets (Redash API keys, Google OAuth refresh tokens).
        ///
        /// Backed by Windows Credential Manager on Windows, Keychain Services
        /// on macOS, and Secret Service (secret-tool) with a 0600 local file
        /// fallback on Linux.
        ///
        class SecretStore
        {
        public:
                static bool store( const QString& key, const QString& secret );
                static QString retrieve( const QString& key );
                static bool remove( const QString& key );
                static bool hasSecret( const QString& key );

                /// Human-readable description of the active storage backend.
                static QString backendDescription();

                /// Isolate unit tests from the host OS credential store.
                static void setMemoryModeForTests( bool enabled );
                static void clearMemoryStoreForTests();
        };

}


#endif // datasource_SecretStore_hpp
