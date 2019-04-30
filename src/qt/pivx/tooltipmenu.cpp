#include "qt/pivx/tooltipmenu.h"
#include "qt/pivx/forms/ui_tooltipmenu.h"

#include "ui_interface.h"
#include "qt/pivx/PIVXGUI.h"
#include "qt/pivx/qtutils.h"
#include <QModelIndex>

#include <QTimer>
#include <iostream>

TooltipMenu::TooltipMenu(PIVXGUI *_window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::TooltipMenu)
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
    hide();
    emit onDeleteClicked();
}

void TooltipMenu::copyClicked(){
    hide();
    emit onCopyClicked();
}

void TooltipMenu::editClicked(){
    hide();
    emit onEditClicked();
}

void TooltipMenu::showEvent(QShowEvent *event){
    QTimer::singleShot(5000, this, SLOT(hide()));
}

TooltipMenu::~TooltipMenu()
{
    delete ui;
}
