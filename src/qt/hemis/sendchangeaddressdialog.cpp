// Copyright (c) 2019-2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/sendchangeaddressdialog.h"
#include "qt/pivx/forms/ui_sendchangeaddressdialog.h"
#include "qt/pivx/qtutils.h"

SendChangeAddressDialog::SendChangeAddressDialog(QWidget* parent, WalletModel* model, bool isTransparent) : FocusedDialog(parent),
                                                                                                            walletModel(model),
                                                                                                            ui(new Ui::SendChangeAddressDialog)
{
    // Change address
    dest = CNoDestination();
    this->isTransparent = isTransparent;
    if (!walletModel) {
        throw std::runtime_error(strprintf("%s: No wallet model set", __func__));
    }
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

    // Container
    ui->frame->setProperty("cssClass", "container-dialog");

    // Text
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    initCssEditLine(ui->lineEditAddress, true);

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    setCssBtnPrimary(ui->btnSave);

    connect(ui->btnEsc, &QPushButton::clicked, this, &SendChangeAddressDialog::close);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SendChangeAddressDialog::reset);
    connect(ui->btnSave, &QPushButton::clicked, this, &SendChangeAddressDialog::accept);
}

void SendChangeAddressDialog::setAddress(QString address)
{
    ui->lineEditAddress->setText(address);
    ui->btnCancel->setText(tr("RESET"));
}

CWDestination SendChangeAddressDialog::getDestination() const
{
    return dest;
}

void SendChangeAddressDialog::showEvent(QShowEvent *event)
{
    if (ui->lineEditAddress) ui->lineEditAddress->setFocus();
}

void SendChangeAddressDialog::reset()
{
    if (!ui->lineEditAddress->text().isEmpty()) {
        ui->lineEditAddress->clear();
        ui->btnCancel->setText(tr("CANCEL"));
    }
    close();
}

void SendChangeAddressDialog::accept()
{
    if (ui->lineEditAddress->text().isEmpty()) {
        // no custom change address set
        dest = CNoDestination();
        QDialog::accept();
    } else {
        // validate address
        bool isStakingAddr = false;
        bool isShield = false;
        dest = Standard::DecodeDestination(ui->lineEditAddress->text().toStdString(), isStakingAddr, isShield);

        if (!Standard::IsValidDestination(dest)) {
            inform(tr("Invalid address"));
        } else if (isStakingAddr) {
            inform(tr("Cannot use cold staking addresses for change"));
        } else if (!isShield && !isTransparent) {
            inform(tr("Cannot use a transparent change for a shield transaction"));
        } else {
            QDialog::accept();
        }
    }
}

void SendChangeAddressDialog::inform(const QString& text)
{
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

SendChangeAddressDialog::~SendChangeAddressDialog()
{
    delete ui;
}
