//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Sombe/settings/settingsbackupwallet.h"
#include "qt/Sombe/settings/forms/ui_settingsbackupwallet.h"
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include "guiutil.h"
#include "qt/Sombe/qtutils.h"
#include "guiinterface.h"
#include "qt/Sombe/qtutils.h"
SettingsBackupWallet::SettingsBackupWallet(SombeGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsBackupWallet)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelTitle_2->setProperty("cssClass", "text-title-screen");
    ui->labelDivider->setProperty("cssClass", "container-divider");

    // Subtitles
    setCssProperty({ui->labelSubtitle1, ui->labelSubtitle_2}, "text-subtitle");

    // Location
    ui->labelSubtitleLocation->setProperty("cssClass", "text-title");
    ui->pushButtonDocuments->setProperty("cssClass", "btn-edit-primary-folder");
    setShadow(ui->pushButtonDocuments);

    // Buttons
    setCssBtnPrimary(ui->pushButtonSave_2);

    connect(ui->pushButtonDocuments, &QPushButton::clicked, this, &SettingsBackupWallet::selectFileOutput);
    connect(ui->pushButtonSave_2, &QPushButton::clicked, this, &SettingsBackupWallet::changePassphrase);
}

void SettingsBackupWallet::selectFileOutput()
{
    QString filename = GUIUtil::getSaveFileName(this,
                                        tr("Backup Wallet"), QString(),
                                        tr("Wallet Data (*.dat)"), NULL);

    if (!filename.isEmpty() && walletModel) {
        ui->pushButtonDocuments->setText(filename);
        inform(walletModel->backupWallet(filename) ? tr("Backup created") : tr("Backup creation failed"));
    } else {
        ui->pushButtonDocuments->setText(tr("Select folder..."));
        inform(tr("Please select a folder to export the backup first."));
    }
}

void SettingsBackupWallet::changePassphrase()
{
    showHideOp(true);
    AskPassphraseDialog *dlg = nullptr;
    if (walletModel->getEncryptionStatus() == WalletModel::Unencrypted) {
        dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::Encrypt, window,
                walletModel, AskPassphraseDialog::Context::Encrypt);
    } else {
        dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::ChangePass, window,
                walletModel, AskPassphraseDialog::Context::ChangePass);
    }
    dlg->adjustSize();
    Q_EMIT execDialog(dlg);
    dlg->deleteLater();
}

SettingsBackupWallet::~SettingsBackupWallet()
{
    delete ui;
}
