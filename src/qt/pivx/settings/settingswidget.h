#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "qt/pivx/settings/settingsbackupwallet.h"
#include "qt/pivx/settings/settingsbittoolwidget.h"
#include "qt/pivx/settings/settingssignmessagewidgets.h"
#include "qt/pivx/settings/settingschangepasswordwidget.h"
#include "qt/pivx/settings/settingsopenurlwidget.h"
#include "qt/pivx/settings/settingswalletrepairwidget.h"
#include "qt/pivx/settings/settingsnetworkwidget.h"
#include "qt/pivx/settings/settingswalletoptionswidget.h"
#include "qt/pivx/settings/settingsmainoptionswidget.h"
#include "qt/pivx/settings/settingsdisplayoptionswidget.h"
#include "qt/pivx/settings/settingsmultisendwidget.h"
#include "qt/pivx/settings/settingsinformationwidget.h"
#include "qt/pivx/settings/settingspeerslistwidget.h"
#include "qt/pivx/settings/settingslockwalletwidget.h"
#include "qt/pivx/settings/settingsconsolewidget.h"
#include "qt/pivx/settings/settingswindowoptionswidget.h"
#include "qt/pivx/settings/settingsnetworkmonitorwidget.h"

class PIVXGUI;

namespace Ui {
class SettingsWidget;
}

class SettingsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsWidget();

    void loadClientModel() override;

    void showDebugConsole();


private slots:
    // File

    void onFileClicked();
    void onOpenUrlClicked();
    void onBackupWalletClicked();
    void onSignMessageClicked();
    void onVerifyMessageClicked();

    // Wallet Configuration

    void onConfigurationClicked();
    void onChangePasswordClicked();
    void onLockWalletClicked();
    void onBipToolClicked();
    void onMultisendClicked();

    // Options

    void onOptionsClicked();
    void onMainOptionsClicked();
    void onWalletOptionsClicked();
    void onNetworkOptionsClicked();
    void onWindowOptionsClicked();
    void onDisplayOptionsClicked();


    // Tools

    void onToolsClicked();
    void onInformationClicked();
    void onDebugConsoleClicked();
    void onNetworkMonitorClicked();
    void onPeersListClicked();
    void onWalletRepairClicked();

    // Help

    void onHelpClicked();
    void onFaqClicked();
    void onAboutClicked();



    void changeTheme(bool isLightTheme, QString &theme);
private:
    Ui::SettingsWidget *ui;
    PIVXGUI* window;

    SettingsBackupWallet *settingsBackupWallet;
    SettingsBitToolWidget *settingsBitToolWidget;
    SettingsSignMessageWidgets *settingsSingMessageWidgets;
    SettingsChangePasswordWidget *settingsChangePasswordWidget;
    SettingsOpenUrlWidget *settingsOpenUrlWidget;
    SettingsWalletRepairWidget *settingsWalletRepairWidget;
    SettingsNetworkWidget *settingsNetworkWidget;
    SettingsWalletOptionsWidget *settingsWalletOptionsWidget;
    SettingsMainOptionsWidget *settingsMainOptionsWidget;
    SettingsDisplayOptionsWidget *settingsDisplayOptionsWidget;
    SettingsMultisendWidget *settingsMultisendWidget;
    SettingsInformationWidget *settingsInformationWidget;
    SettingsPeersListWidget *settingsPeersListWidget;
    SettingsLockWalletWidget *settingsLockWalletWidget;
    SettingsConsoleWidget *settingsConsoleWidget;
    SettingsWindowOptionsWidget *settingsWindowOptionsWidget;
    SettingsNetworkMonitorWidget *settingsNetworkMonitorWidget;
};

#endif // SETTINGSWIDGET_H
