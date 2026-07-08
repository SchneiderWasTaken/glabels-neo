//  DeployView.hpp
//

#ifndef DeployView_hpp
#define DeployView_hpp


#include "FillConfig.hpp"

#include <QWidget>
#include <QStringList>


class QCheckBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QVBoxLayout;


namespace glabels { namespace model { class Model; } }


namespace glabels
{


        ///
        /// Designer page for configuring and building a kiosk (glabels-fill)
        /// deployment from the current project.
        ///
        class DeployView : public QWidget
        {
                Q_OBJECT

        public:
                explicit DeployView( QWidget* parent = nullptr );

                void setModel( model::Model* model );

        private slots:
                void onBrowseSource();
                void onBrowsePrinter();
                void onBrowseFillExe();
                void onBuild();

        private:
                QStringList collectFieldNames() const;
                void refreshLockedFields();


                model::Model*   mModel{ nullptr };
                QLineEdit*      mAppNameEdit{ nullptr };
                QLineEdit*      mPinEdit{ nullptr };
                QLineEdit*      mPrinterEdit{ nullptr };
                QLineEdit*      mSourcePathEdit{ nullptr };
                QLineEdit*      mFillExeEdit{ nullptr };
                QLineEdit*      mLabelNameEdit{ nullptr };
                QListWidget*    mLockedFieldsList{ nullptr };
                QPushButton*    mBuildButton{ nullptr };
        };


}


#endif // DeployView_hpp
