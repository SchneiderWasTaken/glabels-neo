//  TestPrinting.cpp
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

#include "TestPrinting.hpp"

#include "barcode/Backends.hpp"
#include "merge/Factory.hpp"
#include "model/Db.hpp"
#include "model/Model.hpp"
#include "model/PageRenderer.hpp"
#include "model/Settings.hpp"
#include "model/XmlLabelParser.hpp"
#include "printing/PrintService.hpp"
#include "printing/ZplPrinter.hpp"

#include <QBuffer>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

using namespace glabels::printing;


namespace
{

        std::unique_ptr<glabels::model::Model> loadLabel()
        {
                return std::unique_ptr<glabels::model::Model>(
                        glabels::model::XmlLabelParser::readFile( QString( TEST_DATA_DIR ) + "/fields.glabels" ) );
        }

}


void TestPrinting::initTestCase()
{
        QCoreApplication::setOrganizationName( "glabels.org" );
        QCoreApplication::setApplicationName( "glabelsneo-test" );
        glabels::model::Settings::init();
        glabels::model::Db::init();
        glabels::merge::Factory::init();
        glabels::barcode::Backends::init();
}


void TestPrinting::zplPacksPixelsMostSignificantBitFirst()
{
        // 16 x 2 dots, only the top-left one black: row 0 = 0x80 0x00, row 1 = 0x00 0x00.
        QImage image( 16, 2, QImage::Format_ARGB32 );
        image.fill( Qt::white );
        image.setPixel( 0, 0, qRgb( 0, 0, 0 ) );

        const QByteArray zpl = ZplPrinter::imageToZpl( image );
        QVERIFY2( zpl.startsWith( "^XA" ), zpl.constData() );
        QVERIFY( zpl.endsWith( "^XZ" ) );
        QVERIFY( zpl.contains( "^PW16" ) );
        QVERIFY( zpl.contains( "^LL2" ) );
        QVERIFY2( zpl.contains( "^GFA,4,4,2,80000000^FS" ), zpl.constData() );

        // The last dot of a row is the lowest bit of the row's last byte.
        QImage edge( 9, 1, QImage::Format_ARGB32 );
        edge.fill( Qt::white );
        edge.setPixel( 8, 0, qRgb( 0, 0, 0 ) );
        QVERIFY2( ZplPrinter::imageToZpl( edge ).contains( "^GFA,2,2,2,0080^FS" ), "9th dot is the top bit of byte 2" );

        // Mid-grey and darker print, lighter does not (threshold 128).
        QImage grey( 8, 1, QImage::Format_ARGB32 );
        grey.fill( qRgb( 200, 200, 200 ) );
        grey.setPixel( 0, 0, qRgb( 100, 100, 100 ) );
        QVERIFY( ZplPrinter::imageToZpl( grey ).contains( "^GFA,1,1,1,80^FS" ) );

        QVERIFY( ZplPrinter::imageToZpl( QImage() ).isEmpty() );
}


void TestPrinting::zplRepeatsCopies()
{
        QImage image( 8, 1, QImage::Format_ARGB32 );
        image.fill( Qt::black );
        const QByteArray zpl = ZplPrinter::imageToZpl( image, 3 );
        QCOMPARE( zpl.count( "^XA" ), 3 );
        QCOMPARE( zpl.count( "^XZ" ), 3 );
}


void TestPrinting::zplJoinsPages()
{
        QImage a( 8, 1, QImage::Format_ARGB32 );
        a.fill( Qt::black );
        QImage b( 8, 1, QImage::Format_ARGB32 );
        b.fill( Qt::white );
        const QByteArray zpl = ZplPrinter::imagesToZpl( { a, b } );
        QCOMPARE( zpl.count( "^XA" ), 2 );
        QVERIFY( zpl.contains( "^GFA,1,1,1,FF^FS" ) );
        QVERIFY( zpl.contains( "^GFA,1,1,1,00^FS" ) );
}


void TestPrinting::zebraDetection()
{
        QVERIFY( ZplPrinter::isZebraPrinter( "Zebra ZD420-203dpi ZPL" ) );
        QVERIFY( ZplPrinter::isZebraPrinter( "ZDesigner GK420d" ) );
        QVERIFY( !ZplPrinter::isZebraPrinter( QString() ) );
        QVERIFY( !ZplPrinter::isZebraPrinter( "Brother QL-800" ) );
        QVERIFY( !ZplPrinter::isZebraPrinter( "HP LaserJet Pro M404" ) );
}


void TestPrinting::driverChoiceOverridesDetection()
{
        // --driver lets a script override a misdetection in either direction.
        QVERIFY( PrintService::usesZpl( "Brother QL-800", Driver::Zpl ) );
        QVERIFY( !PrintService::usesZpl( "Zebra ZD420", Driver::System ) );
        QVERIFY( PrintService::usesZpl( "Zebra ZD420", Driver::Auto ) );
        QVERIFY( !PrintService::usesZpl( "Brother QL-800", Driver::Auto ) );
}


void TestPrinting::printersAreNeverGuessed()
{
        // Whatever is installed on this machine, a made-up name must not resolve.
        QCOMPARE( PrintService::resolvePrinter( "No Such Printer 12345" ).status, ResolvedPrinter::Status::NotFound );
        QCOMPARE( PrintService::resolvePrinter( "" ).status, ResolvedPrinter::Status::NotFound );

        // "default" is Found (with a real name) or NoDefault, never NotFound.
        const ResolvedPrinter d = PrintService::resolvePrinter( "DEFAULT" );
        QVERIFY( d.status == ResolvedPrinter::Status::Found || d.status == ResolvedPrinter::Status::NoDefault );
        if ( d.status == ResolvedPrinter::Status::Found )
        {
                QVERIFY( !d.name.isEmpty() );
        }

        // An installed printer resolves exactly and case-insensitively, but a prefix does not.
        const auto printers = PrintService::printers();
        if ( !printers.isEmpty() )
        {
                const QString name = printers.first().name;
                QCOMPARE( PrintService::resolvePrinter( name ).name, name );
                QCOMPARE( PrintService::resolvePrinter( name.toUpper() ).status, ResolvedPrinter::Status::Found );
                if ( name.size() > 3 )
                {
                        QVERIFY( PrintService::resolvePrinter( name.left( name.size() - 1 ) ).status != ResolvedPrinter::Status::Found );
                }
        }
}


void TestPrinting::unknownPrinterFailsCleanly()
{
        auto model = loadLabel();
        QVERIFY( model );
        glabels::model::PageRenderer renderer( model.get() );
        renderer.setNCopies( 1 );

        const PrintResult r = PrintService::printToPrinter( renderer, "No Such Printer 12345" );
        QVERIFY( !r.ok );
        QVERIFY2( r.error.contains( "not installed" ), qPrintable( r.error ) );
}


void TestPrinting::pdfFileAndStandardOutput()
{
        auto model = loadLabel();
        QVERIFY( model );
        glabels::model::PageRenderer renderer( model.get() );
        renderer.setNCopies( 2 );
        renderer.setStartItem( 0 );

        QTemporaryDir dir;
        PrintResult r = PrintService::printToPdf( renderer, dir.filePath( "a.pdf" ) );
        QVERIFY2( r.ok, qPrintable( r.error ) );
        QCOMPARE( r.pages, 2 );
        QFile f( dir.filePath( "a.pdf" ) );
        QVERIFY( f.open( QIODevice::ReadOnly ) );
        QCOMPARE( f.read( 4 ), QByteArray( "%PDF" ) );

        // "-" goes to the supplied device, and leaves no file behind
        QBuffer out;
        QVERIFY( out.open( QIODevice::WriteOnly ) );
        r = PrintService::printToPdf( renderer, "-", &out );
        QVERIFY2( r.ok, qPrintable( r.error ) );
        QVERIFY( out.data().startsWith( "%PDF" ) );

        // ...and without a device it fails instead of silently dropping the PDF
        r = PrintService::printToPdf( renderer, "-", nullptr );
        QVERIFY( !r.ok );
}


void TestPrinting::imagesPerPage()
{
        auto model = loadLabel();
        QVERIFY( model );
        glabels::model::PageRenderer renderer( model.get() );
        renderer.setNCopies( 3 );
        renderer.setStartItem( 0 );

        QTemporaryDir dir;
        PrintResult r = PrintService::printToImages( renderer, dir.filePath( "l.png" ), 72 );
        QVERIFY2( r.ok, qPrintable( r.error ) );
        QCOMPARE( r.pages, 3 );
        for ( int i = 1; i <= 3; i++ )
        {
                QImage image( dir.filePath( QString( "l-%1.png" ).arg( i ) ) );
                QVERIFY( !image.isNull() );
                // 72 dpi: one pixel per point, so the size is the template's page size
                QCOMPARE( image.width(),  qRound( model->tmplate().pageWidth().pt() ) );
                QCOMPARE( image.height(), qRound( model->tmplate().pageHeight().pt() ) );
        }
        QVERIFY( !QFileInfo::exists( dir.filePath( "l-4.png" ) ) );

        // a single page keeps the exact name
        renderer.setNCopies( 1 );
        r = PrintService::printToImages( renderer, dir.filePath( "single.png" ), 72 );
        QVERIFY( r.ok );
        QVERIFY( QFileInfo::exists( dir.filePath( "single.png" ) ) );
}


void TestPrinting::badOutputFolderFails()
{
        auto model = loadLabel();
        QVERIFY( model );
        glabels::model::PageRenderer renderer( model.get() );
        renderer.setNCopies( 1 );

        QTemporaryDir dir;
        PrintResult r = PrintService::printToPdf( renderer, dir.filePath( "no/such/folder/a.pdf" ) );
        QVERIFY( !r.ok );
        QVERIFY( r.error.contains( "does not exist" ) );

        r = PrintService::printToImages( renderer, dir.filePath( "no/such/folder/a.png" ), 72 );
        QVERIFY( !r.ok );
}


QTEST_MAIN( TestPrinting )
