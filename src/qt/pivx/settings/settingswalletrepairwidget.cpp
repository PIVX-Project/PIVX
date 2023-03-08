// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingswalletrepairwidget.h"
#include "qt/pivx/settings/forms/ui_settingswalletrepairwidget.h"
#include "qt/pivx/qtutils.h"

SettingsWalletRepairWidget::SettingsWalletRepairWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsWalletRepairWidget)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);
    ui->scrollStack->setProperty("cssClass", "container");

    // Title
    ui->labelTitle->setText(tr("Wallet Repair"));
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Labels
    setCssProperty({ui->labelMessageSalvage, ui->labelMessageRescan, ui->labelMessageRecover1,
                    ui->labelMessageRecover2, ui->labelMessageUpgrade, ui->labelMessageRebuild,
                    ui->labelMessageDelete}, "text-main-settings");

    // Buttons
    setCssProperty({ui->pushButtonSalvage, ui->pushButtonRescan, ui->pushButtonRecover1,
                    ui->pushButtonRecover2, ui->pushButtonUpgrade, ui->pushButtonRebuild,
                    ui->pushButtonDelete}, "btn-primary");

    // Wallet Repair Buttons
    connect(ui->pushButtonSalvage, &QPushButton::clicked, this, &SettingsWalletRepairWidget::walletSalvage);
    connect(ui->pushButtonRescan, &QPushButton::clicked, this, &SettingsWalletRepairWidget::walletRescan);
    connect(ui->pushButtonRecover1, &QPushButton::clicked, this, &SettingsWalletRepairWidget::walletZaptxes1);
    connect(ui->pushButtonRecover2, &QPushButton::clicked, this, &SettingsWalletRepairWidget::walletZaptxes2);
    connect(ui->pushButtonUpgrade, &QPushButton::clicked, this, &SettingsWalletRepairWidget::walletUpgrade);
    connect(ui->pushButtonRebuild, &QPushButton::clicked, this, &SettingsWalletRepairWidget::walletReindex);
    connect(ui->pushButtonDelete, &QPushButton::clicked, this, &SettingsWalletRepairWidget::walletResync);
}

/** Restart wallet with "-salvagewallet" */
void SettingsWalletRepairWidget::walletSalvage()
{
    buildParameterlist(SALVAGEWALLET);
}

/** Restart wallet with "-rescan" */
void SettingsWalletRepairWidget::walletRescan()
{
    buildParameterlist(RESCAN);
}

/** Restart wallet with "-zapwallettxes=1" */
void SettingsWalletRepairWidget::walletZaptxes1()
{
    buildParameterlist(ZAPTXES1);
}

/** Restart wallet with "-zapwallettxes=2" */
void SettingsWalletRepairWidget::walletZaptxes2()
{
    buildParameterlist(ZAPTXES2);
}

/** Restart wallet with "-upgradewallet" */
void SettingsWalletRepairWidget::walletUpgrade()
{
    buildParameterlist(UPGRADEWALLET);
}

/** Restart wallet with "-reindex" */
void SettingsWalletRepairWidget::walletReindex()
{
    buildParameterlist(REINDEX);
}

/** Restart wallet with "-resync" */
void SettingsWalletRepairWidget::walletResync()
{
    QString resyncWarning = tr("This will delete your local blockchain folders and the wallet will synchronize the complete Blockchain from scratch.<br /><br />");
    resyncWarning +=   tr("This needs quite some time and downloads a lot of data.<br /><br />");
    resyncWarning +=   tr("Your transactions and funds will be visible again after the download has completed.<br /><br />");
    resyncWarning +=   tr("Do you want to continue?.<br />");
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm resync Blockchain"),
                                                               resyncWarning,
                                                               QMessageBox::Yes | QMessageBox::Cancel,
                                                               QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        // Resync canceled
        return;
    }

    // Restart and resync
    buildParameterlist(RESYNC);
}

/** Build command-line parameter list for restart */
void SettingsWalletRepairWidget::buildParameterlist(QString arg)
{
    // Get command-line arguments and remove the application name
    QStringList args = QApplication::arguments();
    args.removeFirst();

    // Remove existing repair-options
    args.removeAll(SALVAGEWALLET);
    args.removeAll(RESCAN);
    args.removeAll(ZAPTXES1);
    args.removeAll(ZAPTXES2);
    args.removeAll(UPGRADEWALLET);
    args.removeAll(REINDEX);

    // Append repair parameter to command line.
    args.append(arg);

    // Send command-line arguments to PIVXGUI::handleRestart()
    Q_EMIT handleRestart(args);
}

SettingsWalletRepairWidget::~SettingsWalletRepairWidget()
{
    delete ui;
}
