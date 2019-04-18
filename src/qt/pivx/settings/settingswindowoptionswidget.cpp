#include "qt/pivx/settings/settingswindowoptionswidget.h"
#include "qt/pivx/settings/settingswindowoptionswidget.h"
#include "qt/pivx/settings/forms/ui_settingswindowoptionswidget.h"
#include "optionsmodel.h"

SettingsWindowOptionsWidget::SettingsWindowOptionsWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsWindowOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    // Title

    ui->labelTitle->setText("Window");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");


    // CheckBox

    ui->checkBoxMinTaskbar->setText("Minimize to they tray instead of the taskbar");
    ui->checkBoxMinClose->setText("Minimize on close");

    // Buttons

    ui->pushButtonSave->setText("SAVE");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonReset->setText("Reset to default");
    ui->pushButtonReset->setProperty("cssClass", "btn-secundary");
}

void SettingsWindowOptionsWidget::setMapper(QDataWidgetMapper *mapper){
    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->checkBoxMinTaskbar, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->checkBoxMinClose, OptionsModel::MinimizeOnClose);
#endif
}

SettingsWindowOptionsWidget::~SettingsWindowOptionsWidget()
{
    delete ui;
}
