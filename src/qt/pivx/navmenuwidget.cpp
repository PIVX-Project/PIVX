#include "qt/pivx/navmenuwidget.h"
#include "qt/pivx/forms/ui_navmenuwidget.h"
#include <QFile>
#include "PIVXGUI.h"

NavMenuWidget::NavMenuWidget(PIVXGUI *mainWindow, QWidget *parent) :
    QWidget(parent),
    window(mainWindow),
    ui(new Ui::NavMenuWidget)
{
    ui->setupUi(this);

    this->setFixedWidth(100);

    ui->navContainer_2->setProperty("cssClass", "container-nav");

    ui->imgLogo->setProperty("cssClass", "img-nav-logo");

    // App version
    ui->labelVersion->setText("v 1.0.0");
    ui->labelVersion->setProperty("cssClass", "text-title-white");

    // Buttons

    ui->btnDashboard->setProperty("cssClass", "btn-nav-dash");
    ui->btnDashboard->setText("HOME\n");
    ui->btnDashboard->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnSend->setProperty("cssClass", "btn-nav-send");
    ui->btnSend->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    ui->btnSend->setText("SEND\n");

    ui->btnAddress->setProperty("cssClass", "btn-nav-address");
    ui->btnAddress->setText("CONTACTS\n");
    ui->btnAddress->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnPrivacy->setProperty("cssClass", "btn-nav-privacy");
    ui->btnPrivacy->setText("PRIVACY\n");
    ui->btnPrivacy->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnMaster->setProperty("cssClass", "btn-nav-master");
    ui->btnMaster->setText("MASTER\nNODES");
    ui->btnMaster->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnSettings->setProperty("cssClass", "btn-nav-settings");
    ui->btnSettings->setText("SETTINGS\n");
    ui->btnSettings->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    ui->btnReceive->setProperty("cssClass", "btn-nav-receive");
    ui->btnReceive->setText("RECEIVE\n");
    ui->btnReceive->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);




    connect(ui->btnDashboard,SIGNAL(clicked()),this, SLOT(onDashboardClicked()));
    connect(ui->btnSend,SIGNAL(clicked()),this, SLOT(onSendClicked()));
    connect(ui->btnAddress,SIGNAL(clicked()),this, SLOT(onAddressClicked()));
    connect(ui->btnPrivacy,SIGNAL(clicked()),this, SLOT(onPrivacyClicked()));
    connect(ui->btnMaster,SIGNAL(clicked()),this, SLOT(onMasterNodesClicked()));
    connect(ui->btnSettings,SIGNAL(clicked()),this, SLOT(onSettingsClicked()));
    connect(ui->btnReceive,SIGNAL(clicked()),this, SLOT(onReceiveClicked()));
}

void NavMenuWidget::onSendClicked(){
    //window->goToSend();
}

void NavMenuWidget::onDashboardClicked(){
    //window->goToDashboard();
}

void NavMenuWidget::onAddressClicked(){
    //window->goToAddressBook();
}


void NavMenuWidget::onPrivacyClicked(){
    //window->goToPrivacy();
}

void NavMenuWidget::onMasterNodesClicked(){
    //window->goToMasterNodes();
}

void NavMenuWidget::onSettingsClicked(){
    //window->goToSettings();
}

void NavMenuWidget::onReceiveClicked(){
    //window->goToReceive();
}

NavMenuWidget::~NavMenuWidget()
{
    delete ui;
}
