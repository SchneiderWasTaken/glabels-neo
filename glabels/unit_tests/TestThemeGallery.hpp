//  TestThemeGallery.hpp
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
/// Opens every restyled dialog (SPEC.md 7) in light and dark through the real AppearanceModel
/// path and checks that each one polishes and paints without stylesheet warnings.  When the
/// environment variable GLABELS_THEME_SCREENSHOTS names a folder, a PNG of each is saved there
/// for comparison with docs/ribbon-ui/mockups/d01..d11.
///
class TestThemeGallery : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();
        void dialogs_data();
        void dialogs();
        void switchesWithoutRestart();
        void mainWindowScreens();
};
