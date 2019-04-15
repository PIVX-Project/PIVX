#include "qt/pivx/settings/settingsbackupwallet.h"
#include "qt/pivx/settings/forms/ui_settingsbackupwallet.h"
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include "guiutil.h"
#include "qt/pivx/qtutils.h"
#include "ui_interface.h"
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
    setCssEditLine(ui->lineEditName,true,false);
    ui->lineEditName->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditName->setGraphicsEffect(shadowEffect);

    // Location

    ui->labelSubtitleLocation->setText("Where");
    ui->labelSubtitleLocation->setProperty("cssClass", "text-title");


    ui->pushButtonDocuments->setText(tr("Set a folder location"));
    ui->pushButtonDocuments->setProperty("cssClass", "btn-edit-primary-folder");
    ui->pushButtonDocuments->setGraphicsEffect(shadowEffect);

    // Buttons

    ui->pushButtonSave->setText("Backup");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->container_file_name->setVisible(false);
    //ui->verticalSpacer_3->setVisible(false);

    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(backupWallet()));
    connect(ui->pushButtonDocuments, SIGNAL(clicked()), this, SLOT(selectFileOutput()));

}

void SettingsBackupWallet::selectFileOutput(){
    QString filenameRet = GUIUtil::getSaveFileName(this,
                                        tr("Backup Wallet"), QString(),
                                        tr("Wallet Data (*.dat)"), NULL);

    if (!filenameRet.isEmpty()) {
        filename = filenameRet;
        ui->pushButtonDocuments->setText(filename);
    }
}

void SettingsBackupWallet::backupWallet(){
    if(walletModel && !filename.isEmpty()) {
        inform(walletModel->backupWallet(filename) ? tr("Backup created") : tr("Backup creation failed"));
        filename = QString();
        ui->pushButtonDocuments->setText(tr("Set a folder location"));
    }
}

SettingsBackupWallet::~SettingsBackupWallet(){
    delete ui;
}
