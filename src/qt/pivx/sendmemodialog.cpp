// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/sendmemodialog.h"
#include "qt/pivx/forms/ui_sendmemodialog.h"
#include "qt/pivx/snackbar.h"
#include "qt/pivx/qtutils.h"

SendMemoDialog::SendMemoDialog(QWidget* parent, WalletModel* model) :
    FocusedDialog(parent),
    walletModel(model),
    ui(new Ui::SendMemoDialog)
{
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

    setCssProperty(ui->textEdit, "edit-text-dialog");

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    setCssBtnPrimary(ui->btnSave);

    connect(ui->textEdit, &QTextEdit::textChanged, this, &SendMemoDialog::textChanged);
    connect(ui->btnEsc, &QPushButton::clicked, [this]() {
        operationResult = false;
        close();
    });
    connect(ui->btnCancel, &QPushButton::clicked, this, &SendMemoDialog::reset);
    connect(ui->btnSave, &QPushButton::clicked, this, &SendMemoDialog::accept);
}

void SendMemoDialog::setMemo(QString text)
{
    ui->textEdit->setText(text);
    ui->btnCancel->setText(tr("RESET"));
}

QString SendMemoDialog::getMemo()
{
    return ui->textEdit->toPlainText();
}

void SendMemoDialog::textChanged()
{
    if (ui->textEdit->toPlainText().length() > 512) {
        ui->textEdit->textCursor().deletePreviousChar();
    }
}

void SendMemoDialog::showEvent(QShowEvent *event)
{
    if (ui->textEdit) ui->textEdit->setFocus();
}

void SendMemoDialog::reset()
{
    if (!ui->textEdit->toPlainText().isEmpty()) {
        ui->textEdit->clear();
        ui->btnCancel->setText(tr("CANCEL"));
    }
    // caller reset memo on the recipient
    operationResult = true;
    close();
}

void SendMemoDialog::accept()
{
    operationResult = true;
    if (ui->textEdit->toPlainText().isEmpty()) {
        QDialog::close();
    } else {
        QDialog::accept();
    }
}

void SendMemoDialog::inform(const QString& text)
{
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

SendMemoDialog::~SendMemoDialog()
{
    delete ui;
}
