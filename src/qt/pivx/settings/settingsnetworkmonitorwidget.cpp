#include "qt/pivx/settings/settingsnetworkmonitorwidget.h"
#include "qt/pivx/settings/forms/ui_settingsnetworkmonitorwidget.h"

SettingsNetworkMonitorWidget::SettingsNetworkMonitorWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window,parent),
    ui(new Ui::SettingsNetworkMonitorWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Network Monitor");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");
}

SettingsNetworkMonitorWidget::~SettingsNetworkMonitorWidget()
{
    delete ui;
}
