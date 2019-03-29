#include "qt/pivx/settings/settingspeerslistwidget.h"
#include "qt/pivx/settings/forms/ui_settingspeerslistwidget.h"

SettingsPeersListWidget::SettingsPeersListWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsPeersListWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Peers List");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle->setText("Select a peer for additional details");
    ui->labelSubtitle->setProperty("cssClass", "text-main-grey");

    // list

    ui->listView->setVisible(false);

    //ui->emptyContainer->setVisible(false);
    ui->pushImgEmpty->setProperty("cssClass", "img-empty-peers");

    ui->labelEmpty->setText("No peers yet");
    ui->labelEmpty->setProperty("cssClass", "text-empty");

}

SettingsPeersListWidget::~SettingsPeersListWidget()
{
    delete ui;
}
