//  TestMarketingShots.hpp
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
/// Captures the pictures used by the README and the website.  Does nothing unless the
/// environment variable GLABELS_MARKETING_SHOTS names an output folder, so it is safe in the
/// normal test run.  It uses the sample projects in docs/assets/samples, so the pictures are
/// reproducible:
///
///     GLABELS_MARKETING_SHOTS=out ctest -R MarketingShots
///
class TestMarketingShots : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();
        void screens();
        void schemes();
};
