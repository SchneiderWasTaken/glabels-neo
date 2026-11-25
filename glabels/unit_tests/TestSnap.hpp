//  TestSnap.hpp
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

#ifndef TestSnap_hpp
#define TestSnap_hpp


#include <QtTest/QtTest>


class TestSnap : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();

        // Display-free SnapEngine tests
        void moveSnapsToObjectsCenterAndSafeArea();
        void resizeSnapsOnlyDraggedEdgesByEachHandle();
        void createDragAndClickCreate();
        void toleranceEdgesAndScale();
        void tieBreakingPriority();
        void disabledToggles();
        void multiSelectionBox();
        void zeroMargin();
        void rotatedAndLineBounds();

        // Offscreen LabelEditor + View tab integration tests (+ optional screenshots)
        void editorMoveResizeCreateAndAltBypass();
        void viewTabSnapGroupAndSettingsSync();
};


#endif // TestSnap_hpp
