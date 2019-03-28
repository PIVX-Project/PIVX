#include "qt/pivx/myaddressrow.h"
#include "qt/pivx/forms/ui_myaddressrow.h"
#include "QFile"

MyAddressRow::MyAddressRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MyAddressRow)
{
    ui->setupUi(this);

    ui->labelName->setProperty("cssClass", "text-list-title1");
    ui->labelAddress->setProperty("cssClass", "text-list-body1");
    ui->labelDate->setProperty("cssClass", "text-list-caption");

}

MyAddressRow::~MyAddressRow()
{
    delete ui;
}
