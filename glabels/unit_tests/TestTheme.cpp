//  TestTheme.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels Neo.
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


#include "TestTheme.hpp"

#include "theme/Decorator.hpp"
#include "theme/Theme.hpp"
#include "theme/ThemeIcons.hpp"
#include "theme/ThemeScheme.hpp"
#include "theme/ThemeStyle.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>


QTEST_MAIN(TestTheme)


using namespace glabels::theme;


namespace
{

        ///
        /// Collects stylesheet parser warnings while alive
        ///
        class StyleWarnings
        {
        public:
                StyleWarnings()
                {
                        messages().clear();
                        mPrevious = qInstallMessageHandler( handler );
                }

                ~StyleWarnings()
                {
                        qInstallMessageHandler( mPrevious );
                }

                static QStringList& messages()
                {
                        static QStringList list;
                        return list;
                }

        private:
                static void handler( QtMsgType type, const QMessageLogContext&, const QString& msg )
                {
                        if ( type != QtDebugMsg && type != QtInfoMsg &&
                             ( msg.contains( "stylesheet", Qt::CaseInsensitive ) ||
                               msg.contains( "parse", Qt::CaseInsensitive ) ||
                               msg.contains( "ThemeStyle" ) ||
                               msg.contains( "svg", Qt::CaseInsensitive ) ) )
                        {
                                messages().append( msg );
                        }
                }

                QtMessageHandler mPrevious;
        };


        double luminance( const QColor& c )
        {
                auto channel = []( double v ) {
                        return v <= 0.03928 ? v / 12.92 : std::pow( ( v + 0.055 ) / 1.055, 2.4 );
                };
                return 0.2126 * channel( c.redF() ) + 0.7152 * channel( c.greenF() ) + 0.0722 * channel( c.blueF() );
        }


        double contrast( const QColor& a, const QColor& b )
        {
                double la = luminance( a ), lb = luminance( b );
                if ( la < lb ) std::swap( la, lb );
                return ( la + 0.05 ) / ( lb + 0.05 );
        }


        bool near( const QColor& a, const QColor& b, int tolerance = 3 )
        {
                return std::abs( a.red() - b.red() ) <= tolerance &&
                       std::abs( a.green() - b.green() ) <= tolerance &&
                       std::abs( a.blue() - b.blue() ) <= tolerance;
        }


        /// The most opaque pixel of an icon image, un-premultiplied
        QColor strongestPixel( const QImage& source )
        {
                QImage image = source.convertToFormat( QImage::Format_ARGB32 );
                QColor best( 0, 0, 0, 0 );
                for ( int y = 0; y < image.height(); y++ )
                {
                        for ( int x = 0; x < image.width(); x++ )
                        {
                                QColor c = QColor::fromRgba( image.pixel( x, y ) );
                                if ( c.alpha() > best.alpha() )
                                {
                                        best = c;
                                }
                        }
                }
                return best;
        }


        QString stripComments( QString css )
        {
                static const QRegularExpression comment( QStringLiteral( "/\\*.*?\\*/" ),
                                                         QRegularExpression::DotMatchesEverythingOption );
                return css.remove( comment );
        }


        QStringList specIcons()
        {
                // SPEC.md 4.4
                return {
                        "file-plus", "folder-open", "save", "save-all", "undo-2", "redo-2",
                        "scissors", "copy", "clipboard-paste", "trash-2", "square-dashed-mouse-pointer",
                        "square-dashed", "zoom-in", "zoom-out", "scan-line", "scan", "bring-to-front",
                        "send-to-back", "grid-3x3", "ruler", "align-start-vertical", "align-center-vertical",
                        "align-end-vertical", "align-start-horizontal", "align-center-horizontal",
                        "align-end-horizontal", "rotate-ccw", "rotate-cw", "flip-horizontal-2",
                        "flip-vertical-2", "align-horizontal-justify-center", "align-vertical-justify-center",
                        "crosshair", "lock", "lock-open", "align-left", "align-center", "align-right",
                        "bold", "italic", "underline", "braces", "tag", "database", "table-2", "printer",
                        "settings-2", "send", "package", "download", "upload", "plus", "minus", "x",
                        "refresh-cw", "book-open", "bug", "info", "settings", "square-dashed-bottom-code",
                        "mouse-pointer-2", "type", "square", "slash", "circle", "image", "barcode"
                };
        }
}


///
/// Every test starts from the light theme with no application stylesheet
///
void TestTheme::cleanup()
{
        Theme::instance().setScheme( "neo" );
        Theme::instance().setMode( Mode::Light );
        ThemeIcons::invalidate();
        qApp->setStyleSheet( QString() );
}


void TestTheme::lightPaletteMatchesTheSpec()
{
        Palette p = Theme::lightPalette();

        QCOMPARE( p.bg,      QColor( "#F8FAFC" ) );
        QCOMPARE( p.chrome,  QColor( "#F1F5F9" ) );
        QCOMPARE( p.ribbon,  QColor( "#FFFFFF" ) );
        QCOMPARE( p.work,    QColor( "#E2E8F0" ) );
        QCOMPARE( p.panel,   QColor( "#FFFFFF" ) );
        QCOMPARE( p.line,    QColor( "#E2E8F0" ) );
        QCOMPARE( p.line2,   QColor( "#CBD5E1" ) );
        QCOMPARE( p.ink,     QColor( "#0F172A" ) );
        QCOMPARE( p.ink2,    QColor( "#475569" ) );
        QCOMPARE( p.ink3,    QColor( "#64748B" ) );
        QCOMPARE( p.accent,  QColor( "#4F46E5" ) );
        QCOMPARE( p.tint,    QColor( "#EDE9FE" ) );
        QCOMPARE( p.hover,   QColor( "#F1F5F9" ) );
        QCOMPARE( p.pressed, QColor( "#E0E7FF" ) );
        QCOMPARE( p.ctx,     QColor( "#EDE9FE" ) );
        QCOMPARE( p.ctxLine, QColor( "#7C3AED" ) );
        QCOMPARE( p.ok,      QColor( "#047857" ) );
        QCOMPARE( p.okTint,  QColor( "#ECFDF5" ) );
        QCOMPARE( p.warn,    QColor( "#B45309" ) );
        QCOMPARE( p.gradA,   QColor( "#7C3AED" ) );
        QCOMPARE( p.gradB,   QColor( "#4F46E5" ) );
        QCOMPARE( p.gradC,   QColor( "#2563EB" ) );
        QCOMPARE( p.onGrad,  QColor( "#FFFFFF" ) );
}


void TestTheme::darkPaletteMatchesTheSpec()
{
        Palette p = Theme::darkPalette();

        QCOMPARE( p.bg,      QColor( "#090D1A" ) );
        QCOMPARE( p.chrome,  QColor( "#0F172A" ) );
        QCOMPARE( p.ribbon,  QColor( "#151E32" ) );
        QCOMPARE( p.work,    QColor( "#090D1A" ) );
        QCOMPARE( p.panel,   QColor( "#0F172A" ) );
        QCOMPARE( p.line,    QColor( "#1E293B" ) );
        QCOMPARE( p.line2,   QColor( "#334155" ) );
        QCOMPARE( p.ink,     QColor( "#F8FAFC" ) );
        QCOMPARE( p.ink2,    QColor( "#CBD5E1" ) );
        QCOMPARE( p.ink3,    QColor( "#94A3B8" ) );
        QCOMPARE( p.accent,  QColor( "#818CF8" ) );
        QCOMPARE( p.tint,    QColor( "#1E293B" ) );
        QCOMPARE( p.hover,   QColor( "#1E293B" ) );
        QCOMPARE( p.pressed, QColor( "#2B3556" ) );
        QCOMPARE( p.ctx,     QColor( "#2A2447" ) );
        QCOMPARE( p.ctxLine, QColor( "#A78BFA" ) );
        QCOMPARE( p.ok,      QColor( "#34D399" ) );
        QCOMPARE( p.okTint,  QColor( "#064E3B" ) );
        QCOMPARE( p.warn,    QColor( "#FBBF24" ) );
        QCOMPARE( p.gradA,   QColor( "#A78BFA" ) );
        QCOMPARE( p.gradB,   QColor( "#818CF8" ) );
        QCOMPARE( p.gradC,   QColor( "#60A5FA" ) );
        QCOMPARE( p.onGrad,  QColor( "#090D1A" ) );
}


void TestTheme::metricsMatchTheSpec()
{
        QCOMPARE( Metrics::tabStrip, 42 );
        QCOMPARE( Metrics::tab, 34 );
        QCOMPARE( Metrics::tabActive, 36 );
        QCOMPARE( Metrics::ribbon, 118 );
        QCOMPARE( Metrics::largeW, 58 );
        QCOMPARE( Metrics::largeH, 78 );
        QCOMPARE( Metrics::smallH, 26 );
        QCOMPARE( Metrics::iconBtn, 28 );
        QCOMPARE( Metrics::field, 26 );
        QCOMPARE( Metrics::groupName, 22 );
        QCOMPARE( Metrics::groupPad, 8 );
        QCOMPARE( Metrics::contentPanel, 300 );
        QCOMPARE( Metrics::status, 28 );
        QCOMPARE( Metrics::ruler, 22 );

        // 6 top padding + 90 group body + 22 group name
        QCOMPARE( 6 + 90 + Metrics::groupName, Metrics::ribbon );
}


void TestTheme::modeSelectsThePalette()
{
        auto& theme = Theme::instance();

        theme.setMode( Mode::Dark );
        QCOMPARE( theme.mode(), Mode::Dark );
        QCOMPARE( theme.palette().bg, QColor( "#090D1A" ) );
        QCOMPARE( theme.qtPalette().color( QPalette::Window ), QColor( "#090D1A" ) );
        QCOMPARE( theme.qtPalette().color( QPalette::Highlight ), QColor( "#818CF8" ) );

        theme.setMode( Mode::Light );
        QCOMPARE( theme.mode(), Mode::Light );
        QCOMPARE( theme.palette().accent, QColor( "#4F46E5" ) );
        QCOMPARE( theme.qtPalette().color( QPalette::Text ), QColor( "#0F172A" ) );

        // Disabled text is ink at 40 %
        QColor disabled = theme.qtPalette().color( QPalette::Disabled, QPalette::Text );
        QCOMPARE( disabled.rgb(), QColor( "#0F172A" ).rgb() );
        QCOMPARE( disabled.alpha(), qRound( 0.4 * 255 ) );
}


void TestTheme::preferenceResolvesToAMode()
{
        // model::Settings order: 0 light, 1 dark, 2 system
        QCOMPARE( Theme::resolveMode( 0, false ), Mode::Light );
        QCOMPARE( Theme::resolveMode( 0, true ),  Mode::Light );
        QCOMPARE( Theme::resolveMode( 1, false ), Mode::Dark );
        QCOMPARE( Theme::resolveMode( 1, true ),  Mode::Dark );
        QCOMPARE( Theme::resolveMode( 2, false ), Mode::Light );
        QCOMPARE( Theme::resolveMode( 2, true ),  Mode::Dark );
        QCOMPARE( Theme::resolveMode( 7, true ),  Mode::Light );   // unknown value: light, as Settings does
}


void TestTheme::fontsUseTheSpecStacks()
{
        QFont ui = Theme::instance().uiFont( 12, 600 );
        QCOMPARE( ui.pixelSize(), 12 );
        QCOMPARE( ui.weight(), QFont::DemiBold );
        QCOMPARE( ui.families().value( 0 ), QString( "Inter" ) );
        QCOMPARE( ui.families().value( 1 ), QString( "Segoe UI" ) );

        QFont title = Theme::instance().uiFont( 20, 700 );
        QCOMPARE( title.pixelSize(), 20 );
        QCOMPARE( title.weight(), QFont::Bold );

        QFont mono = Theme::instance().monoFont( 12 );
        QCOMPARE( mono.pixelSize(), 12 );
        QCOMPARE( mono.families().value( 0 ), QString( "JetBrains Mono" ) );
        QCOMPARE( mono.families().value( 1 ), QString( "Consolas" ) );
}


void TestTheme::textTokensMeetContrast()
{
        // SPEC.md 11: 4.5:1 for text.  Only the pairs the stylesheet uses for body text are
        // asserted; known failing pairs are reported in docs/ribbon-ui/QUESTIONS.md.
        for ( Mode mode : { Mode::Light, Mode::Dark } )
        {
                const Palette& p = Theme::paletteFor( mode );
                for ( const QColor& surface : { p.bg, p.chrome, p.ribbon, p.panel } )
                {
                        QVERIFY2( contrast( p.ink, surface ) >= 4.5, "ink" );
                        QVERIFY2( contrast( p.ink2, surface ) >= 4.5, "ink2" );
                }
                for ( const QColor& surface : { p.bg, p.ribbon, p.panel } )
                {
                        QVERIFY2( contrast( p.ink3, surface ) >= 4.5, "ink3" );
                }
                QVERIFY2( contrast( p.onGrad, p.accent ) >= 4.5, "onGrad on accent" );
                QVERIFY2( contrast( p.onGrad, p.gradA ) >= 4.5, "onGrad on gradA" );
                QVERIFY2( contrast( p.onGrad, p.gradC ) >= 4.5, "onGrad on gradC" );
                QVERIFY2( contrast( p.ok, p.okTint ) >= 4.5, "ok on okTint" );
                QVERIFY2( contrast( p.accent, p.panel ) >= 4.5, "accent (links) on panel" );
        }
}


void TestTheme::stylesheetExpandsEveryToken()
{
        for ( Mode mode : { Mode::Light, Mode::Dark } )
        {
                QStringList unknown;
                QString css = ThemeStyle::expand( ThemeStyle::styleTemplate(),
                                                  ThemeStyle::tokens( Theme::paletteFor( mode ), mode ),
                                                  &unknown );
                QVERIFY2( unknown.isEmpty(), qPrintable( unknown.join( ", " ) ) );
                QVERIFY( !css.contains( '@' ) );
                QVERIFY( css.size() > 10000 );
        }

        // Alpha suffix
        QHash<QString, QString> tokens { { "ink", "#0f172a" } };
        QCOMPARE( ThemeStyle::expand( "color: @ink/40;", tokens ), QString( "color: rgba(15, 23, 42, 40%);" ) );
        QCOMPARE( ThemeStyle::expand( "color: @ink;", tokens ), QString( "color: #0f172a;" ) );
}


void TestTheme::stylesheetIsWellFormed()
{
        QString css = stripComments( Theme::instance().styleSheet() );

        int depth = 0;
        int rules = 0;
        QString selector;
        QString body;
        for ( QChar c : css )
        {
                if ( c == '{' )
                {
                        QCOMPARE( depth, 0 );
                        QVERIFY2( !selector.trimmed().isEmpty(), "rule without a selector" );
                        depth++;
                        body.clear();
                }
                else if ( c == '}' )
                {
                        QCOMPARE( depth, 1 );
                        depth--;
                        rules++;
                        for ( const QString& declaration : body.split( ';', Qt::SkipEmptyParts ) )
                        {
                                if ( declaration.trimmed().isEmpty() ) continue;
                                QVERIFY2( declaration.contains( ':' ),
                                          qPrintable( selector.trimmed() + " / " + declaration.trimmed() ) );
                                QVERIFY2( !declaration.section( ':', 1 ).trimmed().isEmpty(),
                                          qPrintable( selector.trimmed() + " / " + declaration.trimmed() ) );
                        }
                        selector.clear();
                }
                else if ( depth == 0 )
                {
                        selector += c;
                }
                else
                {
                        body += c;
                }
        }
        QCOMPARE( depth, 0 );
        QVERIFY( selector.trimmed().isEmpty() );
        QVERIFY( rules > 150 );
}


void TestTheme::stylesheetParsesWithoutWarnings()
{
        for ( Mode mode : { Mode::Light, Mode::Dark } )
        {
                StyleWarnings warnings;

                Theme::instance().setMode( mode );
                Theme::instance().applyToApplication();

                // One of everything SPEC.md 8 lists, polished and painted
                QDialog dialog;
                auto* layout = new QVBoxLayout( &dialog );
                layout->addWidget( new QLabel( "Label" ) );
                auto* edit = new QLineEdit;
                edit->setProperty( "role", "ribbonField" );
                layout->addWidget( edit );
                layout->addWidget( new QSpinBox );
                auto* combo = new QComboBox;
                combo->addItems( { "One", "Two" } );
                layout->addWidget( combo );
                layout->addWidget( new QPlainTextEdit( "Text" ) );
                auto* check = new QCheckBox( "Check" );
                check->setChecked( true );
                layout->addWidget( check );
                auto* radio = new QRadioButton( "Radio" );
                radio->setChecked( true );
                layout->addWidget( radio );
                auto* group = new QGroupBox( "Card" );
                auto* groupLayout = new QVBoxLayout( group );
                groupLayout->addWidget( new QLabel( "In a card" ) );
                layout->addWidget( group );
                auto* tabs = new QTabWidget;
                tabs->addTab( new QWidget, "Tab one" );
                tabs->addTab( new QWidget, "Tab two" );
                layout->addWidget( tabs );
                auto* table = new QTableWidget( 3, 3 );
                table->setHorizontalHeaderLabels( { "A", "B", "C" } );
                layout->addWidget( table );
                layout->addWidget( new QScrollBar( Qt::Horizontal ) );
                layout->addWidget( new QSlider( Qt::Horizontal ) );
                auto* progress = new QProgressBar;
                progress->setValue( 42 );
                layout->addWidget( progress );
                for ( const char* role : { "ribbonLarge", "ribbonSmall", "ribbonIcon", "ribbonPrimary",
                                           "ribbonTab", "fileButton", "ribbonToggle" } )
                {
                        auto* button = new QToolButton;
                        button->setProperty( "role", role );
                        button->setText( role );
                        button->setIcon( Theme::instance().icon( "save" ) );
                        layout->addWidget( button );
                }
                auto* status = new QStatusBar;
                status->showMessage( "Ready" );
                layout->addWidget( status );
                auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
                layout->addWidget( buttons );

                QMenu menu;
                menu.addAction( "Cut" );
                menu.addSeparator();
                menu.addAction( "Paste" )->setCheckable( true );

                dialog.show();
                QVERIFY( QTest::qWaitForWindowExposed( &dialog ) );
                QImage image = dialog.grab().toImage();
                QVERIFY( !image.isNull() );
                QImage menuImage = menu.grab().toImage();
                QVERIFY( !menuImage.isNull() );

                QVERIFY2( StyleWarnings::messages().isEmpty(), qPrintable( StyleWarnings::messages().join( "\n" ) ) );
        }
}


void TestTheme::stylesheetCoversSpecSection8()
{
        QString css = Theme::instance().styleSheet();

        // SPEC.md 8: ribbon widgets, tab bar, buttons, tool buttons, combos, spins, line edits,
        // text edits, check boxes, radio buttons, group boxes, tables and headers, tabs, scroll
        // bars, menus, tooltips, dialogs, message boxes, the status bar, sliders.
        const QStringList selectors {
                "[role=\"ribbonBar\"]", "[role=\"ribbonTab\"]", "QPushButton", "QPushButton:default",
                "QToolButton", "QComboBox", "QAbstractSpinBox", "QLineEdit", "QPlainTextEdit", "QTextEdit",
                "QCheckBox::indicator", "QRadioButton::indicator", "QGroupBox", "QAbstractItemView",
                "QHeaderView::section", "QTabBar::tab", "QTabWidget::pane", "QScrollBar:vertical",
                "QScrollBar:horizontal", "QMenu", "QMenu::item", "QToolTip", "QDialog", "QMessageBox",
                "QStatusBar", "QSlider::groove:horizontal", "QSlider::handle:horizontal",
                ":focus", ":disabled", ":hover", ":checked"
        };
        for ( const QString& selector : selectors )
        {
                QVERIFY2( css.contains( selector ), qPrintable( selector ) );
        }

        // Fusion stays the base: no rule may replace the base style
        QVERIFY( !css.contains( "QStyle" ) );
}


void TestTheme::stylesheetMatchesEveryDocumentedRole()
{
        QFile file( QStringLiteral( GLABELS_SOURCE_DIR "/docs/ribbon-ui/QSS-HOOKS.md" ) );
        QVERIFY( file.open( QIODevice::ReadOnly | QIODevice::Text ) );
        QString doc = QString::fromUtf8( file.readAll() );

        static const QRegularExpression row( QStringLiteral( "^\\| `([A-Za-z]+)` \\|" ),
                                             QRegularExpression::MultilineOption );
        QStringList roles;
        auto it = row.globalMatch( doc );
        while ( it.hasNext() )
        {
                roles << it.next().captured( 1 );
        }
        QVERIFY( roles.size() > 40 );

        QString css = ThemeStyle::styleTemplate();
        QStringList missing;
        for ( const QString& role : roles )
        {
                if ( !css.contains( QStringLiteral( "[role=\"%1\"]" ).arg( role ) ) )
                {
                        missing << role;
                }
        }
        QVERIFY2( missing.isEmpty(), qPrintable( "no selector for: " + missing.join( ", " ) ) );
}


void TestTheme::stylesheetFollowsTheMode()
{
        QString light = ThemeStyle::generate( Theme::lightPalette(), Mode::Light );
        QString dark  = ThemeStyle::generate( Theme::darkPalette(), Mode::Dark );

        QVERIFY( light != dark );
        QVERIFY( light.contains( "#4f46e5" ) );
        QVERIFY( dark.contains( "#818cf8" ) );
        QVERIFY( dark.contains( "#090d1a" ) );

        // The gradient goes A > B > C at 135 degrees
        QVERIFY( light.contains( "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7c3aed, stop:0.5 #4f46e5, stop:1 #2563eb)" ) );
        QVERIFY( dark.contains( "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #a78bfa, stop:0.5 #818cf8, stop:1 #60a5fa)" ) );
}


void TestTheme::stylesheetImagesExist()
{
        for ( Mode mode : { Mode::Light, Mode::Dark } )
        {
                QString css = ThemeStyle::generate( Theme::paletteFor( mode ), mode );

                static const QRegularExpression url( QStringLiteral( "url\\(\"([^\"]+)\"\\)" ) );
                int count = 0;
                auto it = url.globalMatch( css );
                while ( it.hasNext() )
                {
                        QString path = it.next().captured( 1 );
                        QVERIFY2( QFile::exists( path ), qPrintable( path ) );
                        count++;
                }
                QVERIFY( count > 10 );
        }

        // Check mark on a checked box is drawn in onGrad
        QFile check( ThemeIcons::file( "check", QColor( "#ffffff" ) ) );
        QVERIFY( check.open( QIODevice::ReadOnly ) );
        QByteArray data = check.readAll();
        QVERIFY( data.contains( "#ffffff" ) );
        QVERIFY( !data.contains( "currentColor" ) );
}


void TestTheme::applyToApplicationSetsEverything()
{
        auto& theme = Theme::instance();
        QSignalSpy spy( &theme, &Theme::changed );

        theme.setMode( Mode::Dark );
        theme.applyToApplication();

        QCOMPARE( spy.count(), 1 );
        QCOMPARE( qApp->styleSheet(), theme.styleSheet() );
        QCOMPARE( QApplication::font().pixelSize(), 13 );

        theme.setMode( Mode::Light );
        theme.applyToApplication();
        QCOMPARE( spy.count(), 2 );
        QVERIFY( qApp->styleSheet().contains( "#4f46e5" ) );
}


void TestTheme::iconSetHasEverySpecIcon()
{
        QStringList missing;
        for ( const QString& name : specIcons() )
        {
                if ( !ThemeIcons::exists( name ) )
                {
                        missing << name;
                }
        }
        QVERIFY2( missing.isEmpty(), qPrintable( missing.join( ", " ) ) );

        QVERIFY( !ThemeIcons::exists( "no-such-icon" ) );
        QVERIFY( Theme::instance().icon( "no-such-icon" ).isNull() );
        QVERIFY( !Theme::instance().icon( "save" ).isNull() );
}


void TestTheme::iconsAreRecoloured()
{
        QByteArray svg = ThemeIcons::recolouredSvg( "save", QColor( 255, 0, 0 ) );
        QVERIFY( svg.contains( "#ff0000" ) );
        QVERIFY( !svg.contains( "currentColor" ) );
        QVERIFY( svg.contains( "stroke-width=\"1.75\"" ) );

        QImage image = ThemeIcons::render( "save", QColor( 255, 0, 0 ), QSize( 48, 48 ) );
        QCOMPARE( image.size(), QSize( 48, 48 ) );
        int painted = 0;
        QImage argb = image.convertToFormat( QImage::Format_ARGB32 );
        for ( int y = 0; y < argb.height(); y++ )
        {
                for ( int x = 0; x < argb.width(); x++ )
                {
                        QColor c = QColor::fromRgba( argb.pixel( x, y ) );
                        if ( c.alpha() > 0 )
                        {
                                painted++;
                                QCOMPARE( c.green(), 0 );
                                QCOMPARE( c.blue(), 0 );
                        }
                }
        }
        QVERIFY( painted > 100 );

        // Translucent colour: painted at that opacity
        QColor translucent( 0, 0, 255 );
        translucent.setAlphaF( 0.4 );
        QColor strongest = strongestPixel( ThemeIcons::render( "save", translucent, QSize( 48, 48 ) ) );
        QVERIFY( strongest.alpha() <= qRound( 0.4 * 255 ) + 1 );
        QVERIFY( strongest.alpha() >= qRound( 0.4 * 255 ) - 8 );

        // HiDPI: the engine renders at the device pixel ratio
        QPixmap pm = Theme::instance().icon( "save" ).pixmap( QSize( 16, 16 ), 2.0 );
        QCOMPARE( pm.size(), QSize( 32, 32 ) );
        QCOMPARE( pm.devicePixelRatio(), 2.0 );
}


void TestTheme::iconColourFollowsRoleAndState()
{
        Theme::instance().setMode( Mode::Light );
        const Palette& p = Theme::instance().palette();

        QCOMPARE( ThemeIcons::colorFor( IconRole::Normal, QIcon::Normal, QIcon::Off ), p.ink2 );
        QCOMPARE( ThemeIcons::colorFor( IconRole::Normal, QIcon::Active, QIcon::Off ), p.accent );
        QCOMPARE( ThemeIcons::colorFor( IconRole::Normal, QIcon::Normal, QIcon::On ), p.accent );
        QCOMPARE( ThemeIcons::colorFor( IconRole::Accent, QIcon::Normal, QIcon::Off ), p.accent );
        QCOMPARE( ThemeIcons::colorFor( IconRole::OnGradient, QIcon::Normal, QIcon::Off ), p.onGrad );
        QCOMPARE( ThemeIcons::colorFor( IconRole::OnGradient, QIcon::Active, QIcon::On ), p.onGrad );

        QColor disabled = ThemeIcons::colorFor( IconRole::Normal, QIcon::Disabled, QIcon::Off );
        QCOMPARE( disabled.rgb(), p.ink2.rgb() );
        QCOMPARE( disabled.alpha(), qRound( 0.4 * 255 ) );

        QColor disabledRole = ThemeIcons::colorFor( IconRole::Disabled, QIcon::Normal, QIcon::Off );
        QCOMPARE( disabledRole.rgb(), p.ink.rgb() );
        QCOMPARE( disabledRole.alpha(), qRound( 0.4 * 255 ) );

        // The pixels follow the colour
        QIcon icon = Theme::instance().icon( "save", IconRole::Accent );
        QColor drawn = strongestPixel( icon.pixmap( QSize( 24, 24 ), 1.0 ).toImage() );
        QVERIFY2( near( drawn, p.accent, 6 ), qPrintable( drawn.name() ) );
}


void TestTheme::iconFollowsThemeSwitch()
{
        QIcon icon = Theme::instance().icon( "save" );

        Theme::instance().setMode( Mode::Light );
        ThemeIcons::invalidate();
        QColor light = strongestPixel( icon.pixmap( QSize( 24, 24 ), 1.0 ).toImage() );

        Theme::instance().setMode( Mode::Dark );
        ThemeIcons::invalidate();
        QColor dark = strongestPixel( icon.pixmap( QSize( 24, 24 ), 1.0 ).toImage() );

        QVERIFY2( near( light, Theme::lightPalette().ink2, 6 ), qPrintable( light.name() ) );
        QVERIFY2( near( dark, Theme::darkPalette().ink2, 6 ), qPrintable( dark.name() ) );
}


void TestTheme::dialogFooterIsPainted()
{
        Theme::instance().setMode( Mode::Light );
        Theme::instance().applyToApplication();
        const Palette& p = Theme::instance().palette();

        QDialog dialog;
        auto* layout = new QVBoxLayout( &dialog );
        layout->addWidget( new QLabel( "Body text" ) );
        layout->addStretch();
        layout->addWidget( new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel ) );
        dialog.resize( 400, 240 );
        dialog.show();
        QVERIFY( QTest::qWaitForWindowExposed( &dialog ) );

        int top = Decorator::footerTop( &dialog );
        QVERIFY2( top > 120 && top < 220, qPrintable( QString::number( top ) ) );

        QImage image = dialog.grab().toImage();
        QColor footer = image.pixelColor( 4, dialog.height() - 3 );
        QColor body   = image.pixelColor( 4, 20 );
        QVERIFY2( near( footer, p.chrome ), qPrintable( footer.name() ) );
        QVERIFY2( near( body, p.panel ), qPrintable( body.name() ) );
        QVERIFY2( near( image.pixelColor( 4, top ), p.line ), qPrintable( image.pixelColor( 4, top ).name() ) );

        // No bottom button row: no footer
        QDialog plain;
        auto* plainLayout = new QVBoxLayout( &plain );
        plainLayout->addWidget( new QLabel( "Nothing to press" ) );
        plain.resize( 300, 200 );
        plain.show();
        QVERIFY( QTest::qWaitForWindowExposed( &plain ) );
        QCOMPARE( Decorator::footerTop( &plain ), -1 );

        // Opt-out
        dialog.setProperty( "plainFooter", true );
        QCOMPARE( Decorator::footerTop( &dialog ), -1 );
}


void TestTheme::dialogFooterNeverOverlapsContent()
{
        Theme::instance().setMode( Mode::Light );
        Theme::instance().applyToApplication();
        const Palette& p = Theme::instance().palette();

        // A tightly packed dialog whose content sits only 6 px above the button row
        // (Qt's default layout spacing): the footer hairline must still sit below the
        // content widget, never painted behind it.
        QDialog tight;
        auto* layout = new QVBoxLayout( &tight );
        layout->setSpacing( 6 );
        auto* content = new QFrame( &tight );
        content->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
        auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &tight );
        layout->addWidget( content, 1 );
        layout->addWidget( buttons );
        tight.resize( 400, 240 );
        tight.show();
        QVERIFY( QTest::qWaitForWindowExposed( &tight ) );

        const int contentBottom = content->mapTo( &tight, QPoint( 0, 0 ) ).y() + content->height();
        const int buttonTop = buttons->findChild<QPushButton*>()->mapTo( &tight, QPoint( 0, 0 ) ).y();
        const int top = Decorator::footerTop( &tight );
        QVERIFY2( top >= contentBottom,
                  qPrintable( QString( "footerTop %1 overlapped contentBottom %2" ).arg( top ).arg( contentBottom ) ) );
        QVERIFY2( top <= buttonTop,
                  qPrintable( QString( "footerTop %1 went below buttonTop %2" ).arg( top ).arg( buttonTop ) ) );

        QImage image = tight.grab().toImage();
        QVERIFY2( near( image.pixelColor( 4, top ), p.line ),
                  qPrintable( image.pixelColor( 4, top ).name() ) );
        QVERIFY2( near( image.pixelColor( 4, tight.height() - 3 ), p.chrome ),
                  qPrintable( image.pixelColor( 4, tight.height() - 3 ).name() ) );
}


void TestTheme::groupBoxIsACard()
{
        Theme::instance().setMode( Mode::Dark );
        Theme::instance().applyToApplication();
        const Palette& p = Theme::instance().palette();

        QWidget host;
        auto* hostLayout = new QVBoxLayout( &host );
        auto* box = new QGroupBox( "Appearance" );
        auto* layout = new QVBoxLayout( box );
        auto* label = new QLabel( "Inside" );
        layout->addWidget( label );
        hostLayout->addWidget( box );
        host.resize( 320, 200 );
        host.show();
        QVERIFY( QTest::qWaitForWindowExposed( &host ) );

        QImage image = box->grab().toImage();
        int mid = box->width() / 2;
        QVERIFY2( near( image.pixelColor( mid, 6 ), p.chrome ), qPrintable( image.pixelColor( mid, 6 ).name() ) );
        QVERIFY2( near( image.pixelColor( mid, Decorator::cardHeader ), p.line ), "header line" );
        QVERIFY2( near( image.pixelColor( mid, box->height() - 6 ), p.panel ), "card body" );

        // Content sits below the header band
        QVERIFY2( label->geometry().top() >= Decorator::cardHeader,
                  qPrintable( QString::number( label->geometry().top() ) ) );
}


namespace
{
        /// docs/ribbon-ui/THEMES.md, normative.  Order: bg chrome ribbon work panel line line2
        /// ink ink2 ink3 accent tint hover pressed ctx ctxLine ok okTint warn grad onGrad
        struct SchemeRow { const char* id; const char* light; const char* dark; };
        const SchemeRow schemeTable[] = {
                { "graphite",
                  "#F4F4F5 #EAEAEC #FFFFFF #DCDCDF #FFFFFF #E2E2E5 #B8B8BE #18181B #3F3F46 #5B5B64 #2F6DB5 #E4ECF6 #EDEDEF #DCE6F3 #E4ECF6 #2F6DB5 #15803D #E8F5EC #A15C07 #2F6DB5 #FFFFFF",
                  "#121214 #1A1A1D #202024 #0C0C0E #18181B #2C2C31 #494950 #F4F4F5 #D0D0D5 #9A9AA3 #6EA2E0 #26303D #2A2A2F #2E3A4B #26303D #6EA2E0 #4ADE80 #123221 #E0A93B #5B93D6 #0C0C0E" },
                { "paper",
                  "#FAF8F4 #F0EDE6 #FEFDFB #E5E0D6 #FEFDFB #E7E2D8 #BDB6A8 #1C1917 #44403C #6B655C #2F6B4F #E3EEE7 #F3F0E9 #DCEAE1 #E3EEE7 #2F6B4F #166534 #E6F2EA #9A5B0B #2F6B4F #FFFFFF",
                  "#141311 #1C1A17 #24211D #0E0D0B #1A1815 #2F2B25 #4D473E #F5F2EC #D6D0C5 #A39C8F #7CC29B #243229 #2B2823 #2D4034 #243229 #7CC29B #6EE7A0 #12301F #E3AE4F #7CC29B #0E0D0B" },
                { "harbor",
                  "#F3F6F8 #E8EEF2 #FFFFFF #D6DFE6 #FFFFFF #DCE4EA #AEBBC6 #0B1F2A #334A58 #55697A #0F766E #DDF0EE #EDF2F5 #CFE9E6 #DDF0EE #0F766E #15803D #E4F4EA #A15C07 #0F766E #FFFFFF",
                  "#0B141A #111D25 #17262F #070D11 #0F1A21 #1F313C #3A5160 #EAF2F6 #C3D3DC #8FA6B4 #4FD1C5 #12302F #1C2E39 #17403F #12302F #4FD1C5 #4ADE80 #0F2E1D #E5B04A #4FD1C5 #06161A" },
                { "signal",
                  "#FAFAF9 #F1F0EE #FFFFFF #E3E2DF #FFFFFF #E5E3DF #A8A59E #111111 #3A3936 #5C5A55 #C2410C #FDEBDD #F4F3F0 #FBDCC4 #FDEBDD #C2410C #15803D #E6F4EA #92400E #C2410C #FFFFFF",
                  "#0E0E0D #171716 #1F1F1D #080807 #141413 #2B2A28 #55534E #FAFAF9 #D6D4CF #A19E97 #FB923C #3A2412 #262523 #4A2C14 #3A2412 #FB923C #4ADE80 #122C1B #FBBF24 #FB923C #140A02" },
                { "system",
                  "#F3F3F3 #EBEBEB #FBFBFB #E0E0E0 #FFFFFF #E0E0E0 #B3B3B3 #1A1A1A #3D3D3D #5F5F5F #0F6CBD #E1EDF8 #F0F0F0 #D6E6F5 #E1EDF8 #0F6CBD #107C10 #E5F3E5 #8A5A00 #0F6CBD #FFFFFF",
                  "#202020 #2B2B2B #2F2F2F #181818 #272727 #3A3A3A #5C5C5C #FFFFFF #D6D6D6 #A6A6A6 #60CDFF #1F3A4D #353535 #1F4A66 #1F3A4D #60CDFF #6CCB5F #1B3A1B #E0B03A #60CDFF #00263A" },
        };


        QList<QColor> tokensOf( const Palette& p )
        {
                return { p.bg, p.chrome, p.ribbon, p.work, p.panel, p.line, p.line2, p.ink, p.ink2, p.ink3,
                         p.accent, p.tint, p.hover, p.pressed, p.ctx, p.ctxLine, p.ok, p.okTint, p.warn,
                         p.gradB, p.onGrad };
        }
}


void TestTheme::schemeRegistry()
{
        const auto& all = ThemeSchemes::all();
        QCOMPARE( all.size(), 6 );
        QStringList ids;
        for ( const auto& scheme : all )
        {
                ids << scheme.id;
                QVERIFY( !scheme.displayName().isEmpty() );
                QVERIFY( !scheme.displayDescription().isEmpty() );
                QVERIFY( !scheme.displayDescription().endsWith( '.' ) );
        }
        QCOMPARE( ids, QStringList( { "graphite", "neo", "paper", "harbor", "signal", "system" } ) );
        QCOMPARE( ThemeSchemes::defaultId(), QString( "graphite" ) );
        QCOMPARE( all.first().id, ThemeSchemes::defaultId() );

        // Neo stays available and unchanged
        const ThemeScheme& neo = ThemeSchemes::find( "neo" );
        QVERIFY( neo.gradient );
        QCOMPARE( tokensOf( neo.light ), tokensOf( Theme::lightPalette() ) );
        QCOMPARE( tokensOf( neo.dark ), tokensOf( Theme::darkPalette() ) );
        QCOMPARE( neo.light.gradA, QColor( "#7C3AED" ) );
        QCOMPARE( neo.light.gradC, QColor( "#2563EB" ) );

        // Unknown, empty or corrupt ids fall back to the default (Graphite); "neo" keeps Neo
        QCOMPARE( ThemeSchemes::find( "no-such-scheme" ).id, QString( "graphite" ) );
        QCOMPARE( ThemeSchemes::find( QString() ).id, QString( "graphite" ) );
        QCOMPARE( ThemeSchemes::find( "NEO\x01" ).id, QString( "graphite" ) );
        QVERIFY( !ThemeSchemes::contains( "no-such-scheme" ) );
        Theme::instance().setScheme( "no-such-scheme" );
        QCOMPARE( Theme::instance().scheme(), QString( "graphite" ) );
        Theme::instance().setScheme( "neo" );
        QCOMPARE( Theme::instance().scheme(), QString( "neo" ) );

        // The selected scheme drives palette() and paletteFor()
        Theme::instance().setScheme( "harbor" );
        Theme::instance().setMode( Mode::Dark );
        QCOMPARE( Theme::instance().palette().accent, QColor( "#4FD1C5" ) );
        QCOMPARE( Theme::paletteFor( Mode::Light ).accent, QColor( "#0F766E" ) );
}


void TestTheme::schemePalettesMatchTheTable_data()
{
        QTest::addColumn<QString>( "id" );
        QTest::addColumn<bool>( "dark" );
        QTest::addColumn<QString>( "hex" );
        for ( const auto& row : schemeTable )
        {
                QTest::addRow( "%s-light", row.id ) << QString( row.id ) << false << QString( row.light );
                QTest::addRow( "%s-dark", row.id ) << QString( row.id ) << true << QString( row.dark );
        }
}


void TestTheme::schemePalettesMatchTheTable()
{
        QFETCH( QString, id );
        QFETCH( bool, dark );
        QFETCH( QString, hex );

        QVERIFY( ThemeSchemes::contains( id ) );
        const ThemeScheme& scheme = ThemeSchemes::find( id );
        QVERIFY( !scheme.gradient );
        const Palette& p = scheme.palette( dark ? Mode::Dark : Mode::Light );

        const QStringList expected = hex.split( ' ' );
        const QList<QColor> actual = tokensOf( p );
        QCOMPARE( actual.size(), expected.size() );
        for ( int i = 0; i < expected.size(); i++ )
        {
                QVERIFY2( actual.at( i ) == QColor( expected.at( i ) ),
                          qPrintable( QString( "token %1: %2 != %3" ).arg( i ).arg( actual.at( i ).name(), expected.at( i ) ) ) );
        }
        // Solid schemes: one gradient colour
        QCOMPARE( p.gradA, p.gradB );
        QCOMPARE( p.gradC, p.gradB );
}


void TestTheme::schemeContrast_data()
{
        QTest::addColumn<QString>( "id" );
        QTest::addColumn<bool>( "dark" );
        for ( const auto& scheme : ThemeSchemes::all() )
        {
                QTest::addRow( "%s-light", qPrintable( scheme.id ) ) << scheme.id << false;
                QTest::addRow( "%s-dark", qPrintable( scheme.id ) ) << scheme.id << true;
        }
}


void TestTheme::schemeContrast()
{
        QFETCH( QString, id );
        QFETCH( bool, dark );
        const Palette& p = ThemeSchemes::find( id ).palette( dark ? Mode::Dark : Mode::Light );

        struct Rule { const char* name; QColor fg; QColor bg; double min; };
        QList<Rule> rules {
                { "ink/work",      p.ink,    p.work,   4.5 },   // work: 4.5 accepted (THEMES.md)
                { "ink2/bg",       p.ink2,   p.bg,     4.5 },
                { "ink2/chrome",   p.ink2,   p.chrome, 4.5 },
                { "ink2/ribbon",   p.ink2,   p.ribbon, 4.5 },
                { "ink2/panel",    p.ink2,   p.panel,  4.5 },
                { "ink3/bg",       p.ink3,   p.bg,     4.5 },
                { "ink3/ribbon",   p.ink3,   p.ribbon, 4.5 },
                { "ink3/panel",    p.ink3,   p.panel,  4.5 },
                { "onGrad/accent", p.onGrad, p.accent, 4.5 },
                { "onGrad/gradA",  p.onGrad, p.gradA,  4.5 },
                { "onGrad/gradC",  p.onGrad, p.gradC,  4.5 },
                { "accent/ribbon", p.accent, p.ribbon, 4.5 },
                { "accent/panel",  p.accent, p.panel,  4.5 },
        };
        // ink >= 7:1 on the main surfaces (Neo was specified before this rule; it passes too)
        for ( const QColor& surface : { p.bg, p.chrome, p.ribbon, p.panel } )
        {
                rules.append( { "ink/surface", p.ink, surface, 7.0 } );
        }

        for ( const Rule& rule : rules )
        {
                const double ratio = contrast( rule.fg, rule.bg );
                QVERIFY2( ratio >= rule.min,
                          qPrintable( QString( "%1 %2: %3 < %4" ).arg( id, rule.name ).arg( ratio, 0, 'f', 2 ).arg( rule.min ) ) );
        }
}


void TestTheme::schemeStylesheetsExpand()
{
        for ( const auto& scheme : ThemeSchemes::all() )
        {
                for ( Mode mode : { Mode::Light, Mode::Dark } )
                {
                        QStringList unknown;
                        QString css = ThemeStyle::expand( ThemeStyle::styleTemplate(),
                                                          ThemeStyle::tokens( scheme.palette( mode ), mode ), &unknown );
                        QVERIFY2( unknown.isEmpty(), qPrintable( scheme.id + ": " + unknown.join( ", " ) ) );
                        QVERIFY( css.contains( scheme.palette( mode ).accent.name() ) );
                }
        }
}


void TestTheme::schemeAppliesLive()
{
        StyleWarnings warnings;
        auto& theme = Theme::instance();
        QSignalSpy spy( &theme, &Theme::changed );

        QWidget probe;
        probe.show();
        QVERIFY( QTest::qWaitForWindowExposed( &probe ) );

        for ( const auto& scheme : ThemeSchemes::all() )
        {
                theme.setScheme( scheme.id );
                theme.setMode( Mode::Light );
                theme.applyToApplication();
                QVERIFY( qApp->styleSheet().contains( scheme.light.accent.name() ) );
                QCOMPARE( theme.palette().bg, scheme.light.bg );
        }
        QCOMPARE( spy.count(), ThemeSchemes::all().size() );
        QVERIFY2( StyleWarnings::messages().isEmpty(), qPrintable( StyleWarnings::messages().join( "\n" ) ) );
}
