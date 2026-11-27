//  ReportBugDialog.cpp
//
//  Copyright (C) 2019-2026  Jaye Evins <evins@snaught.com>
//  Copyright (C) 2026       gLabels Neo contributors
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


#include "ReportBugDialog.hpp"

#include "model/Branding.hpp"
#include "model/BugReport.hpp"
#include "model/LogBuffer.hpp"
#include "model/Model.hpp"
#include "model/Version.hpp"
#include "model/XmlLabelCreator.hpp"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>


namespace glabels
{

        namespace
        {
                const int MAX_ATTACHMENT_CHARS = 55000;     // the relay accepts 60000
                const int MAX_POW_BITS = 24;                // refuse absurd work from a misconfigured relay
                const int MIN_DESCRIPTION_CHARS = 10;

                QString normalizeEndpoint( const QString& configured )
                {
                        QString base = configured.trimmed();
                        while ( base.endsWith( '/' ) )
                        {
                                base.chop( 1 );
                        }
                        // Reports contain what the user typed: only ever send them over HTTPS
                        // (plain HTTP is allowed to localhost for development).
                        const QUrl url( base );
                        const bool secure = url.scheme() == "https" ||
                                ( url.scheme() == "http" && ( url.host() == "localhost" || url.host() == "127.0.0.1" ) );
                        return secure ? base : QString();
                }

                QNetworkRequest makeRequest( const QString& url )
                {
                        QNetworkRequest request{ QUrl( url ) };
                        request.setHeader( QNetworkRequest::UserAgentHeader, "glabels-neo/" + model::Version::STRING );
                        request.setTransferTimeout( 20000 );
                        request.setAttribute( QNetworkRequest::RedirectPolicyAttribute,
                                              QNetworkRequest::NoLessSafeRedirectPolicy );
                        return request;
                }

                QString truncated( QString text )
                {
                        if ( text.length() > MAX_ATTACHMENT_CHARS )
                        {
                                text.truncate( MAX_ATTACHMENT_CHARS );
                                text += "\n[truncated]";
                        }
                        return text;
                }
        }


        struct ReportBugDialog::Content
        {
                QString                            summary;
                QString                            description;
                QString                            diagnostics;
                QList<model::BugReport::Attachment> attachments;
        };


        ///
        /// Constructor
        ///
        ReportBugDialog::ReportBugDialog( const model::Model* model, QWidget *parent, const QString& endpointOverride )
                : QDialog( parent ), mModel( model )
        {
                mEndpoint = normalizeEndpoint( endpointOverride.isEmpty() ? model::Version::REPORT_ENDPOINT : endpointOverride );

                setWindowTitle( tr( "%1 - Report a Problem" ).arg( model::Branding::DISPLAY_NAME ) );
                setMinimumWidth( 620 );

                auto* layout = new QVBoxLayout( this );
                layout->setSpacing( 10 );

                auto* intro = new QLabel( this );
                intro->setWordWrap( true );
                intro->setTextFormat( Qt::RichText );
                intro->setOpenExternalLinks( true );
                intro->setText(
                        "<p><b>" + tr( "Tell us what went wrong." ) + "</b> " +
                        tr( "Your report is filed as a public issue on GitHub so the developers can look into it. "
                            "Please do not include passwords or private information." ) +
                        "</p>" );
                layout->addWidget( intro );

                mSummaryEdit = new QLineEdit( this );
                mSummaryEdit->setPlaceholderText( tr( "Short summary, e.g. \"Crash when printing a two-column sheet\"" ) );
                mSummaryEdit->setMaxLength( 120 );
                layout->addWidget( mSummaryEdit );

                mDescriptionEdit = new QPlainTextEdit( this );
                mDescriptionEdit->setPlaceholderText(
                        tr( "What happened? What did you expect instead? What were you doing just before?" ) );
                mDescriptionEdit->setMinimumHeight( 110 );
                layout->addWidget( mDescriptionEdit );

                auto* include = new QGroupBox( tr( "What to include" ), this );
                auto* includeLayout = new QVBoxLayout( include );

                mSystemCheck = new QCheckBox( tr( "System details (program version, operating system, Qt, language)" ), include );
                mSystemCheck->setChecked( true );
                includeLayout->addWidget( mSystemCheck );

                mLabelCheck = new QCheckBox(
                        tr( "The label that is open now (it may contain names and addresses you typed)" ), include );
                mLabelCheck->setEnabled( mModel != nullptr );
                includeLayout->addWidget( mLabelCheck );

                mLogCheck = new QCheckBox( tr( "Recent program messages (helps find crashes and errors)" ), include );
                includeLayout->addWidget( mLogCheck );

                auto* redactNote = new QLabel(
                        tr( "Your user name, computer name, home folder, e-mail addresses and IP addresses are "
                            "removed automatically from system details and program messages." ), include );
                redactNote->setWordWrap( true );
                includeLayout->addWidget( redactNote );
                layout->addWidget( include );

                layout->addWidget( new QLabel( tr( "Exactly what will be sent:" ), this ) );
                mPreview = new QPlainTextEdit( this );
                mPreview->setReadOnly( true );
                mPreview->setMinimumHeight( 150 );
                layout->addWidget( mPreview );

                mStatus = new QLabel( this );
                mStatus->setWordWrap( true );
                mStatus->setTextFormat( Qt::RichText );
                mStatus->setOpenExternalLinks( true );
                layout->addWidget( mStatus );

                mProgress = new QProgressBar( this );
                mProgress->setRange( 0, 0 );
                mProgress->setTextVisible( false );
                mProgress->setVisible( false );
                layout->addWidget( mProgress );

                layout->addSpacing( 14 );

                auto* buttons = new QDialogButtonBox( this );
                mSendButton = buttons->addButton( tr( "&Send report" ), QDialogButtonBox::AcceptRole );
                mSendButton->setDefault( true );
                mBrowserButton = buttons->addButton( tr( "Open on &GitHub instead..." ), QDialogButtonBox::ActionRole );
                mCloseButton = buttons->addButton( QDialogButtonBox::Close );
                layout->addWidget( buttons );

                // The button box is not connected to accept(): Send must keep the dialog open while it works.
                connect( mSendButton,    &QPushButton::clicked, this, &ReportBugDialog::onSendClicked );
                connect( mBrowserButton, &QPushButton::clicked, this, &ReportBugDialog::onBrowserClicked );
                connect( mCloseButton,   &QPushButton::clicked, this, &QDialog::reject );

                connect( mSummaryEdit,     &QLineEdit::textChanged,       this, &ReportBugDialog::onInputChanged );
                connect( mDescriptionEdit, &QPlainTextEdit::textChanged,  this, &ReportBugDialog::onInputChanged );
                connect( mSystemCheck,     &QCheckBox::toggled,           this, &ReportBugDialog::onInputChanged );
                connect( mLabelCheck,      &QCheckBox::toggled,           this, &ReportBugDialog::onInputChanged );
                connect( mLogCheck,        &QCheckBox::toggled,           this, &ReportBugDialog::onInputChanged );

                if ( mEndpoint.isEmpty() )
                {
                        mSendButton->setVisible( false );
                        mBrowserButton->setDefault( true );
                        mStatus->setText( tr( "Automatic sending is not set up in this build. "
                                              "\"Open on GitHub\" opens a pre-filled report in your browser." ) );
                }

                updatePreview();
        }


        ///
        /// Closing while a request is in flight cancels it.
        ///
        void ReportBugDialog::done( int result )
        {
                mCancelled = true;
                QDialog::done( result );
        }


        ///
        /// What the report currently contains.
        ///
        ReportBugDialog::Content ReportBugDialog::buildContent() const
        {
                Content c;
                c.summary = mSummaryEdit->text().simplified();
                c.description = mDescriptionEdit->toPlainText().trimmed();
                if ( mSystemCheck->isChecked() )
                {
                        c.diagnostics = model::BugReport::diagnostics();
                }
                if ( mLabelCheck->isChecked() && mLabelCheck->isEnabled() )
                {
                        const QString text = labelText();
                        if ( !text.isEmpty() )
                        {
                                c.attachments.append( { "label", "label.glabels", text } );
                        }
                }
                if ( mLogCheck->isChecked() )
                {
                        const QString log = truncated( model::BugReport::redact( model::LogBuffer::recent() ) );
                        if ( !log.isEmpty() )
                        {
                                c.attachments.append( { "log", "messages.txt", log } );
                        }
                }
                return c;
        }


        ///
        /// The open label as .glabels XML (computed once, with machine-specific paths removed).
        ///
        QString ReportBugDialog::labelText() const
        {
                if ( !mLabelCached )
                {
                        mLabelCached = true;
                        if ( mModel )
                        {
                                QByteArray buffer;
                                model::XmlLabelCreator::writeBuffer( mModel, buffer );
                                mLabelCache = truncated( model::BugReport::redactPaths( QString::fromUtf8( buffer ) ) );
                        }
                }
                return mLabelCache;
        }


        void ReportBugDialog::updatePreview()
        {
                const Content c = buildContent();

                QString text;
                text += tr( "Summary: " ) + ( c.summary.isEmpty() ? tr( "(none)" ) : c.summary ) + "\n\n";
                text += tr( "Description:" ) + "\n" + ( c.description.isEmpty() ? tr( "(nothing yet)" ) : c.description ) + "\n";
                if ( !c.diagnostics.isEmpty() )
                {
                        text += "\n" + tr( "System details:" ) + "\n" + c.diagnostics + "\n";
                }
                for ( const auto& a : c.attachments )
                {
                        text += "\n" + ( a.kind == "label" ? tr( "Label file:" ) : tr( "Program messages:" ) ) + "\n" + a.text + "\n";
                }
                mPreview->setPlainText( text );

                const bool ready = c.description.length() >= MIN_DESCRIPTION_CHARS && !mBusy;
                mSendButton->setEnabled( ready );
                mBrowserButton->setEnabled( c.description.length() >= MIN_DESCRIPTION_CHARS && !mBusy );
        }


        void ReportBugDialog::onInputChanged()
        {
                updatePreview();
        }


        void ReportBugDialog::setBusy( bool busy, const QString& status )
        {
                mBusy = busy;
                mProgress->setVisible( busy );
                mStatus->setText( status );
                for ( QWidget* w : { static_cast<QWidget*>( mSummaryEdit ), static_cast<QWidget*>( mDescriptionEdit ),
                                     static_cast<QWidget*>( mSystemCheck ), static_cast<QWidget*>( mLabelCheck ),
                                     static_cast<QWidget*>( mLogCheck ) } )
                {
                        w->setEnabled( !busy );
                }
                if ( !busy )
                {
                        mLabelCheck->setEnabled( mModel != nullptr );
                }
                updatePreview();
        }


        void ReportBugDialog::showError( const QString& message )
        {
                setBusy( false, "<span style='color:#b91c1c'>" + message.toHtmlEscaped() + "</span> " +
                                tr( "You can still use \"Open on GitHub instead\"." ) );
        }


        ///
        /// "Open on GitHub instead" - pre-filled issue form in the browser.
        ///
        void ReportBugDialog::onBrowserClicked()
        {
                const Content c = buildContent();

                QString what = c.description;
                if ( !c.diagnostics.isEmpty() )
                {
                        what += "\n\n" + c.diagnostics;
                }

                if ( QDesktopServices::openUrl( model::BugReport::prefilledIssueUrl( c.summary, what ) ) )
                {
                        mStatus->setText( tr( "Your browser has opened a pre-filled report. "
                                              "Review it and press \"Submit new issue\" on GitHub." ) );
                }
                else
                {
                        showError( tr( "Could not open the web browser." ) );
                }
        }


        ///
        /// "Send report" - challenge, proof of work, post.
        ///
        void ReportBugDialog::onSendClicked()
        {
                if ( mEndpoint.isEmpty() )
                {
                        onBrowserClicked();
                        return;
                }
                mCancelled = false;
                setBusy( true, tr( "Contacting the report service..." ) );
                fetchChallenge();
        }


        void ReportBugDialog::fetchChallenge()
        {
                QNetworkReply* reply = mNetwork.get( makeRequest( mEndpoint + "/v1/challenge" ) );
                connect( reply, &QNetworkReply::finished, this, [this, reply]() {
                        reply->deleteLater();
                        if ( mCancelled )
                        {
                                return;
                        }
                        if ( reply->error() != QNetworkReply::NoError )
                        {
                                showError( tr( "Could not reach the report service." ) );
                                return;
                        }
                        const QJsonObject o = QJsonDocument::fromJson( reply->readAll() ).object();
                        const QString challenge = o["challenge"].toString();
                        const int bits = o["bits"].toInt( -1 );
                        if ( challenge.isEmpty() || bits < 0 || bits > MAX_POW_BITS )
                        {
                                showError( tr( "The report service sent an unexpected answer." ) );
                                return;
                        }

                        setBusy( true, tr( "Preparing your report..." ) );
                        const QString counter = model::BugReport::solveProofOfWork(
                                challenge, bits, [this]() {
                                        QCoreApplication::processEvents();
                                        return !mCancelled;
                                } );
                        if ( mCancelled )
                        {
                                return;
                        }
                        if ( counter.isEmpty() )
                        {
                                showError( tr( "Could not prepare the report." ) );
                                return;
                        }
                        postReport( challenge, counter );
                } );
        }


        void ReportBugDialog::postReport( const QString& challenge, const QString& counter )
        {
                const Content c = buildContent();
                const QByteArray body =
                        model::BugReport::toJson( challenge, counter, c.summary, c.description, c.diagnostics, c.attachments );

                setBusy( true, tr( "Sending your report..." ) );

                QNetworkRequest request = makeRequest( mEndpoint + "/v1/reports" );
                request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
                QNetworkReply* reply = mNetwork.post( request, body );
                connect( reply, &QNetworkReply::finished, this, [this, reply]() {
                        reply->deleteLater();
                        if ( mCancelled )
                        {
                                return;
                        }
                        const int status = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
                        const QJsonObject o = QJsonDocument::fromJson( reply->readAll() ).object();

                        if ( status == 201 && o["ok"].toBool() )
                        {
                                const QString url = o["url"].toString();
                                const QUrl parsed( url );
                                const bool trusted = parsed.scheme() == "https" && parsed.host() == "github.com";
                                mBusy = false;
                                mProgress->setVisible( false );
                                mSendButton->setEnabled( false );
                                mBrowserButton->setEnabled( false );
                                mStatus->setText(
                                        "<b>" + tr( "Thank you! Your report was filed as issue #%1." ).arg( o["number"].toInt() ) + "</b> " +
                                        ( trusted ? "<a href='" + url.toHtmlEscaped() + "'>" + tr( "View it on GitHub" ) + "</a>" : QString() ) );
                                mCloseButton->setText( tr( "&Close" ) );
                                return;
                        }
                        if ( status == 429 )
                        {
                                showError( tr( "Too many reports have been sent recently. Please try again later." ) );
                        }
                        else
                        {
                                showError( tr( "The report could not be sent." ) );
                        }
                } );
        }

} // namespace glabels
