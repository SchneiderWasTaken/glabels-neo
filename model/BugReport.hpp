//  BugReport.hpp
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


#ifndef model_BugReport_hpp
#define model_BugReport_hpp


#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include <functional>


namespace glabels::model::BugReport
{

        ///
        /// Something the user chose to attach to a report (the current label file, recent log lines).
        ///
        struct Attachment
        {
                QString kind;   // "label" or "log"
                QString name;
                QString text;
        };


        ///
        /// Remove machine-specific details: the home directory and the user and host names.
        /// Suitable for text that is content the user created (such as a label file).
        ///
        QString redactPaths( const QString& text );


        ///
        /// redactPaths() plus e-mail addresses and IPv4 addresses.  For diagnostics and log messages.
        ///
        QString redact( const QString& text );


        ///
        /// System details for a report (redacted).
        ///
        QString diagnostics();


        ///
        /// Number of leading zero bits of a digest.
        ///
        int leadingZeroBits( const QByteArray& digest );


        ///
        /// Solve the relay's proof-of-work challenge: find a counter such that
        /// SHA-256( challenge + ":" + counter ) has at least `bits` leading zero bits.
        /// `keepGoing` is polled regularly so the caller can pump events or cancel.
        /// Returns an empty string when cancelled or when `maxIterations` is exhausted.
        ///
        QString solveProofOfWork( const QString&                     challenge,
                                  int                                bits,
                                  const std::function<bool()>&       keepGoing = {},
                                  quint64                            maxIterations = 200000000 );


        ///
        /// JSON body for the relay's POST /v1/reports.
        ///
        QByteArray toJson( const QString&           challenge,
                           const QString&           counter,
                           const QString&           summary,
                           const QString&           description,
                           const QString&           diagnosticsText,
                           const QList<Attachment>& attachments );


        ///
        /// GitHub "new issue" URL with the bug-report form pre-filled.  Used when the relay cannot
        /// be reached.  The URL is kept short enough for browsers and GitHub.
        ///
        QUrl prefilledIssueUrl( const QString& summary, const QString& description );

}


#endif // model_BugReport_hpp
