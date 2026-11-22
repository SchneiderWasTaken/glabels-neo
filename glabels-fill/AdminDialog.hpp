//  AdminDialog.hpp
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


//  AdminDialog.hpp
//

#ifndef AdminDialog_hpp
#define AdminDialog_hpp


#include "FillConfig.hpp"

#include <QDialog>


class QLineEdit;
class QListWidget;
class QPushButton;


namespace glabels
{


        ///
        /// PIN-entry + setup dialog for the kiosk admin mode.
        ///
        /// Two stages:
        ///   1. PIN entry (verifies against the config's adminPin).
        ///   2. Settings editor (label files, printer, PIN, branding).
        ///
        class AdminDialog : public QDialog
        {
                Q_OBJECT


        public:
                explicit AdminDialog( const FillConfig& cfg, QWidget* parent = nullptr );

                /// The (possibly edited) config.  Only valid if exec() == Accepted.
                FillConfig result() const { return mCfg; }


        private slots:
                void onVerifyPin();
                void onAddLabel();
                void onRemoveLabel();
                void onBrowsePrinter();
                void onAccept();


        private:
                void buildPinPage();
                void buildSettingsPage();


                FillConfig   mCfg;
                bool         mPinOk{ false };

                // PIN page
                QLineEdit*   mPinEdit{ nullptr };

                // Settings page
                QListWidget* mLabelList{ nullptr };
                QLineEdit*   mPrinterEdit{ nullptr };
                QLineEdit*   mNewPinEdit{ nullptr };
                QLineEdit*   mAppNameEdit{ nullptr };
        };


}


#endif // AdminDialog_hpp
