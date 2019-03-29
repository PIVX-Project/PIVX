#include "qt/pivx/settings/settingsbackupwallet.h"
#include "qt/pivx/settings/forms/ui_settingsbackupwallet.h"
#include <QFile>
#include <QGraphicsDropShadowEffect>

SettingsBackupWallet::SettingsBackupWallet(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsBackupWallet)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);


    // Title
    ui->labelTitle->setText("Backup Wallet ");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");


    // Backup Name

    ui->labelSubtitleName->setText("Save as");
    ui->labelSubtitleName->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditName->setPlaceholderText("Set a name for your backup file");
    ui->lineEditName->setProperty("cssClass", "edit-primary");
    ui->lineEditName->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditName->setGraphicsEffect(shadowEffect);

    // Location

    ui->labelSubtitleLocation->setText("Where");
    ui->labelSubtitleLocation->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);

    ui->pushButtonDocuments->setText("Set a folder location");
    ui->pushButtonDocuments->setProperty("cssClass", "btn-edit-primary-folder");
    ui->pushButtonDocuments->setGraphicsEffect(shadowEffect2);

    // Buttons

    ui->pushButtonSave->setText("Backup");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");


}

SettingsBackupWallet::~SettingsBackupWallet()
{
    delete ui;
}
