#include "qt/pivx/navmenuwidget.h"
#include "qt/pivx/forms/ui_navmenuwidget.h"
#include <QFile>

#include "qt/pivx/PIVXGUI.h"

NavMenuWidget::NavMenuWidget(PIVXGUI *mainWindow, QWidget *parent) :
    QWidget(parent),
    window(mainWindow),
    ui(new Ui::NavMenuWidget)
{
    ui->setupUi(this);

    //this->setStyleSheet(parent->styleSheet());
    setFixedWidth(100);

    ui->navContainer_2->setProperty("cssClass", "container-nav");

    // App version
    ui->labelVersion->setText("v 1.0.0");
    ui->labelVersion->setProperty("cssClass", "text-title-white");

    // Buttons

    ui->btnDashboard->setProperty("cssClass", "btn-nav-dash");
    ui->btnDashboard->setText("HOME");
    ui->btnDashboard->setStyleSheet("padding-top: 25px;");

    ui->btnSend->setProperty("cssClass", "btn-nav-send");
    ui->btnSend->setText("SEND");
    ui->btnSend->setStyleSheet("padding-top: 25px;");

    ui->btnAddress->setProperty("cssClass", "btn-nav-address");
    ui->btnAddress->setText("CONTACTS");
    ui->btnAddress->setStyleSheet("padding-top: 25px;");

    ui->btnPrivacy->setProperty("cssClass", "btn-nav-privacy");
    ui->btnPrivacy->setText("PRIVACY");
    ui->btnPrivacy->setStyleSheet("padding-top: 25px;");

    ui->btnMaster->setProperty("cssClass", "btn-nav-master");
    ui->btnMaster->setText("MASTER\nNODES");
    ui->btnMaster->setStyleSheet("padding-top: 32px;");

    ui->btnSettings->setProperty("cssClass", "btn-nav-settings");
    ui->btnSettings->setText("SETTINGS");
    ui->btnSettings->setStyleSheet("padding-top: 25px;");

    ui->btnReceive->setProperty("cssClass", "btn-nav-receive");
    ui->btnReceive->setText("RECEIVE");
    ui->btnReceive->setStyleSheet("padding-top: 25px;");



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
