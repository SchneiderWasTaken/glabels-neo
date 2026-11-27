//  TestDataInDesigner.cpp
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

#include "TestDataInDesigner.hpp"

#include "AppearanceModel.hpp"
#include "LabelEditor.hpp"
#include "MainWindow.hpp"
#include "ribbon/ContentPanel.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/ColumnMatcher.hpp"
#include "model/Db.hpp"
#include "model/ExternalDataSource.hpp"
#include "model/FieldBinding.hpp"
#include "model/FrameRect.hpp"
#include "model/Model.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelImageObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"
#include "model/Template.hpp"
#include "model/Variable.hpp"
#include "model/XmlLabelCreator.hpp"
#include "model/XmlLabelParser.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDropEvent>
#include <QMimeData>
#include <QPushButton>
#include <QSignalSpy>

#include <memory>

using namespace glabels;
using namespace glabels::model;


namespace
{

        Template makeTemplate()
        {
                Template tmpl( "Test", "DATA-1", "Data test label",
                               "US-Letter", Distance::in( 8.5 ), Distance::in( 11.0 ) );
                FrameRect frame( Distance::pt( 240 ), Distance::pt( 160 ),
                                 Distance::pt( 6 ), Distance::pt( 0 ), Distance::pt( 0 ) );
                frame.addLayout( Layout( 1, 1, Distance::pt( 36 ), Distance::pt( 36 ),
                                         Distance::pt( 240 ), Distance::pt( 160 ) ) );
                tmpl.addFrame( frame );
                return tmpl;
        }


        ExternalDataSource makeSheetSource()
        {
                ExternalDataSource src;
                src.kind           = ExternalSourceKind::GoogleSheets;
                src.spreadsheetId  = QStringLiteral( "1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms" );
                src.googleAuthMode = GoogleAuthMode::PublicLink;
                src.refreshOnOpen = false;       // never reach for the network from a test
                src.refreshBeforePrint = false;
                return src;
        }


        void dropMime( QWidget* target, const QPointF& pos, QMimeData& mime );

        void dropColumn( QWidget* target, const QPointF& pos, const QString& column )
        {
                QMimeData mime;
                mime.setData( ribbon::ContentPanel::columnMimeType(), column.toUtf8() );
                mime.setText( QStringLiteral( "${%1}" ).arg( column ) );
                dropMime( target, pos, mime );
        }


        void dropMime( QWidget* target, const QPointF& pos, QMimeData& mime )
        {
                QDragEnterEvent enter( pos.toPoint(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier );
                QCoreApplication::sendEvent( target, &enter );
                QDragMoveEvent move( pos.toPoint(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier );
                QCoreApplication::sendEvent( target, &move );
                QDropEvent drop( pos, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier );
                QCoreApplication::sendEvent( target, &drop );
                QCoreApplication::processEvents();
        }


        QPointF centerOf( const LabelEditor* editor, const ModelObject* obj )
        {
                return editor->labelToWidget( QPointF( obj->x0().pt() + obj->w().pt() / 2.0,
                                                       obj->y0().pt() + obj->h().pt() / 2.0 ) );
        }

}


void TestDataInDesigner::initTestCase()
{
        QCoreApplication::setOrganizationName( QStringLiteral( "glabels-test-data-in-designer" ) );
        QCoreApplication::setApplicationName( QStringLiteral( "TestDataInDesigner" ) );

        Settings::init();
        Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();

        auto* mime = new QMimeData();
        mime->setText( QString() );
        QApplication::clipboard()->setMimeData( mime );
}


void TestDataInDesigner::matcherPrefersChoiceThenNameThenNormalizedAndOnlySuggestsTheRest()
{
        const QStringList columns = { "SKU", "unit_price", "Product Name", "Qty", "Notes" };
        const QMap<QString,QString> none;

        // Same name
        auto m = ColumnMatcher::match( "SKU", columns, none );
        QCOMPARE( m.kind, ColumnMatch::Kind::Exact );
        QCOMPARE( m.column, QStringLiteral( "SKU" ) );

        // Same name, other case
        m = ColumnMatcher::match( "sku", columns, none );
        QCOMPARE( m.kind, ColumnMatch::Kind::CaseInsensitive );
        QCOMPARE( m.column, QStringLiteral( "SKU" ) );

        // Same name ignoring spaces, underscores and punctuation
        m = ColumnMatcher::match( "Unit price", columns, none );
        QCOMPARE( m.kind, ColumnMatch::Kind::Normalized );
        QCOMPARE( m.column, QStringLiteral( "unit_price" ) );
        QVERIFY( m.isApplied() );

        m = ColumnMatcher::match( "productname", columns, none );
        QCOMPARE( m.kind, ColumnMatch::Kind::Normalized );
        QCOMPARE( m.column, QStringLiteral( "Product Name" ) );

        // A looser resemblance is only a suggestion: it is NOT applied
        m = ColumnMatcher::match( "price", columns, none );
        QCOMPARE( m.kind, ColumnMatch::Kind::Suggested );
        QCOMPARE( m.column, QStringLiteral( "unit_price" ) );
        QVERIFY( !m.isApplied() );

        // Nothing resembles it
        m = ColumnMatcher::match( "weight", columns, none );
        QCOMPARE( m.kind, ColumnMatch::Kind::None );
        QVERIFY( m.column.isEmpty() );

        // Very short names are never suggested (too many accidental hits)
        m = ColumnMatcher::match( "no", columns, none );
        QCOMPARE( m.kind, ColumnMatch::Kind::None );

        // An explicit choice wins over a name match
        QMap<QString,QString> chosen;
        chosen.insert( "SKU", "Notes" );
        m = ColumnMatcher::match( "SKU", columns, chosen );
        QCOMPARE( m.kind, ColumnMatch::Kind::Mapped );
        QCOMPARE( m.column, QStringLiteral( "Notes" ) );

        // Columns that carry the quantity or the print flag are not offered as unused
        const auto matches = ColumnMatcher::matchAll( { "SKU" }, columns, none );
        QCOMPARE( ColumnMatcher::unusedColumns( columns, matches ),
                  QStringList( { "unit_price", "Product Name", "Notes" } ) );
}


void TestDataInDesigner::matcherBlankAndMissingChoice()
{
        const QStringList columns = { "SKU", "Name" };

        // "Leave blank": the field has no column even though one has its name
        QMap<QString,QString> map;
        map.insert( "SKU", QString() );
        auto m = ColumnMatcher::match( "SKU", columns, map );
        QCOMPARE( m.kind, ColumnMatch::Kind::Blank );
        QVERIFY( !m.isApplied() );

        // The chosen column was removed from the sheet: fall back to matching by name
        map.clear();
        map.insert( "Name", "Gone" );
        m = ColumnMatcher::match( "Name", columns, map );
        QCOMPARE( m.kind, ColumnMatch::Kind::Exact );
        QCOMPARE( m.column, QStringLiteral( "Name" ) );

        // ... and when that fails too, it needs a column again
        map.clear();
        map.insert( "Colour", "Gone" );
        m = ColumnMatcher::match( "Colour", columns, map );
        QCOMPARE( m.kind, ColumnMatch::Kind::None );
}


void TestDataInDesigner::columnMapSurvivesSaveAndLoad()
{
        Model model;
        model.setTmplate( makeTemplate() );

        ExternalDataSource src = makeSheetSource();
        src.columnMap.insert( QStringLiteral( "price" ),  QStringLiteral( "Unit price" ) );
        src.columnMap.insert( QStringLiteral( "note" ),   QString() );   // left blank on purpose
        src.columnMap.insert( QStringLiteral( "a&b <c>" ), QStringLiteral( "Quote \"x\"" ) );
        model.setExternalDataSource( src );

        QByteArray xml;
        XmlLabelCreator::writeBuffer( &model, xml );
        QVERIFY( xml.contains( "<Column" ) );

        std::unique_ptr<Model> parsed( XmlLabelParser::readBuffer( xml ) );
        QVERIFY( parsed != nullptr );
        QCOMPARE( parsed->externalDataSource().columnMap, src.columnMap );
        QCOMPARE( parsed->externalDataSource(), src );

        // A file without any <Column> element still loads (files from before this feature)
        ExternalDataSource plain = makeSheetSource();
        model.setExternalDataSource( plain );
        xml.clear();
        XmlLabelCreator::writeBuffer( &model, xml );
        QVERIFY( !xml.contains( "<Column" ) );
        std::unique_ptr<Model> parsedPlain( XmlLabelParser::readBuffer( xml ) );
        QVERIFY( parsedPlain != nullptr );
        QVERIFY( parsedPlain->externalDataSource().columnMap.isEmpty() );
}


void TestDataInDesigner::fieldBindingsKnowComputedConstantAndUnusedFields()
{
        Model model;
        model.setTmplate( makeTemplate() );

        auto* text = new ModelTextObject();
        text->setText( QStringLiteral( "${sku} ${price} ${n} ${site} ${note:=none}" ) );
        model.addObject( text );

        // A counter (computed), a constant, and a blank variable nothing on the label uses yet
        model.variables().addVariable( Variable( Variable::Type::INTEGER, "n", "1", Variable::Increment::PER_ITEM ) );
        model.variables().addVariable( Variable( Variable::Type::STRING, "site", "Dock 4" ) );
        model.variables().addVariable( Variable( Variable::Type::STRING, "spare", QString() ) );

        const QStringList columns = { "SKU", "Unit Price" };
        const auto bindings = FieldBindings::compute( model, columns );

        auto find = [&]( const QString& name ) -> FieldBinding {
                for ( const auto& b : bindings ) { if ( b.field == name ) { return b; } }
                return FieldBinding();
        };

        QVERIFY( find( "sku" ).match.isApplied() );
        QVERIFY( !find( "sku" ).needsColumn() );

        // "Unit Price" is not "price" (normalised "unitprice" != "price"): only a suggestion
        QVERIFY( !find( "price" ).match.isApplied() );
        QCOMPARE( find( "price" ).match.kind, ColumnMatch::Kind::Suggested );
        QVERIFY( find( "price" ).needsColumn() );

        QVERIFY( find( "n" ).isComputed );
        QVERIFY( !find( "n" ).needsColumn() );

        QVERIFY( find( "site" ).isConstant );
        QVERIFY( !find( "site" ).needsColumn() );

        QVERIFY( find( "note" ).isConstant );                    // has a :=default
        QVERIFY( !find( "note" ).needsColumn() );

        QVERIFY( find( "spare" ).needsColumn() );                // blank variable, nothing fills it
        QVERIFY( !find( "spare" ).isUsed );

        QStringList needs;
        for ( const auto& b : FieldBindings::needingColumn( bindings ) ) { needs.append( b.field ); }
        needs.sort();
        QCOMPARE( needs, QStringList( { "price", "spare" } ) );
}


void TestDataInDesigner::fieldsComeFromBarcodeAndImageToo()
{
        Model model;
        model.setTmplate( makeTemplate() );

        auto* barcode = new ModelBarcodeObject();
        barcode->setBcData( QStringLiteral( "${code}" ) );
        model.addObject( barcode );

        auto* image = new ModelImageObject();
        image->setFilenameNode( TextNode( true, QStringLiteral( "photo" ) ) );
        model.addObject( image );

        const QStringList names = FieldBindings::fieldNames( model );
        QVERIFY( names.contains( QStringLiteral( "code" ) ) );
        QVERIFY( names.contains( QStringLiteral( "photo" ) ) );
}


void TestDataInDesigner::dropOnObjectsBindsThemAndDropOnEmptyLabelCreatesText()
{
        MainWindow win;
        win.resize( 1440, 900 );

        auto* mdl = new Model();
        mdl->setTmplate( makeTemplate() );

        auto* text = new ModelTextObject();
        text->setText( QStringLiteral( "Price:" ) );
        text->setPosition( Distance::pt( 20 ), Distance::pt( 20 ) );
        text->setSize( Distance::pt( 90 ), Distance::pt( 24 ) );
        mdl->addObject( text );

        auto* barcode = new ModelBarcodeObject();
        barcode->setPosition( Distance::pt( 20 ), Distance::pt( 60 ) );
        barcode->setSize( Distance::pt( 120 ), Distance::pt( 50 ) );
        mdl->addObject( barcode );

        auto* image = new ModelImageObject();
        image->setPosition( Distance::pt( 150 ), Distance::pt( 20 ) );
        image->setSize( Distance::pt( 60 ), Distance::pt( 60 ) );
        mdl->addObject( image );

        win.setModel( mdl );
        win.show();
        QCoreApplication::processEvents();

        LabelEditor* editor = win.labelEditor();
        QVERIFY( editor );

        // Onto a text object: the field is added after the existing text
        dropColumn( editor, centerOf( editor, text ), QStringLiteral( "Unit price" ) );
        QCOMPARE( text->text(), QStringLiteral( "Price: ${Unit price}" ) );

        // Dropping the same column again does not repeat it
        dropColumn( editor, centerOf( editor, text ), QStringLiteral( "Unit price" ) );
        QCOMPARE( text->text(), QStringLiteral( "Price: ${Unit price}" ) );

        // Onto a barcode: its data becomes the field
        dropColumn( editor, centerOf( editor, barcode ), QStringLiteral( "SKU" ) );
        QCOMPARE( barcode->bcData(), QStringLiteral( "${SKU}" ) );

        // Onto an image: its file name becomes the field
        dropColumn( editor, centerOf( editor, image ), QStringLiteral( "Photo" ) );
        QVERIFY( image->filenameNode().isField() );
        QCOMPARE( image->filenameNode().data(), QStringLiteral( "Photo" ) );

        // Onto empty label: a new text object holding the field, selected
        const int before = mdl->objectList().size();
        dropColumn( editor, editor->labelToWidget( QPointF( 180, 130 ) ), QStringLiteral( "Notes" ) );
        QCOMPARE( mdl->objectList().size(), before + 1 );
        auto* created = dynamic_cast<ModelTextObject*>( mdl->objectList().last() );
        QVERIFY( created != nullptr );
        QCOMPARE( created->text(), QStringLiteral( "${Notes}" ) );
        QVERIFY( created->isSelected() );

        // A plain ${field} chip from the Fields list behaves the same way
        {
                QMimeData mime;
                mime.setText( QStringLiteral( "${SKU}" ) );
                dropMime( editor, centerOf( editor, text ), mime );
                QCOMPARE( text->text(), QStringLiteral( "Price: ${Unit price} ${SKU}" ) );
        }

        // Ordinary text still creates a plain text object, as before
        {
                QMimeData mime;
                mime.setText( QStringLiteral( "hello" ) );
                const int n = mdl->objectList().size();
                dropMime( editor, editor->labelToWidget( QPointF( 100, 140 ) ), mime );
                QCOMPARE( mdl->objectList().size(), n + 1 );
        }
}


void TestDataInDesigner::dropIsOneUndoStep()
{
        MainWindow win;
        win.resize( 1440, 900 );

        auto* mdl = new Model();
        mdl->setTmplate( makeTemplate() );
        auto* text = new ModelTextObject();
        text->setText( QStringLiteral( "A" ) );
        text->setPosition( Distance::pt( 20 ), Distance::pt( 20 ) );
        text->setSize( Distance::pt( 90 ), Distance::pt( 24 ) );
        mdl->addObject( text );

        win.setModel( mdl );
        win.show();
        QCoreApplication::processEvents();

        LabelEditor* editor = win.labelEditor();
        dropColumn( editor, centerOf( editor, text ), QStringLiteral( "SKU" ) );
        QCOMPARE( mdl->objectList().size(), 1 );

        // Undo restores the model from a snapshot, so find the object again
        QAction* undo = nullptr;
        for ( auto* a : win.findChildren<QAction*>() )
        {
                if ( a->shortcut() == QKeySequence( QKeySequence::Undo ) ) { undo = a; break; }
        }
        QVERIFY( undo != nullptr );
        QVERIFY( undo->isEnabled() );
        undo->trigger();
        QCoreApplication::processEvents();

        QCOMPARE( mdl->objectList().size(), 1 );
        auto* restored = dynamic_cast<ModelTextObject*>( mdl->objectList().first() );
        QVERIFY( restored != nullptr );
        QCOMPARE( restored->text(), QStringLiteral( "A" ) );
}


void TestDataInDesigner::panelListsColumnsAndOffersChoicesForFieldsThatNeedOne()
{
        MainWindow win;
        win.resize( 1440, 900 );

        auto* mdl = new Model();
        mdl->setTmplate( makeTemplate() );
        auto* text = new ModelTextObject();
        text->setText( QStringLiteral( "${SKU} ${price}" ) );
        mdl->addObject( text );
        mdl->setExternalDataSource( makeSheetSource() );

        win.setModel( mdl );
        win.show();
        QCoreApplication::processEvents();

        auto* panel = win.contentPanel();
        QVERIFY( panel );

        QList<QMap<QString,QString>> rows;
        rows.append( { { "SKU", "A-1" }, { "Unit price", "9.99" }, { "Qty", "2" } } );
        rows.append( { { "SKU", "B-2" }, { "Unit price", "4.50" }, { "Qty", "1" } } );
        mdl->setSourceData( { "SKU", "Unit price", "Qty" }, rows );
        mdl->setFillRows( { { { "SKU", "A-1" }, { "price", "" } }, { { "SKU", "B-2" }, { "price", "" } } } );

        // One row per column, each readable by a screen reader
        auto hasRowFor = [&]( const QString& column )
        {
                for ( auto* w : panel->findChildren<QWidget*>() )
                {
                        if ( w->isVisibleTo( panel ) && w->accessibleName().startsWith( QStringLiteral( "Column %1," ).arg( column ) ) )
                        {
                                return true;
                        }
                }
                return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT( hasRowFor( "SKU" ), 3000 );
        QVERIFY( hasRowFor( "Unit price" ) );
        QVERIFY( hasRowFor( "Qty" ) );

        // ${price} has no column of that name: the panel asks, and suggests "Unit price"
        QComboBox* combo = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT( ( combo = [&]() -> QComboBox* {
                for ( auto* c : panel->findChildren<QComboBox*>() )
                {
                        if ( c->accessibleName() == QStringLiteral( "Column for price" ) ) { return c; }
                }
                return nullptr; }() ) != nullptr, 3000 );

        bool suggestionShown = false;
        for ( auto* b : panel->findChildren<QPushButton*>() )
        {
                if ( b->isVisibleTo( panel ) && b->text().contains( QStringLiteral( "Unit price" ) ) ) { suggestionShown = true; }
        }
        QVERIFY( suggestionShown );

        // ${SKU} matched by name, so it is not asked about
        for ( auto* c : panel->findChildren<QComboBox*>() )
        {
                QVERIFY( c->accessibleName() != QStringLiteral( "Column for SKU" ) );
        }

        const QString shotDir = qEnvironmentVariable( "GLABELS_THEME_SCREENSHOTS" );
        if ( !shotDir.isEmpty() )
        {
                QDir().mkpath( shotDir );
                win.grab().save( shotDir + QStringLiteral( "/data-panel.png" ) );
        }

        // Choosing a column is reported as (field, column)
        QSignalSpy chosen( panel, &ribbon::ContentPanel::columnChosen );
        const int idx = combo->findData( QStringLiteral( "Unit price" ) );
        QVERIFY( idx > 0 );
        combo->setCurrentIndex( idx );
        QMetaObject::invokeMethod( combo, "activated", Q_ARG( int, idx ) );
        QCOMPARE( chosen.count(), 1 );
        QCOMPARE( chosen.at( 0 ).at( 0 ).toString(), QStringLiteral( "price" ) );
        QCOMPARE( chosen.at( 0 ).at( 1 ).toString(), QStringLiteral( "Unit price" ) );

        // The window records it in the model as an undoable step, and the question goes away
        QTRY_COMPARE_WITH_TIMEOUT( mdl->externalDataSource().columnMap.value( "price" ), QStringLiteral( "Unit price" ), 3000 );
        QTRY_VERIFY_WITH_TIMEOUT( [&]() {
                for ( auto* c : panel->findChildren<QComboBox*>() )
                {
                        if ( c->isVisibleTo( panel ) && c->accessibleName() == QStringLiteral( "Column for price" ) ) { return false; }
                }
                return true; }(), 3000 );
}


void TestDataInDesigner::panelEmptyStateOffersToConnect()
{
        // A panel on its own: in the main window the button opens the (modal) connection dialog.
        Model model;
        model.setTmplate( makeTemplate() );

        ribbon::ContentPanel standalone;
        standalone.setModel( &model );
        standalone.resize( 300, 700 );
        standalone.show();
        QCoreApplication::processEvents();
        auto* panel = &standalone;

        QSignalSpy connectSpy( panel, &ribbon::ContentPanel::connectSourceRequested );
        QPushButton* sheets = nullptr;
        for ( auto* b : panel->findChildren<QPushButton*>() )
        {
                if ( b->isVisibleTo( panel ) && b->text() == QStringLiteral( "Google Sheets..." ) ) { sheets = b; }
        }
        QVERIFY( sheets != nullptr );
        sheets->click();
        QCOMPARE( connectSpy.count(), 1 );
        QCOMPARE( connectSpy.at( 0 ).at( 0 ).value<ExternalSourceKind>(), ExternalSourceKind::GoogleSheets );
}


void TestDataInDesigner::showRowDataChangesWhatTheEditorDraws()
{
        MainWindow win;
        win.resize( 1440, 900 );

        auto* mdl = new Model();
        mdl->setTmplate( makeTemplate() );
        auto* text = new ModelTextObject();
        text->setText( QStringLiteral( "${sku}" ) );
        text->setPosition( Distance::pt( 20 ), Distance::pt( 20 ) );
        text->setSize( Distance::pt( 180 ), Distance::pt( 40 ) );
        mdl->addObject( text );

        win.setModel( mdl );
        win.show();
        QCoreApplication::processEvents();

        LabelEditor* editor = win.labelEditor();
        QVERIFY( editor );

        QList<QMap<QString,QString>> rows;
        rows.append( { { "sku", "ROW-ONE" } } );
        rows.append( { { "sku", "ROW-TWO-LONGER" } } );
        mdl->setFillRows( rows );

        const QImage placeholders = editor->grab().toImage();

        editor->setShowRowData( true );
        QCoreApplication::processEvents();
        const QImage rowOne = editor->grab().toImage();
        QVERIFY( rowOne != placeholders );

        editor->setPreviewRow( 1 );
        QCoreApplication::processEvents();
        const QImage rowTwo = editor->grab().toImage();
        QVERIFY( rowTwo != rowOne );

        editor->setShowRowData( false );
        QCoreApplication::processEvents();
        QCOMPARE( editor->grab().toImage(), placeholders );
}


QTEST_MAIN( TestDataInDesigner )
