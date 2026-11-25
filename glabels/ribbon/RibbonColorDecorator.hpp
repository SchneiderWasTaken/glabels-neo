//  RibbonColorDecorator.hpp
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

#ifndef ribbon_RibbonColorDecorator_hpp
#define ribbon_RibbonColorDecorator_hpp


#include <QColor>
#include <QString>


namespace glabels
{
        class ColorButton;
}


namespace glabels::ribbon
{

        ///
        /// Installs a lightweight paint filter on an existing ColorButton so it renders as a
        /// compact Neo ribbon color drop button ([swatch] Name v) without modifying ColorButton.*.
        ///
        void installRibbonColorStyle( ColorButton*   button,
                                      const QString& transparentLabel = QString() );

        ///
        /// Friendly short name for a QColor (e.g. "Black", "White", "Indigo", or "#RRGGBB").
        ///
        QString friendlyColorName( const QColor& color, const QString& transparentLabel = QString() );

}


#endif // ribbon_RibbonColorDecorator_hpp
