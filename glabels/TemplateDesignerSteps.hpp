//  TemplateDesignerSteps.hpp
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

#ifndef TemplateDesignerSteps_hpp
#define TemplateDesignerSteps_hpp


#include <QList>
#include <QString>


namespace glabels::designer
{

        ///
        /// Page ids of the Product Template Designer wizard (TemplateDesigner.cpp)
        ///
        enum PageId
        {
                IntroPageId,
                NamePageId,
                PageSizePageId,
                ShapePageId,
                RectPageId,
                RoundPageId,
                EllipsePageId,
                CdPageId,
                PathPageId,
                ContinuousPageId,
                NLayoutsPageId,
                OneLayoutPageId,
                TwoLayoutPageId,
                ApplyPageId,
                ThermalPageId
        };


        ///
        /// Steps shown in the designer's side bar (mockups d03, d04)
        ///
        enum class Step { Start, Name, PageSize, Shape, Size, Layout, Save };

        enum class StepState { Done, Current, Todo };

        struct StepItem
        {
                Step      step;
                StepState state;

                bool operator==( const StepItem& other ) const { return step == other.step && state == other.state; }
        };


        /// The step a wizard page belongs to.  The unsupported-style pages (path, continuous)
        /// belong to Start: they only explain why the copy cannot be edited and lead back to it.
        Step stepForPage( int pageId );

        ///
        /// The steps in play and their state, from the wizard's own history.
        ///
        /// visitedIds: QWizard::visitedIds() (the pages on the way to the current one, current
        ///             last; QWizard drops pages from it when the user presses Back).
        /// currentId:  QWizard::currentId().
        /// thermal:    the "New thermal / roll label" path was chosen (Start, Name, Size, Save).
        ///
        /// Rules: on the Start page the path is not chosen yet, so all seven steps are shown.
        /// The thermal path shows Start, Name, Size, Save.  When the wizard did not start on the
        /// intro page (editing an existing template) Start is left out.  A step is Done when a
        /// page of it was visited before the current page, Current for the current page, Todo
        /// otherwise.
        ///
        QList<StepItem> stepList( const QList<int>& visitedIds, int currentId, bool thermal );

        /// Sentence-case title of a step ("Page size"), translated
        QString stepTitle( Step step );

}


#endif // TemplateDesignerSteps_hpp
