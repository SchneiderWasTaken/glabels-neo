//  ThemeScheme.cpp
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


#include "theme/ThemeScheme.hpp"

#include <QCoreApplication>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QStringList>


namespace
{
        using glabels::theme::Palette;
        using glabels::theme::ThemeScheme;
        using glabels::theme::ThemeSchemes;


        QList<ThemeScheme> buildSchemes()
        {
                // Hex values are normative (docs/ribbon-ui/THEMES.md).  Token order:
                // bg chrome ribbon work panel line line2 ink ink2 ink3 accent tint hover pressed
                // ctx ctxLine ok okTint warn grad onGrad
                QList<ThemeScheme> list;

                list.append( { QStringLiteral( "graphite" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Graphite" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Neutral greys and a steel-blue accent, like a professional tool" ),
                               ThemeSchemes::solidPalette( "#F4F4F5 #EAEAEC #FFFFFF #DCDCDF #FFFFFF #E2E2E5 #B8B8BE #18181B #3F3F46 #5B5B64 #2F6DB5 #E4ECF6 #EDEDEF #DCE6F3 #E4ECF6 #2F6DB5 #15803D #E8F5EC #A15C07 #2F6DB5 #FFFFFF" ),
                               ThemeSchemes::solidPalette( "#121214 #1A1A1D #202024 #0C0C0E #18181B #2C2C31 #494950 #F4F4F5 #D0D0D5 #9A9AA3 #6EA2E0 #26303D #2A2A2F #2E3A4B #26303D #6EA2E0 #4ADE80 #123221 #E0A93B #5B93D6 #0C0C0E" ),
                               false } );

                list.append( { QStringLiteral( "neo" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Neo" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Violet-blue gradient and an indigo accent; the original gLabels Neo look" ),
                               glabels::theme::Theme::lightPalette(),
                               glabels::theme::Theme::darkPalette(),
                               true } );

                list.append( { QStringLiteral( "paper" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Paper" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Warm stone surfaces, ink-black text and a forest-green accent" ),
                               ThemeSchemes::solidPalette( "#FAF8F4 #F0EDE6 #FEFDFB #E5E0D6 #FEFDFB #E7E2D8 #BDB6A8 #1C1917 #44403C #6B655C #2F6B4F #E3EEE7 #F3F0E9 #DCEAE1 #E3EEE7 #2F6B4F #166534 #E6F2EA #9A5B0B #2F6B4F #FFFFFF" ),
                               ThemeSchemes::solidPalette( "#141311 #1C1A17 #24211D #0E0D0B #1A1815 #2F2B25 #4D473E #F5F2EC #D6D0C5 #A39C8F #7CC29B #243229 #2B2823 #2D4034 #243229 #7CC29B #6EE7A0 #12301F #E3AE4F #7CC29B #0E0D0B" ),
                               false } );

                list.append( { QStringLiteral( "harbor" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Harbor" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Cool navy surfaces and a teal accent" ),
                               ThemeSchemes::solidPalette( "#F3F6F8 #E8EEF2 #FFFFFF #D6DFE6 #FFFFFF #DCE4EA #AEBBC6 #0B1F2A #334A58 #55697A #0F766E #DDF0EE #EDF2F5 #CFE9E6 #DDF0EE #0F766E #15803D #E4F4EA #A15C07 #0F766E #FFFFFF" ),
                               ThemeSchemes::solidPalette( "#0B141A #111D25 #17262F #070D11 #0F1A21 #1F313C #3A5160 #EAF2F6 #C3D3DC #8FA6B4 #4FD1C5 #12302F #1C2E39 #17403F #12302F #4FD1C5 #4ADE80 #0F2E1D #E5B04A #4FD1C5 #06161A" ),
                               false } );

                list.append( { QStringLiteral( "signal" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Signal" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "High contrast with a burnt-orange accent" ),
                               ThemeSchemes::solidPalette( "#FAFAF9 #F1F0EE #FFFFFF #E3E2DF #FFFFFF #E5E3DF #A8A59E #111111 #3A3936 #5C5A55 #C2410C #FDEBDD #F4F3F0 #FBDCC4 #FDEBDD #C2410C #15803D #E6F4EA #92400E #C2410C #FFFFFF" ),
                               ThemeSchemes::solidPalette( "#0E0E0D #171716 #1F1F1D #080807 #141413 #2B2A28 #55534E #FAFAF9 #D6D4CF #A19E97 #FB923C #3A2412 #262523 #4A2C14 #3A2412 #FB923C #4ADE80 #122C1B #FBBF24 #FB923C #140A02" ),
                               false } );

                list.append( { QStringLiteral( "system" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "System" ),
                               QT_TRANSLATE_NOOP( "ThemeScheme", "Neutral surfaces and the Windows 11 blue accent, like a native app" ),
                               ThemeSchemes::solidPalette( "#F3F3F3 #EBEBEB #FBFBFB #E0E0E0 #FFFFFF #E0E0E0 #B3B3B3 #1A1A1A #3D3D3D #5F5F5F #0F6CBD #E1EDF8 #F0F0F0 #D6E6F5 #E1EDF8 #0F6CBD #107C10 #E5F3E5 #8A5A00 #0F6CBD #FFFFFF" ),
                               ThemeSchemes::solidPalette( "#202020 #2B2B2B #2F2F2F #181818 #272727 #3A3A3A #5C5C5C #FFFFFF #D6D6D6 #A6A6A6 #60CDFF #1F3A4D #353535 #1F4A66 #1F3A4D #60CDFF #6CCB5F #1B3A1B #E0B03A #60CDFF #00263A" ),
                               false } );

                return list;
        }
}


namespace glabels::theme
{

        QString ThemeScheme::displayName() const
        {
                return QCoreApplication::translate( "ThemeScheme", name );
        }


        QString ThemeScheme::displayDescription() const
        {
                return QCoreApplication::translate( "ThemeScheme", description );
        }


        QString ThemeSchemes::defaultId()
        {
                // Graphite since round 4 (owner decision).  A missing setting, and an unknown or
                // corrupt saved id, both resolve to it; a saved "neo" keeps Neo.
                return QStringLiteral( "graphite" );
        }


        const QList<ThemeScheme>& ThemeSchemes::all()
        {
                static const QList<ThemeScheme> schemes = buildSchemes();
                return schemes;
        }


        bool ThemeSchemes::contains( const QString& id )
        {
                for ( const auto& scheme : all() )
                {
                        if ( scheme.id == id )
                        {
                                return true;
                        }
                }
                return false;
        }


        const ThemeScheme& ThemeSchemes::find( const QString& id )
        {
                for ( const auto& scheme : all() )
                {
                        if ( scheme.id == id )
                        {
                                return scheme;
                        }
                }
                for ( const auto& scheme : all() )
                {
                        if ( scheme.id == defaultId() )
                        {
                                return scheme;
                        }
                }
                return all().first();
        }


        Palette ThemeSchemes::solidPalette( const QString& hexList )
        {
                const QStringList v = hexList.split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
                Palette p;
                if ( v.size() != 21 )
                {
                        return p;
                }
                int i = 0;
                for ( QColor* c : { &p.bg, &p.chrome, &p.ribbon, &p.work, &p.panel, &p.line, &p.line2,
                                    &p.ink, &p.ink2, &p.ink3, &p.accent, &p.tint, &p.hover, &p.pressed,
                                    &p.ctx, &p.ctxLine, &p.ok, &p.okTint, &p.warn, &p.gradB, &p.onGrad } )
                {
                        *c = QColor( v.at( i++ ) );
                }
                p.gradA = p.gradB;
                p.gradC = p.gradB;
                return p;
        }


        QIcon ThemeSchemes::swatch( const ThemeScheme& scheme, Mode mode, int size )
        {
                const Palette& p = scheme.palette( mode );
                QIcon icon;
                for ( int scale : { 1, 2 } )
                {
                        const int px = size * scale;
                        QPixmap pm( px, px );
                        pm.fill( Qt::transparent );
                        QPainter painter( &pm );
                        painter.setRenderHint( QPainter::Antialiasing );
                        const qreal r = px * 0.2;
                        QRectF box( 0.5 * scale, 0.5 * scale, px - scale, px - scale );
                        painter.setPen( QPen( p.line2, scale ) );
                        painter.setBrush( p.bg );
                        painter.drawRoundedRect( box, r, r );
                        // panel half and accent dot
                        painter.setPen( Qt::NoPen );
                        painter.setBrush( p.panel );
                        painter.drawRect( QRectF( box.left() + box.width() / 2, box.top() + scale, box.width() / 2 - scale, box.height() - 2 * scale ) );
                        painter.setBrush( p.accent );
                        painter.drawEllipse( box.center(), px * 0.22, px * 0.22 );
                        painter.end();
                        pm.setDevicePixelRatio( scale );
                        icon.addPixmap( pm );
                }
                return icon;
        }


        void ThemeSchemes::fillCombo( QComboBox* combo, const QString& currentId, Mode mode )
        {
                const QSignalBlocker blocker( combo );
                combo->clear();
                for ( const auto& scheme : all() )
                {
                        combo->addItem( swatch( scheme, mode ), scheme.displayName(), scheme.id );
                        combo->setItemData( combo->count() - 1, scheme.displayDescription(), Qt::ToolTipRole );
                }
                const int index = combo->findData( find( currentId ).id );
                combo->setCurrentIndex( index < 0 ? 0 : index );
        }

}
