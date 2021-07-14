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
    setCssProperty({ui->labelAmount, ui->labelSend, ui->labelInputs, ui->labelFee, ui->labelChange, ui->labelId, ui->labelSize, ui->labelStatus, ui->labelConfirmations, ui->labelDate, ui->labelMemo}, "text-subtitle");
    setCssProperty({ui->labelDividerID, ui->labelDividerOutputs, ui->labelDividerPrevtx, ui->labelDividerFeeSize, ui->labelDividerChange, ui->labelDividerConfs, ui->labelDividerMemo}, "container-divider");
    setCssProperty({ui->textAmount, ui->textSendLabel, ui->textInputs, ui->textFee, ui->textChange, ui->textId, ui->textSize, ui->textStatus, ui->textConfirmations, ui->textDate, ui->textMemo} , "text-body3-dialog");

    setCssProperty({ui->pushCopy, ui->pushCopyMemo}, "ic-copy-big");
    setCssProperty({ui->pushInputs, ui->pushOutputs}, "ic-arrow-down");
    setCssProperty(ui->btnEsc, "ic-close");

    ui->labelWarning->setVisible(false);
    ui->gridInputs->setVisible(false);
    ui->outputsScrollArea->setVisible(false);

    // hide change address for now
    ui->contentChangeAddress->setVisible(false);
    ui->labelDividerChange->setVisible(false);

    // Memo text
    ui->textMemo->setProperty("cssClass","edit-primary-no-border");

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

void TxDetailDialog::setInputsType(CTransactionRef tx)
{
    if (tx->sapData && tx->sapData->vShieldedSpend.empty()) {
        ui->labelTitlePrevTx->setText(tr("Previous Transaction"));
        ui->labelOutputIndex->setText(tr("Output Index"));
    } else {
        ui->labelTitlePrevTx->setText(tr("Note From Address"));
        ui->labelOutputIndex->setText(tr("Index"));
    }
}

void TxDetailDialog::setData(WalletModel *_model, const QModelIndex &index)
{
    this->model = _model;
    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());
    QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
    QString address = index.data(Qt::DisplayRole).toString();
    qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
    QString amountText = BitcoinUnits::formatWithUnit(nDisplayUnit, amount, true, BitcoinUnits::separatorAlways);
    ui->textAmount->setText(amountText);

    const CWalletTx* _tx = model->getTx(rec->hash);
    if (_tx) {
        this->txHash = rec->hash;
        QString hash = QString::fromStdString(_tx->GetHash().GetHex());
        ui->textId->setText(hash.left(20) + "..." + hash.right(20));
        ui->textId->setTextInteractionFlags(Qt::TextSelectableByMouse);
        // future: subdivide shielded and transparent by type and
        // do not show send xxx recipients for txes with a single output + change (show the address directly).
        if (_tx->tx->vout.size() == 1 || (_tx->tx->sapData && _tx->tx->sapData->vShieldedOutput.size() == 1)) {
            ui->textSendLabel->setText((address.size() < 40) ? address : address.left(20) + "..." + address.right(20));
        } else {
            ui->textSendLabel->setText(QString::number(_tx->tx->vout.size() +
                (_tx->tx->sapData ? _tx->tx->sapData->vShieldedOutput.size() : 0)) + " recipients");
        }
        ui->textSend->setVisible(false);
        isShieldedToShieldedRecv = rec->type == TransactionRecord::Type::RecvWithShieldedAddress;

        // Do not show inputs button if there is no data to show
        QString shieldedInputsExtraMsg = "";
        if (isShieldedToShieldedRecv) {
            ui->pushInputs->setVisible(false);
            shieldedInputsExtraMsg = " shielded";
        }

        setInputsType(_tx->tx);
        int inputsSize = (_tx->tx->sapData && !_tx->tx->sapData->vShieldedSpend.empty()) ? _tx->tx->sapData->vShieldedSpend.size() : _tx->tx->vin.size();
        ui->textInputs->setText(QString::number(inputsSize) + shieldedInputsExtraMsg);
        ui->textConfirmations->setText(QString::number(rec->status.depth));
        ui->textDate->setText(GUIUtil::dateTimeStrWithSeconds(date));
        ui->textStatus->setText(QString::fromStdString(rec->statusToString()));
        ui->textSize->setText(QString::number(rec->size) + " bytes");

        // If there is a memo in this record
        if (rec->memo) {
            ui->textMemo->insertPlainText(QString::fromStdString(*rec->memo));
            ui->contentMemo->setVisible(true);
            ui->labelDividerMemo->setVisible(true);
            ui->textMemo->adjustSize();
            ui->textMemo->moveCursor(QTextCursor::Start);
            ui->textMemo->ensureCursorVisible();
        } else {
            ui->contentMemo->setVisible(false);
            ui->labelDividerMemo->setVisible(false);
            adjustSize();
        }

        connect(ui->pushCopy, &QPushButton::clicked, [this](){
            GUIUtil::setClipboard(QString::fromStdString(this->txHash.GetHex()));
            if (!snackBar) snackBar = new SnackBar(nullptr, this);
            snackBar->setText(tr("ID copied"));
            snackBar->resize(this->width(), snackBar->height());
            openDialog(snackBar, this);
        });

        connect(ui->pushCopyMemo, &QPushButton::clicked, [this, rec](){
            GUIUtil::setClipboard(QString::fromStdString(*rec->memo));
            if (!snackBar) snackBar = new SnackBar(nullptr, this);
            snackBar->setText(tr("Memo copied"));
            snackBar->resize(this->width(), snackBar->height());
            openDialog(snackBar, this);
        });
    }

}

QString formatAdressToShow(const QString& address)
{
    QString addressToShow;
    if (address.size() > 60) {
        addressToShow = address.left(57) + "\n" + address.mid(57);
    } else {
        addressToShow = address;
    }
    return addressToShow;
}

void TxDetailDialog::setData(WalletModel *_model, WalletModelTransaction* _tx)
{
    this->model = _model;
    this->tx = _tx;
    CAmount txFee = tx->getTransactionFee();

    // inputs label
    CTransactionRef walletTx = tx->getTransaction();
    setInputsType(walletTx);

    CAmount totalAmount = tx->getTotalTransactionAmount();
    if (tx->subtractFeeFromRecipents() == 0) totalAmount += txFee;

    ui->textAmount->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, totalAmount, false, BitcoinUnits::separatorAlways) + " (Fee included)");

    const QList<SendCoinsRecipient>& recipients = tx->getRecipients();
    int nRecipients = recipients.size();
    if (nRecipients == 1) {
        const SendCoinsRecipient& recipient = recipients.at(0);
        if (recipient.isP2CS) {
            ui->labelSend->setText(tr("Delegating to"));
        }
        if (recipient.isShieldedAddr) {
            ui->labelSend->setText(tr("Shielding to"));
        }
        if (recipient.label.isEmpty()) { // If there is no label, then do not show the blank space.
            ui->textSend->setVisible(false);
            ui->textSendLabel->setText(formatAdressToShow(recipient.address));
        } else {
            ui->textSend->setText(formatAdressToShow(recipient.address));
            ui->textSendLabel->setText(recipient.label);
        }
        ui->pushOutputs->setVisible(false);

        // If there is a single output, then show the memo.
        if (!recipient.message.isEmpty()) {
            ui->textMemo->insertPlainText(recipient.message);
            ui->contentMemo->setVisible(true);
            ui->labelDividerMemo->setVisible(true);
            ui->textMemo->moveCursor(QTextCursor::Start);
            ui->textMemo->ensureCursorVisible();
        } else {
            ui->contentMemo->setVisible(false);
            ui->labelDividerMemo->setVisible(false);
        }
    } else {
        ui->textSendLabel->setText(QString::number(nRecipients) + " recipients");
        ui->textSend->setVisible(false);
        ui->contentMemo->setVisible(false);
        ui->labelDividerMemo->setVisible(false);
    }

    int inputsSize = (walletTx->sapData && !walletTx->sapData->vShieldedSpend.empty()) ? walletTx->sapData->vShieldedSpend.size() : walletTx->vin.size();
    ui->textInputs->setText(QString::number(inputsSize));
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

void loadInputs(const QString& leftLabel, const QString& rightLabel, QGridLayout *gridLayoutInput, int pos)
{
    QLabel *label_txid = new QLabel(leftLabel);
    QLabel *label_txidn = new QLabel(rightLabel);
    label_txidn->setAlignment(Qt::AlignCenter | Qt::AlignRight);
    setCssProperty({label_txid, label_txidn}, "text-body2-dialog");

    gridLayoutInput->addWidget(label_txid, pos, 0);
    gridLayoutInput->addWidget(label_txidn, pos, 1);
}

void TxDetailDialog::onInputsClicked()
{
    if (ui->gridInputs->isVisible()) {
        ui->gridInputs->setVisible(false);
    } else {
        bool showGrid = !isShieldedToShieldedRecv;
        if (!inputsLoaded) {
            inputsLoaded = true;
            if (showGrid) {
                CTransactionRef walletTx = (this->tx) ? this->tx->getTransaction() : model->getTx(this->txHash)->tx;
                if (walletTx) {
                    if (walletTx->sapData && walletTx->sapData->vShieldedSpend.empty()) {
                        // transparent inputs
                        ui->gridInputs->setMinimumHeight(50 + (50 * walletTx->vin.size()));
                        int i = 1;
                        for (const CTxIn& in : walletTx->vin) {
                            QString hash = QString::fromStdString(in.prevout.hash.GetHex());
                            loadInputs(hash.left(18) + "..." + hash.right(18),
                                       QString::number(in.prevout.n),
                                       ui->gridLayoutInput, i);
                            i++;
                        }
                    } else {
                        ui->gridInputs->setMinimumHeight(50 + (50 * walletTx->sapData->vShieldedSpend.size()));
                        bool fInfoAvailable = false;
                        for (int i = 0; i < (int) walletTx->sapData->vShieldedSpend.size(); ++i) {
                            Optional<QString> opAddr = model->getShieldedAddressFromSpendDesc(walletTx->GetHash(), i);
                            if (opAddr) {
                                QString addr = *opAddr;
                                loadInputs(addr.left(18) + "..." + addr.right(18),
                                           QString::number(i),
                                           ui->gridLayoutInput, i + 1);
                                fInfoAvailable = true;
                            }
                        }

                        if (!fInfoAvailable) {
                            // note: the spends are not from the wallet, let's not show anything here
                            showGrid = false;
                        }

                    }
                }
            }
        }
        ui->gridInputs->setVisible(showGrid);
    }
}

void appendOutput(QGridLayout* layoutGrid, int gridPosition, QString labelRes, CAmount nValue, int nDisplayUnit)
{
    QLabel *label_address = new QLabel(labelRes);
    QLabel *label_value = new QLabel(BitcoinUnits::formatWithUnit(nDisplayUnit, nValue, false, BitcoinUnits::separatorAlways));
    label_value->setAlignment(Qt::AlignCenter | Qt::AlignRight);
    setCssProperty({label_address, label_value}, "text-body2-dialog");
    layoutGrid->addWidget(label_address, gridPosition, 0);
    layoutGrid->addWidget(label_value, gridPosition, 0);
}

void TxDetailDialog::onOutputsClicked()
{
    if (ui->outputsScrollArea->isVisible()) {
        ui->outputsScrollArea->setVisible(false);
    } else {
        ui->outputsScrollArea->setVisible(true);
        if (!outputsLoaded) {
            outputsLoaded = true;
            QGridLayout* layoutGrid = new QGridLayout(this);
            layoutGrid->setContentsMargins(0,0,12,0);
            ui->container_outputs_base->setLayout(layoutGrid);

            // If the there is a model tx, then this is a confirmation dialog
            if (tx) {
                const QList<SendCoinsRecipient>& recipients = tx->getRecipients();
                unsigned int sffa = tx->subtractFeeFromRecipents();
                CAmount rcp_fee = (sffa > 0) ? (tx->getTransactionFee() / sffa) : 0;
                for (int i = 0; i < recipients.size(); ++i) {
                    const auto& recipient = recipients[i];
                    CAmount rcp_amt = recipient.amount - (recipient.fSubtractFee ? rcp_fee : 0);
                    int charsSize = recipient.isShieldedAddr ? 18 : 16;
                    QString labelRes = recipient.address.left(charsSize) + "..." + recipient.address.right(charsSize);
                    appendOutput(layoutGrid, i, labelRes, rcp_amt, nDisplayUnit);
                }
            } else {
                // Tx detail dialog
                const CWalletTx* walletTx = model->getTx(this->txHash);
                if (!walletTx) return;

                // transparent recipients
                int i = 0;
                for (const CTxOut& out : walletTx->tx->vout) {
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
                    appendOutput(layoutGrid, i, labelRes, out.nValue, nDisplayUnit);
                    i++;
                }

                // shielded recipients
                if (walletTx->tx->sapData) {
                    for (int j = 0; j < (int) walletTx->tx->sapData->vShieldedOutput.size(); ++j) {
                        const SaplingOutPoint op(walletTx->GetHash(), j);
                        // TODO: This only works for txs that are stored, not for when this is a confirmation dialog..
                        if (walletTx->mapSaplingNoteData.find(op) == walletTx->mapSaplingNoteData.end()) {
                            continue;
                        }
                        // Obtain the noteData to get the cached amount value
                        SaplingNoteData noteData = walletTx->mapSaplingNoteData.at(op);
                        const QString& addrStr = model->getSaplingAddressString(walletTx, op);
                        appendOutput(layoutGrid, i, addrStr, *noteData.amount, nDisplayUnit);

                        i++;
                    }
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
