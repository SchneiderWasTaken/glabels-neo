//  RibbonButton.cpp
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


#include "RibbonButton.hpp"

#include "RibbonTypes.hpp"
#include "theme/Theme.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QStatusTipEvent>


namespace glabels::ribbon
{

        RibbonButton::RibbonButton( Size size, Variant variant, QWidget* parent )
                : QAbstractButton( parent )
                , mSize( size )
                , mVariant( variant )
        {
                setFocusPolicy( Qt::TabFocus );
                setAttribute( Qt::WA_Hover, true );
                setCursor( Qt::PointingHandCursor );

                updateRoleProperty();

                connect( this, &QAbstractButton::clicked, this, &RibbonButton::onClicked );
                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &RibbonButton::onThemeChanged );
        }


        RibbonButton::RibbonButton( QAction*       action,
                                    const QString& iconName,
                                    Size           size,
                                    Variant        variant,
                                    QWidget*       parent )
                : RibbonButton( size, variant, parent )
        {
                mIconName = iconName;
                setAction( action );
        }


        void RibbonButton::setAction( QAction* action )
        {
                if ( mAction )
                {
                        disconnect( mAction, nullptr, this, nullptr );
                }
                mAction = action;
                if ( mAction )
                {
                        connect( mAction, &QAction::changed, this, &RibbonButton::syncFromAction );
                        syncFromAction();
                }
        }


        QAction* RibbonButton::action() const
        {
                return mAction;
        }


        void RibbonButton::setIconName( const QString& iconName )
        {
                mIconName = iconName;
                update();
        }


        QString RibbonButton::iconName() const
        {
                return mIconName;
        }


        void RibbonButton::setLabelOverride( const QString& label )
        {
                mLabelOverride = label;
                updateGeometry();
                update();
        }


        QString RibbonButton::displayLabel() const
        {
                if ( !mLabelOverride.isEmpty() )
                {
                        return mLabelOverride;
                }
                if ( mAction )
                {
                        return stripMnemonic( mAction->text() );
                }
                return stripMnemonic( text() );
        }


        RibbonButton::Size RibbonButton::buttonSize() const
        {
                return mSize;
        }


        RibbonButton::Variant RibbonButton::variant() const
        {
                return mVariant;
        }


        void RibbonButton::updateRoleProperty()
        {
                if ( mVariant == Variant::Primary )
                {
                        setProperty( "role", QStringLiteral( "ribbonPrimary" ) );
                }
                else
                {
                        switch ( mSize )
                        {
                        case Size::Large:    setProperty( "role", QStringLiteral( "ribbonLarge" ) ); break;
                        case Size::Small:    setProperty( "role", QStringLiteral( "ribbonSmall" ) ); break;
                        case Size::IconOnly: setProperty( "role", QStringLiteral( "ribbonIcon" ) );  break;
                        }
                }
        }


        bool RibbonButton::hasChevron() const
        {
                return false;
        }


        void RibbonButton::syncFromAction()
        {
                if ( !mAction )
                {
                        return;
                }

                const QString clean = stripMnemonic( mAction->text() );
                setText( clean );
                setCheckable( mAction->isCheckable() );
                setChecked( mAction->isChecked() );
                setEnabled( mAction->isEnabled() );
                setVisible( mAction->isVisible() );
                setStatusTip( mAction->statusTip() );
                setToolTip( formatActionTooltip( mAction ) );
                setAccessibleName( clean );
                setAccessibleDescription( mAction->statusTip() );

                updateGeometry();
                update();
        }


        void RibbonButton::onClicked()
        {
                if ( mAction && mAction->isEnabled() )
                {
                        mAction->trigger();
                }
        }


        void RibbonButton::onThemeChanged()
        {
                update();
        }


        QSize RibbonButton::sizeHint() const
        {
                const auto& th = theme::Theme::instance();
                const QFontMetrics fm( th.uiFont( 12, 400 ) );

                switch ( mSize )
                {
                case Size::Large:
                {
                        const QString lbl = displayLabel();
                        int w = theme::Metrics::largeW;
                        // Allow slight expansion if a single word is wider than 54px
                        for ( const QString& word : lbl.split( QLatin1Char( ' ' ), Qt::SkipEmptyParts ) )
                        {
                                w = qMax( w, fm.horizontalAdvance( word ) + 10 );
                        }
                        w = qMin( w, 84 );
                        return QSize( w, theme::Metrics::largeH );
                }

                case Size::Small:
                {
                        const int textW = fm.horizontalAdvance( displayLabel() );
                        const int chevW = hasChevron() ? 16 : 0;
                        // 8 left pad + 16 icon + 8 gap + text + 10 right pad
                        const int w = qMin( 8 + 16 + 8 + textW + chevW + 10, 170 );
                        return QSize( w, theme::Metrics::smallH );
                }

                case Size::IconOnly:
                        return QSize( theme::Metrics::iconBtn, theme::Metrics::iconBtn );
                }

                return QSize( theme::Metrics::iconBtn, theme::Metrics::iconBtn );
        }


        QSize RibbonButton::minimumSizeHint() const
        {
                return sizeHint();
        }


        void RibbonButton::enterEvent( QEnterEvent* event )
        {
                mHovered = true;
                if ( mAction && !mAction->statusTip().isEmpty() )
                {
                        QStatusTipEvent tipEvent( mAction->statusTip() );
                        QApplication::sendEvent( window(), &tipEvent );
                }
                update();
                QAbstractButton::enterEvent( event );
        }


        void RibbonButton::leaveEvent( QEvent* event )
        {
                mHovered = false;
                if ( mAction && !mAction->statusTip().isEmpty() )
                {
                        QStatusTipEvent tipEvent{ QString() };
                        QApplication::sendEvent( window(), &tipEvent );
                }
                update();
                QAbstractButton::leaveEvent( event );
        }


        void RibbonButton::keyPressEvent( QKeyEvent* event )
        {
                if ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter )
                {
                        animateClick();
                        event->accept();
                        return;
                }
                QAbstractButton::keyPressEvent( event );
        }


        void RibbonButton::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                p.setRenderHint( QPainter::Antialiasing, true );

                const auto& th  = theme::Theme::instance();
                const auto& pal = th.palette();

                const bool enabled = isEnabled();
                if ( !enabled )
                {
                        p.setOpacity( 0.40 );
                }

                const qreal radius = ( mSize == Size::Large ) ? 8.0 : 6.0;
                const QRectF r = QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );

                // Background fill
                if ( mVariant == Variant::Primary )
                {
                        QLinearGradient grad( r.topLeft(), r.bottomRight() );
                        grad.setColorAt( 0.0, pal.gradA );
                        grad.setColorAt( 0.5, pal.gradB );
                        grad.setColorAt( 1.0, pal.gradC );
                        p.setPen( Qt::NoPen );
                        p.setBrush( grad );
                        p.drawRoundedRect( r, radius, radius );
                        if ( enabled && ( isDown() || mHovered ) )
                        {
                                p.setBrush( isDown() ? QColor( 0, 0, 0, 36 ) : QColor( 255, 255, 255, 22 ) );
                                p.drawRoundedRect( r, radius, radius );
                        }
                }
                else if ( enabled && ( isDown() || isChecked() ) )
                {
                        p.setPen( Qt::NoPen );
                        p.setBrush( pal.pressed );
                        p.drawRoundedRect( r, radius, radius );
                }
                else if ( enabled && mHovered )
                {
                        p.setPen( Qt::NoPen );
                        p.setBrush( pal.hover );
                        p.drawRoundedRect( r, radius, radius );
                }

                // Focus ring (SPEC 4.1: 2px solid accent)
                if ( hasFocus() && enabled )
                {
                        QPen focusPen( pal.accent, 2.0 );
                        p.setPen( focusPen );
                        p.setBrush( Qt::NoBrush );
                        p.drawRoundedRect( QRectF( rect() ).adjusted( 1.5, 1.5, -1.5, -1.5 ), radius, radius );
                }

                // Resolve icon
                theme::IconRole iconRole = theme::IconRole::Normal;
                if ( mVariant == Variant::Primary )
                {
                        iconRole = theme::IconRole::OnGradient;
                }
                else if ( !enabled )
                {
                        iconRole = theme::IconRole::Disabled;
                }
                else if ( mSize == Size::Large || isChecked() )
                {
                        iconRole = theme::IconRole::Accent;
                }

                QIcon ic;
                if ( !mIconName.isEmpty() )
                {
                        ic = th.icon( mIconName, iconRole );
                }
                if ( ic.isNull() && mAction )
                {
                        ic = mAction->icon();
                }
                if ( ic.isNull() )
                {
                        ic = icon();
                }

                const QColor textColor = ( mVariant == Variant::Primary )
                                         ? pal.onGrad
                                         : ( ( isChecked() && mSize == Size::IconOnly ) ? pal.accent : pal.ink );

                p.setFont( th.uiFont( 12, ( mVariant == Variant::Primary ) ? 600 : 400 ) );
                p.setPen( textColor );

                switch ( mSize )
                {
                case Size::Large:
                {
                        const QString lbl = displayLabel();
                        const bool chev = hasChevron();
                        const bool multiLine = lbl.contains( QLatin1Char( ' ' ) ) &&
                                               QFontMetrics( p.font() ).horizontalAdvance( lbl ) > width() - 6;
                        const int iconSz = chev ? ( multiLine ? 24 : 28 ) : 30;
                        const int iconY  = chev ? ( multiLine ? 4 : 6 ) : 8;
                        const QRect iconRect( ( width() - iconSz ) / 2, iconY, iconSz, iconSz );

                        if ( mIconName == QStringLiteral( "shadow" ) )
                        {
                                // Drawn 2-square "shadow" glyph (SPEC 4.4 & s04 mockup)
                                p.save();
                                const QColor glyphColor = ( mVariant == Variant::Primary )
                                                          ? pal.onGrad
                                                          : ( enabled ? pal.accent : pal.ink3 );
                                QColor backFill = glyphColor;
                                backFill.setAlpha( 48 );
                                QColor backStroke = glyphColor;
                                backStroke.setAlpha( 140 );

                                const qreal s = iconSz * 0.54;
                                const qreal off = iconSz * 0.18;
                                const QRectF frontR( iconRect.left() + iconSz * 0.14,
                                                     iconRect.top() + iconSz * 0.14, s, s );
                                const QRectF backR = frontR.translated( off, off );

                                p.setPen( QPen( backStroke, 1.5 ) );
                                p.setBrush( backFill );
                                p.drawRoundedRect( backR, 3.0, 3.0 );

                                p.setPen( QPen( glyphColor, 1.8 ) );
                                p.setBrush( pal.ribbon );
                                p.drawRoundedRect( frontR, 3.0, 3.0 );
                                p.restore();
                        }
                        else if ( !ic.isNull() )
                        {
                                ic.paint( &p, iconRect, Qt::AlignCenter,
                                          enabled ? QIcon::Normal : QIcon::Disabled,
                                          isChecked() ? QIcon::On : QIcon::Off );
                        }

                        const int textY = chev ? ( multiLine ? 31 : 38 ) : 42;
                        const int textH = chev ? ( height() - textY - 10 ) : ( height() - textY - 2 );
                        const QRect textRect( 2, textY, width() - 4, textH );
                        p.drawText( textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, lbl );

                        if ( chev )
                        {
                                QPainterPath cp;
                                const qreal cx = width() / 2.0;
                                const qreal cy = height() - 5.5;
                                cp.moveTo( cx - 3.0, cy - 1.5 );
                                cp.lineTo( cx, cy + 1.5 );
                                cp.lineTo( cx + 3.0, cy - 1.5 );
                                p.setPen( QPen( mVariant == Variant::Primary ? pal.onGrad : pal.ink3, 1.5,
                                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
                                p.setBrush( Qt::NoBrush );
                                p.drawPath( cp );
                        }
                        break;
                }

                case Size::Small:
                {
                        const int iconSz = 16;
                        const int iconY  = ( height() - iconSz ) / 2;
                        const QRect iconRect( 8, iconY, iconSz, iconSz );
                        if ( !ic.isNull() )
                        {
                                ic.paint( &p, iconRect, Qt::AlignCenter,
                                          enabled ? QIcon::Normal : QIcon::Disabled,
                                          isChecked() ? QIcon::On : QIcon::Off );
                        }

                        const int rightPad = hasChevron() ? 22 : 8;
                        const QRect textRect( 32, 0, qMax( 0, width() - 32 - rightPad ), height() );
                        const QFontMetrics fm( p.font() );
                        const QString elided = fm.elidedText( displayLabel(), Qt::ElideRight, textRect.width() );
                        p.drawText( textRect, Qt::AlignVCenter | Qt::AlignLeft, elided );

                        if ( hasChevron() )
                        {
                                QPainterPath chev;
                                const qreal cx = width() - 12.0;
                                const qreal cy = height() / 2.0;
                                chev.moveTo( cx - 3.5, cy - 1.5 );
                                chev.lineTo( cx, cy + 2.0 );
                                chev.lineTo( cx + 3.5, cy - 1.5 );
                                p.setPen( QPen( mVariant == Variant::Primary ? pal.onGrad : pal.ink3, 1.5,
                                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
                                p.setBrush( Qt::NoBrush );
                                p.drawPath( chev );
                        }
                        break;
                }

                case Size::IconOnly:
                {
                        const int iconSz = 16;
                        const QRect iconRect( ( width() - iconSz ) / 2, ( height() - iconSz ) / 2, iconSz, iconSz );
                        if ( !ic.isNull() )
                        {
                                ic.paint( &p, iconRect, Qt::AlignCenter,
                                          enabled ? QIcon::Normal : QIcon::Disabled,
                                          isChecked() ? QIcon::On : QIcon::Off );
                        }
                        break;
                }
                }
        }

} // namespace glabels::ribbon
