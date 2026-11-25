//  StartupView.cpp
//
//  Copyright (C) 2016-2026  Jaye Evins <evins@snaught.com>
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


#include "StartupView.hpp"

#include "File.hpp"
#include "MainWindow.hpp"
#include "theme/Theme.hpp"

#include "model/Branding.hpp"
#include "model/Settings.hpp"

#include <QAbstractButton>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QScrollArea>


namespace glabels
{

        namespace
        {
                /// Welcome command-link card (560px wide, 44x44 tint icon chip, title + subtitle).
                class WelcomeCommandCard : public QAbstractButton
                {
                public:
                        WelcomeCommandCard( const QString& title,
                                            const QString& subtitle,
                                            const QString& iconName,
                                            QWidget*       parent = nullptr )
                                : QAbstractButton( parent )
                                , mTitle( title )
                                , mSubtitle( subtitle )
                                , mIconName( iconName )
                        {
                                setAccessibleName( title );
                                setFocusPolicy( Qt::TabFocus );
                                setAttribute( Qt::WA_Hover, true );
                                setCursor( Qt::PointingHandCursor );
                                setFixedSize( 560, 78 );
                                setProperty( "role", QStringLiteral( "commandLink" ) );
                        }

                protected:
                        void enterEvent( QEnterEvent* e ) override { mHovered = true; update(); QAbstractButton::enterEvent( e ); }
                        void leaveEvent( QEvent* e ) override      { mHovered = false; update(); QAbstractButton::leaveEvent( e ); }

                        void paintEvent( QPaintEvent* ) override
                        {
                                QPainter p( this );
                                p.setRenderHint( QPainter::Antialiasing, true );

                                const auto& th  = theme::Theme::instance();
                                const auto& pal = th.palette();

                                const QRectF r = QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );
                                p.setPen( QPen( hasFocus() ? pal.accent : pal.line, hasFocus() ? 2.0 : 1.0 ) );
                                p.setBrush( mHovered ? pal.hover : pal.panel );
                                p.drawRoundedRect( r, 12.0, 12.0 );

                                const QRectF chip( 20, ( height() - 44 ) / 2.0, 44, 44 );
                                p.setPen( Qt::NoPen );
                                p.setBrush( pal.tint );
                                p.drawRoundedRect( chip, 10.0, 10.0 );

                                const QIcon ic = th.icon( mIconName, theme::IconRole::Accent );
                                if ( !ic.isNull() )
                                {
                                        ic.paint( &p, QRect( 31, ( height() - 22 ) / 2, 22, 22 ) );
                                }

                                p.setFont( th.uiFont( 15, 600 ) );
                                p.setPen( pal.ink );
                                p.drawText( QRect( 80, 17, width() - 100, 22 ),
                                            Qt::AlignVCenter | Qt::AlignLeft, mTitle );

                                p.setFont( th.uiFont( 12, 400 ) );
                                p.setPen( pal.ink3 );
                                p.drawText( QRect( 80, 40, width() - 100, 20 ),
                                            Qt::AlignVCenter | Qt::AlignLeft, mSubtitle );
                        }

                private:
                        QString mTitle;
                        QString mSubtitle;
                        QString mIconName;
                        bool    mHovered{ false };
                };


                /// Non-clickable header for the Recent card (icon chip + title + subtitle).
                class RecentCardHeader : public QWidget
                {
                public:
                        explicit RecentCardHeader( QWidget* parent = nullptr )
                                : QWidget( parent )
                        {
                                setFixedHeight( 64 );
                        }

                protected:
                        void paintEvent( QPaintEvent* ) override
                        {
                                QPainter p( this );
                                p.setRenderHint( QPainter::Antialiasing, true );

                                const auto& th  = theme::Theme::instance();
                                const auto& pal = th.palette();

                                const QRectF chip( 20, ( height() - 44 ) / 2.0, 44, 44 );
                                p.setPen( Qt::NoPen );
                                p.setBrush( pal.tint );
                                p.drawRoundedRect( chip, 10.0, 10.0 );

                                const QIcon ic = th.icon( QStringLiteral( "refresh-cw" ), theme::IconRole::Accent );
                                if ( !ic.isNull() )
                                {
                                        ic.paint( &p, QRect( 31, ( height() - 22 ) / 2, 22, 22 ) );
                                }

                                p.setFont( th.uiFont( 15, 600 ) );
                                p.setPen( pal.ink );
                                p.drawText( QRect( 80, 11, width() - 100, 22 ),
                                            Qt::AlignVCenter | Qt::AlignLeft, QObject::tr( "Recent" ) );

                                p.setFont( th.uiFont( 12, 400 ) );
                                p.setPen( pal.ink3 );
                                p.drawText( QRect( 80, 34, width() - 100, 20 ),
                                            Qt::AlignVCenter | Qt::AlignLeft,
                                            QObject::tr( "Open a recent gLabels project" ) );
                        }
                };


                /// Clickable recent file row inside the Welcome Recent card.
                class WelcomeRecentRow : public QAbstractButton
                {
                public:
                        WelcomeRecentRow( const QString& filePath, QWidget* parent = nullptr )
                                : QAbstractButton( parent )
                                , mFilePath( filePath )
                        {
                                const QFileInfo fi( filePath );
                                mFileName   = fi.fileName();
                                mFolderPath = fi.absolutePath();
                                setAccessibleName( mFileName );
                                setToolTip( filePath );
                                setFocusPolicy( Qt::TabFocus );
                                setAttribute( Qt::WA_Hover, true );
                                setCursor( Qt::PointingHandCursor );
                                setFixedHeight( 38 );
                        }

                        QString filePath() const { return mFilePath; }

                protected:
                        void enterEvent( QEnterEvent* e ) override { mHovered = true; update(); QAbstractButton::enterEvent( e ); }
                        void leaveEvent( QEvent* e ) override      { mHovered = false; update(); QAbstractButton::leaveEvent( e ); }

                        void paintEvent( QPaintEvent* ) override
                        {
                                QPainter p( this );
                                p.setRenderHint( QPainter::Antialiasing, true );

                                const auto& th  = theme::Theme::instance();
                                const auto& pal = th.palette();

                                if ( mHovered )
                                {
                                        p.fillRect( rect().adjusted( 1, 1, -1, 0 ), pal.hover );
                                }

                                p.setPen( pal.line );
                                p.drawLine( 20, 0, width() - 20, 0 );

                                const QIcon ic = th.icon( QStringLiteral( "file" ), theme::IconRole::Normal );
                                if ( !ic.isNull() )
                                {
                                        ic.paint( &p, QRect( 24, ( height() - 16 ) / 2, 16, 16 ) );
                                }

                                p.setFont( th.uiFont( 12, 400 ) );
                                const QFontMetrics fmFolder( p.font() );
                                const int folderW = qMin( fmFolder.horizontalAdvance( mFolderPath ), width() / 2 );
                                p.setPen( pal.ink3 );
                                p.drawText( QRect( width() - 20 - folderW, 0, folderW, height() ),
                                            Qt::AlignVCenter | Qt::AlignRight,
                                            fmFolder.elidedText( mFolderPath, Qt::ElideMiddle, folderW ) );

                                p.setFont( th.uiFont( 13, 400 ) );
                                const QFontMetrics fmName( p.font() );
                                const int nameW = qMax( 0, width() - 48 - folderW - 28 );
                                p.setPen( pal.ink );
                                p.drawText( QRect( 48, 0, nameW, height() ),
                                            Qt::AlignVCenter | Qt::AlignLeft,
                                            fmName.elidedText( mFileName, Qt::ElideRight, nameW ) );
                        }

                private:
                        QString mFilePath;
                        QString mFileName;
                        QString mFolderPath;
                        bool    mHovered{ false };
                };
        }


        ///
        /// Constructor
        ///
        StartupView::StartupView( MainWindow* window )
                : QWidget( window )
                , mWindow( window )
        {
                setProperty( "role", QStringLiteral( "welcome" ) );

                auto* outer = new QVBoxLayout( this );
                outer->setContentsMargins( 0, 0, 0, 0 );
                outer->setSpacing( 0 );

                auto* scroll = new QScrollArea( this );
                scroll->setWidgetResizable( true );
                scroll->setFrameShape( QFrame::NoFrame );

                auto* content = new QWidget( scroll );
                auto* layout = new QVBoxLayout( content );
                layout->setContentsMargins( 24, 44, 24, 32 );
                layout->setSpacing( 12 );
                layout->setAlignment( Qt::AlignHCenter | Qt::AlignTop );

                // Brand mark (brand/mark.svg, 72x72)
                mMarkLabel = new QLabel( content );
                mMarkLabel->setAlignment( Qt::AlignCenter );
                QIcon markIcon( QStringLiteral( ":/brand/mark.svg" ) );
                if ( markIcon.isNull() )
                {
                        markIcon = QIcon::fromTheme( QStringLiteral( "glabels" ) );
                }
                mMarkLabel->setPixmap( markIcon.pixmap( 72, 72 ) );
                layout->addWidget( mMarkLabel, 0, Qt::AlignHCenter );

                mHeadingLabel = new QLabel(
                        tr( "Welcome to %1" ).arg( model::Branding::DISPLAY_NAME ), content );
                mHeadingLabel->setAlignment( Qt::AlignCenter );
                layout->addWidget( mHeadingLabel, 0, Qt::AlignHCenter );

                mAttributionLabel = new QLabel(
                        QStringLiteral( "%1 · %2" )
                                .arg( model::Branding::ATTRIBUTION_SHORT, tr( "Let's get started:" ) ),
                        content );
                mAttributionLabel->setAlignment( Qt::AlignCenter );
                mAttributionLabel->setProperty( "role", QStringLiteral( "caption" ) );
                layout->addWidget( mAttributionLabel, 0, Qt::AlignHCenter );

                layout->addSpacing( 12 );

                auto* newCard = new WelcomeCommandCard(
                        tr( "New..." ),
                        tr( "Create a new blank gLabels project" ),
                        QStringLiteral( "file-plus" ),
                        content );
                connect( newCard, &QAbstractButton::clicked,
                         this, &StartupView::onNewProjectButtonClicked );
                layout->addWidget( newCard, 0, Qt::AlignHCenter );

                auto* openCard = new WelcomeCommandCard(
                        tr( "Browse..." ),
                        tr( "Open an existing gLabels project" ),
                        QStringLiteral( "folder-open" ),
                        content );
                connect( openCard, &QAbstractButton::clicked,
                         this, &StartupView::onOpenProjectButtonClicked );
                layout->addWidget( openCard, 0, Qt::AlignHCenter );

                mRecentCard = new QWidget( content );
                mRecentCard->setFixedWidth( 560 );
                mRecentCard->setProperty( "role", QStringLiteral( "card" ) );
                auto* recentLayout = new QVBoxLayout( mRecentCard );
                recentLayout->setContentsMargins( 0, 8, 0, 8 );
                recentLayout->setSpacing( 0 );
                recentLayout->addWidget( new RecentCardHeader( mRecentCard ) );

                mRecentRowsLayout = new QVBoxLayout();
                mRecentRowsLayout->setContentsMargins( 0, 0, 0, 0 );
                mRecentRowsLayout->setSpacing( 0 );
                recentLayout->addLayout( mRecentRowsLayout );

                layout->addWidget( mRecentCard, 0, Qt::AlignHCenter );
                layout->addStretch( 1 );

                scroll->setWidget( content );
                outer->addWidget( scroll );

                loadRecentsList();

                connect( model::Settings::instance(), SIGNAL(changed()), this, SLOT(onSettingsChanged()) );
                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &StartupView::onThemeChanged );
                onThemeChanged();
        }


        void StartupView::onNewProjectButtonClicked()
        {
                File::newLabel( mWindow );
        }


        void StartupView::onOpenProjectButtonClicked()
        {
                File::open( mWindow );
        }


        void StartupView::onSettingsChanged()
        {
                loadRecentsList();
        }


        void StartupView::onThemeChanged()
        {
                const auto& th  = theme::Theme::instance();
                const auto& pal = th.palette();

                mHeadingLabel->setFont( th.uiFont( 24, 700 ) );
                QPalette hp = mHeadingLabel->palette();
                hp.setColor( QPalette::WindowText, pal.ink );
                mHeadingLabel->setPalette( hp );

                mAttributionLabel->setFont( th.uiFont( 12, 400 ) );
                QPalette ap = mAttributionLabel->palette();
                ap.setColor( QPalette::WindowText, pal.ink3 );
                mAttributionLabel->setPalette( ap );

                mRecentCard->setStyleSheet(
                        QStringLiteral( "QWidget[role=\"card\"] { background: %1; border: 1px solid %2; border-radius: 12px; }" )
                                .arg( pal.panel.name(), pal.line.name() ) );
                update();
        }


        void StartupView::loadRecentsList()
        {
                while ( QLayoutItem* item = mRecentRowsLayout->takeAt( 0 ) )
                {
                        if ( QWidget* w = item->widget() )
                        {
                                w->deleteLater();
                        }
                        delete item;
                }

                const QStringList fileList = model::Settings::recentFileList();
                if ( fileList.isEmpty() )
                {
                        auto* emptyLbl = new QLabel( tr( "No recent projects." ), mRecentCard );
                        emptyLbl->setContentsMargins( 24, 8, 24, 8 );
                        emptyLbl->setProperty( "role", QStringLiteral( "caption" ) );
                        mRecentRowsLayout->addWidget( emptyLbl );
                        return;
                }

                for ( const QString& filename : fileList )
                {
                        auto* row = new WelcomeRecentRow( filename, mRecentCard );
                        connect( row, &QAbstractButton::clicked, this, [this, filename]()
                        {
                                File::open( filename, mWindow );
                        } );
                        mRecentRowsLayout->addWidget( row );
                }
        }


        void StartupView::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                p.fillRect( rect(), theme::Theme::instance().palette().bg );
        }

} // namespace glabels
