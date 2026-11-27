//  TestDataInDesigner.hpp
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

#ifndef TestDataInDesigner_hpp
#define TestDataInDesigner_hpp


#include <QtTest/QtTest>


///
/// Data in the designer: matching columns to fields, the saved column choices, the Data section
/// of the Content panel, dropping a column on the label, and showing a row on the label.
///
class TestDataInDesigner : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();

        // No GUI
        void matcherPrefersChoiceThenNameThenNormalizedAndOnlySuggestsTheRest();
        void matcherBlankAndMissingChoice();
        void columnMapSurvivesSaveAndLoad();
        void fieldBindingsKnowComputedConstantAndUnusedFields();
        void fieldsComeFromBarcodeAndImageToo();

        // Offscreen GUI
        void dropOnObjectsBindsThemAndDropOnEmptyLabelCreatesText();
        void dropIsOneUndoStep();
        void panelListsColumnsAndOffersChoicesForFieldsThatNeedOne();
        void panelEmptyStateOffersToConnect();
        void showRowDataChangesWhatTheEditorDraws();
};


#endif // TestDataInDesigner_hpp
