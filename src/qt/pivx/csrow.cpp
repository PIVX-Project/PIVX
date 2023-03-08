// Copyright (c) 2019 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/csrow.h"
#include "qt/pivx/forms/ui_csrow.h"

CSRow::CSRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSRow)
{
    ui->setupUi(this);
    ui->labelName->setProperty("cssClass", "text-list-title1");
    ui->labelAddress->setProperty("cssClass", "text-list-body2");
    ui->labelStaking->setProperty("cssClass", "text-list-caption-medium");
    ui->labelAmount->setProperty("cssClass", "text-list-amount-unconfirmed");
}

void CSRow::updateView(const QString& address, const QString& label, bool isStaking, bool isReceivedDelegation, const QString& amount) {
    ui->labelName->setText(label);
    ui->labelAddress->setText(address);
    ui->labelAmount->setText(amount);

    if (isReceivedDelegation) {
        ui->labelStaking->setText(tr(isStaking ? "Staking" : "Not staking"));
    } else {
        ui->labelStaking->setText(tr("Own delegation"));
    }
}

void CSRow::updateState(bool isLightTheme, bool isHovered, bool isSelected) {
    ui->lblDivisory->setStyleSheet((isLightTheme) ?  "background-color:#bababa" : "background-color:#40ffffff");
    if (fShowMenuButton) {
        ui->pushButtonMenu->setVisible(isHovered);
    }
}

void CSRow::showMenuButton(bool show) {
    this->fShowMenuButton = show;
}

void CSRow::enterEvent(QEvent *) {
    if (fShowMenuButton) {
        ui->pushButtonMenu->setVisible(true);
        update();
    }
}

void CSRow::leaveEvent(QEvent *) {
    if (fShowMenuButton) {
        ui->pushButtonMenu->setVisible(false);
        update();
    }
}

CSRow::~CSRow(){
    delete ui;
}
