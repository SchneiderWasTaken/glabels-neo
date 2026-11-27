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

#include "model/ColumnMatcher.hpp"
#include "model/FieldBinding.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QFileInfo>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
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


                /// A column of the linked data source: name, a sample value and where it is used.
                /// Drag it onto the label (a new text object, or the object under the pointer), or
                /// double-click to put it into the selected object.
                class ColumnChipRow : public QWidget
                {
                public:
                        ColumnChipRow( const QString& name,
                                       const QString& sample,
                                       const QString& useText,
                                       bool           isUsed,
                                       bool           draggable,
                                       std::function<void( const QString& )> onActivate,
                                       QWidget* parent = nullptr )
                                : QWidget( parent )
                                , mName( name )
                                , mSample( sample )
                                , mUseText( useText )
                                , mIsUsed( isUsed )
                                , mDraggable( draggable )
                                , mOnActivate( std::move( onActivate ) )
                        {
                                setFixedHeight( 44 );
                                setAttribute( Qt::WA_Hover, true );
                                setFocusPolicy( Qt::StrongFocus );
                                if ( mDraggable )
                                {
                                        setCursor( Qt::OpenHandCursor );
                                        setToolTip( QObject::tr( "Drag onto the label, or double-click to insert ${%1}" ).arg( name ) );
                                }
                                else
                                {
                                        setToolTip( useText );
                                }
                                setAccessibleName( QObject::tr( "Column %1, %2" ).arg( name, useText ) );
                        }

                protected:
                        void enterEvent( QEnterEvent* e ) override { mHovered = true; update(); QWidget::enterEvent( e ); }
                        void leaveEvent( QEvent* e ) override      { mHovered = false; update(); QWidget::leaveEvent( e ); }

                        void keyPressEvent( QKeyEvent* event ) override
                        {
                                if ( mDraggable && mOnActivate &&
                                     ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ) )
                                {
                                        mOnActivate( mName );
                                        return;
                                }
                                QWidget::keyPressEvent( event );
                        }

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
                                if ( mDraggable && ( event->buttons() & Qt::LeftButton ) &&
                                     ( event->pos() - mDragStart ).manhattanLength() >= QApplication::startDragDistance() )
                                {
                                        auto* drag = new QDrag( this );
                                        auto* mime = new QMimeData();
                                        mime->setData( ContentPanel::columnMimeType(), mName.toUtf8() );
                                        mime->setText( QStringLiteral( "${%1}" ).arg( mName ) );
                                        drag->setMimeData( mime );
                                        drag->exec( Qt::CopyAction );
                                        return;
                                }
                                QWidget::mouseMoveEvent( event );
                        }

                        void mouseDoubleClickEvent( QMouseEvent* event ) override
                        {
                                if ( mDraggable && event->button() == Qt::LeftButton && mOnActivate )
                                {
                                        mOnActivate( mName );
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

                                if ( mHovered || hasFocus() )
                                {
                                        p.fillRect( rect(), pal.hover );
                                }

                                // 6-dot grip indicator
                                if ( mDraggable )
                                {
                                        p.setPen( Qt::NoPen );
                                        p.setBrush( pal.ink3 );
                                        for ( int row = 0; row < 3; ++row )
                                        {
                                                const int dy = 16 + row * 5;
                                                p.drawEllipse( QPoint( 16, dy ), 1, 1 );
                                                p.drawEllipse( QPoint( 21, dy ), 1, 1 );
                                        }
                                }

                                const int textX = 32;

                                // Use text (right): accent when the label uses the column, muted otherwise
                                const QFont small = th.uiFont( 11, 500 );
                                const QFontMetrics fmSmall( small );
                                const int useW = qMin( fmSmall.horizontalAdvance( mUseText ) + 4, width() / 2 );
                                p.setFont( small );
                                p.setPen( mIsUsed ? pal.accent : pal.ink2 );
                                p.drawText( QRect( width() - 16 - useW, 4, useW, 20 ), Qt::AlignVCenter | Qt::AlignRight,
                                            fmSmall.elidedText( mUseText, Qt::ElideRight, useW ) );

                                // Column name
                                const QFont nameFont = th.uiFont( 12, 600 );
                                const QFontMetrics fmName( nameFont );
                                const int nameW = qMax( 0, width() - textX - 16 - useW - 8 );
                                p.setFont( nameFont );
                                p.setPen( pal.ink );
                                p.drawText( QRect( textX, 4, nameW, 20 ), Qt::AlignVCenter | Qt::AlignLeft,
                                            fmName.elidedText( mName, Qt::ElideRight, nameW ) );

                                // Sample value
                                if ( !mSample.isEmpty() )
                                {
                                        const QFont ui = th.uiFont( 11, 400 );
                                        const QFontMetrics fm( ui );
                                        p.setFont( ui );
                                        p.setPen( pal.ink2 );
                                        p.drawText( QRect( textX, 24, width() - textX - 16, 16 ), Qt::AlignVCenter | Qt::AlignLeft,
                                                    fm.elidedText( mSample, Qt::ElideRight, width() - textX - 16 ) );
                                }

                                // Bottom hairline
                                p.setPen( pal.line );
                                p.drawLine( 0, height() - 1, width(), height() - 1 );
                        }

                private:
                        QString                               mName;
                        QString                               mSample;
                        QString                               mUseText;
                        bool                                  mIsUsed{ false };
                        bool                                  mDraggable{ true };
                        std::function<void( const QString& )> mOnActivate;
                        QPoint                                mDragStart;
                        bool                                  mHovered{ false };
                };


                /// Stacked widget whose sizeHint tracks the current page rather than the tallest page,
                /// so the Data section sits right beneath the empty/shape captions without a tall gap.
                class FitStackedWidget : public QStackedWidget
                {
                public:
                        using QStackedWidget::QStackedWidget;

                        QSize sizeHint() const override
                        {
                                return currentWidget() ? currentWidget()->sizeHint() : QStackedWidget::sizeHint();
                        }

                        QSize minimumSizeHint() const override
                        {
                                return currentWidget() ? currentWidget()->minimumSizeHint() : QStackedWidget::minimumSizeHint();
                        }
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
                mScroll = new QScrollArea( this );
                auto* scroll = mScroll;
                scroll->setWidgetResizable( true );
                scroll->setFrameShape( QFrame::NoFrame );
                scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

                auto* inner = new QWidget( scroll );
                auto* innerLayout = new QVBoxLayout( inner );
                innerLayout->setContentsMargins( 0, 0, 0, 0 );
                innerLayout->setSpacing( 0 );

                // Stacked editor section
                mEditorStack = new FitStackedWidget( inner );
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

                // Data section: the linked source, its columns, and the fields that still need one
                mDataSection = new QWidget( inner );
                auto* dataLayout = new QVBoxLayout( mDataSection );
                dataLayout->setContentsMargins( 0, 0, 0, 0 );
                dataLayout->setSpacing( 0 );
                buildDataSection( mDataSection, dataLayout );
                innerLayout->addWidget( mDataSection );

                mDataTimer = new QTimer( this );
                mDataTimer->setSingleShot( true );
                mDataTimer->setInterval( 80 );
                connect( mDataTimer, &QTimer::timeout, this, &ContentPanel::rebuildDataSection );

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

                        connect( mModel, &model::Model::changed,
                                 this, &ContentPanel::scheduleDataRefresh );
                        connect( mModel, &model::Model::sourceDataChanged,
                                 this, &ContentPanel::scheduleDataRefresh );
                        connect( mModel, &model::Model::sourceDataChanged,
                                 this, &ContentPanel::syncFieldButtons );
                        connect( mModel, &model::Model::externalDataSourceChanged,
                                 this, &ContentPanel::scheduleDataRefresh );
                        connect( mModel, &model::Model::fillRowsChanged,
                                 this, &ContentPanel::scheduleDataRefresh );
                }
                refreshFieldsAndVariables();
                mDataSignature.clear();
                rebuildDataSection();
        }


        QString ContentPanel::columnMimeType()
        {
                return QStringLiteral( "application/x-glabels-column" );
        }


        QString ContentPanel::blankColumnToken()
        {
                return QStringLiteral( "__glabels_blank__" );
        }


        void ContentPanel::setSourceStatus( const QString& title, const QString& status, bool isError, bool isBusy )
        {
                mSourceTitle  = title;
                mSourceStatus = status;
                mSourceError  = isError;
                mSourceBusy   = isBusy;
                scheduleDataRefresh();
        }


        void ContentPanel::revealDataSection()
        {
                if ( mScroll && mDataSection )
                {
                        mScroll->ensureWidgetVisible( mDataSection, 0, 8 );
                }
        }


        void ContentPanel::scheduleDataRefresh()
        {
                if ( mDataTimer )
                {
                        mDataTimer->start();
                }
        }


        ///
        /// The static parts of the Data section.  The column and field rows are rebuilt by
        /// rebuildDataSection() whenever the source, the label or the column choices change.
        ///
        void ContentPanel::buildDataSection( QWidget* parent, QVBoxLayout* into )
        {
                into->addWidget( makeSectionHeader( tr( "Data" ), &mDataCaption, 40, false, parent ) );

                auto* body = new QWidget( parent );
                auto* bl = new QVBoxLayout( body );
                bl->setContentsMargins( 16, 10, 16, 10 );
                bl->setSpacing( 8 );

                // No source yet
                mDataEmptyBox = new QWidget( body );
                auto* el = new QVBoxLayout( mDataEmptyBox );
                el->setContentsMargins( 0, 0, 0, 0 );
                el->setSpacing( 8 );
                auto* emptyCaption = new QLabel(
                        tr( "Fill labels from a Google Sheet or a Redash query. Its columns appear here: drag one onto the label to place it." ),
                        mDataEmptyBox );
                emptyCaption->setWordWrap( true );
                emptyCaption->setProperty( "role", QStringLiteral( "caption" ) );
                el->addWidget( emptyCaption );
                auto* connectRow = new QHBoxLayout();
                connectRow->setSpacing( 8 );
                auto* sheetsBtn = new QPushButton( tr( "Google Sheets..." ), mDataEmptyBox );
                auto* redashBtn = new QPushButton( tr( "Redash..." ), mDataEmptyBox );
                connectRow->addWidget( sheetsBtn );
                connectRow->addWidget( redashBtn );
                connectRow->addStretch( 1 );
                el->addLayout( connectRow );
                connect( sheetsBtn, &QPushButton::clicked, this,
                         [this]() { emit connectSourceRequested( model::ExternalSourceKind::GoogleSheets ); } );
                connect( redashBtn, &QPushButton::clicked, this,
                         [this]() { emit connectSourceRequested( model::ExternalSourceKind::Redash ); } );
                bl->addWidget( mDataEmptyBox );

                // A source is linked
                mDataSourceBox = new QWidget( body );
                auto* sl = new QVBoxLayout( mDataSourceBox );
                sl->setContentsMargins( 0, 0, 0, 0 );
                sl->setSpacing( 4 );
                mDataTitleLabel = new QLabel( mDataSourceBox );
                mDataTitleLabel->setProperty( "role", QStringLiteral( "fieldLabel" ) );
                mDataTitleLabel->setWordWrap( true );
                mDataStatusLabel = new QLabel( mDataSourceBox );
                mDataStatusLabel->setProperty( "role", QStringLiteral( "caption" ) );
                mDataStatusLabel->setWordWrap( true );
                sl->addWidget( mDataTitleLabel );
                sl->addWidget( mDataStatusLabel );
                auto* btnRow = new QHBoxLayout();
                btnRow->setSpacing( 8 );
                mDataRefreshBtn = new QPushButton( tr( "Refresh" ), mDataSourceBox );
                mDataChangeBtn  = new QPushButton( tr( "Change..." ), mDataSourceBox );
                btnRow->addWidget( mDataRefreshBtn );
                btnRow->addWidget( mDataChangeBtn );
                btnRow->addStretch( 1 );
                sl->addLayout( btnRow );
                connect( mDataRefreshBtn, &QPushButton::clicked, this, &ContentPanel::refreshSourceRequested );
                connect( mDataChangeBtn, &QPushButton::clicked, this, [this]()
                {
                        const auto kind = mModel ? mModel->externalDataSource().kind
                                                 : model::ExternalSourceKind::GoogleSheets;
                        emit connectSourceRequested( kind );
                } );
                bl->addWidget( mDataSourceBox );

                // Show a row's data on the label
                mRowNavBox = new QWidget( body );
                auto* rl = new QHBoxLayout( mRowNavBox );
                rl->setContentsMargins( 0, 0, 0, 0 );
                rl->setSpacing( 6 );
                mShowRowCheck = new QCheckBox( tr( "Show row data" ), mRowNavBox );
                mShowRowCheck->setToolTip( tr( "Show the values of one row on the label instead of ${field} placeholders" ) );
                mRowPrevBtn = new QToolButton( mRowNavBox );
                mRowPrevBtn->setText( QStringLiteral( "‹" ) );
                mRowPrevBtn->setAccessibleName( tr( "Previous row" ) );
                mRowNextBtn = new QToolButton( mRowNavBox );
                mRowNextBtn->setText( QStringLiteral( "›" ) );
                mRowNextBtn->setAccessibleName( tr( "Next row" ) );
                mRowLabel = new QLabel( mRowNavBox );
                mRowLabel->setProperty( "role", QStringLiteral( "caption" ) );
                rl->addWidget( mShowRowCheck );
                rl->addStretch( 1 );
                rl->addWidget( mRowPrevBtn );
                rl->addWidget( mRowLabel );
                rl->addWidget( mRowNextBtn );
                bl->addWidget( mRowNavBox );
                connect( mShowRowCheck, &QCheckBox::toggled, this, [this]( bool on )
                {
                        updateRowNav();
                        emit showRowDataToggled( on );
                        emit previewRowChanged( mPreviewRow );
                } );
                connect( mRowPrevBtn, &QToolButton::clicked, this, [this]() { stepPreviewRow( -1 ); } );
                connect( mRowNextBtn, &QToolButton::clicked, this, [this]() { stepPreviewRow( +1 ); } );

                into->addWidget( body );

                // Columns
                mColumnsHeader = new QLabel( tr( "Columns" ), parent );
                mColumnsHeader->setProperty( "role", QStringLiteral( "caption" ) );
                mColumnsHeader->setContentsMargins( 16, 6, 16, 4 );
                into->addWidget( mColumnsHeader );

                mColumnsHost = new QWidget( parent );
                auto* cl = new QVBoxLayout( mColumnsHost );
                cl->setContentsMargins( 0, 0, 0, 0 );
                cl->setSpacing( 0 );
                into->addWidget( mColumnsHost );

                // Fields that still need a column
                mNeedsHeader = new QLabel( parent );
                mNeedsHeader->setProperty( "role", QStringLiteral( "caption" ) );
                mNeedsHeader->setContentsMargins( 16, 10, 16, 4 );
                into->addWidget( mNeedsHeader );

                mNeedsHost = new QWidget( parent );
                auto* nl = new QVBoxLayout( mNeedsHost );
                nl->setContentsMargins( 16, 0, 16, 8 );
                nl->setSpacing( 6 );
                into->addWidget( mNeedsHost );
        }


        void ContentPanel::stepPreviewRow( int delta )
        {
                const int n = mModel ? static_cast<int>( mModel->fillRows().size() ) : 0;
                if ( n <= 0 )
                {
                        return;
                }
                mPreviewRow = qBound( 0, mPreviewRow + delta, n - 1 );
                updateRowNav();
                emit previewRowChanged( mPreviewRow );
        }


        void ContentPanel::updateRowNav()
        {
                const int n = mModel ? static_cast<int>( mModel->fillRows().size() ) : 0;
                if ( n > 0 )
                {
                        mPreviewRow = qBound( 0, mPreviewRow, n - 1 );
                }
                const bool on = mShowRowCheck && mShowRowCheck->isChecked();
                mShowRowCheck->setEnabled( n > 0 );
                mRowPrevBtn->setEnabled( on && n > 0 && mPreviewRow > 0 );
                mRowNextBtn->setEnabled( on && n > 0 && mPreviewRow < n - 1 );
                mRowLabel->setText( n > 0 ? tr( "Row %1 of %2" ).arg( mPreviewRow + 1 ).arg( n ) : tr( "No rows" ) );
                mRowLabel->setEnabled( on && n > 0 );
        }


        ///
        /// Rebuild the source summary, the column list and the "Needs a column" rows.
        /// Skipped when nothing they show has changed, so an open drop-down is never pulled away.
        ///
        void ContentPanel::rebuildDataSection()
        {
                if ( !mDataSection )
                {
                        return;
                }

                const bool hasSource = mModel && mModel->externalDataSource().isValid();
                const QStringList columns = mModel ? mModel->sourceColumns() : QStringList();
                const QList<model::FieldBinding> bindings = mModel ? model::FieldBindings::compute( *mModel, columns )
                                                                   : QList<model::FieldBinding>();
                const int nRows = mModel ? static_cast<int>( mModel->fillRows().size() ) : 0;

                // What the section shows, as one string
                QString sig;
                sig += hasSource ? QStringLiteral( "S|" ) : QStringLiteral( "-|" );
                sig += mSourceTitle + QLatin1Char( '|' ) + mSourceStatus + QLatin1Char( '|' )
                       + ( mSourceError ? QLatin1Char( 'E' ) : QLatin1Char( 'o' ) )
                       + ( mSourceBusy ? QLatin1Char( 'B' ) : QLatin1Char( 'o' ) ) + QLatin1Char( '|' );
                sig += columns.join( QLatin1Char( '\x1f' ) ) + QLatin1Char( '|' );
                if ( !columns.isEmpty() && mModel && !mModel->sourceRows().isEmpty() )
                {
                        const auto& first = mModel->sourceRows().first();
                        for ( const QString& c : columns )
                        {
                                sig += first.value( c ) + QLatin1Char( '\x1f' );
                        }
                }
                for ( const auto& b : bindings )
                {
                        sig += b.field + QLatin1Char( ':' ) + QString::number( static_cast<int>( b.match.kind ) )
                               + b.match.column + QLatin1Char( b.isComputed ? 'c' : 'v' )
                               + QLatin1Char( b.isConstant ? 'k' : 'v' ) + QLatin1Char( ';' );
                }
                sig += QString::number( nRows );
                if ( sig == mDataSignature )
                {
                        return;
                }
                mDataSignature = sig;

                // Source summary
                mDataEmptyBox->setVisible( !hasSource );
                mDataSourceBox->setVisible( hasSource );
                mDataTitleLabel->setText( mSourceTitle.isEmpty() && mModel ? mModel->externalDataSource().summaryText()
                                                                           : mSourceTitle );
                mDataStatusLabel->setText( mSourceStatus );
                mDataStatusLabel->setStyleSheet( mSourceError
                                                 ? QStringLiteral( "color: %1;" ).arg( theme::Theme::instance().palette().warn.name() )
                                                 : QString() );
                mDataStatusLabel->setVisible( !mSourceStatus.isEmpty() );
                mDataRefreshBtn->setEnabled( !mSourceBusy );
                mDataCaption->setText( nRows > 0 ? tr( "%n row(s)", nullptr, nRows ) : QString() );

                mRowNavBox->setVisible( nRows > 0 );
                updateRowNav();

                // Clear the dynamic rows
                auto clearHost = []( QWidget* host )
                {
                        auto* l = qobject_cast<QVBoxLayout*>( host->layout() );
                        while ( l && l->count() > 0 )
                        {
                                QLayoutItem* item = l->takeAt( 0 );
                                if ( QWidget* w = item->widget() )
                                {
                                        w->hide();
                                        w->deleteLater();
                                }
                                delete item;
                        }
                };
                clearHost( mColumnsHost );
                clearHost( mNeedsHost );

                // Columns: which label fields each one fills
                QMap<QString,QStringList> fieldsByColumn;
                for ( const auto& b : bindings )
                {
                        if ( b.match.isApplied() )
                        {
                                fieldsByColumn[ b.match.column ].append( b.field );
                        }
                }

                const QMap<QString,QString> firstRow = ( mModel && !mModel->sourceRows().isEmpty() )
                                                               ? mModel->sourceRows().first()
                                                               : QMap<QString,QString>();

                auto* cl = qobject_cast<QVBoxLayout*>( mColumnsHost->layout() );
                int nShown = 0;
                for ( const QString& col : columns )
                {
                        if ( col.trimmed().isEmpty() )
                        {
                                continue;
                        }
                        ++nShown;

                        QString useText;
                        bool    used = false;
                        bool    draggable = true;
                        if ( model::ColumnMatcher::isControlColumn( col ) )
                        {
                                useText   = col.compare( QStringLiteral( "Qty" ), Qt::CaseInsensitive ) == 0
                                                    ? tr( "Quantity" ) : tr( "Print flag" );
                                draggable = false;
                        }
                        else if ( col.contains( QLatin1Char( '}' ) ) || col.contains( QLatin1Char( ':' ) ) )
                        {
                                // ${...} cannot refer to a name with these characters
                                useText   = tr( "Rename to use" );
                                draggable = false;
                        }
                        else if ( fieldsByColumn.contains( col ) )
                        {
                                const QStringList& fs = fieldsByColumn[ col ];
                                used    = true;
                                useText = ( fs.size() == 1 && fs.first() == col ) ? tr( "On label" )
                                                                                  : tr( "Fills ${%1}" ).arg( fs.join( QStringLiteral( "}, ${" ) ) );
                        }
                        else
                        {
                                useText = tr( "Not on label" );
                        }

                        cl->addWidget( new ColumnChipRow( col, firstRow.value( col ), useText, used, draggable,
                                                          [this]( const QString& k ) { emit fieldActivated( k ); },
                                                          mColumnsHost ) );
                }
                mColumnsHeader->setVisible( nShown > 0 );
                mColumnsHeader->setText( tr( "Columns · drag onto the label" ) );
                mColumnsHost->setVisible( nShown > 0 );

                // Fields that still need a column
                const QList<model::FieldBinding> needs = model::FieldBindings::needingColumn( bindings );
                const bool showNeeds = hasSource && !columns.isEmpty() && !needs.isEmpty();
                mNeedsHeader->setVisible( showNeeds );
                mNeedsHost->setVisible( showNeeds );
                if ( showNeeds )
                {
                        mNeedsHeader->setText( tr( "Needs a column (%1)" ).arg( needs.size() ) );
                        auto* nl = qobject_cast<QVBoxLayout*>( mNeedsHost->layout() );
                        for ( const auto& b : needs )
                        {
                                auto* row = new QWidget( mNeedsHost );
                                auto* rowL = new QVBoxLayout( row );
                                rowL->setContentsMargins( 0, 0, 0, 0 );
                                rowL->setSpacing( 4 );

                                auto* line = new QHBoxLayout();
                                line->setSpacing( 8 );
                                auto* chip = new QLabel( QStringLiteral( "${%1}" ).arg( b.field ), row );
                                chip->setFont( theme::Theme::instance().monoFont( 12 ) );
                                chip->setStyleSheet( QStringLiteral( "QLabel { color: %1; background: %2; border-radius: 6px; padding: 3px 8px; }" )
                                                     .arg( theme::Theme::instance().palette().accent.name(),
                                                           theme::Theme::instance().palette().tint.name() ) );
                                chip->setMaximumWidth( 120 );
                                chip->setToolTip( QStringLiteral( "${%1}" ).arg( b.field ) );
                                line->addWidget( chip );

                                auto* combo = new QComboBox( row );
                                combo->setAccessibleName( tr( "Column for %1" ).arg( b.field ) );
                                combo->addItem( tr( "Choose column..." ), QString() );
                                for ( const QString& col : columns )
                                {
                                        if ( !col.trimmed().isEmpty() )
                                        {
                                                combo->addItem( col, col );
                                        }
                                }
                                combo->insertSeparator( combo->count() );
                                combo->addItem( tr( "Leave blank" ), blankColumnToken() );
                                combo->setSizeAdjustPolicy( QComboBox::AdjustToMinimumContentsLengthWithIcon );
                                combo->setMinimumContentsLength( 8 );
                                line->addWidget( combo, 1 );
                                rowL->addLayout( line );

                                const QString field = b.field;
                                connect( combo, QOverload<int>::of( &QComboBox::activated ), this, [this, combo, field]( int )
                                {
                                        const QString col = combo->currentData().toString();
                                        if ( !col.isEmpty() )
                                        {
                                                emit columnChosen( field, col );
                                        }
                                } );

                                if ( b.match.kind == model::ColumnMatch::Kind::Suggested )
                                {
                                        auto* use = new QPushButton( tr( "Use “%1”" ).arg( b.match.column ), row );
                                        use->setFlat( true );
                                        use->setCursor( Qt::PointingHandCursor );
                                        use->setToolTip( tr( "This column looks like a match for %1" ).arg( b.field ) );
                                        use->setStyleSheet( QStringLiteral( "QPushButton { color: %1; text-align: left; border: none; padding: 0 2px; }" )
                                                            .arg( theme::Theme::instance().palette().accent.name() ) );
                                        const QString suggested = b.match.column;
                                        connect( use, &QPushButton::clicked, this, [this, field, suggested]()
                                        {
                                                emit columnChosen( field, suggested );
                                        } );
                                        rowL->addWidget( use, 0, Qt::AlignLeft );
                                }
                                nl->addWidget( row );
                        }
                }
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
                bool hasSourceCol = false;
                if ( mModel )
                {
                        for ( const QString& c : mModel->sourceColumns() )
                        {
                                if ( !c.trimmed().isEmpty() &&
                                     !model::ColumnMatcher::isControlColumn( c ) &&
                                     !c.contains( QLatin1Char( '}' ) ) &&
                                     !c.contains( QLatin1Char( ':' ) ) )
                                {
                                        hasSourceCol = true;
                                        break;
                                }
                        }
                }
                const bool hasAny = mModel && ( ( mModel->merge() && !mModel->merge()->keys().isEmpty() ) ||
                                                !mModel->constVariables().isEmpty() ||
                                                hasSourceCol );
                const QString tipDisabled = tr( "No merge fields, data columns or variables" );
                const QString tipEnabled  = tr( "Insert a field, data column or variable" );

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
                mDataSignature.clear();
                rebuildDataSection();
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
