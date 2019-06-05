#ifndef SETTINGSFAQWIDGET_H
#define SETTINGSFAQWIDGET_H

#include <QDialog>

namespace Ui {
class SettingsFaqWidget;
}

class SettingsFaqWidget : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsFaqWidget(QWidget *parent = nullptr);
    ~SettingsFaqWidget();

public slots:
   void windowResizeEvent(QResizeEvent* event);
private slots:
    void onFaq1Clicked();
    void onFaq2Clicked();
    void onFaq3Clicked();
    void onFaq4Clicked();
    void onFaq5Clicked();
    void onFaq6Clicked();
    void onFaq7Clicked();
    void onFaq8Clicked();
private:
    Ui::SettingsFaqWidget *ui;
    int pos = 0;
};

#endif // SETTINGSFAQWIDGET_H
