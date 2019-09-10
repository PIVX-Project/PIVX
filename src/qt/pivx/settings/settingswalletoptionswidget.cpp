// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingswalletoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingswalletoptionswidget.h"
#include <QListView>
#include "optionsmodel.h"
#include "clientmodel.h"
#include "qt/pivx/qtutils.h"

SettingsWalletOptionsWidget::SettingsWalletOptionsWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsWalletOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);
    ui->labelDivider->setProperty("cssClass", "container-divider");

    // Title
    ui->labelTitle->setText(tr("Wallet"));
    ui->labelSubtitle1->setText(tr("Customize the internal wallet options"));
    setCssTitleScreen(ui->labelTitle);
    setCssSubtitleScreen(ui->labelSubtitle1);

    // Combobox
    ui->labelTitleStake->setText(tr("Stake split threshold:"));
    ui->labelTitleStake->setProperty("cssClass", "text-main-settings");

    ui->spinBoxStakeSplitThreshold->setProperty("cssClass", "btn-spin-box");
    ui->spinBoxStakeSplitThreshold->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->spinBoxStakeSplitThreshold);

    // Radio buttons
    ui->radioButtonSpend->setText(tr("Spend unconfirmed change"));

    // Title
    ui->labelTitleNetwork->setText(tr("Network"));
    ui->labelSubtitleNetwork->setText(tr("Customize the node network options"));
    setCssTitleScreen(ui->labelTitleNetwork);
    setCssSubtitleScreen(ui->labelSubtitleNetwork);

    // Proxy
    ui->labelSubtitleProxy->setText(tr("Proxy IP:"));
    ui->labelSubtitleProxy->setProperty("cssClass", "text-main-settings");

    ui->lineEditProxy->setPlaceholderText(tr("Enter proxy IP"));
    initCssEditLine(ui->lineEditProxy);

    // Port
    ui->labelSubtitlePort->setText(tr("Port:"));
    ui->labelSubtitlePort->setProperty("cssClass", "text-main-settings");

    ui->lineEditPort->setPlaceholderText("Enter port");
    initCssEditLine(ui->lineEditPort);

    // Radio buttons
    ui->checkBoxMap->setText(tr("Map port using UPnP"));
    ui->checkBoxAllow->setText(tr("Allow incoming connections"));
    ui->checkBoxConnect->setText(tr("Connect through SOCKS5 proxy (default proxy):"));

    // Buttons
    ui->pushButtonSave->setText(tr("SAVE"));
    ui->pushButtonReset->setText(tr("Reset to default"));
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonReset);
    setCssBtnSecondary(ui->pushButtonClean);

    connect(ui->pushButtonSave, SIGNAL(clicked()), parent, SLOT(onSaveOptionsClicked()));
    connect(ui->pushButtonReset, SIGNAL(clicked()), this, SLOT(onResetClicked()));
    connect(ui->pushButtonClean, SIGNAL(clicked()), parent, SLOT(onDiscardChanges()));
}

void SettingsWalletOptionsWidget::onResetClicked(){
    if (clientModel) {
        OptionsModel *optionsModel = clientModel->getOptionsModel();
        QSettings settings;
        optionsModel->setWalletDefaultOptions(settings, true);
        optionsModel->setNetworkDefaultOptions(settings, true);
        inform(tr("Options reset succeed"));
    }
}

void SettingsWalletOptionsWidget::setMapper(QDataWidgetMapper *mapper){
    mapper->addMapping(ui->radioButtonSpend, OptionsModel::SpendZeroConfChange);
    mapper->addMapping(ui->spinBoxStakeSplitThreshold, OptionsModel::StakeSplitThreshold);

    // Network
    mapper->addMapping(ui->checkBoxMap, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->checkBoxAllow, OptionsModel::Listen);
    mapper->addMapping(ui->checkBoxConnect, OptionsModel::ProxyUse);
    mapper->addMapping(ui->lineEditProxy, OptionsModel::ProxyIP);
    mapper->addMapping(ui->lineEditPort, OptionsModel::ProxyPort);
}

SettingsWalletOptionsWidget::~SettingsWalletOptionsWidget(){
    delete ui;
}
