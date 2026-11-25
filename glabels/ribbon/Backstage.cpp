//  Backstage.cpp
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


#include "Backstage.hpp"

#include "RibbonTypes.hpp"
#include "theme/Theme.hpp"

#include <QAbstractButton>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>


namespace glabels::ribbon
{

        namespace
        {
                /// Left-nav row inside the Backstage.
                class BackstageNavButton : public QAbstractButton
                {
                public:
                        BackstageNavButton( const QString& label,
                                            const QString& iconName,
                                            QAction*       action,
                                            bool           activeStyle = false,
                                            QWidget*       parent      = nullptr )
                                : QAbstractButton( parent )
                                , mIconName( iconName )
                                , mAction( action )
                                , mActiveStyle( activeStyle )
                        {
                                setText( label );
                                setAccessibleName( label );
                                setFocusPolicy( Qt::TabFocus );
                                setAttribute( Qt::WA_Hover, true );
                                setCursor( Qt::PointingHandCursor );
                                setMinimumHeight( 38 );
                                setProperty( "role", QStringLiteral( "backstageNavItem" ) );

                                if ( mAction )
                                {
                                        connect( mAction, &QAction::changed, this, &BackstageNavButton::syncAction );
                                        syncAction();
                                }
                        }

                        void setActiveStyle( bool active )
                        {
                                mActiveStyle = active;
                                update();
                        }

                        QSize sizeHint() const override
                        {
                                return QSize( 224, 38 );
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

                                if ( !isEnabled() )
                                {
                                        p.setOpacity( 0.40 );
                                }

                                const QRectF r = QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );
                                if ( mActiveStyle )
                                {
                                        p.setPen( Qt::NoPen );
                                        p.setBrush( pal.pressed );
                                        p.drawRoundedRect( r, 8.0, 8.0 );
                                }
                                else if ( isEnabled() && mHovered )
                                {
                                        p.setPen( Qt::NoPen );
                                        p.setBrush( pal.hover );
                                        p.drawRoundedRect( r, 8.0, 8.0 );
                                }

                                if ( hasFocus() )
                                {
                                        p.setPen( QPen( pal.accent, 2.0 ) );
                                        p.setBrush( Qt::NoBrush );
                                        p.drawRoundedRect( r.adjusted( 1.0, 1.0, -1.0, -1.0 ), 8.0, 8.0 );
                                }

                                int textX = 14;
                                if ( !mIconName.isEmpty() )
                                {
                                        const QIcon ic = th.icon( mIconName,
                                                                  mActiveStyle ? theme::IconRole::Accent
                                                                               : theme::IconRole::Normal );
                                        if ( !ic.isNull() )
                                        {
                                                ic.paint( &p, QRect( 14, ( height() - 16 ) / 2, 16, 16 ) );
                                        }
                                        textX = 42;
                                }

                                QString shortcut;
                                if ( mAction && !mAction->shortcut().isEmpty() )
                                {
                                        shortcut = mAction->shortcut().toString( QKeySequence::NativeText );
                                }

                                int shortcutW = 0;
                                if ( !shortcut.isEmpty() )
                                {
                                        p.setFont( th.uiFont( 12, 400 ) );
                                        p.setPen( pal.ink3 );
                                        shortcutW = QFontMetrics( p.font() ).horizontalAdvance( shortcut ) + 8;
                                        p.drawText( QRect( width() - 14 - shortcutW, 0, shortcutW, height() ),
                                                    Qt::AlignVCenter | Qt::AlignRight, shortcut );
                                }

                                p.setFont( th.uiFont( 14, mActiveStyle ? 600 : 400 ) );
                                p.setPen( mActiveStyle ? pal.accent : ( mIconName.isEmpty() ? pal.ink2 : pal.ink ) );
                                p.drawText( QRect( textX, 0, width() - textX - 14 - shortcutW, height() ),
                                            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, text() );
                        }

                private:
                        void syncAction()
                        {
                                if ( mAction )
                                {
                                        setEnabled( mAction->isEnabled() );
                                        setToolTip( formatActionTooltip( mAction ) );
                                        update();
                                }
                        }

                        QString  mIconName;
                        QAction* mAction{ nullptr };
                        bool     mActiveStyle{ false };
                        bool     mHovered{ false };
                };


                /// Command link card ("Browse...") inside the right pane.
                class BackstageCommandLink : public QAbstractButton
                {
                public:
                        BackstageCommandLink( const QString& title,
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

                                // Icon chip (44x44)
                                const QRectF chip( 20, ( height() - 44 ) / 2.0, 44, 44 );
                                p.setPen( Qt::NoPen );
                                p.setBrush( pal.tint );
                                p.drawRoundedRect( chip, 10.0, 10.0 );

                                const QIcon ic = th.icon( mIconName, theme::IconRole::Accent );
                                if ( !ic.isNull() )
                                {
                                        ic.paint( &p, QRect( 31, ( height() - 22 ) / 2, 22, 22 ) );
                                }

                                p.setFont( th.uiFont( 14, 600 ) );
                                p.setPen( pal.ink );
                                p.drawText( QRect( 80, 18, width() - 100, 22 ),
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


                /// Recent file row inside the Recent card.
                class RecentFileRow : public QAbstractButton
                {
                public:
                        RecentFileRow( QAction* action, bool drawBottomLine, QWidget* parent = nullptr )
                                : QAbstractButton( parent )
                                , mAction( action )
                                , mDrawBottomLine( drawBottomLine )
                        {
                                const QString fullPath = action ? action->data().toString() : QString();
                                const QFileInfo fi( fullPath );
                                mFileName   = fi.fileName().isEmpty() ? stripMnemonic( action->text() ) : fi.fileName();
                                mFolderPath = fi.absolutePath();

                                setAccessibleName( mFileName );
                                setToolTip( fullPath );
                                setFocusPolicy( Qt::TabFocus );
                                setAttribute( Qt::WA_Hover, true );
                                setCursor( Qt::PointingHandCursor );
                                setFixedHeight( 42 );
                        }

                        QAction* action() const { return mAction; }

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
                                        p.fillRect( rect(), pal.hover );
                                }

                                const QIcon ic = th.icon( QStringLiteral( "file" ), theme::IconRole::Normal );
                                if ( !ic.isNull() )
                                {
                                        ic.paint( &p, QRect( 16, ( height() - 16 ) / 2, 16, 16 ) );
                                }

                                p.setFont( th.uiFont( 12, 400 ) );
                                const QFontMetrics fmFolder( p.font() );
                                const int folderW = qMin( fmFolder.horizontalAdvance( mFolderPath ), width() / 2 );
                                p.setPen( pal.ink3 );
                                p.drawText( QRect( width() - 16 - folderW, 0, folderW, height() ),
                                            Qt::AlignVCenter | Qt::AlignRight,
                                            fmFolder.elidedText( mFolderPath, Qt::ElideMiddle, folderW ) );

                                p.setFont( th.uiFont( 13, 400 ) );
                                const QFontMetrics fmName( p.font() );
                                const int nameW = qMax( 0, width() - 42 - folderW - 28 );
                                p.setPen( pal.ink );
                                p.drawText( QRect( 42, 0, nameW, height() ),
                                            Qt::AlignVCenter | Qt::AlignLeft,
                                            fmName.elidedText( mFileName, Qt::ElideRight, nameW ) );

                                if ( mDrawBottomLine )
                                {
                                        p.setPen( pal.line );
                                        p.drawLine( 0, height() - 1, width(), height() - 1 );
                                }
                        }

                private:
                        QAction* mAction{ nullptr };
                        QString  mFileName;
                        QString  mFolderPath;
                        bool     mDrawBottomLine{ false };
                        bool     mHovered{ false };
                };


                QFrame* makeNavSeparator( QWidget* parent )
                {
                        auto* sep = new QFrame( parent );
                        sep->setFrameShape( QFrame::HLine );
                        sep->setFixedHeight( 1 );
                        sep->setProperty( "role", QStringLiteral( "backstageSeparator" ) );
                        return sep;
                }
        }


        Backstage::Backstage( const Actions& actions, QWidget* parent )
                : QWidget( parent )
                , mActions( actions )
        {
                setProperty( "role", QStringLiteral( "backstage" ) );
                setFocusPolicy( Qt::StrongFocus );

                auto* root = new QHBoxLayout( this );
                root->setContentsMargins( 0, 0, 0, 0 );
                root->setSpacing( 0 );

                // Left navigation rail (240px, SPEC 6.3)
                mNavPanel = new QWidget( this );
                mNavPanel->setFixedWidth( 240 );
                mNavPanel->setProperty( "role", QStringLiteral( "backstageNav" ) );
                auto* nav = new QVBoxLayout( mNavPanel );
                nav->setContentsMargins( 8, 12, 8, 12 );
                nav->setSpacing( 2 );

                auto* backBtn = new BackstageNavButton( tr( "Back" ), QString(), nullptr, false, mNavPanel );
                connect( backBtn, &QAbstractButton::clicked, this, &Backstage::closeRequested );
                nav->addWidget( backBtn );
                nav->addSpacing( 6 );
                nav->addWidget( makeNavSeparator( mNavPanel ) );
                nav->addSpacing( 6 );

                auto addNavAction = [&]( QAction* act, const QString& fallbackLabel, const QString& iconName, bool active = false )
                {
                        if ( !act )
                        {
                                return;
                        }
                        mBoundActions.append( act );
                        const QString label = act->text().isEmpty() ? fallbackLabel : stripMnemonic( act->text() );
                        auto* btn = new BackstageNavButton( label, iconName, act, active, mNavPanel );
                        connect( btn, &QAbstractButton::clicked, this, [this, act]()
                        {
                                emit closeRequested();
                                act->trigger();
                        } );
                        nav->addWidget( btn );
                };

                addNavAction( mActions.fileNew, tr( "New..." ), QStringLiteral( "file-plus" ) );
                addNavAction( mActions.fileOpen, tr( "Open..." ), QStringLiteral( "folder-open" ), true );

                auto* recentBtn = new BackstageNavButton( tr( "Open Recent" ), QStringLiteral( "refresh-cw" ), nullptr, false, mNavPanel );
                connect( recentBtn, &QAbstractButton::clicked, this, [this]()
                {
                        if ( mRecentCard )
                        {
                                mRecentCard->setFocus();
                        }
                } );
                nav->addWidget( recentBtn );

                addNavAction( mActions.fileSave, tr( "Save" ), QStringLiteral( "save" ) );
                addNavAction( mActions.fileSaveAs, tr( "Save As..." ), QStringLiteral( "save-all" ) );

                nav->addSpacing( 6 );
                nav->addWidget( makeNavSeparator( mNavPanel ) );
                nav->addSpacing( 6 );

                addNavAction( mActions.fileTemplateDesigner, tr( "Product Template Designer..." ), QStringLiteral( "tag" ) );
                addNavAction( mActions.editPreferences, tr( "Preferences..." ), QStringLiteral( "settings" ) );

                nav->addSpacing( 6 );
                nav->addWidget( makeNavSeparator( mNavPanel ) );
                nav->addSpacing( 6 );

                addNavAction( mActions.fileClose, tr( "Close" ), QStringLiteral( "x" ) );
                addNavAction( mActions.fileExit, tr( "Exit" ), QStringLiteral( "minus" ) );

                nav->addStretch( 1 );
                root->addWidget( mNavPanel );

                // Right pane
                auto* rightScroll = new QScrollArea( this );
                rightScroll->setWidgetResizable( true );
                rightScroll->setFrameShape( QFrame::NoFrame );

                auto* rightPane = new QWidget( rightScroll );
                auto* rightLayout = new QVBoxLayout( rightPane );
                rightLayout->setContentsMargins( 40, 32, 40, 32 );
                rightLayout->setSpacing( 16 );
                rightLayout->setAlignment( Qt::AlignTop | Qt::AlignLeft );

                auto* heading = new QLabel( tr( "Open" ), rightPane );
                heading->setProperty( "role", QStringLiteral( "pageTitle" ) );
                rightLayout->addWidget( heading );

                auto* browseLink = new BackstageCommandLink( tr( "Browse..." ),
                                                             tr( "Open an existing gLabels project" ),
                                                             QStringLiteral( "folder-open" ),
                                                             rightPane );
                connect( browseLink, &QAbstractButton::clicked, this, [this]()
                {
                        emit closeRequested();
                        if ( mActions.fileOpen )
                        {
                                mActions.fileOpen->trigger();
                        }
                } );
                rightLayout->addWidget( browseLink );

                rightLayout->addSpacing( 8 );

                auto* recentCaption = new QLabel( tr( "Recent" ), rightPane );
                recentCaption->setProperty( "role", QStringLiteral( "caption" ) );
                rightLayout->addWidget( recentCaption );

                mRecentCard = new QWidget( rightPane );
                mRecentCard->setFixedWidth( 560 );
                mRecentCard->setProperty( "role", QStringLiteral( "card" ) );
                auto* recentLayout = new QVBoxLayout( mRecentCard );
                recentLayout->setContentsMargins( 0, 0, 0, 0 );
                recentLayout->setSpacing( 0 );
                rightLayout->addWidget( mRecentCard );

                rightLayout->addStretch( 1 );
                rightScroll->setWidget( rightPane );
                root->addWidget( rightScroll, 1 );

                // Register recent actions for coverage tracking
                for ( QAction* act : mActions.fileRecent )
                {
                        if ( act )
                        {
                                mBoundActions.append( act );
                        }
                }

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &Backstage::onThemeChanged );
                onThemeChanged();
                rebuildRecentList();
        }


        void Backstage::refresh()
        {
                rebuildRecentList();
                update();
        }


        QList<QAction*> Backstage::boundActions() const
        {
                return mBoundActions;
        }


        void Backstage::rebuildRecentList()
        {
                auto* layout = qobject_cast<QVBoxLayout*>( mRecentCard->layout() );
                if ( !layout )
                {
                        return;
                }

                while ( QLayoutItem* item = layout->takeAt( 0 ) )
                {
                        if ( QWidget* w = item->widget() )
                        {
                                w->hide();   // deleteLater() only runs on return to the event loop
                                w->deleteLater();
                        }
                        delete item;
                }

                QList<QAction*> visibleRecent;
                for ( QAction* act : mActions.fileRecent )
                {
                        if ( act && act->isVisible() && !act->data().toString().isEmpty() )
                        {
                                visibleRecent.append( act );
                        }
                }

                if ( visibleRecent.isEmpty() )
                {
                        auto* emptyLbl = new QLabel( tr( "No recent projects." ), mRecentCard );
                        emptyLbl->setContentsMargins( 16, 14, 16, 14 );
                        emptyLbl->setProperty( "role", QStringLiteral( "caption" ) );
                        layout->addWidget( emptyLbl );
                        return;
                }

                for ( int i = 0; i < visibleRecent.size(); ++i )
                {
                        QAction* act = visibleRecent.at( i );
                        const bool hasMore = ( i + 1 < visibleRecent.size() );
                        auto* row = new RecentFileRow( act, hasMore, mRecentCard );
                        connect( row, &QAbstractButton::clicked, this, [this, act]()
                        {
                                emit closeRequested();
                                act->trigger();
                        } );
                        layout->addWidget( row );
                }
        }


        void Backstage::keyPressEvent( QKeyEvent* event )
        {
                if ( event->key() == Qt::Key_Escape )
                {
                        emit closeRequested();
                        event->accept();
                        return;
                }
                QWidget::keyPressEvent( event );
        }


        void Backstage::showEvent( QShowEvent* event )
        {
                QWidget::showEvent( event );
                refresh();
                // Keep initial focus on the backstage surface so Back does not draw a focus ring on open (s02)
                setFocus( Qt::OtherFocusReason );
        }


        void Backstage::onThemeChanged()
        {
                const auto& th  = theme::Theme::instance();
                const auto& pal = th.palette();

                mNavPanel->setStyleSheet(
                        QStringLiteral( "QWidget[role=\"backstageNav\"] { background: %1; border-right: 1px solid %2; }" )
                                .arg( pal.chrome.name(), pal.line.name() ) );
                mRecentCard->setStyleSheet(
                        QStringLiteral( "QWidget[role=\"card\"] { background: %1; border: 1px solid %2; border-radius: 12px; }" )
                                .arg( pal.panel.name(), pal.line.name() ) );
                update();
        }


        void Backstage::paintEvent( QPaintEvent* )
        {
                QPainter p( this );
                p.fillRect( rect(), theme::Theme::instance().palette().bg );
        }

} // namespace glabels::ribbon
