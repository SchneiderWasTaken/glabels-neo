//  DeployView.hpp
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


//  DeployView.hpp
//

#ifndef DeployView_hpp
#define DeployView_hpp


#include "FillConfig.hpp"

#include <QWidget>
#include <QStringList>


class QCheckBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QVBoxLayout;


namespace glabels { namespace model { class Model; } }


namespace glabels
{


        ///
        /// Designer page for configuring and building a kiosk (glabels-fill)
        /// deployment from the current project.
        ///
        class DeployView : public QWidget
        {
                Q_OBJECT

        public:
                explicit DeployView( QWidget* parent = nullptr );

                void setModel( model::Model* model );
                QPushButton* buildButton() const { return mBuildButton; }

        private slots:
                void onBrowseSource();
                void onBrowsePrinter();
                void onBrowseFillExe();
                void onBuild();

        private:
                QStringList collectFieldNames() const;
                void refreshLockedFields();


                model::Model*   mModel{ nullptr };
                QLineEdit*      mAppNameEdit{ nullptr };
                QLineEdit*      mPinEdit{ nullptr };
                QLineEdit*      mPrinterEdit{ nullptr };
                QLineEdit*      mSourcePathEdit{ nullptr };
                QLineEdit*      mFillExeEdit{ nullptr };
                QLineEdit*      mLabelNameEdit{ nullptr };
                QListWidget*    mLockedFieldsList{ nullptr };
                QPushButton*    mBuildButton{ nullptr };
        };


}


#endif // DeployView_hpp
