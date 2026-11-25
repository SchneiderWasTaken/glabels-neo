//  ContentPanel.hpp
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

#ifndef ribbon_ContentPanel_hpp
#define ribbon_ContentPanel_hpp


#include "RibbonTypes.hpp"

#include "model/Model.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>


namespace glabels::ribbon
{

        class RibbonDropButton;

        ///
        /// Right-hand 300px Content panel (SPEC 6.5).
        /// Hosts the transplanted content controls from ObjectEditor (textEdit, barcodeDataEdit,
        /// image file controls) plus the live Merge Fields and Variables chip lists.
        ///
        class ContentPanel : public QWidget
        {
                Q_OBJECT

        public:
                struct TransplantedControls
                {
                        QWidget*     textEdit{ nullptr };
                        QWidget*     textInsertFieldButton{ nullptr };
                        QWidget*     barcodeDataEdit{ nullptr };
                        QWidget*     barcodeInsertFieldButton{ nullptr };
                        QLineEdit*   imageFilenameSource{ nullptr }; // synced with ContentPanel's display
                        QPushButton* imageBrowseSource{ nullptr };
                        QWidget*     imageFieldButton{ nullptr };
                };

        public:
                explicit ContentPanel( QWidget* parent = nullptr );

                /// Transplant ObjectEditor's content widgets once at startup (SPEC 3.4).
                void installControls( const TransplantedControls& controls );

                /// Bind to the active model so Fields and Variables stay up to date.
                void setModel( model::Model* model );

                /// Switch visible editor section based on current selection.
                void setSelectionKind( SelectionKind kind );

        signals:
                /// Emitted when user double-clicks a field or variable chip row.
                void fieldActivated( const QString& key );

        protected:
                void paintEvent( QPaintEvent* event ) override;

        private slots:
                void refreshFieldsAndVariables();
                void onThemeChanged();

        private:
                void syncFieldButtons();

                model::Model*   mModel{ nullptr };
                SelectionKind   mSelectionKind{ SelectionKind::None };

                QLabel*         mTypeCaption{ nullptr };
                QStackedWidget* mEditorStack{ nullptr };

                QWidget*        mTextPage{ nullptr };
                QWidget*        mBarcodePage{ nullptr };
                QWidget*        mImagePage{ nullptr };
                QLineEdit*      mImageFileMirror{ nullptr };

                QPushButton*      mTextFieldSource{ nullptr };
                QPushButton*      mBarcodeFieldSource{ nullptr };
                QPushButton*      mImageFieldSource{ nullptr };
                RibbonDropButton* mTextFieldBtn{ nullptr };
                RibbonDropButton* mBarcodeFieldBtn{ nullptr };
                RibbonDropButton* mImageFieldBtn{ nullptr };

                QWidget*        mFieldsSection{ nullptr };
                QLabel*         mFieldsSourceLabel{ nullptr };
                QWidget*        mFieldsListHost{ nullptr };

                QWidget*        mVariablesSection{ nullptr };
                QWidget*        mVariablesListHost{ nullptr };
        };

}


#endif // ribbon_ContentPanel_hpp
