//  TestReportBugDialog.hpp
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


#include <QtTest/QtTest>


class TestReportBugDialog : public QObject
{
        Q_OBJECT

private slots:
        void sendsAReportThroughTheRelay();
        void saysSoWhenTheRelayIsDown();
        void refusesToSendOverPlainHttpToARemoteHost();
        void showsTheRateLimitMessage();
};
