#include "qt/pivx/pwidget.h"
#include "qt/pivx/qtutils.h"

#include "qt/pivx/moc_pwidget.cpp"

PWidget::PWidget(PIVXGUI* _window, QWidget *parent) : QWidget(parent), window(_window)
{
    if(window)
        connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(changeTheme(bool, QString&)));
}

void PWidget::setClientModel(ClientModel* model){
    this->clientModel = model;
    loadClientModel();
}

void PWidget::changeTheme(bool isLightTheme, QString& theme){
    // Change theme in all of the childs here..
    this->setStyleSheet(theme);
    updateStyle(this);
}

void PWidget::loadClientModel(){
    // override
}