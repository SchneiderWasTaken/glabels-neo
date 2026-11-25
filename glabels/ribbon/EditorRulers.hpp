//  EditorRulers.hpp
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

#ifndef ribbon_EditorRulers_hpp
#define ribbon_EditorRulers_hpp


#include "model/Model.hpp"

#include <QScrollArea>
#include <QWidget>


namespace glabels
{
        class LabelEditor;
}


namespace glabels::ribbon
{

        ///
        /// Wraps the LabelEditor scroll area with 22px horizontal and vertical rulers
        /// (SPEC 4.3, mockup s03).
        ///
        class EditorRulerFrame : public QWidget
        {
                Q_OBJECT

        public:
                EditorRulerFrame( QScrollArea* scrollArea,
                                  LabelEditor* editor,
                                  QWidget*     parent = nullptr );

                void setModel( model::Model* model );

        public slots:
                void updateRulers();

        protected:
                bool eventFilter( QObject* watched, QEvent* event ) override;

        private:
                class RulerStrip;
                class CornerBox;

                QScrollArea*  mScrollArea{ nullptr };
                LabelEditor*  mEditor{ nullptr };
                model::Model* mModel{ nullptr };

                CornerBox*    mCorner{ nullptr };
                RulerStrip*   mHorizontalRuler{ nullptr };
                RulerStrip*   mVerticalRuler{ nullptr };
        };

}


#endif // ribbon_EditorRulers_hpp
