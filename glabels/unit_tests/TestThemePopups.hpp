//  TestThemePopups.hpp
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
/// Popup audit (docs/ribbon-ui/POPUP-AUDIT.md): every popup kind is opened offscreen and
/// measured.  A list of up to 15 items shows every item without scrolling; longer lists show
/// 15 and scroll.  The corners of every popup window, read back from the window's backing
/// store, are never black on the light theme.  With GLABELS_THEME_SCREENSHOTS set, each popup
/// is saved under <dir>/popups/.
///
class TestThemePopups : public QObject
{
        Q_OBJECT

private slots:
        void initTestCase();
        void cleanup();

        void comboShowsEveryItem_data();
        void comboShowsEveryItem();
        void comboInEveryDialogAndPage();
        void menusAndSubmenus();
        void customPopups();
        void toolTipAndCompleter();
};
