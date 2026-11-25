//  ThemeStyle.hpp
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

#ifndef theme_ThemeStyle_hpp
#define theme_ThemeStyle_hpp


#include "theme/Theme.hpp"

#include <QHash>
#include <QString>
#include <QStringList>


namespace glabels::theme
{

        ///
        /// Generates the one application stylesheet (SPEC.md section 8) from the colour tokens
        ///
        /// The stylesheet is written as a template with tokens: "@ink" is a colour token,
        /// "@ink/40" the same colour at 40 % opacity; image tokens such as "@imgCheck" expand to
        /// recoloured icon files.  Selectors use the dynamic "role" property listed in
        /// docs/ribbon-ui/QSS-HOOKS.md.
        ///
        class ThemeStyle
        {
        public:
                /// The complete stylesheet for a palette
                static QString generate( const Palette& palette, Mode mode );

                /// The raw template (tokens not expanded)
                static QString styleTemplate();

                /// Token values for a palette (colours as #rrggbb, images as file paths)
                static QHash<QString, QString> tokens( const Palette& palette, Mode mode );

                /// Expand tokens in a template; unknown tokens are collected in *unknown
                static QString expand( const QString& text,
                                       const QHash<QString, QString>& tokens,
                                       QStringList* unknown = nullptr );

                /// A colour as a stylesheet value: #rrggbb, or rgba(r, g, b, a%) when translucent
                static QString cssColor( const QColor& color );
        };

}


#endif // theme_ThemeStyle_hpp
