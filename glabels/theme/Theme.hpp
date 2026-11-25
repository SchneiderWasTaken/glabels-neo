//  Theme.hpp
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

#ifndef theme_Theme_hpp
#define theme_Theme_hpp


#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QPalette>
#include <QString>


namespace glabels::theme
{

        ///
        /// Resolved colour mode (the "System" preference is resolved before it gets here)
        ///
        enum class Mode     { Light, Dark };


        ///
        /// How an icon is tinted (SPEC.md 4.4)
        ///
        ///   Normal     ink2, accent when hovered or checked
        ///   Accent     accent (large ribbon buttons)
        ///   OnGradient onGrad (buttons on the gradient)
        ///   Disabled   ink at 40 % opacity
        ///
        enum class IconRole { Normal, Accent, OnGradient, Disabled };


        ///
        /// Colour tokens (SPEC.md 4.1)
        ///
        struct Palette
        {
                QColor bg, chrome, ribbon, work, panel, line, line2;
                QColor ink, ink2, ink3, accent, tint, hover, pressed;
                QColor ctx, ctxLine, ok, okTint, warn;
                QColor gradA, gradB, gradC, onGrad;   // 135 degree gradient A > B > C
        };


        ///
        /// Dimensions in device-independent pixels (SPEC.md 4.3)
        ///
        struct Metrics
        {
                static constexpr int tabStrip = 42, tab = 34, tabActive = 36, ribbon = 118;
                static constexpr int largeW = 58, largeH = 78, smallH = 26, iconBtn = 28;
                static constexpr int field = 26, groupName = 22, groupPad = 8;
                static constexpr int contentPanel = 300, status = 28, ruler = 22;
        };


        ///
        /// Application theme singleton: tokens, fonts, icons and the generated stylesheet
        ///
        /// Custom-painted widgets read palette() and repaint on changed().  Plain Qt widgets get
        /// their look from styleSheet() (see docs/ribbon-ui/QSS-HOOKS.md).
        ///
        class Theme : public QObject
        {
                Q_OBJECT

                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        private:
                Theme();

        public:
                static Theme& instance();


                /////////////////////////////////
                // Accessors (WORKPLAN.md contract)
                /////////////////////////////////
        public:
                /// Resolved mode (System already resolved)
                Mode            mode() const;

                /// Colour tokens of the current mode
                const Palette&  palette() const;

                /// UI font (Inter, Segoe UI, system UI font) at a pixel size and CSS weight
                QFont           uiFont( int px = 12, int weight = 400 ) const;

                /// Monospace font (JetBrains Mono, Consolas, system fixed font) at a pixel size
                QFont           monoFont( int px = 12 ) const;

                /// Tinted icon from the neo set (glabels/icons/neo/<name>.svg)
                QIcon           icon( const QString& name, IconRole role = IconRole::Normal ) const;

                /// The whole application stylesheet for the current mode
                QString         styleSheet() const;

                /// Set palette, font and stylesheet on the application; emits changed()
                void            applyToApplication();


                /////////////////////////////////
                // Extensions (not part of the WP1 contract)
                /////////////////////////////////
        public:
                /// Select the mode (does not apply it; call applyToApplication())
                void            setMode( Mode mode );

                /// QPalette built from the tokens, for the Fusion base style
                QPalette        qtPalette() const;

                /// Token tables
                static Palette  lightPalette();
                static Palette  darkPalette();
                static const Palette& paletteFor( Mode mode );

                /// Colour of an icon role in a palette (for the normal, enabled state)
                static QColor   iconColor( const Palette& palette, IconRole role );

                /// Resolve the stored preference (0 light, 1 dark, 2 system; model::Settings order)
                /// against the system colour scheme.
                static Mode     resolveMode( int colorSchemePreference, bool systemPrefersDark );

                /// Does the system ask for a dark scheme? (false when the platform does not say)
                static bool     systemPrefersDark();


                /////////////////////////////////
                // Signals
                /////////////////////////////////
        signals:
                /// Mode or palette changed: repaint custom widgets
                void changed();


                /////////////////////////////////
                // Private data
                /////////////////////////////////
        private:
                Mode    mMode;
                QString mBaseStyleName;
        };

}


#endif // theme_Theme_hpp
