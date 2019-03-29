//
// Created by furszy on 3/21/19.
//

#include "PIVXGUI.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <qt/guiutil.h>
#include "networkstyle.h"
#include "notificator.h"

#include "qt/pivx/qtutils.h"


#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QColor>
#include <QShortcut>
#include <QKeySequence>


#include "util.h"


const QString PIVXGUI::DEFAULT_WALLET = "~Default";

PIVXGUI::PIVXGUI(const NetworkStyle* networkStyle, QWidget* parent) :
        QMainWindow(parent),
        clientModel(0){

    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    GUIUtil::restoreWindowGeometry("nWindow", QSize(1200, 700), this);

    QString windowTitle = tr("PIVX Core") + " - ";
#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !GetBoolArg("-disablewallet", false);
#else
    enableWallet = false;
#endif // ENABLE_WALLET

    if (enableWallet) {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }

    setWindowTitle(windowTitle);

#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getAppIcon());
    setWindowIcon(networkStyle->getAppIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif




#ifdef ENABLE_WALLET
    // Create wallet frame
    if(enableWallet){

        QFrame* centralWidget = new QFrame(this);
        this->setMinimumWidth(1200);
        QHBoxLayout* centralWidgetLayouot = new QHBoxLayout();
        centralWidget->setLayout(centralWidgetLayouot);
        centralWidgetLayouot->setContentsMargins(0,0,0,0);
        centralWidgetLayouot->setSpacing(0);

        centralWidget->setProperty("cssClass", "container");
        centralWidget->setStyleSheet("padding:0px; border:none; margin:0px;");

        // First the nav
        navMenu = new NavMenuWidget(this);
        centralWidgetLayouot->addWidget(navMenu);

        this->setCentralWidget(centralWidget);
        this->setContentsMargins(0,0,0,0);

        QFrame *container = new QFrame(centralWidget);
        container->setContentsMargins(0,0,0,0);
        centralWidgetLayouot->addWidget(container);

        // Then topbar + the stackedWidget
        QVBoxLayout *baseScreensContainer = new QVBoxLayout(this);
        baseScreensContainer->setMargin(0);
        container->setLayout(baseScreensContainer);

        // Insert the topbar
        topBar = new TopBar(this);
        topBar->setContentsMargins(0,0,0,0);
        baseScreensContainer->addWidget(topBar);

        // Now stacked widget
        stackedContainer = new QStackedWidget(this);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        stackedContainer->setSizePolicy(sizePolicy);
        stackedContainer->setContentsMargins(0,0,0,0);
        baseScreensContainer->addWidget(stackedContainer);

        // Init
        dashboard = new DashboardWidget(this, this);
        sendWidget = new SendWidget(this, this);
        receiveWidget = new ReceiveWidget(this,this);
        addressesWidget = new AddressesWidget(this,this);
        privacyWidget = new PrivacyWidget(this,this);
        settingsWidget = new SettingsWidget(this,this);

        // Add to parent
        stackedContainer->addWidget(dashboard);
        stackedContainer->addWidget(sendWidget);
        stackedContainer->addWidget(receiveWidget);
        stackedContainer->addWidget(addressesWidget);
        stackedContainer->addWidget(privacyWidget);
        stackedContainer->addWidget(settingsWidget);
        stackedContainer->setCurrentWidget(dashboard);

    } else
#endif
    {
        // When compiled without wallet or -disablewallet is provided,
        // the central widget is the rpc console.
        rpcConsole = new RPCConsole(enableWallet ? this : 0);
        setCentralWidget(rpcConsole);
    }


    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Connect events
    connectActions();

}

static Qt::Modifier shortKey
#ifdef Q_OS_MAC
     = Qt::CTRL;
#else
     = Qt::ALT;
#endif


/**
 * Here add every event connection
 */
void PIVXGUI::connectActions() {

    QShortcut *homeShort = new QShortcut(this);
    QShortcut *sendShort = new QShortcut(this);
    QShortcut *receiveShort = new QShortcut(this);
    QShortcut *addressesShort = new QShortcut(this);
    QShortcut *privacyShort = new QShortcut(this);
    QShortcut *settingsShort = new QShortcut(this);

    homeShort->setKey(QKeySequence(shortKey + Qt::Key_1));
    sendShort->setKey(QKeySequence(shortKey + Qt::Key_2));
    receiveShort->setKey(QKeySequence(shortKey + Qt::Key_3));
    addressesShort->setKey(QKeySequence(shortKey + Qt::Key_4));
    privacyShort->setKey(QKeySequence(shortKey + Qt::Key_5));
    settingsShort->setKey(QKeySequence(shortKey + Qt::Key_6));

    connect(homeShort, SIGNAL(activated()), this, SLOT(goToDashboard()));
    connect(sendShort, SIGNAL(activated()), this, SLOT(goToSend()));
    connect(receiveShort, SIGNAL(activated()), this, SLOT(goToReceive()));
    connect(addressesShort, SIGNAL(activated()), this, SLOT(goToAddresses()));
    connect(privacyShort, SIGNAL(activated()), this, SLOT(goToPrivacy()));
    connect(settingsShort, SIGNAL(activated()), this, SLOT(goToSettings()));
}


void PIVXGUI::createTrayIcon(const NetworkStyle* networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("PIVX Core client") + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getAppIcon());
    trayIcon->hide();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

//
PIVXGUI::~PIVXGUI() {

}







void PIVXGUI::setClientModel(ClientModel* clientModel) {
    this->clientModel = clientModel;
    // TODO: Complete me..
}



void PIVXGUI::goToDashboard(){
    if(stackedContainer->currentWidget() != dashboard){
        stackedContainer->setCurrentWidget(dashboard);
        topBar->showBottom();
    }
}




void PIVXGUI::goToSend(){
    showTop(sendWidget);
}

void PIVXGUI::goToAddresses(){
    showTop(addressesWidget);
}

void PIVXGUI::goToPrivacy(){
    showTop(privacyWidget);
}

void PIVXGUI::goToMasterNodes(){
    //showTop(masterNodesWidget);
}

void PIVXGUI::goToSettings(){
    showTop(settingsWidget);
}

void PIVXGUI::goToReceive(){
    showTop(receiveWidget);
}

void PIVXGUI::showTop(QWidget* view){
    if(stackedContainer->currentWidget() != view){
        stackedContainer->setCurrentWidget(view);
        topBar->showTop();
    }
}

void PIVXGUI::changeTheme(bool isLightTheme){
    // Change theme in all of the childs here..

    QString css = isLightTheme ? getLightTheme() : getDarkTheme();
    this->setStyleSheet(css);

    // Notify
    emit themeChanged(isLightTheme, css);

    // Update style
    updateStyle(this);
}

void PIVXGUI::resizeEvent(QResizeEvent* event){
    // Parent..
    QMainWindow::resizeEvent(event);
    // background
    showHide(opEnabled);
    // Notify
    emit windowResizeEvent(event);
}

void PIVXGUI::showHide(bool show){
    if(!op) op = new QLabel(this);
    if(!show){
        op->setVisible(false);
        opEnabled = false;
    }else{
        QColor bg("#000000");
        bg.setAlpha(200);
        if(!isLightTheme()){
            bg = QColor("#00000000");
            bg.setAlpha(150);
        }

        QPalette palette;
        palette.setColor(QPalette::Window, bg);
        op->setAutoFillBackground(true);
        op->setPalette(palette);
        op->setWindowFlags(Qt::CustomizeWindowHint);
        op->move(0,0);
        op->show();
        op->activateWindow();
        op->resize(width(), height());
        op->setVisible(true);
        opEnabled = true;
    }
}

int PIVXGUI::getNavWidth(){
    return this->navMenu->width();
}


#ifdef ENABLE_WALLET
bool PIVXGUI::addWallet(const QString& name, WalletModel* walletModel)
{
    // Single wallet supported for now..
    if(!stackedContainer || !clientModel || !walletModel)
        return false;

    // todo: show out of sync warning..
    // todo: complete this next method
    //connect(walletView, SIGNAL(showNormalIfMinimized()), gui, SLOT(showNormalIfMinimized()));

    // set the model for every view
    dashboard->setWalletModel(walletModel);


    return true;
}

bool PIVXGUI::setCurrentWallet(const QString& name)
{
    //if (!walletFrame)
    //    return false;
    return true;//walletFrame->setCurrentWallet(name);
}

void PIVXGUI::removeAllWallets()
{
    //if (!walletFrame)
    //    return;
    //setWalletActionsEnabled(false);
    //walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET
