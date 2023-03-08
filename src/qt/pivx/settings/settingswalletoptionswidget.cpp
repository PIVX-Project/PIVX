// Copyright (c) 2019-2021 The PIVX Core developers
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
    setCssTitleScreen(ui->labelTitle);
    setCssSubtitleScreen(ui->labelSubtitle1);

    // Combobox
    ui->labelTitleStake->setProperty("cssClass", "text-main-settings");
    ui->spinBoxStakeSplitThreshold->setProperty("cssClass", "btn-spin-box");
    ui->spinBoxStakeSplitThreshold->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->spinBoxStakeSplitThreshold);

    setCssProperty(ui->pushNetEnable, "btn-check-left");
    ui->pushNetEnable->setChecked(true);
    setCssProperty(ui->pushNetDisable, "btn-check-right");
    setCssProperty(ui->labelNetActivity, "text-subtitle");

#ifndef USE_UPNP
    ui->mapPortUpnp->setVisible(false);
#endif
#ifndef USE_NATPMP
    ui->mapPortNatpmp->setVisible(false);
#endif

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

    connect(ui->pushNetEnable, &QPushButton::clicked, [this](){setNetworkActivity(true);});
    connect(ui->pushNetDisable,  &QPushButton::clicked, [this](){setNetworkActivity(false);});
    connect(ui->pushButtonSave, &QPushButton::clicked, [this] { Q_EMIT saveSettings(); });
    connect(ui->pushButtonReset, &QPushButton::clicked, this, &SettingsWalletOptionsWidget::onResetClicked);
    connect(ui->pushButtonClean, &QPushButton::clicked, [this] { Q_EMIT discardSettings(); });
}

void SettingsWalletOptionsWidget::onResetClicked()
{
    QSettings settings;
    walletModel->resetWalletOptions(settings);
    clientModel->getOptionsModel()->setNetworkDefaultOptions(settings, true);
    saveMapPortOptions();
    inform(tr("Options reset succeed"));
}

void SettingsWalletOptionsWidget::setMapper(QDataWidgetMapper *mapper)
{
    mapper->addMapping(ui->radioButtonSpend, OptionsModel::SpendZeroConfChange);

    // Network
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->mapPortNatpmp, OptionsModel::MapPortNatpmp);
    mapper->addMapping(ui->checkBoxAllow, OptionsModel::Listen);
    mapper->addMapping(ui->checkBoxConnect, OptionsModel::ProxyUse);
    mapper->addMapping(ui->lineEditProxy, OptionsModel::ProxyIP);
    mapper->addMapping(ui->lineEditPort, OptionsModel::ProxyPort);
}

void SettingsWalletOptionsWidget::loadWalletModel()
{
    reloadWalletOptions();
    connect(walletModel, &WalletModel::notifySSTChanged, this, &SettingsWalletOptionsWidget::setSpinBoxStakeSplitThreshold);
}

void SettingsWalletOptionsWidget::loadClientModel()
{
    connect(clientModel, &ClientModel::networkActiveChanged, [this](bool isActive) {
        ui->pushNetEnable->setChecked(isActive);
        ui->pushNetDisable->setChecked(!isActive);
    });
}

void SettingsWalletOptionsWidget::reloadWalletOptions()
{
    setSpinBoxStakeSplitThreshold(static_cast<double>(walletModel->getWalletStakeSplitThreshold()) / COIN);
}

void SettingsWalletOptionsWidget::setSpinBoxStakeSplitThreshold(double val)
{
    ui->spinBoxStakeSplitThreshold->setValue(val);
}

double SettingsWalletOptionsWidget::getSpinBoxStakeSplitThreshold() const
{
    return ui->spinBoxStakeSplitThreshold->value();
}

bool SettingsWalletOptionsWidget::saveWalletOnlyOptions()
{
    // stake split threshold
    const CAmount sstOld = walletModel->getWalletStakeSplitThreshold();
    const CAmount sstNew = static_cast<CAmount>(getSpinBoxStakeSplitThreshold() * COIN);
    if (sstNew != sstOld) {
        const double stakeSplitMinimum = walletModel->getSSTMinimum();
        if (sstNew != 0 && sstNew < static_cast<CAmount>(stakeSplitMinimum * COIN)) {
            setSpinBoxStakeSplitThreshold(stakeSplitMinimum);
            inform(tr("Stake Split too low, it shall be either >= %1 or equal to 0 (to disable stake splitting)").arg(stakeSplitMinimum));
            return false;
        }
        walletModel->setWalletStakeSplitThreshold(sstNew);
    }
    return true;
}

void SettingsWalletOptionsWidget::discardWalletOnlyOptions()
{
    reloadWalletOptions();
}

void SettingsWalletOptionsWidget::saveMapPortOptions()
{
    clientModel->mapPort(ui->mapPortUpnp->isChecked(), ui->mapPortNatpmp->isChecked());
}

void SettingsWalletOptionsWidget::setNetworkActivity(bool active)
{
    if (clientModel->getNetworkActive() == active) return;
    clientModel->setNetworkActive(active);
    inform(active ? tr("Network activity enabled") : tr("Network activity disabled"));
}

SettingsWalletOptionsWidget::~SettingsWalletOptionsWidget(){
    delete ui;
}
