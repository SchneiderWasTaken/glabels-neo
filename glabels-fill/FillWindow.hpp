//  FillWindow.hpp
//

#ifndef FillWindow_hpp
#define FillWindow_hpp


#include "FillConfig.hpp"

#include <QMainWindow>
#include <QByteArray>

#include <memory>


class QComboBox;


namespace glabels { namespace model { class Model; } }
namespace glabels { class FillView; }


namespace glabels
{


        class FillWindow : public QMainWindow
        {
                Q_OBJECT

        public:
                explicit FillWindow( const FillConfig& cfg, QWidget* parent = nullptr );
                ~FillWindow() override;

        protected:
                void keyPressEvent( QKeyEvent* event ) override;

        private slots:
                void onLabelChanged( int index );
                void onAdmin();

        private:
                void loadCurrentLabel();


                FillConfig                              mCfg;
                QString                                 mExePath;
                std::unique_ptr<model::Model>           mModel;
                FillView*                               mFillView{ nullptr };
                QComboBox*                              mLabelCombo{ nullptr };
        };


}


#endif // FillWindow_hpp
