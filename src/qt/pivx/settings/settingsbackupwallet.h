// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

private Q_SLOTS:
    void selectFileOutput();
    void changePassphrase();

private:
    Ui::SettingsBackupWallet *ui;
};

#endif // SETTINGSBACKUPWALLET_H
