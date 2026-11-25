//  ContentPanel.cpp
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


#include "ContentPanel.hpp"

#include "RibbonDropButton.hpp"
#include "theme/Theme.hpp"

#include <QApplication>
#include <QDrag>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>


namespace glabels::ribbon
{

        namespace
        {
                enum EditorPageIndex
                {
                        PAGE_EMPTY   = 0,
                        PAGE_TEXT    = 1,
                        PAGE_BARCODE = 2,
                        PAGE_IMAGE   = 3,
                        PAGE_SHAPE   = 4,
                        PAGE_LINE    = 5
                };

                /// Draggable, double-clickable field/variable row: grip + mono ${key} chip + preview value.
                class FieldChipRow : public QWidget
                {
                public:
                        FieldChipRow( const QString& key,
                                      const QString& previewValue,
                                      std::function<void( const QString& )> onActivate,
                                      QWidget* parent = nullptr )
                                : QWidget( parent )
                                , mKey( key )
                                , mPreviewValue( previewValue )
                                , mOnActivate( std::move( onActivate ) )
                        {
                                setFixedHeight( 34 );
                                setCursor( Qt::OpenHandCursor );
                                setAttribute( Qt::WA_Hover, true );
                                setToolTip( QObject::tr( "Double-click or drag to insert ${%1}" ).arg( key ) );
                        }

                protected:
                        void enterEvent( QEnterEvent* e ) override { mHovered = true; update(); QWidget::enterEvent( e ); }
                        void leaveEvent( QEvent* e ) override      { mHovered = false; update(); QWidget::leaveEvent( e ); }

                        void mousePressEvent( QMouseEvent* event ) override
                        {
                                if ( event->button() == Qt::LeftButton )
                                {
                                        mDragStart = event->pos();
                                }
                                QWidget::mousePressEvent( event );
                        }

                        void mouseMoveEvent( QMouseEvent* event ) override
                        {
                                if ( ( event->buttons() & Qt::LeftButton ) &&
                                     ( event->pos() - mDragStart ).manhattanLength() >= QApplication::startDragDistance() )
                                {
                                        auto* drag = new QDrag( this );
                                        auto* mime = new QMimeData();
                                        mime->setText( QStringLiteral( "${%1}" ).arg( mKey ) );
                                        drag->setMimeData( mime );
                                        drag->exec( Qt::CopyAction );
                                        return;
                                }
                                QWidget::mouseMoveEvent( event );
                        }

                        void mouseDoubleClickEvent( QMouseEvent* event ) override
                        {
                                if ( event->button() == Qt::LeftButton && mOnActivate )
                                {
                                        mOnActivate( mKey );
                                        event->accept();
                                        return;
                                }
                                QWidget::mouseDoubleClickEvent( event );
                        }

                        void paintEvent( QPaintEvent* ) override
                        {
                                QPainter p( this );
                                p.setRenderHint( QPainter::Antialiasing, true );

                                const auto& th  = theme::Theme::instance();
                                const auto& pal = th.palette();

                                if ( mHovered )
                                {
                                        p.fillRect( rect(), pal.hover );
                                }

                                // 6-dot grip indicator
                                p.setPen( Qt::NoPen );
                                p.setBrush( pal.ink3 );
                                for ( int row = 0; row < 3; ++row )
                                {
                                        const int dy = 11 + row * 5;
                                        p.drawEllipse( QPoint( 16, dy ), 1, 1 );
                                        p.drawEllipse( QPoint( 21, dy ), 1, 1 );
                                }

                                // Mono chip "${key}"
                                const QString token = QStringLiteral( "${%1}" ).arg( mKey );
                                const QFont mono = th.monoFont( 12 );
                                const QFontMetrics fmMono( mono );
                                const int chipW = qMin( fmMono.horizontalAdvance( token ) + 14, width() - 100 );
                                const QRectF chipRect( 32, ( height() - 22 ) / 2.0, chipW, 22 );
                                p.setBrush( pal.tint );
                                p.drawRoundedRect( chipRect, 6.0, 6.0 );

                                p.setFont( mono );
                                p.setPen( pal.accent );
                                p.drawText( chipRect, Qt::AlignCenter,
                                            fmMono.elidedText( token, Qt::ElideRight, chipW - 8 ) );

                                // Value preview on right
                                if ( !mPreviewValue.isEmpty() )
                                {
                                        const QFont ui = th.uiFont( 12, 400 );
                                        const QFontMetrics fmUi( ui );
                                        const int valX = static_cast<int>( chipRect.right() ) + 8;
                                        const int valW = qMax( 0, width() - valX - 16 );
                                        p.setFont( ui );
                                        p.setPen( pal.ink2 );
                                        p.drawText( QRect( valX, 0, valW, height() ),
                                                    Qt::AlignVCenter | Qt::AlignRight,
                                                    fmUi.elidedText( mPreviewValue, Qt::ElideRight, valW ) );
                                }

                                // Bottom hairline
                                p.setPen( pal.line );
                                p.drawLine( 0, height() - 1, width(), height() - 1 );
                        }

                private:
                        QString                               mKey;
                        QString                               mPreviewValue;
                        std::function<void( const QString& )> mOnActivate;
                        QPoint                                mDragStart;
                        bool                                  mHovered{ false };
                };


                QWidget* makeSectionHeader( const QString& title,
                                            QLabel**       rightLabelOut,
                                            int            height,
                                            bool           mutedTitle,
                                            QWidget*       parent )
                {
                        auto* hdr = new QWidget( parent );
                        hdr->setFixedHeight( height );
                        hdr->setProperty( "role", QStringLiteral( "contentPanelHeader" ) );

                        auto* hl = new QHBoxLayout( hdr );
                        hl->setContentsMargins( 16, 0, 16, 0 );
                        hl->setSpacing( 8 );

                        auto* titleLbl = new QLabel( title, hdr );
                        titleLbl->setProperty( "role", mutedTitle ? QStringLiteral( "caption" )
                                                                  : QStringLiteral( "cardTitle" ) );
                        hl->addWidget( titleLbl, 0 );
                        hl->addStretch( 1 );

                        if ( rightLabelOut )
                        {
                                auto* rightLbl = new QLabel( hdr );
                                rightLbl->setProperty( "role", QStringLiteral( "caption" ) );
                                hl->addWidget( rightLbl, 0 );
                                *rightLabelOut = rightLbl;
                        }
                        return hdr;
                }
        }


        ContentPanel::ContentPanel( QWidget* parent )
                : QWidget( parent )
        {
                setFixedWidth( theme::Metrics::contentPanel );
                setProperty( "role", QStringLiteral( "contentPanel" ) );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( 1, 0, 0, 0 ); // 1px left border painted in paintEvent
                root->setSpacing( 0 );

                // Top header: "Content" + selected type
                root->addWidget( makeSectionHeader( tr( "Content" ), &mTypeCaption, 40, false, this ) );

                // Scrollable body
                auto* scroll = new QScrollArea( this );
                scroll->setWidgetResizable( true );
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

                auto* inner = new QWidget( scroll );
                auto* innerLayout = new QVBoxLayout( inner );
                innerLayout->setContentsMargins( 0, 0, 0, 0 );
                innerLayout->setSpacing( 0 );

                // Stacked editor section
                mEditorStack = new QStackedWidget( inner );
                mEditorStack->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );

                // Page 0: Empty / Multiple selection
                auto* emptyPage = new QWidget( mEditorStack );
                auto* emptyL = new QVBoxLayout( emptyPage );
                emptyL->setContentsMargins( 16, 12, 16, 12 );
                auto* emptyCaption = new QLabel(
                        tr( "Select an object to edit its content. Several objects at once: use the Arrange commands on the Home tab." ),
                        emptyPage );
                emptyCaption->setWordWrap( true );
                emptyCaption->setProperty( "role", QStringLiteral( "caption" ) );
                emptyL->addWidget( emptyCaption );
                emptyL->addStretch( 1 );   // caption at the top (word-wrapped labels centre otherwise)
                mEditorStack->addWidget( emptyPage );

                // Page 1: Text (populated in installControls)
                mTextPage = new QWidget( mEditorStack );
                mEditorStack->addWidget( mTextPage );

                // Page 2: Barcode (populated in installControls)
                mBarcodePage = new QWidget( mEditorStack );
                mEditorStack->addWidget( mBarcodePage );

                // Page 3: Image (populated in installControls)
                mImagePage = new QWidget( mEditorStack );
                mEditorStack->addWidget( mImagePage );

                // Page 4: Shape (box / ellipse)
                auto* shapePage = new QWidget( mEditorStack );
                auto* shapeL = new QVBoxLayout( shapePage );
                shapeL->setContentsMargins( 16, 12, 16, 12 );
                auto* shapeCaption = new QLabel(
                        tr( "Boxes and ellipses have no content. Edit line, fill and shadow on the Shape tab." ),
                        shapePage );
                shapeCaption->setWordWrap( true );
                shapeCaption->setProperty( "role", QStringLiteral( "caption" ) );
                shapeL->addWidget( shapeCaption );
                shapeL->addStretch( 1 );   // caption at the top (word-wrapped labels centre otherwise)
                mEditorStack->addWidget( shapePage );

                // Page 5: Line
                auto* linePage = new QWidget( mEditorStack );
                auto* lineL = new QVBoxLayout( linePage );
                lineL->setContentsMargins( 16, 12, 16, 12 );
                auto* lineCaption = new QLabel(
                        tr( "Lines have no content. Edit width, color and size on the Line tab." ),
                        linePage );
                lineCaption->setWordWrap( true );
                lineCaption->setProperty( "role", QStringLiteral( "caption" ) );
                lineL->addWidget( lineCaption );
                lineL->addStretch( 1 );   // caption at the top (word-wrapped labels centre otherwise)
                mEditorStack->addWidget( linePage );

                innerLayout->addWidget( mEditorStack );

                // A stacked widget is as tall as its tallest page; size it to the current page so
                // that short pages sit at the top and the Fields list follows directly (s03..s08).
                auto fitStackToPage = [this]( int current )
                {
                        for ( int i = 0; i < mEditorStack->count(); ++i )
                        {
                                mEditorStack->widget( i )->setSizePolicy( QSizePolicy::Preferred,
                                                                          i == current ? QSizePolicy::Preferred
                                                                                       : QSizePolicy::Ignored );
                        }
                        mEditorStack->updateGeometry();
                };
                connect( mEditorStack, &QStackedWidget::currentChanged, this, fitStackToPage );
                fitStackToPage( mEditorStack->currentIndex() );

                // Fields section (hidden when merge has no keys)
                mFieldsSection = new QWidget( inner );
                auto* fSecLayout = new QVBoxLayout( mFieldsSection );
                fSecLayout->setContentsMargins( 0, 0, 0, 0 );
                fSecLayout->setSpacing( 0 );
                fSecLayout->addWidget( makeSectionHeader( tr( "Fields" ), &mFieldsSourceLabel, 40, false, mFieldsSection ) );

                mFieldsListHost = new QWidget( mFieldsSection );
                auto* fl = new QVBoxLayout( mFieldsListHost );
                fl->setContentsMargins( 0, 0, 0, 0 );
                fl->setSpacing( 0 );
                fSecLayout->addWidget( mFieldsListHost );
                mFieldsSection->hide();
                innerLayout->addWidget( mFieldsSection );

                // Variables section (hidden when empty)
                mVariablesSection = new QWidget( inner );
                auto* vSecLayout = new QVBoxLayout( mVariablesSection );
                vSecLayout->setContentsMargins( 0, 0, 0, 0 );
                vSecLayout->setSpacing( 0 );
                vSecLayout->addWidget( makeSectionHeader( tr( "Variables" ), nullptr, 32, true, mVariablesSection ) );

                mVariablesListHost = new QWidget( mVariablesSection );
                auto* vl = new QVBoxLayout( mVariablesListHost );
                vl->setContentsMargins( 0, 0, 0, 0 );
                vl->setSpacing( 0 );
                vSecLayout->addWidget( mVariablesListHost );
                mVariablesSection->hide();
                innerLayout->addWidget( mVariablesSection );

                innerLayout->addStretch( 1 );
                scroll->setWidget( inner );
                root->addWidget( scroll, 1 );

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &ContentPanel::onThemeChanged );
                onThemeChanged();
        }


        void ContentPanel::installControls( const TransplantedControls& controls )
        {
                // Page 1: Text
                {
                        auto* l = new QVBoxLayout( mTextPage );
                        l->setContentsMargins( 16, 12, 16, 12 );
                        l->setSpacing( 8 );

                        auto* lbl = new QLabel( tr( "Text" ), mTextPage );
                        lbl->setProperty( "role", QStringLiteral( "fieldLabel" ) );
                        l->addWidget( lbl );

                        if ( controls.textEdit )
                        {
                                controls.textEdit->setParent( mTextPage );
                                controls.textEdit->setMinimumHeight( 88 );
                                controls.textEdit->setMaximumHeight( 160 );
                                controls.textEdit->show();
                                l->addWidget( controls.textEdit );
                        }

                        auto* btnRow = new QHBoxLayout();
                        btnRow->setSpacing( 8 );
                        if ( controls.textInsertFieldButton )
                        {
                                controls.textInsertFieldButton->setParent( mTextPage );
                                controls.textInsertFieldButton->hide();
                                mTextFieldSource = qobject_cast<QPushButton*>( controls.textInsertFieldButton );
                        }
                        mTextFieldBtn = new RibbonDropButton(
                                tr( "Insert field" ), QStringLiteral( "braces" ),
                                RibbonButton::Size::Small, mTextPage );
                        if ( mTextFieldSource )
                        {
                                mTextFieldBtn->setMenu( mTextFieldSource->menu() );
                        }
                        btnRow->addWidget( mTextFieldBtn, 0, Qt::AlignVCenter );

                        auto* hint = new QLabel(
                                tr( "Plain text. Use\u00A0${field} for merge data." ), mTextPage );
                        hint->setWordWrap( true );
                        hint->setProperty( "role", QStringLiteral( "caption" ) );
                        btnRow->addWidget( hint, 1 );
                        l->addLayout( btnRow );
                }

                // Page 2: Barcode
                {
                        auto* l = new QVBoxLayout( mBarcodePage );
                        l->setContentsMargins( 16, 12, 16, 12 );
                        l->setSpacing( 8 );

                        auto* lbl = new QLabel( tr( "Barcode data" ), mBarcodePage );
                        lbl->setProperty( "role", QStringLiteral( "fieldLabel" ) );
                        l->addWidget( lbl );

                        if ( controls.barcodeDataEdit )
                        {
                                controls.barcodeDataEdit->setParent( mBarcodePage );
                                controls.barcodeDataEdit->setMinimumHeight( 88 );
                                controls.barcodeDataEdit->setMaximumHeight( 140 );
                                controls.barcodeDataEdit->show();
                                l->addWidget( controls.barcodeDataEdit );
                        }

                        if ( controls.barcodeInsertFieldButton )
                        {
                                controls.barcodeInsertFieldButton->setParent( mBarcodePage );
                                controls.barcodeInsertFieldButton->hide();
                                mBarcodeFieldSource = qobject_cast<QPushButton*>( controls.barcodeInsertFieldButton );
                        }
                        mBarcodeFieldBtn = new RibbonDropButton(
                                tr( "Insert field" ), QStringLiteral( "braces" ),
                                RibbonButton::Size::Small, mBarcodePage );
                        if ( mBarcodeFieldSource )
                        {
                                mBarcodeFieldBtn->setMenu( mBarcodeFieldSource->menu() );
                        }
                        l->addWidget( mBarcodeFieldBtn, 0, Qt::AlignLeft );
                }

                // Page 3: Image
                {
                        auto* l = new QVBoxLayout( mImagePage );
                        l->setContentsMargins( 16, 12, 16, 12 );
                        l->setSpacing( 8 );

                        auto* lbl = new QLabel( tr( "Image file" ), mImagePage );
                        lbl->setProperty( "role", QStringLiteral( "fieldLabel" ) );
                        l->addWidget( lbl );

                        mImageFileMirror = new QLineEdit( mImagePage );
                        mImageFileMirror->setReadOnly( true );
                        mImageFileMirror->setPlaceholderText( tr( "None" ) );
                        mImageFileMirror->setFixedHeight( theme::Metrics::field );
                        if ( controls.imageFilenameSource )
                        {
                                mImageFileMirror->setText( controls.imageFilenameSource->text() );
                                connect( controls.imageFilenameSource, &QLineEdit::textChanged,
                                         mImageFileMirror, &QLineEdit::setText );
                        }
                        l->addWidget( mImageFileMirror );

                        auto* row = new QHBoxLayout();
                        row->setSpacing( 8 );

                        auto* browseBtn = new QPushButton( tr( "Browse..." ), mImagePage );
                        if ( controls.imageBrowseSource )
                        {
                                connect( browseBtn, &QPushButton::clicked,
                                         controls.imageBrowseSource, &QPushButton::click );
                        }
                        row->addWidget( browseBtn );

                        if ( controls.imageFieldButton )
                        {
                                controls.imageFieldButton->setParent( mImagePage );
                                controls.imageFieldButton->hide();
                                mImageFieldSource = qobject_cast<QPushButton*>( controls.imageFieldButton );
                        }
                        mImageFieldBtn = new RibbonDropButton(
                                tr( "Use merge field" ), QStringLiteral( "braces" ),
                                RibbonButton::Size::Small, mImagePage );
                        if ( mImageFieldSource )
                        {
                                mImageFieldBtn->setMenu( mImageFieldSource->menu() );
                        }
                        row->addWidget( mImageFieldBtn );
                        row->addStretch( 1 );
                        l->addLayout( row );
                }

                syncFieldButtons();
        }


        void ContentPanel::setModel( model::Model* model )
        {
                if ( mModel )
                {
                        disconnect( mModel, nullptr, this, nullptr );
                }
                mModel = model;
                if ( mModel )
                {
                        connect( mModel, &model::Model::mergeChanged,
                                 this, &ContentPanel::refreshFieldsAndVariables );
                        connect( mModel, &model::Model::mergeSourceChanged,
                                 this, &ContentPanel::refreshFieldsAndVariables );
                        connect( mModel, &model::Model::variablesChanged,
                                 this, &ContentPanel::refreshFieldsAndVariables );
                }
                refreshFieldsAndVariables();
        }


        void ContentPanel::setSelectionKind( SelectionKind kind )
        {
                mSelectionKind = kind;
                switch ( kind )
                {
                case SelectionKind::Text:
                        mTypeCaption->setText( tr( "Text" ) );
                        mEditorStack->setCurrentIndex( PAGE_TEXT );
                        break;
                case SelectionKind::Barcode:
                        mTypeCaption->setText( tr( "Barcode" ) );
                        mEditorStack->setCurrentIndex( PAGE_BARCODE );
                        break;
                case SelectionKind::Image:
                        mTypeCaption->setText( tr( "Image" ) );
                        mEditorStack->setCurrentIndex( PAGE_IMAGE );
                        break;
                case SelectionKind::Box:
                case SelectionKind::Ellipse:
                        mTypeCaption->setText( tr( "Shape" ) );
                        mEditorStack->setCurrentIndex( PAGE_SHAPE );
                        break;
                case SelectionKind::Line:
                        mTypeCaption->setText( tr( "Line" ) );
                        mEditorStack->setCurrentIndex( PAGE_LINE );
                        break;
                case SelectionKind::None:
                case SelectionKind::Multiple:
                default:
                        mTypeCaption->clear();
                        mEditorStack->setCurrentIndex( PAGE_EMPTY );
                        break;
                }
        }


        void ContentPanel::refreshFieldsAndVariables()
        {
                // Clear existing rows
                auto clearHost = []( QWidget* host )
                {
                        auto* l = qobject_cast<QVBoxLayout*>( host->layout() );
                        if ( !l )
                        {
                                return;
                        }
                        while ( QLayoutItem* item = l->takeAt( 0 ) )
                        {
                                if ( QWidget* w = item->widget() )
                                {
                                        w->hide();   // deleteLater() only runs on return to the event loop
                                        w->deleteLater();
                                }
                                delete item;
                        }
                };

                clearHost( mFieldsListHost );
                clearHost( mVariablesListHost );

                if ( !mModel )
                {
                        mFieldsSection->hide();
                        mVariablesSection->hide();
                        syncFieldButtons();
                        return;
                }

                // Merge fields
                const merge::Merge* merge = mModel->merge();
                const QStringList keys = merge ? merge->keys() : QStringList();
                if ( keys.isEmpty() )
                {
                        mFieldsSection->hide();
                }
                else
                {
                        mFieldsSourceLabel->setText( QFileInfo( merge->source() ).fileName() );
                        const auto& records = merge->recordList();
                        const merge::Record* firstRec = records.isEmpty() ? nullptr : &records.first();

                        auto* fl = qobject_cast<QVBoxLayout*>( mFieldsListHost->layout() );
                        for ( const QString& key : keys )
                        {
                                const QString previewVal = firstRec ? firstRec->value( key ) : QString();
                                auto* row = new FieldChipRow(
                                        key, previewVal,
                                        [this]( const QString& k ) { emit fieldActivated( k ); },
                                        mFieldsListHost );
                                fl->addWidget( row );
                        }
                        mFieldsSection->show();
                }

                // Variables
                const auto& vars = mModel->constVariables();
                if ( vars.isEmpty() )
                {
                        mVariablesSection->hide();
                }
                else
                {
                        auto* vl = qobject_cast<QVBoxLayout*>( mVariablesListHost->layout() );
                        for ( const auto& var : vars )
                        {
                                QString info = var.initialValue();
                                if ( var.increment() != model::Variable::Increment::NEVER )
                                {
                                        info += QStringLiteral( " · " ) +
                                                model::Variable::incrementToI18nString( var.increment() ).toLower();
                                }
                                auto* row = new FieldChipRow(
                                        var.name(), info,
                                        [this]( const QString& k ) { emit fieldActivated( k ); },
                                        mVariablesListHost );
                                vl->addWidget( row );
                        }
                        mVariablesSection->show();
                }

                syncFieldButtons();
        }


        void ContentPanel::syncFieldButtons()
        {
                const bool hasAny = mModel && ( ( mModel->merge() && !mModel->merge()->keys().isEmpty() ) ||
                                                !mModel->constVariables().isEmpty() );
                const QString tipDisabled = tr( "No merge fields or variables" );
                const QString tipEnabled  = tr( "Insert a merge field or variable" );

                auto apply = [&]( RibbonDropButton* btn, QPushButton* src )
                {
                        if ( !btn )
                        {
                                return;
                        }
                        const bool en = src ? ( src->isEnabled() || hasAny ) : hasAny;
                        btn->setEnabled( en );
                        btn->setToolTip( en ? tipEnabled : tipDisabled );
                };

                apply( mTextFieldBtn, mTextFieldSource );
                apply( mBarcodeFieldBtn, mBarcodeFieldSource );
                apply( mImageFieldBtn, mImageFieldSource );
        }


        void ContentPanel::onThemeChanged()
        {
                const auto& th  = theme::Theme::instance();
                const auto& pal = th.palette();

                setStyleSheet(
                        QStringLiteral(
                                "QWidget[role=\"contentPanelHeader\"] { border-top: 1px solid %1; border-bottom: 1px solid %1; }" )
                                .arg( pal.line.name() ) );
                update();
        }


        void ContentPanel::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                const auto& pal = theme::Theme::instance().palette();
                p.fillRect( rect(), pal.panel );
                p.setPen( pal.line );
                p.drawLine( 0, 0, 0, height() );
        }

} // namespace glabels::ribbon
