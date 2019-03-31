#include "qt/pivx/myaddressrow.h"
#include "qt/pivx/forms/ui_myaddressrow.h"
#include "QFile"

MyAddressRow::MyAddressRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MyAddressRow)
{
    ui->setupUi(this);

    ui->labelName->setProperty("cssClass", "text-list-title1");
    ui->labelAddress->setProperty("cssClass", "text-list-body2");
    ui->labelDate->setProperty("cssClass", "text-list-caption");

}

void MyAddressRow::updateView(QString address, QString label){
    ui->labelName->setText(label);
    ui->labelAddress->setText(address);
}

MyAddressRow::~MyAddressRow()
{
    delete ui;
}
