#include "qt/pivx/sendconfirmdialog.h"
#include "qt/pivx/forms/ui_sendconfirmdialog.h"
#include "bitcoinunits.h"
#include "walletmodel.h"
#include "transactiontablemodel.h"
#include "transactionrecord.h"
#include "wallet/wallet.h"
#include "guiutil.h"
#include "qt/pivx/snackbar.h"
#include "qt/pivx/qtutils.h"
#include <QList>
#include <QDateTime>

TxDetailDialog::TxDetailDialog(QWidget *parent, bool isConfirmDialog) :
    QDialog(parent),
    ui(new Ui::TxDetailDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Container
    ui->frame->setProperty("cssClass", "container-dialog");

    // Title
    ui->labelTitle->setText(tr("Confirm your transaction"));
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // Labels
    setCssTextBodyDialog({ui->labelAmount, ui->labelSend, ui->labelInputs, ui->labelFee, ui->labelChange, ui->labelId, ui->labelSize, ui->labelStatus, ui->labelConfirmations, ui->labelDate});
    setCssProperty({ui->labelDivider1, ui->labelDivider2, ui->labelDivider3, ui->labelDivider4, ui->labelDivider5, ui->labelDivider6, ui->labelDivider7, ui->labelDivider8, ui->labelDivider9}, "container-divider");
    setCssTextBodyDialog({ui->textAmount, ui->textSend, ui->textInputs, ui->textFee, ui->textChange, ui->textId, ui->textSize, ui->textStatus, ui->textConfirmations, ui->textDate});

    ui->pushCopy->setProperty("cssClass", "ic-copy-big");
    ui->pushInputs->setProperty("cssClass", "ic-arrow-down");
    ui->pushOutputs->setProperty("cssClass", "ic-arrow-down");

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->gridInputs->setVisible(false);
    ui->outputsScrollArea->setVisible(false);
    ui->contentChangeAddress->setVisible(false);
    ui->labelDivider4->setVisible(false);

    ui->labelOutputIndex->setProperty("cssClass", "text-body2-dialog");
    ui->labelTitlePrevTx->setProperty("cssClass", "text-body2-dialog");

    if(isConfirmDialog){

        ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
        ui->btnSave->setText(tr("SEND"));
        setCssBtnPrimary(ui->btnSave);

        // hide change address for now
        ui->contentConfirmations->setVisible(false);
        ui->contentStatus->setVisible(false);
        ui->contentDate->setVisible(false);
        ui->contentSize->setVisible(false);
        ui->contentConfirmations->setVisible(false);
        ui->contentID->setVisible(false);
        ui->labelDivider7->setVisible(false);
        ui->labelDivider5->setVisible(false);
        ui->labelDivider3->setVisible(false);

        connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
        connect(ui->btnSave, &QPushButton::clicked, [this](){acceptTx();});
    }else{
        ui->containerButtons->setVisible(false);
    }

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->pushInputs, SIGNAL(clicked()), this, SLOT(onInputsClicked()));
    connect(ui->pushOutputs, SIGNAL(clicked()), this, SLOT(onOutputsClicked()));
}

void TxDetailDialog::setData(WalletModel *model, QModelIndex &index){
    this->model = model;
    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());
    QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
    QString address = index.data(Qt::DisplayRole).toString();
    qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
    QString amountText = BitcoinUnits::formatWithUnit(nDisplayUnit, amount, true, BitcoinUnits::separatorAlways);
    ui->textAmount->setText(amountText);

    const CWalletTx* tx = model->getTx(rec->hash);
    if(tx) {
        this->txHash = rec->hash;
        QString hash = QString::fromStdString(tx->GetHash().GetHex());
        ui->textId->setText(hash.left(20) + "..." + hash.right(20));
        ui->textId->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (tx->vout.size() == 1) {
            ui->textSend->setText(address);
        } else {
            ui->textSend->setText(QString::number(tx->vout.size()) + " recipients");
        }
        ui->textInputs->setText(QString::number(tx->vin.size()));
        ui->textConfirmations->setText(QString::number(tx->GetDepthInMainChain()));
        ui->textDate->setText(GUIUtil::dateTimeStr(date));
        ui->textStatus->setText(QString::fromStdString(rec->statusToString()));
        ui->textSize->setText(QString::number(rec->size) + " bytes");

        connect(ui->pushCopy, &QPushButton::clicked, [this](){
            GUIUtil::setClipboard(QString::fromStdString(this->txHash.GetHex()));
            SnackBar *snackBar = new SnackBar(nullptr, this);
            snackBar->setText(tr("ID copied"));
            snackBar->resize(this->width(), snackBar->height());
            openDialog(snackBar, this);
            snackBar->deleteLater();
        });
    }

}

void TxDetailDialog::setData(WalletModel *model, WalletModelTransaction &tx){
    this->model = model;
    this->tx = &tx;
    CAmount txFee = tx.getTransactionFee();
    CAmount totalAmount = tx.getTotalTransactionAmount() + txFee;

    ui->textAmount->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, totalAmount, false, BitcoinUnits::separatorAlways) + " (Fee included)");
    if(tx.getRecipients().size() == 1){
        ui->textSend->setText(tx.getRecipients().at(0).address);
        ui->pushOutputs->setVisible(false);
    }else{
        ui->textSend->setText(QString::number(tx.getRecipients().size()) + " recipients");
    }
    ui->textInputs->setText(QString::number(tx.getTransaction()->vin.size()));
    ui->textFee->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, txFee, false, BitcoinUnits::separatorAlways));
}

void TxDetailDialog::acceptTx(){
    this->confirm = true;
    this->sendStatus = model->sendCoins(*this->tx);
    accept();
}

void TxDetailDialog::onInputsClicked() {
    if (ui->gridInputs->isVisible()) {
        ui->gridInputs->setVisible(false);
        ui->contentInputs->layout()->setContentsMargins(0,9,12,9);
    } else {
        ui->gridInputs->setVisible(true);
        ui->contentInputs->layout()->setContentsMargins(0,9,12,0);
        if (!inputsLoaded) {
            inputsLoaded = true;
            const CWalletTx* tx = (this->tx) ? this->tx->getTransaction() : model->getTx(this->txHash);
            if(tx) {
                ui->gridInputs->setMinimumHeight(50 + (50 * tx->vin.size()));
                int i = 1;
                for (const CTxIn &in : tx->vin) {
                    QString hash = QString::fromStdString(in.prevout.hash.GetHex());
                    QLabel *label = new QLabel(hash.left(18) + "..." + hash.right(18));
                    label->setProperty("cssClass", "text-body2-dialog");
                    QLabel *label1 = new QLabel(QString::number(in.prevout.n));
                    label1->setProperty("cssClass", "text-body2-dialog");
                    label1->setAlignment(Qt::AlignCenter);

                    ui->gridLayoutInput->addWidget(label,i,0);
                    ui->gridLayoutInput->addWidget(label1,i,1, Qt::AlignCenter);
                    i++;
                }
            }
        }
    }
}

void TxDetailDialog::onOutputsClicked() {
    if (ui->outputsScrollArea->isVisible()) {
        ui->outputsScrollArea->setVisible(false);
    } else {
        ui->outputsScrollArea->setVisible(true);
        if (!outputsLoaded) {
            outputsLoaded = true;
            QVBoxLayout* layoutVertical = new QVBoxLayout();
            layoutVertical->setContentsMargins(0,0,12,0);
            layoutVertical->setSpacing(6);
            ui->container_outputs_base->setLayout(layoutVertical);

            const CWalletTx* tx = model->getTx(this->txHash);
            if(tx) {
                for (const CTxOut &out : tx->vout) {
                    QFrame *frame = new QFrame(ui->container_outputs_base);

                    QHBoxLayout *layout = new QHBoxLayout();
                    layout->setContentsMargins(0, 0, 0, 0);
                    layout->setSpacing(12);
                    frame->setLayout(layout);

                    QLabel *label = nullptr;
                    QString labelRes;
                    CTxDestination dest;
                    if (ExtractDestination(out.scriptPubKey, dest)) {
                        std::string address = CBitcoinAddress(dest).ToString();
                        labelRes = QString::fromStdString(address);
                        labelRes = labelRes.left(16) + "..." + labelRes.right(16);
                    } else {
                        labelRes = tr("Unknown");
                    }
                    label = new QLabel(labelRes);
                    label->setProperty("cssClass", "text-body2-dialog");
                    QLabel *label1 = new QLabel(BitcoinUnits::formatWithUnit(nDisplayUnit, out.nValue, false, BitcoinUnits::separatorAlways));
                    label1->setProperty("cssClass", "text-body2-dialog");
                    label1->setAlignment(Qt::AlignCenter | Qt::AlignRight);

                    layout->addWidget(label);
                    layout->addWidget(label1);
                    layoutVertical->addWidget(frame);
                }
            }
        }
    }
}

TxDetailDialog::~TxDetailDialog()
{
    delete ui;
}
