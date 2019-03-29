#ifndef SETTINGSBACKUPWALLET_H
#define SETTINGSBACKUPWALLET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsBackupWallet;
}

class SettingsBackupWallet : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsBackupWallet(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsBackupWallet();

private:
    Ui::SettingsBackupWallet *ui;
};

#endif // SETTINGSBACKUPWALLET_H
