//  TestCli.hpp
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

#ifndef TestCli_hpp
#define TestCli_hpp

#include <QtTest>


class TestCli : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();

        // In-process: exit codes and files
        void printFieldsToPdf();
        void needsATarget();
        void rejectsBothTargets();
        void unknownPrinterIsAPrinterError();
        void missingLabelFileIsAnInputError();
        void notALabelFileIsAnInputError();
        void strictTurnsWarningsIntoErrors();
        void warningsAloneDoNotFail();
        void dryRunWritesNothing();
        void maxLabelsStopsFloods();
        void perRecordCopiesBecomePages();
        void pngOutputNamesPages();
        void firstBeyondTheSheetIsAUsageError();
        void stdinCanBeUsedOnce();
        void badNumbersAreUsageErrors();
        void unknownOptionsAreUsageErrors();
        void jobDocumentRunsEveryJob();
        void jobDocumentIsAllOrNothing();
        void commandLineOverridesTheJobDocument();

        // In-process: prompting
        void promptsForMissingFields();
        void endOfInputCancels();
        void forcedInteractiveAsksForEverything();
        void answeringNoCancelsTheJob();

        // Real processes: the machine interface
        void versionContract();
        void legacyVersionContract();
        void jsonResultShape();
        void jsonUsageError();
        void pdfOnStdoutIsClean();
        void labelFromStdin();
        void inspectReportsFields();
        void printersListsAsJson();
        void helpAndUnknownCommand();
        void legacyToolStillPrints();
        void schemaMatchesTheLoader();
};

#endif // TestCli_hpp
