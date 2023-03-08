// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/receivedialog.h"
#include "qt/pivx/forms/ui_receivedialog.h"
#include "qt/pivx/qtutils.h"
#include "qt/walletmodel.h"

#include <QPixmap>

ReceiveDialog::ReceiveDialog(QWidget *parent) :
    FocusedDialog(parent),
    ui(new Ui::ReceiveDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    ui->frameContainer->setProperty("cssClass", "container-dialog");
    ui->frameContainer->setContentsMargins(10,10,10,10);

    // Title
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // Address
    ui->labelAddress->setProperty("cssClass", "label-address-box");

    // QR image
    QPixmap pixmap(":/res/img/img-qr-test-big.png");
    ui->labelQrImg->setPixmap(pixmap.scaled(
                                  ui->labelQrImg->width(),
                                  ui->labelQrImg->height(),
                                  Qt::KeepAspectRatio)
                              );

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");
    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setProperty("cssClass", "btn-primary");
    ui->btnCancel->setVisible(false);

    connect(ui->btnEsc, &QPushButton::clicked, this, &ReceiveDialog::close);
    connect(ui->btnSave, &QPushButton::clicked, this, &ReceiveDialog::onCopy);
}

void ReceiveDialog::updateQr(const QString& address)
{
    if (!info) info = new SendCoinsRecipient();
    info->address = address;
    QString uri = GUIUtil::formatBitcoinURI(*info);
    ui->labelQrImg->setText("");
    ui->labelAddress->setText(address);
    QString error;
    QPixmap pixmap = encodeToQr(uri, error);
    if (!pixmap.isNull()) {
        ui->labelQrImg->setPixmap(pixmap.scaled(ui->labelQrImg->width(), ui->labelQrImg->height()));
    } else {
        ui->labelQrImg->setText(!error.isEmpty() ? error : "Error encoding address");
    }
}

void ReceiveDialog::onCopy()
{
    GUIUtil::setClipboard(info->address);
    accept();
}

ReceiveDialog::~ReceiveDialog()
{
    delete ui;
}
