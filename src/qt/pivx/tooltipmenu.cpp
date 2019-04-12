#include "qt/pivx/tooltipmenu.h"
#include "qt/pivx/forms/ui_tooltipmenu.h"

#include "ui_interface.h"
#include "qt/pivx/PIVXGUI.h"

#include <QTimer>
#include <iostream>

TooltipMenu::TooltipMenu(PIVXGUI *_window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TooltipMenu),
    window(_window)
{
    ui->setupUi(this);

    ui->container->setProperty("cssClass", "container-list-menu");
    ui->btnCopy->setProperty("cssClass", "btn-list-menu");
    ui->btnDelete->setProperty("cssClass", "btn-list-menu");
    ui->btnEdit->setProperty("cssClass", "btn-list-menu");

    connect(ui->btnCopy, SIGNAL(clicked()), this, SLOT(copyClicked()));
    connect(ui->btnDelete, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->btnEdit, SIGNAL(clicked()), this, SLOT(editClicked()));
}

void TooltipMenu::deleteClicked(){
    std::cout << "het" << std::endl;
    bool ret = false;
    //emit message(tr("Delete Contact Label"), tr("Are you sure?"), CClientUIInterface::MSG_INFORMATION & CClientUIInterface::BTN_MASK, &ret);
    window->message(tr("Delete Contact Label"), tr("Are you sure?"), CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL, &ret);
    if(ret){
        std::cout << "holas" << std::endl;
    }
}

void TooltipMenu::copyClicked(){

}

void TooltipMenu::editClicked(){

}

void TooltipMenu::showEvent(QShowEvent *event){
    QTimer::singleShot(5000, this, SLOT(hide()));
}

TooltipMenu::~TooltipMenu()
{
    delete ui;
}
