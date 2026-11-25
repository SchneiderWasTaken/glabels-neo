//  VariablesView.cpp
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


#include "VariablesView.hpp"

#include "EditVariableDialog.hpp"

#include <QColor>
#include <QDebug>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QTableWidgetItem>
#include <QVBoxLayout>


namespace
{
        enum ICol {
                I_COL_NAME,
                I_COL_TYPE,
                I_COL_VALUE,
                I_COL_INCREMENT,
                I_COL_STEP_SIZE,
                I_COL_DUMMY,
                N_COLS
        };
}


namespace glabels
{

        ///
        /// Constructor
        ///
        VariablesView::VariablesView( QWidget *parent )
                : QWidget(parent), mModel(nullptr), mUndoRedoModel(nullptr)
        {
                setupUi( this );

                titleLabel->setText( tr("Variables") );
                titleLabel->setProperty( "role", QStringLiteral( "pageTitle" ) );

                auto* intro = new QLabel(
                        tr( "Variables are available as ${name} fields in text, barcodes and images." ), this );
                intro->setWordWrap( true );
                intro->setProperty( "role", QStringLiteral( "caption" ) );

                if ( auto* gl = qobject_cast<QGridLayout*>( layout() ) )
                {
                        gl->setContentsMargins( 24, 20, 24, 20 );
                        gl->setVerticalSpacing( 10 );
                        gl->removeWidget( table );
                        gl->removeItem( horizontalLayout );
                        gl->addWidget( intro, 1, 0 );
                        gl->addWidget( table, 2, 0 );
                        gl->addLayout( horizontalLayout, 3, 0 );
                        gl->setRowStretch( 2, 1 );
                }
                for ( auto* gb : findChildren<QGroupBox*>() )
                {
                        gb->setProperty( "role", QStringLiteral( "card" ) );
                }

                // Add/Edit/Delete live on Data > Variables ribbon; keep wired
                addButton->hide();
                editButton->hide();
                deleteButton->hide();

                connect( table, &QTableWidget::itemDoubleClicked,
                         this, [this]( QTableWidgetItem* ) { onEditButtonClicked(); } );

                table->setColumnCount( N_COLS );

                auto* nameHeaderItem = new QTableWidgetItem( tr("Name") );
                nameHeaderItem->setFlags( nameHeaderItem->flags() ^ Qt::ItemIsEditable );
                table->setHorizontalHeaderItem( I_COL_NAME, nameHeaderItem );

                auto* typeHeaderItem = new QTableWidgetItem( tr("Type") );
                typeHeaderItem->setFlags( typeHeaderItem->flags() ^ Qt::ItemIsEditable );
                table->setHorizontalHeaderItem( I_COL_TYPE, typeHeaderItem );

                auto* valueHeaderItem = new QTableWidgetItem( tr("Value") );
                valueHeaderItem->setFlags( valueHeaderItem->flags() ^ Qt::ItemIsEditable );
                table->setHorizontalHeaderItem( I_COL_VALUE, valueHeaderItem );

                auto* incrementHeaderItem = new QTableWidgetItem( tr("Increment") );
                incrementHeaderItem->setFlags( incrementHeaderItem->flags() ^ Qt::ItemIsEditable );
                table->setHorizontalHeaderItem( I_COL_INCREMENT, incrementHeaderItem );

                auto* stepSizeHeaderItem = new QTableWidgetItem( tr("Step Size") );
                stepSizeHeaderItem->setFlags( stepSizeHeaderItem->flags() ^ Qt::ItemIsEditable );
                table->setHorizontalHeaderItem( I_COL_STEP_SIZE, stepSizeHeaderItem );

                auto* dummyHeaderItem = new QTableWidgetItem();
                dummyHeaderItem->setFlags( Qt::NoItemFlags );
                table->setHorizontalHeaderItem( I_COL_DUMMY, dummyHeaderItem );
                table->horizontalHeader()->setStretchLastSection( true );
        }


        ///
        /// Set Model
        ///
        void VariablesView::setModel( model::Model* model, UndoRedoModel* undoRedoModel )
        {
                mModel = model;
                mUndoRedoModel = undoRedoModel;

                updateControls();
                loadTable();

                connect( mModel, SIGNAL(variablesChanged()), this, SLOT(onVariablesChanged()) );
        }


        ///
        /// table Selection Changed
        ///
        void VariablesView::onTableSelectionChanged()
        {
                updateControls();
        }


        ///
        /// addButton Clicked
        ///
        void VariablesView::onAddButtonClicked()
        {
                EditVariableDialog dialog( this );

                model::Variable v( model::Variable::Type::INTEGER,
                                   "x",
                                   "0",
                                   model::Variable::Increment::NEVER,
                                   "1" );
                dialog.setVariable( v );
                dialog.setWindowTitle( tr("Add Variable") );

                if ( dialog.exec() == QDialog::Accepted )
                {
                        mModel->variables().addVariable( dialog.variable() );
                        selectVariable( dialog.variable().name() );
                }
        }


        ///
        /// editButton Clicked
        ///
        void VariablesView::onEditButtonClicked()
        {
                if ( table->selectedItems().isEmpty() || !mModel )
                {
                        return;
                }
                int iRow = table->selectedItems()[0]->row();
                QString name = table->item( iRow, I_COL_NAME )->text();

                if ( mModel->variables().hasVariable( name ) )
                {
                        model::Variable v = mModel->variables().value( name );

                        EditVariableDialog dialog( this );
                        dialog.setVariable( v );
                        dialog.setWindowTitle( tr("Edit Variable") );

                        if ( dialog.exec() == QDialog::Accepted )
                        {
                                mModel->variables().replaceVariable( name, dialog.variable() );
                                selectVariable( dialog.variable().name() );
                        }
                }
        }


        ///
        /// deleteButton Clicked
        ///
        void VariablesView::onDeleteButtonClicked()
        {
                if ( table->selectedItems().isEmpty() || !mModel )
                {
                        return;
                }
                int iRow = table->selectedItems()[0]->row();

                QString name = table->item( iRow, I_COL_NAME )->text();
                mModel->variables().deleteVariable( name );
        }


        ///
        /// Variables Changed
        ///
        void VariablesView::onVariablesChanged()
        {
                // Reload table from variables
                loadTable();
        }


        ///
        /// update controls
        ///
        void VariablesView::updateControls()
        {
                bool hasSelection = !table->selectedItems().isEmpty();

                editButton->setEnabled( hasSelection );
                deleteButton->setEnabled( hasSelection );
        }


        ///
        /// load table from variables
        ///
        void VariablesView::loadTable()
        {
                table->clearContents();
                table->setRowCount( mModel->variables().size() );

                int iRow = 0;
                for( const auto& v : mModel->variables() )
                {
                        auto* typeItem = new QTableWidgetItem( model::Variable::typeToI18nString(v.type()) );
                        typeItem->setFlags( typeItem->flags() ^ Qt::ItemIsEditable );
                        table->setItem( iRow, I_COL_TYPE, typeItem );

                        auto* nameItem = new QTableWidgetItem( v.name() );
                        nameItem->setFlags( nameItem->flags() ^ Qt::ItemIsEditable );
                        table->setItem( iRow, I_COL_NAME, nameItem );

                        auto* valueItem = new QTableWidgetItem( v.initialValue() );
                        valueItem->setFlags( valueItem->flags() ^ Qt::ItemIsEditable );
                        if ( v.type() == model::Variable::Type::COLOR )
                        {
                                QColor c( v.initialValue() );
                                if ( c.isValid() )
                                {
                                        QPixmap sw( 14, 14 );
                                        sw.fill( Qt::transparent );
                                        QPainter sp( &sw );
                                        sp.setRenderHint( QPainter::Antialiasing, true );
                                        sp.setBrush( c );
                                        sp.setPen( QColor( 0, 0, 0, 80 ) );
                                        sp.drawRoundedRect( QRectF( 0.5, 0.5, 13, 13 ), 3.0, 3.0 );
                                        valueItem->setIcon( QIcon( sw ) );
                                }
                        }
                        table->setItem( iRow, I_COL_VALUE, valueItem );

                        auto* incrementItem = new QTableWidgetItem( model::Variable::incrementToI18nString(v.increment()) );
                        incrementItem->setFlags( incrementItem->flags() ^ Qt::ItemIsEditable );
                        table->setItem( iRow, I_COL_INCREMENT, incrementItem );

                        auto* stepSizeItem = new QTableWidgetItem( v.stepSize() );
                        stepSizeItem->setFlags( stepSizeItem->flags() ^ Qt::ItemIsEditable );
                        table->setItem( iRow, I_COL_STEP_SIZE, stepSizeItem );

                        table->showRow( iRow );
                        iRow++;
                }
        }


        void VariablesView::selectVariable( const QString& name )
        {
                int iRow = 0;
                for( const auto& v : mModel->variables() )
                {
                        if ( v.name() == name )
                        {
                                table->setCurrentCell( iRow, 0,
                                                       (QItemSelectionModel::Select|QItemSelectionModel::Rows) );
                                break;
                        }

                        iRow++;
                }
        }


} // namespace glabels
