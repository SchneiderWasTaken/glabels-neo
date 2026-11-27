//  SecretStore.cpp
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


#include "SecretStore.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincred.h>
#elif defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif


namespace glabels::datasource
{

        namespace
        {
                QMutex                gMutex;
                bool                  gMemoryMode{ false };
                QMap<QString,QString> gMemoryStore;

#if defined(Q_OS_WIN)
                std::wstring winTargetName( const QString& key )
                {
                        return ( QStringLiteral( "gLabelsNeo/" ) + key ).toStdWString();
                }
#elif defined(Q_OS_MACOS)
                struct CfDeleter
                {
                        void operator()( CFTypeRef ref ) const
                        {
                                if ( ref )
                                {
                                        CFRelease( ref );
                                }
                        }
                };

                CFStringRef toCfString( const QString& s )
                {
                        const QByteArray utf8 = s.toUtf8();
                        return CFStringCreateWithBytes(
                                kCFAllocatorDefault,
                                reinterpret_cast<const UInt8*>( utf8.constData() ),
                                utf8.size(),
                                kCFStringEncodingUTF8,
                                false );
                }
#else
                QString fallbackFilePath()
                {
                        const QString dirPath = QStandardPaths::writableLocation( QStandardPaths::AppDataLocation );
                        if ( dirPath.isEmpty() )
                        {
                                return {};
                        }
                        QDir().mkpath( dirPath );
                        return dirPath + QStringLiteral( "/credentials.json" );
                }

                QJsonObject loadFallbackJson()
                {
                        const QString path = fallbackFilePath();
                        if ( path.isEmpty() )
                        {
                                return {};
                        }
                        QFile f( path );
                        if ( !f.open( QIODevice::ReadOnly ) )
                        {
                                return {};
                        }
                        return QJsonDocument::fromJson( f.readAll() ).object();
                }

                bool saveFallbackJson( const QJsonObject& obj )
                {
                        const QString path = fallbackFilePath();
                        if ( path.isEmpty() )
                        {
                                return false;
                        }
                        QSaveFile f( path );
                        if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
                        {
                                return false;
                        }
                        f.setPermissions( QFileDevice::ReadOwner | QFileDevice::WriteOwner );
                        const QByteArray payload = QJsonDocument( obj ).toJson( QJsonDocument::Compact );
                        if ( f.write( payload ) != payload.size() )
                        {
                                f.cancelWriting();
                                return false;
                        }
                        if ( !f.commit() )
                        {
                                return false;
                        }
                        QFile::setPermissions( path, QFileDevice::ReadOwner | QFileDevice::WriteOwner );
                        return true;
                }

                bool hasSecretTool()
                {
                        return !QStandardPaths::findExecutable( QStringLiteral( "secret-tool" ) ).isEmpty();
                }
#endif
        }


        void SecretStore::setMemoryModeForTests( bool enabled )
        {
                QMutexLocker lock( &gMutex );
                gMemoryMode = enabled;
                if ( !enabled )
                {
                        gMemoryStore.clear();
                }
        }


        void SecretStore::clearMemoryStoreForTests()
        {
                QMutexLocker lock( &gMutex );
                gMemoryStore.clear();
        }


        bool SecretStore::store( const QString& key, const QString& secret )
        {
                if ( key.trimmed().isEmpty() )
                {
                        return false;
                }

                {
                        QMutexLocker lock( &gMutex );
                        if ( gMemoryMode )
                        {
                                gMemoryStore.insert( key, secret );
                                return true;
                        }
                }

#if defined(Q_OS_WIN)
                std::wstring target = winTargetName( key );
                std::wstring user = L"gLabelsNeo";
                QByteArray blob = secret.toUtf8();

                CREDENTIALW cred{};
                cred.Type = CRED_TYPE_GENERIC;
                cred.TargetName = target.data();
                cred.CredentialBlobSize = static_cast<DWORD>( blob.size() );
                cred.CredentialBlob = reinterpret_cast<LPBYTE>( blob.data() );
                cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
                cred.UserName = user.data();

                return CredWriteW( &cred, 0 ) == TRUE;

#elif defined(Q_OS_MACOS)
                CFStringRef service = CFSTR( "io.github.schneiderwastaken.glabels-neo" );
                CFStringRef account = toCfString( key );
                const QByteArray secretUtf8 = secret.toUtf8();
                CFDataRef valueData = CFDataCreate(
                        kCFAllocatorDefault,
                        reinterpret_cast<const UInt8*>( secretUtf8.constData() ),
                        secretUtf8.size() );

                const void* queryKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
                const void* queryValues[] = { kSecClassGenericPassword, service, account };
                CFDictionaryRef query = CFDictionaryCreate(
                        kCFAllocatorDefault, queryKeys, queryValues, 3,
                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

                const void* updateKeys[] = { kSecValueData };
                const void* updateValues[] = { valueData };
                CFDictionaryRef attrsToUpdate = CFDictionaryCreate(
                        kCFAllocatorDefault, updateKeys, updateValues, 1,
                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

                OSStatus status = SecItemUpdate( query, attrsToUpdate );
                if ( status == errSecItemNotFound )
                {
                        const void* addKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData };
                        const void* addValues[] = { kSecClassGenericPassword, service, account, valueData };
                        CFDictionaryRef addDict = CFDictionaryCreate(
                                kCFAllocatorDefault, addKeys, addValues, 4,
                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
                        status = SecItemAdd( addDict, nullptr );
                        CFRelease( addDict );
                }

                CFRelease( attrsToUpdate );
                CFRelease( query );
                CFRelease( valueData );
                CFRelease( account );
                return status == errSecSuccess;

#else
                if ( hasSecretTool() )
                {
                        QProcess proc;
                        proc.start( QStringLiteral( "secret-tool" ),
                                    { QStringLiteral( "store" ),
                                      QStringLiteral( "--label=gLabels Neo credential" ),
                                      QStringLiteral( "application" ), QStringLiteral( "glabels-neo" ),
                                      QStringLiteral( "key" ), key } );
                        if ( proc.waitForStarted( 3000 ) )
                        {
                                proc.write( secret.toUtf8() );
                                proc.closeWriteChannel();
                                if ( proc.waitForFinished( 5000 ) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 )
                                {
                                        return true;
                                }
                        }
                }

                QMutexLocker lock( &gMutex );
                QJsonObject obj = loadFallbackJson();
                obj.insert( key, secret );
                return saveFallbackJson( obj );
#endif
        }


        QString SecretStore::retrieve( const QString& key )
        {
                if ( key.trimmed().isEmpty() )
                {
                        return {};
                }

                {
                        QMutexLocker lock( &gMutex );
                        if ( gMemoryMode )
                        {
                                return gMemoryStore.value( key );
                        }
                }

#if defined(Q_OS_WIN)
                const std::wstring target = winTargetName( key );
                PCREDENTIALW pCred = nullptr;
                if ( !CredReadW( target.c_str(), CRED_TYPE_GENERIC, 0, &pCred ) || !pCred )
                {
                        return {};
                }
                const QString value = QString::fromUtf8(
                        reinterpret_cast<const char*>( pCred->CredentialBlob ),
                        static_cast<qsizetype>( pCred->CredentialBlobSize ) );
                CredFree( pCred );
                return value;

#elif defined(Q_OS_MACOS)
                CFStringRef service = CFSTR( "io.github.schneiderwastaken.glabels-neo" );
                CFStringRef account = toCfString( key );

                const void* queryKeys[] = {
                        kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit
                };
                const void* queryValues[] = {
                        kSecClassGenericPassword, service, account, kCFBooleanTrue, kSecMatchLimitOne
                };
                CFDictionaryRef query = CFDictionaryCreate(
                        kCFAllocatorDefault, queryKeys, queryValues, 5,
                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

                CFTypeRef result = nullptr;
                const OSStatus status = SecItemCopyMatching( query, &result );

                QString value;
                if ( status == errSecSuccess && result && CFGetTypeID( result ) == CFDataGetTypeID() )
                {
                        CFDataRef data = static_cast<CFDataRef>( result );
                        value = QString::fromUtf8(
                                reinterpret_cast<const char*>( CFDataGetBytePtr( data ) ),
                                static_cast<qsizetype>( CFDataGetLength( data ) ) );
                }
                if ( result )
                {
                        CFRelease( result );
                }
                CFRelease( query );
                CFRelease( account );
                return value;

#else
                if ( hasSecretTool() )
                {
                        QProcess proc;
                        proc.start( QStringLiteral( "secret-tool" ),
                                    { QStringLiteral( "lookup" ),
                                      QStringLiteral( "application" ), QStringLiteral( "glabels-neo" ),
                                      QStringLiteral( "key" ), key } );
                        if ( proc.waitForFinished( 5000 ) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 )
                        {
                                QByteArray out = proc.readAllStandardOutput();
                                if ( out.endsWith( '\n' ) )
                                {
                                        out.chop( 1 );
                                }
                                if ( !out.isEmpty() )
                                {
                                        return QString::fromUtf8( out );
                                }
                        }
                }

                QMutexLocker lock( &gMutex );
                const QJsonObject obj = loadFallbackJson();
                return obj.value( key ).toString();
#endif
        }


        bool SecretStore::remove( const QString& key )
        {
                if ( key.trimmed().isEmpty() )
                {
                        return false;
                }

                {
                        QMutexLocker lock( &gMutex );
                        if ( gMemoryMode )
                        {
                                return gMemoryStore.remove( key ) > 0;
                        }
                }

#if defined(Q_OS_WIN)
                const std::wstring target = winTargetName( key );
                if ( CredDeleteW( target.c_str(), CRED_TYPE_GENERIC, 0 ) == TRUE )
                {
                        return true;
                }
                return GetLastError() == ERROR_NOT_FOUND;

#elif defined(Q_OS_MACOS)
                CFStringRef service = CFSTR( "io.github.schneiderwastaken.glabels-neo" );
                CFStringRef account = toCfString( key );

                const void* queryKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
                const void* queryValues[] = { kSecClassGenericPassword, service, account };
                CFDictionaryRef query = CFDictionaryCreate(
                        kCFAllocatorDefault, queryKeys, queryValues, 3,
                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

                const OSStatus status = SecItemDelete( query );
                CFRelease( query );
                CFRelease( account );
                return status == errSecSuccess || status == errSecItemNotFound;

#else
                bool ok = true;
                if ( hasSecretTool() )
                {
                        QProcess proc;
                        proc.start( QStringLiteral( "secret-tool" ),
                                    { QStringLiteral( "clear" ),
                                      QStringLiteral( "application" ), QStringLiteral( "glabels-neo" ),
                                      QStringLiteral( "key" ), key } );
                        proc.waitForFinished( 5000 );
                }

                QMutexLocker lock( &gMutex );
                QJsonObject obj = loadFallbackJson();
                if ( obj.contains( key ) )
                {
                        obj.remove( key );
                        ok = saveFallbackJson( obj );
                }
                return ok;
#endif
        }


        bool SecretStore::hasSecret( const QString& key )
        {
                return !retrieve( key ).isEmpty();
        }


        QString SecretStore::backendDescription()
        {
#if defined(Q_OS_WIN)
                return QCoreApplication::translate( "SecretStore", "Windows Credential Manager" );
#elif defined(Q_OS_MACOS)
                return QCoreApplication::translate( "SecretStore", "macOS Keychain" );
#else
                if ( hasSecretTool() )
                {
                        return QCoreApplication::translate( "SecretStore", "System Keyring (libsecret)" );
                }
                return QCoreApplication::translate( "SecretStore", "Local user credential file (owner-only permissions)" );
#endif
        }

}
