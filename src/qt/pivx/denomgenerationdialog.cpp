// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/denomgenerationdialog.h"
#include "qt/pivx/forms/ui_denomgenerationdialog.h"
#include "QGraphicsDropShadowEffect"

DenomGenerationDialog::DenomGenerationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DenomGenerationDialog)
{
    ui->setupUi(this);


    // Stylesheet
    this->setStyleSheet(parent->styleSheet());

    // Container

    ui->frame->setProperty("cssClass", "container-dialog");

    // Text

    ui->labelTitle->setText("Denom Generation");
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    ui->labelMessage->setText("This will unlock your wallet fully, so that anyone with access to it can spend until the wallet is closed or locked again.");
    ui->labelMessage->setProperty("cssClass", "text-main-grey");


    /* Check Denom */

    ui->checkBox5000->setText("5000");

    ui->checkBox1000->setText("1000");

    ui->checkBox500->setText("500");

    ui->checkBox100->setText("100");

    ui->checkBox50->setText("50");

    ui->checkBox10->setText("10");

    ui->checkBox5->setText("5");

    ui->checkBox1->setText("1");

    ui->checkBoxAll->setText("Select all");



    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");

    ui->btnCancel->setProperty("cssClass", "btn-dialog-cancel");
    ui->btnSave->setText("SAVE");
    ui->btnSave->setProperty("cssClass", "btn-primary");

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
}

DenomGenerationDialog::~DenomGenerationDialog()
{
    delete ui;
}
