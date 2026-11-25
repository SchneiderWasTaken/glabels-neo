//  TestSnap.cpp
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


#include "TestSnap.hpp"

#include "AppearanceModel.hpp"
#include "LabelEditor.hpp"
#include "MainWindow.hpp"
#include "SnapEngine.hpp"
#include "UndoRedoModel.hpp"
#include "ribbon/RibbonBar.hpp"
#include "ribbon/RibbonButton.hpp"
#include "ribbon/RibbonGroup.hpp"
#include "ribbon/RibbonTab.hpp"
#include "theme/Theme.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/FrameRect.hpp"
#include "model/Markup.hpp"
#include "model/Model.hpp"
#include "model/ModelBarcodeObject.hpp"
#include "model/ModelBoxObject.hpp"
#include "model/ModelEllipseObject.hpp"
#include "model/ModelTextObject.hpp"
#include "model/Settings.hpp"
#include "model/Template.hpp"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollArea>

#include <cmath>


using namespace glabels;


namespace
{
        SnapContext makeBaseContext()
        {
                SnapContext ctx;
                ctx.labelSize = QSizeF( 200.0, 100.0 );
                ctx.safeInset = { 10.0, 10.0 };
                ctx.tolerance = 5.0;
                ctx.options   = { true, true, true };
                return ctx;
        }

        void sendMouse( QWidget*              widget,
                        QEvent::Type          type,
                        const QPointF&        localPos,
                        Qt::MouseButton       button,
                        Qt::MouseButtons      buttons,
                        Qt::KeyboardModifiers mods = Qt::NoModifier )
        {
                QMouseEvent ev( type, localPos, widget->mapToGlobal( localPos ), button, buttons, mods );
                QCoreApplication::sendEvent( widget, &ev );
        }

        model::Template makeTestTemplate( double wPt = 243.0, double hPt = 168.0, double marginPt = 9.0 )
        {
                model::Template tmpl( "Test", "SNAP-1", "Snap test label",
                                      "US-Letter", model::Distance::in( 8.5 ), model::Distance::in( 11.0 ) );
                model::FrameRect frame( model::Distance::pt( wPt ),
                                        model::Distance::pt( hPt ),
                                        model::Distance::pt( 6.0 ),
                                        model::Distance::pt( 0.0 ),
                                        model::Distance::pt( 0.0 ) );
                if ( marginPt > 0.0 )
                {
                        frame.addMarkup( model::MarkupMargin( model::Distance::pt( marginPt ) ) );
                }
                frame.addLayout( model::Layout( 1, 1,
                                                model::Distance::pt( 36 ),
                                                model::Distance::pt( 36 ),
                                                model::Distance::pt( wPt ),
                                                model::Distance::pt( hPt ) ) );
                tmpl.addFrame( frame );
                return tmpl;
        }
} // namespace


void TestSnap::initTestCase()
{
        QCoreApplication::setOrganizationName( QStringLiteral( "glabels-test-snap" ) );
        QCoreApplication::setApplicationName( QStringLiteral( "TestSnap" ) );

        model::Settings::init();
        model::Settings::setSnapObjects( true );
        model::Settings::setSnapCenter( true );
        model::Settings::setSnapSafeArea( true );
        model::Db::init();
        merge::Factory::init();
        barcode::Backends::init();
        AppearanceModel::init();

        // Seed clipboard so offscreen Model::canPaste() never sees null mimeData
        auto* mime = new QMimeData();
        mime->setText( QString() );
        QApplication::clipboard()->setMimeData( mime );
}


void TestSnap::moveSnapsToObjectsCenterAndSafeArea()
{
        SnapContext ctx = makeBaseContext();
        ctx.targets.push_back( { QRectF( 30.0, 20.0, 40.0, 20.0 ), QStringLiteral( "box" ) } );

        // 1. Snap left edge to another object's right edge (70.0) and top to object's bottom (40.0)
        // Height 16 -> top=38 (dist 2 from 40), centre=46 (dist 4 from 50), bottom=54 (dist 4 from 50)
        {
                const SnapResult res = SnapEngine::snapMove( QRectF( 73.0, 38.0, 20.0, 16.0 ), ctx );
                QVERIFY( res.snappedX );
                QVERIFY( res.snappedY );
                QCOMPARE( res.dx, -3.0 );
                QCOMPARE( res.dy,  2.0 );
                QCOMPARE( res.guides.size(), 2 );
                QCOMPARE( res.guides[0].orientation, SnapGuideOrientation::Vertical );
                QCOMPARE( res.guides[0].position, 70.0 );
                QCOMPARE( res.guides[0].kind, SnapGuideKind::ObjectEdge );
                QCOMPARE( res.guides[0].description, QStringLiteral( "Edge of box" ) );
                QCOMPARE( res.guides[1].orientation, SnapGuideOrientation::Horizontal );
                QCOMPARE( res.guides[1].position, 40.0 );
                QCOMPARE( res.guides[1].kind, SnapGuideKind::ObjectEdge );
        }

        // 2. Snap moving centre to label centre (100.0, 50.0)
        {
                const SnapResult res = SnapEngine::snapMove( QRectF( 87.0, 42.0, 20.0, 20.0 ), ctx );
                // centre is (97, 52) -> delta (+3, -2)
                QVERIFY( res.snappedX );
                QVERIFY( res.snappedY );
                QCOMPARE( res.dx,  3.0 );
                QCOMPARE( res.dy, -2.0 );
                QCOMPARE( res.guides.size(), 2 );
                QCOMPARE( res.guides[0].kind, SnapGuideKind::Center );
                QCOMPARE( res.guides[0].description, QStringLiteral( "Centre" ) );
                QCOMPARE( res.guides[1].kind, SnapGuideKind::Center );
                QCOMPARE( res.guides[1].description, QStringLiteral( "Centre" ) );
        }

        // 3. Snap to safe area (10.0 left, 90.0 bottom) and verify outer edge (0.0, 200.0) is NOT a target
        {
                const SnapResult res = SnapEngine::snapMove( QRectF( 12.5, 67.0, 20.0, 20.0 ), ctx );
                // left 12.5 -> 10.0 (-2.5), bottom 87.0 -> 90.0 (+3.0)
                QVERIFY( res.snappedX );
                QVERIFY( res.snappedY );
                QVERIFY( res.hasSafeAreaSnap() );
                QCOMPARE( res.dx, -2.5 );
                QCOMPARE( res.dy,  3.0 );
                QCOMPARE( res.guides[0].kind, SnapGuideKind::SafeArea );
                QCOMPARE( res.guides[0].description, QStringLiteral( "Safe area" ) );
                QCOMPARE( res.guides[1].kind, SnapGuideKind::SafeArea );
        }
        {
                // Near outer edge x=1.0, y=1.0, size 2x2 (right=3, dist 7 > tol=5 from margin 10; dist 1 from outer edge 0)
                const SnapResult res = SnapEngine::snapMove( QRectF( 1.0, 1.0, 2.0, 2.0 ), ctx );
                QVERIFY( !res.snappedX );
                QVERIFY( !res.snappedY );
                QVERIFY( res.guides.isEmpty() );
        }
}


void TestSnap::resizeSnapsOnlyDraggedEdgesByEachHandle()
{
        SnapContext ctx = makeBaseContext();
        // Label 200x100, safe area at 10/190 (X) and 10/90 (Y), centre at 100/50.
        // Add target box at x=[40..60], y=[20..40].
        ctx.targets.push_back( { QRectF( 40.0, 20.0, 20.0, 20.0 ), QStringLiteral( "text" ) } );

        // Verify centre line is NEVER a source during resize:
        // Rect [80..124] has centre 102 (within 5pt of label centre 100), right=124 (far from targets), left=80 (far).
        {
                const SnapResult res = SnapEngine::snapResize(
                        QRectF( 80.0, 55.0, 44.0, 15.0 ), SnapEdge::Right, ctx );
                QVERIFY( !res.snappedX );
                QVERIFY( !res.snappedY );
        }

        // Handle W (Left) snaps to safe area 10.0; Right (192) is ignored because not dragged
        {
                const SnapResult res = SnapEngine::snapResize(
                        QRectF( 12.0, 55.0, 180.0, 15.0 ), SnapEdge::Left, ctx );
                QVERIFY( res.snappedX );
                QVERIFY( !res.snappedY );
                QCOMPARE( res.dx, -2.0 );
        }

        // Handle E (Right) snaps to safe area 190.0; Left (12) is ignored
        {
                const SnapResult res = SnapEngine::snapResize(
                        QRectF( 12.0, 55.0, 176.0, 15.0 ), SnapEdge::Right, ctx );
                QVERIFY( res.snappedX );
                QVERIFY( !res.snappedY );
                QCOMPARE( res.dx, 2.0 );
        }

        // Handle N (Top) snaps to object bottom 40.0
        {
                const SnapResult res = SnapEngine::snapResize(
                        QRectF( 120.0, 43.0, 30.0, 20.0 ), SnapEdge::Top, ctx );
                QVERIFY( !res.snappedX );
                QVERIFY( res.snappedY );
                QCOMPARE( res.dy, -3.0 );
        }

        // Handle S (Bottom) snaps to safe area 90.0
        {
                const SnapResult res = SnapEngine::snapResize(
                        QRectF( 120.0, 60.0, 30.0, 28.0 ), SnapEdge::Bottom, ctx );
                QVERIFY( !res.snappedX );
                QVERIFY( res.snappedY );
                QCOMPARE( res.dy, 2.0 );
        }

        // Corner handles: NW, NE, SW, SE snap both dragged axes independently
        {
                const SnapResult nw = SnapEngine::snapResize(
                        QRectF( 13.0, 12.0, 30.0, 30.0 ), SnapEdge::Left | SnapEdge::Top, ctx );
                QVERIFY( nw.snappedX && nw.snappedY );
                QCOMPARE( nw.dx, -3.0 );
                QCOMPARE( nw.dy, -2.0 );

                const SnapResult ne = SnapEngine::snapResize(
                        QRectF( 140.0, 12.0, 48.0, 30.0 ), SnapEdge::Right | SnapEdge::Top, ctx );
                QVERIFY( ne.snappedX && ne.snappedY );
                QCOMPARE( ne.dx,  2.0 );
                QCOMPARE( ne.dy, -2.0 );

                const SnapResult sw = SnapEngine::snapResize(
                        QRectF( 13.0, 60.0, 30.0, 28.0 ), SnapEdge::Left | SnapEdge::Bottom, ctx );
                QVERIFY( sw.snappedX && sw.snappedY );
                QCOMPARE( sw.dx, -3.0 );
                QCOMPARE( sw.dy,  2.0 );

                const SnapResult se = SnapEngine::snapResize(
                        QRectF( 140.0, 60.0, 48.0, 28.0 ), SnapEdge::Right | SnapEdge::Bottom, ctx );
                QVERIFY( se.snappedX && se.snappedY );
                QCOMPARE( se.dx, 2.0 );
                QCOMPARE( se.dy, 2.0 );
        }
}


void TestSnap::createDragAndClickCreate()
{
        SnapContext ctx = makeBaseContext();
        ctx.targets.push_back( { QRectF( 30.0, 20.0, 30.0, 20.0 ), QStringLiteral( "barcode" ) } );

        // Drag-create corner under cursor snaps to object right edge (60) and label centre Y (50)
        {
                const SnapResult res = SnapEngine::snapPoint( QPointF( 58.0, 52.0 ), ctx );
                QVERIFY( res.snappedX && res.snappedY );
                QCOMPARE( res.dx,  2.0 );
                QCOMPARE( res.dy, -2.0 );
                QCOMPARE( res.guides[0].description, QStringLiteral( "Edge of barcode" ) );
                QCOMPARE( res.guides[1].description, QStringLiteral( "Centre" ) );
        }

        // Click-create default size (72x36) snaps its centre to label centre (100, 50)
        {
                // Place at (66, 30) -> centre is (102, 48) -> delta (-2, +2)
                const SnapResult res = SnapEngine::snapCenter( QRectF( 66.0, 30.0, 72.0, 36.0 ), ctx );
                QVERIFY( res.snappedX && res.snappedY );
                QCOMPARE( res.dx, -2.0 );
                QCOMPARE( res.dy,  2.0 );
        }
}


void TestSnap::toleranceEdgesAndScale()
{
        QCOMPARE( SnapEngine::toleranceForScale( 1.0 ), 6.0 );
        QCOMPARE( SnapEngine::toleranceForScale( 2.0 ), 3.0 );
        QCOMPARE( SnapEngine::toleranceForScale( 0.5 ), 12.0 );
        QCOMPARE( SnapEngine::toleranceForScale( 0.0 ), 0.0 );

        SnapContext ctx = makeBaseContext();
        ctx.tolerance = 6.0;

        // Exact tolerance boundary (distance == 6.0 from centre x=100) -> snaps
        {
                const SnapResult atEdge = SnapEngine::snapPoint( QPointF( 94.0, 25.0 ), ctx );
                QVERIFY( atEdge.snappedX );
                QCOMPARE( atEdge.dx, 6.0 );
        }

        // Just outside tolerance (distance == 6.01) -> does NOT snap
        {
                const SnapResult outside = SnapEngine::snapPoint( QPointF( 93.99, 25.0 ), ctx );
                QVERIFY( !outside.snappedX );
                QCOMPARE( outside.dx, 0.0 );
        }
}


void TestSnap::tieBreakingPriority()
{
        SnapContext ctx;
        ctx.labelSize = QSizeF( 200.0, 100.0 );
        ctx.safeInset = { 100.0, 10.0 }; // Safe area X coincides with Centre X at 100.0
        ctx.tolerance = 5.0;
        ctx.options   = { true, true, true };
        ctx.targets.push_back( { QRectF( 100.0, 20.0, 20.0, 20.0 ), QStringLiteral( "box" ) } );

        // 1. Exact tie between Centre, Safe area, and Object edge at x=100 -> Centre wins
        {
                const SnapResult res = SnapEngine::snapPoint( QPointF( 98.0, 25.0 ), ctx );
                QVERIFY( res.snappedX );
                QCOMPARE( res.guides[0].kind, SnapGuideKind::Center );
                QCOMPARE( res.guides[0].description, QStringLiteral( "Centre" ) );
        }

        // 2. Disable Centre: tie between Safe area and Object edge at x=100 -> Safe area wins
        {
                ctx.options.center = false;
                const SnapResult res = SnapEngine::snapPoint( QPointF( 98.0, 25.0 ), ctx );
                QVERIFY( res.snappedX );
                QCOMPARE( res.guides[0].kind, SnapGuideKind::SafeArea );
                QCOMPARE( res.guides[0].description, QStringLiteral( "Safe area" ) );
        }

        // 3. Nearer lower-priority target beats farther higher-priority target
        {
                ctx.options.center = true;
                ctx.safeInset = { 10.0, 10.0 };
                ctx.targets.clear();
                // Object edge at 97.0 (dist 1.0) vs Centre at 100.0 (dist 2.0) from x=98.0
                ctx.targets.push_back( { QRectF( 77.0, 20.0, 20.0, 20.0 ), QStringLiteral( "ellipse" ) } );
                const SnapResult res = SnapEngine::snapPoint( QPointF( 98.0, 25.0 ), ctx );
                QVERIFY( res.snappedX );
                QCOMPARE( res.dx, -1.0 );
                QCOMPARE( res.guides[0].kind, SnapGuideKind::ObjectEdge );
                QCOMPARE( res.guides[0].description, QStringLiteral( "Edge of ellipse" ) );
        }
}


void TestSnap::disabledToggles()
{
        SnapContext ctx = makeBaseContext();
        ctx.targets.push_back( { QRectF( 40.0, 20.0, 20.0, 20.0 ), QStringLiteral( "box" ) } );

        // Disable objects: x=61 (near object right=60) does not snap
        ctx.options = { false, true, true };
        QVERIFY( !SnapEngine::snapPoint( QPointF( 61.0, 75.0 ), ctx ).snappedX );
        // Centre still snaps
        QVERIFY( SnapEngine::snapPoint( QPointF( 99.0, 75.0 ), ctx ).snappedX );

        // Disable centre: x=99 does not snap
        ctx.options = { true, false, true };
        QVERIFY( !SnapEngine::snapPoint( QPointF( 99.0, 75.0 ), ctx ).snappedX );
        // Safe area still snaps
        QVERIFY( SnapEngine::snapPoint( QPointF( 11.0, 75.0 ), ctx ).snappedX );

        // Disable safe area: x=11 does not snap
        ctx.options = { true, true, false };
        QVERIFY( !SnapEngine::snapPoint( QPointF( 11.0, 75.0 ), ctx ).snappedX );

        // Disable all: nothing snaps
        ctx.options = { false, false, false };
        QVERIFY( !SnapEngine::snapPoint( QPointF( 100.0, 50.0 ), ctx ).snappedX );
        QVERIFY( !SnapEngine::snapPoint( QPointF( 100.0, 50.0 ), ctx ).snappedY );
}


void TestSnap::multiSelectionBox()
{
        SnapContext ctx = makeBaseContext();
        // Two selected objects combined: [22..42] and [52..72] -> union box [22..72], width 50, centre 47.
        // Translate by +51 -> union box [73..123], centre = 98 (within 5pt of label centre 100).
        const QRectF a( 22.0, 15.0, 20.0, 10.0 );
        const QRectF b( 52.0, 35.0, 20.0, 10.0 );
        const QRectF selectionBox = a.united( b ).translated( 51.0, 22.0 );
        // selectionBox = [73..123] x [37..67], centre = (98, 52) -> snaps to (100, 50) -> (+2, -2)
        const SnapResult res = SnapEngine::snapMove( selectionBox, ctx );
        QVERIFY( res.snappedX && res.snappedY );
        QCOMPARE( res.dx,  2.0 );
        QCOMPARE( res.dy, -2.0 );
}


void TestSnap::zeroMargin()
{
        SnapContext ctx;
        ctx.labelSize = QSizeF( 200.0, 100.0 );
        ctx.safeInset = { 0.0, 0.0 };
        ctx.tolerance = 5.0;
        ctx.options   = { false, false, true };

        // With 0 margin and Safe area ON, safe-area edges coincide with 0 and label W/H
        const SnapResult res = SnapEngine::snapMove( QRectF( 3.0, 78.0, 20.0, 20.0 ), ctx );
        QVERIFY( res.snappedX && res.snappedY );
        QCOMPARE( res.dx, -3.0 ); // left -> 0
        QCOMPARE( res.dy,  2.0 ); // bottom 98 -> 100
        QVERIFY( res.hasSafeAreaSnap() );

        // With Safe area OFF, outer edges (0, 200) are NOT targets
        ctx.options.safeArea = false;
        const SnapResult off = SnapEngine::snapMove( QRectF( 3.0, 78.0, 20.0, 20.0 ), ctx );
        QVERIFY( !off.snappedX && !off.snappedY );
}


void TestSnap::rotatedAndLineBounds()
{
        // 90-degree rotation of 40x20 rect around origin (50, 50)
        QTransform rot90;
        rot90.rotate( 90.0 );
        const QRectF r90 = SnapEngine::rotatedBoundingBox( QRectF( 0.0, 0.0, 40.0, 20.0 ), rot90, QPointF( 50.0, 50.0 ) );
        // Corners (0,0)->(0,0), (40,0)->(0,40), (40,20)->(-20,40), (0,20)->(-20,0) + (50,50) => [30..50] x [50..90]
        QVERIFY( std::abs( r90.left()   - 30.0 ) < 1e-6 );
        QVERIFY( std::abs( r90.right()  - 50.0 ) < 1e-6 );
        QVERIFY( std::abs( r90.top()    - 50.0 ) < 1e-6 );
        QVERIFY( std::abs( r90.bottom() - 90.0 ) < 1e-6 );

        // 45-degree rotation of 20x20 square around (100, 50)
        QTransform rot45;
        rot45.rotate( 45.0 );
        const QRectF r45 = SnapEngine::rotatedBoundingBox( QRectF( 0.0, 0.0, 20.0, 20.0 ), rot45, QPointF( 100.0, 50.0 ) );
        const double diag = 20.0 * std::sqrt( 2.0 );
        QVERIFY( std::abs( r45.width()  - diag ) < 1e-6 );
        QVERIFY( std::abs( r45.height() - diag ) < 1e-6 );

        // Diagonal line from (10, 20) by (+30, -10) -> bbox [10..40] x [10..20]
        const QRectF lb = SnapEngine::lineBoundingBox( QPointF( 0.0, 0.0 ), QPointF( 30.0, -10.0 ),
                                                       QTransform(), QPointF( 10.0, 20.0 ) );
        QCOMPARE( lb.left(),   10.0 );
        QCOMPARE( lb.right(),  40.0 );
        QCOMPARE( lb.top(),    10.0 );
        QCOMPARE( lb.bottom(), 20.0 );

        // Target using rotated bounds snaps accurately
        SnapContext ctx = makeBaseContext();
        ctx.targets.push_back( { r90, QStringLiteral( "box" ) } );
        const SnapResult res = SnapEngine::snapMove( QRectF( 52.0, 15.0, 20.0, 10.0 ), ctx );
        QVERIFY( res.snappedX );
        QCOMPARE( res.dx, -2.0 ); // left 52 -> rotated right edge 50
}


void TestSnap::editorMoveResizeCreateAndAltBypass()
{
        model::Settings::setSnapObjects( true );
        model::Settings::setSnapCenter( true );
        model::Settings::setSnapSafeArea( true );
        model::Settings::setColorScheme( model::Settings::LIGHT_COLOR_SCHEME );
        model::Settings::setThemeScheme( QStringLiteral( "neo" ) );

        MainWindow win;
        win.resize( 1440, 900 );
        auto* mdl = new model::Model();
        mdl->setTmplate( makeTestTemplate( 240.0, 160.0, 12.0 ) );
        // Label: 240 x 160 pt; centre = (120, 80); safe area = [12..228] x [12..148]

        // Fixed target box at (24, 24), size 60 x 36 -> right = 84, bottom = 60, centre = (54, 42)
        auto* anchorBox = new model::ModelBoxObject(
                model::Distance::pt( 24.0 ), model::Distance::pt( 24.0 ),
                model::Distance::pt( 60.0 ), model::Distance::pt( 36.0 ),
                false, model::Distance::pt( 1.5 ),
                model::ColorNode( QColor( 0x4F, 0x46, 0xE5 ) ),
                model::ColorNode( QColor( 0xED, 0xE9, 0xFE ) ) );
        mdl->addObject( anchorBox );

        // Movable box starting at (140, 100), size 40 x 24
        auto* mover = new model::ModelBoxObject(
                model::Distance::pt( 140.0 ), model::Distance::pt( 100.0 ),
                model::Distance::pt( 40.0 ),  model::Distance::pt( 24.0 ),
                false, model::Distance::pt( 1.5 ),
                model::ColorNode( QColor( 0x0F, 0x17, 0x2A ) ),
                model::ColorNode( QColor( 0xE0, 0xF2, 0xFE ) ) );
        mdl->addObject( mover );

        win.setModel( mdl );
        win.show();
        QCoreApplication::processEvents();

        LabelEditor* editor = win.labelEditor();
        QVERIFY( editor );
        const double tolPt = SnapEngine::toleranceForScale( editor->scale() );
        QVERIFY( tolPt > 0.5 );
        const double nudgePt = tolPt * 0.45; // comfortably inside 6 px tolerance

        auto dragObject = [&]( model::ModelObject* obj,
                               double              dxPt,
                               double              dyPt,
                               Qt::KeyboardModifiers mods,
                               const QString&      screenshotFile = QString() )
        {
                const QPointF startLabel( obj->x0().pt() + obj->w().pt() / 2.0,
                                          obj->y0().pt() + obj->h().pt() / 2.0 );
                const QPointF endLabel( startLabel.x() + dxPt, startLabel.y() + dyPt );
                const QPointF startPx = editor->labelToWidget( startLabel );
                const QPointF endPx   = editor->labelToWidget( endLabel );

                sendMouse( editor, QEvent::MouseButtonPress, startPx, Qt::LeftButton, Qt::LeftButton, mods );
                sendMouse( editor, QEvent::MouseMove,        endPx,   Qt::NoButton,   Qt::LeftButton, mods );
                QCoreApplication::processEvents();

                if ( !screenshotFile.isEmpty() )
                {
                        win.grab().save( screenshotFile );
                }

                sendMouse( editor, QEvent::MouseButtonRelease, endPx, Qt::LeftButton, Qt::NoButton, mods );
                QCoreApplication::processEvents();
        };

        const QString shotBase = qEnvironmentVariable( "GLABELS_THEME_SCREENSHOTS" );
        QString snapShotDir;
        if ( !shotBase.isEmpty() )
        {
                snapShotDir = QDir( shotBase ).filePath( QStringLiteral( "snap" ) );
                QDir().mkpath( snapShotDir );
        }

        // 1. Move mover near anchorBox's right edge (84.0) and bottom edge (60.0)
        // Currently mover at (140, 100); target raw pos = (84 + nudgePt, 60 + nudgePt)
        dragObject( mover, (84.0 + nudgePt) - 140.0, (60.0 + nudgePt) - 100.0, Qt::NoModifier );
        QVERIFY2( std::abs( mover->x0().pt() - 84.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected x0=84, got %1" ).arg( mover->x0().pt() ) ) );
        QVERIFY2( std::abs( mover->y0().pt() - 60.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected y0=60, got %1" ).arg( mover->y0().pt() ) ) );
        QVERIFY( editor->activeSnap().guides.isEmpty() ); // cleared after release

        // 2. Move mover near label centre (120, 80): desired top-left = (100, 68), add nudgePt
        dragObject( mover, (100.0 + nudgePt) - mover->x0().pt(), (68.0 - nudgePt) - mover->y0().pt(),
                    Qt::NoModifier );
        QVERIFY2( std::abs( (mover->x0().pt() + mover->w().pt() / 2.0) - 120.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected centre X=120, got %1" ).arg( mover->x0().pt() + mover->w().pt() / 2.0 ) ) );
        QVERIFY2( std::abs( (mover->y0().pt() + mover->h().pt() / 2.0) - 80.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected centre Y=80, got %1" ).arg( mover->y0().pt() + mover->h().pt() / 2.0 ) ) );

        // 2b. Capture light screenshot on View tab showing Snap group + dashed Centre (X) + solid Edge of box (Y) + 2-line caption
        win.ribbonBar()->setCurrentTab( ribbon::TabId::View, true );
        QCoreApplication::processEvents();
        const QString lightShot = snapShotDir.isEmpty()
                ? QString()
                : QDir( snapShotDir ).filePath( QStringLiteral( "snap-guides-light.png" ) );
        // Target: centre X = 120 (x0 = 100), top Y = anchorBox bottom 60.0
        dragObject( mover, (100.0 + nudgePt) - mover->x0().pt(), (60.0 + nudgePt) - mover->y0().pt(),
                    Qt::NoModifier, lightShot );
        QVERIFY2( std::abs( (mover->x0().pt() + mover->w().pt() / 2.0) - 120.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected centre X=120, got %1" ).arg( mover->x0().pt() + mover->w().pt() / 2.0 ) ) );
        QVERIFY2( std::abs( mover->y0().pt() - 60.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected top Y=60, got %1" ).arg( mover->y0().pt() ) ) );

        // 3. Move mover near safe-area right (228) and bottom (148) in dark theme
        // Desired top-left = (188, 124), offset by -nudgePt
        model::Settings::setColorScheme( model::Settings::DARK_COLOR_SCHEME );
        win.ribbonBar()->setCurrentTab( ribbon::TabId::Home, true );
        QCoreApplication::processEvents();
        const QString darkShot = snapShotDir.isEmpty()
                ? QString()
                : QDir( snapShotDir ).filePath( QStringLiteral( "snap-guides-dark.png" ) );
        dragObject( mover, (188.0 - nudgePt) - mover->x0().pt(), (124.0 - nudgePt) - mover->y0().pt(),
                    Qt::NoModifier, darkShot );
        QVERIFY2( std::abs( (mover->x0().pt() + mover->w().pt()) - 228.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected right=228, got %1" ).arg( mover->x0().pt() + mover->w().pt() ) ) );
        QVERIFY2( std::abs( (mover->y0().pt() + mover->h().pt()) - 148.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Expected bottom=148, got %1" ).arg( mover->y0().pt() + mover->h().pt() ) ) );

        // Restore light mode for remaining checks
        model::Settings::setColorScheme( model::Settings::LIGHT_COLOR_SCHEME );
        QCoreApplication::processEvents();

        // 4. Alt bypasses snapping: move near safe area (12, 12) with Qt::AltModifier -> stays at exact unsnapped coords
        const double rawBypassX0 = 12.0 + nudgePt;
        const double rawBypassY0 = 12.0 + nudgePt;
        dragObject( mover, rawBypassX0 - mover->x0().pt(), rawBypassY0 - mover->y0().pt(), Qt::AltModifier );
        QVERIFY2( std::abs( mover->x0().pt() - rawBypassX0 ) < 1.0,
                  qPrintable( QStringLiteral( "Alt bypass expected ~%1, got %2" ).arg( rawBypassX0 ).arg( mover->x0().pt() ) ) );
        QVERIFY( std::abs( mover->x0().pt() - 12.0 ) > 0.2 ); // definitely NOT snapped to 12.0

        // 5. Resize E handle of anchorBox (currently right=84) near label centre x=120 -> snaps right to 120.0
        mdl->unselectAll();
        mdl->selectObject( anchorBox );
        QCoreApplication::processEvents();
        const QPointF eHandlePx = editor->labelToWidget( QPointF( 84.0, 42.0 ) );
        const QPointF eTargetPx = editor->labelToWidget( QPointF( 120.0 - nudgePt, 42.0 ) );
        sendMouse( editor, QEvent::MouseButtonPress,   eHandlePx, Qt::LeftButton, Qt::LeftButton );
        sendMouse( editor, QEvent::MouseMove,          eTargetPx, Qt::NoButton,   Qt::LeftButton );
        QVERIFY( editor->activeSnap().snappedX );
        sendMouse( editor, QEvent::MouseButtonRelease, eTargetPx, Qt::LeftButton, Qt::NoButton );
        QVERIFY2( std::abs( (anchorBox->x0().pt() + anchorBox->w().pt()) - 120.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Resize E expected right=120, got %1" ).arg( anchorBox->x0().pt() + anchorBox->w().pt() ) ) );

        // 6. Create with Box tool (drag-create): anchor at (20, 80), drag cursor near safe area right=228, bottom=148
        editor->createBoxMode();
        const QPointF createStartPx = editor->labelToWidget( QPointF( 20.0, 80.0 ) );
        const QPointF createEndPx   = editor->labelToWidget( QPointF( 228.0 - nudgePt, 148.0 - nudgePt ) );
        sendMouse( editor, QEvent::MouseButtonPress,   createStartPx, Qt::LeftButton, Qt::LeftButton );
        sendMouse( editor, QEvent::MouseMove,          createEndPx,   Qt::NoButton,   Qt::LeftButton );
        QVERIFY( editor->activeSnap().hasSafeAreaSnap() );
        sendMouse( editor, QEvent::MouseButtonRelease, createEndPx,   Qt::LeftButton, Qt::NoButton );
        QCoreApplication::processEvents();

        model::ModelObject* created = mdl->getFirstSelectedObject();
        QVERIFY( created && created != anchorBox && created != mover );
        QVERIFY2( std::abs( (created->x0().pt() + created->w().pt()) - 228.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Drag-create expected right=228, got %1" ).arg( created->x0().pt() + created->w().pt() ) ) );
        QVERIFY2( std::abs( (created->y0().pt() + created->h().pt()) - 148.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Drag-create expected bottom=148, got %1" ).arg( created->y0().pt() + created->h().pt() ) ) );

        // 7. Plain click-create (Text 72x36 default): click so centre is within tolerance of label centre (120, 80)
        // Default top-left for centre (120, 80) is (84, 62); click at (84 + nudgePt, 62 - nudgePt)
        editor->createTextMode();
        const QPointF clickPx = editor->labelToWidget( QPointF( 84.0 + nudgePt, 62.0 - nudgePt ) );
        sendMouse( editor, QEvent::MouseButtonPress,   clickPx, Qt::LeftButton, Qt::LeftButton );
        sendMouse( editor, QEvent::MouseButtonRelease, clickPx, Qt::LeftButton, Qt::NoButton );
        QCoreApplication::processEvents();

        model::ModelObject* clicked = mdl->getFirstSelectedObject();
        QVERIFY( clicked && clicked != created );
        QVERIFY2( std::abs( (clicked->x0().pt() + clicked->w().pt() / 2.0) - 120.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Click-create expected centre X=120, got %1" ).arg( clicked->x0().pt() + clicked->w().pt() / 2.0 ) ) );
        QVERIFY2( std::abs( (clicked->y0().pt() + clicked->h().pt() / 2.0) - 80.0 ) < 1e-6,
                  qPrintable( QStringLiteral( "Click-create expected centre Y=80, got %1" ).arg( clicked->y0().pt() + clicked->h().pt() / 2.0 ) ) );

        // 8. Keyboard nudging does NOT snap
        const double beforeKeyX = clicked->x0().pt();
        QKeyEvent keyRight( QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier );
        QCoreApplication::sendEvent( editor, &keyRight );
        QVERIFY( clicked->x0().pt() > beforeKeyX );
}


void TestSnap::viewTabSnapGroupAndSettingsSync()
{
        // Reset keys to verify default-on behaviour
        model::Settings::instance()->beginGroup( QStringLiteral( "View" ) );
        model::Settings::instance()->remove( QStringLiteral( "snapObjects" ) );
        model::Settings::instance()->remove( QStringLiteral( "snapCenter" ) );
        model::Settings::instance()->remove( QStringLiteral( "snapSafeArea" ) );
        model::Settings::instance()->endGroup();

        QVERIFY( model::Settings::snapObjects() );
        QVERIFY( model::Settings::snapCenter() );
        QVERIFY( model::Settings::snapSafeArea() );

        MainWindow win1;
        MainWindow win2;

        // Verify View tab has Snap group between Show and Zoom
        ribbon::RibbonTab* viewTab = win1.ribbonBar()->tab( ribbon::TabId::View );
        QVERIFY( viewTab );
        QStringList groupTitles;
        ribbon::RibbonGroup* snapGroup = nullptr;
        for ( ribbon::RibbonGroup* g : viewTab->groups() )
        {
                groupTitles.push_back( g->title() );
                if ( g->title() == QStringLiteral( "Snap" ) )
                {
                        snapGroup = g;
                }
        }
        QVERIFY( snapGroup );
        const int idxShow = groupTitles.indexOf( QStringLiteral( "Show" ) );
        const int idxSnap = groupTitles.indexOf( QStringLiteral( "Snap" ) );
        const int idxZoom = groupTitles.indexOf( QStringLiteral( "Zoom" ) );
        QVERIFY( idxShow >= 0 && idxSnap == idxShow + 1 && idxZoom == idxSnap + 1 );

        // Verify caption "Hold Alt to bypass" exists inside Snap group
        bool hasCaption = false;
        for ( QLabel* lbl : snapGroup->findChildren<QLabel*>() )
        {
                if ( lbl->text() == QStringLiteral( "Hold Alt to bypass" ) )
                {
                        hasCaption = true;
                        break;
                }
        }
        QVERIFY( hasCaption );

        // Verify the 3 actions exist, are checkable, checked by default, have status tips, and sync across windows
        auto findAction = []( MainWindow& w, const QString& text ) -> QAction*
        {
                for ( QAction* a : w.allWindowActions() )
                {
                        if ( a->text() == text )
                        {
                                return a;
                        }
                }
                return nullptr;
        };

        QAction* w1Obj  = findAction( win1, QStringLiteral( "Objects" ) );
        QAction* w1Ctr  = findAction( win1, QStringLiteral( "Centre" ) );
        QAction* w1Safe = findAction( win1, QStringLiteral( "Safe area" ) );
        QAction* w2Obj  = findAction( win2, QStringLiteral( "Objects" ) );
        QAction* w2Ctr  = findAction( win2, QStringLiteral( "Centre" ) );
        QAction* w2Safe = findAction( win2, QStringLiteral( "Safe area" ) );

        QVERIFY( w1Obj && w1Ctr && w1Safe && w2Obj && w2Ctr && w2Safe );
        QVERIFY( w1Obj->isCheckable() && w1Obj->isChecked() && !w1Obj->statusTip().isEmpty() );
        QVERIFY( w1Ctr->isCheckable() && w1Ctr->isChecked() && !w1Ctr->statusTip().isEmpty() );
        QVERIFY( w1Safe->isCheckable() && w1Safe->isChecked() && !w1Safe->statusTip().isEmpty() );

        // Toggling in win1 persists to Settings and syncs to win2
        w1Obj->setChecked( false );
        QVERIFY( !model::Settings::snapObjects() );
        QVERIFY( !w2Obj->isChecked() );

        w1Ctr->setChecked( false );
        QVERIFY( !model::Settings::snapCenter() );
        QVERIFY( !w2Ctr->isChecked() );

        w1Safe->setChecked( false );
        QVERIFY( !model::Settings::snapSafeArea() );
        QVERIFY( !w2Safe->isChecked() );

        // Restore defaults
        model::Settings::setSnapObjects( true );
        model::Settings::setSnapCenter( true );
        model::Settings::setSnapSafeArea( true );
}


QTEST_MAIN( TestSnap )
