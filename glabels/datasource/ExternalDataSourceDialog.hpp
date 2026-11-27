//  ExternalDataSourceDialog.hpp
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

#ifndef datasource_ExternalDataSourceDialog_hpp
#define datasource_ExternalDataSourceDialog_hpp


#include "RedashClient.hpp"

#include "model/ExternalDataSource.hpp"

#include <QDialog>
#include <QStringList>


class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;


namespace glabels::datasource
{

        ///
        /// Dialog for linking or editing a live Redash query for the Fill table.
        ///
        class ExternalDataSourceDialog : public QDialog
        {
                Q_OBJECT

        public:
                explicit ExternalDataSourceDialog( const model::ExternalDataSource& initialSource,
                                                   const QStringList&               labelFields,
                                                   QWidget*                         parent = nullptr );

                model::ExternalDataSource source() const;
                bool wasUnlinked() const { return mUnlinked; }

                /// If the user ran "Test connection" and succeeded, returns those
                /// fetched rows so FillView can apply them immediately without a
                /// second round-trip.
                bool hasTestedResult() const { return mHasTestedResult; }
                const FetchResult& testedResult() const { return mTestedResult; }

        private slots:
                void onQuickUrlChanged( const QString& text );
                void onAuthModeChanged();
                void onCredentialTargetChanged();
                void onToggleShowKey( bool checked );
                void onAddParameter();
                void onRemoveParameter();
                void onTestConnection();
                void onUnlinkClicked();
                void onAcceptClicked();

        private:
                model::ExternalDataSource buildSourceFromForm() const;
                QString effectiveApiKey( const model::ExternalDataSource& src ) const;
                void setParametersInTable( const QMap<QString,QString>& params );
                QMap<QString,QString> parametersFromTable() const;
                void updateStatus( const QString& text, bool isError = false );

        private:
                model::ExternalDataSource mInitialSource;
                QStringList               mLabelFields;
                RedashClient              mClient;

                QLineEdit*    mQuickUrlEdit{ nullptr };
                QComboBox*    mServerCombo{ nullptr };
                QSpinBox*     mQueryIdSpin{ nullptr };
                QComboBox*    mAuthModeCombo{ nullptr };
                QLineEdit*    mApiKeyEdit{ nullptr };
                QPushButton*  mShowKeyBtn{ nullptr };
                QLabel*       mKeyHintLabel{ nullptr };
                QLineEdit*    mDisplayNameEdit{ nullptr };

                QTableWidget* mParamsTable{ nullptr };
                QPushButton*  mAddParamBtn{ nullptr };
                QPushButton*  mRemoveParamBtn{ nullptr };

                QCheckBox*    mRefreshOnOpenCheck{ nullptr };
                QCheckBox*    mRefreshBeforePrintCheck{ nullptr };
                QComboBox*    mMatchKeyCombo{ nullptr };

                QPushButton*  mTestBtn{ nullptr };
                QLabel*       mStatusLabel{ nullptr };
                QPushButton*  mUnlinkBtn{ nullptr };
                QPushButton*  mSaveBtn{ nullptr };

                model::ExternalDataSource mResultSource;
                bool                      mUnlinked{ false };
                bool                      mHasTestedResult{ false };
                FetchResult               mTestedResult;
        };

}


#endif // datasource_ExternalDataSourceDialog_hpp
