//  RibbonGroup.cpp
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


#include "RibbonGroup.hpp"

#include "RibbonButton.hpp"
#include "RibbonDropButton.hpp"
#include "theme/Theme.hpp"

#include <QAbstractButton>
#include <QKeyEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <functional>


namespace glabels::ribbon
{

        namespace
        {
                /// Popup container that notifies RibbonGroup when hidden so body can be re-parented back.
                class GroupOverflowPopup : public QWidget
                {
                public:
                        explicit GroupOverflowPopup( QWidget* parent = nullptr )
                                : QWidget( parent, Qt::Popup | Qt::FramelessWindowHint )
                        {
                                setAttribute( Qt::WA_StyledBackground, true );
                                setProperty( "role", QStringLiteral( "ribbonPopover" ) );
                                auto* l = new QVBoxLayout( this );
                                l->setContentsMargins( 10, 8, 10, 8 );
                                l->setSpacing( 0 );
                        }

                        std::function<void()> onHide;

                protected:
                        void paintEvent( QPaintEvent* ) override
                        {
                                QPainter p( this );
                                p.setRenderHint( QPainter::Antialiasing, true );
                                const auto& pal = theme::Theme::instance().palette();
                                p.setPen( QPen( pal.line2, 1.0 ) );
                                p.setBrush( pal.panel );
                                p.drawRoundedRect( QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 ), 10.0, 10.0 );
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


        RibbonGroup::RibbonGroup( const QString& title,
                                  const QString& overflowIconName,
                                  int            collapsePriority,
                                  QWidget*       parent )
                : QWidget( parent )
                , mTitle( title )
                , mOverflowIconName( overflowIconName )
                , mCollapsePriority( collapsePriority )
        {
                setProperty( "role", QStringLiteral( "ribbonGroup" ) );
                setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
                setFixedHeight( theme::Metrics::ribbon - 6 );

                auto* root = new QVBoxLayout( this );
                root->setContentsMargins( theme::Metrics::groupPad, 0, theme::Metrics::groupPad, 0 );
                root->setSpacing( 0 );

                mBodyContainer = new QWidget( this );
                auto* containerLayout = new QHBoxLayout( mBodyContainer );
                containerLayout->setContentsMargins( 0, 0, 0, 0 );
                containerLayout->setSpacing( 0 );

                mBody = new QWidget( mBodyContainer );
                mBodyLayout = new QHBoxLayout( mBody );
                mBodyLayout->setContentsMargins( 0, 0, 0, 0 );
                mBodyLayout->setSpacing( 2 );
                mBodyLayout->setAlignment( Qt::AlignLeft | Qt::AlignTop );
                containerLayout->addWidget( mBody );

                mOverflowButton = new RibbonDropButton( title,
                                                        mOverflowIconName,
                                                        RibbonButton::Size::Large,
                                                        mBodyContainer );
                mOverflowButton->hide();
                containerLayout->addWidget( mOverflowButton );

                auto* popup = new GroupOverflowPopup( this );
                popup->onHide = [this]() { restoreBodyFromPopup(); };
                mOverflowPopup = popup;
                mOverflowButton->setPopupWidget( mOverflowPopup );
                connect( mOverflowButton, &RibbonDropButton::aboutToShowPopup,
                         this, &RibbonGroup::onOverflowAboutToShow );

                root->addWidget( mBodyContainer, 1 );

                mNameLabel = new QLabel( title, this );
                mNameLabel->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
                mNameLabel->setFixedHeight( theme::Metrics::groupName );
                mNameLabel->setProperty( "role", QStringLiteral( "ribbonGroupName" ) );
                root->addWidget( mNameLabel, 0 );

                mBody->installEventFilter( this );

                connect( &theme::Theme::instance(), &theme::Theme::changed,
                         this, &RibbonGroup::onThemeChanged );
                onThemeChanged();
        }


        QString RibbonGroup::title() const
        {
                return mTitle;
        }


        QHBoxLayout* RibbonGroup::bodyLayout() const
        {
                return mBodyLayout;
        }


        QWidget* RibbonGroup::bodyWidget() const
        {
                return mBody;
        }


        void RibbonGroup::addWidget( QWidget* widget, Qt::Alignment alignment )
        {
                if ( widget )
                {
                        mBodyLayout->addWidget( widget, 0, alignment );
                }
        }


        void RibbonGroup::addLayout( QLayout* layout )
        {
                if ( layout )
                {
                        mBodyLayout->addLayout( layout );
                }
        }


        int RibbonGroup::collapsePriority() const
        {
                return mCollapsePriority;
        }


        void RibbonGroup::setCollapsePriority( int priority )
        {
                mCollapsePriority = priority;
        }


        bool RibbonGroup::isCollapsed() const
        {
                return mCollapsed;
        }


        void RibbonGroup::setCollapsed( bool collapsed )
        {
                if ( mCollapsed == collapsed )
                {
                        return;
                }
                mCollapsed = collapsed;

                if ( !mCollapsed )
                {
                        if ( mOverflowPopup && mOverflowPopup->isVisible() )
                        {
                                mOverflowPopup->hide();
                        }
                        restoreBodyFromPopup();
                        mOverflowButton->hide();
                        mBody->show();
                }
                else
                {
                        mBody->hide();
                        mOverflowButton->show();
                }

                updateGeometry();
                update();
        }


        int RibbonGroup::expandedWidthHint() const
        {
                const int bodyW = mBody ? mBody->sizeHint().width() : 0;
                const int nameW = mNameLabel ? mNameLabel->sizeHint().width() : 0;
                return qMax( bodyW, nameW ) + 2 * theme::Metrics::groupPad + 2;
        }


        void RibbonGroup::setShowSeparator( bool show )
        {
                if ( mShowSeparator != show )
                {
                        mShowSeparator = show;
                        update();
                }
        }


        void RibbonGroup::registerAction( QAction* action )
        {
                if ( action && !mExtraActions.contains( action ) )
                {
                        mExtraActions.append( action );
                }
        }


        QList<QAction*> RibbonGroup::boundActions() const
        {
                QList<QAction*> result = mExtraActions;
                const auto buttons = mBody->findChildren<RibbonButton*>();
                for ( RibbonButton* btn : buttons )
                {
                        if ( QAction* act = btn->action() )
                        {
                                if ( !result.contains( act ) )
                                {
                                        result.append( act );
                                }
                        }
                }
                return result;
        }


        void RibbonGroup::onOverflowAboutToShow()
        {
                if ( !mCollapsed || !mOverflowPopup )
                {
                        return;
                }
                if ( auto* containerLayout = qobject_cast<QHBoxLayout*>( mBodyContainer->layout() ) )
                {
                        containerLayout->removeWidget( mBody );
                }
                mBody->setParent( mOverflowPopup );
                if ( auto* popupLayout = qobject_cast<QVBoxLayout*>( mOverflowPopup->layout() ) )
                {
                        popupLayout->addWidget( mBody );
                }
                mBody->show();
                mOverflowPopup->adjustSize();
        }


        void RibbonGroup::restoreBodyFromPopup()
        {
                if ( mBody->parentWidget() == mOverflowPopup )
                {
                        if ( auto* popupLayout = qobject_cast<QVBoxLayout*>( mOverflowPopup->layout() ) )
                        {
                                popupLayout->removeWidget( mBody );
                        }
                        mBody->setParent( mBodyContainer );
                        if ( auto* containerLayout = qobject_cast<QHBoxLayout*>( mBodyContainer->layout() ) )
                        {
                                containerLayout->insertWidget( 0, mBody );
                        }
                        mBody->setVisible( !mCollapsed );
                }
        }


        void RibbonGroup::onThemeChanged()
        {
                const auto& th = theme::Theme::instance();
                mNameLabel->setFont( th.uiFont( 12, 400 ) );
                QPalette p = mNameLabel->palette();
                p.setColor( QPalette::WindowText, th.palette().ink3 );
                mNameLabel->setPalette( p );
                update();
        }


        bool RibbonGroup::eventFilter( QObject* watched, QEvent* event )
        {
                if ( event->type() == QEvent::KeyPress )
                {
                        auto* ke = static_cast<QKeyEvent*>( event );
                        if ( ke->key() == Qt::Key_Right || ke->key() == Qt::Key_Left ||
                             ke->key() == Qt::Key_Down  || ke->key() == Qt::Key_Up )
                        {
                                QList<QAbstractButton*> buttons;
                                for ( auto* b : mBody->findChildren<QAbstractButton*>() )
                                {
                                        if ( b->isVisible() && b->isEnabled() && b->focusPolicy() != Qt::NoFocus )
                                        {
                                                buttons.append( b );
                                        }
                                }
                                QWidget* focused = focusWidget();
                                const int idx = buttons.indexOf( qobject_cast<QAbstractButton*>( focused ) );
                                if ( idx >= 0 && buttons.size() > 1 )
                                {
                                        const int step = ( ke->key() == Qt::Key_Right || ke->key() == Qt::Key_Down ) ? 1 : -1;
                                        const int next = ( idx + step + buttons.size() ) % buttons.size();
                                        buttons.at( next )->setFocus( Qt::TabFocusReason );
                                        return true;
                                }
                        }
                }
                return QWidget::eventFilter( watched, event );
        }


        void RibbonGroup::paintEvent( QPaintEvent* )
        {
                if ( mShowSeparator )
                {
                        QPainter p( this );
                        const auto& pal = theme::Theme::instance().palette();
                        p.setPen( pal.line );
                        p.drawLine( width() - 1, 2, width() - 1, height() - 6 );
                }
        }

} // namespace glabels::ribbon
