#include "qt/pivx/settings/settingsbackupwallet.h"
#include "qt/pivx/settings/forms/ui_settingsbackupwallet.h"
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include "guiutil.h"
#include "qt/pivx/qtutils.h"
#include "ui_interface.h"
#include "qt/pivx/qtutils.h"
SettingsBackupWallet::SettingsBackupWallet(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsBackupWallet)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,0,10,10);


    // Title
    ui->labelTitle->setText("Backup Wallet ");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    ui->labelTitle_2->setText("Change Wallet Passphrase");
    ui->labelTitle_2->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelSubtitle1->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelSubtitle_2->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
    ui->labelSubtitle_2->setProperty("cssClass", "text-subtitle");


    // Backup Name

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    // Location

    ui->labelSubtitleLocation->setText("Where");
    ui->labelSubtitleLocation->setProperty("cssClass", "text-title");


    ui->pushButtonDocuments->setText(tr("Set a folder location"));
    ui->pushButtonDocuments->setProperty("cssClass", "btn-edit-primary-folder");
    ui->pushButtonDocuments->setGraphicsEffect(shadowEffect);

    // Buttons

    ui->pushButtonSave->setText("Backup");
    ui->pushButtonSave->setProperty("cssClass", "btn-primary");

    ui->pushButtonSave_2->setText("Change Passphrase");
    ui->pushButtonSave_2->setProperty("cssClass", "btn-primary");

    connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(backupWallet()));
    connect(ui->pushButtonDocuments, SIGNAL(clicked()), this, SLOT(selectFileOutput()));
    connect(ui->pushButtonSave_2, SIGNAL(clicked()), this, SLOT(changePassphrase()));
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

void SettingsBackupWallet::changePassphrase(){
    emit showHide(true);
    AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::ChangePass, window,
            walletModel, AskPassphraseDialog::Context::ChangePass);
    dlg->adjustSize();
    emit execDialog(dlg);
}

SettingsBackupWallet::~SettingsBackupWallet(){
    delete ui;
}
