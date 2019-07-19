// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/addnewcontactdialog.h"
#include "qt/pivx/forms/ui_addnewcontactdialog.h"
#include "qt/pivx/qtutils.h"

AddNewContactDialog::AddNewContactDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddNewContactDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    ui->frame->setProperty("cssClass", "container-dialog");
    // Title
    ui->labelTitle->setText(tr("Edit Contact"));
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");

    ui->labelMessage->setText(tr("Set a label for the selected address"));
    ui->labelMessage->setProperty("cssClass", "text-main-grey");

    // Address
    ui->lineEditName->setPlaceholderText(tr("Enter a name for the address (e.g Exchange)"));
    initCssEditLine(ui->lineEditName, true);

    // Buttons
    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnOk->setText(tr("SAVE"));
    ui->btnOk->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnOk, SIGNAL(clicked()), this, SLOT(ok()));
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

void AddNewContactDialog::ok() {
    this->res = true;
    accept();
}

QString AddNewContactDialog::getLabel(){
    return ui->lineEditName->text();
}

AddNewContactDialog::~AddNewContactDialog()
{
    delete ui;
}
