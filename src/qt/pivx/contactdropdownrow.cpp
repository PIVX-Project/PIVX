#include "qt/pivx/contactdropdownrow.h"
#include "qt/pivx/forms/ui_contactdropdownrow.h"

ContactDropdownRow::ContactDropdownRow(bool isLightTheme, bool isHover, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ContactDropdownRow)
{
    ui->setupUi(this);
    ui->lblAddress->setProperty("cssClass", "text-list-contact-body1");
    ui->lblLabel->setProperty("cssClass", "text-list-contact-title1");
    update(isLightTheme, isHover, false);
}

void ContactDropdownRow::update(bool isLightTheme, bool isHover, bool isSelected){
    if(isLightTheme)
        ui->lblDivisory->setStyleSheet("background-color:#bababa");
    else
        ui->lblDivisory->setStyleSheet("background-color:#bababa");
}

void ContactDropdownRow::enterEvent(QEvent *) {
    //update();
}

void ContactDropdownRow::leaveEvent(QEvent *) {
    //update();
}


ContactDropdownRow::~ContactDropdownRow()
{
    delete ui;
}
