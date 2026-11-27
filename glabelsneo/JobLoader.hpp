//  JobLoader.hpp
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

#ifndef cli_JobLoader_hpp
#define cli_JobLoader_hpp


#include "JobSpec.hpp"
#include "Reporter.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>


namespace glabels::cli
{

        ///
        /// What the command line said about a job (before it is merged with a job document).
        ///
        struct CliJobInput
        {
                QString     file;
                QString     printer;
                QString     output;
                QStringList fields;        ///< raw NAME=VALUE
                QStringList variables;     ///< raw NAME=VALUE
                QString     data;
                QString     dataFormat;
                PrintOptions options;
        };


        ///
        /// Builds JobSpecs from the command line and from JSON job documents.
        ///
        class JobLoader
        {
        public:
                /// Split NAME=VALUE at the first '='.  False if there is no '=' or the name is
                /// empty or contains a character a field name cannot have (':' or '}').
                static bool parseAssignment( const QString& text, QString& name, QString& value );

                /// Parse a JSON job document.  Relative paths in it (file, data, output) are
                /// resolved against 'baseDir'.  On failure returns false and fills 'error'.
                static bool parseJobDocument( const QByteArray&  json,
                                              const QString&     baseDir,
                                              QList<JobSpec>&    jobs,
                                              Diagnostic&        error );

                /// A single job from command-line flags alone.
                static bool fromCommandLine( const CliJobInput& in,
                                             JobSpec&           job,
                                             Diagnostic&        error );

                /// Apply command-line flags on top of a job (from a document): they win.
                static bool applyOverrides( const CliJobInput& in,
                                            JobSpec&           job,
                                            Diagnostic&        error );

        private:
                static bool parseJob( const QJsonObject& object,
                                      const QString&     where,
                                      const QString&     baseDir,
                                      JobSpec&           job,
                                      Diagnostic&        error );

                static bool parseStringMap( const QJsonValue&        value,
                                            const QString&           where,
                                            QMap<QString,QString>&   map,
                                            Diagnostic&              error );

                static bool parseOptions( const QJsonObject& object,
                                          const QString&     where,
                                          PrintOptions&      options,
                                          Diagnostic&        error );
        };

}


#endif // cli_JobLoader_hpp
