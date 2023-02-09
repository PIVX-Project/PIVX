// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/mninfodialog.h"
#include "qt/pivx/forms/ui_mninfodialog.h"

#include "guiutil.h"
#include "qt/pivx/qtutils.h"

MnInfoDialog::MnInfoDialog(QWidget *parent) :
    FocusedDialog(parent),
    ui(new Ui::MnInfoDialog)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");
    setCssProperty(ui->labelTitle, "text-title-dialog");
    setCssTextBodyDialog({ui->labelAmount, ui->labelSend, ui->labelInputs, ui->labelFee, ui->labelId,
                          ui->labelOwnerPayoutAddr, ui->labelOperatorPubKey, ui->labelOperatorSk, ui->labelVoterAddr});
    setCssProperty({ui->labelDivider1, ui->labelDivider4, ui->labelDivider6, ui->labelDivider7,
                    ui->labelDivider8, ui->labelDivider9, ui->labelDivider10, ui->labelDivider11,
                    ui->labelDivider12, ui->labelDivider13}, "container-divider");
    setCssTextBodyDialog({ui->textAmount, ui->textAddress, ui->textInputs, ui->textStatus,
                          ui->textId, ui->textExport, ui->textOwnerPayoutAddr, ui->textOperatorPubKey, ui->textOperatorSk,
                          ui->textVoterAddr});
    setCssProperty({ui->pushCopy, ui->pushCopyId, ui->pushExport, ui->pushCopyOwnerPayoutAddr,
                    ui->pushCopyOperatorPubKey, ui->pushCopyOperatorSk, ui->pushCopyVoterAddr}, "ic-copy-big");
    setCssProperty(ui->btnEsc, "ic-close");
    connect(ui->btnEsc, &QPushButton::clicked, this, &MnInfoDialog::close);
    connect(ui->pushCopy, &QPushButton::clicked, [this](){
        if (dmnData) copyInform(QString::fromStdString(dmnData->ownerMainAddr), tr("Masternode owner address copied"));
        else copyInform(pubKey, tr("Masternode public key copied"));
    });
    connect(ui->pushCopyId, &QPushButton::clicked, [this](){ copyInform(txId, tr("Collateral tx id copied")); });
    connect(ui->pushExport, &QPushButton::clicked, [this](){ exportMN = true; accept(); });
    connect(ui->pushCopyOperatorPubKey, &QPushButton::clicked, [this](){
        if (dmnData) copyInform(QString::fromStdString(dmnData->operatorPk), tr("Operator public key copied"));
    });
    connect(ui->pushCopyOperatorSk, &QPushButton::clicked, [this](){
        if (dmnData) copyInform(QString::fromStdString(dmnData->operatorSk), tr("Operator secret key copied"));
    });
    connect(ui->pushCopyOwnerPayoutAddr, &QPushButton::clicked, [this](){
        if (dmnData) copyInform(QString::fromStdString(dmnData->ownerPayoutAddr), tr("Owner payout script copied"));
    });
    connect(ui->pushCopyVoterAddr, &QPushButton::clicked, [this](){
        if (dmnData) copyInform(QString::fromStdString(dmnData->votingAddr), tr("Voter address copied"));
    });
}

void MnInfoDialog::setDMNDataVisible(bool show)
{
    ui->contentOwnerPayoutAddr->setVisible(show);
    ui->contentOperatorPubKey->setVisible(show);
    ui->contentOperatorSk->setVisible(show);
    ui->contentVoterAddr->setVisible(show);
    ui->labelDivider9->setVisible(show);
    ui->labelDivider11->setVisible(show);
    ui->labelDivider12->setVisible(show);
    ui->labelDivider13->setVisible(show);
}

void MnInfoDialog::setData(const QString& _pubKey,
                           const QString& name,
                           const QString& address,
                           const QString& _txId,
                           const QString& outputIndex,
                           const QString& status,
                           const Optional<DMNData>& _dmnData)
{
    pubKey = _pubKey;
    txId = _txId;
    setShortTextIfExceedSize(ui->textId, _pubKey, 13, 20);
    setShortTextIfExceedSize(ui->textAmount, _txId, 12, 20);
    setShortTextIfExceedSize(ui->textAddress, address, 12, 40);
    ui->textInputs->setText(outputIndex);
    ui->textStatus->setText(status);

    dmnData = _dmnData;
    if (dmnData) {
        setDMNDataVisible(true);
        ui->labelId->setText(tr("Owner Address"));
        setShortText(ui->textId, QString::fromStdString(dmnData->ownerMainAddr), 15);
        setShortText(ui->textOwnerPayoutAddr, QString::fromStdString(dmnData->ownerPayoutAddr), 15);
        setShortText(ui->textVoterAddr, QString::fromStdString(dmnData->votingAddr), 15);
        setShortTextIfExceedSize(ui->textOperatorPubKey, QString::fromStdString(dmnData->operatorPk), 12, 20);
        if (!dmnData->operatorSk.empty())
            setShortTextIfExceedSize(ui->textOperatorSk, QString::fromStdString(dmnData->operatorSk), 12, 20);
    } else {
        ui->labelId->setText(tr("Public Key"));
        setDMNDataVisible(false);
    }
}

void MnInfoDialog::copyInform(const QString& copyStr, const QString& message)
{
    GUIUtil::setClipboard(copyStr);
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(tr(message.toStdString().c_str()));
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

void MnInfoDialog::reject()
{
    if (snackBar && snackBar->isVisible()) snackBar->hide();
    QDialog::reject();
}

MnInfoDialog::~MnInfoDialog()
{
    if (snackBar) delete snackBar;
    delete ui;
}
