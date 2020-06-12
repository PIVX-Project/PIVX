// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/sendconfirmdialog.h"
#include "qt/pivx/forms/ui_sendconfirmdialog.h"
#include "bitcoinunits.h"
#include "walletmodel.h"
#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "wallet/wallet.h"
#include "guiutil.h"
#include "qt/pivx/qtutils.h"
#include <QList>
#include <QDateTime>

TxDetailDialog::TxDetailDialog(QWidget *parent, bool _isConfirmDialog, const QString& warningStr) :
    FocusedDialog(parent),
    ui(new Ui::TxDetailDialog),
    isConfirmDialog(_isConfirmDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Container
    setCssProperty(ui->frame, "container-dialog");
    setCssProperty(ui->labelTitle, "text-title-dialog");

    // Labels
    setCssProperty(ui->labelWarning, "text-title2-dialog");
    setCssProperty({ui->labelAmount, ui->labelSend, ui->labelInputs, ui->labelFee, ui->labelChange, ui->labelId, ui->labelSize, ui->labelStatus, ui->labelConfirmations, ui->labelDate}, "text-subtitle");
    setCssProperty({ui->labelDividerID, ui->labelDividerOutputs, ui->labelDividerPrevtx, ui->labelDividerFeeSize, ui->labelDividerChange, ui->labelDividerConfs}, "container-divider");
    setCssProperty({ui->textAmount, ui->textSendLabel, ui->textInputs, ui->textFee, ui->textChange, ui->textId, ui->textSize, ui->textStatus, ui->textConfirmations, ui->textDate} , "text-body3-dialog");

    setCssProperty(ui->pushCopy, "ic-copy-big");
    setCssProperty({ui->pushInputs, ui->pushOutputs}, "ic-arrow-down");
    setCssProperty(ui->btnEsc, "ic-close");

    ui->labelWarning->setVisible(false);
    ui->gridInputs->setVisible(false);
    ui->outputsScrollArea->setVisible(false);

    // hide change address for now
    ui->contentChangeAddress->setVisible(false);
    ui->labelDividerChange->setVisible(false);

    setCssProperty({ui->labelOutputIndex, ui->textSend, ui->labelTitlePrevTx}, "text-body2-dialog");

    if (isConfirmDialog) {
        ui->labelTitle->setText(tr("Confirm Your Transaction"));
        setCssProperty(ui->btnCancel, "btn-dialog-cancel");
        ui->btnSave->setText(tr("SEND"));
        setCssBtnPrimary(ui->btnSave);
        if (!warningStr.isEmpty()) {
            ui->labelWarning->setVisible(true);
            ui->labelWarning->setText(warningStr);
        } else {
            ui->labelWarning->setVisible(false);
        }

        // hide id / confirmations / date / status / size
        ui->contentID->setVisible(false);
        ui->labelDividerID->setVisible(false);
        ui->gridConfDateStatus->setVisible(false);
        ui->labelDividerConfs->setVisible(false);
        ui->contentSize->setVisible(false);

        connect(ui->btnCancel, &QPushButton::clicked, this, &TxDetailDialog::close);
        connect(ui->btnSave, &QPushButton::clicked, [this](){accept();});
    } else {
        ui->labelTitle->setText(tr("Transaction Details"));
        ui->containerButtons->setVisible(false);
    }

    connect(ui->btnEsc, &QPushButton::clicked, this, &TxDetailDialog::close);
    connect(ui->pushInputs, &QPushButton::clicked, this, &TxDetailDialog::onInputsClicked);
    connect(ui->pushOutputs, &QPushButton::clicked, this, &TxDetailDialog::onOutputsClicked);
}

void TxDetailDialog::setData(WalletModel *model, const QModelIndex &index)
{
    this->model = model;
    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());
    QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
    QString address = index.data(Qt::DisplayRole).toString();
    qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
    QString amountText = BitcoinUnits::formatWithUnit(nDisplayUnit, amount, true, BitcoinUnits::separatorAlways);
    ui->textAmount->setText(amountText);

    const CWalletTx* tx = model->getTx(rec->hash);
    if (tx) {
        this->txHash = rec->hash;
        QString hash = QString::fromStdString(tx->GetHash().GetHex());
        ui->textId->setText(hash.left(20) + "..." + hash.right(20));
        ui->textId->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (tx->vout.size() == 1) {
            ui->textSendLabel->setText(address);
        } else {
            ui->textSendLabel->setText(QString::number(tx->vout.size()) + " recipients");
        }
        ui->textSend->setVisible(false);

        ui->textInputs->setText(QString::number(tx->vin.size()));
        ui->textConfirmations->setText(QString::number(rec->status.depth));
        ui->textDate->setText(GUIUtil::dateTimeStrWithSeconds(date));
        ui->textStatus->setText(QString::fromStdString(rec->statusToString()));
        ui->textSize->setText(QString::number(rec->size) + " bytes");

        connect(ui->pushCopy, &QPushButton::clicked, [this](){
            GUIUtil::setClipboard(QString::fromStdString(this->txHash.GetHex()));
            if (!snackBar) snackBar = new SnackBar(nullptr, this);
            snackBar->setText(tr("ID copied"));
            snackBar->resize(this->width(), snackBar->height());
            openDialog(snackBar, this);
        });
    }

}

void TxDetailDialog::setData(WalletModel *model, WalletModelTransaction &tx)
{
    this->model = model;
    this->tx = &tx;
    CAmount txFee = tx.getTransactionFee();
    CAmount totalAmount = tx.getTotalTransactionAmount() + txFee;

    ui->textAmount->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, totalAmount, false, BitcoinUnits::separatorAlways) + " (Fee included)");
    int nRecipients = tx.getRecipients().size();
    if (nRecipients == 1) {
        const SendCoinsRecipient& recipient = tx.getRecipients().at(0);
        if (recipient.isP2CS) {
            ui->labelSend->setText(tr("Delegating to"));
        }
        if (recipient.label.isEmpty()) { // If there is no label, then do not show the blank space.
            ui->textSendLabel->setText(recipient.address);
            ui->textSend->setVisible(false);
        } else {
            ui->textSend->setText(recipient.address);
            ui->textSendLabel->setText(recipient.label);
        }
        ui->pushOutputs->setVisible(false);
    } else {
        ui->textSendLabel->setText(QString::number(nRecipients) + " recipients");
        ui->textSend->setVisible(false);
    }
    ui->textInputs->setText(QString::number(tx.getTransaction()->vin.size()));
    ui->textFee->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, txFee, false, BitcoinUnits::separatorAlways));
}

void TxDetailDialog::accept()
{
    if (isConfirmDialog) {
        this->confirm = true;
        this->sendStatus = model->sendCoins(*this->tx);
    }
    QDialog::accept();
}

void TxDetailDialog::onInputsClicked()
{
    if (ui->gridInputs->isVisible()) {
        ui->gridInputs->setVisible(false);
    } else {
        ui->gridInputs->setVisible(true);
        if (!inputsLoaded) {
            inputsLoaded = true;
            const CWalletTx* tx = (this->tx) ? this->tx->getTransaction() : model->getTx(this->txHash);
            if (tx) {
                ui->gridInputs->setMinimumHeight(50 + (50 * tx->vin.size()));
                int i = 1;
                for (const CTxIn &in : tx->vin) {
                    QString hash = QString::fromStdString(in.prevout.hash.GetHex());
                    QLabel *label_txid = new QLabel(hash.left(18) + "..." + hash.right(18));
                    QLabel *label_txidn = new QLabel(QString::number(in.prevout.n));
                    label_txidn->setAlignment(Qt::AlignCenter | Qt::AlignRight);
                    setCssProperty({label_txid, label_txidn}, "text-body2-dialog");

                    ui->gridLayoutInput->addWidget(label_txid,i,0);
                    ui->gridLayoutInput->addWidget(label_txidn,i,1);
                    i++;
                }
            }
        }
    }
}

void TxDetailDialog::onOutputsClicked()
{
    if (ui->outputsScrollArea->isVisible()) {
        ui->outputsScrollArea->setVisible(false);
    } else {
        ui->outputsScrollArea->setVisible(true);
        if (!outputsLoaded) {
            outputsLoaded = true;
            QGridLayout* layoutGrid = new QGridLayout();
            layoutGrid->setContentsMargins(0,0,12,0);
            ui->container_outputs_base->setLayout(layoutGrid);

            const CWalletTx* tx = (this->tx) ? this->tx->getTransaction() : model->getTx(this->txHash);
            if (tx) {
                int i = 0;
                for (const CTxOut &out : tx->vout) {
                    QString labelRes;
                    CTxDestination dest;
                    bool isCsAddress = out.scriptPubKey.IsPayToColdStaking();
                    if (ExtractDestination(out.scriptPubKey, dest, isCsAddress)) {
                        std::string address = EncodeDestination(dest, isCsAddress);
                        labelRes = QString::fromStdString(address);
                        labelRes = labelRes.left(16) + "..." + labelRes.right(16);
                    } else {
                        labelRes = tr("Unknown");
                    }
                    QLabel *label_address = new QLabel(labelRes);
                    QLabel *label_value = new QLabel(BitcoinUnits::formatWithUnit(nDisplayUnit, out.nValue, false, BitcoinUnits::separatorAlways));
                    label_value->setAlignment(Qt::AlignCenter | Qt::AlignRight);
                    setCssProperty({label_address, label_value}, "text-body2-dialog");
                    layoutGrid->addWidget(label_address,i,0);
                    layoutGrid->addWidget(label_value,i,0);
                    i++;
                }
            }
        }
    }
}

void TxDetailDialog::reject()
{
    if (snackBar && snackBar->isVisible()) snackBar->hide();
    QDialog::reject();
}

TxDetailDialog::~TxDetailDialog()
{
    if (snackBar) delete snackBar;
    delete ui;
}
