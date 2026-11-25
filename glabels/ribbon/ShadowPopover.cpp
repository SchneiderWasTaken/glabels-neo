//  ShadowPopover.cpp
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


#include "ShadowPopover.hpp"

#include "theme/Theme.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>


namespace glabels::ribbon
{

        ShadowPopover::ShadowPopover( const Controls& controls, QWidget* parent )
                : QWidget( parent, Qt::Popup | Qt::FramelessWindowHint )
                , mControls( controls )
        {
                setFixedWidth( 260 );
                setProperty( "role", QStringLiteral( "ribbonPopover" ) );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( 14, 14, 14, 14 );
                root->setSpacing( 10 );

                mEnableCheck = new QCheckBox( tr( "Enable shadow" ), this );
                root->addWidget( mEnableCheck );

                auto* form = new QFormLayout();
                form->setContentsMargins( 0, 0, 0, 0 );
                form->setHorizontalSpacing( 10 );
                form->setVerticalSpacing( 8 );

                auto addField = [&]( const QString& label, QWidget* w, int width )
                {
                        if ( !w )
                        {
                                return;
                        }
                        w->setParent( this );
                        w->setProperty( "role", QStringLiteral( "ribbonField" ) );
                        w->setFixedHeight( theme::Metrics::field );
                        if ( width > 0 )
                        {
                                w->setMinimumWidth( width );
                        }
                        w->show();

                        auto* lbl = new QLabel( label, this );
                        lbl->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                        lbl->setFixedWidth( 64 );
                        form->addRow( lbl, w );
                };

                addField( tr( "X offset" ), mControls.xSpin, 100 );
                addField( tr( "Y offset" ), mControls.ySpin, 100 );
                addField( tr( "Color" ),    mControls.colorButton, 110 );
                addField( tr( "Opacity" ),  mControls.opacitySpin, 100 );

                root->addLayout( form );

                connect( mEnableCheck, &QCheckBox::toggled,
                         this, &ShadowPopover::onEnableCheckToggled );
                if ( mControls.enableGroup )
                {
                        connect( mControls.enableGroup, &QGroupBox::toggled,
                                 this, &ShadowPopover::onGroupToggled );
                }

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &ShadowPopover::onThemeChanged );

                syncFromObjectEditor();
                onThemeChanged();
        }


        void ShadowPopover::syncFromObjectEditor()
        {
                if ( !mControls.enableGroup )
                {
                        return;
                }
                mSyncing = true;
                const bool on = mControls.enableGroup->isChecked();
                mEnableCheck->setChecked( on );
                updateControlsEnabled( on );
                mSyncing = false;
        }


        void ShadowPopover::onEnableCheckToggled( bool checked )
        {
                if ( mSyncing )
                {
                        return;
                }
                mSyncing = true;
                updateControlsEnabled( checked );
                if ( mControls.enableGroup )
                {
                        mControls.enableGroup->setChecked( checked );
                }
                mSyncing = false;
        }


        void ShadowPopover::onGroupToggled( bool checked )
        {
                if ( mSyncing )
                {
                        return;
                }
                mSyncing = true;
                mEnableCheck->setChecked( checked );
                updateControlsEnabled( checked );
                mSyncing = false;
        }


        void ShadowPopover::updateControlsEnabled( bool enabled )
        {
                if ( mControls.xSpin )       mControls.xSpin->setEnabled( enabled );
                if ( mControls.ySpin )       mControls.ySpin->setEnabled( enabled );
                if ( mControls.colorButton ) mControls.colorButton->setEnabled( enabled );
                if ( mControls.opacitySpin ) mControls.opacitySpin->setEnabled( enabled );
        }


        void ShadowPopover::showEvent( QShowEvent* event )
        {
                syncFromObjectEditor();
                QWidget::showEvent( event );
        }


        void ShadowPopover::onThemeChanged()
        {
                update();
        }


        void ShadowPopover::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                p.setRenderHint( QPainter::Antialiasing, true );
                const auto& pal = theme::Theme::instance().palette();
                p.setPen( QPen( pal.line2, 1.0 ) );
                p.setBrush( pal.panel );
                p.drawRoundedRect( QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 ), 10.0, 10.0 );
        }

} // namespace glabels::ribbon
