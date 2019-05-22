#ifndef SETTINGSBITTOOLWIDGET_H
#define SETTINGSBITTOOLWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "qt/pivx/contactsdropdown.h"

namespace Ui {
class SettingsBitToolWidget;
}

class SettingsBitToolWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsBitToolWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsBitToolWidget();
public slots:
    void onEncryptSelected(bool isEncr);
    void setAddress_ENC(const QString& address);
    void on_encryptKeyButton_ENC_clicked();
    void on_clear_all();
    void onAddressesClicked();

private:
    Ui::SettingsBitToolWidget *ui;
    QAction *btnContact;
};

#endif // SETTINGSBITTOOLWIDGET_H
