#include "qt/pivx/settings/settingsinformationwidget.h"
#include "qt/pivx/settings/forms/ui_settingsinformationwidget.h"
#include "clientmodel.h"
#include "chainparams.h"
#include "db.h"
#include "util.h"

#include <QDir>

SettingsInformationWidget::SettingsInformationWidget(PIVXGUI* _window,QWidget *parent) :
    PWidget(_window,parent),
    ui(new Ui::SettingsInformationWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers

    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);

    ui->layoutOptions1->setProperty("cssClass", "container-options");
    ui->layoutOptions2->setProperty("cssClass", "container-options");
    ui->layoutOptions3->setProperty("cssClass", "container-options");


    // Title

    ui->labelTitle->setText("Information");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");


    // Subtitle

    ui->labelTitleGeneral->setText("General");
    ui->labelTitleGeneral->setProperty("cssClass", "text-title");


    ui->labelTitleClient->setText("Client Version: ");
    ui->labelTitleClient->setProperty("cssClass", "text-main-grey");
    ui->labelTitleAgent->setText("User Agent:");
    ui->labelTitleAgent->setProperty("cssClass", "text-main-grey");
    ui->labelTitleBerkeley->setText("Using BerkeleyDB version:");
    ui->labelTitleBerkeley->setProperty("cssClass", "text-main-grey");
    ui->labelTitleDataDir->setText("Datadir: ");
    ui->labelTitleDataDir->setProperty("cssClass", "text-main-grey");
    ui->labelTitleTime->setText("Startup Time:  ");
    ui->labelTitleTime->setProperty("cssClass", "text-main-grey");

    ui->labelTitleNetwork->setText("Network");
    ui->labelTitleNetwork->setProperty("cssClass", "text-title");

    ui->labelTitleName->setText("Name:");
    ui->labelTitleName->setProperty("cssClass", "text-main-grey");
    ui->labelTitleConnections->setText("Number Connections:");
    ui->labelTitleConnections->setProperty("cssClass", "text-main-grey");


    ui->labelTitleBlockchain->setText("Blockchain");
    ui->labelTitleBlockchain->setProperty("cssClass", "text-title");

    ui->labelTitleBlockNumber->setText("Current Number of Blocks:");
    ui->labelTitleBlockNumber->setProperty("cssClass", "text-main-grey");
    ui->labelTitleBlockTime->setText("Last Block Time:");
    ui->labelTitleBlockTime->setProperty("cssClass", "text-main-grey");

    ui->labelTitleMemory->setText("Memory Pool");
    ui->labelTitleMemory->setProperty("cssClass", "text-title");

    ui->labelTitleNumberTransactions->setText("Current Number of Transactions:");
    ui->labelTitleNumberTransactions->setProperty("cssClass", "text-main-grey");




    // Information General

    ui->labelInfoClient->setText("v0.17.0.0-afd206ceb-dirty");
    ui->labelInfoClient->setProperty("cssClass", "text-main-grey");
    ui->labelInfoAgent->setText("/Satoshi:0.17.0/");
    ui->labelInfoAgent->setProperty("cssClass", "text-main-grey");
    ui->labelInfoBerkeley->setText("Berkeley DB 4.8.30: (April 9, 2010)");
    ui->labelInfoBerkeley->setProperty("cssClass", "text-main-grey");
    ui->labelInfoDataDir->setText("/Users/furszy/Desktop/PIVX-test");
    ui->labelInfoDataDir->setProperty("cssClass", "text-main-grey");
    ui->labelInfoTime->setText("Oct, 25, 2018. Thursday, 1:32  AM");
    ui->labelInfoTime->setProperty("cssClass", "text-main-grey");

    // Information Network

    ui->labelInfoName->setText("Main");
    ui->labelInfoName->setProperty("cssClass", "text-main-grey");
    ui->labelInfoConnections->setText("0 (In: 0 / Out:0)");
    ui->labelInfoConnections->setProperty("cssClass", "text-main-grey");

    // Information Blockchain

    ui->labelInfoBlockNumber->setText("0");
    ui->labelInfoBlockNumber->setProperty("cssClass", "text-main-grey");
    ui->labelInfoBlockTime->setText("Sept 6, 2018. Thursday, 8:21:49 PM");
    ui->labelInfoBlockTime->setProperty("cssClass", "text-main-grey");

    // Information Memmory

    ui->labelInfoNumberTransactions->setText("0");
    ui->labelInfoNumberTransactions->setProperty("cssClass", "text-main-grey");

    // Buttons

    ui->pushButtonFile->setText("Backups");
    ui->pushButtonFile->setProperty("cssClass", "btn-secundary");

    ui->pushButtonBackups->setText("Wallet file");
    ui->pushButtonBackups->setProperty("cssClass", "btn-secundary");



    // Data
#ifdef ENABLE_WALLET
    // Wallet data -- remove it with if it's needed
    ui->labelInfoBerkeley->setText(DbEnv::version(0, 0, 0));
    ui->labelInfoDataDir->setText(QString::fromStdString(GetDataDir().string() + QDir::separator().toLatin1() + GetArg("-wallet", "wallet.dat")));
#else
    ui->labelInfoBerkeley->setText(tr("No information"));
#endif
}


void SettingsInformationWidget::loadClientModel(){
    if (clientModel && clientModel->getPeerTableModel() && clientModel->getBanTableModel()) {
        // Provide initial values
        ui->labelInfoClient->setText(clientModel->formatFullVersion());
        ui->labelInfoAgent->setText(clientModel->clientName());
        ui->labelInfoTime->setText(clientModel->formatClientStartupTime());
        ui->labelInfoName->setText(QString::fromStdString(Params().NetworkIDString()));


        /* TODO: Complete me..
        ui->clientVersion->setText(model->formatFullVersion());
        ui->clientName->setText(model->clientName());
        ui->buildDate->setText(model->formatBuildDate());
        ui->startupTime->setText(model->formatClientStartupTime());
        ui->networkName->setText(QString::fromStdString(Params().NetworkIDString()));
         */
    }
}

SettingsInformationWidget::~SettingsInformationWidget()
{
    delete ui;
}
