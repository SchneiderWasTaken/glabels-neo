//  ThemeScheme.hpp
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

#ifndef theme_ThemeScheme_hpp
#define theme_ThemeScheme_hpp


#include "theme/Theme.hpp"

#include <QComboBox>
#include <QIcon>
#include <QList>
#include <QString>


namespace glabels::theme
{

        ///
        /// A colour scheme: the same UI with a different palette (docs/ribbon-ui/THEMES.md)
        ///
        /// Layout, metrics and behaviour are identical for every scheme; only the tokens and
        /// whether the File button / primary buttons use a gradient differ.  Light, Dark and
        /// System still choose the variant on top of the scheme.
        ///
        struct ThemeScheme
        {
                QString     id;            ///< stored in the settings ("neo", "graphite", ...)
                const char* name;          ///< display name, translated by displayName()
                const char* description;   ///< one line, translated by displayDescription()
                Palette     light;
                Palette     dark;
                bool        gradient;      ///< false: gradA == gradB == gradC (solid File button)

                const Palette& palette( Mode mode ) const { return mode == Mode::Dark ? dark : light; }

                QString displayName() const;
                QString displayDescription() const;
        };


        ///
        /// The registry of schemes
        ///
        class ThemeSchemes
        {
        public:
                /// The id of the default scheme (Graphite)
                static QString defaultId();

                /// Every scheme, the default first
                static const QList<ThemeScheme>& all();

                /// A scheme by id; unknown or corrupt ids give the default (Graphite)
                static const ThemeScheme& find( const QString& id );

                /// Is this a known id?
                static bool contains( const QString& id );

                /// A palette from 21 hex values in THEMES.md order: bg, chrome, ribbon, work, panel,
                /// line, line2, ink, ink2, ink3, accent, tint, hover, pressed, ctx, ctxLine, ok,
                /// okTint, warn, grad (solid), onGrad.  Empty palette if the list is malformed.
                static Palette solidPalette( const QString& hexList );

                /// Small swatch for pickers: background, panel and accent of a scheme in a mode
                static QIcon swatch( const ThemeScheme& scheme, Mode mode, int size = 16 );

                /// Fill a combo box with every scheme (data = id, tooltip = description) and select one
                static void fillCombo( QComboBox* combo, const QString& currentId, Mode mode );
        };

}


#endif // theme_ThemeScheme_hpp
