#include "qt/pivx/settings/settingsopenurlwidget.h"
#include "qt/pivx/settings/forms/ui_settingsopenurlwidget.h"
#include "QGraphicsDropShadowEffect"

SettingsOpenUrlWidget::SettingsOpenUrlWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsOpenUrlWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Open Url");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Url

    ui->labelSubtitleUrl->setText("Open payment request from Url or file");
    ui->labelSubtitleUrl->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect3 = new QGraphicsDropShadowEffect();
    shadowEffect3->setColor(QColor(0, 0, 0, 22));
    shadowEffect3->setXOffset(0);
    shadowEffect3->setYOffset(3);
    shadowEffect3->setBlurRadius(6);

    ui->pushButtonUrl->setText("PIVX:");
    ui->pushButtonUrl->setProperty("cssClass", "btn-edit-primary");
    ui->pushButtonUrl->setGraphicsEffect(shadowEffect3);

    // Buttons

    ui->pushButtonSave->setText("OK");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");
}

SettingsOpenUrlWidget::~SettingsOpenUrlWidget()
{
    delete ui;
}
