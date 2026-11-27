//  Branding.hpp
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

#ifndef model_Branding_hpp
#define model_Branding_hpp


#include <QString>


namespace glabels::model::Branding
{

        ///
        /// Product display name.  Use this wherever the product name is shown to users.
        /// It must always be accompanied (nearby) by ATTRIBUTION or ATTRIBUTION_SHORT.
        ///
        /// NOTE: these are display strings only.  Identity strings (QSettings paths, binary names,
        /// file formats, MIME types, namespaces) deliberately remain "glabels" - see
        /// docs/CHANGES-FROM-UPSTREAM.md.  The one exception is the command line tool, which is
        /// called "glabelsneo" (docs/CLI-SPEC.md); the older "glabels-batch-qt" remains for one
        /// release because the self-updater of installed copies depends on it.
        ///
        inline const QString DISPLAY_NAME       = QStringLiteral( "gLabels Neo" );
        inline const QString ATTRIBUTION        = QStringLiteral( "Based on gLabels by Jaye Evins and contributors" );
        inline const QString ATTRIBUTION_SHORT  = QStringLiteral( "A modernized fork of gLabels" );
        inline const QString UPSTREAM_URL       = QStringLiteral( "https://github.com/j-evins/glabels-qt" );
        inline const QString PROJECT_URL        = QStringLiteral( "https://github.com/SchneiderWasTaken/glabels-neo" );


        ///
        /// Replace the product token "gLabels" with DISPLAY_NAME in (translated) UI text.
        /// Idempotent: text that already contains DISPLAY_NAME is returned unchanged.
        ///
        QString rebrand( const QString& text );

}


#endif // model_Branding_hpp
