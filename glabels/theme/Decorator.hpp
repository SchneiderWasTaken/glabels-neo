//  Decorator.hpp
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

#ifndef theme_Decorator_hpp
#define theme_Decorator_hpp


#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QObject>


namespace glabels::theme
{

        ///
        /// Paints the parts of the look a stylesheet cannot express, behind the normal painting:
        ///
        ///  - dialog footers: a `chrome` band with a `line` top border behind the bottom row of
        ///    push buttons of every QDialog (SPEC.md 7, "Dialog footers sit on chrome");
        ///  - cards: every titled, non-flat QGroupBox becomes a `panel` card with a `line`
        ///    border, radius 10, and a 38 px `chrome` header band under its title (SPEC.md 4.3);
        ///  - popups (docs/ribbon-ui/POPUP-AUDIT.md): combo boxes show up to 15 rows without
        ///    scrolling and size their rows from the stylesheet; lists in popup windows get square
        ///    corners; frameless popup windows always paint their background.
        ///
        /// install() puts a small watcher on the application that hooks each dialog and group
        /// box once, when it is polished.  The decorator only draws; it never changes geometry,
        /// layouts or behaviour.  Set the dynamic property "plainFooter" (dialog) or "plainCard"
        /// (group box) to true to opt a widget out.
        ///
        class Decorator : public QObject
        {
                Q_OBJECT

        public:
                static Decorator* instance();

                /// Hook the application (idempotent)
                static void install();

                /// Header height of a card, in device-independent pixels
                static constexpr int cardHeader = 38;

                /// Top of the footer band of a dialog, or -1 when it has no bottom row of buttons
                static int footerTop( const QDialog* dialog );

                /// Rows a combo box popup shows before it scrolls
                static constexpr int popupRows = 15;

                /// Popup fixes, applied on every polish (public for the tests)
                static void polishCombo( QComboBox* combo );
                static void polishPopupView( QAbstractItemView* view );
                static void polishPopupWindow( QWidget* window );

        protected:
                bool eventFilter( QObject* watched, QEvent* event ) override;

        private:
                Decorator();

                static void paintFooter( QDialog* dialog );
                static void paintCard( QGroupBox* box );
        };

}


#endif // theme_Decorator_hpp
