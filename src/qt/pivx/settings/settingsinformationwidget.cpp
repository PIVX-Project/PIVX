// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/settings/settingsinformationwidget.h"
#include "qt/pivx/settings/forms/ui_settingsinformationwidget.h"
#include "clientmodel.h"
#include "chainparams.h"
#include "db.h"
#include "util.h"
#include "guiutil.h"
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
    ui->labelTitleClient->setProperty("cssClass", "text-main-settings");
    ui->labelTitleAgent->setText("User Agent:");
    ui->labelTitleAgent->setProperty("cssClass", "text-main-settings");
    ui->labelTitleBerkeley->setText("Using BerkeleyDB version:");
    ui->labelTitleBerkeley->setProperty("cssClass", "text-main-settings");
    ui->labelTitleDataDir->setText("Datadir: ");
    ui->labelTitleDataDir->setProperty("cssClass", "text-main-settings");
    ui->labelTitleTime->setText("Startup Time:  ");
    ui->labelTitleTime->setProperty("cssClass", "text-main-settings");

    ui->labelTitleNetwork->setText("Network");
    ui->labelTitleNetwork->setProperty("cssClass", "text-title");

    ui->labelTitleName->setText("Name:");
    ui->labelTitleName->setProperty("cssClass", "text-main-settings");
    ui->labelTitleConnections->setText("Number Connections:");
    ui->labelTitleConnections->setProperty("cssClass", "text-main-settings");


    ui->labelTitleBlockchain->setText("Blockchain");
    ui->labelTitleBlockchain->setProperty("cssClass", "text-title");

    ui->labelTitleBlockNumber->setText("Current Number of Blocks:");
    ui->labelTitleBlockNumber->setProperty("cssClass", "text-main-settings");
    ui->labelTitleBlockTime->setText("Last Block Time:");
    ui->labelTitleBlockTime->setProperty("cssClass", "text-main-settings");

    ui->labelTitleMemory->setText("Memory Pool");
    ui->labelTitleMemory->setProperty("cssClass", "text-title");
    ui->labelTitleMemory->setVisible(false);

    ui->labelTitleNumberTransactions->setText("Current Number of Transactions:");
    ui->labelTitleNumberTransactions->setProperty("cssClass", "text-main-settings");
    ui->labelTitleNumberTransactions->setVisible(false);

    ui->labelInfoNumberTransactions->setText("0");
    ui->labelInfoNumberTransactions->setProperty("cssClass", "text-main-settings");
    ui->labelInfoNumberTransactions->setVisible(false);


    // Information General

    ui->labelInfoClient->setProperty("cssClass", "text-main-settings");
    ui->labelInfoAgent->setProperty("cssClass", "text-main-settings");
    ui->labelInfoBerkeley->setProperty("cssClass", "text-main-settings");
    ui->labelInfoDataDir->setProperty("cssClass", "text-main-settings");
    ui->labelInfoTime->setProperty("cssClass", "text-main-settings");

    // Information Network

    ui->labelInfoName->setText("Main");
    ui->labelInfoName->setProperty("cssClass", "text-main-settings");
    ui->labelInfoConnections->setText("0 (In: 0 / Out:0)");
    ui->labelInfoConnections->setProperty("cssClass", "text-main-settings");

    // Information Blockchain

    ui->labelInfoBlockNumber->setText("0");
    ui->labelInfoBlockNumber->setProperty("cssClass", "text-main-settings");
    ui->labelInfoBlockTime->setText("Sept 6, 2018. Thursday, 8:21:49 PM");
    ui->labelInfoBlockTime->setProperty("cssClass", "text-main-grey");

    // Buttons

    ui->pushButtonFile->setText("Wallet Conf");
    ui->pushButtonFile->setProperty("cssClass", "btn-secundary");

    ui->pushButtonBackups->setText("Backups");
    ui->pushButtonBackups->setProperty("cssClass", "btn-secundary");

    ui->pushButtonNetworkMonitor->setText(tr("Network Monitor"));
    ui->pushButtonNetworkMonitor->setProperty("cssClass", "btn-secundary");


    // Data
#ifdef ENABLE_WALLET
    // Wallet data -- remove it with if it's needed
    ui->labelInfoBerkeley->setText("Berkeley");//DbEnv::version(0, 0, 0));
    ui->labelInfoDataDir->setText(QString::fromStdString(GetDataDir().string() + QDir::separator().toLatin1() + GetArg("-wallet", "wallet.dat")));
#else
    ui->labelInfoBerkeley->setText(tr("No information"));
#endif

    connect(ui->pushButtonBackups, &QPushButton::clicked, [](){GUIUtil::showBackups();});
    connect(ui->pushButtonFile, &QPushButton::clicked, [](){GUIUtil::openConfigfile();});
    connect(ui->pushButtonNetworkMonitor, SIGNAL(clicked()), this, SLOT(openNetworkMonitor()));

}


void SettingsInformationWidget::loadClientModel(){
    if (clientModel && clientModel->getPeerTableModel() && clientModel->getBanTableModel()) {
        // Provide initial values
        ui->labelInfoClient->setText(clientModel->formatFullVersion());
        ui->labelInfoAgent->setText(clientModel->clientName());
        ui->labelInfoTime->setText(clientModel->formatClientStartupTime());
        ui->labelInfoName->setText(QString::fromStdString(Params().NetworkIDString()));

        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));
    }
}

void SettingsInformationWidget::setNumConnections(int count){
    if (!clientModel)
        return;

    QString connections = QString::number(count) + " (";
    connections += tr("In:") + " " + QString::number(clientModel->getNumConnections(CONNECTIONS_IN)) + " / ";
    connections += tr("Out:") + " " + QString::number(clientModel->getNumConnections(CONNECTIONS_OUT)) + ")";

    ui->labelInfoConnections->setText(connections);
}

void SettingsInformationWidget::setNumBlocks(int count){
    ui->labelInfoBlockNumber->setText(QString::number(count));
    if (clientModel)
        ui->labelInfoBlockTime->setText(clientModel->getLastBlockDate().toString());
}

void SettingsInformationWidget::openNetworkMonitor(){
    if(!rpcConsole){
        rpcConsole = new RPCConsole(0);
        rpcConsole->setClientModel(clientModel);
    }
    rpcConsole->showNetwork();
}

SettingsInformationWidget::~SettingsInformationWidget(){
    delete ui;
}
