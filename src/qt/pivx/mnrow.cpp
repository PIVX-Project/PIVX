// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/mnrow.h"
#include "qt/pivx/forms/ui_mnrow.h"
#include "qt/pivx/qtutils.h"

MNRow::MNRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MNRow)
{
    ui->setupUi(this);
    setCssProperty(ui->labelAddress, "text-list-body2");
    setCssProperty(ui->labelName, "text-list-title1");
    setCssProperty(ui->labelDate, "text-list-caption-medium");
    ui->lblDivisory->setStyleSheet("background-color:#bababa;");
}

void MNRow::updateView(QString address, QString label, QString status, bool wasCollateralAccepted){
    ui->labelName->setText(label);
    ui->labelAddress->setText(address);
    ui->labelDate->setText("Status: " + status);
    if (!wasCollateralAccepted){
        ui->labelDate->setText("Status: Collateral tx not found");
    } else {
        ui->labelDate->setText("Status: " + status);
    }
}

MNRow::~MNRow(){
    delete ui;
}
