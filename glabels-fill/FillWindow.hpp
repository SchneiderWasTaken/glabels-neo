//  FillWindow.hpp
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


//  FillWindow.hpp
//

#ifndef FillWindow_hpp
#define FillWindow_hpp


#include "FillConfig.hpp"

#include <QMainWindow>
#include <QByteArray>

#include <memory>


class QComboBox;


namespace glabels { namespace model { class Model; } }
namespace glabels { class FillView; }


namespace glabels
{


        class FillWindow : public QMainWindow
        {
                Q_OBJECT

        public:
                explicit FillWindow( const FillConfig& cfg, QWidget* parent = nullptr );
                ~FillWindow() override;

        protected:
                void keyPressEvent( QKeyEvent* event ) override;

        private slots:
                void onLabelChanged( int index );
                void onAdmin();

        private:
                void loadCurrentLabel();


                FillConfig                              mCfg;
                QString                                 mExePath;
                std::unique_ptr<model::Model>           mModel;
                FillView*                               mFillView{ nullptr };
                QComboBox*                              mLabelCombo{ nullptr };
        };


}


#endif // FillWindow_hpp
