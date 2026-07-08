//  FillWindow.cpp
//

#include "FillWindow.hpp"

#include "AdminDialog.hpp"
#include "FillConfig.hpp"

#include "FillView.hpp"
#include "PrinterMonitor.hpp"

#include "model/Db.hpp"
#include "model/Model.hpp"
#include "model/Settings.hpp"
#include "model/XmlLabelParser.hpp"
#include "model/XmlLabelCreator.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>


namespace glabels
{


        namespace
        {
                bool isAdminChord( QKeyEvent* e )
                {
                        return ( e->modifiers() & Qt::ControlModifier ) &&
                               ( e->modifiers() & Qt::ShiftModifier ) &&
                               ( e->key() == Qt::Key_A );
                }
        }


        FillWindow::FillWindow( const FillConfig& cfg, QWidget* parent )
                : QMainWindow( parent ), mCfg( cfg )
        {
                mExePath = QCoreApplication::applicationFilePath();
                setWindowTitle( mCfg.appName.isEmpty() ? QStringLiteral("glabels Fill") : mCfg.appName );

                auto* central = new QWidget( this );
                auto* topLayout = new QVBoxLayout( central );
                topLayout->setContentsMargins( 0, 0, 0, 0 );
                topLayout->setSpacing( 0 );

                // Label selector (only shown if multiple labels).
                int labelCount = mCfg.embeddedLabels.size() + mCfg.labelFiles.size();
                if ( labelCount > 1 )
                {
                        auto* bar = new QHBoxLayout();
                        bar->setContentsMargins( 8, 4, 8, 4 );
                        bar->addWidget( new QLabel( tr("Label:") ) );
                        mLabelCombo = new QComboBox();
                        for ( const auto& lbl : mCfg.embeddedLabels )
                        {
                                mLabelCombo->addItem( lbl.name.isEmpty() ? tr("(embedded)") : lbl.name );
                        }
                        for ( const auto& path : mCfg.labelFiles )
                        {
                                mLabelCombo->addItem( QFileInfo(path).completeBaseName() );
                        }
                        bar->addWidget( mLabelCombo, 1 );
                        topLayout->addLayout( bar );
                        connect( mLabelCombo, SIGNAL(currentIndexChanged(int)),
                                 this, SLOT(onLabelChanged(int)) );
                }

                mFillView = new FillView( central );
                topLayout->addWidget( mFillView );

                setCentralWidget( central );
                statusBar()->showMessage( tr("Press Ctrl+Shift+A for administration.") );

                loadCurrentLabel();
        }


        FillWindow::~FillWindow() = default;


        void FillWindow::loadCurrentLabel()
        {
                if ( !mCfg.hasLabels() )
                {
                        statusBar()->showMessage(
                                tr("No label configured. Press Ctrl+Shift+A to set one.") );
                        return;
                }

                model::Settings::init();
                model::Db::init();

                int idx = mLabelCombo ? mLabelCombo->currentIndex() : 0;

                std::unique_ptr<model::Model> model;
                if ( idx < mCfg.embeddedLabels.size() )
                {
                        const auto& lbl = mCfg.embeddedLabels.at( idx );
                        model.reset( model::XmlLabelParser::readBuffer( lbl.xml ) );
                }
                else
                {
                        int fileIdx = idx - mCfg.embeddedLabels.size();
                        if ( fileIdx >= 0 && fileIdx < mCfg.labelFiles.size() )
                        {
                                model.reset( model::XmlLabelParser::readFile( mCfg.labelFiles.at(fileIdx) ) );
                        }
                }

                if ( !model )
                {
                        QString detail = tr("Could not load label.");
                        if ( idx < mCfg.embeddedLabels.size() )
                        {
                                const auto& lbl = mCfg.embeddedLabels.at( idx );
                                detail += tr("\n\nEmbedded label '%1', XML size: %2 bytes.")
                                          .arg( lbl.name )
                                          .arg( lbl.xml.size() );
                                if ( lbl.xml.size() > 0 )
                                {
                                        detail += "\n\nFirst 200 chars:\n" + QString::fromUtf8( lbl.xml.left(200) );
                                }
                        }
                        QMessageBox::critical( this, tr("Load error"), detail );
                        return;
                }

                // Always pass locked fields + optional source to FillView.
                // setKioskSource stores both; setModel applies them after
                // the field columns are populated.
                mFillView->setKioskSource( mCfg.sourcePath, mCfg.lockedFields );

                mModel = std::move( model );
                mFillView->setModel( mModel.get() );

                if ( !mCfg.printerName.isEmpty() )
                {
                        model::Settings::setRecentPrinter( mCfg.printerName );
                }
        }


        void FillWindow::onLabelChanged( int )
        {
                loadCurrentLabel();
        }


        void FillWindow::keyPressEvent( QKeyEvent* event )
        {
                if ( isAdminChord( event ) )
                {
                        onAdmin();
                        event->accept();
                        return;
                }
                QMainWindow::keyPressEvent( event );
        }


        void FillWindow::onAdmin()
        {
                AdminDialog dlg( mCfg, this );
                if ( dlg.exec() == QDialog::Accepted )
                {
                        mCfg = dlg.result();
                        if ( !FillConfigIO::writeToExe( mExePath, mCfg ) )
                        {
                                QMessageBox::warning( this, tr("Admin"),
                                                      tr("Could not write settings to the executable.") );
                        }
                        setWindowTitle( mCfg.appName.isEmpty() ? QStringLiteral("glabels Fill") : mCfg.appName );
                        loadCurrentLabel();
                }
        }


} // namespace glabels
