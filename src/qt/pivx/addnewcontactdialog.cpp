// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/addnewcontactdialog.h"
#include "qt/pivx/forms/ui_addnewcontactdialog.h"
#include "qt/pivx/qtutils.h"

AddNewContactDialog::AddNewContactDialog(QWidget *parent) :
    FocusedDialog(parent),
    ui(new Ui::AddNewContactDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());
    ui->frame->setProperty("cssClass", "container-dialog");

    // Title
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    // Description
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Address
    initCssEditLine(ui->lineEditName, true);

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");
    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnOk->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, &QPushButton::clicked, this, &AddNewContactDialog::close);
    connect(ui->btnCancel, &QPushButton::clicked, this, &AddNewContactDialog::close);
    connect(ui->btnOk, &QPushButton::clicked, this, &AddNewContactDialog::accept);
}

void AddNewContactDialog::setTexts(QString title, const char* message) {
    ui->labelTitle->setText(title);
    this->message = message;
}

void AddNewContactDialog::setData(QString address, QString label){
    ui->labelMessage->setText(
            (
                    !message ?
            tr("Edit label for the selected address:\n%1").arg(address.toUtf8().constData()) :
            tr(message).arg(address.toUtf8().constData())
            )
    );
    if (!label.isEmpty()) ui->lineEditName->setText(label);
}

void AddNewContactDialog::showEvent(QShowEvent *event)
{
    if (ui->lineEditName) ui->lineEditName->setFocus();
}

void AddNewContactDialog::accept() {
    this->res = true;
    QDialog::accept();
}

QString AddNewContactDialog::getLabel(){
    return ui->lineEditName->text();
}

AddNewContactDialog::~AddNewContactDialog()
{
    delete ui;
}
