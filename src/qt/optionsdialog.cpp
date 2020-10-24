// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2017-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/pivx-config.h"
#endif

#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "netbase.h"
#include "txdb.h" // for -dbcache defaults

#ifdef ENABLE_WALLET
#include "wallet/wallet.h" // for CWallet::minTxFee
#endif

#include <boost/thread.hpp>

#include <QDataWidgetMapper>
#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>

OptionsDialog::OptionsDialog(QWidget* parent, bool enableWallet) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
                                                                   ui(new Ui::OptionsDialog),
                                                                   model(0),
                                                                   mapper(0),
                                                                   fProxyIpValid(true)
{
    ui->setupUi(this);
    GUIUtil::restoreWindowGeometry("nOptionsDialogWindow", this->size(), this);

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    ui->threadsScriptVerif->setMinimum(-GetNumCores());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    connect(ui->connectSocks, &QCheckBox::toggled, ui->proxyIp, &QWidget::setEnabled);
    connect(ui->connectSocks, &QCheckBox::toggled, ui->proxyPort, &QWidget::setEnabled);

    ui->proxyIp->installEventFilter(this);
    ui->proxyPort->installEventFilter(this);

/* Window elements init */
#ifdef Q_OS_MAC
    /* remove Window tab on Mac */
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWindow));
#endif

    /* remove Wallet tab in case of -disablewallet */
    if (!enableWallet) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWallet));
    }

    /* Display elements init */

    /* Number of displayed decimal digits selector */
    QString digits;
    for (int index = 2; index <= 8; index++) {
        digits.setNum(index);
        ui->digits->addItem(digits, digits);
    }

    /* Theme selector static themes */
    ui->theme->addItem(QString("Default"), QVariant("default"));

    /* Preferred Zerocoin Denominations */
    ui->preferredDenom->addItem(QString(tr("Any")), QVariant("0"));
    ui->preferredDenom->addItem(QString("1"), QVariant("1"));
    ui->preferredDenom->addItem(QString("5"), QVariant("5"));
    ui->preferredDenom->addItem(QString("10"), QVariant("10"));
    ui->preferredDenom->addItem(QString("50"), QVariant("50"));
    ui->preferredDenom->addItem(QString("100"), QVariant("100"));
    ui->preferredDenom->addItem(QString("500"), QVariant("500"));
    ui->preferredDenom->addItem(QString("1000"), QVariant("1000"));
    ui->preferredDenom->addItem(QString("5000"), QVariant("5000"));

    /* Theme selector external themes */
    fs::path pathAddr = GetDataDir() / "themes";
    QDir dir(pathAddr.string().c_str());
    dir.setFilter(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    QFileInfoList list = dir.entryInfoList();

    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        ui->theme->addItem(fileInfo.fileName(), QVariant(fileInfo.fileName()));
    }

    /* Language selector */
    QDir translations(":translations");
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for (const QString& langStr : translations.entryList()) {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
        else
        {
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
    }
    ui->thirdPartyTxUrls->setPlaceholderText("https://example.com/tx/%s");

    ui->unit->setModel(new BitcoinUnits(this));

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* setup/change UI elements when proxy IP is invalid/valid */
    connect(this, &OptionsDialog::proxyIpChecks, this, &OptionsDialog::doProxyIpChecks);
}

OptionsDialog::~OptionsDialog()
{
    GUIUtil::saveWindowGeometry("nOptionsDialogWindow", this);
    delete ui;
}

void OptionsDialog::setModel(OptionsModel* model)
{
    this->model = model;

    if (model) {
        /* check if client restart is needed and show persistent message */
        if (model->isRestartRequired())
            showRestartWarning(true);

        QString strLabel = model->getOverriddenByCommandLine();
        if (strLabel.isEmpty())
            strLabel = tr("none");
        ui->overriddenByCommandLineLabel->setText(strLabel);

        mapper->setModel(model);
        setMapper();
        mapper->toFirst();
    }

    /* warn when one of the following settings changes by user action (placed here so init via mapper doesn't trigger them) */

    /* Main */
    connect(ui->databaseCache, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &OptionsDialog::showRestartWarning);
    connect(ui->threadsScriptVerif, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &OptionsDialog::showRestartWarning);
    /* Wallet */
    connect(ui->spendZeroConfChange, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    /* Network */
    connect(ui->allowIncoming, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    connect(ui->connectSocks, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    /* Display */
    connect(ui->digits, static_cast<void (QValueComboBox::*)()>(&QValueComboBox::valueChanged), [this]{ showRestartWarning(); });
    connect(ui->theme, static_cast<void (QValueComboBox::*)()>(&QValueComboBox::valueChanged), [this]{ showRestartWarning(); });
    connect(ui->lang, static_cast<void (QValueComboBox::*)()>(&QValueComboBox::valueChanged), [this]{ showRestartWarning(); });
    connect(ui->thirdPartyTxUrls, &QLineEdit::textChanged, [this]{ showRestartWarning(); });
    connect(ui->showMasternodesTab, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->bitcoinAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);
    // Zeromint Enabled
    mapper->addMapping(ui->checkBoxZeromintEnable, OptionsModel::ZeromintEnable);
    // Zeromint Addresses
    mapper->addMapping(ui->checkBoxZeromintAddresses, OptionsModel::ZeromintAddresses);
    // Zerocoin mint percentage
    mapper->addMapping(ui->zeromintPercentage, OptionsModel::ZeromintPercentage);
    // Zerocoin preferred denomination
    mapper->addMapping(ui->preferredDenom, OptionsModel::ZeromintPrefDenom);

    /* Wallet */
    mapper->addMapping(ui->spendZeroConfChange, OptionsModel::SpendZeroConfChange);
    mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);
    mapper->addMapping(ui->spinBoxStakeSplitThreshold, OptionsModel::StakeSplitThreshold);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->digits, OptionsModel::Digits);
    mapper->addMapping(ui->theme, OptionsModel::Theme);
    mapper->addMapping(ui->theme, OptionsModel::Theme);
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);
    mapper->addMapping(ui->checkBoxHideZeroBalances, OptionsModel::HideZeroBalances);
    mapper->addMapping(ui->checkBoxHideOrphans, OptionsModel::HideOrphans);

    /* Masternode Tab */
    mapper->addMapping(ui->showMasternodesTab, OptionsModel::ShowMasternodesTab);
}

void OptionsDialog::enableOkButton()
{
    /* prevent enabling of the OK button when data modified, if there is an invalid proxy address present */
    if (fProxyIpValid)
        setOkButtonState(true);
}

void OptionsDialog::disableOkButton()
{
    setOkButtonState(false);
}

void OptionsDialog::setOkButtonState(bool fState)
{
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_resetButton_clicked()
{
    if (model) {
        // confirmation dialog
        QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm options reset"),
            tr("Client restart required to activate changes.") + "<br><br>" + tr("Client will be shutdown, do you want to proceed?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if (btnRetVal == QMessageBox::Cancel)
            return;

        /* reset all options and close GUI */
        model->Reset();
        QApplication::quit();
    }
}

void OptionsDialog::on_okButton_clicked()
{
    mapper->submit();
    pwalletMain->MarkDirty();
    accept();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::showRestartWarning(bool fPersistent)
{
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if (fPersistent) {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    } else {
        ui->statusLabel->setText(tr("This change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // Todo: should perhaps be a class attribute, if we extend the use of statusLabel
        QTimer::singleShot(10000, this, &OptionsDialog::clearStatusLabel);
    }
}

void OptionsDialog::clearStatusLabel()
{
    ui->statusLabel->clear();
}

void OptionsDialog::updateHideOrphans(bool fHide)
{
    if(ui->checkBoxHideOrphans->isChecked() != fHide)
        ui->checkBoxHideOrphans->setChecked(fHide);
}

void OptionsDialog::doProxyIpChecks(QValidatedLineEdit* pUiProxyIp, QLineEdit* pUiProxyPort)
{
    const std::string strAddrProxy = pUiProxyIp->text().toStdString();
    const int nProxyPort = pUiProxyPort->text().toInt();
    CService addrProxy(LookupNumeric(strAddrProxy.c_str(), nProxyPort));

    // Check for a valid IPv4 / IPv6 address
    if (!(fProxyIpValid = addrProxy.IsValid())) {
        disableOkButton();
        pUiProxyIp->setValid(false);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
        return;
    }
    // Check proxy port
    if (!pUiProxyPort->hasAcceptableInput()) {
        disableOkButton();
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy port is invalid."));
        return;
    }

    proxyType checkProxy = proxyType(addrProxy);
    if (!checkProxy.IsValid()) {
        disableOkButton();
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy settings are invalid."));
        return;
    }

    enableOkButton();
    ui->statusLabel->clear();
}

bool OptionsDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::FocusOut) {
        if (object == ui->proxyIp || object == ui->proxyPort) {
            Q_EMIT proxyIpChecks(ui->proxyIp, ui->proxyPort);
        }
    }
    return QDialog::eventFilter(object, event);
}

void OptionsDialog::setCurrentIndex(int index)
{
    ui->tabWidget->setCurrentIndex(index);
}
