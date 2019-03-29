#include "qt/pivx/settings/settingsnetworkwidget.h"
#include "qt/pivx/settings/forms/ui_settingsnetworkwidget.h"
#include "QGraphicsDropShadowEffect"

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

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditProxy->setPlaceholderText("Enter proxy IP");
    ui->lineEditProxy->setProperty("cssClass", "edit-primary");
    ui->lineEditProxy->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditProxy->setGraphicsEffect(shadowEffect);
    // Port

    ui->labelSubtitlePort->setText("Port:");
    ui->labelSubtitlePort->setProperty("cssClass", "text-main-grey");

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);

    ui->lineEditPort->setPlaceholderText("Enter port");
    ui->lineEditPort->setProperty("cssClass", "edit-primary");
    ui->lineEditPort->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditPort->setGraphicsEffect(shadowEffect2);

    // Radio buttons

    ui->radioButtonMap->setText("Map port using UPnP");
    ui->radioButtonAllow->setText("Allow incoming connections");
    ui->radioButtonConnect->setText("Connect through SOCKS5 proxy (default proxy):");

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");
}

SettingsNetworkWidget::~SettingsNetworkWidget()
{
    delete ui;
}
