// TEMP STUB: replaced by feat/ribbon-ui-theme
#pragma once

#include "AppearanceModel.hpp"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QPalette>
#include <QString>

namespace glabels::theme
{
        enum class Mode     { Light, Dark };
        enum class IconRole { Normal, Accent, OnGradient, Disabled };

        struct Palette
        {
                QColor bg, chrome, ribbon, work, panel, line, line2;
                QColor ink, ink2, ink3, accent, tint, hover, pressed;
                QColor ctx, ctxLine, ok, okTint, warn;
                QColor gradA, gradB, gradC, onGrad;   // 135 degree gradient A > B > C
        };

        struct Metrics   // all in device-independent pixels, section 4.3 of SPEC.md
        {
                static constexpr int tabStrip = 42, tab = 34, tabActive = 36, ribbon = 118;
                static constexpr int largeW = 58, largeH = 78, smallH = 26, iconBtn = 28;
                static constexpr int field = 26, groupName = 22, groupPad = 8;
                static constexpr int contentPanel = 300, status = 28, ruler = 22;
        };

        class Theme : public QObject
        {
                Q_OBJECT
        public:
                static Theme& instance()
                {
                        static Theme sInstance;
                        return sInstance;
                }

                Mode mode() const
                {
                        return AppearanceModel::style() == AppearanceModel::STYLE_DARK
                               ? Mode::Dark
                               : Mode::Light;
                }

                const Palette& palette() const
                {
                        static const Palette light = {
                                QColor( "#F8FAFC" ), QColor( "#F1F5F9" ), QColor( "#FFFFFF" ), QColor( "#E2E8F0" ),
                                QColor( "#FFFFFF" ), QColor( "#E2E8F0" ), QColor( "#CBD5E1" ),
                                QColor( "#0F172A" ), QColor( "#475569" ), QColor( "#64748B" ), QColor( "#4F46E5" ),
                                QColor( "#EDE9FE" ), QColor( "#F1F5F9" ), QColor( "#E0E7FF" ),
                                QColor( "#EDE9FE" ), QColor( "#7C3AED" ), QColor( "#047857" ), QColor( "#ECFDF5" ),
                                QColor( "#B45309" ),
                                QColor( "#7C3AED" ), QColor( "#4F46E5" ), QColor( "#2563EB" ), QColor( "#FFFFFF" )
                        };
                        static const Palette dark = {
                                QColor( "#090D1A" ), QColor( "#0F172A" ), QColor( "#151E32" ), QColor( "#090D1A" ),
                                QColor( "#0F172A" ), QColor( "#1E293B" ), QColor( "#334155" ),
                                QColor( "#F8FAFC" ), QColor( "#CBD5E1" ), QColor( "#94A3B8" ), QColor( "#818CF8" ),
                                QColor( "#1E293B" ), QColor( "#1E293B" ), QColor( "#2B3556" ),
                                QColor( "#2A2447" ), QColor( "#A78BFA" ), QColor( "#34D399" ), QColor( "#064E3B" ),
                                QColor( "#FBBF24" ),
                                QColor( "#A78BFA" ), QColor( "#818CF8" ), QColor( "#60A5FA" ), QColor( "#090D1A" )
                        };
                        return mode() == Mode::Dark ? dark : light;
                }

                QFont uiFont( int px = 12, int weight = 400 ) const
                {
                        QFont f( QStringLiteral( "Segoe UI" ) );
                        f.setPixelSize( px );
                        f.setWeight( static_cast<QFont::Weight>( weight ) );
                        return f;
                }

                QFont monoFont( int px = 12 ) const
                {
                        QFont f( QStringLiteral( "Consolas" ) );
                        f.setStyleHint( QFont::Monospace );
                        f.setPixelSize( px );
                        return f;
                }

                QIcon icon( const QString& name, IconRole role = IconRole::Normal ) const
                {
                        Q_UNUSED( role );
                        QIcon ic = QIcon::fromTheme( name );
                        return ic;
                }

                QString styleSheet() const
                {
                        return QString();
                }

                void applyToApplication()
                {
                        emit changed();
                }

        signals:
                void changed();

        private:
                Theme()
                {
                        if ( auto* am = AppearanceModel::instance() )
                        {
                                connect( am, &AppearanceModel::changed, this, &Theme::changed );
                        }
                }
        };
}
