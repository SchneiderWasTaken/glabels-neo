//  MainWindow.cpp
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


#include "MainWindow.hpp"

#include "AppearanceModel.hpp"
#include "theme/ThemeScheme.hpp"
#include "ColorButton.hpp"
#include "DeployView.hpp"
#include "File.hpp"
#include "FillView.hpp"
#include "Help.hpp"
#include "LabelEditor.hpp"
#include "MergeView.hpp"
#include "ObjectEditor.hpp"
#include "PreferencesDialog.hpp"
#include "PrintView.hpp"
#include "PrinterMonitor.hpp"
#include "PropertiesView.hpp"
#include "StartupView.hpp"
#include "UpdateUi.hpp"
#include "VariablesView.hpp"
#include "ribbon/Backstage.hpp"
#include "ribbon/ContentPanel.hpp"
#include "ribbon/EditorRulers.hpp"
#include "ribbon/QuickAccessBar.hpp"
#include "ribbon/RibbonBar.hpp"
#include "ribbon/RibbonButton.hpp"
#include "ribbon/RibbonColorDecorator.hpp"
#include "ribbon/RibbonDropButton.hpp"
#include "ribbon/RibbonField.hpp"
#include "ribbon/RibbonGroup.hpp"
#include "ribbon/RibbonStatusBar.hpp"
#include "ribbon/RibbonTab.hpp"
#include "ribbon/RibbonToggleGroup.hpp"
#include "ribbon/ShadowPopover.hpp"
#include "theme/Theme.hpp"

#include "model/Branding.hpp"
#include "model/Db.hpp"
#include "model/FrameContinuous.hpp"
#include "model/Model.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelBoxObject.hpp"
#include "model/ModelEllipseObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelLineObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"

#include <QApplication>
#include <QComboBox>
#include <QClipboard>
#include <QDebug>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPrinterInfo>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QStatusTipEvent>
#include <QTimer>
#include <QVBoxLayout>


namespace glabels
{

        namespace
        {
                int pageIndex( ribbon::PageId id )
                {
                        return static_cast<int>( id );
                }
        }


        ///
        /// Constructor
        ///
        MainWindow::MainWindow()
        {
                QIcon appIcon( QStringLiteral( ":/brand/mark.svg" ) );
                if ( appIcon.isNull() )
                {
                        appIcon = QIcon::fromTheme( QStringLiteral( "glabels" ) );
                }
                setWindowIcon( appIcon );
                setMinimumSize( 1180, 720 );

                createActions();
                createContextMenus();
                createStatusBar();

                // Build pages first so ribbon tabs can bind to page controls
                QWidget* welcomePage    = createWelcomePage();
                QWidget* editorPage     = createEditorPage();
                QWidget* propertiesPage = createPropertiesPage();
                QWidget* mergePage      = createMergePage();
                QWidget* variablesPage  = createVariablesPage();
                QWidget* printPage      = createPrintPage();
                QWidget* fillPage       = createFillPage();
                QWidget* deployPage     = createDeployPage();

                mPages = new QStackedWidget( this );
                mPages->addWidget( welcomePage );
                mPages->addWidget( editorPage );
                mPages->addWidget( propertiesPage );
                mPages->addWidget( mergePage );
                mPages->addWidget( variablesPage );
                mPages->addWidget( printPage );
                mPages->addWidget( fillPage );
                mPages->addWidget( deployPage );

                createRibbon();

                // Build File Backstage
                ribbon::Backstage::Actions bsActions;
                bsActions.fileNew              = fileNewAction;
                bsActions.fileOpen             = fileOpenAction;
                bsActions.fileRecent           = fileRecentActionList;
                bsActions.fileSave             = fileSaveAction;
                bsActions.fileSaveAs           = fileSaveAsAction;
                bsActions.fileTemplateDesigner = fileTemplateDesignerAction;
                bsActions.editPreferences      = editPreferencesAction;
                bsActions.fileClose            = fileCloseAction;
                bsActions.fileExit             = fileExitAction;
                mBackstage = new ribbon::Backstage( bsActions, this );
                connect( mBackstage, &ribbon::Backstage::closeRequested,
                         this, &MainWindow::onBackstageCloseRequested );

                // Workspace shell (RibbonBar + mPages + RibbonStatusBar)
                auto* workspace = new QWidget( this );
                auto* wsLayout  = new QVBoxLayout( workspace );
                wsLayout->setContentsMargins( 0, 0, 0, 0 );
                wsLayout->setSpacing( 0 );
                wsLayout->addWidget( mRibbonBar, 0 );
                wsLayout->addWidget( mPages, 1 );
                wsLayout->addWidget( mRibbonStatusBar, 0 );

                // Top-level stack: 0 = workspace, 1 = File backstage
                mShellStack = new QStackedWidget( this );
                mShellStack->addWidget( workspace );
                mShellStack->addWidget( mBackstage );
                mShellStack->setCurrentIndex( 0 );
                setCentralWidget( mShellStack );

                // Initial page: Welcome
                mCurrentPage = ribbon::PageId::Welcome;
                mPages->setCurrentIndex( pageIndex( ribbon::PageId::Welcome ) );
                mRibbonBar->setWelcomeMode( true );
                mRibbonStatusBar->setEditorControlsVisible( false );

                manageActions();
                setTitle();
                updateStatusBarSummary();

                // Global signals
                connect( mLabelEditor, &LabelEditor::zoomChanged, this, &MainWindow::onZoomChanged );
                connect( mLabelEditor, &LabelEditor::pointerMoved, this, &MainWindow::onPointerMoved );
                connect( mLabelEditor, &LabelEditor::pointerExited, this, &MainWindow::onPointerExit );
                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(onSettingsChanged()) );
                connect( QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(clipboardChanged()) );

                readSettings();
        }


        ///
        /// Get model accessor
        ///
        model::Model* MainWindow::model() const
        {
                return mModel.get();
        }


        ///
        /// Set model accessor
        ///
        void MainWindow::setModel( model::Model* model )
        {
                if ( mModel )
                {
                        disconnect( mModel.get(), nullptr, this, nullptr );
                }
                if ( mUndoRedoModel )
                {
                        disconnect( mUndoRedoModel.get(), nullptr, this, nullptr );
                }

                mModel.reset( model );
                mUndoRedoModel = std::make_unique<UndoRedoModel>( mModel.get() );

                mPropertiesView->setModel( mModel.get(), mUndoRedoModel.get() );
                mLabelEditor->setModel( mModel.get(), mUndoRedoModel.get() );
                mObjectEditor->setModel( mModel.get(), mUndoRedoModel.get() );
                mMergeView->setModel( mModel.get(), mUndoRedoModel.get() );
                mVariablesView->setModel( mModel.get(), mUndoRedoModel.get() );
                mFillView->setModel( mModel.get() );
                mPrintView->setModel( mModel.get() );
                mDeployView->setModel( mModel.get() );

                if ( mEditorRulerFrame )
                {
                        mEditorRulerFrame->setModel( mModel.get() );
                }
                if ( mContentPanel )
                {
                        mContentPanel->setModel( mModel.get() );
                }

                mRibbonBar->setWelcomeMode( false );
                mShellStack->setCurrentIndex( 0 );
                switchToPage( ribbon::PageId::Editor, true );

                manageActions();
                setTitle();
                updateLabelTabInfo();
                updatePrintTabInfo();
                updateStatusBarSummary();
                updateContextualTabState( false );
                syncFieldDropButtons();

                connect( mLabelEditor, SIGNAL(contextMenuActivate(model::Point)),
                         this, SLOT(onContextMenuActivate(model::Point)), Qt::UniqueConnection );
                connect( mModel.get(), SIGNAL(nameChanged()), this, SLOT(onNameChanged()) );
                connect( mModel.get(), SIGNAL(modifiedChanged()), this, SLOT(onModifiedChanged()) );
                connect( mModel.get(), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()) );
                connect( mModel.get(), SIGNAL(changed()), this, SLOT(onLabelChanged()) );
                connect( mModel.get(), SIGNAL(sizeChanged()), this, SLOT(onLabelChanged()) );
                connect( mModel.get(), &model::Model::mergeSourceChanged, this, [this]() { syncFieldDropButtons(); } );
                connect( mModel.get(), &model::Model::variablesChanged, this, [this]() { syncFieldDropButtons(); } );
                connect( mUndoRedoModel.get(), SIGNAL(changed()), this, SLOT(onUndoRedoChanged()) );
        }


        ///
        /// Is window empty?
        ///
        bool MainWindow::isEmpty() const
        {
                return !mModel;
        }


        ribbon::RibbonBar* MainWindow::ribbonBar() const
        {
                return mRibbonBar;
        }


        ribbon::Backstage* MainWindow::backstage() const
        {
                return mBackstage;
        }


        ribbon::ContentPanel* MainWindow::contentPanel() const
        {
                return mContentPanel;
        }


        ribbon::RibbonStatusBar* MainWindow::ribbonStatusBar() const
        {
                return mRibbonStatusBar;
        }


        QStackedWidget* MainWindow::pageStack() const
        {
                return mPages;
        }


        LabelEditor* MainWindow::labelEditor() const
        {
                return mLabelEditor;
        }


        QList<QAction*> MainWindow::allWindowActions() const
        {
                return mAllWindowActions;
        }


        ribbon::SelectionKind MainWindow::currentSelectionKind() const
        {
                if ( !mModel || mModel->isSelectionEmpty() )
                {
                        return ribbon::SelectionKind::None;
                }
                if ( !mModel->isSelectionAtomic() )
                {
                        return ribbon::SelectionKind::Multiple;
                }

                model::ModelObject* obj = mModel->getFirstSelectedObject();
                if ( dynamic_cast<model::ModelTextObject*>( obj ) )    return ribbon::SelectionKind::Text;
                if ( dynamic_cast<model::ModelBarcodeObject*>( obj ) ) return ribbon::SelectionKind::Barcode;
                if ( dynamic_cast<model::ModelImageObject*>( obj ) )   return ribbon::SelectionKind::Image;
                if ( dynamic_cast<model::ModelBoxObject*>( obj ) )     return ribbon::SelectionKind::Box;
                if ( dynamic_cast<model::ModelEllipseObject*>( obj ) ) return ribbon::SelectionKind::Ellipse;
                if ( dynamic_cast<model::ModelLineObject*>( obj ) )    return ribbon::SelectionKind::Line;
                return ribbon::SelectionKind::None;
        }


        ///
        /// Close Event Handler
        ///
        void MainWindow::closeEvent( QCloseEvent *event )
        {
                if ( isOkToClose() )
                {
                        writeSettings();
                        event->accept();
                }
                else
                {
                        event->ignore();
                }
        }


        bool MainWindow::event( QEvent *event )
        {
                if ( event->type() == QEvent::StatusTip && mRibbonStatusBar )
                {
                        auto* tipEvent = static_cast<QStatusTipEvent*>( event );
                        mRibbonStatusBar->setTemporaryMessage( tipEvent->tip() );
                        return true;
                }
                return QMainWindow::event( event );
        }


        bool MainWindow::eventFilter( QObject *watched, QEvent *event )
        {
                if ( watched == mLabelEditor )
                {
                        if ( event->type() == QEvent::MouseButtonRelease )
                        {
                                auto* me = static_cast<QMouseEvent*>( event );
                                if ( me->button() == Qt::LeftButton && mCreatingObject )
                                {
                                        mCreatingObject = false;
                                        objectsArrowModeAction->setChecked( true );
                                        // Activate contextual tab right after creation (SPEC 5.3)
                                        if ( mModel && mModel->isSelectionAtomic() )
                                        {
                                                updateContextualTabState( true );
                                        }
                                }
                        }
                        else if ( event->type() == QEvent::MouseButtonDblClick )
                        {
                                auto* me = static_cast<QMouseEvent*>( event );
                                if ( me->button() == Qt::LeftButton && mModel && mModel->isSelectionAtomic() )
                                {
                                        // Double-click activates contextual tab (SPEC 5.3)
                                        updateContextualTabState( true );
                                }
                        }
                }
                return QMainWindow::eventFilter( watched, event );
        }


        ///
        /// Create Actions
        ///
        void MainWindow::createActions()
        {
                auto registerAction = [this]( QAction* act )
                {
                        mAllWindowActions.append( act );
                        // Register on window so shortcuts work with menuBar removed
                        addAction( act );
                };

                /* File actions */
                fileNewAction = new QAction( tr("&New..."), this );
                fileNewAction->setIcon( QIcon::fromTheme( "glabels-file-new" ) );
                fileNewAction->setShortcut( QKeySequence::New );
                fileNewAction->setStatusTip( tr("Create a new gLabels project") );
                connect( fileNewAction, SIGNAL(triggered()), this, SLOT(fileNew()) );
                registerAction( fileNewAction );

                fileOpenAction = new QAction( tr("&Open..."), this );
                fileOpenAction->setIcon( QIcon::fromTheme( "glabels-file-open" ) );
                fileOpenAction->setShortcut( QKeySequence::Open );
                fileOpenAction->setStatusTip( tr("Open an existing gLabels project") );
                connect( fileOpenAction, SIGNAL(triggered()), this, SLOT(fileOpen()) );
                registerAction( fileOpenAction );

                for ( int i = 0; i < model::Settings::maxRecentFiles(); i++ )
                {
                        auto* action = new QAction( this );
                        action->setIcon( QIcon::fromTheme( "glabels" ) );
                        action->setVisible( false );
                        fileRecentActionList.append( action );
                        connect( action, SIGNAL(triggered()), this, SLOT(fileOpenRecent()) );
                }

                fileSaveAction = new QAction( tr("&Save"), this );
                fileSaveAction->setIcon( QIcon::fromTheme( "glabels-file-save" ) );
                fileSaveAction->setShortcut( QKeySequence::Save );
                fileSaveAction->setStatusTip( tr("Save current gLabels project") );
                connect( fileSaveAction, SIGNAL(triggered()), this, SLOT(fileSave()) );
                registerAction( fileSaveAction );

                fileSaveAsAction = new QAction( tr("Save &As..."), this );
                fileSaveAsAction->setIcon( QIcon::fromTheme( "glabels-file-save-as" ) );
                fileSaveAsAction->setShortcut( QKeySequence::SaveAs );
                fileSaveAsAction->setStatusTip( tr("Save current gLabels project to a different name") );
                connect( fileSaveAsAction, SIGNAL(triggered()), this, SLOT(fileSaveAs()) );
                registerAction( fileSaveAsAction );

                fileShowEditorPageAction = new QAction( tr("&Edit"), this );
                fileShowEditorPageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_1 ) );
                fileShowEditorPageAction->setStatusTip( tr("Select project Edit mode") );
                connect( fileShowEditorPageAction, SIGNAL(triggered()), this, SLOT(fileShowEditorPage()) );
                registerAction( fileShowEditorPageAction );

                fileShowPropertiesPageAction = new QAction( tr("P&roperties"), this );
                fileShowPropertiesPageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_2 ) );
                fileShowPropertiesPageAction->setStatusTip( tr("Select project Properties mode") );
                connect( fileShowPropertiesPageAction, SIGNAL(triggered()), this, SLOT(fileShowPropertiesPage()) );
                registerAction( fileShowPropertiesPageAction );

                fileShowMergePageAction = new QAction( tr("&Merge"), this );
                fileShowMergePageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_3 ) );
                fileShowMergePageAction->setStatusTip( tr("Select project Merge mode") );
                fileShowMergePageAction->setCheckable( true );
                connect( fileShowMergePageAction, SIGNAL(triggered()), this, SLOT(fileShowMergePage()) );
                registerAction( fileShowMergePageAction );

                fileShowVariablesPageAction = new QAction( tr("&Variables"), this );
                fileShowVariablesPageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_4 ) );
                fileShowVariablesPageAction->setStatusTip( tr("Select project Variables mode") );
                fileShowVariablesPageAction->setCheckable( true );
                connect( fileShowVariablesPageAction, SIGNAL(triggered()), this, SLOT(fileShowVariablesPage()) );
                registerAction( fileShowVariablesPageAction );

                fileShowFillPageAction = new QAction( tr("&Fill"), this );
                fileShowFillPageAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_5 ) );
                fileShowFillPageAction->setStatusTip( tr("Select project Fill mode") );
                fileShowFillPageAction->setCheckable( true );
                connect( fileShowFillPageAction, SIGNAL(triggered()), this, SLOT(fileShowFillPage()) );
                registerAction( fileShowFillPageAction );

                fileShowPrintPageAction = new QAction( tr("&Print"), this );
                fileShowPrintPageAction->setShortcut( QKeySequence::Print );
                fileShowPrintPageAction->setStatusTip( tr("Select project Print mode") );
                connect( fileShowPrintPageAction, SIGNAL(triggered()), this, SLOT(fileShowPrintPage()) );
                registerAction( fileShowPrintPageAction );

                fileShowDeployPageAction = new QAction( tr("&Deploy"), this );
                fileShowDeployPageAction->setStatusTip( tr("Build a kiosk deployment") );
                connect( fileShowDeployPageAction, SIGNAL(triggered()), this, SLOT(fileShowDeployPage()) );
                registerAction( fileShowDeployPageAction );

                fileTemplateDesignerAction = new QAction( tr("Product Template &Designer..."), this );
                fileTemplateDesignerAction->setStatusTip( tr("Create custom templates") );
                connect( fileTemplateDesignerAction, SIGNAL(triggered()), this, SLOT(fileTemplateDesigner()) );
                registerAction( fileTemplateDesignerAction );

                fileCloseAction = new QAction( tr("&Close"), this );
                fileCloseAction->setIcon( QIcon::fromTheme( "window-close" ) );
                fileCloseAction->setShortcut( QKeySequence::Close );
                fileCloseAction->setStatusTip( tr("Close the current window") );
                connect( fileCloseAction, SIGNAL(triggered()), this, SLOT(fileClose()) );
                registerAction( fileCloseAction );

                fileExitAction = new QAction( tr("E&xit"), this );
                fileExitAction->setIcon( QIcon::fromTheme( "application-exit" ) );
                fileExitAction->setShortcut( QKeySequence::Quit );
                fileExitAction->setStatusTip( tr("Exit glabels") );
                connect( fileExitAction, SIGNAL(triggered()), this, SLOT(fileExit()) );
                registerAction( fileExitAction );


                /* Edit actions */
                editUndoAction = new QAction( tr("Undo"), this );
                editUndoAction->setIcon( QIcon::fromTheme( "edit-undo" ) );
                editUndoAction->setShortcut( QKeySequence::Undo );
                editUndoAction->setStatusTip( tr("Undo") );
                connect( editUndoAction, SIGNAL(triggered()), this, SLOT(editUndo()) );
                registerAction( editUndoAction );

                editRedoAction = new QAction( tr("Redo"), this );
                editRedoAction->setIcon( QIcon::fromTheme( "edit-redo" ) );
                editRedoAction->setShortcut( QKeySequence::Redo );
                editRedoAction->setStatusTip( tr("Redo") );
                connect( editRedoAction, SIGNAL(triggered()), this, SLOT(editRedo()) );
                registerAction( editRedoAction );

                editCutAction = new QAction( tr("Cut"), this );
                editCutAction->setIcon( QIcon::fromTheme( "glabels-edit-cut" ) );
                editCutAction->setShortcut( QKeySequence::Cut );
                editCutAction->setStatusTip( tr("Cut the selection") );
                connect( editCutAction, SIGNAL(triggered()), this, SLOT(editCut()) );
                registerAction( editCutAction );

                editCopyAction = new QAction( tr("&Copy"), this );
                editCopyAction->setIcon( QIcon::fromTheme( "glabels-edit-copy" ) );
                editCopyAction->setShortcut( QKeySequence::Copy );
                editCopyAction->setStatusTip( tr("Copy the selection") );
                connect( editCopyAction, SIGNAL(triggered()), this, SLOT(editCopy()) );
                registerAction( editCopyAction );

                editPasteAction = new QAction( tr("&Paste"), this );
                editPasteAction->setIcon( QIcon::fromTheme( "glabels-edit-paste" ) );
                editPasteAction->setShortcut( QKeySequence::Paste );
                editPasteAction->setStatusTip( tr("Paste the clipboard") );
                connect( editPasteAction, SIGNAL(triggered()), this, SLOT(editPaste()) );
                registerAction( editPasteAction );

                editDeleteAction = new QAction( tr("&Delete"), this );
                editDeleteAction->setIcon( QIcon::fromTheme( "edit-delete" ) );
                editDeleteAction->setShortcut( QKeySequence::Delete );
                editDeleteAction->setStatusTip( tr("Delete the selected objects") );
                connect( editDeleteAction, SIGNAL(triggered()), this, SLOT(editDelete()) );
                registerAction( editDeleteAction );

                editSelectAllAction = new QAction( tr("Select &all"), this );
                editSelectAllAction->setIcon( QIcon::fromTheme( "edit-select-all" ) );
                editSelectAllAction->setShortcut( QKeySequence::SelectAll );
                editSelectAllAction->setStatusTip( tr("Select all objects") );
                connect( editSelectAllAction, SIGNAL(triggered()), this, SLOT(editSelectAll()) );
                registerAction( editSelectAllAction );

                editUnSelectAllAction = new QAction( tr("Unselect all"), this );
                editUnSelectAllAction->setIcon( QIcon::fromTheme( "edit-unselect-all" ) );
                editUnSelectAllAction->setStatusTip( tr("Remove all selections") );
                connect( editUnSelectAllAction, SIGNAL(triggered()), this, SLOT(editUnSelectAll()) );
                registerAction( editUnSelectAllAction );

                editPreferencesAction = new QAction( tr("Preferences..."), this );
                editPreferencesAction->setIcon( QIcon::fromTheme( "edit-preferences" ) );
                editPreferencesAction->setShortcut( QKeySequence::Preferences );
                editPreferencesAction->setStatusTip( tr("Configure the application") );
                connect( editPreferencesAction, SIGNAL(triggered()), this, SLOT(editPreferences()) );
                registerAction( editPreferencesAction );


                /* View actions */
                viewFileToolBarAction = new QAction( tr("Quick Access toolbar"), this );
                viewFileToolBarAction->setCheckable( true );
                viewFileToolBarAction->setStatusTip( tr("Change visibility of the \"Quick Access\" toolbar in current window") );
                connect( viewFileToolBarAction, SIGNAL(toggled(bool)), this, SLOT(viewFileToolBar(bool)) );
                registerAction( viewFileToolBarAction );

                viewContentPanelAction = new QAction( tr("Content panel"), this );
                viewContentPanelAction->setCheckable( true );
                viewContentPanelAction->setStatusTip( tr("Change visibility of the Content panel in current window") );
                connect( viewContentPanelAction, SIGNAL(toggled(bool)), this, SLOT(viewContentPanel(bool)) );
                registerAction( viewContentPanelAction );

                viewCollapseRibbonAction = new QAction( tr("Collapse ribbon"), this );
                viewCollapseRibbonAction->setCheckable( true );
                viewCollapseRibbonAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_F1 ) );
                viewCollapseRibbonAction->setStatusTip( tr("Collapse or expand the ribbon") );
                registerAction( viewCollapseRibbonAction );

                viewGridAction = new QAction( tr("Grid"), this );
                viewGridAction->setCheckable( true );
                viewGridAction->setStatusTip( tr("Change visibility of the grid in current window") );
                connect( viewGridAction, SIGNAL(toggled(bool)), this, SLOT(viewGrid(bool)) );
                registerAction( viewGridAction );

                viewMarkupAction = new QAction( tr("Markup"), this );
                viewMarkupAction->setCheckable( true );
                viewMarkupAction->setStatusTip( tr("Change visibility of markup lines in current window") );
                connect( viewMarkupAction, SIGNAL(toggled(bool)), this, SLOT(viewMarkup(bool)) );
                registerAction( viewMarkupAction );

                viewSnapObjectsAction = new QAction( tr("Objects"), this );
                viewSnapObjectsAction->setCheckable( true );
                viewSnapObjectsAction->setChecked( model::Settings::snapObjects() );
                viewSnapObjectsAction->setStatusTip( tr("Snap to edges and centres of other objects") );
                connect( viewSnapObjectsAction, &QAction::toggled, this, []( bool checked )
                {
                        if ( model::Settings::snapObjects() != checked )
                        {
                                model::Settings::setSnapObjects( checked );
                        }
                } );
                registerAction( viewSnapObjectsAction );

                viewSnapCenterAction = new QAction( tr("Centre"), this );
                viewSnapCenterAction->setCheckable( true );
                viewSnapCenterAction->setChecked( model::Settings::snapCenter() );
                viewSnapCenterAction->setStatusTip( tr("Snap to the horizontal and vertical centre of the label") );
                connect( viewSnapCenterAction, &QAction::toggled, this, []( bool checked )
                {
                        if ( model::Settings::snapCenter() != checked )
                        {
                                model::Settings::setSnapCenter( checked );
                        }
                } );
                registerAction( viewSnapCenterAction );

                viewSnapSafeAreaAction = new QAction( tr("Safe area"), this );
                viewSnapSafeAreaAction->setCheckable( true );
                viewSnapSafeAreaAction->setChecked( model::Settings::snapSafeArea() );
                viewSnapSafeAreaAction->setStatusTip( tr("Snap to the inside edges of the label margin") );
                connect( viewSnapSafeAreaAction, &QAction::toggled, this, []( bool checked )
                {
                        if ( model::Settings::snapSafeArea() != checked )
                        {
                                model::Settings::setSnapSafeArea( checked );
                        }
                } );
                registerAction( viewSnapSafeAreaAction );

                viewZoomInAction = new QAction( tr("Zoom &In"), this );
                viewZoomInAction->setIcon( QIcon::fromTheme( "glabels-zoom-in" ) );
                viewZoomInAction->setShortcut( QKeySequence::ZoomIn );
                viewZoomInAction->setStatusTip( tr("Increase magnification") );
                connect( viewZoomInAction, SIGNAL(triggered()), this, SLOT(viewZoomIn()) );
                registerAction( viewZoomInAction );

                viewZoomOutAction = new QAction( tr("Zoom &Out"), this );
                viewZoomOutAction->setIcon( QIcon::fromTheme( "glabels-zoom-out" ) );
                viewZoomOutAction->setShortcut( QKeySequence::ZoomOut );
                viewZoomOutAction->setStatusTip( tr("Decrease magnification") );
                connect( viewZoomOutAction, SIGNAL(triggered()), this, SLOT(viewZoomOut()) );
                registerAction( viewZoomOutAction );

                viewZoom1To1Action = new QAction( tr("Zoom &1 to 1"), this );
                viewZoom1To1Action->setIcon( QIcon::fromTheme( "glabels-zoom-one-to-one" ) );
                viewZoom1To1Action->setStatusTip( tr("Restore scale to 100%") );
                connect( viewZoom1To1Action, SIGNAL(triggered()), this, SLOT(viewZoom1To1()) );
                registerAction( viewZoom1To1Action );

                viewZoomToFitAction = new QAction( tr("Zoom to &Fit"), this );
                viewZoomToFitAction->setIcon( QIcon::fromTheme( "glabels-zoom-to-fit" ) );
                viewZoomToFitAction->setStatusTip( tr("Set scale to fit window") );
                connect( viewZoomToFitAction, SIGNAL(triggered()), this, SLOT(viewZoomToFit()) );
                registerAction( viewZoomToFitAction );


                /* Object actions */
                mInsertToolGroup = new QActionGroup( this );
                mInsertToolGroup->setExclusive( true );

                objectsArrowModeAction = new QAction( tr("Select"), this );
                objectsArrowModeAction->setIcon( QIcon::fromTheme( "glabels-arrow" ) );
                objectsArrowModeAction->setStatusTip( tr("Select, move and modify objects") );
                objectsArrowModeAction->setCheckable( true );
                objectsArrowModeAction->setChecked( true );
                mInsertToolGroup->addAction( objectsArrowModeAction );
                connect( objectsArrowModeAction, SIGNAL(triggered()), this, SLOT(objectsArrowMode()) );
                registerAction( objectsArrowModeAction );

                objectsCreateTextAction = new QAction( tr("Text"), this );
                objectsCreateTextAction->setIcon( QIcon::fromTheme( "glabels-text" ) );
                objectsCreateTextAction->setStatusTip( tr("Create text object") );
                objectsCreateTextAction->setCheckable( true );
                mInsertToolGroup->addAction( objectsCreateTextAction );
                connect( objectsCreateTextAction, SIGNAL(triggered()), this, SLOT(objectsCreateText()) );
                registerAction( objectsCreateTextAction );

                objectsCreateBoxAction = new QAction( tr("Box"), this );
                objectsCreateBoxAction->setIcon( QIcon::fromTheme( "glabels-box" ) );
                objectsCreateBoxAction->setStatusTip( tr("Create box object") );
                objectsCreateBoxAction->setCheckable( true );
                mInsertToolGroup->addAction( objectsCreateBoxAction );
                connect( objectsCreateBoxAction, SIGNAL(triggered()), this, SLOT(objectsCreateBox()) );
                registerAction( objectsCreateBoxAction );

                objectsCreateLineAction = new QAction( tr("Line"), this );
                objectsCreateLineAction->setIcon( QIcon::fromTheme( "glabels-line" ) );
                objectsCreateLineAction->setStatusTip( tr("Create line object") );
                objectsCreateLineAction->setCheckable( true );
                mInsertToolGroup->addAction( objectsCreateLineAction );
                connect( objectsCreateLineAction, SIGNAL(triggered()), this, SLOT(objectsCreateLine()) );
                registerAction( objectsCreateLineAction );

                objectsCreateEllipseAction = new QAction( tr("Ellipse"), this );
                objectsCreateEllipseAction->setIcon( QIcon::fromTheme( "glabels-ellipse" ) );
                objectsCreateEllipseAction->setStatusTip( tr("Create ellipse/circle object") );
                objectsCreateEllipseAction->setCheckable( true );
                mInsertToolGroup->addAction( objectsCreateEllipseAction );
                connect( objectsCreateEllipseAction, SIGNAL(triggered()), this, SLOT(objectsCreateEllipse()) );
                registerAction( objectsCreateEllipseAction );

                objectsCreateImageAction = new QAction( tr("Image"), this );
                objectsCreateImageAction->setIcon( QIcon::fromTheme( "glabels-image" ) );
                objectsCreateImageAction->setStatusTip( tr("Create image object") );
                objectsCreateImageAction->setCheckable( true );
                mInsertToolGroup->addAction( objectsCreateImageAction );
                connect( objectsCreateImageAction, SIGNAL(triggered()), this, SLOT(objectsCreateImage()) );
                registerAction( objectsCreateImageAction );

                objectsCreateBarcodeAction = new QAction( tr("Barcode"), this );
                objectsCreateBarcodeAction->setIcon( QIcon::fromTheme( "glabels-barcode" ) );
                objectsCreateBarcodeAction->setStatusTip( tr("Create barcode object") );
                objectsCreateBarcodeAction->setCheckable( true );
                mInsertToolGroup->addAction( objectsCreateBarcodeAction );
                connect( objectsCreateBarcodeAction, SIGNAL(triggered()), this, SLOT(objectsCreateBarcode()) );
                registerAction( objectsCreateBarcodeAction );

                objectsOrderRaiseAction = new QAction( tr("Bring to Front"), this );
                objectsOrderRaiseAction->setIcon( QIcon::fromTheme( "glabels-order-top" ) );
                objectsOrderRaiseAction->setStatusTip( tr("Raise selection to top") );
                connect( objectsOrderRaiseAction, SIGNAL(triggered()), this, SLOT(objectsOrderRaise()) );
                registerAction( objectsOrderRaiseAction );

                objectsOrderLowerAction = new QAction( tr("Send to Back"), this );
                objectsOrderLowerAction->setIcon( QIcon::fromTheme( "glabels-order-bottom" ) );
                objectsOrderLowerAction->setStatusTip( tr("Lower selection to bottom") );
                connect( objectsOrderLowerAction, SIGNAL(triggered()), this, SLOT(objectsOrderLower()) );
                registerAction( objectsOrderLowerAction );

                objectsXformRotateLeftAction = new QAction( tr("Rotate Left"), this );
                objectsXformRotateLeftAction->setIcon( QIcon::fromTheme( "glabels-rotate-left" ) );
                objectsXformRotateLeftAction->setStatusTip( tr("Rotate object(s) 90 degrees counter-clockwise") );
                connect( objectsXformRotateLeftAction, SIGNAL(triggered()), this, SLOT(objectsXformRotateLeft()) );
                registerAction( objectsXformRotateLeftAction );

                objectsXformRotateRightAction = new QAction( tr("Rotate Right"), this );
                objectsXformRotateRightAction->setIcon( QIcon::fromTheme( "glabels-rotate-right" ) );
                objectsXformRotateRightAction->setStatusTip( tr("Rotate object(s) 90 degrees clockwise") );
                connect( objectsXformRotateRightAction, SIGNAL(triggered()), this, SLOT(objectsXformRotateRight()) );
                registerAction( objectsXformRotateRightAction );

                objectsXformFlipHorizAction = new QAction( tr("Flip Horizontally"), this );
                objectsXformFlipHorizAction->setIcon( QIcon::fromTheme( "glabels-flip-horiz" ) );
                objectsXformFlipHorizAction->setStatusTip( tr("Flip object(s) horizontally") );
                connect( objectsXformFlipHorizAction, SIGNAL(triggered()), this, SLOT(objectsXformFlipHoriz()) );
                registerAction( objectsXformFlipHorizAction );

                objectsXformFlipVertAction = new QAction( tr("Flip Vertically"), this );
                objectsXformFlipVertAction->setIcon( QIcon::fromTheme( "glabels-flip-vert" ) );
                objectsXformFlipVertAction->setStatusTip( tr("Flip object(s) vertically") );
                connect( objectsXformFlipVertAction, SIGNAL(triggered()), this, SLOT(objectsXformFlipVert()) );
                registerAction( objectsXformFlipVertAction );

                objectsAlignLeftAction = new QAction( tr("Align Left"), this );
                objectsAlignLeftAction->setIcon( QIcon::fromTheme( "glabels-align-left" ) );
                objectsAlignLeftAction->setStatusTip( tr("Align objects to left edges") );
                connect( objectsAlignLeftAction, SIGNAL(triggered()), this, SLOT(objectsAlignLeft()) );
                registerAction( objectsAlignLeftAction );

                objectsAlignHCenterAction = new QAction( tr("Align Center"), this );
                objectsAlignHCenterAction->setIcon( QIcon::fromTheme( "glabels-align-hcenter" ) );
                objectsAlignHCenterAction->setStatusTip( tr("Align objects to horizontal centers") );
                connect( objectsAlignHCenterAction, SIGNAL(triggered()), this, SLOT(objectsAlignHCenter()) );
                registerAction( objectsAlignHCenterAction );

                objectsAlignRightAction = new QAction( tr("Align Right"), this );
                objectsAlignRightAction->setIcon( QIcon::fromTheme( "glabels-align-right" ) );
                objectsAlignRightAction->setStatusTip( tr("Align objects to right edges") );
                connect( objectsAlignRightAction, SIGNAL(triggered()), this, SLOT(objectsAlignRight()) );
                registerAction( objectsAlignRightAction );

                objectsAlignTopAction = new QAction( tr("Align Top"), this );
                objectsAlignTopAction->setIcon( QIcon::fromTheme( "glabels-align-top" ) );
                objectsAlignTopAction->setStatusTip( tr("Align objects to top edges") );
                connect( objectsAlignTopAction, SIGNAL(triggered()), this, SLOT(objectsAlignTop()) );
                registerAction( objectsAlignTopAction );

                objectsAlignVCenterAction = new QAction( tr("Align Middle"), this );
                objectsAlignVCenterAction->setIcon( QIcon::fromTheme( "glabels-align-vcenter" ) );
                objectsAlignVCenterAction->setStatusTip( tr("Align objects to vertical centers") );
                connect( objectsAlignVCenterAction, SIGNAL(triggered()), this, SLOT(objectsAlignVCenter()) );
                registerAction( objectsAlignVCenterAction );

                objectsAlignBottomAction = new QAction( tr("Align Bottom"), this );
                objectsAlignBottomAction->setIcon( QIcon::fromTheme( "glabels-align-bottom" ) );
                objectsAlignBottomAction->setStatusTip( tr("Align objects to bottom edges") );
                connect( objectsAlignBottomAction, SIGNAL(triggered()), this, SLOT(objectsAlignBottom()) );
                registerAction( objectsAlignBottomAction );

                objectsCenterHorizAction = new QAction( tr("Center Horizontally"), this );
                objectsCenterHorizAction->setIcon( QIcon::fromTheme( "glabels-center-horiz" ) );
                objectsCenterHorizAction->setStatusTip( tr("Horizontally center objects in label") );
                connect( objectsCenterHorizAction, SIGNAL(triggered()), this, SLOT(objectsCenterHoriz()) );
                registerAction( objectsCenterHorizAction );

                objectsCenterVertAction = new QAction( tr("Center Vertically"), this );
                objectsCenterVertAction->setIcon( QIcon::fromTheme( "glabels-center-vert" ) );
                objectsCenterVertAction->setStatusTip( tr("Vertically center objects in label") );
                connect( objectsCenterVertAction, SIGNAL(triggered()), this, SLOT(objectsCenterVert()) );
                registerAction( objectsCenterVertAction );

                objectsCenterAction = new QAction( tr("Center Both"), this );
                objectsCenterAction->setIcon( QIcon::fromTheme( "glabels-center" ) );
                objectsCenterAction->setStatusTip( tr("Center objects in label") );
                connect( objectsCenterAction, SIGNAL(triggered()), this, SLOT(objectsCenter()) );
                registerAction( objectsCenterAction );


                /* Help actions */
                helpContentsAction = new QAction( tr("&User Manual"), this );
                helpContentsAction->setIcon( QIcon::fromTheme( "help-contents" ) );
                helpContentsAction->setShortcut( QKeySequence::HelpContents );
                helpContentsAction->setStatusTip( tr("Open gLabels manual") );
                connect( helpContentsAction, SIGNAL(triggered()), this, SLOT(helpContents()) );
                registerAction( helpContentsAction );

                helpReportBugAction = new QAction( tr("&Report a Problem"), this );
                helpReportBugAction->setStatusTip( tr("Report a problem to the developers") );
                connect( helpReportBugAction, SIGNAL(triggered()), this, SLOT(helpReportBug()) );
                registerAction( helpReportBugAction );

                helpCheckForUpdatesAction = new QAction( tr("Check for &Updates"), this );
                helpCheckForUpdatesAction->setStatusTip( tr("Look for a newer version of the program") );
                connect( helpCheckForUpdatesAction, &QAction::triggered,
                         UpdateUi::instance(), &UpdateUi::checkForUpdatesRequested );
                registerAction( helpCheckForUpdatesAction );

                helpAboutAction = new QAction( tr("&About"), this );
                helpAboutAction->setIcon( QIcon::fromTheme( "help-about" ) );
                helpAboutAction->setStatusTip( tr("About gLabels") );
                connect( helpAboutAction, SIGNAL(triggered()), this, SLOT(helpAbout()) );
                registerAction( helpAboutAction );


                /* Context menu version of edit actions (unchanged) */
                contextCutAction = new QAction( tr("Cut"), this );
                contextCutAction->setIcon( QIcon::fromTheme( "glabels-edit-cut" ) );
                contextCutAction->setStatusTip( tr("Cut the selection") );
                connect( contextCutAction, SIGNAL(triggered()), this, SLOT(editCut()) );

                contextCopyAction = new QAction( tr("&Copy"), this );
                contextCopyAction->setIcon( QIcon::fromTheme( "glabels-edit-copy" ) );
                contextCopyAction->setStatusTip( tr("Copy the selection") );
                connect( contextCopyAction, SIGNAL(triggered()), this, SLOT(editCopy()) );

                contextPasteAction = new QAction( tr("&Paste"), this );
                contextPasteAction->setIcon( QIcon::fromTheme( "glabels-edit-paste" ) );
                contextPasteAction->setStatusTip( tr("Paste the clipboard") );
                connect( contextPasteAction, SIGNAL(triggered()), this, SLOT(editContextPaste()) );

                contextDeleteAction = new QAction( tr("&Delete"), this );
                contextDeleteAction->setIcon( QIcon::fromTheme( "edit-delete" ) );
                contextDeleteAction->setStatusTip( tr("Delete the selected objects") );
                connect( contextDeleteAction, SIGNAL(triggered()), this, SLOT(editDelete()) );
        }


        ///
        /// Create Canvas Context Menus (unchanged; menuBar is removed per SPEC 3.1)
        ///
        void MainWindow::createContextMenus()
        {
                contextMenu = new QMenu( this );
                contextOrderMenu = contextMenu->addMenu( tr("&Order") );
                contextOrderMenu->addAction( objectsOrderRaiseAction );
                contextOrderMenu->addAction( objectsOrderLowerAction );
                contextXformMenu = contextMenu->addMenu( tr("&Rotate/Flip") );
                contextXformMenu->addAction( objectsXformRotateLeftAction );
                contextXformMenu->addAction( objectsXformRotateRightAction );
                contextXformMenu->addAction( objectsXformFlipHorizAction );
                contextXformMenu->addAction( objectsXformFlipVertAction );
                contextAlignMenu = contextMenu->addMenu( tr("&Alignment") );
                contextAlignMenu->addAction( objectsAlignLeftAction );
                contextAlignMenu->addAction( objectsAlignHCenterAction );
                contextAlignMenu->addAction( objectsAlignRightAction );
                contextAlignMenu->addSeparator();
                contextAlignMenu->addAction( objectsAlignTopAction );
                contextAlignMenu->addAction( objectsAlignVCenterAction );
                contextAlignMenu->addAction( objectsAlignBottomAction );
                contextCenterMenu = contextMenu->addMenu( tr("Center") );
                contextCenterMenu->addAction( objectsCenterHorizAction );
                contextCenterMenu->addAction( objectsCenterVertAction );
                contextCenterMenu->addAction( objectsCenterAction );
                contextMenu->addSeparator();
                contextMenu->addAction( contextCutAction );
                contextMenu->addAction( contextCopyAction );
                contextMenu->addAction( contextPasteAction );
                contextMenu->addAction( contextDeleteAction );

                noSelectionContextMenu = new QMenu( this );
                noSelectionContextMenu->addAction( contextPasteAction );
        }


        ///
        /// Create Ribbon Bar and populate all tabs (SPEC 6.1 & 6.2)
        ///
        void MainWindow::createRibbon()
        {
                mRibbonBar = new ribbon::RibbonBar( this );

                // Quick Access toolbar: New, Open, Save, Undo, Redo (SPEC 3.1)
                auto* qat = mRibbonBar->quickAccessBar();
                qat->addAction( fileNewAction,  QStringLiteral( "file-plus" ) );
                qat->addAction( fileOpenAction, QStringLiteral( "folder-open" ) );
                qat->addAction( fileSaveAction, QStringLiteral( "save" ) );
                qat->addAction( editUndoAction, QStringLiteral( "undo-2" ) );
                qat->addAction( editRedoAction, QStringLiteral( "redo-2" ) );

                // Normal tabs
                mRibbonBar->addTab( ribbon::TabId::Home,   tr( "Home" ),   false );
                mRibbonBar->addTab( ribbon::TabId::Label,  tr( "Label" ),  false );
                mRibbonBar->addTab( ribbon::TabId::Data,   tr( "Data" ),   false );
                mRibbonBar->addTab( ribbon::TabId::Print,  tr( "Print" ),  false );
                mRibbonBar->addTab( ribbon::TabId::Deploy, tr( "Deploy" ), false );
                mRibbonBar->addTab( ribbon::TabId::View,   tr( "View" ),   false );
                mRibbonBar->addTab( ribbon::TabId::Help,   tr( "Help" ),   false );

                // Associate page-switching actions with their ribbon tabs (for tooltips & coverage)
                mRibbonBar->registerTabAction( ribbon::TabId::Home,   fileShowEditorPageAction );
                mRibbonBar->registerTabAction( ribbon::TabId::Label,  fileShowPropertiesPageAction );
                mRibbonBar->registerTabAction( ribbon::TabId::Print,  fileShowPrintPageAction );
                mRibbonBar->registerTabAction( ribbon::TabId::Deploy, fileShowDeployPageAction );

                // Contextual tabs
                mRibbonBar->addTab( ribbon::TabId::ContextText,    tr( "Text" ),    true );
                mRibbonBar->addTab( ribbon::TabId::ContextBarcode, tr( "Barcode" ), true );
                mRibbonBar->addTab( ribbon::TabId::ContextImage,   tr( "Image" ),   true );
                mRibbonBar->addTab( ribbon::TabId::ContextShape,   tr( "Shape" ),   true );
                mRibbonBar->addTab( ribbon::TabId::ContextLine,    tr( "Line" ),    true );

                populateHomeTab();
                populateLabelTab();
                populateDataTab();
                populatePrintTab();
                populateDeployTab();
                populateViewTab();
                populateHelpTab();
                populateContextualTabs();

                connect( mRibbonBar, &ribbon::RibbonBar::tabChanged,
                         this, &MainWindow::onRibbonTabChanged );
                connect( mRibbonBar, &ribbon::RibbonBar::fileButtonClicked,
                         this, &MainWindow::onFileButtonClicked );
                connect( viewCollapseRibbonAction, &QAction::toggled,
                         mRibbonBar, &ribbon::RibbonBar::setCollapsed );
                connect( mRibbonBar, &ribbon::RibbonBar::collapsedChanged,
                         viewCollapseRibbonAction, &QAction::setChecked );
        }


        void MainWindow::populateHomeTab()
        {
                using Btn = ribbon::RibbonButton;
                ribbon::RibbonTab* home = mRibbonBar->tab( ribbon::TabId::Home );

                // 1. Clipboard (collapse priority 1: collapses last among collapsible groups)
                auto* clipGrp = home->addGroup( tr( "Clipboard" ), QStringLiteral( "clipboard-paste" ), 1 );
                clipGrp->addWidget( new Btn( editPasteAction, QStringLiteral( "clipboard-paste" ), Btn::Size::Large, Btn::Variant::Standard, clipGrp ) );
                auto* clipCol = new QVBoxLayout();
                clipCol->setContentsMargins( 0, 0, 0, 0 );
                clipCol->setSpacing( 1 );
                clipCol->addWidget( new Btn( editCutAction,    QStringLiteral( "scissors" ), Btn::Size::Small, Btn::Variant::Standard, clipGrp ) );
                clipCol->addWidget( new Btn( editCopyAction,   QStringLiteral( "copy" ),     Btn::Size::Small, Btn::Variant::Standard, clipGrp ) );
                clipCol->addWidget( new Btn( editDeleteAction, QStringLiteral( "trash-2" ),  Btn::Size::Small, Btn::Variant::Standard, clipGrp ) );
                clipGrp->addLayout( clipCol );

                // 2. Edit (collapse priority 2)
                auto* editGrp = home->addGroup( tr( "Edit" ), QStringLiteral( "undo-2" ), 2 );
                auto* undoCol = new QVBoxLayout();
                undoCol->setContentsMargins( 0, 12, 0, 0 );
                undoCol->setSpacing( 2 );
                undoCol->addWidget( new Btn( editUndoAction, QStringLiteral( "undo-2" ), Btn::Size::Small, Btn::Variant::Standard, editGrp ) );
                undoCol->addWidget( new Btn( editRedoAction, QStringLiteral( "redo-2" ), Btn::Size::Small, Btn::Variant::Standard, editGrp ) );
                editGrp->addLayout( undoCol );

                auto* selCol = new QVBoxLayout();
                selCol->setContentsMargins( 4, 12, 0, 0 );
                selCol->setSpacing( 2 );
                selCol->addWidget( new Btn( editSelectAllAction,   QStringLiteral( "square-dashed-mouse-pointer" ), Btn::Size::Small, Btn::Variant::Standard, editGrp ) );
                selCol->addWidget( new Btn( editUnSelectAllAction, QStringLiteral( "square-dashed" ),               Btn::Size::Small, Btn::Variant::Standard, editGrp ) );
                editGrp->addLayout( selCol );

                // 3. Insert object (collapse priority 0: stays expanded longest)
                auto* insertGrp = home->addGroup( tr( "Insert object" ), QStringLiteral( "type" ), 0 );
                insertGrp->addWidget( new Btn( objectsArrowModeAction,     QStringLiteral( "mouse-pointer-2" ), Btn::Size::Large, Btn::Variant::Standard, insertGrp ) );
                insertGrp->addWidget( new Btn( objectsCreateTextAction,    QStringLiteral( "type" ),            Btn::Size::Large, Btn::Variant::Standard, insertGrp ) );
                insertGrp->addWidget( new Btn( objectsCreateBoxAction,     QStringLiteral( "square" ),          Btn::Size::Large, Btn::Variant::Standard, insertGrp ) );
                insertGrp->addWidget( new Btn( objectsCreateLineAction,    QStringLiteral( "slash" ),           Btn::Size::Large, Btn::Variant::Standard, insertGrp ) );
                insertGrp->addWidget( new Btn( objectsCreateEllipseAction, QStringLiteral( "circle" ),          Btn::Size::Large, Btn::Variant::Standard, insertGrp ) );
                insertGrp->addWidget( new Btn( objectsCreateImageAction,   QStringLiteral( "image" ),           Btn::Size::Large, Btn::Variant::Standard, insertGrp ) );
                insertGrp->addWidget( new Btn( objectsCreateBarcodeAction, QStringLiteral( "barcode" ),         Btn::Size::Large, Btn::Variant::Standard, insertGrp ) );

                // 4. Arrange (collapse priority 3)
                auto* arrangeGrp = home->addGroup( tr( "Arrange" ), QStringLiteral( "bring-to-front" ), 3 );
                auto* orderCol = new QVBoxLayout();
                orderCol->setContentsMargins( 0, 12, 0, 0 );
                orderCol->setSpacing( 2 );
                orderCol->addWidget( new Btn( objectsOrderRaiseAction, QStringLiteral( "bring-to-front" ), Btn::Size::Small, Btn::Variant::Standard, arrangeGrp ) );
                orderCol->addWidget( new Btn( objectsOrderLowerAction, QStringLiteral( "send-to-back" ),   Btn::Size::Small, Btn::Variant::Standard, arrangeGrp ) );
                arrangeGrp->addLayout( orderCol );

                auto* alignGrid = new QGridLayout();
                alignGrid->setContentsMargins( 6, 8, 0, 0 );
                alignGrid->setSpacing( 2 );
                alignGrid->addWidget( new Btn( objectsAlignLeftAction,    QStringLiteral( "align-start-vertical" ),    Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 0, 0 );
                alignGrid->addWidget( new Btn( objectsAlignHCenterAction, QStringLiteral( "align-center-vertical" ),   Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 0, 1 );
                alignGrid->addWidget( new Btn( objectsAlignRightAction,   QStringLiteral( "align-end-vertical" ),      Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 0, 2 );
                alignGrid->addWidget( new Btn( objectsAlignTopAction,     QStringLiteral( "align-start-horizontal" ),  Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 1, 0 );
                alignGrid->addWidget( new Btn( objectsAlignVCenterAction, QStringLiteral( "align-center-horizontal" ), Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 1, 1 );
                alignGrid->addWidget( new Btn( objectsAlignBottomAction,  QStringLiteral( "align-end-horizontal" ),    Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 1, 2 );
                arrangeGrp->addLayout( alignGrid );

                auto* xformGrid = new QGridLayout();
                xformGrid->setContentsMargins( 6, 8, 0, 0 );
                xformGrid->setSpacing( 2 );
                xformGrid->addWidget( new Btn( objectsXformRotateLeftAction,  QStringLiteral( "rotate-ccw" ),        Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 0, 0 );
                xformGrid->addWidget( new Btn( objectsXformRotateRightAction, QStringLiteral( "rotate-cw" ),         Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 0, 1 );
                xformGrid->addWidget( new Btn( objectsXformFlipHorizAction,   QStringLiteral( "flip-horizontal-2" ), Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 1, 0 );
                xformGrid->addWidget( new Btn( objectsXformFlipVertAction,    QStringLiteral( "flip-vertical-2" ),   Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 1, 1 );
                arrangeGrp->addLayout( xformGrid );

                auto* centerRow = new QHBoxLayout();
                centerRow->setContentsMargins( 6, 8, 0, 0 );
                centerRow->setSpacing( 2 );
                centerRow->addWidget( new Btn( objectsCenterHorizAction, QStringLiteral( "align-horizontal-justify-center" ), Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ) );
                centerRow->addWidget( new Btn( objectsCenterVertAction,  QStringLiteral( "align-vertical-justify-center" ),   Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ) );
                centerRow->addWidget( new Btn( objectsCenterAction,      QStringLiteral( "crosshair" ),                       Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ) );
                arrangeGrp->addLayout( centerRow );

                // 5. Zoom (collapse priority 4: collapses first per SPEC 5.2)
                auto* zoomGrp = home->addGroup( tr( "Zoom" ), QStringLiteral( "zoom-in" ), 4 );
                auto* zoomGrid = new QGridLayout();
                zoomGrid->setContentsMargins( 0, 8, 0, 0 );
                zoomGrid->setSpacing( 2 );
                zoomGrid->addWidget( new Btn( viewZoomInAction,    QStringLiteral( "zoom-in" ),   Btn::Size::IconOnly, Btn::Variant::Standard, zoomGrp ), 0, 0 );
                zoomGrid->addWidget( new Btn( viewZoomOutAction,   QStringLiteral( "zoom-out" ),  Btn::Size::IconOnly, Btn::Variant::Standard, zoomGrp ), 0, 1 );
                zoomGrid->addWidget( new Btn( viewZoom1To1Action,  QStringLiteral( "scan-line" ), Btn::Size::IconOnly, Btn::Variant::Standard, zoomGrp ), 1, 0 );
                zoomGrid->addWidget( new Btn( viewZoomToFitAction, QStringLiteral( "scan" ),      Btn::Size::IconOnly, Btn::Variant::Standard, zoomGrp ), 1, 1 );
                zoomGrp->addLayout( zoomGrid );
        }


        void MainWindow::populateLabelTab()
        {
                using Btn = ribbon::RibbonButton;
                ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::Label );

                // Product
                auto* prodGrp = tab->addGroup( tr( "Product" ), QStringLiteral( "tag" ) );
                auto* changeBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, prodGrp );
                changeBtn->setText( tr( "Change product" ) );
                changeBtn->setIconName( QStringLiteral( "tag" ) );
                changeBtn->setToolTip( tr( "Select another product for this gLabels project." ) );
                connect( changeBtn, &QAbstractButton::clicked,
                         mPropertiesView->changeProductButton, &QAbstractButton::click );
                prodGrp->addWidget( changeBtn );

                // Current
                auto* curGrp = tab->addGroup( tr( "Current" ), QStringLiteral( "info" ) );
                auto* curCol = new QVBoxLayout();
                curCol->setContentsMargins( 2, 4, 2, 0 );
                curCol->setSpacing( 4 );
                auto* cap = new QLabel( tr( "Selected product" ), curGrp );
                cap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                mLabelTabName = new QLabel( curGrp );
                mLabelTabName->setProperty( "role", QStringLiteral( "cardTitle" ) );
                mLabelTabDetails = new QLabel( curGrp );
                mLabelTabDetails->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                curCol->addWidget( cap );
                curCol->addWidget( mLabelTabName );
                curCol->addWidget( mLabelTabDetails );
                curGrp->addLayout( curCol );

                // Parameters (visible only for continuous/adjustable templates)
                mLabelTabParamsGroup = tab->addGroup( tr( "Parameters" ), QStringLiteral( "ruler" ) );
                auto* pCol = new QVBoxLayout();
                pCol->setContentsMargins( 2, 4, 2, 0 );
                pCol->setSpacing( 4 );
                mLabelTabLengthSpin = new QDoubleSpinBox( mLabelTabParamsGroup );
                mLabelTabLengthSpin->setFixedWidth( 110 );
                auto* lenField = new ribbon::RibbonField( tr( "Label length" ), mLabelTabLengthSpin, 110,
                                                          ribbon::RibbonField::LabelPosition::Above, 0,
                                                          mLabelTabParamsGroup );
                pCol->addWidget( lenField );
                auto* adjCap = new QLabel( tr( "Adjustable parameter" ), mLabelTabParamsGroup );
                adjCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                pCol->addWidget( adjCap );
                mLabelTabParamsGroup->addLayout( pCol );
                mLabelTabParamsGroup->hide();

                connect( mLabelTabLengthSpin, QOverload<double>::of( &QDoubleSpinBox::valueChanged ),
                         this, [this]( double v )
                         {
                                 if ( !mSyncingRibbon && mPropertiesView )
                                 {
                                         mPropertiesView->lengthSpin->setValue( v );
                                 }
                         } );
                connect( mPropertiesView->lengthSpin, QOverload<double>::of( &QDoubleSpinBox::valueChanged ),
                         this, [this]( double v )
                         {
                                 if ( mLabelTabLengthSpin )
                                 {
                                         const QSignalBlocker b( mLabelTabLengthSpin );
                                         mLabelTabLengthSpin->setValue( v );
                                 }
                         } );

                // Orientation
                auto* oriGrp = tab->addGroup( tr( "Orientation" ), QStringLiteral( "rotate-cw" ) );
                mLabelTabOrientationCombo = new QComboBox( oriGrp );
                mLabelTabOrientationCombo->addItem( tr( "Horizontal" ) );
                mLabelTabOrientationCombo->addItem( tr( "Vertical" ) );
                auto* oriField = new ribbon::RibbonField( tr( "Orientation" ), mLabelTabOrientationCombo, 130,
                                                          ribbon::RibbonField::LabelPosition::Above, 0, oriGrp );
                oriGrp->addWidget( oriField );

                connect( mLabelTabOrientationCombo, QOverload<int>::of( &QComboBox::activated ),
                         this, [this]( int idx )
                         {
                                 if ( !mSyncingRibbon && mPropertiesView )
                                 {
                                         mPropertiesView->orientationCombo->setCurrentIndex( idx );
                                         QMetaObject::invokeMethod( mPropertiesView, "onOrientationActivated" );
                                 }
                         } );
                connect( mPropertiesView->orientationCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ),
                         this, [this]( int idx )
                         {
                                 if ( mLabelTabOrientationCombo )
                                 {
                                         const QSignalBlocker b( mLabelTabOrientationCombo );
                                         mLabelTabOrientationCombo->setCurrentIndex( idx );
                                 }
                         } );
        }


        void MainWindow::populateDataTab()
        {
                using Btn = ribbon::RibbonButton;
                ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::Data );

                // Data page switcher (three large exclusive toggles)
                auto* switcherGrp = tab->addGroup( tr( "Data page" ), QStringLiteral( "database" ) );
                mDataMergeBtn = new Btn( fileShowMergePageAction, QStringLiteral( "database" ), Btn::Size::Large, Btn::Variant::Standard, switcherGrp );
                mDataMergeBtn->setCheckable( true );
                mDataVariablesBtn = new Btn( fileShowVariablesPageAction, QStringLiteral( "braces" ), Btn::Size::Large, Btn::Variant::Standard, switcherGrp );
                mDataVariablesBtn->setCheckable( true );
                mDataFillBtn = new Btn( fileShowFillPageAction, QStringLiteral( "table-2" ), Btn::Size::Large, Btn::Variant::Standard, switcherGrp );
                mDataFillBtn->setCheckable( true );
                switcherGrp->addWidget( mDataMergeBtn );
                switcherGrp->addWidget( mDataVariablesBtn );
                switcherGrp->addWidget( mDataFillBtn );

                // --- Merge sub-page groups ---
                mDataMergeSourceGroup = tab->addGroup( tr( "Source" ), QStringLiteral( "folder-open" ) );
                auto* mergeBrowseBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataMergeSourceGroup );
                mergeBrowseBtn->setText( tr( "Browse..." ) );
                mergeBrowseBtn->setIconName( QStringLiteral( "folder-open" ) );
                connect( mergeBrowseBtn, &QAbstractButton::clicked,
                         mMergeView->locationBrowseButton, &QAbstractButton::click );
                mDataMergeSourceGroup->addWidget( mergeBrowseBtn );

                auto* mergeReloadBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataMergeSourceGroup );
                mergeReloadBtn->setText( tr( "Reload" ) );
                mergeReloadBtn->setIconName( QStringLiteral( "refresh-cw" ) );
                connect( mergeReloadBtn, &QAbstractButton::clicked,
                         mMergeView->pushButton, &QAbstractButton::click );
                mDataMergeSourceGroup->addWidget( mergeReloadBtn );

                mDataMergeRecordsGroup = tab->addGroup( tr( "Records" ), QStringLiteral( "square-dashed-mouse-pointer" ) );
                auto* recCol = new QVBoxLayout();
                recCol->setContentsMargins( 0, 8, 0, 0 );
                recCol->setSpacing( 2 );
                auto* mergeSelAll = new Btn( Btn::Size::Small, Btn::Variant::Standard, mDataMergeRecordsGroup );
                mergeSelAll->setText( tr( "Select all" ) );
                mergeSelAll->setIconName( QStringLiteral( "square-dashed-mouse-pointer" ) );
                connect( mergeSelAll, &QAbstractButton::clicked,
                         mMergeView->selectAllButton, &QAbstractButton::click );
                recCol->addWidget( mergeSelAll );

                auto* mergeUnselAll = new Btn( Btn::Size::Small, Btn::Variant::Standard, mDataMergeRecordsGroup );
                mergeUnselAll->setText( tr( "Unselect all" ) );
                mergeUnselAll->setIconName( QStringLiteral( "square-dashed" ) );
                connect( mergeUnselAll, &QAbstractButton::clicked,
                         mMergeView->unselectAllButton, &QAbstractButton::click );
                recCol->addWidget( mergeUnselAll );
                mDataMergeRecordsGroup->addLayout( recCol );

                // --- Variables sub-page groups ---
                mDataVarsGroup = tab->addGroup( tr( "Variables" ), QStringLiteral( "braces" ) );
                auto* varAdd = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataVarsGroup );
                varAdd->setText( tr( "Add variable" ) );
                varAdd->setIconName( QStringLiteral( "plus" ) );
                connect( varAdd, &QAbstractButton::clicked, mVariablesView->addButton, &QAbstractButton::click );
                mDataVarsGroup->addWidget( varAdd );

                mVarsEditRibbonBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataVarsGroup );
                mVarsEditRibbonBtn->setText( tr( "Edit" ) );
                mVarsEditRibbonBtn->setIconName( QStringLiteral( "pencil" ) );
                mVarsEditRibbonBtn->setEnabled( false );
                connect( mVarsEditRibbonBtn, &QAbstractButton::clicked, mVariablesView->editButton, &QAbstractButton::click );
                mDataVarsGroup->addWidget( mVarsEditRibbonBtn );

                mVarsDeleteRibbonBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataVarsGroup );
                mVarsDeleteRibbonBtn->setText( tr( "Delete" ) );
                mVarsDeleteRibbonBtn->setIconName( QStringLiteral( "trash-2" ) );
                mVarsDeleteRibbonBtn->setEnabled( false );
                connect( mVarsDeleteRibbonBtn, &QAbstractButton::clicked, mVariablesView->deleteButton, &QAbstractButton::click );
                mDataVarsGroup->addWidget( mVarsDeleteRibbonBtn );

                connect( mVariablesView->table, &QTableWidget::itemSelectionChanged, this, [this]()
                {
                        const bool hasSel = !mVariablesView->table->selectedItems().isEmpty();
                        mVarsEditRibbonBtn->setEnabled( hasSel );
                        mVarsDeleteRibbonBtn->setEnabled( hasSel );
                } );

                // --- Fill sub-page groups ---
                mDataFillRowsGroup = tab->addGroup( tr( "Rows" ), QStringLiteral( "plus" ) );
                auto* fillAdd = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillRowsGroup );
                fillAdd->setText( tr( "Add row" ) );
                fillAdd->setIconName( QStringLiteral( "plus" ) );
                connect( fillAdd, &QAbstractButton::clicked, mFillView->addRowButton, &QAbstractButton::click );
                mDataFillRowsGroup->addWidget( fillAdd );

                auto* fillRem = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillRowsGroup );
                fillRem->setText( tr( "Remove" ) );
                fillRem->setIconName( QStringLiteral( "minus" ) );
                connect( fillRem, &QAbstractButton::clicked, mFillView->removeRowButton, &QAbstractButton::click );
                mDataFillRowsGroup->addWidget( fillRem );

                auto* fillClear = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillRowsGroup );
                fillClear->setText( tr( "Clear" ) );
                fillClear->setIconName( QStringLiteral( "x" ) );
                connect( fillClear, &QAbstractButton::clicked, mFillView->clearButton, &QAbstractButton::click );
                mDataFillRowsGroup->addWidget( fillClear );

                mDataFillLiveGroup = tab->addGroup( tr( "Live data" ), QStringLiteral( "database" ) );
                auto* fillSheets = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillLiveGroup );
                fillSheets->setText( tr( "Google Sheets..." ) );
                fillSheets->setIconName( QStringLiteral( "file-spreadsheet" ) );
                connect( fillSheets, &QAbstractButton::clicked,
                         mFillView->linkGoogleSheetsButton, &QAbstractButton::click );
                mDataFillLiveGroup->addWidget( fillSheets );

                auto* fillRedash = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillLiveGroup );
                fillRedash->setText( tr( "Redash..." ) );
                fillRedash->setIconName( QStringLiteral( "database" ) );
                connect( fillRedash, &QAbstractButton::clicked,
                         mFillView->linkExternalSourceButton, &QAbstractButton::click );
                mDataFillLiveGroup->addWidget( fillRedash );

                mFillRefreshSourceRibbonBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillLiveGroup );
                mFillRefreshSourceRibbonBtn->setText( tr( "Refresh" ) );
                mFillRefreshSourceRibbonBtn->setIconName( QStringLiteral( "refresh-cw" ) );
                mFillRefreshSourceRibbonBtn->setEnabled( false );
                connect( mFillRefreshSourceRibbonBtn, &QAbstractButton::clicked,
                         mFillView->refreshExternalSourceButton, &QAbstractButton::click );
                mDataFillLiveGroup->addWidget( mFillRefreshSourceRibbonBtn );

                connect( mFillView, &FillView::externalSourceStateChanged, this,
                         [this]( bool hasSource, bool isRefreshing )
                         {
                                 if ( mFillRefreshSourceRibbonBtn )
                                 {
                                         mFillRefreshSourceRibbonBtn->setEnabled( hasSource && !isRefreshing );
                                 }
                         } );

                mDataFillIoGroup = tab->addGroup( tr( "Import and export" ), QStringLiteral( "download" ) );
                auto* impCsv = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillIoGroup );
                impCsv->setText( tr( "Import CSV" ) );
                impCsv->setIconName( QStringLiteral( "download" ) );
                connect( impCsv, &QAbstractButton::clicked, mFillView->importCsvButton, &QAbstractButton::click );
                mDataFillIoGroup->addWidget( impCsv );

                auto* impXlsx = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillIoGroup );
                impXlsx->setText( tr( "Import Excel" ) );
                impXlsx->setIconName( QStringLiteral( "download" ) );
                connect( impXlsx, &QAbstractButton::clicked, mFillView->importExcelButton, &QAbstractButton::click );
                mDataFillIoGroup->addWidget( impXlsx );

                auto* expCsv = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillIoGroup );
                expCsv->setText( tr( "Export CSV" ) );
                expCsv->setIconName( QStringLiteral( "upload" ) );
                connect( expCsv, &QAbstractButton::clicked, mFillView->exportCsvButton, &QAbstractButton::click );
                mDataFillIoGroup->addWidget( expCsv );

                auto* expXlsx = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillIoGroup );
                expXlsx->setText( tr( "Export Excel" ) );
                expXlsx->setIconName( QStringLiteral( "upload" ) );
                connect( expXlsx, &QAbstractButton::clicked, mFillView->exportExcelButton, &QAbstractButton::click );
                mDataFillIoGroup->addWidget( expXlsx );

                mDataFillOutputGroup = tab->addGroup( tr( "Output" ), QStringLiteral( "printer" ) );
                mFillPrintRibbonBtn = new Btn( Btn::Size::Large, Btn::Variant::Primary, mDataFillOutputGroup );
                mFillPrintRibbonBtn->setText( tr( "Print" ) );
                mFillPrintRibbonBtn->setIconName( QStringLiteral( "printer" ) );
                connect( mFillPrintRibbonBtn, &QAbstractButton::clicked, mFillView->printButton, &QAbstractButton::click );
                mDataFillOutputGroup->addWidget( mFillPrintRibbonBtn );

                auto* fillZpl = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillOutputGroup );
                fillZpl->setText( tr( "Send to Zebra" ) );
                fillZpl->setIconName( QStringLiteral( "send" ) );
                connect( fillZpl, &QAbstractButton::clicked, mFillView->zplButton, &QAbstractButton::click );
                mDataFillOutputGroup->addWidget( fillZpl );

                mFillSystemDialogRibbonBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, mDataFillOutputGroup );
                mFillSystemDialogRibbonBtn->setText( tr( "System dialog..." ) );
                mFillSystemDialogRibbonBtn->setIconName( QStringLiteral( "settings" ) );
                connect( mFillSystemDialogRibbonBtn, &QAbstractButton::clicked, mFillView->systemDialogButton, &QAbstractButton::click );
                mDataFillOutputGroup->addWidget( mFillSystemDialogRibbonBtn );

                connect( mFillView, &FillView::printAvailabilityChanged, this, [this]( bool canPrint )
                {
                        if ( mFillPrintRibbonBtn )        mFillPrintRibbonBtn->setEnabled( canPrint );
                        if ( mFillSystemDialogRibbonBtn ) mFillSystemDialogRibbonBtn->setEnabled( canPrint );
                } );

                updateDataTabGroups();
        }


        void MainWindow::populatePrintTab()
        {
                using Btn = ribbon::RibbonButton;
                ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::Print );

                // Output
                auto* outGrp = tab->addGroup( tr( "Output" ), QStringLiteral( "printer" ) );
                mPrintRibbonBtn = new Btn( Btn::Size::Large, Btn::Variant::Primary, outGrp );
                mPrintRibbonBtn->setText( tr( "Print" ) );
                mPrintRibbonBtn->setIconName( QStringLiteral( "printer" ) );
                connect( mPrintRibbonBtn, &QAbstractButton::clicked,
                         mPrintView->printButton, &QAbstractButton::click );
                outGrp->addWidget( mPrintRibbonBtn );

                mPrintSystemDialogRibbonBtn = new Btn( Btn::Size::Large, Btn::Variant::Standard, outGrp );
                mPrintSystemDialogRibbonBtn->setText( tr( "System dialog..." ) );
                mPrintSystemDialogRibbonBtn->setIconName( QStringLiteral( "settings" ) );
                connect( mPrintSystemDialogRibbonBtn, &QAbstractButton::clicked,
                         mPrintView->systemDialogButton, &QAbstractButton::click );
                outGrp->addWidget( mPrintSystemDialogRibbonBtn );

                connect( mPrintView, &PrintView::printAvailabilityChanged, this, [this]( bool canPrint )
                {
                        if ( mPrintRibbonBtn )             mPrintRibbonBtn->setEnabled( canPrint );
                        if ( mPrintSystemDialogRibbonBtn ) mPrintSystemDialogRibbonBtn->setEnabled( canPrint );
                } );

                // Printer
                auto* prnGrp = tab->addGroup( tr( "Printer" ), QStringLiteral( "printer" ) );
                auto* pCol = new QVBoxLayout();
                pCol->setContentsMargins( 2, 4, 2, 0 );
                pCol->setSpacing( 4 );

                mPrintTabDestCombo = new QComboBox( prnGrp );
                auto* destField = new ribbon::RibbonField( tr( "Destination" ), mPrintTabDestCombo, 240,
                                                           ribbon::RibbonField::LabelPosition::Above, 0, prnGrp );
                pCol->addWidget( destField );

                mPrintTabStatusLabel = new QLabel( tr( "Ready" ), prnGrp );
                mPrintTabStatusLabel->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                pCol->addWidget( mPrintTabStatusLabel );
                prnGrp->addLayout( pCol );

                // Keep Print ribbon destination combo synced with PrintView::destinationCombo
                connect( mPrintTabDestCombo, QOverload<int>::of( &QComboBox::activated ),
                         this, [this]( int idx )
                         {
                                 if ( !mSyncingRibbon && mPrintView && idx >= 0 && idx < mPrintView->destinationCombo->count() )
                                 {
                                         mPrintView->destinationCombo->setCurrentIndex( idx );
                                         updatePrintTabInfo();
                                 }
                         } );
                connect( mPrintView->destinationCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ),
                         this, [this]() { updatePrintTabInfo(); } );
                connect( PrinterMonitor::instance(), &PrinterMonitor::availablePrintersChanged,
                         this, [this]() { updatePrintTabInfo(); } );

                updatePrintTabInfo();
        }


        void MainWindow::populateDeployTab()
        {
                using Btn = ribbon::RibbonButton;
                ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::Deploy );

                // Kiosk
                auto* kioskGrp = tab->addGroup( tr( "Kiosk" ), QStringLiteral( "package" ) );
                auto* buildBtn = new Btn( Btn::Size::Large, Btn::Variant::Primary, kioskGrp );
                buildBtn->setText( tr( "Build kiosk..." ) );
                buildBtn->setIconName( QStringLiteral( "package" ) );
                connect( buildBtn, &QAbstractButton::clicked,
                         mDeployView->buildButton(), &QAbstractButton::click );
                kioskGrp->addWidget( buildBtn );

                // About (three lines of read-only help text, SPEC 6.1 & s14)
                auto* aboutGrp = tab->addGroup( tr( "About" ), QStringLiteral( "info" ) );
                auto* col = new QVBoxLayout();
                col->setContentsMargins( 4, 4, 4, 0 );
                col->setSpacing( 4 );
                auto* l1 = new QLabel( tr( "Single-file kiosk executable" ), aboutGrp );
                l1->setProperty( "role", QStringLiteral( "panelTitle" ) );
                auto* l2 = new QLabel( tr( "Embeds this label. Selected fields are locked." ), aboutGrp );
                l2->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                auto* l3 = new QLabel( tr( "Runs on Windows with no install." ), aboutGrp );
                l3->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                col->addWidget( l1 );
                col->addWidget( l2 );
                col->addWidget( l3 );
                aboutGrp->addLayout( col );
        }


        void MainWindow::populateViewTab()
        {
                using Btn = ribbon::RibbonButton;
                ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::View );

                // Show (checkable rows synced to existing QActions)
                auto* showGrp = tab->addGroup( tr( "Show" ), QStringLiteral( "grid-3x3" ) );
                auto* showCol = new QVBoxLayout();
                showCol->setContentsMargins( 2, 2, 4, 0 );
                showCol->setSpacing( 3 );

                auto bindCheck = [&]( QAction* act ) -> QCheckBox*
                {
                        auto* cb = new QCheckBox( ribbon::stripMnemonic( act->text() ), showGrp );
                        cb->setChecked( act->isChecked() );
                        cb->setEnabled( act->isEnabled() );
                        cb->setToolTip( ribbon::formatActionTooltip( act ) );
                        connect( cb, &QCheckBox::toggled, act, &QAction::setChecked );
                        connect( act, &QAction::changed, cb, [cb, act]()
                        {
                                const QSignalBlocker b( cb );
                                cb->setChecked( act->isChecked() );
                                cb->setEnabled( act->isEnabled() );
                        } );
                        showGrp->registerAction( act );
                        showCol->addWidget( cb );
                        return cb;
                };

                mViewGridCheck         = bindCheck( viewGridAction );
                mViewMarkupCheck       = bindCheck( viewMarkupAction );
                mViewQatCheck          = bindCheck( viewFileToolBarAction );
                mViewContentPanelCheck = bindCheck( viewContentPanelAction );
                showGrp->addLayout( showCol );

                // Snap (three checkable small buttons + bypass caption, docs/ribbon-ui/SNAP.md)
                auto* snapGrp = tab->addGroup( tr( "Snap" ), QStringLiteral( "magnet" ) );
                auto* snapCol = new QVBoxLayout();
                snapCol->setContentsMargins( 2, 4, 2, 0 );
                snapCol->setSpacing( 6 );

                auto* snapRow = new QHBoxLayout();
                snapRow->setContentsMargins( 0, 0, 0, 0 );
                snapRow->setSpacing( 4 );
                snapRow->addWidget( new Btn( viewSnapObjectsAction,  QStringLiteral( "magnet" ),        Btn::Size::Small, Btn::Variant::Standard, snapGrp ) );
                snapRow->addWidget( new Btn( viewSnapCenterAction,   QStringLiteral( "crosshair" ),     Btn::Size::Small, Btn::Variant::Standard, snapGrp ) );
                snapRow->addWidget( new Btn( viewSnapSafeAreaAction, QStringLiteral( "square-dashed" ), Btn::Size::Small, Btn::Variant::Standard, snapGrp ) );
                snapCol->addLayout( snapRow );

                auto* snapCap = new QLabel( tr( "Hold Alt to bypass" ), snapGrp );
                snapCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                snapCol->addWidget( snapCap );
                snapCol->addStretch( 1 );
                snapGrp->addLayout( snapCol );

                // Zoom (four large buttons)
                auto* zoomGrp = tab->addGroup( tr( "Zoom" ), QStringLiteral( "zoom-in" ) );
                zoomGrp->addWidget( new Btn( viewZoomInAction,    QStringLiteral( "zoom-in" ),   Btn::Size::Large, Btn::Variant::Standard, zoomGrp ) );
                zoomGrp->addWidget( new Btn( viewZoomOutAction,   QStringLiteral( "zoom-out" ),  Btn::Size::Large, Btn::Variant::Standard, zoomGrp ) );
                zoomGrp->addWidget( new Btn( viewZoom1To1Action,  QStringLiteral( "scan-line" ), Btn::Size::Large, Btn::Variant::Standard, zoomGrp ) );
                zoomGrp->addWidget( new Btn( viewZoomToFitAction, QStringLiteral( "scan" ),      Btn::Size::Large, Btn::Variant::Standard, zoomGrp ) );

                // Appearance (Light / Dark / System segmented)
                auto* appGrp = tab->addGroup( tr( "Appearance" ), QStringLiteral( "settings" ) );
                auto* appCol = new QVBoxLayout();
                appCol->setContentsMargins( 2, 4, 2, 0 );
                appCol->setSpacing( 5 );
                auto* csCap = new QLabel( tr( "Color scheme" ), appGrp );
                csCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                appCol->addWidget( csCap );

                mViewColorSchemeGroup = new ribbon::RibbonToggleGroup( appGrp );
                mViewColorSchemeGroup->addOption( model::Settings::LIGHT_COLOR_SCHEME,  tr( "Light" ) );
                mViewColorSchemeGroup->addOption( model::Settings::DARK_COLOR_SCHEME,   tr( "Dark" ) );
                mViewColorSchemeGroup->addOption( model::Settings::SYSTEM_COLOR_SCHEME, tr( "System" ) );
                mViewColorSchemeGroup->setOptionEnabled( model::Settings::SYSTEM_COLOR_SCHEME,
                                                         AppearanceModel::supportsSystemColorScheme() );
                mViewColorSchemeGroup->setCurrentId( model::Settings::colorScheme() );
                mViewColorSchemeGroup->setFixedWidth( 210 );
                connect( mViewColorSchemeGroup, &ribbon::RibbonToggleGroup::currentIdChanged,
                         this, []( int id )
                         {
                                 model::Settings::setColorScheme( static_cast<model::Settings::ColorScheme>( id ) );
                         } );
                appCol->addWidget( mViewColorSchemeGroup );

                auto* sameCap = new QLabel( tr( "Same setting as Preferences" ), appGrp );
                sameCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                appCol->addWidget( sameCap );
                appGrp->addLayout( appCol );

                // Theme (colour scheme picker, same setting as Preferences; THEMES.md)
                auto* themeGrp = tab->addGroup( tr( "Theme" ), QStringLiteral( "palette" ) );
                auto* themeCol = new QVBoxLayout();
                themeCol->setContentsMargins( 2, 4, 2, 0 );
                themeCol->setSpacing( 5 );
                auto* thCap = new QLabel( tr( "Theme" ), themeGrp );
                thCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                themeCol->addWidget( thCap );
                auto* themeCombo = new QComboBox( themeGrp );
                themeCombo->setObjectName( QStringLiteral( "viewThemeCombo" ) );
                themeCombo->setAccessibleName( tr( "Theme" ) );
                themeCombo->setProperty( "role", QStringLiteral( "ribbonField" ) );
                themeCombo->setFixedWidth( 150 );
                auto refreshTheme = [themeCombo]()
                {
                        theme::ThemeSchemes::fillCombo( themeCombo, model::Settings::themeScheme(),
                                                        theme::Theme::instance().mode() );
                };
                refreshTheme();
                connect( themeCombo, &QComboBox::currentIndexChanged, this, [themeCombo]( int index )
                {
                        const QString id = themeCombo->itemData( index ).toString();
                        if ( !id.isEmpty() && id != model::Settings::themeScheme() )
                        {
                                model::Settings::setThemeScheme( id );
                        }
                } );
                connect( &theme::Theme::instance(), &theme::Theme::changed, themeCombo, refreshTheme );
                themeCol->addWidget( themeCombo );
                themeGrp->addLayout( themeCol );

                // Icons (Flat / Vector segmented)
                auto* iconGrp = tab->addGroup( tr( "Icons" ), QStringLiteral( "image" ) );
                auto* iconCol = new QVBoxLayout();
                iconCol->setContentsMargins( 2, 4, 2, 0 );
                iconCol->setSpacing( 5 );
                auto* isCap = new QLabel( tr( "Icon style" ), iconGrp );
                isCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                iconCol->addWidget( isCap );

                mViewIconStyleGroup = new ribbon::RibbonToggleGroup( iconGrp );
                mViewIconStyleGroup->addOption( model::Settings::FLAT_ICON_STYLE,   tr( "Flat" ) );
                mViewIconStyleGroup->addOption( model::Settings::VECTOR_ICON_STYLE, tr( "Vector" ) );
                mViewIconStyleGroup->setCurrentId( model::Settings::iconStyle() );
                mViewIconStyleGroup->setFixedWidth( 160 );
                connect( mViewIconStyleGroup, &ribbon::RibbonToggleGroup::currentIdChanged,
                         this, []( int id )
                         {
                                 model::Settings::setIconStyle( static_cast<model::Settings::IconStyle>( id ) );
                         } );
                iconCol->addWidget( mViewIconStyleGroup );
                iconGrp->addLayout( iconCol );

                // Ribbon (Collapse ribbon)
                auto* ribGrp = tab->addGroup( tr( "Ribbon" ), QStringLiteral( "chevron-down" ) );
                ribGrp->addWidget( new Btn( viewCollapseRibbonAction, QStringLiteral( "chevron-down" ), Btn::Size::Large, Btn::Variant::Standard, ribGrp ) );
        }


        void MainWindow::populateHelpTab()
        {
                using Btn = ribbon::RibbonButton;
                ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::Help );

                // Help (F1)
                auto* helpGrp = tab->addGroup( tr( "Help (F1)" ), QStringLiteral( "book-open" ) );
                helpGrp->addWidget( new Btn( helpContentsAction,  QStringLiteral( "book-open" ), Btn::Size::Large, Btn::Variant::Standard, helpGrp ) );
                helpGrp->addWidget( new Btn( helpReportBugAction, QStringLiteral( "bug" ),       Btn::Size::Large, Btn::Variant::Standard, helpGrp ) );

                // gLabels Neo
                auto* appGrp = tab->addGroup( model::Branding::DISPLAY_NAME, QStringLiteral( "info" ) );
                appGrp->addWidget( new Btn( helpCheckForUpdatesAction, QStringLiteral( "refresh-cw" ), Btn::Size::Large, Btn::Variant::Standard, appGrp ) );
                appGrp->addWidget( new Btn( helpAboutAction,           QStringLiteral( "info" ),       Btn::Size::Large, Btn::Variant::Standard, appGrp ) );
        }


        void MainWindow::populateContextualTabs()
        {
                using Btn = ribbon::RibbonButton;
                using Cid = ObjectEditor::ControlId;

                mObjectEditor->setTransplanted( true );

                auto ctrl = [this]( Cid id ) -> QWidget*
                {
                        return mObjectEditor->control( id );
                };

                auto makeFieldLabel = []( const QString& text, QWidget* parent ) -> QLabel*
                {
                        auto* lbl = new QLabel( text, parent );
                        lbl->setProperty( "role", QStringLiteral( "fieldLabel" ) );
                        return lbl;
                };

                // Compact Neo ribbon color style ([swatch] Name v) on all ColorButtons (s04-s08)
                ribbon::installRibbonColorStyle( qobject_cast<ColorButton*>( ctrl( Cid::TextColorButton ) ) );
                ribbon::installRibbonColorStyle( qobject_cast<ColorButton*>( ctrl( Cid::BarcodeColorButton ) ) );
                ribbon::installRibbonColorStyle( qobject_cast<ColorButton*>( ctrl( Cid::LineColorButton ) ), tr( "No line" ) );
                ribbon::installRibbonColorStyle( qobject_cast<ColorButton*>( ctrl( Cid::FillColorButton ) ), tr( "No fill" ) );
                ribbon::installRibbonColorStyle( qobject_cast<ColorButton*>( ctrl( Cid::ShadowColorButton ) ) );

                // -----------------------------------------------------------------
                // Single shared ShadowPopover (SPEC 6.2 & mockup d10)
                // Used by Text, Image, Shape, Line — NEVER Barcode!
                // -----------------------------------------------------------------
                ribbon::ShadowPopover::Controls shadowCtrls;
                shadowCtrls.enableGroup = qobject_cast<QGroupBox*>( ctrl( Cid::ShadowEnableCheck ) );
                shadowCtrls.xSpin       = ctrl( Cid::ShadowXSpin );
                shadowCtrls.ySpin       = ctrl( Cid::ShadowYSpin );
                shadowCtrls.colorButton = ctrl( Cid::ShadowColorButton );
                shadowCtrls.opacitySpin = ctrl( Cid::ShadowOpacitySpin );
                mShadowPopover = new ribbon::ShadowPopover( shadowCtrls, this );

                // -----------------------------------------------------------------
                // Shared transplanted Position (X, Y), Rect Size (W, H, Lock aspect,
                // Image Original size + Reset), and Line (Width, Color) containers
                // -----------------------------------------------------------------
                mSharedPosWidget = new QWidget( this );
                {
                        auto* grid = new QGridLayout( mSharedPosWidget );
                        grid->setContentsMargins( 0, 4, 0, 0 );
                        grid->setHorizontalSpacing( 6 );
                        grid->setVerticalSpacing( 6 );

                        QWidget* posX = ctrl( Cid::PosXSpin );
                        QWidget* posY = ctrl( Cid::PosYSpin );
                        posX->setParent( mSharedPosWidget );
                        posY->setParent( mSharedPosWidget );
                        posX->setFixedWidth( 92 );
                        posY->setFixedWidth( 92 );
                        posX->show();
                        posY->show();

                        grid->addWidget( makeFieldLabel( tr( "X" ), mSharedPosWidget ), 0, 0 );
                        grid->addWidget( posX, 0, 1 );
                        grid->addWidget( makeFieldLabel( tr( "Y" ), mSharedPosWidget ), 1, 0 );
                        grid->addWidget( posY, 1, 1 );
                }

                mSharedRectSizeWidget = new QWidget( this );
                {
                        auto* row = new QHBoxLayout( mSharedRectSizeWidget );
                        row->setContentsMargins( 8, 4, 0, 0 );
                        row->setSpacing( 8 );

                        auto* grid = new QGridLayout();
                        grid->setContentsMargins( 0, 0, 0, 0 );
                        grid->setHorizontalSpacing( 6 );
                        grid->setVerticalSpacing( 6 );

                        QWidget* sizeW = ctrl( Cid::SizeWSpin );
                        QWidget* sizeH = ctrl( Cid::SizeHSpin );
                        sizeW->setParent( mSharedRectSizeWidget );
                        sizeH->setParent( mSharedRectSizeWidget );
                        sizeW->setFixedWidth( 92 );
                        sizeH->setFixedWidth( 92 );
                        sizeW->show();
                        sizeH->show();

                        grid->addWidget( makeFieldLabel( tr( "W" ), mSharedRectSizeWidget ), 0, 0 );
                        grid->addWidget( sizeW, 0, 1 );
                        grid->addWidget( makeFieldLabel( tr( "H" ), mSharedRectSizeWidget ), 1, 0 );
                        grid->addWidget( sizeH, 1, 1 );
                        row->addLayout( grid );

                        // Keep ObjectEditor's sizeAspectCheck alive (hidden) and expose it as a compact
                        // lock / lock-open icon toggle matching mockups s04..s07.
                        auto* aspectCheck = qobject_cast<QCheckBox*>( ctrl( Cid::SizeAspectCheck ) );
                        if ( aspectCheck )
                        {
                                aspectCheck->setParent( mSharedRectSizeWidget );
                                aspectCheck->hide();
                        }

                        mLockAspectButton = new QToolButton( mSharedRectSizeWidget );
                        mLockAspectButton->setCheckable( true );
                        mLockAspectButton->setFixedSize( 28, 28 );
                        mLockAspectButton->setIconSize( QSize( 16, 16 ) );
                        mLockAspectButton->setCursor( Qt::PointingHandCursor );
                        mLockAspectButton->setProperty( "role", QStringLiteral( "ribbonToggle" ) );
                        mLockAspectButton->setToolTip( tr( "Lock aspect ratio" ) );
                        mLockAspectButton->setAccessibleName( tr( "Lock aspect ratio" ) );

                        auto refreshLockIcon = [this]()
                        {
                                if ( !mLockAspectButton )
                                {
                                        return;
                                }
                                const bool locked = mLockAspectButton->isChecked();
                                mLockAspectButton->setIcon( theme::Theme::instance().icon(
                                        locked ? QStringLiteral( "lock" ) : QStringLiteral( "lock-open" ),
                                        locked ? theme::IconRole::Accent : theme::IconRole::Normal ) );
                        };

                        if ( aspectCheck )
                        {
                                mLockAspectButton->setChecked( aspectCheck->isChecked() );
                                connect( mLockAspectButton, &QToolButton::toggled, this, [aspectCheck, refreshLockIcon]( bool checked )
                                {
                                        if ( aspectCheck->isChecked() != checked )
                                        {
                                                aspectCheck->setChecked( checked );
                                        }
                                        refreshLockIcon();
                                } );
                                connect( aspectCheck, &QCheckBox::toggled, this, [this, refreshLockIcon]( bool checked )
                                {
                                        if ( mLockAspectButton && mLockAspectButton->isChecked() != checked )
                                        {
                                                const QSignalBlocker b( mLockAspectButton );
                                                mLockAspectButton->setChecked( checked );
                                        }
                                        refreshLockIcon();
                                } );
                        }
                        connect( &theme::Theme::instance(), &theme::Theme::changed,
                                 mLockAspectButton, [refreshLockIcon]() { refreshLockIcon(); } );
                        refreshLockIcon();

                        row->addWidget( mLockAspectButton, 0, Qt::AlignVCenter );

                        // Image-only sub-widget (Original size + Reset button)
                        mImageResetSubWidget = new QWidget( mSharedRectSizeWidget );
                        auto* imgCol = new QVBoxLayout( mImageResetSubWidget );
                        imgCol->setContentsMargins( 6, 0, 0, 0 );
                        imgCol->setSpacing( 3 );
                        QWidget* origLbl  = ctrl( Cid::SizeOriginalSizeLabel );
                        QWidget* resetBtn = ctrl( Cid::SizeResetImageButton );
                        origLbl->setParent( mImageResetSubWidget );
                        origLbl->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                        origLbl->show();
                        resetBtn->setParent( mImageResetSubWidget );
                        resetBtn->show();
                        imgCol->addWidget( origLbl );
                        imgCol->addWidget( resetBtn, 0, Qt::AlignLeft );
                        mImageResetSubWidget->hide();
                        row->addWidget( mImageResetSubWidget, 0, Qt::AlignVCenter );
                }

                mSharedLineWidget = new QWidget( this );
                {
                        auto* grid = new QGridLayout( mSharedLineWidget );
                        grid->setContentsMargins( 0, 4, 0, 0 );
                        grid->setHorizontalSpacing( 6 );
                        grid->setVerticalSpacing( 6 );

                        QWidget* lw = ctrl( Cid::LineWidthSpin );
                        QWidget* lc = ctrl( Cid::LineColorButton );
                        lw->setParent( mSharedLineWidget );
                        lc->setParent( mSharedLineWidget );
                        lw->setFixedWidth( 84 );
                        lc->setFixedWidth( 104 );
                        lw->show();
                        lc->show();

                        grid->addWidget( makeFieldLabel( tr( "Width" ), mSharedLineWidget ), 0, 0 );
                        grid->addWidget( lw, 0, 1 );
                        grid->addWidget( makeFieldLabel( tr( "Color" ), mSharedLineWidget ), 1, 0 );
                        grid->addWidget( lc, 1, 1 );
                }

                // Helpers for repeated contextual groups
                auto addPosSizeHost = [this]( ribbon::RibbonTab* tab, ribbon::TabId id, const QString& title )
                {
                        auto* grp = tab->addGroup( title, QStringLiteral( "ruler" ), 2 );
                        auto* host = new QHBoxLayout();
                        host->setContentsMargins( 0, 0, 0, 0 );
                        host->setSpacing( 0 );
                        grp->addLayout( host );
                        mPosSizeHostLayouts.insert( id, host );
                };

                auto addLineHost = [this]( ribbon::RibbonTab* tab, ribbon::TabId id )
                {
                        auto* grp = tab->addGroup( tr( "Line" ), QStringLiteral( "slash" ), 0 );
                        auto* host = new QHBoxLayout();
                        host->setContentsMargins( 0, 0, 0, 0 );
                        host->setSpacing( 0 );
                        grp->addLayout( host );
                        mLineHostLayouts.insert( id, host );
                };

                auto addEffectsGroup = [this]( ribbon::RibbonTab* tab )
                {
                        auto* grp = tab->addGroup( tr( "Effects" ), QStringLiteral( "shadow" ), 2 );
                        auto* shadowBtn = new ribbon::RibbonDropButton(
                                tr( "Shadow" ), QStringLiteral( "shadow" ), Btn::Size::Large, grp );
                        shadowBtn->setToolTip( tr( "Enable and configure drop shadow" ) );
                        shadowBtn->setPopupWidget( mShadowPopover );
                        connect( shadowBtn, &ribbon::RibbonDropButton::aboutToShowPopup,
                                 mShadowPopover, &ribbon::ShadowPopover::syncFromObjectEditor );
                        grp->addWidget( shadowBtn );
                };

                auto addArrangeGroup = [this]( ribbon::RibbonTab* tab )
                {
                        auto* arrangeGrp = tab->addGroup( tr( "Arrange" ), QStringLiteral( "bring-to-front" ), 3 );
                        auto* col = new QVBoxLayout();
                        col->setContentsMargins( 0, 2, 0, 0 );
                        col->setSpacing( 1 );
                        col->addWidget( new Btn( objectsOrderRaiseAction, QStringLiteral( "bring-to-front" ), Btn::Size::Small, Btn::Variant::Standard, arrangeGrp ) );
                        col->addWidget( new Btn( objectsOrderLowerAction, QStringLiteral( "send-to-back" ),   Btn::Size::Small, Btn::Variant::Standard, arrangeGrp ) );
                        col->addWidget( new Btn( editDeleteAction,        QStringLiteral( "trash-2" ),        Btn::Size::Small, Btn::Variant::Standard, arrangeGrp ) );
                        arrangeGrp->addLayout( col );

                        auto* xformGrid = new QGridLayout();
                        xformGrid->setContentsMargins( 4, 8, 0, 0 );
                        xformGrid->setSpacing( 2 );
                        xformGrid->addWidget( new Btn( objectsXformRotateLeftAction,  QStringLiteral( "rotate-ccw" ),        Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 0, 0 );
                        xformGrid->addWidget( new Btn( objectsXformRotateRightAction, QStringLiteral( "rotate-cw" ),         Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 0, 1 );
                        xformGrid->addWidget( new Btn( objectsXformFlipHorizAction,   QStringLiteral( "flip-horizontal-2" ), Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 1, 0 );
                        xformGrid->addWidget( new Btn( objectsXformFlipVertAction,    QStringLiteral( "flip-vertical-2" ),   Btn::Size::IconOnly, Btn::Variant::Standard, arrangeGrp ), 1, 1 );
                        arrangeGrp->addLayout( xformGrid );
                };

                // =================================================================
                // 1. Contextual tab: Text (SPEC 6.2 & s04)
                // =================================================================
                {
                        ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::ContextText );

                        // Font
                        auto* fontGrp = tab->addGroup( tr( "Font" ), QStringLiteral( "type" ), 0 );
                        auto* fontCol = new QVBoxLayout();
                        fontCol->setContentsMargins( 0, 4, 0, 0 );
                        fontCol->setSpacing( 6 );

                        auto* row1 = new QHBoxLayout();
                        row1->setSpacing( 4 );
                        QWidget* fontCombo = ctrl( Cid::TextFontFamilyCombo );
                        QWidget* fontSize  = ctrl( Cid::TextFontSizeSpin );
                        fontCombo->setParent( fontGrp );
                        fontSize->setParent( fontGrp );
                        fontCombo->setFixedWidth( 150 );
                        fontSize->setFixedWidth( 68 );
                        fontCombo->show();
                        fontSize->show();
                        row1->addWidget( fontCombo );
                        row1->addWidget( fontSize );
                        fontCol->addLayout( row1 );

                        auto* row2 = new QHBoxLayout();
                        row2->setSpacing( 3 );
                        QWidget* boldBtn   = ctrl( Cid::TextFontBoldToggle );
                        QWidget* italicBtn = ctrl( Cid::TextFontItalicToggle );
                        QWidget* underBtn  = ctrl( Cid::TextFontUnderlineToggle );
                        QWidget* colorBtn  = ctrl( Cid::TextColorButton );
                        for ( QWidget* w : { boldBtn, italicBtn, underBtn, colorBtn } )
                        {
                                w->setParent( fontGrp );
                                w->show();
                        }
                        colorBtn->setFixedWidth( 104 );
                        row2->addWidget( boldBtn );
                        row2->addWidget( italicBtn );
                        row2->addWidget( underBtn );
                        row2->addSpacing( 6 );
                        row2->addWidget( colorBtn );
                        row2->addStretch( 1 );
                        fontCol->addLayout( row2 );
                        fontGrp->addLayout( fontCol );

                        // Alignment
                        auto* alignGrp = tab->addGroup( tr( "Alignment" ), QStringLiteral( "align-left" ), 1 );
                        auto* alignCol = new QVBoxLayout();
                        alignCol->setContentsMargins( 0, 4, 0, 0 );
                        alignCol->setSpacing( 6 );

                        auto* hRow = new QHBoxLayout();
                        hRow->setSpacing( 2 );
                        for ( Cid cid : { Cid::TextHAlignLeftToggle, Cid::TextHAlignCenterToggle, Cid::TextHAlignRightToggle } )
                        {
                                QWidget* w = ctrl( cid );
                                w->setParent( alignGrp );
                                w->show();
                                hRow->addWidget( w );
                        }
                        alignCol->addLayout( hRow );

                        auto* vRow = new QHBoxLayout();
                        vRow->setSpacing( 2 );
                        for ( Cid cid : { Cid::TextVAlignTopToggle, Cid::TextVAlignMiddleToggle, Cid::TextVAlignBottomToggle } )
                        {
                                QWidget* w = ctrl( cid );
                                w->setParent( alignGrp );
                                w->show();
                                vRow->addWidget( w );
                        }
                        alignCol->addLayout( vRow );
                        alignGrp->addLayout( alignCol );

                        // Paragraph (2-row Line spacing / Wrap mode + auto-shrink check, s04 mockup)
                        auto* paraGrp = tab->addGroup( tr( "Paragraph" ), QStringLiteral( "align-left" ), 2 );
                        auto* paraCol = new QVBoxLayout();
                        paraCol->setContentsMargins( 0, 2, 0, 0 );
                        paraCol->setSpacing( 3 );

                        auto* pGrid = new QGridLayout();
                        pGrid->setContentsMargins( 0, 0, 0, 0 );
                        pGrid->setHorizontalSpacing( 6 );
                        pGrid->setVerticalSpacing( 3 );

                        QWidget* lsSpin    = ctrl( Cid::TextLineSpacingSpin );
                        QWidget* wrapCombo = ctrl( Cid::TextWrapModeCombo );
                        lsSpin->setParent( paraGrp );
                        wrapCombo->setParent( paraGrp );
                        lsSpin->setFixedWidth( 82 );
                        wrapCombo->setFixedWidth( 104 );
                        lsSpin->show();
                        wrapCombo->show();

                        pGrid->addWidget( makeFieldLabel( tr( "Line spacing" ), paraGrp ), 0, 0 );
                        pGrid->addWidget( lsSpin, 0, 1 );
                        pGrid->addWidget( makeFieldLabel( tr( "Wrap mode" ), paraGrp ), 1, 0 );
                        pGrid->addWidget( wrapCombo, 1, 1 );
                        paraCol->addLayout( pGrid );

                        QWidget* shrinkCheck = ctrl( Cid::TextAutoShrinkCheck );
                        shrinkCheck->setParent( paraGrp );
                        shrinkCheck->show();
                        paraCol->addWidget( shrinkCheck );
                        paraGrp->addLayout( paraCol );

                        // Content (Insert field dropdown sharing textInsertFieldButton's QMenu)
                        auto* contentGrp = tab->addGroup( tr( "Content" ), QStringLiteral( "braces" ), 1 );
                        mTextInsertFieldDropBtn = new ribbon::RibbonDropButton(
                                tr( "Insert field" ), QStringLiteral( "braces" ), Btn::Size::Large, contentGrp );
                        if ( auto* fb = qobject_cast<QPushButton*>( ctrl( Cid::TextInsertFieldButton ) ) )
                        {
                                mTextInsertFieldDropBtn->setMenu( fb->menu() );
                                mTextInsertFieldDropBtn->setEnabled( fb->isEnabled() );
                        }
                        contentGrp->addWidget( mTextInsertFieldDropBtn );

                        addPosSizeHost( tab, ribbon::TabId::ContextText, tr( "Position and size" ) );
                        addEffectsGroup( tab );
                        addArrangeGroup( tab );
                }

                // =================================================================
                // 2. Contextual tab: Barcode (SPEC 6.2 & s05 — NO Effects/Shadow!)
                // =================================================================
                {
                        ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::ContextBarcode );

                        // Type (Symbology caption + left-aligned combo-like button + flags, s05 mockup)
                        auto* typeGrp = tab->addGroup( tr( "Type" ), QStringLiteral( "barcode" ), 0 );
                        auto* typeCol = new QVBoxLayout();
                        typeCol->setContentsMargins( 0, 2, 0, 0 );
                        typeCol->setSpacing( 4 );

                        auto* symCap = new QLabel( tr( "Symbology" ), typeGrp );
                        symCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                        typeCol->addWidget( symCap );

                        QWidget* styleBtn = ctrl( Cid::BarcodeStyleButton );
                        styleBtn->setParent( typeGrp );
                        styleBtn->setMinimumWidth( 172 );
                        styleBtn->setStyleSheet(
                                QStringLiteral( "QPushButton { text-align: left; padding-left: 8px; padding-right: 22px; font-weight: 400; }" ) );
                        styleBtn->show();
                        typeCol->addWidget( styleBtn );

                        auto* flagsRow = new QHBoxLayout();
                        flagsRow->setSpacing( 10 );
                        QWidget* showText = ctrl( Cid::BarcodeShowTextCheck );
                        QWidget* checksum = ctrl( Cid::BarcodeChecksumCheck );
                        showText->setParent( typeGrp );
                        checksum->setParent( typeGrp );
                        showText->show();
                        checksum->show();
                        flagsRow->addWidget( showText );
                        flagsRow->addWidget( checksum );
                        typeCol->addLayout( flagsRow );
                        typeGrp->addLayout( typeCol );

                        // Color ("Bar color" caption + compact swatch/name button, s05 mockup)
                        auto* colorGrp = tab->addGroup( tr( "Color" ), QStringLiteral( "palette" ), 1 );
                        auto* colorCol = new QVBoxLayout();
                        colorCol->setContentsMargins( 0, 2, 0, 0 );
                        colorCol->setSpacing( 4 );
                        auto* barColCap = new QLabel( tr( "Bar color" ), colorGrp );
                        barColCap->setProperty( "role", QStringLiteral( "ribbonCaption" ) );
                        colorCol->addWidget( barColCap );
                        QWidget* bcColor = ctrl( Cid::BarcodeColorButton );
                        bcColor->setParent( colorGrp );
                        bcColor->setFixedWidth( 104 );
                        bcColor->show();
                        colorCol->addWidget( bcColor );
                        colorGrp->addLayout( colorCol );

                        // Data
                        auto* dataGrp = tab->addGroup( tr( "Data" ), QStringLiteral( "braces" ), 1 );
                        mBarcodeInsertFieldDropBtn = new ribbon::RibbonDropButton(
                                tr( "Insert field" ), QStringLiteral( "braces" ), Btn::Size::Large, dataGrp );
                        if ( auto* fb = qobject_cast<QPushButton*>( ctrl( Cid::BarcodeInsertFieldButton ) ) )
                        {
                                mBarcodeInsertFieldDropBtn->setMenu( fb->menu() );
                                mBarcodeInsertFieldDropBtn->setEnabled( fb->isEnabled() );
                        }
                        dataGrp->addWidget( mBarcodeInsertFieldDropBtn );

                        addPosSizeHost( tab, ribbon::TabId::ContextBarcode, tr( "Position and size" ) );
                        // Intentionally NO Effects/Shadow group on Barcode (SPEC 6.2 & hard rule)
                        addArrangeGroup( tab );
                }

                // =================================================================
                // 3. Contextual tab: Image (SPEC 6.2 & s06)
                // =================================================================
                {
                        ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::ContextImage );

                        // Source
                        auto* srcGrp = tab->addGroup( tr( "Source" ), QStringLiteral( "image" ), 0 );
                        auto* srcCol = new QVBoxLayout();
                        srcCol->setContentsMargins( 0, 4, 0, 0 );
                        srcCol->setSpacing( 6 );

                        auto* fileRow = new QHBoxLayout();
                        fileRow->setSpacing( 4 );
                        QWidget* fnEdit   = ctrl( Cid::ImageFilenameLineEdit );
                        QWidget* browseBt = ctrl( Cid::ImageBrowseButton );
                        fnEdit->setParent( srcGrp );
                        browseBt->setParent( srcGrp );
                        fnEdit->setFixedWidth( 170 );
                        fnEdit->show();
                        browseBt->show();
                        fileRow->addWidget( fnEdit );
                        fileRow->addWidget( browseBt );
                        srcCol->addLayout( fileRow );

                        mImageUseFieldDropBtn = new ribbon::RibbonDropButton(
                                tr( "Use merge field" ), QStringLiteral( "braces" ), Btn::Size::Small, srcGrp );
                        if ( auto* fb = qobject_cast<QPushButton*>( ctrl( Cid::ImageFieldButton ) ) )
                        {
                                mImageUseFieldDropBtn->setMenu( fb->menu() );
                                mImageUseFieldDropBtn->setEnabled( fb->isEnabled() );
                        }
                        srcCol->addWidget( mImageUseFieldDropBtn, 0, Qt::AlignLeft );
                        srcGrp->addLayout( srcCol );

                        addPosSizeHost( tab, ribbon::TabId::ContextImage, tr( "Position and size" ) );
                        addEffectsGroup( tab );
                        addArrangeGroup( tab );
                }

                // =================================================================
                // 4. Contextual tab: Shape (SPEC 6.2 & s07)
                // =================================================================
                {
                        ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::ContextShape );

                        addLineHost( tab, ribbon::TabId::ContextShape );

                        // Fill
                        auto* fillGrp = tab->addGroup( tr( "Fill" ), QStringLiteral( "palette" ), 1 );
                        auto* fillCol = new QVBoxLayout();
                        fillCol->setContentsMargins( 0, 4, 0, 0 );
                        fillCol->setSpacing( 4 );
                        fillCol->addWidget( makeFieldLabel( tr( "Fill" ), fillGrp ) );
                        QWidget* fillBtn = ctrl( Cid::FillColorButton );
                        fillBtn->setParent( fillGrp );
                        fillBtn->setFixedWidth( 104 );
                        fillBtn->show();
                        fillCol->addWidget( fillBtn );
                        fillGrp->addLayout( fillCol );

                        addPosSizeHost( tab, ribbon::TabId::ContextShape, tr( "Position and size" ) );
                        addEffectsGroup( tab );
                        addArrangeGroup( tab );
                }

                // =================================================================
                // 5. Contextual tab: Line (SPEC 6.2 & s08)
                // =================================================================
                {
                        ribbon::RibbonTab* tab = mRibbonBar->tab( ribbon::TabId::ContextLine );

                        addLineHost( tab, ribbon::TabId::ContextLine );
                        addPosSizeHost( tab, ribbon::TabId::ContextLine, tr( "Position" ) );

                        // Size (Length + Angle)
                        auto* sizeGrp = tab->addGroup( tr( "Size" ), QStringLiteral( "ruler" ), 1 );
                        auto* grid = new QGridLayout();
                        grid->setContentsMargins( 0, 4, 0, 0 );
                        grid->setHorizontalSpacing( 6 );
                        grid->setVerticalSpacing( 6 );

                        QWidget* lenSpin = ctrl( Cid::SizeLineLengthSpin );
                        QWidget* angSpin = ctrl( Cid::SizeLineAngleSpin );
                        lenSpin->setParent( sizeGrp );
                        angSpin->setParent( sizeGrp );
                        lenSpin->setFixedWidth( 92 );
                        angSpin->setFixedWidth( 92 );
                        lenSpin->show();
                        angSpin->show();

                        grid->addWidget( makeFieldLabel( tr( "Length" ), sizeGrp ), 0, 0 );
                        grid->addWidget( lenSpin, 0, 1 );
                        grid->addWidget( makeFieldLabel( tr( "Angle" ), sizeGrp ), 1, 0 );
                        grid->addWidget( angSpin, 1, 1 );
                        sizeGrp->addLayout( grid );

                        addEffectsGroup( tab );
                        addArrangeGroup( tab );
                }

                // =================================================================
                // Transplant Content panel controls once (SPEC 3.4 & 6.5)
                // =================================================================
                if ( mContentPanel )
                {
                        ribbon::ContentPanel::TransplantedControls panelCtrls;
                        panelCtrls.textEdit                 = ctrl( Cid::TextEdit );
                        panelCtrls.textInsertFieldButton    = ctrl( Cid::TextInsertFieldButton );
                        panelCtrls.barcodeDataEdit          = ctrl( Cid::BarcodeDataEdit );
                        panelCtrls.barcodeInsertFieldButton = ctrl( Cid::BarcodeInsertFieldButton );
                        panelCtrls.imageFilenameSource      = qobject_cast<QLineEdit*>( ctrl( Cid::ImageFilenameLineEdit ) );
                        panelCtrls.imageBrowseSource        = qobject_cast<QPushButton*>( ctrl( Cid::ImageBrowseButton ) );
                        panelCtrls.imageFieldButton         = ctrl( Cid::ImageFieldButton );
                        mContentPanel->installControls( panelCtrls );

                        connect( mContentPanel, &ribbon::ContentPanel::fieldActivated,
                                 this, [this]( const QString& key )
                                 {
                                         switch ( currentSelectionKind() )
                                         {
                                         case ribbon::SelectionKind::Text:
                                                 mObjectEditor->insertTextField( key );
                                                 break;
                                         case ribbon::SelectionKind::Barcode:
                                                 mObjectEditor->insertBarcodeField( key );
                                                 break;
                                         case ribbon::SelectionKind::Image:
                                                 mObjectEditor->setImageField( key );
                                                 break;
                                         default:
                                                 break;
                                         }
                                 } );
                }

                // Initial dock of shared widgets so they belong to live ribbon layouts
                attachSharedContextualWidgets( ribbon::TabId::ContextShape );
                attachSharedContextualWidgets( ribbon::TabId::ContextText );
                syncFieldDropButtons();
        }


        void MainWindow::attachSharedContextualWidgets( ribbon::TabId ctxTab )
        {
                if ( auto* posHost = mPosSizeHostLayouts.value( ctxTab, nullptr ) )
                {
                        if ( mSharedPosWidget )
                        {
                                posHost->addWidget( mSharedPosWidget );
                                mSharedPosWidget->show();
                        }
                        if ( mSharedRectSizeWidget )
                        {
                                if ( ctxTab == ribbon::TabId::ContextLine )
                                {
                                        mSharedRectSizeWidget->hide();
                                }
                                else
                                {
                                        posHost->addWidget( mSharedRectSizeWidget );
                                        mSharedRectSizeWidget->show();
                                        if ( mImageResetSubWidget )
                                        {
                                                mImageResetSubWidget->setVisible( ctxTab == ribbon::TabId::ContextImage );
                                        }
                                }
                        }
                }

                if ( auto* lineHost = mLineHostLayouts.value( ctxTab, nullptr ) )
                {
                        if ( mSharedLineWidget )
                        {
                                lineHost->addWidget( mSharedLineWidget );
                                mSharedLineWidget->show();
                        }
                }

                if ( auto* tabWidget = mRibbonBar ? mRibbonBar->tab( ctxTab ) : nullptr )
                {
                        tabWidget->updateLayoutState();
                }
        }


        void MainWindow::syncFieldDropButtons()
        {
                if ( !mObjectEditor )
                {
                        return;
                }
                using Cid = ObjectEditor::ControlId;
                const QString disabledTip = tr( "No merge fields or variables" );
                const QString enabledTip  = tr( "Insert a merge field or variable" );

                auto syncOne = [&]( ribbon::RibbonDropButton* dropBtn, Cid srcId )
                {
                        if ( !dropBtn )
                        {
                                return;
                        }
                        const QWidget* fb = mObjectEditor->control( srcId );
                        const bool en = fb && fb->isEnabled();
                        dropBtn->setEnabled( en );
                        dropBtn->setToolTip( en ? enabledTip : disabledTip );
                };

                syncOne( mTextInsertFieldDropBtn,    Cid::TextInsertFieldButton );
                syncOne( mBarcodeInsertFieldDropBtn, Cid::BarcodeInsertFieldButton );
                syncOne( mImageUseFieldDropBtn,      Cid::ImageFieldButton );
        }


        ///
        /// Create Status Bar
        ///
        void MainWindow::createStatusBar()
        {
                // Keep QMainWindow::statusBar() alive (hidden) so UpdateUi::showMessage() works transparently.
                statusBar()->hide();
                mRibbonStatusBar = new ribbon::RibbonStatusBar( viewZoomOutAction, viewZoomInAction, this );
                connect( statusBar(), &QStatusBar::messageChanged,
                         mRibbonStatusBar, &ribbon::RibbonStatusBar::setTemporaryMessage );
                connect( mRibbonStatusBar, &ribbon::RibbonStatusBar::zoomStepsRequested,
                         this, &MainWindow::onStatusZoomStepsRequested );
        }


        ///
        /// Create Welcome Page
        ///
        QWidget* MainWindow::createWelcomePage()
        {
                mWelcomeView = new StartupView( this );
                return mWelcomeView;
        }


        ///
        /// Create Properties Page
        ///
        QWidget* MainWindow::createPropertiesPage()
        {
                mPropertiesView = new PropertiesView();
                return mPropertiesView;
        }


        ///
        /// Create Editor Page
        ///
        QWidget* MainWindow::createEditorPage()
        {
                QWidget* page = new QWidget;

                mLabelEditorScrollArea = new QScrollArea();
                mLabelEditorScrollArea->setMinimumSize( 640, 450 );
                mLabelEditorScrollArea->setWidgetResizable( true );
                mLabelEditorScrollArea->setFrameShape( QFrame::NoFrame );

                mLabelEditor = new LabelEditor( mLabelEditorScrollArea );
                mLabelEditor->installEventFilter( this );
                mLabelEditorScrollArea->setWidget( mLabelEditor );

                // ObjectEditor stays alive as a non-visual controller (SPEC 3.4); never shown directly.
                mObjectEditor = new ObjectEditor( this );
                mObjectEditor->hide();

                mEditorRulerFrame = new ribbon::EditorRulerFrame( mLabelEditorScrollArea, mLabelEditor, page );
                mContentPanel     = new ribbon::ContentPanel( page );

                auto* editorHLayout = new QHBoxLayout( page );
                editorHLayout->setContentsMargins( 0, 0, 0, 0 );
                editorHLayout->setSpacing( 0 );
                editorHLayout->addWidget( mEditorRulerFrame, 1 );
                editorHLayout->addWidget( mContentPanel, 0 );

                return page;
        }


        ///
        /// Create Merge Page
        ///
        QWidget* MainWindow::createMergePage()
        {
                mMergeView = new MergeView();
                return mMergeView;
        }


        ///
        /// Create Variables Page
        ///
        QWidget* MainWindow::createVariablesPage()
        {
                mVariablesView = new VariablesView();
                return mVariablesView;
        }


        ///
        /// Create Print Page
        ///
        QWidget* MainWindow::createPrintPage()
        {
                mPrintView = new PrintView();
                return mPrintView;
        }


        ///
        /// Create Fill Page
        ///
        QWidget* MainWindow::createFillPage()
        {
                mFillView = new FillView();
                mFillView->setRibbonHosted( true );
                return mFillView;
        }


        ///
        /// Create Deploy Page
        ///
        QWidget* MainWindow::createDeployPage()
        {
                mDeployView = new DeployView();
                return mDeployView;
        }


        ///
        /// Switch active page and sync ribbon tab
        ///
        void MainWindow::switchToPage( ribbon::PageId page, bool syncRibbon )
        {
                mCurrentPage = page;
                mPages->setCurrentIndex( pageIndex( page ) );

                if ( auto sub = ribbon::dataSubPageForPage( page ) )
                {
                        mLastDataSubPage = *sub;
                }

                if ( syncRibbon && !mRibbonBar->isWelcomeMode() )
                {
                        const ribbon::TabId targetTab = ribbon::tabForPage( page );
                        // Keep contextual tab active if staying on Editor
                        if ( !( page == ribbon::PageId::Editor && ribbon::isContextualTab( mRibbonBar->currentTab() ) ) )
                        {
                                mRibbonBar->setCurrentTab( targetTab, false );
                        }
                }

                mRibbonStatusBar->setEditorControlsVisible( page == ribbon::PageId::Editor );
                if ( page == ribbon::PageId::Editor && mLabelEditor )
                {
                        mLabelEditor->setFocus( Qt::OtherFocusReason );
                }
                updateDataTabGroups();
                updateContextualTabState( false );
                manageActions();
        }


        void MainWindow::onRibbonTabChanged( ribbon::TabId id )
        {
                if ( mRibbonBar->isWelcomeMode() )
                {
                        return;
                }

                if ( auto targetPage = ribbon::pageForTab( id, mLastDataSubPage ) )
                {
                        switchToPage( *targetPage, false );
                }
                else
                {
                        // View and Help stay on the current page (SPEC 3.2)
                        updateContextualTabState( false );
                        manageActions();
                }
        }


        void MainWindow::onFileButtonClicked()
        {
                mBackstage->refresh();
                mShellStack->setCurrentWidget( mBackstage );
        }


        void MainWindow::onBackstageCloseRequested()
        {
                mShellStack->setCurrentIndex( 0 );
        }


        void MainWindow::onStatusZoomStepsRequested( int steps )
        {
                if ( !mLabelEditor || mCurrentPage != ribbon::PageId::Editor )
                {
                        return;
                }
                if ( steps > 0 )
                {
                        for ( int i = 0; i < steps && !mLabelEditor->isZoomMax(); ++i )
                        {
                                mLabelEditor->zoomIn();
                        }
                }
                else
                {
                        for ( int i = 0; i < -steps && !mLabelEditor->isZoomMin(); ++i )
                        {
                                mLabelEditor->zoomOut();
                        }
                }
        }


        void MainWindow::updateContextualTabState( bool activateIfSingle )
        {
                const ribbon::SelectionKind kind = currentSelectionKind();

                if ( mContentPanel )
                {
                        mContentPanel->setSelectionKind( kind );
                }

                if ( mRibbonBar->isWelcomeMode() )
                {
                        mRibbonBar->setVisibleContextualTab( std::nullopt );
                        return;
                }

                if ( ribbon::shouldShowContextualTab( mCurrentPage, kind ) )
                {
                        const auto ctxTab = ribbon::contextualTabForSelection( kind );
                        if ( ctxTab.has_value() )
                        {
                                attachSharedContextualWidgets( *ctxTab );
                        }
                        mRibbonBar->setVisibleContextualTab( ctxTab );
                }
                else
                {
                        mRibbonBar->setVisibleContextualTab( std::nullopt );
                }

                if ( mShadowPopover && mShadowPopover->isVisible() )
                {
                        mShadowPopover->syncFromObjectEditor();
                }

                // If LabelEditor replaces selection via unselectAll() + selectObject() within
                // the same event turn while a contextual tab was active, preserve contextual focus.
                const ribbon::TabId curTab = mRibbonBar->currentTab();
                const bool effectiveActivate =
                        activateIfSingle || ( mRecentContextualDrop && ribbon::shouldShowContextualTab( mCurrentPage, kind ) );

                if ( kind == ribbon::SelectionKind::None && ribbon::isContextualTab( curTab ) )
                {
                        mRecentContextualDrop = true;
                        QTimer::singleShot( 0, this, [this]() { mRecentContextualDrop = false; } );
                }
                else
                {
                        mRecentContextualDrop = false;
                }

                const ribbon::TabId nextTab = ribbon::resolveTabOnSelectionChange(
                        curTab, mCurrentPage, kind, effectiveActivate );
                if ( nextTab != curTab )
                {
                        mRibbonBar->setCurrentTab( nextTab, false );
                }
        }


        void MainWindow::updateDataTabGroups()
        {
                const bool isMerge = ( mLastDataSubPage == ribbon::DataSubPage::Merge );
                const bool isVars  = ( mLastDataSubPage == ribbon::DataSubPage::Variables );
                const bool isFill  = ( mLastDataSubPage == ribbon::DataSubPage::Fill );

                if ( mDataMergeBtn )     mDataMergeBtn->setChecked( isMerge );
                if ( mDataVariablesBtn ) mDataVariablesBtn->setChecked( isVars );
                if ( mDataFillBtn )      mDataFillBtn->setChecked( isFill );

                if ( mDataMergeSourceGroup )  mDataMergeSourceGroup->setVisible( isMerge );
                if ( mDataMergeRecordsGroup ) mDataMergeRecordsGroup->setVisible( isMerge );
                if ( mDataVarsGroup )         mDataVarsGroup->setVisible( isVars );
                if ( mDataFillRowsGroup )     mDataFillRowsGroup->setVisible( isFill );
                if ( mDataFillLiveGroup )     mDataFillLiveGroup->setVisible( isFill );
                if ( mDataFillIoGroup )       mDataFillIoGroup->setVisible( isFill );
                if ( mDataFillOutputGroup )   mDataFillOutputGroup->setVisible( isFill );

                if ( auto* dataTab = mRibbonBar ? mRibbonBar->tab( ribbon::TabId::Data ) : nullptr )
                {
                        dataTab->updateLayoutState();
                }
        }


        void MainWindow::updateLabelTabInfo()
        {
                if ( !mModel || !mLabelTabName )
                {
                        return;
                }

                mSyncingRibbon = true;
                const auto tmplate = mModel->tmplate();
                const auto* frame  = tmplate.frame();
                const auto units   = model::Settings::units();

                mLabelTabName->setText( tmplate.name() );
                if ( frame )
                {
                        mLabelTabDetails->setText(
                                QStringLiteral( "%1 · %2 · %3" )
                                        .arg( tmplate.description(),
                                              frame->sizeDescription( units ),
                                              frame->layoutDescription() ) );
                }

                const bool hasAdjustable = dynamic_cast<const model::FrameContinuous*>( frame ) != nullptr;
                if ( mLabelTabParamsGroup && mLabelTabLengthSpin )
                {
                        mLabelTabParamsGroup->setVisible( hasAdjustable );
                        if ( hasAdjustable )
                        {
                                mLabelTabLengthSpin->setSuffix( mPropertiesView->lengthSpin->suffix() );
                                mLabelTabLengthSpin->setDecimals( mPropertiesView->lengthSpin->decimals() );
                                mLabelTabLengthSpin->setSingleStep( mPropertiesView->lengthSpin->singleStep() );
                                mLabelTabLengthSpin->setRange( mPropertiesView->lengthSpin->minimum(),
                                                               mPropertiesView->lengthSpin->maximum() );
                                mLabelTabLengthSpin->setValue( mPropertiesView->lengthSpin->value() );
                        }
                }

                if ( mLabelTabOrientationCombo )
                {
                        mLabelTabOrientationCombo->setEnabled( mPropertiesView->orientationCombo->isEnabled() );
                        mLabelTabOrientationCombo->setCurrentIndex( mPropertiesView->orientationCombo->currentIndex() );
                }

                if ( auto* labelTab = mRibbonBar->tab( ribbon::TabId::Label ) )
                {
                        labelTab->updateLayoutState();
                }
                mSyncingRibbon = false;
        }


        void MainWindow::updatePrintTabInfo()
        {
                if ( !mPrintTabDestCombo || !mPrintView )
                {
                        return;
                }

                mSyncingRibbon = true;
                const QSignalBlocker blocker( mPrintTabDestCombo );
                mPrintTabDestCombo->clear();
                for ( int i = 0; i < mPrintView->destinationCombo->count(); ++i )
                {
                        mPrintTabDestCombo->addItem( mPrintView->destinationCombo->itemText( i ) );
                }
                mPrintTabDestCombo->setCurrentIndex( mPrintView->destinationCombo->currentIndex() );

                if ( mPrintTabStatusLabel )
                {
                        const QString dest = mPrintTabDestCombo->currentText();
                        const QPrinterInfo info = QPrinterInfo::printerInfo( dest );
                        if ( info.isNull() )
                        {
                                mPrintTabStatusLabel->setText( tr( "PDF file output" ) );
                        }
                        else
                        {
                                switch ( info.state() )
                                {
                                case QPrinter::Idle:   mPrintTabStatusLabel->setText( tr( "Ready" ) );  break;
                                case QPrinter::Active: mPrintTabStatusLabel->setText( tr( "Busy" ) );   break;
                                case QPrinter::Error:  mPrintTabStatusLabel->setText( tr( "Error" ) );  break;
                                default:               mPrintTabStatusLabel->setText( tr( "Ready" ) );  break;
                                }
                        }
                }

                if ( mPrintRibbonBtn && mPrintSystemDialogRibbonBtn )
                {
                        mPrintRibbonBtn->setEnabled( mPrintView->printButton->isEnabled() );
                        mPrintSystemDialogRibbonBtn->setEnabled( mPrintView->systemDialogButton->isEnabled() );
                }
                mSyncingRibbon = false;
        }


        void MainWindow::updateStatusBarSummary()
        {
                if ( !mRibbonStatusBar )
                {
                        return;
                }

                if ( !mModel )
                {
                        mRibbonStatusBar->setLabelSummary( tr( "Welcome" ) );
                        mRibbonStatusBar->setSelectionCount( 0 );
                        return;
                }

                const auto units   = model::Settings::units();
                const auto tmplate = mModel->tmplate();
                const auto* frame  = tmplate.frame();

                QString summary = tmplate.description().isEmpty() ? tmplate.name() : tmplate.description();
                if ( frame )
                {
                        summary += QStringLiteral( " · %1 · %2" )
                                           .arg( frame->sizeDescription( units ),
                                                 tr( "%1 per sheet" ).arg( frame->nLabels() ) );
                }
                mRibbonStatusBar->setLabelSummary( summary );
                mRibbonStatusBar->setSelectionCount( mModel->getSelection().size() );
        }


        ///
        /// Manage enabled/visibility state of actions
        ///
        void MainWindow::manageActions()
        {
                const bool hasModel = ( mModel != nullptr );

                const bool isWelcomePage    = ( mCurrentPage == ribbon::PageId::Welcome );
                const bool isEditorPage     = ( mCurrentPage == ribbon::PageId::Editor );
                const bool isPropertiesPage = ( mCurrentPage == ribbon::PageId::Properties );
                const bool isMergePage      = ( mCurrentPage == ribbon::PageId::Merge );
                const bool isVariablesPage  = ( mCurrentPage == ribbon::PageId::Variables );
                const bool isFillPage       = ( mCurrentPage == ribbon::PageId::Fill );
                const bool isPrintPage      = ( mCurrentPage == ribbon::PageId::Print );
                const bool isDeployPage     = ( mCurrentPage == ribbon::PageId::Deploy );

                const bool hasSelection      = hasModel && !mModel->isSelectionEmpty();
                const bool hasMultiSelection = hasSelection && !mModel->isSelectionAtomic();
                const bool canPaste          = hasModel && QApplication::clipboard() &&
                                               QApplication::clipboard()->mimeData() && mModel->canPaste();

                // Recent file actions
                QStringList recentFileList = model::Settings::recentFileList();
                for ( int i = 0; i < recentFileList.size() && i < fileRecentActionList.size(); i++ )
                {
                        QString baseName = QFileInfo( recentFileList.at(i) ).completeBaseName();
                        fileRecentActionList.at(i)->setText( baseName );
                        fileRecentActionList.at(i)->setData( recentFileList.at(i) );
                        fileRecentActionList.at(i)->setVisible( true );
                }
                for ( int i = recentFileList.size(); i < fileRecentActionList.size(); i++ )
                {
                        fileRecentActionList.at(i)->setVisible( false );
                }
                if ( mBackstage )
                {
                        mBackstage->refresh();
                }

                // File actions
                fileNewAction->setEnabled( true );
                fileOpenAction->setEnabled( true );
                fileSaveAction->setEnabled( hasModel && mModel->isModified() );
                fileSaveAsAction->setEnabled( hasModel );
                fileShowEditorPageAction->setEnabled( !isWelcomePage && !isEditorPage );
                fileShowPropertiesPageAction->setEnabled( !isWelcomePage && !isPropertiesPage );
                // Data sub-page toggles stay enabled and show checked state for the active page (SPEC 6.1, s10-s12)
                fileShowMergePageAction->setEnabled( !isWelcomePage );
                fileShowMergePageAction->setChecked( isMergePage );
                fileShowVariablesPageAction->setEnabled( !isWelcomePage );
                fileShowVariablesPageAction->setChecked( isVariablesPage );
                fileShowFillPageAction->setEnabled( !isWelcomePage );
                fileShowFillPageAction->setChecked( isFillPage );
                fileShowPrintPageAction->setEnabled( !isWelcomePage && !isPrintPage );
                fileShowDeployPageAction->setEnabled( !isWelcomePage && !isDeployPage );
                fileTemplateDesignerAction->setEnabled( true );
                fileCloseAction->setEnabled( true );
                fileExitAction->setEnabled( true );

                // Edit actions
                if ( hasModel )
                {
                        if ( mUndoRedoModel->canUndo() )
                        {
                                editUndoAction->setEnabled( true );
                                // TRANSLATORS
                                //:   %1 is the action name/description (e.g. move, delete, ...)  to undo.
                                editUndoAction->setText( QString( tr("Undo %1") ).arg( mUndoRedoModel->undoDescription() ) );
                        }
                        else
                        {
                                editUndoAction->setEnabled( false );
                                editUndoAction->setText( tr("Undo") );
                        }
                        if ( mUndoRedoModel->canRedo() )
                        {
                                editRedoAction->setEnabled( true );
                                // TRANSLATORS
                                //:   %1 is the action name/description (e.g. move, delete, ...) to redo.
                                editRedoAction->setText( QString( tr("Redo %1") ).arg( mUndoRedoModel->redoDescription() ) );
                        }
                        else
                        {
                                editRedoAction->setEnabled( false );
                                editRedoAction->setText( tr("Redo") );
                        }
                }
                else
                {
                        editUndoAction->setEnabled( false );
                        editUndoAction->setText( tr("Undo") );
                        editRedoAction->setEnabled( false );
                        editRedoAction->setText( tr("Redo") );
                }
                editCutAction->setEnabled( isEditorPage && hasSelection );
                editCopyAction->setEnabled( isEditorPage && hasSelection );
                editPasteAction->setEnabled( isEditorPage && canPaste );
                editDeleteAction->setEnabled( isEditorPage && hasSelection );
                editSelectAllAction->setEnabled( isEditorPage );
                editUnSelectAllAction->setEnabled( isEditorPage && hasSelection );
                editPreferencesAction->setEnabled( true );

                // View actions
                viewFileToolBarAction->setEnabled( true );
                viewContentPanelAction->setEnabled( isEditorPage );
                viewCollapseRibbonAction->setEnabled( !isWelcomePage );
                viewGridAction->setEnabled( isEditorPage );
                viewMarkupAction->setEnabled( isEditorPage );
                viewSnapObjectsAction->setEnabled( isEditorPage );
                viewSnapCenterAction->setEnabled( isEditorPage );
                viewSnapSafeAreaAction->setEnabled( isEditorPage );
                viewZoomInAction->setEnabled( isEditorPage && ( !mLabelEditor || !mLabelEditor->isZoomMax() ) );
                viewZoomOutAction->setEnabled( isEditorPage && ( !mLabelEditor || !mLabelEditor->isZoomMin() ) );
                viewZoom1To1Action->setEnabled( isEditorPage );
                viewZoomToFitAction->setEnabled( isEditorPage );

                // Object actions
                objectsArrowModeAction->setEnabled( isEditorPage );
                objectsCreateTextAction->setEnabled( isEditorPage );
                objectsCreateLineAction->setEnabled( isEditorPage );
                objectsCreateBoxAction->setEnabled( isEditorPage );
                objectsCreateEllipseAction->setEnabled( isEditorPage );
                objectsCreateImageAction->setEnabled( isEditorPage );
                objectsCreateBarcodeAction->setEnabled( isEditorPage );
                objectsOrderRaiseAction->setEnabled( isEditorPage && hasSelection );
                objectsOrderLowerAction->setEnabled( isEditorPage && hasSelection );
                objectsXformRotateLeftAction->setEnabled( isEditorPage && hasSelection );
                objectsXformRotateRightAction->setEnabled( isEditorPage && hasSelection );
                objectsXformFlipHorizAction->setEnabled( isEditorPage && hasSelection );
                objectsXformFlipVertAction->setEnabled( isEditorPage && hasSelection );
                objectsAlignLeftAction->setEnabled( isEditorPage && hasMultiSelection );
                objectsAlignRightAction->setEnabled( isEditorPage && hasMultiSelection );
                objectsAlignHCenterAction->setEnabled( isEditorPage && hasMultiSelection );
                objectsAlignTopAction->setEnabled( isEditorPage && hasMultiSelection );
                objectsAlignBottomAction->setEnabled( isEditorPage && hasMultiSelection );
                objectsAlignVCenterAction->setEnabled( isEditorPage && hasMultiSelection );
                objectsCenterHorizAction->setEnabled( isEditorPage && hasSelection );
                objectsCenterVertAction->setEnabled( isEditorPage && hasSelection );
                objectsCenterAction->setEnabled( isEditorPage && hasSelection );

                // Context menu submenu enable states
                contextOrderMenu->setEnabled( isEditorPage && hasSelection );
                contextXformMenu->setEnabled( isEditorPage && hasSelection );
                contextAlignMenu->setEnabled( isEditorPage && hasMultiSelection );
                contextCenterMenu->setEnabled( isEditorPage && hasSelection );

                // Help actions
                helpContentsAction->setEnabled( true );
                helpReportBugAction->setEnabled( true );
                helpCheckForUpdatesAction->setEnabled( true );
                helpAboutAction->setEnabled( true );

                // Special context actions
                contextCutAction->setEnabled( isEditorPage && hasSelection );
                contextCopyAction->setEnabled( isEditorPage && hasSelection );
                contextPasteAction->setEnabled( isEditorPage && canPaste );
                contextDeleteAction->setEnabled( isEditorPage && hasSelection );

                // Keep Print / Fill output button enable states mirrored
                if ( mPrintRibbonBtn && mPrintView )
                {
                        mPrintRibbonBtn->setEnabled( mPrintView->printButton->isEnabled() );
                        mPrintSystemDialogRibbonBtn->setEnabled( mPrintView->systemDialogButton->isEnabled() );
                }
                if ( mFillPrintRibbonBtn && mFillView )
                {
                        mFillPrintRibbonBtn->setEnabled( mFillView->printButton->isEnabled() );
                        mFillSystemDialogRibbonBtn->setEnabled( mFillView->systemDialogButton->isEnabled() );
                }
        }


        ///
        /// Set window title
        ///
        void MainWindow::setTitle()
        {
                if ( !mModel )
                {
                        setWindowTitle( model::Branding::DISPLAY_NAME );
                }
                else
                {
                        if ( mModel->isModified() )
                        {
                                setWindowTitle( model::Branding::DISPLAY_NAME + " - " + mModel->shortName() + " " + tr("(modified)") );
                        }
                        else
                        {
                                setWindowTitle( model::Branding::DISPLAY_NAME + " - " + mModel->shortName() );
                        }
                }
        }


        ///
        /// Read MainWindow Settings
        ///
        void MainWindow::readSettings()
        {
                QSettings settings;

                settings.beginGroup( "MainWindow" );
                bool showFileToolBar  = settings.value( "showFileToolBar",  true ).toBool();
                bool showContentPanel = settings.value( "showContentPanel", true ).toBool();
                bool ribbonCollapsed  = settings.value( "ribbonCollapsed",  false ).toBool();
                bool showGrid         = settings.value( "showGrid",         true ).toBool();
                bool showMarkup       = settings.value( "showMarkup",       true ).toBool();
                settings.endGroup();

                viewFileToolBarAction   ->setChecked( showFileToolBar );
                viewContentPanelAction  ->setChecked( showContentPanel );
                viewCollapseRibbonAction->setChecked( ribbonCollapsed );
                viewGridAction          ->setChecked( showGrid );
                viewMarkupAction        ->setChecked( showMarkup );

                mRibbonBar->quickAccessBar()->setVisible( showFileToolBar );
                mRibbonBar->setCollapsed( ribbonCollapsed );
                mContentPanel->setVisible( showContentPanel );
                mLabelEditor->setGridVisible( showGrid );
                mLabelEditor->setMarkupVisible( showMarkup );
        }


        ///
        /// Write MainWindow Settings
        ///
        void MainWindow::writeSettings()
        {
                QSettings settings;

                settings.beginGroup( "MainWindow" );
                settings.setValue( "showFileToolBar",  viewFileToolBarAction->isChecked() );
                settings.setValue( "showContentPanel", viewContentPanelAction->isChecked() );
                settings.setValue( "ribbonCollapsed",  mRibbonBar->isCollapsed() );
                settings.setValue( "showGrid",         viewGridAction->isChecked() );
                settings.setValue( "showMarkup",       viewMarkupAction->isChecked() );
                settings.endGroup();
        }


        ///
        /// Is it ok to close window?
        ///
        bool MainWindow::isOkToClose()
        {
                bool ok = true;

                if ( !this->isEmpty() )
                {
                        if ( mModel->isModified() )
                        {
                                QString msg = tr("Save changes to project \"%1\" before closing?").arg( mModel->shortName() );
                                QString info = tr("Your changes will be lost if you don't save them.");

                                int ret = QMessageBox::warning( this,
                                                                tr( "Save project?" ),
                                                                "<b>" + msg + "</b><p>" + info + "</p>",
                                                                (QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel),
                                                                QMessageBox::Save );

                                switch (ret) {
                                case QMessageBox::Save:
                                        ok = File::save( this );
                                        break;
                                case QMessageBox::Discard:
                                        ok = true;
                                        break;
                                case QMessageBox::Cancel:
                                default:
                                        ok = false;
                                        break;
                                }
                        }
                }

                return ok;
        }


        void MainWindow::clipboardChanged()
        {
                manageActions();
        }


        void MainWindow::fileNew()
        {
                File::newLabel( this );
        }


        void MainWindow::fileOpen()
        {
                File::open( this );
        }


        void MainWindow::fileOpenRecent()
        {
                QAction* action = qobject_cast<QAction*>( sender() );
                if ( action )
                {
                        QString fileName = action->data().toString();
                        File::open( fileName, this );
                }
        }


        void MainWindow::fileSave()
        {
                File::save( this );
        }


        void MainWindow::fileSaveAs()
        {
                File::saveAs( this );
        }


        void MainWindow::fileShowEditorPage()
        {
                switchToPage( ribbon::PageId::Editor, true );
        }


        void MainWindow::fileShowPropertiesPage()
        {
                switchToPage( ribbon::PageId::Properties, true );
        }


        void MainWindow::fileShowMergePage()
        {
                switchToPage( ribbon::PageId::Merge, true );
        }


        void MainWindow::fileShowVariablesPage()
        {
                switchToPage( ribbon::PageId::Variables, true );
        }


        void MainWindow::fileShowFillPage()
        {
                switchToPage( ribbon::PageId::Fill, true );
        }


        void MainWindow::fileShowPrintPage()
        {
                switchToPage( ribbon::PageId::Print, true );
        }


        void MainWindow::fileShowDeployPage()
        {
                switchToPage( ribbon::PageId::Deploy, true );
        }


        void MainWindow::fileTemplateDesigner()
        {
                File::templateDesigner( this );
        }


        void MainWindow::fileClose()
        {
                File::close( this );
        }


        void MainWindow::fileExit()
        {
                File::exit();
        }


        void MainWindow::editUndo()
        {
                mUndoRedoModel->undo();
        }


        void MainWindow::editRedo()
        {
                mUndoRedoModel->redo();
        }


        void MainWindow::editCut()
        {
                mUndoRedoModel->checkpoint( tr("Cut") );
                mModel->cutSelection();
        }


        void MainWindow::editCopy()
        {
                mModel->copySelection();
        }


        void MainWindow::editPaste()
        {
                mUndoRedoModel->checkpoint( tr("Paste") );
                mModel->paste( model::Point() );
        }


        void MainWindow::editContextPaste()
        {
                auto *action = qobject_cast<QAction *>(sender());
                auto p = action->data().value<model::Point>();

                mUndoRedoModel->checkpoint( tr("Paste") );
                mModel->paste( p );
        }


        void MainWindow::editDelete()
        {
                mUndoRedoModel->checkpoint( tr("Delete") );
                mModel->deleteSelection();
        }


        void MainWindow::editSelectAll()
        {
                mModel->selectAll();
        }


        void MainWindow::editUnSelectAll()
        {
                mModel->unselectAll();
        }


        void MainWindow::editPreferences()
        {
                PreferencesDialog dialog( this );
                dialog.exec();
        }


        void MainWindow::viewFileToolBar( bool state )
        {
                if ( mRibbonBar )
                {
                        mRibbonBar->quickAccessBar()->setVisible( state );
                }
        }


        void MainWindow::viewContentPanel( bool state )
        {
                if ( mContentPanel )
                {
                        mContentPanel->setVisible( state );
                }
        }


        void MainWindow::viewGrid( bool state )
        {
                mLabelEditor->setGridVisible( state );
        }


        void MainWindow::viewMarkup( bool state )
        {
                mLabelEditor->setMarkupVisible( state );
        }


        void MainWindow::viewZoomIn()
        {
                mLabelEditor->zoomIn();
        }


        void MainWindow::viewZoomOut()
        {
                mLabelEditor->zoomOut();
        }


        void MainWindow::viewZoom1To1()
        {
                mLabelEditor->zoom1To1();
        }


        void MainWindow::viewZoomToFit()
        {
                mLabelEditor->zoomToFit();
        }


        void MainWindow::objectsArrowMode()
        {
                mCreatingObject = false;
                mLabelEditor->arrowMode();
        }


        void MainWindow::objectsCreateText()
        {
                mCreatingObject = true;
                mUndoRedoModel->checkpoint( tr("Create Text") );
                mLabelEditor->createTextMode();
        }


        void MainWindow::objectsCreateBox()
        {
                mCreatingObject = true;
                mUndoRedoModel->checkpoint( tr("Create Box") );
                mLabelEditor->createBoxMode();
        }


        void MainWindow::objectsCreateLine()
        {
                mCreatingObject = true;
                mUndoRedoModel->checkpoint( tr("Create Line") );
                mLabelEditor->createLineMode();
        }


        void MainWindow::objectsCreateEllipse()
        {
                mCreatingObject = true;
                mUndoRedoModel->checkpoint( tr("Create Ellipse") );
                mLabelEditor->createEllipseMode();
        }


        void MainWindow::objectsCreateImage()
        {
                mCreatingObject = true;
                mUndoRedoModel->checkpoint( tr("Create Image") );
                mLabelEditor->createImageMode();
        }


        void MainWindow::objectsCreateBarcode()
        {
                mCreatingObject = true;
                mUndoRedoModel->checkpoint( tr("Create Barcode") );
                mLabelEditor->createBarcodeMode();
        }


        void MainWindow::objectsOrderRaise()
        {
                mUndoRedoModel->checkpoint( tr("Bring To Front") );
                mModel->raiseSelectionToTop();
        }


        void MainWindow::objectsOrderLower()
        {
                mUndoRedoModel->checkpoint( tr("Send To Back") );
                mModel->lowerSelectionToBottom();
        }


        void MainWindow::objectsXformRotateLeft()
        {
                mUndoRedoModel->checkpoint( tr("Rotate Left") );
                mModel->rotateSelectionLeft();
        }


        void MainWindow::objectsXformRotateRight()
        {
                mUndoRedoModel->checkpoint( tr("Rotate Right") );
                mModel->rotateSelectionRight();
        }


        void MainWindow::objectsXformFlipHoriz()
        {
                mUndoRedoModel->checkpoint( tr("Flip Horizontally") );
                mModel->flipSelectionHoriz();
        }


        void MainWindow::objectsXformFlipVert()
        {
                mUndoRedoModel->checkpoint( tr("Flip Vertically") );
                mModel->flipSelectionVert();
        }


        void MainWindow::objectsAlignLeft()
        {
                mUndoRedoModel->checkpoint( tr("Align Left") );
                mModel->alignSelectionLeft();
        }


        void MainWindow::objectsAlignHCenter()
        {
                mUndoRedoModel->checkpoint( tr("Align Center") );
                mModel->alignSelectionHCenter();
        }


        void MainWindow::objectsAlignRight()
        {
                mUndoRedoModel->checkpoint( tr("Align Right") );
                mModel->alignSelectionRight();
        }


        void MainWindow::objectsAlignTop()
        {
                mUndoRedoModel->checkpoint( tr("Align Top") );
                mModel->alignSelectionTop();
        }


        void MainWindow::objectsAlignVCenter()
        {
                mUndoRedoModel->checkpoint( tr("Align Middle") );
                mModel->alignSelectionVCenter();
        }


        void MainWindow::objectsAlignBottom()
        {
                mUndoRedoModel->checkpoint( tr("Align Bottom") );
                mModel->alignSelectionBottom();
        }


        void MainWindow::objectsCenterHoriz()
        {
                mUndoRedoModel->checkpoint( tr("Center Horizontally") );
                mModel->centerSelectionHoriz();
        }


        void MainWindow::objectsCenter()
        {
                mUndoRedoModel->checkpoint( tr("Center") );
                mModel->centerSelection();
        }


        void MainWindow::objectsCenterVert()
        {
                mUndoRedoModel->checkpoint( tr("Center Vertically") );
                mModel->centerSelectionVert();
        }


        void MainWindow::helpContents()
        {
                Help::displayContents( this );
        }


        void MainWindow::helpReportBug()
        {
                Help::displayReportBug( this, mModel.get() );
        }


        void MainWindow::helpAbout()
        {
                Help::displayAbout( this );
        }


        void MainWindow::onContextMenuActivate( model::Point p )
        {
                QVariant variant;
                variant.setValue( p );
                contextPasteAction->setData( variant );

                if ( mModel->isSelectionEmpty() )
                {
                        noSelectionContextMenu->popup( QCursor::pos() );
                }
                else
                {
                        contextMenu->popup( QCursor::pos() );
                }
        }


        void MainWindow::onZoomChanged()
        {
                if ( mRibbonStatusBar )
                {
                        mRibbonStatusBar->setZoomFactor( mLabelEditor->zoom() );
                }
                viewZoomInAction->setEnabled( mCurrentPage == ribbon::PageId::Editor && !mLabelEditor->isZoomMax() );
                viewZoomOutAction->setEnabled( mCurrentPage == ribbon::PageId::Editor && !mLabelEditor->isZoomMin() );
        }


        void MainWindow::onPointerMoved( model::Point p )
        {
                if ( mRibbonStatusBar )
                {
                        const auto units = model::Settings::units();
                        const int digits = units.resolutionDigits();
                        const QString suffix = units.toIdString();
                        mRibbonStatusBar->setCursorPositionText(
                                QStringLiteral( "x %1 %3  y %2 %3" )
                                        .arg( p.x().inUnits( units ), 0, 'f', digits )
                                        .arg( p.y().inUnits( units ), 0, 'f', digits )
                                        .arg( suffix ) );
                }
        }


        void MainWindow::onPointerExit()
        {
                if ( mRibbonStatusBar )
                {
                        mRibbonStatusBar->setCursorPositionText( QString() );
                }
        }


        void MainWindow::onNameChanged()
        {
                setTitle();
                updateStatusBarSummary();
        }


        void MainWindow::onModifiedChanged()
        {
                manageActions();
                setTitle();
        }


        void MainWindow::onSelectionChanged()
        {
                manageActions();
                updateContextualTabState( false );
                updateStatusBarSummary();
        }


        void MainWindow::onLabelChanged()
        {
                updateLabelTabInfo();
                updatePrintTabInfo();
                updateStatusBarSummary();
        }


        void MainWindow::onUndoRedoChanged()
        {
                manageActions();
        }


        void MainWindow::onSettingsChanged()
        {
                if ( mViewColorSchemeGroup )
                {
                        mViewColorSchemeGroup->setCurrentId( model::Settings::colorScheme() );
                }
                if ( mViewIconStyleGroup )
                {
                        mViewIconStyleGroup->setCurrentId( model::Settings::iconStyle() );
                }
                if ( viewSnapObjectsAction )
                {
                        const QSignalBlocker b( viewSnapObjectsAction );
                        viewSnapObjectsAction->setChecked( model::Settings::snapObjects() );
                }
                if ( viewSnapCenterAction )
                {
                        const QSignalBlocker b( viewSnapCenterAction );
                        viewSnapCenterAction->setChecked( model::Settings::snapCenter() );
                }
                if ( viewSnapSafeAreaAction )
                {
                        const QSignalBlocker b( viewSnapSafeAreaAction );
                        viewSnapSafeAreaAction->setChecked( model::Settings::snapSafeArea() );
                }
                manageActions();
                updateLabelTabInfo();
                updateStatusBarSummary();
        }

} // namespace glabels
