//  AppearanceModel.cpp
//
//  Copyright (C) 2026  Jaye Evins <evins@snaught.com>
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


#include "AppearanceModel.hpp"

#include "theme/Theme.hpp"

#include <QApplication>
#include <QDebug>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QStyleHints>


namespace glabels
{

        //
        // Static data
        //
        std::unique_ptr<AppearanceModel> AppearanceModel::mInstance;
        model::Settings::ColorScheme AppearanceModel::mColorScheme = model::Settings::LIGHT_COLOR_SCHEME;
        model::Settings::IconStyle AppearanceModel::mIconStyle = model::Settings::FLAT_ICON_STYLE;


        //
        // Constructor
        //
        AppearanceModel::AppearanceModel()
        {
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
                auto styleName = QApplication::style()->name();
                if ( ( styleName.compare( "windows", Qt::CaseInsensitive ) != 0 ) &&
                     ( styleName.compare( "macos",   Qt::CaseInsensitive ) != 0 ) &&
                     ( styleName.compare( "fusion",  Qt::CaseInsensitive ) != 0 ) )
                {
	                // Default to and hardcode to "fusion", if not one of the above
	                QApplication::setStyle( QStyleFactory::create( "Fusion" ) );
                }
#else
                // Hardcode style to "fusion"
                QApplication::setStyle( QStyleFactory::create( "Fusion" ) );
#endif

                setColorScheme( model::Settings::colorScheme(), model::Settings::iconStyle() );

                connect( model::Settings::instance(), SIGNAL(changed()),
                         this, SLOT(onSettingsChanged()) );

#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
                // While System is selected, follow the system scheme without a restart.
                connect( QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                         this, &AppearanceModel::onSystemColorSchemeChanged );
#endif
        }


        //
        // Initialize
        //
        void AppearanceModel::init()
        {
                if ( !mInstance )
                {
                        mInstance.reset( new AppearanceModel() );
                }
        }


        //
        // Get singleton instance
        //
        AppearanceModel* AppearanceModel::instance()
        {
                init();
                return mInstance.get();
        }


        //
        // Query current style
        //
        AppearanceModel::Style AppearanceModel::style()
        {
                switch ( mColorScheme )
                {
                case model::Settings::LIGHT_COLOR_SCHEME: return STYLE_LIGHT;
                case model::Settings::DARK_COLOR_SCHEME:  return STYLE_DARK;
                case model::Settings::SYSTEM_COLOR_SCHEME:
                        return theme::Theme::instance().mode() == theme::Mode::Dark ? STYLE_DARK : STYLE_LIGHT;
                default:
                        qWarning() << "Unknown color scheme: " << mColorScheme;
                        return STYLE_LIGHT;
                }
        }


        //
        // Do we support SYSTEM_COLOR_SCHEME?
        //
        bool AppearanceModel::supportsSystemColorScheme()
        {
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
                // QStyleHints reports the system scheme and can drop an override (6.8).
                return true;
#else
                return false;
#endif
        }


        //
        // Set Color Scheme
        //
        void AppearanceModel::setColorScheme( model::Settings::ColorScheme scheme,
                                              model::Settings::IconStyle iconStyle )
        {
                switch ( scheme )
                {
                case model::Settings::LIGHT_COLOR_SCHEME:  setLightColorScheme( iconStyle );  break;
                case model::Settings::DARK_COLOR_SCHEME:   setDarkColorScheme( iconStyle );   break;
                case model::Settings::SYSTEM_COLOR_SCHEME: setSystemColorScheme( iconStyle ); break;
                default:
                        qWarning() << "Unknown color scheme: " << scheme;
                        break;
                }
        }


        //
        // Set Light Color Scheme
        //
        void AppearanceModel::setLightColorScheme( model::Settings::IconStyle iconStyle )
        {
                mColorScheme = model::Settings::LIGHT_COLOR_SCHEME;
                mIconStyle = iconStyle;

                setIconTheme( theme::Mode::Light, iconStyle );

                applyTheme( theme::Mode::Light, true );
        }


        //
        // Set Dark Color Scheme
        //
        void AppearanceModel::setDarkColorScheme( model::Settings::IconStyle iconStyle )
        {
                mColorScheme = model::Settings::DARK_COLOR_SCHEME;
                mIconStyle = iconStyle;

                setIconTheme( theme::Mode::Dark, iconStyle );

                applyTheme( theme::Mode::Dark, true );
        }


        //
        // Set System Color Scheme
        //
        void AppearanceModel::setSystemColorScheme( model::Settings::IconStyle iconStyle )
        {
                if ( !supportsSystemColorScheme() )
                {
                        setLightColorScheme( iconStyle );
                        return;
                }

                mColorScheme = model::Settings::SYSTEM_COLOR_SCHEME;
                mIconStyle = iconStyle;

#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
                // Drop any Light/Dark override so that colorScheme() reports the system again.
                QGuiApplication::styleHints()->unsetColorScheme();
#endif
                auto mode = theme::Theme::resolveMode( model::Settings::SYSTEM_COLOR_SCHEME,
                                                       theme::Theme::systemPrefersDark() );

                setIconTheme( mode, iconStyle );
                applyTheme( mode, false );
        }


        //
        // Set icon theme for a resolved mode
        //
        void AppearanceModel::setIconTheme( theme::Mode mode, model::Settings::IconStyle iconStyle )
        {
                bool dark = ( mode == theme::Mode::Dark );
                switch ( iconStyle )
                {
                case model::Settings::FLAT_ICON_STYLE:
                        QIcon::setThemeName( dark ? "glabels-flat-dark" : "glabels-flat" );
                        break;
                case model::Settings::VECTOR_ICON_STYLE:
                        QIcon::setThemeName( dark ? "glabels-vector-dark" : "glabels-vector" );
                        break;
                default:
                        qWarning() << "Unknown icon style: " << iconStyle;
                        break;
                }
        }


        //
        // Apply the theme for a resolved mode
        //
        void AppearanceModel::applyTheme( theme::Mode mode, bool overrideScheme )
        {
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
                // Native (non-Fusion) styles take their colours from the scheme hint.
                auto styleName = QApplication::style()->name();
                if ( overrideScheme &&
                     !styleName.isEmpty() &&
                     styleName.compare( "fusion", Qt::CaseInsensitive ) != 0 )
                {
                        QGuiApplication::styleHints()->setColorScheme( mode == theme::Mode::Dark ? Qt::ColorScheme::Dark
                                                                                                  : Qt::ColorScheme::Light );
                }
#else
                Q_UNUSED( overrideScheme )
#endif

                auto& th = theme::Theme::instance();
                th.setMode( mode );
                th.applyToApplication();      // palette, font, stylesheet (re-polishes every widget)

                if ( mInstance ) emit mInstance->changed();
        }


        //
        // Handle Settings "changed" signal
        //
        void AppearanceModel::onSettingsChanged()
        {

                auto newScheme = model::Settings::colorScheme();
                auto newIconStyle = model::Settings::iconStyle();
                if ( (newScheme != mColorScheme) || (newIconStyle != mIconStyle) )
                {
                        setColorScheme( newScheme, newIconStyle );
                }
        }


        //
        // Handle the system colour scheme changing (only matters while System is selected)
        //
        void AppearanceModel::onSystemColorSchemeChanged()
        {
                if ( mColorScheme != model::Settings::SYSTEM_COLOR_SCHEME )
                {
                        return;
                }

                auto mode = theme::Theme::resolveMode( model::Settings::SYSTEM_COLOR_SCHEME,
                                                       theme::Theme::systemPrefersDark() );
                if ( mode != theme::Theme::instance().mode() )
                {
                        setSystemColorScheme( mIconStyle );
                }
        }

}
