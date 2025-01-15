// Copyright (c) 2019 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_PIVX_SETTINGS_SETTINGSWALLETREPAIRWIDGET_H
#define PIVX_QT_PIVX_SETTINGS_SETTINGSWALLETREPAIRWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"

namespace Ui {
class SettingsWalletRepairWidget;
}

class SettingsWalletRepairWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsWalletRepairWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsWalletRepairWidget();

    /** Build parameter list for restart */
    void buildParameterlist(QString arg);

Q_SIGNALS:
    /** Get restart command-line parameters and handle restart */
    void handleRestart(QStringList args);

public Q_SLOTS:
    void walletSalvage();
    void walletRescan();
    void walletZaptxes1();
    void walletZaptxes2();
    void walletUpgrade();
    void walletReindex();
    void walletResync();
    void walletRewind();

private:
    Ui::SettingsWalletRepairWidget *ui;
};

#endif // PIVX_QT_PIVX_SETTINGS_SETTINGSWALLETREPAIRWIDGET_H
