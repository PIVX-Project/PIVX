//Copyright (c) 2019 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Sombe/settings/settingswalletoptionswidget.h"
#include "qt/Sombe/settings/forms/ui_settingswalletoptionswidget.h"
#include <QListView>
#include "optionsmodel.h"
#include "clientmodel.h"
#include "qt/Sombe/qtutils.h"

SettingsWalletOptionsWidget::SettingsWalletOptionsWidget(SombeGUI* _window, QWidget *parent) :
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
    setCssTitleScreen(ui->labelTitle);
    setCssSubtitleScreen(ui->labelSubtitle1);

    // Combobox
    ui->labelTitleStake->setProperty("cssClass", "text-main-settings");
    ui->spinBoxStakeSplitThreshold->setProperty("cssClass", "btn-spin-box");
    ui->spinBoxStakeSplitThreshold->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->spinBoxStakeSplitThreshold);

    // Radio buttons

    // Title
    ui->labelTitleNetwork->setText(tr("Network"));
    setCssTitleScreen(ui->labelTitleNetwork);
    setCssSubtitleScreen(ui->labelSubtitleNetwork);

    // Proxy
    ui->labelSubtitleProxy->setProperty("cssClass", "text-main-settings");
    initCssEditLine(ui->lineEditProxy);

    // Port
    ui->labelSubtitlePort->setProperty("cssClass", "text-main-settings");
    initCssEditLine(ui->lineEditPort);

    // Buttons
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonReset);
    setCssBtnSecondary(ui->pushButtonClean);

    connect(ui->pushButtonSave, &QPushButton::clicked, [this] { Q_EMIT saveSettings(); });
    connect(ui->pushButtonReset, &QPushButton::clicked, this, &SettingsWalletOptionsWidget::onResetClicked);
    connect(ui->pushButtonClean, &QPushButton::clicked, [this] { Q_EMIT discardSettings(); });
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

void SettingsWalletOptionsWidget::setSpinBoxStakeSplitThreshold(double val)
{
    ui->spinBoxStakeSplitThreshold->setValue(val);
}

SettingsWalletOptionsWidget::~SettingsWalletOptionsWidget(){
    delete ui;
}
