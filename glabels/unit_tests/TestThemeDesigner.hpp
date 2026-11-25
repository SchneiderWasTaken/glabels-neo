//  TestThemeDesigner.hpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels Neo.
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


#include <QtTest/QtTest>


///
/// Product Template Designer in the new design: the step list (pure logic, every branch), the
/// side bar following the real wizard (Next and Back), the themed illustrations, and an
/// offscreen capture of every page in Graphite light and dark (GLABELS_THEME_SCREENSHOTS).
///
class TestThemeDesigner : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();

        void stepForEveryPage();
        void stepListOnStart();
        void stepListSheetPath();
        void stepListRollSkipsNumberOfLayouts();
        void stepListAfterBack();
        void stepListThermalPath();
        void stepListStaleThermalFlagOnStart();
        void stepListEditing();
        void stepListUnsupportedCopy();

        void illustrationsFollowThePalette();
        void wizardWalk();
};
