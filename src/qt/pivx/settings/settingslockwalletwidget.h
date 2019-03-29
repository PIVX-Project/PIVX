#ifndef SETTINGSLOCKWALLETWIDGET_H
#define SETTINGSLOCKWALLETWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsLockWalletWidget;
}

class SettingsLockWalletWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsLockWalletWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsLockWalletWidget();

private:
    Ui::SettingsLockWalletWidget *ui;
};

#endif // SETTINGSLOCKWALLETWIDGET_H
