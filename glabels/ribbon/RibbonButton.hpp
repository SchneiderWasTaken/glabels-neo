//  RibbonButton.hpp
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

#ifndef ribbon_RibbonButton_hpp
#define ribbon_RibbonButton_hpp


#include <QAbstractButton>
#include <QAction>
#include <QString>


namespace glabels::ribbon
{

        ///
        /// Ribbon button built from a QAction (or standalone text/icon).
        /// Supports Large (58x78), Small (height 26), and IconOnly (28x28) sizes,
        /// plus Standard and Primary (gradient) visual variants.
        ///
        class RibbonButton : public QAbstractButton
        {
                Q_OBJECT

        public:
                enum class Size
                {
                        Large,
                        Small,
                        IconOnly
                };

                enum class Variant
                {
                        Standard,
                        Primary
                };

        public:
                explicit RibbonButton( Size     size    = Size::Small,
                                       Variant  variant = Variant::Standard,
                                       QWidget* parent  = nullptr );

                RibbonButton( QAction*       action,
                              const QString& iconName,
                              Size           size    = Size::Small,
                              Variant        variant = Variant::Standard,
                              QWidget*       parent  = nullptr );

                void     setAction( QAction* action );
                QAction* action() const;

                void    setIconName( const QString& iconName );
                QString iconName() const;

                void    setLabelOverride( const QString& label );
                QString displayLabel() const;

                Size    buttonSize() const;
                Variant variant() const;

                QSize sizeHint() const override;
                QSize minimumSizeHint() const override;

        protected:
                void paintEvent( QPaintEvent* event ) override;
                void enterEvent( QEnterEvent* event ) override;
                void leaveEvent( QEvent* event ) override;
                void keyPressEvent( QKeyEvent* event ) override;

        private slots:
                void syncFromAction();
                void onClicked();
                void onThemeChanged();

        protected:
                virtual bool hasChevron() const;
                void updateRoleProperty();

                QAction* mAction{ nullptr };
                QString  mIconName;
                QString  mLabelOverride;
                Size     mSize;
                Variant  mVariant;
                bool     mHovered{ false };
        };

}


#endif // ribbon_RibbonButton_hpp
