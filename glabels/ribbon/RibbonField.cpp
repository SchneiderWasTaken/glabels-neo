//  RibbonField.cpp
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


#include "RibbonField.hpp"

#include "theme/Theme.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>


namespace glabels::ribbon
{

        RibbonField::RibbonField( const QString& label,
                                  QWidget*       control,
                                  int            controlWidth,
                                  LabelPosition  labelPosition,
                                  int            labelWidth,
                                  QWidget*       parent )
                : QWidget( parent )
                , mControl( control )
        {
                QBoxLayout* layout = nullptr;
                if ( labelPosition == LabelPosition::Above )
                {
                        layout = new QVBoxLayout( this );
                        layout->setSpacing( 3 );
                }
                else
                {
                        layout = new QHBoxLayout( this );
                        layout->setSpacing( 6 );
                }
                layout->setContentsMargins( 0, 0, 0, 0 );

                if ( labelPosition != LabelPosition::None && !label.isEmpty() )
                {
                        mLabel = new QLabel( label, this );
                        mLabel->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                        if ( labelWidth > 0 )
                        {
                                mLabel->setFixedWidth( labelWidth );
                        }
                        if ( mControl )
                        {
                                mLabel->setBuddy( mControl );
                        }
                        layout->addWidget( mLabel );
                }

                if ( mControl )
                {
                        mControl->setParent( this );
                        mControl->setProperty( "role", QStringLiteral( "ribbonField" ) );
                        mControl->setFixedHeight( theme::Metrics::field );
                        if ( controlWidth > 0 )
                        {
                                mControl->setFixedWidth( controlWidth );
                        }
                        mControl->show();
                        layout->addWidget( mControl );
                }

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &RibbonField::onThemeChanged );
                onThemeChanged();
        }


        QLabel* RibbonField::labelWidget() const
        {
                return mLabel;
        }


        QWidget* RibbonField::controlWidget() const
        {
                return mControl;
        }


        void RibbonField::onThemeChanged()
        {
                const auto& th = theme::Theme::instance();
                if ( mLabel )
                {
                        mLabel->setFont( th.uiFont( 12, 400 ) );
                        QPalette p = mLabel->palette();
                        p.setColor( QPalette::WindowText, th.palette().ink3 );
                        mLabel->setPalette( p );
                }
                if ( mControl )
                {
                        mControl->setFont( th.uiFont( 12, 400 ) );
                }
        }

} // namespace glabels::ribbon
