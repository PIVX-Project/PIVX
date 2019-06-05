#include "qt/pivx/mnrow.h"
#include "qt/pivx/forms/ui_mnrow.h"

MNRow::MNRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MNRow)
{
    ui->setupUi(this);
    ui->labelAddress->setProperty("cssClass", "text-list-title1");
    ui->labelName->setProperty("cssClass", "text-list-body2");
    ui->labelDate->setProperty("cssClass", "text-list-caption");

}

void MNRow::updateView(QString address, QString label){
    ui->labelName->setText(label);
    ui->labelAddress->setText(address);
}

MNRow::~MNRow()
{
    delete ui;
}
