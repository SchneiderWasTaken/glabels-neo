//  AdminDialog.hpp
//

#ifndef AdminDialog_hpp
#define AdminDialog_hpp


#include "FillConfig.hpp"

#include <QDialog>


class QLineEdit;
class QListWidget;
class QPushButton;


namespace glabels
{


        ///
        /// PIN-entry + setup dialog for the kiosk admin mode.
        ///
        /// Two stages:
        ///   1. PIN entry (verifies against the config's adminPin).
        ///   2. Settings editor (label files, printer, PIN, branding).
        ///
        class AdminDialog : public QDialog
        {
                Q_OBJECT


        public:
                explicit AdminDialog( const FillConfig& cfg, QWidget* parent = nullptr );

                /// The (possibly edited) config.  Only valid if exec() == Accepted.
                FillConfig result() const { return mCfg; }


        private slots:
                void onVerifyPin();
                void onAddLabel();
                void onRemoveLabel();
                void onBrowsePrinter();
                void onAccept();


        private:
                void buildPinPage();
                void buildSettingsPage();


                FillConfig   mCfg;
                bool         mPinOk{ false };

                // PIN page
                QLineEdit*   mPinEdit{ nullptr };

                // Settings page
                QListWidget* mLabelList{ nullptr };
                QLineEdit*   mPrinterEdit{ nullptr };
                QLineEdit*   mNewPinEdit{ nullptr };
                QLineEdit*   mAppNameEdit{ nullptr };
        };


}


#endif // AdminDialog_hpp
