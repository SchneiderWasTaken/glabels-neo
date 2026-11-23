//  ReportBugDialog.hpp
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


#ifndef ReportBugDialog_hpp
#define ReportBugDialog_hpp


#include <QDialog>
#include <QNetworkAccessManager>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;


namespace glabels
{

        namespace model
        {
                class Model;
        }


        ///
        /// "Report a Problem" dialog.
        ///
        /// The user describes the problem, chooses what to include and sees exactly what will be sent.
        /// "Send report" files it as a GitHub issue through the report relay (relay/README.md); if the
        /// relay is not configured or not reachable, "Open on GitHub instead" opens a pre-filled issue
        /// form in the browser.  Nothing is sent without the user pressing a button.
        ///
        class ReportBugDialog : public QDialog
        {
                Q_OBJECT


                /////////////////////////////////
                // Life Cycle
                /////////////////////////////////
        public:
                explicit ReportBugDialog( const model::Model* model = nullptr, QWidget *parent = nullptr );


                /////////////////////////////////
                // Overrides
                /////////////////////////////////
        public:
                void done( int result ) override;


                /////////////////////////////////
                // Slots
                /////////////////////////////////
        private slots:
                void onInputChanged();
                void onSendClicked();
                void onBrowserClicked();


                /////////////////////////////////
                // Private
                /////////////////////////////////
        private:
                struct Content;
                Content buildContent() const;
                void    updatePreview();
                void    setBusy( bool busy, const QString& status );
                void    showError( const QString& message );
                void    fetchChallenge();
                void    postReport( const QString& challenge, const QString& counter );
                QString labelText() const;

                const model::Model*   mModel;
                QNetworkAccessManager mNetwork;
                bool                  mBusy = false;
                bool                  mCancelled = false;
                mutable QString       mLabelCache;
                mutable bool          mLabelCached = false;

                QLineEdit*            mSummaryEdit;
                QPlainTextEdit*       mDescriptionEdit;
                QCheckBox*            mSystemCheck;
                QCheckBox*            mLabelCheck;
                QCheckBox*            mLogCheck;
                QPlainTextEdit*       mPreview;
                QLabel*               mStatus;
                QProgressBar*         mProgress;
                QPushButton*          mSendButton;
                QPushButton*          mBrowserButton;
                QPushButton*          mCloseButton;
        };

}


#endif // ReportBugDialog_hpp
