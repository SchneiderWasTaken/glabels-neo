//  Theme.cpp
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


#include "theme/Theme.hpp"

#include "theme/Decorator.hpp"
#include "theme/ThemeIcons.hpp"
#include "theme/ThemeStyle.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStyle>
#include <QStyleHints>


namespace
{
        using glabels::theme::Palette;

        QColor hex( const char* value )
        {
                return QColor( QString::fromLatin1( value ) );
        }


        QColor withAlpha( QColor color, double alpha )
        {
                color.setAlphaF( alpha );
                return color;
        }
}


namespace glabels::theme
{

        ///
        /// Constructor
        ///
        Theme::Theme()
                : mMode( Mode::Light )
        {
        }


        ///
        /// Singleton
        ///
        Theme& Theme::instance()
        {
                static Theme theme;
                return theme;
        }


        ///
        /// Light tokens (SPEC.md 4.1)
        ///
        Palette Theme::lightPalette()
        {
                Palette p;
                p.bg      = hex( "#F8FAFC" );
                p.chrome  = hex( "#F1F5F9" );
                p.ribbon  = hex( "#FFFFFF" );
                p.work    = hex( "#E2E8F0" );
                p.panel   = hex( "#FFFFFF" );
                p.line    = hex( "#E2E8F0" );
                p.line2   = hex( "#CBD5E1" );
                p.ink     = hex( "#0F172A" );
                p.ink2    = hex( "#475569" );
                p.ink3    = hex( "#64748B" );
                p.accent  = hex( "#4F46E5" );
                p.tint    = hex( "#EDE9FE" );
                p.hover   = hex( "#F1F5F9" );
                p.pressed = hex( "#E0E7FF" );
                p.ctx     = hex( "#EDE9FE" );
                p.ctxLine = hex( "#7C3AED" );
                p.ok      = hex( "#047857" );
                p.okTint  = hex( "#ECFDF5" );
                p.warn    = hex( "#B45309" );
                p.gradA   = hex( "#7C3AED" );
                p.gradB   = hex( "#4F46E5" );
                p.gradC   = hex( "#2563EB" );
                p.onGrad  = hex( "#FFFFFF" );
                return p;
        }


        ///
        /// Dark tokens (SPEC.md 4.1)
        ///
        Palette Theme::darkPalette()
        {
                Palette p;
                p.bg      = hex( "#090D1A" );
                p.chrome  = hex( "#0F172A" );
                p.ribbon  = hex( "#151E32" );
                p.work    = hex( "#090D1A" );
                p.panel   = hex( "#0F172A" );
                p.line    = hex( "#1E293B" );
                p.line2   = hex( "#334155" );
                p.ink     = hex( "#F8FAFC" );
                p.ink2    = hex( "#CBD5E1" );
                p.ink3    = hex( "#94A3B8" );
                p.accent  = hex( "#818CF8" );
                p.tint    = hex( "#1E293B" );
                p.hover   = hex( "#1E293B" );
                p.pressed = hex( "#2B3556" );
                p.ctx     = hex( "#2A2447" );
                p.ctxLine = hex( "#A78BFA" );
                p.ok      = hex( "#34D399" );
                p.okTint  = hex( "#064E3B" );
                p.warn    = hex( "#FBBF24" );
                p.gradA   = hex( "#A78BFA" );
                p.gradB   = hex( "#818CF8" );
                p.gradC   = hex( "#60A5FA" );
                p.onGrad  = hex( "#090D1A" );
                return p;
        }


        ///
        /// Token table of a mode
        ///
        const Palette& Theme::paletteFor( Mode mode )
        {
                static const Palette light = lightPalette();
                static const Palette dark  = darkPalette();
                return mode == Mode::Dark ? dark : light;
        }


        ///
        /// Icon colour of a role
        ///
        QColor Theme::iconColor( const Palette& palette, IconRole role )
        {
                switch ( role )
                {
                case IconRole::Accent:     return palette.accent;
                case IconRole::OnGradient: return palette.onGrad;
                case IconRole::Disabled:   return withAlpha( palette.ink, 0.4 );
                case IconRole::Normal:
                default:                   return palette.ink2;
                }
        }


        ///
        /// Resolve the stored preference
        ///
        Mode Theme::resolveMode( int colorSchemePreference, bool systemPrefersDark )
        {
                switch ( colorSchemePreference )
                {
                case 1:  return Mode::Dark;
                case 2:  return systemPrefersDark ? Mode::Dark : Mode::Light;
                case 0:
                default: return Mode::Light;
                }
        }


        ///
        /// System colour scheme
        ///
        bool Theme::systemPrefersDark()
        {
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
                return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
                return false;
#endif
        }


        ///
        /// Mode
        ///
        Mode Theme::mode() const
        {
                return mMode;
        }


        ///
        /// Set mode
        ///
        void Theme::setMode( Mode mode )
        {
                mMode = mode;
        }


        ///
        /// Palette
        ///
        const Palette& Theme::palette() const
        {
                return paletteFor( mMode );
        }


        ///
        /// UI font
        ///
        QFont Theme::uiFont( int px, int weight ) const
        {
                QFont font = QApplication::font();
                font.setFamilies( { QStringLiteral( "Inter" ),
                                    QStringLiteral( "Segoe UI" ),
                                    QFontDatabase::systemFont( QFontDatabase::GeneralFont ).family() } );
                font.setPixelSize( px );
                font.setWeight( QFont::Weight( qBound( 100, weight, 900 ) ) );
                font.setItalic( false );
                font.setUnderline( false );
                return font;
        }


        ///
        /// Monospace font
        ///
        QFont Theme::monoFont( int px ) const
        {
                QFont font = QFontDatabase::systemFont( QFontDatabase::FixedFont );
                font.setFamilies( { QStringLiteral( "JetBrains Mono" ),
                                    QStringLiteral( "Consolas" ),
                                    font.family() } );
                font.setStyleHint( QFont::Monospace );
                font.setPixelSize( px );
                return font;
        }


        ///
        /// Icon
        ///
        QIcon Theme::icon( const QString& name, IconRole role ) const
        {
                return ThemeIcons::icon( name, role );
        }


        ///
        /// Stylesheet
        ///
        QString Theme::styleSheet() const
        {
                return ThemeStyle::generate( palette(), mMode );
        }


        ///
        /// QPalette for the Fusion base style
        ///
        QPalette Theme::qtPalette() const
        {
                const Palette& t = palette();
                QPalette p;

                p.setColor( QPalette::Window,          t.bg );
                p.setColor( QPalette::WindowText,      t.ink );
                p.setColor( QPalette::Base,            t.panel );
                p.setColor( QPalette::AlternateBase,   t.bg );
                p.setColor( QPalette::ToolTipBase,     t.panel );
                p.setColor( QPalette::ToolTipText,     t.ink );
                p.setColor( QPalette::PlaceholderText, t.ink3 );
                p.setColor( QPalette::Text,            t.ink );
                p.setColor( QPalette::Button,          t.panel );
                p.setColor( QPalette::ButtonText,      t.ink );
                p.setColor( QPalette::BrightText,      t.onGrad );
                p.setColor( QPalette::Light,           t.panel );
                p.setColor( QPalette::Midlight,        t.chrome );
                p.setColor( QPalette::Mid,             t.line );
                p.setColor( QPalette::Dark,            t.line2 );
                p.setColor( QPalette::Shadow,          withAlpha( QColor( Qt::black ), mMode == Mode::Dark ? 0.6 : 0.2 ) );
                p.setColor( QPalette::Highlight,       t.accent );
                p.setColor( QPalette::HighlightedText, t.onGrad );
                p.setColor( QPalette::Link,            t.accent );
                p.setColor( QPalette::LinkVisited,     t.accent );
#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
                p.setColor( QPalette::Accent,          t.accent );
#endif

                const QColor disabledInk = withAlpha( t.ink, 0.4 );
                p.setColor( QPalette::Disabled, QPalette::WindowText,      disabledInk );
                p.setColor( QPalette::Disabled, QPalette::Text,            disabledInk );
                p.setColor( QPalette::Disabled, QPalette::ButtonText,      disabledInk );
                p.setColor( QPalette::Disabled, QPalette::HighlightedText, disabledInk );
                p.setColor( QPalette::Disabled, QPalette::Highlight,       t.line2 );
                p.setColor( QPalette::Disabled, QPalette::Base,            t.chrome );

                return p;
        }


        ///
        /// Apply to the application
        ///
        void Theme::applyToApplication()
        {
                if ( !qApp )
                {
                        return;
                }

                ThemeIcons::invalidate();

                // The Fusion base style draws from the palette; native styles follow the scheme hint.
                // Once the stylesheet is set, style() is the stylesheet proxy, which may not report the
                // base style's name; remember the last real one.
                auto* style = QApplication::style();
                if ( style && !style->name().isEmpty() )
                {
                        mBaseStyleName = style->name();
                }
                if ( mBaseStyleName.compare( "fusion", Qt::CaseInsensitive ) == 0 )
                {
                        QApplication::setPalette( qtPalette() );
                }

                QApplication::setFont( uiFont( 13 ) );

                Decorator::install();

                // Re-polishes every widget: nothing else needs to be rebuilt.
                qApp->setStyleSheet( styleSheet() );

                emit changed();
        }

}
