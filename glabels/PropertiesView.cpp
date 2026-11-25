//  PropertiesView.cpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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


#include "PropertiesView.hpp"

#include "SelectProductDialog.hpp"
#include "UndoRedoModel.hpp"
#include "theme/Theme.hpp"

#include "model/Db.hpp"
#include "model/FrameContinuous.hpp"
#include "model/Settings.hpp"

#include <QDebug>
#include <QFrame>
#include <QGroupBox>
#include <QLayout>
#include <QStyledItemDelegate>
#include <QVBoxLayout>


namespace glabels
{

        ///
        /// Constructor
        ///
        PropertiesView::PropertiesView( QWidget *parent )
                : QWidget(parent), mModel(nullptr), mUndoRedoModel(nullptr)
        {
                setupUi( this );

                titleLabel->setText( tr("Label") );
                titleLabel->setProperty( "role", QStringLiteral( "pageTitle" ) );

                if ( auto* l = layout() )
                {
                        l->setContentsMargins( 24, 20, 24, 20 );
                        l->setSpacing( 16 );
                }
                verticalLayout->setContentsMargins( 0, 0, 0, 0 );
                verticalLayout->setSpacing( 12 );
                line->hide();

                // Sentence case group titles & 460px left column width (SPEC 6.4 & s09 mockup)
                parametersGroupBox->setTitle( tr("Adjustable parameters") );
                similarProductsGroupBox->setTitle( tr("Similar products") );
                for ( QWidget* w : QList<QWidget*>{ productGroupBox, parametersGroupBox, orientationGroupBox,
                                                    similarProductsGroupBox, similarProductsNullBox } )
                {
                        w->setFixedWidth( 460 );
                }

                // Neo tag icon on Change product button & compact orientation combo icon
                auto applyChangeIcon = [this]()
                {
                        changeProductButton->setIcon( theme::Theme::instance().icon( QStringLiteral( "tag" ) ) );
                        changeProductButton->setIconSize( QSize( 16, 16 ) );
                };
                applyChangeIcon();
                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, [applyChangeIcon]() { applyChangeIcon(); } );
                orientationCombo->setIconSize( QSize( 16, 16 ) );

                // Wrap SimplePreview inside a Preview card with a work-colored backdrop (s09)
                horizontalLayout->removeWidget( preview );
                auto* previewCard = new QGroupBox( tr("Preview"), this );
                previewCard->setProperty( "role", QStringLiteral( "card" ) );
                auto* cardLayout = new QVBoxLayout( previewCard );
                cardLayout->setContentsMargins( 0, 0, 0, 0 );
                cardLayout->setSpacing( 0 );

                auto* well = new QFrame( previewCard );
                well->setProperty( "role", QStringLiteral( "workWell" ) );
                well->setAttribute( Qt::WA_StyledBackground, true );
                auto* wellLayout = new QVBoxLayout( well );
                wellLayout->setContentsMargins( 20, 20, 20, 20 );
                preview->setParent( well );
                preview->setProperty( "role", QStringLiteral( "workWell" ) );
                wellLayout->addWidget( preview, 1 );
                cardLayout->addWidget( well, 1 );
                horizontalLayout->addWidget( previewCard, 1 );

                for ( auto* gb : findChildren<QGroupBox*>() )
                {
                        gb->setProperty( "role", QStringLiteral( "card" ) );
                }

                similarBrowser->setAttribute(Qt::WA_TranslucentBackground);
                similarBrowser->viewport()->setAutoFillBackground(false);

                connect( model::Settings::instance(), SIGNAL(changed()),
                         this, SLOT(onSettingsChanged()) );

                onSettingsChanged();
        }


        ///
        /// Set Model
        ///
        void PropertiesView::setModel( model::Model* model, UndoRedoModel* undoRedoModel )
        {
                mModel = model;
                mUndoRedoModel = undoRedoModel;

                connect( mModel, SIGNAL(sizeChanged()), this, SLOT(onLabelSizeChanged()) );

                onLabelSizeChanged();
        }


        ///
        /// Settings changed handler
        ///
        void PropertiesView::onSettingsChanged()
        {
                mUnits = model::Settings::units();
                if (mModel)
                {
                        onLabelSizeChanged();
                }
        }


        ///
        /// Label size changed handler
        ///
        void PropertiesView::onLabelSizeChanged()
        {
                auto tmplate   = mModel->tmplate();
                auto frame     = tmplate.frame();
                bool isRotated = mModel->rotate();

                preview->setTemplate( tmplate );
                preview->setShowArrow( true );
                preview->setRotate( isRotated );

                vendorLabel->setText( tmplate.brand() );
                if ( model::Db::isVendorNameKnown( tmplate.brand() ) )
                {
                        auto vendor = model::Db::lookupVendorFromName( tmplate.brand() );
                        if ( !vendor.url().isEmpty() )
                        {
                                QString markup = QString( "<a href='%1'>%2</a>" ).arg( vendor.url(), vendor.name() );
                                vendorLabel->setText( markup );
                        }
                }

                if ( !tmplate.productUrl().isEmpty() )
                {
                        QString markup = QString( "<a href='%1'>%2</a>" ).arg( tmplate.productUrl(), tmplate.part() );
                        partLabel->setText( markup );
                }
                else
                {
                        partLabel->setText( tmplate.part() );
                }

                descriptionLabel->setText( tmplate.description() );
                pageSizeLabel->setText( tmplate.paperDescription( mUnits ) );
                labelSizeLabel->setText( frame->sizeDescription( mUnits ) );
                layoutLabel->setText( frame->layoutDescription() );

                QStringList list = model::Db::getNameListOfSimilarTemplates( tmplate.name() );
                if ( list.isEmpty() )
                {
                        similarProductsGroupBox->hide();
                        similarProductsNullBox->show();
                }
                else
                {
                        similarProductsGroupBox->show();
                        similarProductsNullBox->hide();

                        QString similarListString;
                        for ( QString name : list )
                        {
                                similarListString += name + "\n";
                        }
                        similarBrowser->setText( similarListString );
                }

                orientationCombo->setEnabled( frame->w() != frame->h() );
                if ( frame->w() == frame->h() )
                {
                        orientationCombo->setCurrentIndex(0);
                }
                else if ( frame->w() > frame->h() )
                {
                        orientationCombo->setCurrentIndex( isRotated ? 1 : 0 );
                }
                else
                {
                        orientationCombo->setCurrentIndex( isRotated ? 0 : 1 );
                }
                mOldOrientationIndex = orientationCombo->currentIndex();

                if ( auto* continuousFrame = dynamic_cast<const model::FrameContinuous*>( frame ) )
                {
                        if (!mInLengthSpinChanged)
                        {
                                const QSignalBlocker blocker( lengthSpin );

                                lengthSpin->setSuffix( " " + mUnits.toTrName() );
                                lengthSpin->setDecimals( mUnits.resolutionDigits() );
                                lengthSpin->setSingleStep( mUnits.resolution() );
                                lengthSpin->setMinimum( continuousFrame->hMin().inUnits( mUnits ) );
                                lengthSpin->setMaximum( continuousFrame->hMax().inUnits( mUnits ) );
                                lengthSpin->setValue( continuousFrame->h().inUnits( mUnits ) );
                        }

                        parametersGroupBox->setVisible( true );
                }
                else
                {
                        parametersGroupBox->setVisible( false );
                }
        }


        ///
        /// Length spin changed handler
        ///
        void PropertiesView::onLengthSpinChanged()
        {
                mInLengthSpinChanged = true;
                mModel->setH( model::Distance( lengthSpin->value(), mUnits ) );
                mInLengthSpinChanged = false;
        }


        ///
        /// Orientation combo box changed handler
        ///
        void PropertiesView::onOrientationActivated()
        {
                auto tmplate = mModel->tmplate();
                auto frame   = tmplate.frame();

                // Make sure index actually changed.
                int index = orientationCombo->currentIndex();
                if ( index != mOldOrientationIndex )
                {
                        mOldOrientationIndex = index;

                        mUndoRedoModel->checkpoint( tr("Product Rotate") );

                        if ( frame->w() == frame->h() )
                        {
                                mModel->setRotate( false );
                        }
                        else if ( frame->w() > frame->h() )
                        {
                                mModel->setRotate( index == 1 );
                        }
                        else
                        {
                                mModel->setRotate( index == 0 );
                        }
                }
        }


        ///
        /// Change Product Button Clicked handler
        ///
        void PropertiesView::onChangeProductButtonClicked()
        {
                SelectProductDialog selectProductDialog( this );
                selectProductDialog.exec();

                auto tmplate = selectProductDialog.tmplate();
                if ( !tmplate.isNull() )
                {
                        mUndoRedoModel->checkpoint( tr("Change Product") );

                        mModel->setTmplate( tmplate );

                        // Don't rotate circular or round labels
                        auto frame = tmplate.frame();
                        if ( frame->w() == frame->h() )
                        {
                                mModel->setRotate( false );
                        }
                }
        }

} // namespace glabels
