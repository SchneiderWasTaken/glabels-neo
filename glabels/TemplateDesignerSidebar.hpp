//  TemplateDesignerSidebar.hpp
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

#ifndef TemplateDesignerSidebar_hpp
#define TemplateDesignerSidebar_hpp


#include "TemplateDesignerSteps.hpp"

#include <QList>
#include <QWidget>


namespace glabels
{

        ///
        /// Left side bar of the Product Template Designer (mockups d03, d04): the steps in play
        /// with their state (done: check, current: accent chip with the number, todo: empty
        /// circle) and the designer illustration at the bottom.  Painted from the theme palette.
        ///
        class TemplateDesignerSidebar : public QWidget
        {
                Q_OBJECT

        public:
                explicit TemplateDesignerSidebar( QWidget* parent = nullptr );

                void setSteps( const QList<designer::StepItem>& steps );
                const QList<designer::StepItem>& steps() const { return mSteps; }

                QSize sizeHint() const override;
                QSize minimumSizeHint() const override;

        protected:
                void paintEvent( QPaintEvent* event ) override;

        private:
                QList<designer::StepItem> mSteps;
        };

}


#endif // TemplateDesignerSidebar_hpp
