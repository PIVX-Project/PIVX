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
    setCssProperty(ui->labelDate, "text-list-caption");
}

void MNRow::updateView(QString address, QString label)
{
    ui->labelName->setText(label);
    ui->labelAddress->setText(address);
}

MNRow::~MNRow()
{
    delete ui;
}
