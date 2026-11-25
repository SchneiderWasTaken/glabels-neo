//  ShadowPopover.hpp
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

#ifndef ribbon_ShadowPopover_hpp
#define ribbon_ShadowPopover_hpp


#include <QCheckBox>
#include <QGroupBox>
#include <QWidget>


namespace glabels::ribbon
{

        ///
        /// Shadow popover (SPEC 6.2, mockup D10) opened from the Effects > Shadow ribbon button.
        /// Transplants ObjectEditor's shadowXSpin, shadowYSpin, shadowColorButton, shadowOpacitySpin
        /// and stays bidirectionally synced with ObjectEditor's checkable shadowEnableCheck group box.
        ///
        class ShadowPopover : public QWidget
        {
                Q_OBJECT

        public:
                struct Controls
                {
                        QGroupBox* enableGroup{ nullptr };
                        QWidget*   xSpin{ nullptr };
                        QWidget*   ySpin{ nullptr };
                        QWidget*   colorButton{ nullptr };
                        QWidget*   opacitySpin{ nullptr };
                };

        public:
                explicit ShadowPopover( const Controls& controls, QWidget* parent = nullptr );

                void syncFromObjectEditor();

        protected:
                void paintEvent( QPaintEvent* event ) override;
                void showEvent( QShowEvent* event ) override;

        private slots:
                void onEnableCheckToggled( bool checked );
                void onGroupToggled( bool checked );
                void onThemeChanged();

        private:
                void updateControlsEnabled( bool enabled );

                Controls   mControls;
                QCheckBox* mEnableCheck{ nullptr };
                bool       mSyncing{ false };
        };

}


#endif // ribbon_ShadowPopover_hpp
