//  TemplateDesignerSteps.cpp
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


#include "TemplateDesignerSteps.hpp"

#include <QCoreApplication>


namespace glabels::designer
{

        Step stepForPage( int pageId )
        {
                switch ( pageId )
                {
                case NamePageId:      return Step::Name;
                case PageSizePageId:  return Step::PageSize;
                case ShapePageId:     return Step::Shape;
                case RectPageId:
                case RoundPageId:
                case EllipsePageId:
                case CdPageId:
                case ThermalPageId:   return Step::Size;
                case NLayoutsPageId:
                case OneLayoutPageId:
                case TwoLayoutPageId: return Step::Layout;
                case ApplyPageId:     return Step::Save;
                case IntroPageId:
                case PathPageId:
                case ContinuousPageId:
                default:              return Step::Start;
                }
        }


        QList<StepItem> stepList( const QList<int>& visitedIds, int currentId, bool thermal )
        {
                const Step current = stepForPage( currentId );

                // The path is only known once the user has left the Start page.
                const bool onStart = ( current == Step::Start );
                QList<Step> steps;
                if ( thermal && !onStart )
                {
                        steps = { Step::Start, Step::Name, Step::Size, Step::Save };
                }
                else
                {
                        steps = { Step::Start, Step::Name, Step::PageSize, Step::Shape, Step::Size, Step::Layout, Step::Save };
                }

                // Editing an existing template starts on the Name page: there is no Start step.
                const bool startedOnIntro = visitedIds.isEmpty() ? ( currentId == IntroPageId )
                                                                 : ( visitedIds.first() == IntroPageId );
                if ( !startedOnIntro && !onStart )
                {
                        steps.removeAll( Step::Start );
                }

                QList<Step> done;
                for ( int id : visitedIds )
                {
                        if ( id != currentId )
                        {
                                done.append( stepForPage( id ) );
                        }
                }

                QList<StepItem> list;
                for ( Step step : steps )
                {
                        StepState state = StepState::Todo;
                        if ( step == current )
                        {
                                state = StepState::Current;
                        }
                        else if ( done.contains( step ) )
                        {
                                state = StepState::Done;
                        }
                        list.append( { step, state } );
                }
                return list;
        }


        QString stepTitle( Step step )
        {
                switch ( step )
                {
                case Step::Start:    return QCoreApplication::translate( "TemplateDesigner", "Start" );
                case Step::Name:     return QCoreApplication::translate( "TemplateDesigner", "Name" );
                case Step::PageSize: return QCoreApplication::translate( "TemplateDesigner", "Page size" );
                case Step::Shape:    return QCoreApplication::translate( "TemplateDesigner", "Shape" );
                case Step::Size:     return QCoreApplication::translate( "TemplateDesigner", "Size" );
                case Step::Layout:   return QCoreApplication::translate( "TemplateDesigner", "Layout" );
                case Step::Save:     return QCoreApplication::translate( "TemplateDesigner", "Save" );
                }
                return QString();
        }

}
