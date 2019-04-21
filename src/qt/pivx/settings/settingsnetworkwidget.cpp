#include "qt/pivx/settings/settingsnetworkwidget.h"
#include "qt/pivx/settings/forms/ui_settingsnetworkwidget.h"
#include "QGraphicsDropShadowEffect"
#include "optionsmodel.h"
#include "qt/pivx/qtutils.h"

SettingsNetworkWidget::SettingsNetworkWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsNetworkWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Network");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Proxy

    ui->labelSubtitleProxy->setText("Proxy IP:");
    ui->labelSubtitleProxy->setProperty("cssClass", "text-main-grey");

    ui->lineEditProxy->setPlaceholderText("Enter proxy IP");
    ui->lineEditProxy->setProperty("cssClass", "edit-primary");
    ui->lineEditProxy->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditProxy);
    // Port

    ui->labelSubtitlePort->setText("Port:");
    ui->labelSubtitlePort->setProperty("cssClass", "text-main-grey");

    ui->lineEditPort->setPlaceholderText("Enter port");
    ui->lineEditPort->setProperty("cssClass", "edit-primary");
    ui->lineEditPort->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->lineEditPort);

    // Radio buttons

    ui->checkBoxMap->setText("Map port using UPnP");
    ui->checkBoxAllow->setText("Allow incoming connections");
    ui->checkBoxConnect->setText("Connect through SOCKS5 proxy (default proxy):");

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");
}

void SettingsNetworkWidget::setMapper(QDataWidgetMapper *mapper){
    mapper->addMapping(ui->checkBoxMap, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->checkBoxAllow, OptionsModel::Listen);

    mapper->addMapping(ui->checkBoxConnect, OptionsModel::ProxyUse);
    mapper->addMapping(ui->lineEditProxy, OptionsModel::ProxyIP);
    mapper->addMapping(ui->lineEditPort, OptionsModel::ProxyPort);
}

SettingsNetworkWidget::~SettingsNetworkWidget()
{
    delete ui;
}
