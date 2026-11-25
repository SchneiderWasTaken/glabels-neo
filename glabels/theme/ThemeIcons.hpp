//  ThemeIcons.hpp
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

#ifndef theme_ThemeIcons_hpp
#define theme_ThemeIcons_hpp


#include "theme/Theme.hpp"

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>


namespace glabels::theme
{

        ///
        /// Monoline SVG icons (glabels/icons/neo) recoloured from the theme palette
        ///
        /// The QIcon returned by icon() resolves its colour every time it is drawn, so an icon
        /// created once follows later theme switches.  Normal-role icons turn accent when
        /// hovered (QIcon::Active) or checked (QIcon::On); every role draws at 40 % opacity when
        /// disabled.  Rendered pixmaps are cached by (name, colour, pixel size) and the cache is
        /// dropped on every theme switch.
        ///
        class ThemeIcons
        {
        public:
                /// Tinted icon
                static QIcon icon( const QString& name, IconRole role = IconRole::Normal );

                /// True if glabels/icons/neo/<name>.svg is in the resources
                static bool exists( const QString& name );

                /// The SVG source with currentColor replaced by an explicit colour (empty if unknown)
                static QByteArray recolouredSvg( const QString& name, const QColor& color );

                /// Render an icon at an exact pixel size (cached)
                static QImage render( const QString& name, const QColor& color, const QSize& pixelSize );

                /// Colour an icon of this role uses in a QIcon mode and state, with the current palette
                static QColor colorFor( IconRole role, QIcon::Mode mode, QIcon::State state );

                /// Path of a recoloured SVG written to a cache folder, for url() in stylesheets
                static QString file( const QString& name, const QColor& color );

                /// Path of a generated indicator image (radio dot), for url() in stylesheets
                static QString radioFile( const QColor& ring, const QColor& fill, const QColor& dot );

                /// The gLabels Neo mark (brand/mark.svg, as is) at a logical size and device pixel ratio
                static QPixmap brandMark( int size, qreal devicePixelRatio = 1.0 );

                /// An illustration (glabels/icons/neo-art/<name>.svg) in the current palette, fitted
                /// into a logical size (aspect kept) at a device pixel ratio.  The artwork uses five
                /// sentinel colours that are replaced by tokens: #010203 ink2, #040506 line2,
                /// #070809 accent, #0A0B0C panel, #101112 tint.  Null if the name is unknown.
                static QPixmap illustration( const QString& name, const QSize& size, qreal devicePixelRatio = 1.0 );

                /// The illustration source with the sentinels replaced for a palette (for tests)
                static QByteArray illustrationSvg( const QString& name, const Palette& palette );

                /// An icon on a rounded `tint` chip, glyph in `accent` (command cards, mockup d03)
                static QIcon chipIcon( const QString& name );

                /// Drop cached pixmaps (theme switch)
                static void invalidate();
        };

}


#endif // theme_ThemeIcons_hpp
