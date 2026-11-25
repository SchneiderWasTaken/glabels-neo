//  RibbonField.hpp
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

#ifndef ribbon_RibbonField_hpp
#define ribbon_RibbonField_hpp


#include <QLabel>
#include <QString>
#include <QWidget>


namespace glabels::ribbon
{

        ///
        /// Label + compact control wrapper for ribbon fields (QComboBox, QDoubleSpinBox,
        /// QSpinBox, QLineEdit, ColorButton, BarcodeMenuButton).
        /// Re-parents an existing widget (SPEC 3.4) and enforces height = 26 px.
        ///
        class RibbonField : public QWidget
        {
                Q_OBJECT

        public:
                enum class LabelPosition
                {
                        Left,
                        Above,
                        None
                };

        public:
                explicit RibbonField( const QString& label,
                                      QWidget*       control,
                                      int            controlWidth  = 0,
                                      LabelPosition  labelPosition = LabelPosition::Left,
                                      int            labelWidth    = 0,
                                      QWidget*       parent        = nullptr );

                QLabel*  labelWidget() const;
                QWidget* controlWidget() const;

        private slots:
                void onThemeChanged();

        private:
                QLabel*  mLabel{ nullptr };
                QWidget* mControl{ nullptr };
        };

}


#endif // ribbon_RibbonField_hpp
