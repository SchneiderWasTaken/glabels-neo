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

#include "model/ExternalDataSource.hpp"
#include "model/Model.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

class QCheckBox;
class QVBoxLayout;
class QScrollArea;
class QTimer;
class QToolButton;


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

                /// Source name and sync status from the Fill view (Data section header).
                void setSourceStatus( const QString& title, const QString& status, bool isError, bool isBusy );

                /// Scroll the Data section into view.
                void revealDataSection();

                /// MIME type of a dragged data column (payload: the column name, UTF-8).
                static QString columnMimeType();

                /// Passed to columnChosen() when the person picks "Leave blank".
                static QString blankColumnToken();

        signals:
                /// Emitted when user double-clicks a field or variable chip row.
                void fieldActivated( const QString& key );

                /// Connect (or change) the data source.
                void connectSourceRequested( glabels::model::ExternalSourceKind kind );

                /// Refresh the linked data source.
                void refreshSourceRequested();

                /// The person chose the column that fills a label field (blankColumnToken() = leave blank).
                void columnChosen( const QString& field, const QString& column );

                /// "Show row data on the label" toggled.
                void showRowDataToggled( bool on );

                /// Which Fill row the editor shows.
                void previewRowChanged( int row );

        protected:
                void paintEvent( QPaintEvent* event ) override;

        private slots:
                void refreshFieldsAndVariables();
                void onThemeChanged();
                void scheduleDataRefresh();
                void rebuildDataSection();

        private:
                void syncFieldButtons();
                void buildDataSection( QWidget* parent, QVBoxLayout* into );
                void stepPreviewRow( int delta );
                void updateRowNav();

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

                // Data section: the linked source, its columns, and the fields that still need one
                QScrollArea*    mScroll{ nullptr };
                QWidget*        mDataSection{ nullptr };
                QLabel*         mDataCaption{ nullptr };
                QWidget*        mDataEmptyBox{ nullptr };
                QWidget*        mDataSourceBox{ nullptr };
                QLabel*         mDataTitleLabel{ nullptr };
                QLabel*         mDataStatusLabel{ nullptr };
                QPushButton*    mDataRefreshBtn{ nullptr };
                QPushButton*    mDataChangeBtn{ nullptr };
                QWidget*        mRowNavBox{ nullptr };
                QCheckBox*      mShowRowCheck{ nullptr };
                QToolButton*    mRowPrevBtn{ nullptr };
                QToolButton*    mRowNextBtn{ nullptr };
                QLabel*         mRowLabel{ nullptr };
                QLabel*         mColumnsHeader{ nullptr };
                QWidget*        mColumnsHost{ nullptr };
                QLabel*         mNeedsHeader{ nullptr };
                QWidget*        mNeedsHost{ nullptr };
                QTimer*         mDataTimer{ nullptr };

                QString         mSourceTitle;
                QString         mSourceStatus;
                bool            mSourceError{ false };
                bool            mSourceBusy{ false };
                QString         mDataSignature;
                int             mPreviewRow{ 0 };
        };

}


#endif // ribbon_ContentPanel_hpp
