#include "qt/pivx/addresslabelrow.h"
#include "qt/pivx/forms/ui_addresslabelrow.h"
#include "QFile"

AddressLabelRow::AddressLabelRow(bool isLightTheme, bool isHover , QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AddressLabelRow)
{
    ui->setupUi(this);

    ui->lblAddress->setProperty("cssClass", "text-list-body1");
    ui->lblLabel->setProperty("cssClass", "text-list-title1");

    if(isLightTheme)
        ui->lblDivisory->setStyleSheet("background-color:#bababa");
    else
        ui->lblDivisory->setStyleSheet("background-color:#40ffffff");

    ui->btnMenu->setVisible(isHover);
}

void AddressLabelRow::enterEvent(QEvent *) {
    ui->btnMenu->setVisible(true);
    update();
}

void AddressLabelRow::leaveEvent(QEvent *) {
    ui->btnMenu->setVisible(false);
    update();
}

AddressLabelRow::~AddressLabelRow()
{
    delete ui;
}
