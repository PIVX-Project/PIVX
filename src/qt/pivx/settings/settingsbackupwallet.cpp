// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingsbackupwallet.h"
#include "qt/pivx/settings/forms/ui_settingsbackupwallet.h"
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include "guiutil.h"
#include "qt/pivx/qtutils.h"
#include "guiinterface.h"
#include "qt/pivx/qtutils.h"
SettingsBackupWallet::SettingsBackupWallet(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsBackupWallet)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title
    ui->labelTitle->setText(tr("Backup Wallet "));
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    ui->labelTitle_2->setText(tr("Change Wallet Passphrase"));
    ui->labelTitle_2->setProperty("cssClass", "text-title-screen");
    ui->labelDivider->setProperty("cssClass", "container-divider");

    // Subtitle
    ui->labelSubtitle1->setText(tr("Keep your wallet safe doing regular backups, store your backup file externally.\nThis option creates a wallet.dat file that can be used to recover your whole balance (transactions and addresses) from another device."));
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelSubtitle_2->setText(tr("Change your wallet encryption passphrase for another one that you like. This will decrypt and encrypt your whole data under the new passphrase.\nRemember to write it down to not lose access to your funds."));
    ui->labelSubtitle_2->setProperty("cssClass", "text-subtitle");

    // Location
    ui->labelSubtitleLocation->setText(tr("Where"));
    ui->labelSubtitleLocation->setProperty("cssClass", "text-title");

    ui->pushButtonDocuments->setText(tr("Set a folder location"));
    ui->pushButtonDocuments->setProperty("cssClass", "btn-edit-primary-folder");
    setShadow(ui->pushButtonDocuments);

    // Buttons
    ui->pushButtonSave->setText(tr("Backup"));
    setCssBtnPrimary(ui->pushButtonSave);

    ui->pushButtonSave_2->setText(tr("Change Passphrase"));
    setCssBtnPrimary(ui->pushButtonSave_2);

    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(backupWallet()));
    connect(ui->pushButtonDocuments, SIGNAL(clicked()), this, SLOT(selectFileOutput()));
    connect(ui->pushButtonSave_2, SIGNAL(clicked()), this, SLOT(changePassphrase()));
}

void SettingsBackupWallet::selectFileOutput(){
    QString filenameRet = GUIUtil::getSaveFileName(this,
                                        tr("Backup Wallet"), QString(),
                                        tr("Wallet Data (*.dat)"), NULL);

    if (!filenameRet.isEmpty()) {
        filename = filenameRet;
        ui->pushButtonDocuments->setText(filename);
    }
}

void SettingsBackupWallet::backupWallet(){
    if(walletModel && !filename.isEmpty()) {
        inform(walletModel->backupWallet(filename) ? tr("Backup created") : tr("Backup creation failed"));
        filename = QString();
        ui->pushButtonDocuments->setText(tr("Set a folder location"));
    } else {
        inform(tr("Please select a folder to export the backup first."));
    }
}

void SettingsBackupWallet::changePassphrase(){
    showHideOp(true);
    AskPassphraseDialog *dlg = nullptr;
    if (walletModel->getEncryptionStatus() == WalletModel::Unencrypted) {
        dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::ChangePass, window,
                                  walletModel, AskPassphraseDialog::Context::ChangePass);
    } else {
        dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::Encrypt, window,
                                      walletModel, AskPassphraseDialog::Context::Encrypt);
    }
    dlg->adjustSize();
    emit execDialog(dlg);
    dlg->deleteLater();
}

SettingsBackupWallet::~SettingsBackupWallet(){
    delete ui;
}
