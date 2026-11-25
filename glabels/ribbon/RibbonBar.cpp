//  RibbonBar.cpp
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


#include "RibbonBar.hpp"

#include "QuickAccessBar.hpp"
#include "RibbonTab.hpp"
#include "theme/Theme.hpp"

#include <QAbstractButton>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <functional>


namespace glabels::ribbon
{

        namespace
        {
                /// Gradient "File" button (height 28, radius 8, padding 0 20, grad, 600).
                class FileTabButton : public QAbstractButton
                {
                public:
                        explicit FileTabButton( QWidget* parent = nullptr )
                                : QAbstractButton( parent )
                        {
                                setText( QObject::tr( "File" ) );
                                setAccessibleName( QObject::tr( "File" ) );
                                setFocusPolicy( Qt::TabFocus );
                                setAttribute( Qt::WA_Hover, true );
                                setCursor( Qt::PointingHandCursor );
                                setFixedHeight( 28 );
                                setProperty( "role", QStringLiteral( "fileButton" ) );
                        }

                        QSize sizeHint() const override
                        {
                                const QFontMetrics fm( theme::Theme::instance().uiFont( 13, 600 ) );
                                return QSize( fm.horizontalAdvance( text() ) + 40, 28 );
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
                                QLinearGradient grad( r.topLeft(), r.bottomRight() );
                                grad.setColorAt( 0.0, pal.gradA );
                                grad.setColorAt( 0.5, pal.gradB );
                                grad.setColorAt( 1.0, pal.gradC );

                                p.setPen( Qt::NoPen );
                                p.setBrush( grad );
                                p.drawRoundedRect( r, 8.0, 8.0 );

                                if ( isDown() || mHovered )
                                {
                                        p.setBrush( isDown() ? QColor( 0, 0, 0, 36 ) : QColor( 255, 255, 255, 24 ) );
                                        p.drawRoundedRect( r, 8.0, 8.0 );
                                }

                                if ( hasFocus() )
                                {
                                        p.setPen( QPen( pal.accent, 2.0 ) );
                                        p.setBrush( Qt::NoBrush );
                                        p.drawRoundedRect( QRectF( rect() ).adjusted( 1.5, 1.5, -1.5, -1.5 ), 8.0, 8.0 );
                                }

                                p.setFont( th.uiFont( 13, 600 ) );
                                p.setPen( pal.onGrad );
                                p.drawText( rect(), Qt::AlignCenter, text() );
                        }

                private:
                        bool mHovered{ false };
                };


                /// Single tab header button inside the tab strip.
                class TabHeaderButton : public QAbstractButton
                {
                public:
                        TabHeaderButton( TabId id, const QString& label, bool contextual, QWidget* parent = nullptr )
                                : QAbstractButton( parent )
                                , mId( id )
                                , mContextual( contextual )
                        {
                                setText( label );
                                setAccessibleName( label );
                                setCheckable( true );
                                setFocusPolicy( Qt::TabFocus );
                                setAttribute( Qt::WA_Hover, true );
                                setCursor( Qt::PointingHandCursor );
                                setProperty( "role", contextual ? QStringLiteral( "ribbonContextTab" )
                                                                : QStringLiteral( "ribbonTab" ) );
                        }

                        TabId tabId() const { return mId; }
                        bool  isContextual() const { return mContextual; }

                        std::function<void( TabId )> onDoubleClicked;

                        QSize sizeHint() const override
                        {
                                const QFontMetrics fm( theme::Theme::instance().uiFont( 13, 600 ) );
                                const int iconExtra = mContextual ? 22 : 0;
                                const int h = isChecked() ? theme::Metrics::tabActive : theme::Metrics::tab;
                                return QSize( fm.horizontalAdvance( text() ) + 32 + iconExtra, h );
                        }

                        void setActiveVisual( bool active )
                        {
                                setChecked( active );
                                setFixedHeight( active ? theme::Metrics::tabActive : theme::Metrics::tab );
                                update();
                        }

                protected:
                        void enterEvent( QEnterEvent* e ) override { mHovered = true; update(); QAbstractButton::enterEvent( e ); }
                        void leaveEvent( QEvent* e ) override      { mHovered = false; update(); QAbstractButton::leaveEvent( e ); }

                        void mouseDoubleClickEvent( QMouseEvent* e ) override
                        {
                                if ( e->button() == Qt::LeftButton && onDoubleClicked )
                                {
                                        onDoubleClicked( mId );
                                        e->accept();
                                        return;
                                }
                                QAbstractButton::mouseDoubleClickEvent( e );
                        }

                        void paintEvent( QPaintEvent* ) override
                        {
                                QPainter p( this );
                                p.setRenderHint( QPainter::Antialiasing, true );

                                const auto& th  = theme::Theme::instance();
                                const auto& pal = th.palette();
                                const bool active = isChecked();

                                const QRectF r = QRectF( rect() );
                                QPainterPath tabPath;
                                const qreal rad = 8.0;
                                tabPath.moveTo( r.left() + 0.5, r.bottom() );
                                tabPath.lineTo( r.left() + 0.5, r.top() + rad );
                                tabPath.quadTo( r.left() + 0.5, r.top() + 0.5, r.left() + rad, r.top() + 0.5 );
                                tabPath.lineTo( r.right() - rad, r.top() + 0.5 );
                                tabPath.quadTo( r.right() - 0.5, r.top() + 0.5, r.right() - 0.5, r.top() + rad );
                                tabPath.lineTo( r.right() - 0.5, r.bottom() );
                                tabPath.closeSubpath();

                                if ( active )
                                {
                                        p.fillPath( tabPath, mContextual ? pal.ctx : pal.ribbon );
                                        p.setPen( QPen( pal.line, 1.0 ) );
                                        p.drawPath( tabPath );
                                        // Erase bottom border so tab merges into ribbon body
                                        p.setPen( QPen( mContextual ? pal.ctx : pal.ribbon, 2.0 ) );
                                        p.drawLine( QPointF( r.left() + 1.5, r.bottom() ),
                                                    QPointF( r.right() - 1.5, r.bottom() ) );
                                }
                                else if ( mContextual )
                                {
                                        p.fillPath( tabPath, pal.ctx );
                                }
                                else if ( mHovered )
                                {
                                        p.fillPath( tabPath, pal.hover );
                                }

                                // Contextual tab 2px top line (SPEC 4.3)
                                if ( mContextual )
                                {
                                        p.fillRect( QRectF( r.left() + 4.0, r.top(), r.width() - 8.0, 2.0 ), pal.ctxLine );
                                }

                                // Active tab 3px accent bottom underline inset 12px (SPEC 4.3)
                                if ( active )
                                {
                                        QRectF under( r.left() + 12.0, r.bottom() - 3.0, qMax( 0.0, r.width() - 24.0 ), 3.0 );
                                        QPainterPath up;
                                        up.addRoundedRect( under, 1.5, 1.5 );
                                        p.fillPath( up, pal.accent );
                                }

                                // Focus ring
                                if ( hasFocus() )
                                {
                                        p.setPen( QPen( pal.accent, 2.0 ) );
                                        p.setBrush( Qt::NoBrush );
                                        p.drawRoundedRect( r.adjusted( 2.0, 2.0, -2.0, -2.0 ), 6.0, 6.0 );
                                }

                                // Contextual pen icon + text
                                p.setFont( th.uiFont( 13, active ? 600 : 400 ) );
                                p.setPen( ( active || mContextual ) ? pal.ink : pal.ink2 );

                                if ( mContextual )
                                {
                                        const QFontMetrics fm( p.font() );
                                        const int textW = fm.horizontalAdvance( text() );
                                        const int totalW = 14 + 6 + textW;
                                        const int startX = ( width() - totalW ) / 2;
                                        const int iconY  = ( height() - 14 ) / 2;

                                        QIcon penIcon = th.icon( QStringLiteral( "pencil" ), theme::IconRole::Accent );
                                        if ( !penIcon.isNull() )
                                        {
                                                penIcon.paint( &p, QRect( startX, iconY, 14, 14 ) );
                                        }
                                        else
                                        {
                                                // Minimal drawn pen nib fallback
                                                p.save();
                                                p.setPen( QPen( pal.ctxLine, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
                                                p.drawLine( startX + 2, iconY + 12, startX + 12, iconY + 2 );
                                                p.restore();
                                        }
                                        p.drawText( QRect( startX + 20, 0, textW + 4, height() ),
                                                    Qt::AlignVCenter | Qt::AlignLeft, text() );
                                }
                                else
                                {
                                        p.drawText( rect(), Qt::AlignCenter, text() );
                                }
                        }

                private:
                        TabId mId;
                        bool  mContextual{ false };
                        bool  mHovered{ false };
                };


                /// Popup host for collapsed ribbon mode (SPEC 5.2).
                class CollapsedRibbonPopup : public QWidget
                {
                public:
                        explicit CollapsedRibbonPopup( QWidget* parent = nullptr )
                                : QWidget( parent, Qt::Popup | Qt::FramelessWindowHint )
                        {
                                auto* l = new QVBoxLayout( this );
                                l->setContentsMargins( 0, 0, 0, 0 );
                                l->setSpacing( 0 );
                        }

                        std::function<void()> onHide;

                protected:
                        void keyPressEvent( QKeyEvent* event ) override
                        {
                                if ( event->key() == Qt::Key_Escape )
                                {
                                        hide();
                                        event->accept();
                                        return;
                                }
                                QWidget::keyPressEvent( event );
                        }

                        void hideEvent( QHideEvent* event ) override
                        {
                                QWidget::hideEvent( event );
                                if ( onHide )
                                {
                                        onHide();
                                }
                        }
                };
        }


        ///
        /// Internal tab strip widget (42px tall, chrome bg, bottom 1px line, mouse-wheel tab switch)
        ///
        class RibbonBar::TabStripWidget : public QWidget
        {
        public:
                explicit TabStripWidget( QWidget* parent = nullptr )
                        : QWidget( parent )
                {
                        setFixedHeight( theme::Metrics::tabStrip );
                        setProperty( "role", QStringLiteral( "ribbonTabStrip" ) );

                        mLayout = new QHBoxLayout( this );
                        mLayout->setContentsMargins( 10, 0, 10, 0 );
                        mLayout->setSpacing( 2 );
                        mLayout->setAlignment( Qt::AlignLeft | Qt::AlignBottom );

                        mQuickAccess = new QuickAccessBar( this );
                        mLayout->addWidget( mQuickAccess, 0, Qt::AlignVCenter );

                        mFileButton = new FileTabButton( this );
                        auto* fileWrap = new QWidget( this );
                        auto* fl = new QHBoxLayout( fileWrap );
                        fl->setContentsMargins( 10, 0, 12, 4 );
                        fl->addWidget( mFileButton );
                        mLayout->addWidget( fileWrap, 0, Qt::AlignBottom );

                        mNormalLayout = new QHBoxLayout();
                        mNormalLayout->setContentsMargins( 0, 0, 0, 0 );
                        mNormalLayout->setSpacing( 2 );
                        mNormalLayout->setAlignment( Qt::AlignLeft | Qt::AlignBottom );
                        mLayout->addLayout( mNormalLayout );

                        mContextSpacer = new QWidget( this );
                        mContextSpacer->setFixedWidth( 14 );
                        mContextSpacer->hide();
                        mLayout->addWidget( mContextSpacer );

                        mContextLayout = new QHBoxLayout();
                        mContextLayout->setContentsMargins( 0, 0, 0, 0 );
                        mContextLayout->setSpacing( 2 );
                        mContextLayout->setAlignment( Qt::AlignLeft | Qt::AlignBottom );
                        mLayout->addLayout( mContextLayout );

                        mLayout->addStretch( 1 );
                }

                QuickAccessBar* quickAccess() const { return mQuickAccess; }
                FileTabButton*  fileButton() const  { return mFileButton; }

                TabHeaderButton* addTabButton( TabId id, const QString& title, bool contextual )
                {
                        auto* btn = new TabHeaderButton( id, title, contextual, this );
                        mButtons.insert( id, btn );
                        if ( contextual )
                        {
                                btn->hide();
                                mContextLayout->addWidget( btn, 0, Qt::AlignBottom );
                        }
                        else
                        {
                                mNormalLayout->addWidget( btn, 0, Qt::AlignBottom );
                        }
                        return btn;
                }

                TabHeaderButton* button( TabId id ) const { return mButtons.value( id, nullptr ); }
                QWidget*         contextSpacer() const    { return mContextSpacer; }

                std::function<void( int )> onWheel;

        protected:
                void wheelEvent( QWheelEvent* event ) override
                {
                        if ( onWheel && event->angleDelta().y() != 0 )
                        {
                                onWheel( event->angleDelta().y() > 0 ? -1 : 1 );
                                event->accept();
                                return;
                        }
                        QWidget::wheelEvent( event );
                }

                void paintEvent( QPaintEvent* ) override
                {
                        QPainter p( this );
                        const auto& pal = theme::Theme::instance().palette();
                        p.fillRect( rect(), pal.chrome );
                        p.setPen( pal.line );
                        p.drawLine( 0, height() - 1, width(), height() - 1 );
                }

        private:
                QHBoxLayout*                    mLayout{ nullptr };
                QuickAccessBar*                 mQuickAccess{ nullptr };
                FileTabButton*                  mFileButton{ nullptr };
                QHBoxLayout*                    mNormalLayout{ nullptr };
                QWidget*                        mContextSpacer{ nullptr };
                QHBoxLayout*                    mContextLayout{ nullptr };
                QMap<TabId, TabHeaderButton*>   mButtons;
        };


        RibbonBar::RibbonBar( QWidget* parent )
                : QWidget( parent )
        {
                setProperty( "role", QStringLiteral( "ribbonBar" ) );
                setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( 0, 0, 0, 0 );
                root->setSpacing( 0 );

                mStrip = new TabStripWidget( this );
                mQuickAccess = mStrip->quickAccess();
                mStrip->onWheel = [this]( int delta ) { onWheelStep( delta ); };
                connect( mStrip->fileButton(), &QAbstractButton::clicked,
                         this, &RibbonBar::fileButtonClicked );
                root->addWidget( mStrip );

                mBodyHost = new QWidget( this );
                auto* hostLayout = new QVBoxLayout( mBodyHost );
                hostLayout->setContentsMargins( 0, 0, 0, 0 );
                hostLayout->setSpacing( 0 );

                mBodyStack = new QStackedWidget( mBodyHost );
                mBodyStack->setFixedHeight( theme::Metrics::ribbon );
                mBodyStack->setProperty( "role", QStringLiteral( "ribbonBody" ) );
                hostLayout->addWidget( mBodyStack );
                root->addWidget( mBodyHost );

                auto* popup = new CollapsedRibbonPopup( this );
                popup->onHide = [this]() { restoreBodyFromPopup(); };
                mCollapsedPopup = popup;

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &RibbonBar::onThemeChanged );
        }


        QuickAccessBar* RibbonBar::quickAccessBar() const
        {
                return mQuickAccess;
        }


        RibbonTab* RibbonBar::addTab( TabId id, const QString& title, bool contextual )
        {
                auto* tabWidget = new RibbonTab( id, title, contextual, mBodyStack );
                mTabs.insert( id, tabWidget );
                mTabOrder.append( id );
                mBodyStack->addWidget( tabWidget );

                TabHeaderButton* btn = mStrip->addTabButton( id, title, contextual );
                connect( btn, &QAbstractButton::clicked, this, [this, id]() { onTabClicked( id ); } );
                btn->onDoubleClicked = [this]( TabId clickedId ) { onTabDoubleClicked( clickedId ); };

                updateTabStripState();
                updateBodyVisibility();
                return tabWidget;
        }


        RibbonTab* RibbonBar::tab( TabId id ) const
        {
                return mTabs.value( id, nullptr );
        }


        TabId RibbonBar::currentTab() const
        {
                return mCurrentTab;
        }


        void RibbonBar::setCurrentTab( TabId id, bool emitSignal )
        {
                if ( !mTabs.contains( id ) )
                {
                        return;
                }

                const bool changed = ( mCurrentTab != id );
                mCurrentTab = id;
                mBodyStack->setCurrentWidget( mTabs.value( id ) );
                updateTabStripState();
                updateBodyVisibility();

                if ( changed && emitSignal )
                {
                        emit tabChanged( id );
                }
        }


        void RibbonBar::setVisibleContextualTab( std::optional<TabId> tab )
        {
                if ( mVisibleContextual == tab )
                {
                        return;
                }
                mVisibleContextual = tab;
                updateTabStripState();
        }


        std::optional<TabId> RibbonBar::visibleContextualTab() const
        {
                return mVisibleContextual;
        }


        bool RibbonBar::isWelcomeMode() const
        {
                return mWelcomeMode;
        }


        void RibbonBar::setWelcomeMode( bool welcome )
        {
                if ( mWelcomeMode == welcome )
                {
                        return;
                }
                mWelcomeMode = welcome;
                mWelcomeHelpOpen = false;
                if ( welcome )
                {
                        mVisibleContextual.reset();
                }
                updateTabStripState();
                updateBodyVisibility();
        }


        bool RibbonBar::isCollapsed() const
        {
                return mCollapsed;
        }


        void RibbonBar::setCollapsed( bool collapsed )
        {
                if ( mCollapsed == collapsed )
                {
                        return;
                }
                mCollapsed = collapsed;
                if ( !mCollapsed )
                {
                        restoreBodyFromPopup();
                }
                updateBodyVisibility();
                emit collapsedChanged( mCollapsed );
        }


        void RibbonBar::toggleCollapsed()
        {
                setCollapsed( !mCollapsed );
        }


        void RibbonBar::registerTabAction( TabId id, QAction* action )
        {
                if ( action )
                {
                        mTabActions.insert( id, action );
                        if ( auto* btn = mStrip->button( id ) )
                        {
                                btn->setToolTip( formatActionTooltip( action ) );
                        }
                }
        }


        QList<QAction*> RibbonBar::allBoundActions() const
        {
                QList<QAction*> result = mQuickAccess->boundActions();
                for ( auto it = mTabActions.cbegin(); it != mTabActions.cend(); ++it )
                {
                        if ( it.value() && !result.contains( it.value() ) )
                        {
                                result.append( it.value() );
                        }
                }
                for ( TabId id : mTabOrder )
                {
                        if ( RibbonTab* t = mTabs.value( id, nullptr ) )
                        {
                                for ( QAction* act : t->boundActions() )
                                {
                                        if ( act && !result.contains( act ) )
                                        {
                                                result.append( act );
                                        }
                                }
                        }
                }
                return result;
        }


        void RibbonBar::onTabClicked( TabId id )
        {
                if ( mWelcomeMode )
                {
                        if ( id == TabId::Help )
                        {
                                mWelcomeHelpOpen = !mWelcomeHelpOpen;
                                mCurrentTab = TabId::Help;
                                mBodyStack->setCurrentWidget( mTabs.value( id ) );
                                updateTabStripState();
                                updateBodyVisibility();
                        }
                        return;
                }

                if ( mCollapsed )
                {
                        setCurrentTab( id, true );
                        showCollapsedPopup( id );
                        return;
                }

                setCurrentTab( id, true );
        }


        void RibbonBar::onTabDoubleClicked( TabId id )
        {
                if ( mWelcomeMode )
                {
                        return;
                }
                if ( id == mCurrentTab )
                {
                        toggleCollapsed();
                }
        }


        void RibbonBar::onWheelStep( int delta )
        {
                if ( mWelcomeMode )
                {
                        return;
                }

                QList<TabId> visibleTabs;
                for ( TabId id : mTabOrder )
                {
                        if ( auto* btn = mStrip->button( id ); btn && !btn->isHidden() )
                        {
                                visibleTabs.append( id );
                        }
                }
                if ( visibleTabs.size() <= 1 )
                {
                        return;
                }
                const int idx = visibleTabs.indexOf( mCurrentTab );
                if ( idx < 0 )
                {
                        return;
                }
                const int nextIdx = qBound( 0, idx + delta, visibleTabs.size() - 1 );
                if ( nextIdx != idx )
                {
                        setCurrentTab( visibleTabs.at( nextIdx ), true );
                }
        }


        void RibbonBar::onThemeChanged()
        {
                mStrip->update();
                update();
        }


        void RibbonBar::updateTabStripState()
        {
                bool anyContextVisible = false;
                for ( TabId id : mTabOrder )
                {
                        auto* btn = mStrip->button( id );
                        if ( !btn )
                        {
                                continue;
                        }

                        if ( mWelcomeMode )
                        {
                                const bool show = ( id == TabId::Help );
                                btn->setVisible( show );
                                btn->setActiveVisual( show && mWelcomeHelpOpen );
                        }
                        else if ( btn->isContextual() )
                        {
                                const bool show = mVisibleContextual.has_value() && *mVisibleContextual == id;
                                btn->setVisible( show );
                                btn->setActiveVisual( show && mCurrentTab == id );
                                if ( show )
                                {
                                        anyContextVisible = true;
                                }
                        }
                        else
                        {
                                btn->setVisible( true );
                                btn->setActiveVisual( mCurrentTab == id );
                        }
                }

                mStrip->contextSpacer()->setVisible( anyContextVisible );
        }


        void RibbonBar::updateBodyVisibility()
        {
                if ( mWelcomeMode )
                {
                        mBodyHost->setVisible( mWelcomeHelpOpen );
                }
                else
                {
                        mBodyHost->setVisible( !mCollapsed );
                }
                updateGeometry();
        }


        void RibbonBar::showCollapsedPopup( TabId id )
        {
                if ( !mCollapsedPopup || !mTabs.contains( id ) )
                {
                        return;
                }

                mBodyStack->setCurrentWidget( mTabs.value( id ) );
                if ( auto* hostLayout = qobject_cast<QVBoxLayout*>( mBodyHost->layout() ) )
                {
                        hostLayout->removeWidget( mBodyStack );
                }
                mBodyStack->setParent( mCollapsedPopup );
                if ( auto* popLayout = qobject_cast<QVBoxLayout*>( mCollapsedPopup->layout() ) )
                {
                        popLayout->addWidget( mBodyStack );
                }
                mBodyStack->show();

                const QPoint pos = mStrip->mapToGlobal( QPoint( 0, mStrip->height() ) );
                mCollapsedPopup->setFixedWidth( width() );
                mCollapsedPopup->setFixedHeight( theme::Metrics::ribbon );
                mCollapsedPopup->move( pos );
                mCollapsedPopup->show();
                mCollapsedPopup->raise();
                mCollapsedPopup->activateWindow();
        }


        void RibbonBar::restoreBodyFromPopup()
        {
                if ( mBodyStack->parentWidget() == mCollapsedPopup )
                {
                        if ( auto* popLayout = qobject_cast<QVBoxLayout*>( mCollapsedPopup->layout() ) )
                        {
                                popLayout->removeWidget( mBodyStack );
                        }
                        mBodyStack->setParent( mBodyHost );
                        if ( auto* hostLayout = qobject_cast<QVBoxLayout*>( mBodyHost->layout() ) )
                        {
                                hostLayout->addWidget( mBodyStack );
                        }
                        mBodyStack->show();
                }
        }

} // namespace glabels::ribbon
