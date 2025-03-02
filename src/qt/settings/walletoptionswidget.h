// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_SETTINGS_WALLETOPTIONSWIDGET_H
#define PIVX_QT_SETTINGS_WALLETOPTIONSWIDGET_H

#include "pwidget.h"
#include <QDataWidgetMapper>
#include <QWidget>
namespace Ui {
class SettingsWalletOptionsWidget;
}

class SettingsWalletOptionsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsWalletOptionsWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsWalletOptionsWidget();

    void setMapper(QDataWidgetMapper *mapper);

    void discardWalletOnlyOptions();
    bool saveWalletOnlyOptions();

    void saveMapPortOptions();

Q_SIGNALS:
    void saveSettings();
    void discardSettings();

public Q_SLOTS:
    void onResetClicked();

private:
    Ui::SettingsWalletOptionsWidget *ui;

    void loadWalletModel() override;
    void loadClientModel() override;
    void reloadWalletOptions();

    void setSpinBoxStakeSplitThreshold(double val);
    double getSpinBoxStakeSplitThreshold() const;

    // Enable or disable the network activity
    void setNetworkActivity(bool active);
};

#endif // PIVX_QT_SETTINGS_WALLETOPTIONSWIDGET_H
