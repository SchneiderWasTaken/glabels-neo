//  TestJobs.hpp
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

#ifndef TestJobs_hpp
#define TestJobs_hpp

#include <QtTest>


class TestJobs : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();

        // Command-line pieces
        void assignmentSplitsOnTheFirstEquals();
        void assignmentRejectsBadNames();

        // JSON job documents
        void oneJobShorthand();
        void severalJobs();
        void relativePathsResolveAgainstTheDocument();
        void unknownKeysAreErrors();
        void printerAndOutputAreExclusive();
        void labelsAndDataAreExclusive();
        void optionTypesAreChecked();
        void reservedNamesAreRejected();
        void unsupportedVersionIsRejected();
        void badJsonSaysWhere();

        // Command line over a document
        void overridesWin();

        // Field scan
        void scanFindsFieldsAndDefaults();
        void scanSeparatesVariables();

        // Field precedence and suggestions
        void precedenceDataFileWinsByDefault();
        void forceFieldsReversesIt();
        void closestNameFindsTypos();
};

#endif // TestJobs_hpp
