//  RibbonStatusBar.hpp
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

#ifndef ribbon_RibbonStatusBar_hpp
#define ribbon_RibbonStatusBar_hpp


#include <QAction>
#include <QLabel>
#include <QSlider>
#include <QString>
#include <QWidget>


namespace glabels::ribbon
{

        class RibbonButton;

        ///
        /// 28px bottom status bar (SPEC 6.6).
        /// Left: hovered status tip / temporary message, else label summary.
        /// Right: selection count, cursor coordinates, zoom out/slider/in, zoom percent.
        ///
        class RibbonStatusBar : public QWidget
        {
                Q_OBJECT

        public:
                explicit RibbonStatusBar( QAction* zoomOutAction,
                                          QAction* zoomInAction,
                                          QWidget* parent = nullptr );

                void setLabelSummary( const QString& summary );
                void setTemporaryMessage( const QString& message );
                void setSelectionCount( int count );
                void setCursorPositionText( const QString& text );
                void setZoomFactor( double zoom );
                void setEditorControlsVisible( bool visible );

        signals:
                /// Emitted when the user moves the zoom slider to a different discrete zoom notch.
                void zoomStepsRequested( int steps );

        protected:
                void paintEvent( QPaintEvent* event ) override;

        private slots:
                void onSliderValueChanged( int value );
                void onThemeChanged();

        private:
                void updateLeftText();
                static int zoomToSliderIndex( double zoom );

                QLabel*       mLeftLabel{ nullptr };
                QLabel*       mSelectionLabel{ nullptr };
                QLabel*       mCursorLabel{ nullptr };
                QWidget*      mZoomContainer{ nullptr };
                RibbonButton* mZoomOutBtn{ nullptr };
                QSlider*      mZoomSlider{ nullptr };
                RibbonButton* mZoomInBtn{ nullptr };
                QLabel*       mZoomPercentLabel{ nullptr };

                QString       mLabelSummary;
                QString       mTemporaryMessage;
                int           mLastSliderIndex{ 4 };
                bool          mUpdatingSlider{ false };
        };

}


#endif // ribbon_RibbonStatusBar_hpp
