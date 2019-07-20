// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/requestdialog.h"
#include "qt/pivx/forms/ui_requestdialog.h"
#include <QListView>
#include <QDoubleValidator>

#include "qt/pivx/qtutils.h"
#include "guiutil.h"
#include "amount.h"
#include "optionsmodel.h"

RequestDialog::RequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RequestDialog)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");

    // Text
    ui->labelTitle->setText(tr("New Request Payment"));
    setCssProperty(ui->labelTitle, "text-title-dialog");

    ui->labelMessage->setText(tr("Instead of share a PIVX address, you can create a Payment Request message which bundles up more information than is contained in just a PIVX address."));
    setCssProperty(ui->labelMessage, "text-main-grey");

    // Combo Coins
    setCssProperty(ui->comboBoxCoin, "btn-combo-coins");
    setCssProperty(ui->comboContainer, "container-purple");

    ui->comboBoxCoin->addItem("PIV", 0);
    ui->comboBoxCoin->addItem("zPIV", 1);
    ui->comboBoxCoin->setView(new QListView());

    // Label
    ui->labelSubtitleLabel->setText(tr("Label"));
    setCssProperty(ui->labelSubtitleLabel, "text-title2-dialog");
    ui->lineEditLabel->setPlaceholderText(tr("Enter a label to be saved within the address"));
    setCssEditLineDialog(ui->lineEditLabel, true);

    // Amount
    ui->labelSubtitleAmount->setText(tr("Amount"));
    setCssProperty(ui->labelSubtitleAmount, "text-title2-dialog");
    ui->lineEditAmount->setPlaceholderText("0.00 PIV");
    setCssEditLineDialog(ui->lineEditAmount, true);

    QDoubleValidator *doubleValidator = new QDoubleValidator(0, 9999999, 7, this);
    doubleValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineEditAmount->setValidator(doubleValidator);

    // Description
    ui->labelSubtitleDescription->setText(tr("Description (optional)"));
    setCssProperty(ui->labelSubtitleDescription, "text-title2-dialog");

    ui->lineEditDescription->setPlaceholderText(tr("Add description "));
    setCssEditLineDialog(ui->lineEditDescription, true);

    // Stack
    ui->stack->setCurrentIndex(pos);
    // Request QR Page
    // Address
    ui->labelAddress->setText(tr("Error"));
    setCssProperty(ui->labelAddress, "text-main-grey-big");

    // Buttons
    setCssProperty(ui->btnEsc, "ic-close");
    setCssProperty(ui->btnCancel, "btn-dialog-cancel");
    ui->btnSave->setText(tr("GENERATE"));
    setCssBtnPrimary(ui->btnSave);
    setCssBtnPrimary(ui->btnCopyAddress);
    setCssBtnPrimary(ui->btnCopyUrl);

    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnSave, SIGNAL(clicked()), this, SLOT(onNextClicked()));
    // TODO: Change copy address for save image (the method is already implemented in other class called exportQr or something like that)
    connect(ui->btnCopyAddress, SIGNAL(clicked()), this, SLOT(onCopyClicked()));
    connect(ui->btnCopyUrl, SIGNAL(clicked()), this, SLOT(onCopyUriClicked()));
}

void RequestDialog::setWalletModel(WalletModel *model){
    this->walletModel = model;
}


void RequestDialog::onNextClicked(){
    if(walletModel) {
        info = new SendCoinsRecipient();
        info->label = ui->lineEditLabel->text();
        info->message = ui->lineEditDescription->text();
        info->address = QString::fromStdString(walletModel->getNewAddress((info->label.isEmpty() ? "" : info->label.toStdString())).ToString());
        int displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        bool isValueValid = true;
        CAmount value = GUIUtil::parseValue(
                ui->lineEditAmount->text(),
                displayUnit,
                &isValueValid
        );
        info->amount = value;

        if(value <= 0 || !isValueValid){
            return;
        }
        ui->labelTitle->setText("Request for " + BitcoinUnits::format(displayUnit, value, false, BitcoinUnits::separatorAlways) + " PIV");
        updateQr(info->address);
        ui->labelAddress->setText(info->address);
        ui->buttonsStack->setVisible(false);
        pos = 1;
        ui->stack->setCurrentIndex(pos);
    }
}

void RequestDialog::onCopyClicked(){
    if(info) {
        GUIUtil::setClipboard(info->address);
        res = 2;
        accept();
    }
}

void RequestDialog::onCopyUriClicked(){
    if(info) {
        GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(*info));
        res = 1;
        accept();
    }
}

void RequestDialog::updateQr(QString str){
    QString uri = GUIUtil::formatBitcoinURI(*info);
    ui->labelQrImg->setText("");
    QString error;
    QPixmap pixmap = encodeToQr(uri, error);
    if(!pixmap.isNull()){
        qrImage = &pixmap;
        ui->labelQrImg->setPixmap(qrImage->scaled(ui->labelQrImg->width(), ui->labelQrImg->height()));
    }else{
        ui->labelQrImg->setText(!error.isEmpty() ? error : "Error encoding address");
    }
}

RequestDialog::~RequestDialog(){
    delete ui;
}
