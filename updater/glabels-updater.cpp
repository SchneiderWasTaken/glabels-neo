//  glabels-updater.cpp
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

//
// glabels-updater: the last step of a self-update.
//
// The application downloads and unpacks a new version into a hidden sibling of its own install
// directory (".<name>.update") while it is running.  It cannot replace its own files, so it starts
// this helper, which waits for the application to exit and then swaps the directories:
//
//     installed  ->  .<name>.old        (previous version, kept only until the swap is complete)
//     staged     ->  installed          (new version)
//
// Directory renames within one volume are atomic and fast, so the window in which the install
// is "missing" is a few milliseconds.  If anything fails, every swap that already happened is
// rolled back, so the user always has a working install.
//
// This program deliberately depends on nothing but the C++ standard library and the OS (no Qt):
// it runs from a temporary copy while the install directory, including Qt, is being replaced.
//
// Usage:
//    glabels-updater [--wait-pid PID] [--result FILE] [--log FILE] [--lock DIR]
//                    [--retries N] [--retry-ms MS]
//                    --unit INSTALLED STAGED [--unit INSTALLED STAGED ...]
//                    [--relaunch PROGRAM [ARGS...]]
//
// --relaunch must be last: everything after it is the command line of the program to start.
//

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <fcntl.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace fs = std::filesystem;


namespace
{

        struct Unit
        {
                fs::path installed;
                fs::path staged;
                fs::path backup;
        };


        struct Options
        {
                std::int64_t           waitPid{0};
                fs::path               resultFile;
                fs::path               logFile;
                fs::path               lockDir;
                int                    retries{40};
                int                    retryMs{500};
                std::vector<Unit>      units;
                bool                   relaunch{false};
                std::vector<fs::path>  relaunchCommand;
        };


        //
        // Logging
        //
        fs::path gLogFile;

        std::string utf8( const fs::path& p )
        {
                auto u = p.u8string();
                return std::string( u.begin(), u.end() );
        }

        std::string timestamp()
        {
                std::time_t t = std::time( nullptr );
                std::tm local{};
#ifdef _WIN32
                localtime_s( &local, &t );
#else
                localtime_r( &t, &local );
#endif
                char buf[32];
                std::strftime( buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local );
                return buf;
        }

        void log( const std::string& message )
        {
                if ( gLogFile.empty() )
                {
                        return;
                }

                std::error_code ec;
                if ( fs::exists( gLogFile, ec ) && fs::file_size( gLogFile, ec ) > 200 * 1024 )
                {
                        fs::remove( gLogFile, ec );
                }

                std::ofstream out( gLogFile, std::ios::app );
                if ( out )
                {
                        out << timestamp() << " [updater] " << message << "\n";
                }
        }


        std::string jsonEscape( const std::string& s )
        {
                std::string r;
                for ( unsigned char c : s )
                {
                        switch ( c )
                        {
                        case '"':  r += "\\\""; break;
                        case '\\': r += "\\\\"; break;
                        case '\n': r += "\\n";  break;
                        case '\r': r += "\\r";  break;
                        case '\t': r += "\\t";  break;
                        default:
                                if ( c < 0x20 )
                                {
                                        char buf[8];
                                        std::snprintf( buf, sizeof(buf), "\\u%04x", c );
                                        r += buf;
                                }
                                else
                                {
                                        r += static_cast<char>( c );
                                }
                        }
                }
                return r;
        }


        void writeResult( const Options& opt, const char* status, const std::string& detail )
        {
                log( std::string( status ) + ": " + detail );

                if ( opt.resultFile.empty() )
                {
                        return;
                }

                std::ofstream out( opt.resultFile, std::ios::trunc );
                if ( out )
                {
                        out << "{\"status\":\"" << status << "\",\"detail\":\"" << jsonEscape( detail )
                            << "\",\"time\":\"" << timestamp() << "\"}\n";
                }
        }


        //
        // Waiting for the application to exit
        //
        bool waitForExit( std::int64_t pid, int timeoutMs )
        {
                if ( pid <= 0 )
                {
                        return true;
                }

#ifdef _WIN32
                HANDLE h = OpenProcess( SYNCHRONIZE, FALSE, static_cast<DWORD>( pid ) );
                if ( !h )
                {
                        return true; // no such process (any more)
                }
                DWORD r = WaitForSingleObject( h, static_cast<DWORD>( timeoutMs ) );
                CloseHandle( h );
                return r == WAIT_OBJECT_0;
#else
                auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( timeoutMs );
                while ( true )
                {
                        if ( kill( static_cast<pid_t>( pid ), 0 ) != 0 && errno == ESRCH )
                        {
                                return true;
                        }
                        if ( std::chrono::steady_clock::now() >= deadline )
                        {
                                return false;
                        }
                        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                }
#endif
        }


        //
        // Rename with retries: on Windows a directory cannot be renamed while anything inside it is
        // open (the just-exited application, a virus scanner, the indexer, ...) so keep trying.
        //
        bool renameWithRetry( const fs::path& from, const fs::path& to, const Options& opt, std::string* error )
        {
                std::error_code ec;
                for ( int attempt = 0; attempt <= opt.retries; attempt++ )
                {
                        ec.clear();
                        fs::rename( from, to, ec );
                        if ( !ec )
                        {
                                return true;
                        }
                        if ( attempt < opt.retries )
                        {
                                std::this_thread::sleep_for( std::chrono::milliseconds( opt.retryMs ) );
                        }
                }
                *error = "cannot rename " + utf8( from ) + " to " + utf8( to ) + ": " + ec.message();
                return false;
        }


        void removeQuietly( const fs::path& p )
        {
                std::error_code ec;
                fs::remove_all( p, ec );
        }


        //
        // The swap
        //
        enum class SwapOutcome { Applied, AlreadyApplied, Failed };

        SwapOutcome swapAll( const Options& opt, std::vector<Unit>& units, std::string* error )
        {
                std::error_code ec;

                // Nothing staged for any unit: a previous helper (or a manual action) did it already.
                bool anyStaged = false;
                for ( const auto& u : units )
                {
                        anyStaged = anyStaged || fs::exists( u.staged, ec );
                }
                if ( !anyStaged )
                {
                        return SwapOutcome::AlreadyApplied;
                }

                // Every unit must be swappable before the first one is touched.
                for ( const auto& u : units )
                {
                        if ( !fs::exists( u.staged, ec ) )
                        {
                                *error = "staged copy missing: " + utf8( u.staged );
                                return SwapOutcome::Failed;
                        }
                        if ( !fs::exists( u.installed, ec ) )
                        {
                                *error = "installed copy missing: " + utf8( u.installed );
                                return SwapOutcome::Failed;
                        }
                }

                std::vector<Unit> done;
                for ( const auto& u : units )
                {
                        removeQuietly( u.backup ); // leftover of an earlier attempt

                        if ( !renameWithRetry( u.installed, u.backup, opt, error ) )
                        {
                                break;
                        }

                        if ( !renameWithRetry( u.staged, u.installed, opt, error ) )
                        {
                                // put this one back, then everything before it
                                std::string ignored;
                                renameWithRetry( u.backup, u.installed, opt, &ignored );
                                break;
                        }

                        done.push_back( u );
                }

                if ( done.size() == units.size() )
                {
                        return SwapOutcome::Applied;
                }

                // Roll back in reverse order.
                for ( auto it = done.rbegin(); it != done.rend(); ++it )
                {
                        std::string ignored;
                        std::error_code rc;
                        fs::rename( it->installed, it->staged, rc );        // new version back to its staging place
                        renameWithRetry( it->backup, it->installed, opt, &ignored );
                }
                return SwapOutcome::Failed;
        }


        //
        // A lock directory: mkdir is atomic, so only one helper swaps at a time.
        //
        bool acquireLock( const fs::path& dir )
        {
                if ( dir.empty() )
                {
                        return true;
                }

                std::error_code ec;
                if ( fs::create_directory( dir, ec ) )
                {
                        return true;
                }

                // Stale lock (helper killed): take it over after 5 minutes.
                auto mtime = fs::last_write_time( dir, ec );
                if ( !ec && ( fs::file_time_type::clock::now() - mtime ) > std::chrono::minutes( 5 ) )
                {
                        fs::remove_all( dir, ec );
                        return fs::create_directory( dir, ec );
                }
                return false;
        }


        //
        // Starting the application again
        //
        bool launch( const std::vector<fs::path>& command )
        {
                if ( command.empty() )
                {
                        return false;
                }

#ifdef _WIN32
                auto quote = []( const std::wstring& a )
                {
                        if ( !a.empty() && a.find_first_of( L" \t\"" ) == std::wstring::npos )
                        {
                                return a;
                        }
                        std::wstring q = L"\"";
                        size_t backslashes = 0;
                        for ( wchar_t c : a )
                        {
                                if ( c == L'\\' )
                                {
                                        backslashes++;
                                        continue;
                                }
                                if ( c == L'"' )
                                {
                                        q.append( backslashes * 2 + 1, L'\\' );
                                        q += L'"';
                                }
                                else
                                {
                                        q.append( backslashes, L'\\' );
                                        q += c;
                                }
                                backslashes = 0;
                        }
                        q.append( backslashes * 2, L'\\' );
                        q += L'"';
                        return q;
                };

                std::wstring cmd;
                for ( const auto& a : command )
                {
                        if ( !cmd.empty() )
                        {
                                cmd += L' ';
                        }
                        cmd += quote( a.wstring() );
                }

                STARTUPINFOW si{};
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi{};
                std::wstring dir = command[0].parent_path().wstring();
                // No application name: the command line is searched for on PATH (and ".exe" is added), like a shell would.
                BOOL ok = CreateProcessW( nullptr, cmd.data(), nullptr, nullptr, FALSE,
                                          DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr,
                                          dir.empty() ? nullptr : dir.c_str(), &si, &pi );
                if ( ok )
                {
                        CloseHandle( pi.hThread );
                        CloseHandle( pi.hProcess );
                }
                return ok != 0;
#else
                std::vector<std::string> args;
                for ( const auto& a : command )
                {
                        args.push_back( a.string() );
                }

                // A macOS application bundle is started through LaunchServices.
                if ( command[0].extension() == ".app" )
                {
                        std::vector<std::string> openCmd{ "/usr/bin/open", "-n", args[0] };
                        if ( args.size() > 1 )
                        {
                                openCmd.push_back( "--args" );
                                openCmd.insert( openCmd.end(), args.begin() + 1, args.end() );
                        }
                        args = openCmd;
                }

                pid_t pid = fork();
                if ( pid < 0 )
                {
                        return false;
                }
                if ( pid == 0 )
                {
                        setsid();
                        int devnull = open( "/dev/null", O_RDWR );
                        if ( devnull >= 0 )
                        {
                                dup2( devnull, 0 );
                                dup2( devnull, 1 );
                                dup2( devnull, 2 );
                        }
                        std::vector<char*> argv;
                        for ( auto& a : args )
                        {
                                argv.push_back( a.data() );
                        }
                        argv.push_back( nullptr );
                        execv( argv[0], argv.data() );
                        _exit( 127 );
                }
                return true;
#endif
        }


        bool parse( const std::vector<fs::path>& args, Options* opt, std::string* error )
        {
                auto str = []( const fs::path& p ) { return p.string(); };

                for ( size_t i = 0; i < args.size(); i++ )
                {
                        std::string a = str( args[i] );
                        auto need = [&]( size_t n )
                        {
                                if ( i + n >= args.size() )
                                {
                                        *error = "missing value for " + a;
                                        return false;
                                }
                                return true;
                        };

                        if ( a == "--wait-pid" )
                        {
                                if ( !need( 1 ) ) return false;
                                opt->waitPid = std::atoll( str( args[++i] ).c_str() );
                        }
                        else if ( a == "--result" )
                        {
                                if ( !need( 1 ) ) return false;
                                opt->resultFile = args[++i];
                        }
                        else if ( a == "--log" )
                        {
                                if ( !need( 1 ) ) return false;
                                opt->logFile = args[++i];
                        }
                        else if ( a == "--lock" )
                        {
                                if ( !need( 1 ) ) return false;
                                opt->lockDir = args[++i];
                        }
                        else if ( a == "--retries" )
                        {
                                if ( !need( 1 ) ) return false;
                                opt->retries = std::max( 0, std::atoi( str( args[++i] ).c_str() ) );
                        }
                        else if ( a == "--retry-ms" )
                        {
                                if ( !need( 1 ) ) return false;
                                opt->retryMs = std::max( 0, std::atoi( str( args[++i] ).c_str() ) );
                        }
                        else if ( a == "--unit" )
                        {
                                if ( !need( 2 ) ) return false;
                                Unit u;
                                u.installed = args[++i];
                                u.staged    = args[++i];
                                auto name   = u.installed.filename();
                                u.backup    = u.installed.parent_path() / ( "." + name.string() + ".old" );
                                opt->units.push_back( u );
                        }
                        else if ( a == "--relaunch" )
                        {
                                opt->relaunch = true;
                                opt->relaunchCommand.assign( args.begin() + i + 1, args.end() );
                                break;
                        }
                        else
                        {
                                *error = "unknown argument " + a;
                                return false;
                        }
                }

                if ( opt->units.empty() )
                {
                        *error = "no --unit given";
                        return false;
                }
                if ( opt->relaunch && opt->relaunchCommand.empty() )
                {
                        *error = "--relaunch needs a program";
                        return false;
                }
                return true;
        }


        int run( const std::vector<fs::path>& args )
        {
                Options opt;
                std::string error;
                if ( !parse( args, &opt, &error ) )
                {
                        std::fprintf( stderr, "glabels-updater: %s\n", error.c_str() );
                        return 2;
                }

                gLogFile = opt.logFile;
                log( "started, " + std::to_string( opt.units.size() ) + " unit(s)" );

                int exitCode = 0;

                if ( !waitForExit( opt.waitPid, 120 * 1000 ) )
                {
                        writeResult( opt, "failed", "the application did not exit in time; nothing was changed" );
                        return 1;
                }

                if ( !acquireLock( opt.lockDir ) )
                {
                        // Another helper is swapping right now; it takes care of everything.
                        log( "another updater holds the lock; only relaunching" );
                }
                else
                {
                        switch ( swapAll( opt, opt.units, &error ) )
                        {
                        case SwapOutcome::Applied:
                                writeResult( opt, "applied", "install directories replaced" );
                                for ( const auto& u : opt.units )
                                {
                                        removeQuietly( u.backup );
                                }
                                break;
                        case SwapOutcome::AlreadyApplied:
                                writeResult( opt, "applied", "nothing staged: already up to date" );
                                break;
                        case SwapOutcome::Failed:
                                writeResult( opt, "failed", error );
                                exitCode = 1;
                                break;
                        }

                        if ( !opt.lockDir.empty() )
                        {
                                removeQuietly( opt.lockDir );
                        }
                }

                if ( opt.relaunch )
                {
                        // Also after a failure: the previous version is back in place and the user asked
                        // for the program.
                        if ( !launch( opt.relaunchCommand ) )
                        {
                                log( "could not restart " + utf8( opt.relaunchCommand[0] ) );
                                if ( exitCode == 0 )
                                {
                                        exitCode = 1;
                                }
                        }
                }

                return exitCode;
        }

}


#ifdef _WIN32
int wmain( int argc, wchar_t** argv )
#else
int main( int argc, char** argv )
#endif
{
        std::vector<fs::path> args;
        for ( int i = 1; i < argc; i++ )
        {
                args.emplace_back( argv[i] );
        }
        return run( args );
}
