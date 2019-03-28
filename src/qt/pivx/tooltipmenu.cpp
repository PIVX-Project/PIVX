#include "qt/pivx/tooltipmenu.h"
#include "qt/pivx/forms/ui_tooltipmenu.h"

#include <QTimer>

TooltipMenu::TooltipMenu(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TooltipMenu)
{
    ui->setupUi(this);

    ui->container->setProperty("cssClass", "container-list-menu");
    ui->btnCopy->setProperty("cssClass", "btn-list-menu");
    ui->btnDelete->setProperty("cssClass", "btn-list-menu");
    ui->btnEdit->setProperty("cssClass", "btn-list-menu");

}

void TooltipMenu::showEvent(QShowEvent *event){
    QTimer::singleShot(5000, this, SLOT(hide()));
}

TooltipMenu::~TooltipMenu()
{
    delete ui;
}
