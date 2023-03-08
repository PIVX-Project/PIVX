// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/defaultdialog.h"
#include "qt/pivx/forms/ui_defaultdialog.h"
#include "guiutil.h"

DefaultDialog::DefaultDialog(QWidget *parent) :
    FocusedDialog(parent),
    ui(new Ui::DefaultDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent ? parent->styleSheet() : GUIUtil::loadStyleSheet());

    // Container
    ui->frame->setProperty("cssClass", "container-dialog");

    // Text
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, &QPushButton::clicked, this, &DefaultDialog::close);
    connect(ui->btnCancel, &QPushButton::clicked, this, &DefaultDialog::close);
    connect(ui->btnSave, &QPushButton::clicked, this, &DefaultDialog::accept);
}

void DefaultDialog::setText(const QString& title, const QString& message, const QString& okBtnText, const QString& cancelBtnText)
{
    if (!okBtnText.isNull()) ui->btnSave->setText(okBtnText);
    if (!cancelBtnText.isNull()) {
        ui->btnCancel->setVisible(true);
        ui->btnCancel->setText(cancelBtnText);
    } else {
        ui->btnCancel->setVisible(false);
    }
    if (!message.isNull()) ui->labelMessage->setText(message);
    if (!title.isNull()) ui->labelTitle->setText(title);
}

void DefaultDialog::accept()
{
    this->isOk = true;
    QDialog::accept();
}

DefaultDialog::~DefaultDialog()
{
    delete ui;
}
