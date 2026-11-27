//  MainWindow.hpp
//
//  Copyright (C) 2014-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
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

#ifndef MainWindow_hpp
#define MainWindow_hpp


#include "UndoRedoModel.hpp"
#include "ribbon/RibbonTypes.hpp"

#include <model/Model.hpp>

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>

#include <memory>


namespace glabels
{

        // Forward References
        class LabelEditor;
        class MergeView;
        class ObjectEditor;
        class FillView;
        class PrintView;
        class PropertiesView;
        class StartupView;
        class VariablesView;
        class DeployView;

        namespace ribbon
        {
                class Backstage;
                class ContentPanel;
                class EditorRulerFrame;
                class RibbonBar;
                class RibbonButton;
                class RibbonDropButton;
                class RibbonGroup;
                class RibbonStatusBar;
                class RibbonToggleGroup;
                class ShadowPopover;
        }


        ///
        /// MainWindow Widget
        ///
        class MainWindow : public QMainWindow
        {
                Q_OBJECT


                /////////////////////////////////////
                // Lifecycle
                /////////////////////////////////////
        public:
                MainWindow();
                virtual ~MainWindow() = default;


                /////////////////////////////////////
                // Public Methods
                /////////////////////////////////////
        public:
                model::Model* model() const;
                void setModel( model::Model* model );
                bool isEmpty() const;

                // Inspection accessors (used by unit tests)
                ribbon::RibbonBar*       ribbonBar() const;
                ribbon::Backstage*       backstage() const;
                ribbon::ContentPanel*    contentPanel() const;
                ribbon::RibbonStatusBar* ribbonStatusBar() const;
                QStackedWidget*          pageStack() const;
                LabelEditor*             labelEditor() const;
                QList<QAction*>          allWindowActions() const;
                ribbon::SelectionKind    currentSelectionKind() const;


                /////////////////////////////////////
                // Events
                /////////////////////////////////////
        protected:
                void closeEvent( QCloseEvent *event ) override;
                bool event( QEvent *event ) override;
                bool eventFilter( QObject *watched, QEvent *event ) override;


                /////////////////////////////////////
                // Slots
                /////////////////////////////////////
        private slots:
                void onRibbonTabChanged( ribbon::TabId id );
                void onFileButtonClicked();
                void onBackstageCloseRequested();
                void onStatusZoomStepsRequested( int steps );

                void clipboardChanged();

                void fileNew();
                void fileOpen();
                void fileOpenRecent();
                void fileSave();
                void fileSaveAs();
                void fileShowEditorPage();
                void fileShowPropertiesPage();
                void fileShowMergePage();
                void fileShowVariablesPage();
                void fileShowFillPage();
                void fileShowPrintPage();
                void fileShowDeployPage();
                void fileTemplateDesigner();
                void fileClose();
                void fileExit();

                void editUndo();
                void editRedo();
                void editCut();
                void editCopy();
                void editPaste();
                void editContextPaste();
                void editDelete();
                void editSelectAll();
                void editUnSelectAll();
                void editPreferences();

                void viewFileToolBar( bool );
                void viewContentPanel( bool );
                void viewGrid( bool );
                void viewMarkup( bool );
                void viewZoomIn();
                void viewZoomOut();
                void viewZoom1To1();
                void viewZoomToFit();

                void objectsArrowMode();
                void objectsCreateText();
                void objectsCreateBox();
                void objectsCreateLine();
                void objectsCreateEllipse();
                void objectsCreateImage();
                void objectsCreateBarcode();
                void objectsOrderRaise();
                void objectsOrderLower();
                void objectsXformRotateLeft();
                void objectsXformRotateRight();
                void objectsXformFlipHoriz();
                void objectsXformFlipVert();
                void objectsAlignLeft();
                void objectsAlignHCenter();
                void objectsAlignRight();
                void objectsAlignTop();
                void objectsAlignVCenter();
                void objectsAlignBottom();
                void objectsCenter();
                void objectsCenterHoriz();
                void objectsCenterVert();

                void helpContents();
                void helpReportBug();
                void helpAbout();

                void onContextMenuActivate( model::Point );

                void onZoomChanged();
                void onPointerMoved( model::Point );
                void onPointerExit();

                void onNameChanged();
                void onModifiedChanged();
                void onSelectionChanged();
                void onLabelChanged();
                void onUndoRedoChanged();

                void onSettingsChanged();


                /////////////////////////////////////
                // Internal Private Methods
                /////////////////////////////////////
        private:
                void createActions();
                void createContextMenus();
                void createRibbon();
                void createStatusBar();

                void populateHomeTab();
                void populateLabelTab();
                void populateDataTab();
                void populatePrintTab();
                void populateDeployTab();
                void populateViewTab();
                void populateHelpTab();
                void populateContextualTabs();

                QWidget* createWelcomePage();
                QWidget* createEditorPage();
                QWidget* createPropertiesPage();
                QWidget* createMergePage();
                QWidget* createVariablesPage();
                QWidget* createFillPage();
                QWidget* createPrintPage();
                QWidget* createDeployPage();

                void switchToPage( ribbon::PageId page, bool syncRibbon = true );
                void updateContextualTabState( bool activateIfSingle );
                void attachSharedContextualWidgets( ribbon::TabId ctxTab );
                void syncFieldDropButtons();
                void updateDataTabGroups();
                void updateLabelTabInfo();
                void updatePrintTabInfo();
                void updateStatusBarSummary();

                void manageActions();

                void setTitle();

                void readSettings();
                void writeSettings();

                bool isOkToClose();


                /////////////////////////////////////
                // Private Data
                // owned and managed by us
                /////////////////////////////////////
        private:
                std::unique_ptr<model::Model>  mModel;
                std::unique_ptr<UndoRedoModel> mUndoRedoModel;


                /////////////////////////////////////
                // Private Data
                // owned by QMainWindow
                /////////////////////////////////////
        private:
                QStackedWidget*           mShellStack{ nullptr };
                ribbon::RibbonBar*        mRibbonBar{ nullptr };
                ribbon::Backstage*        mBackstage{ nullptr };
                ribbon::RibbonStatusBar*  mRibbonStatusBar{ nullptr };
                ribbon::EditorRulerFrame* mEditorRulerFrame{ nullptr };
                ribbon::ContentPanel*     mContentPanel{ nullptr };
                ribbon::ShadowPopover*    mShadowPopover{ nullptr };

                QMenu*    contextMenu{ nullptr };
                QMenu*    contextOrderMenu{ nullptr };
                QMenu*    contextXformMenu{ nullptr };
                QMenu*    contextAlignMenu{ nullptr };
                QMenu*    contextCenterMenu{ nullptr };
                QMenu*    noSelectionContextMenu{ nullptr };

                QStackedWidget*      mPages{ nullptr };
                StartupView*         mWelcomeView{ nullptr };
                QScrollArea*         mLabelEditorScrollArea{ nullptr };
                LabelEditor*         mLabelEditor{ nullptr };
                ObjectEditor*        mObjectEditor{ nullptr };
                PropertiesView*      mPropertiesView{ nullptr };
                MergeView*           mMergeView{ nullptr };
                VariablesView*       mVariablesView{ nullptr };
                FillView*            mFillView{ nullptr };
                PrintView*           mPrintView{ nullptr };
                DeployView*          mDeployView{ nullptr };

                ribbon::PageId       mCurrentPage{ ribbon::PageId::Welcome };
                ribbon::DataSubPage  mLastDataSubPage{ ribbon::DataSubPage::Merge };
                bool                 mCreatingObject{ false };
                bool                 mSyncingRibbon{ false };
                bool                 mRecentContextualDrop{ false };

                // Ribbon sub-widgets needing synchronization
                QLabel*                    mLabelTabName{ nullptr };
                QLabel*                    mLabelTabDetails{ nullptr };
                ribbon::RibbonGroup*       mLabelTabParamsGroup{ nullptr };
                QDoubleSpinBox*            mLabelTabLengthSpin{ nullptr };
                QComboBox*                 mLabelTabOrientationCombo{ nullptr };

                ribbon::RibbonButton*      mDataMergeBtn{ nullptr };
                ribbon::RibbonButton*      mDataVariablesBtn{ nullptr };
                ribbon::RibbonButton*      mDataFillBtn{ nullptr };
                ribbon::RibbonGroup*       mDataMergeSourceGroup{ nullptr };
                ribbon::RibbonGroup*       mDataMergeRecordsGroup{ nullptr };
                ribbon::RibbonGroup*       mDataVarsGroup{ nullptr };
                ribbon::RibbonGroup*       mDataFillRowsGroup{ nullptr };
                ribbon::RibbonGroup*       mDataFillLiveGroup{ nullptr };
                ribbon::RibbonGroup*       mDataFillIoGroup{ nullptr };
                ribbon::RibbonGroup*       mDataFillOutputGroup{ nullptr };
                ribbon::RibbonButton*      mVarsEditRibbonBtn{ nullptr };
                ribbon::RibbonButton*      mVarsDeleteRibbonBtn{ nullptr };
                ribbon::RibbonButton*      mFillRefreshSourceRibbonBtn{ nullptr };
                ribbon::RibbonButton*      mFillPrintRibbonBtn{ nullptr };
                ribbon::RibbonButton*      mFillSystemDialogRibbonBtn{ nullptr };

                ribbon::RibbonButton*      mPrintRibbonBtn{ nullptr };
                ribbon::RibbonButton*      mPrintSystemDialogRibbonBtn{ nullptr };
                QComboBox*                 mPrintTabDestCombo{ nullptr };
                QLabel*                    mPrintTabStatusLabel{ nullptr };

                QCheckBox*                 mViewGridCheck{ nullptr };
                QCheckBox*                 mViewMarkupCheck{ nullptr };
                QCheckBox*                 mViewQatCheck{ nullptr };
                QCheckBox*                 mViewContentPanelCheck{ nullptr };
                ribbon::RibbonToggleGroup* mViewColorSchemeGroup{ nullptr };
                ribbon::RibbonToggleGroup* mViewIconStyleGroup{ nullptr };

                // WP4 shared transplanted ObjectEditor widgets & contextual tab hosts
                QWidget*                              mSharedPosWidget{ nullptr };
                QWidget*                              mSharedRectSizeWidget{ nullptr };
                QWidget*                              mImageResetSubWidget{ nullptr };
                QToolButton*                          mLockAspectButton{ nullptr };
                QWidget*                              mSharedLineWidget{ nullptr };
                QMap<ribbon::TabId, QHBoxLayout*>     mPosSizeHostLayouts;
                QMap<ribbon::TabId, QHBoxLayout*>     mLineHostLayouts;
                ribbon::RibbonDropButton*             mTextInsertFieldDropBtn{ nullptr };
                ribbon::RibbonDropButton*             mBarcodeInsertFieldDropBtn{ nullptr };
                ribbon::RibbonDropButton*             mImageUseFieldDropBtn{ nullptr };

                QActionGroup*              mInsertToolGroup{ nullptr };

                QAction*  fileNewAction{ nullptr };
                QAction*  fileOpenAction{ nullptr };
                QAction*  fileSaveAction{ nullptr };
                QAction*  fileSaveAsAction{ nullptr };
                QAction*  fileShowEditorPageAction{ nullptr };
                QAction*  fileShowPropertiesPageAction{ nullptr };
                QAction*  fileShowMergePageAction{ nullptr };
                QAction*  fileShowVariablesPageAction{ nullptr };
                QAction*  fileShowFillPageAction{ nullptr };
                QAction*  fileShowPrintPageAction{ nullptr };
                QAction*  fileShowDeployPageAction{ nullptr };
                QAction*  fileTemplateDesignerAction{ nullptr };
                QAction*  fileCloseAction{ nullptr };
                QAction*  fileExitAction{ nullptr };

                QList<QAction*>  fileRecentActionList;

                QAction*  editUndoAction{ nullptr };
                QAction*  editRedoAction{ nullptr };
                QAction*  editCutAction{ nullptr };
                QAction*  editCopyAction{ nullptr };
                QAction*  editPasteAction{ nullptr };
                QAction*  editDeleteAction{ nullptr };
                QAction*  editSelectAllAction{ nullptr };
                QAction*  editUnSelectAllAction{ nullptr };
                QAction*  editPreferencesAction{ nullptr };

                QAction*  viewFileToolBarAction{ nullptr };
                QAction*  viewContentPanelAction{ nullptr };
                QAction*  viewCollapseRibbonAction{ nullptr };
                QAction*  viewGridAction{ nullptr };
                QAction*  viewMarkupAction{ nullptr };
                QAction*  viewSnapObjectsAction{ nullptr };
                QAction*  viewSnapCenterAction{ nullptr };
                QAction*  viewSnapSafeAreaAction{ nullptr };
                QAction*  viewZoomInAction{ nullptr };
                QAction*  viewZoomOutAction{ nullptr };
                QAction*  viewZoom1To1Action{ nullptr };
                QAction*  viewZoomToFitAction{ nullptr };

                QAction*  objectsArrowModeAction{ nullptr };
                QAction*  objectsCreateTextAction{ nullptr };
                QAction*  objectsCreateBoxAction{ nullptr };
                QAction*  objectsCreateLineAction{ nullptr };
                QAction*  objectsCreateEllipseAction{ nullptr };
                QAction*  objectsCreateImageAction{ nullptr };
                QAction*  objectsCreateBarcodeAction{ nullptr };
                QAction*  objectsOrderRaiseAction{ nullptr };
                QAction*  objectsOrderLowerAction{ nullptr };
                QAction*  objectsXformRotateLeftAction{ nullptr };
                QAction*  objectsXformRotateRightAction{ nullptr };
                QAction*  objectsXformFlipHorizAction{ nullptr };
                QAction*  objectsXformFlipVertAction{ nullptr };
                QAction*  objectsAlignLeftAction{ nullptr };
                QAction*  objectsAlignHCenterAction{ nullptr };
                QAction*  objectsAlignRightAction{ nullptr };
                QAction*  objectsAlignTopAction{ nullptr };
                QAction*  objectsAlignVCenterAction{ nullptr };
                QAction*  objectsAlignBottomAction{ nullptr };
                QAction*  objectsCenterAction{ nullptr };
                QAction*  objectsCenterHorizAction{ nullptr };
                QAction*  objectsCenterVertAction{ nullptr };

                QAction*  helpContentsAction{ nullptr };
                QAction*  helpReportBugAction{ nullptr };
                QAction*  helpCheckForUpdatesAction{ nullptr };
                QAction*  helpAboutAction{ nullptr };

                QAction*  contextCutAction{ nullptr };
                QAction*  contextCopyAction{ nullptr };
                QAction*  contextPasteAction{ nullptr };
                QAction*  contextDeleteAction{ nullptr };

                QList<QAction*> mAllWindowActions;
        };


}


#endif // MainWindow_hpp
