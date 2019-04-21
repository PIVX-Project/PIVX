#include "qt/pivx/settings/settingsbittoolwidget.h"
#include "qt/pivx/settings/forms/ui_settingsbittoolwidget.h"
#include "qt/pivx/qtutils.h"

SettingsBitToolWidget::SettingsBitToolWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsBitToolWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);


    /* Title */
    ui->labelTitle->setText(tr("BIP38 Tool"));
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    //Button Group

    ui->pushLeft->setText(tr("Encrypt"));
    ui->pushLeft->setProperty("cssClass", "btn-check-left");
    ui->pushRight->setText(tr("Decrypt"));
    ui->pushRight->setProperty("cssClass", "btn-check-right");

    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    // Key

    ui->labelSubtitleKey->setText(tr("Encrypted key"));
    ui->labelSubtitleKey->setProperty("cssClass", "text-title");

    ui->lineEditKey->setPlaceholderText(tr("Enter a encrypted key"));
    initCssEditLine(ui->lineEditKey);

    // Passphrase

    ui->labelSubtitlePassphrase->setText(tr("Passphrase"));
    ui->labelSubtitlePassphrase->setProperty("cssClass", "text-title");

    ui->lineEditPassphrase->setPlaceholderText(tr("Enter a passphrase "));
    initCssEditLine(ui->lineEditPassphrase);

    // Buttons

    ui->pushButtonSave->setText(tr("DECRYPT KEY"));
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonImport->setText("Import Address");
    ui->pushButtonImport->setProperty("cssClass", "btn-text-primary");
}

SettingsBitToolWidget::~SettingsBitToolWidget()
{
    delete ui;
}
