//  ThemeIcons.cpp
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


#include "theme/ThemeIcons.hpp"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QtMath>


namespace
{
        using glabels::theme::IconRole;
        using glabels::theme::ThemeIcons;


        QString resourcePath( const QString& name )
        {
                return QStringLiteral( ":/icons/neo/%1.svg" ).arg( name );
        }


        ///
        /// SVG sources (kept for the life of the program; the set is small)
        ///
        QHash<QString, QByteArray>& sources()
        {
                static QHash<QString, QByteArray> cache;
                return cache;
        }


        ///
        /// Rendered images by (name, colour, pixel size)
        ///
        QHash<QString, QImage>& images()
        {
                static QHash<QString, QImage> cache;
                return cache;
        }


        QByteArray source( const QString& name )
        {
                auto& cache = sources();
                auto it = cache.constFind( name );
                if ( it != cache.constEnd() )
                {
                        return *it;
                }

                QByteArray data;
                QFile file( resourcePath( name ) );
                if ( file.open( QIODevice::ReadOnly ) )
                {
                        data = file.readAll();
                }
                cache.insert( name, data );
                return data;
        }


        QString cacheDir()
        {
                QString base = QStandardPaths::writableLocation( QStandardPaths::CacheLocation );
                if ( base.isEmpty() )
                {
                        base = QDir::tempPath() + QStringLiteral( "/glabels-neo" );
                }
                QString dir = base + QStringLiteral( "/theme-icons" );
                QDir().mkpath( dir );
                return dir;
        }


        QString colorKey( const QColor& color )
        {
                return color.name( QColor::HexArgb ).mid( 1 );
        }


        QString writeOnce( const QString& fileName, const QByteArray& data )
        {
                QString path = cacheDir() + QLatin1Char( '/' ) + fileName;
                QFile existing( path );
                if ( existing.open( QIODevice::ReadOnly ) && existing.readAll() == data )
                {
                        return path;
                }
                existing.close();

                QSaveFile file( path );
                if ( file.open( QIODevice::WriteOnly ) )
                {
                        file.write( data );
                        file.commit();
                }
                return path;
        }


        ///
        /// Icon engine that resolves its colour from the current theme every time it draws
        ///
        class NeoIconEngine : public QIconEngine
        {
        public:
                NeoIconEngine( const QString& name, IconRole role )
                        : mName( name ), mRole( role )
                {
                }

                QIconEngine* clone() const override
                {
                        return new NeoIconEngine( mName, mRole );
                }

                QString key() const override
                {
                        return QStringLiteral( "glabels-neo" );
                }

                QString iconName() override
                {
                        return mName;
                }

                bool isNull() override
                {
                        return !ThemeIcons::exists( mName );
                }

                QSize actualSize( const QSize& size, QIcon::Mode, QIcon::State ) override
                {
                        return size;
                }

                QList<QSize> availableSizes( QIcon::Mode, QIcon::State ) override
                {
                        return { QSize( 16, 16 ), QSize( 20, 20 ), QSize( 24, 24 ), QSize( 30, 30 ), QSize( 32, 32 ) };
                }

                void paint( QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state ) override
                {
                        qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
                        QPixmap pm = scaledPixmap( rect.size(), mode, state, dpr );
                        painter->drawPixmap( rect, pm );
                }

                QPixmap pixmap( const QSize& size, QIcon::Mode mode, QIcon::State state ) override
                {
                        return scaledPixmap( size, mode, state, 1.0 );
                }

                QPixmap scaledPixmap( const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale ) override
                {
                        QSize px( qCeil( size.width() * scale ), qCeil( size.height() * scale ) );
                        QImage image = ThemeIcons::render( mName, ThemeIcons::colorFor( mRole, mode, state ), px );
                        QPixmap pm = QPixmap::fromImage( image );
                        pm.setDevicePixelRatio( scale );
                        return pm;
                }

        private:
                QString  mName;
                IconRole mRole;
        };


        ///
        /// Icon glyph on a rounded tint chip (palette resolved when drawn)
        ///
        class ChipIconEngine : public QIconEngine
        {
        public:
                explicit ChipIconEngine( const QString& name ) : mName( name ) {}

                QIconEngine* clone() const override { return new ChipIconEngine( mName ); }
                QString key() const override { return QStringLiteral( "glabels-neo-chip" ); }
                QSize actualSize( const QSize& size, QIcon::Mode, QIcon::State ) override { return size; }

                void paint( QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state ) override
                {
                        qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
                        painter->drawPixmap( rect, scaledPixmap( rect.size(), mode, state, dpr ) );
                }

                QPixmap pixmap( const QSize& size, QIcon::Mode mode, QIcon::State state ) override
                {
                        return scaledPixmap( size, mode, state, 1.0 );
                }

                QPixmap scaledPixmap( const QSize& size, QIcon::Mode mode, QIcon::State, qreal scale ) override
                {
                        const auto& p = glabels::theme::Theme::instance().palette();
                        const int side = qMin( size.width(), size.height() );
                        const int px = qCeil( side * scale );
                        QImage image( px, px, QImage::Format_ARGB32_Premultiplied );
                        image.fill( Qt::transparent );
                        {
                                QPainter painter( &image );
                                painter.setRenderHint( QPainter::Antialiasing );
                                if ( mode == QIcon::Disabled )
                                {
                                        painter.setOpacity( 0.4 );
                                }
                                painter.setPen( Qt::NoPen );
                                painter.setBrush( p.tint );
                                painter.drawRoundedRect( QRectF( 0, 0, px, px ), px * 0.22, px * 0.22 );
                                const int glyph = qRound( px * 0.5 );
                                QImage g = ThemeIcons::render( mName, p.accent, QSize( glyph, glyph ) );
                                painter.drawImage( QPointF( ( px - glyph ) / 2.0, ( px - glyph ) / 2.0 ), g );
                        }
                        QPixmap pm = QPixmap::fromImage( image );
                        pm.setDevicePixelRatio( scale );
                        return pm;
                }

        private:
                QString mName;
        };
}


namespace glabels::theme
{

        ///
        /// Tinted icon
        ///
        QIcon ThemeIcons::icon( const QString& name, IconRole role )
        {
                if ( !exists( name ) )
                {
                        return QIcon();
                }
                return QIcon( new NeoIconEngine( name, role ) );
        }


        ///
        /// Chip icon
        ///
        QIcon ThemeIcons::chipIcon( const QString& name )
        {
                if ( !exists( name ) )
                {
                        return QIcon();
                }
                return QIcon( new ChipIconEngine( name ) );
        }


        ///
        /// Is the icon in the set?
        ///
        bool ThemeIcons::exists( const QString& name )
        {
                return !source( name ).isEmpty();
        }


        ///
        /// SVG with an explicit colour
        ///
        QByteArray ThemeIcons::recolouredSvg( const QString& name, const QColor& color )
        {
                QByteArray data = source( name );
                if ( data.isEmpty() )
                {
                        return data;
                }
                data.replace( "currentColor", color.name( QColor::HexRgb ).toLatin1() );
                return data;
        }


        ///
        /// Render at an exact pixel size
        ///
        QImage ThemeIcons::render( const QString& name, const QColor& color, const QSize& pixelSize )
        {
                if ( pixelSize.isEmpty() )
                {
                        return QImage();
                }

                QString key = name + QLatin1Char( '|' ) + colorKey( color ) + QLatin1Char( '|' ) +
                        QString::number( pixelSize.width() ) + QLatin1Char( 'x' ) + QString::number( pixelSize.height() );

                auto& cache = images();
                auto it = cache.constFind( key );
                if ( it != cache.constEnd() )
                {
                        return *it;
                }

                QImage image( pixelSize, QImage::Format_ARGB32_Premultiplied );
                image.fill( Qt::transparent );

                QSvgRenderer renderer( recolouredSvg( name, color ) );
                if ( renderer.isValid() )
                {
                        // Keep the 24 px design square and centred in a non-square request.
                        int side = qMin( pixelSize.width(), pixelSize.height() );
                        QRectF target( ( pixelSize.width() - side ) / 2.0, ( pixelSize.height() - side ) / 2.0, side, side );

                        {
                                QPainter painter( &image );
                                painter.setRenderHint( QPainter::Antialiasing );
                                renderer.render( &painter, target );
                        }

                        // Apply the colour's alpha once to the whole glyph, so that overlapping
                        // strokes do not come out darker than the rest.
                        if ( color.alpha() < 255 )
                        {
                                QImage faded( pixelSize, QImage::Format_ARGB32_Premultiplied );
                                faded.fill( Qt::transparent );
                                QPainter painter( &faded );
                                painter.setOpacity( color.alphaF() );
                                painter.drawImage( 0, 0, image );
                                painter.end();
                                image = faded;
                        }
                }

                cache.insert( key, image );
                return image;
        }


        ///
        /// Colour by role, mode and state
        ///
        QColor ThemeIcons::colorFor( IconRole role, QIcon::Mode mode, QIcon::State state )
        {
                const Palette& palette = Theme::instance().palette();

                QColor color = Theme::iconColor( palette, role );

                if ( mode == QIcon::Disabled || role == IconRole::Disabled )
                {
                        QColor base = ( role == IconRole::Disabled ) ? palette.ink : color;
                        base.setAlphaF( 0.4 );
                        return base;
                }

                if ( role == IconRole::Normal &&
                     ( mode == QIcon::Active || mode == QIcon::Selected || state == QIcon::On ) )
                {
                        return palette.accent;
                }

                return color;
        }


        ///
        /// Recoloured SVG file for stylesheets
        ///
        QString ThemeIcons::file( const QString& name, const QColor& color )
        {
                QByteArray data = recolouredSvg( name, color );
                if ( data.isEmpty() )
                {
                        return QString();
                }
                if ( color.alpha() < 255 )
                {
                        data.replace( "<svg", QByteArray( "<svg opacity=\"" ) +
                                      QByteArray::number( color.alphaF(), 'f', 2 ) + "\"" );
                }
                return writeOnce( name + QLatin1Char( '-' ) + colorKey( color ) + QStringLiteral( ".svg" ), data );
        }


        ///
        /// Generated radio indicator
        ///
        QString ThemeIcons::radioFile( const QColor& ring, const QColor& fill, const QColor& dot )
        {
                auto rgb = []( const QColor& c ) { return c.name( QColor::HexRgb ).toLatin1(); };
                auto alpha = []( const QColor& c ) { return QByteArray::number( c.alphaF(), 'f', 2 ); };

                QByteArray data =
                        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\" viewBox=\"0 0 16 16\">"
                        "<circle cx=\"8\" cy=\"8\" r=\"7.25\" fill=\"" + rgb( fill ) + "\" fill-opacity=\"" + alpha( fill ) +
                        "\" stroke=\"" + rgb( ring ) + "\" stroke-opacity=\"" + alpha( ring ) + "\" stroke-width=\"1.5\"/>";
                if ( dot.alpha() > 0 )
                {
                        data += "<circle cx=\"8\" cy=\"8\" r=\"3.5\" fill=\"" + rgb( dot ) + "\" fill-opacity=\"" + alpha( dot ) + "\"/>";
                }
                data += "</svg>";

                return writeOnce( QStringLiteral( "radio-%1-%2-%3.svg" )
                                  .arg( colorKey( ring ), colorKey( fill ), colorKey( dot ) ), data );
        }


        ///
        /// Illustration source with palette colours
        ///
        QByteArray ThemeIcons::illustrationSvg( const QString& name, const Palette& palette )
        {
                QFile file( QStringLiteral( ":/icons/neo-art/%1.svg" ).arg( name ) );
                if ( !file.open( QIODevice::ReadOnly ) )
                {
                        return QByteArray();
                }
                QByteArray data = file.readAll();
                const struct { const char* sentinel; QColor color; } map[] = {
                        { "#010203", palette.ink2 },
                        { "#040506", palette.line2 },
                        { "#070809", palette.accent },
                        { "#0A0B0C", palette.panel },
                        { "#101112", palette.tint },
                };
                for ( const auto& m : map )
                {
                        const QByteArray hex = m.color.name( QColor::HexRgb ).toLatin1();
                        data.replace( m.sentinel, hex );
                        data.replace( QByteArray( m.sentinel ).toLower(), hex );
                }
                return data;
        }


        ///
        /// Illustration in the current palette
        ///
        QPixmap ThemeIcons::illustration( const QString& name, const QSize& size, qreal devicePixelRatio )
        {
                const Palette& palette = Theme::instance().palette();
                QSvgRenderer renderer( illustrationSvg( name, palette ) );
                if ( !renderer.isValid() || size.isEmpty() )
                {
                        return QPixmap();
                }

                QSizeF art = renderer.defaultSize();
                art.scale( size, Qt::KeepAspectRatio );
                const QSize px( qCeil( art.width() * devicePixelRatio ), qCeil( art.height() * devicePixelRatio ) );

                QImage image( px, QImage::Format_ARGB32_Premultiplied );
                image.fill( Qt::transparent );
                {
                        QPainter painter( &image );
                        painter.setRenderHints( QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform );
                        renderer.render( &painter, QRectF( 0, 0, px.width(), px.height() ) );
                }
                QPixmap pixmap = QPixmap::fromImage( image );
                pixmap.setDevicePixelRatio( devicePixelRatio );
                return pixmap;
        }


        ///
        /// Brand mark
        ///
        QPixmap ThemeIcons::brandMark( int size, qreal devicePixelRatio )
        {
                int px = qCeil( size * devicePixelRatio );
                QImage image( px, px, QImage::Format_ARGB32_Premultiplied );
                image.fill( Qt::transparent );

                QSvgRenderer renderer( QStringLiteral( ":/brand/mark.svg" ) );
                if ( renderer.isValid() )
                {
                        QPainter painter( &image );
                        painter.setRenderHint( QPainter::Antialiasing );
                        renderer.render( &painter, QRectF( 0, 0, px, px ) );
                }

                QPixmap pixmap = QPixmap::fromImage( image );
                pixmap.setDevicePixelRatio( devicePixelRatio );
                return pixmap;
        }


        ///
        /// Drop rendered pixmaps
        ///
        void ThemeIcons::invalidate()
        {
                images().clear();
        }

}
