// Copyright (c) 2019-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/hemis/gminfodialog.h"
#include "qt/hemis/forms/ui_gminfodialog.h"

#include "guiutil.h"
#include "qt/pivx/qtutils.h"

GmInfoDialog::GmInfoDialog(QWidget *parent) :
    FocusedDialog(parent),
    ui(new Ui::GmInfoDialog)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");
    setCssProperty(ui->labelTitle, "text-title-dialog");
    setCssTextBodyDialog({ui->labelAmount, ui->labelSend, ui->labelInputs, ui->labelFee, ui->labelId});
    setCssProperty({ui->labelDivider1, ui->labelDivider4, ui->labelDivider6, ui->labelDivider7, ui->labelDivider8, ui->labelDivider9}, "container-divider");
    setCssTextBodyDialog({ui->textAmount, ui->textAddress, ui->textInputs, ui->textStatus, ui->textId, ui->textExport});
    setCssProperty({ui->pushCopy, ui->pushCopyId, ui->pushExport}, "ic-copy-big");
    setCssProperty(ui->btnEsc, "ic-close");
    connect(ui->btnEsc, &QPushButton::clicked, this, &GmInfoDialog::close);
    connect(ui->pushCopy, &QPushButton::clicked, [this](){ copyInform(pubKey, tr("Gamemaster public key copied")); });
    connect(ui->pushCopyId, &QPushButton::clicked, [this](){ copyInform(txId, tr("Collateral tx id copied")); });
    connect(ui->pushExport, &QPushButton::clicked, [this](){ exportGM = true; accept(); });
}

void GmInfoDialog::setData(const QString& _pubKey, const QString& name, const QString& address, const QString& _txId, const QString& outputIndex, const QString& status)
{
    this->pubKey = _pubKey;
    this->txId = _txId;
    QString shortPubKey = _pubKey;
    QString shortTxId = _txId;
    QString shortAddress = address;
    if (shortPubKey.length() > 20) {
        shortPubKey = shortPubKey.left(13) + "..." + shortPubKey.right(13);
    }
    if (shortTxId.length() > 20) {
        shortTxId = shortTxId.left(12) + "..." + shortTxId.right(12);
    }
    if (shortAddress.length() >= 40) {
        shortAddress = shortAddress.left(11) + "..." + shortAddress.right(20);
    }
    ui->textId->setText(shortPubKey);
    ui->textAddress->setText(shortAddress);
    ui->textAmount->setText(shortTxId);
    ui->textInputs->setText(outputIndex);
    ui->textStatus->setText(status);
}

void GmInfoDialog::copyInform(const QString& copyStr, const QString& message)
{
    GUIUtil::setClipboard(copyStr);
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(tr(message.toStdString().c_str()));
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

void GmInfoDialog::reject()
{
    if (snackBar && snackBar->isVisible()) snackBar->hide();
    QDialog::reject();
}

GmInfoDialog::~GmInfoDialog()
{
    if (snackBar) delete snackBar;
    delete ui;
}
