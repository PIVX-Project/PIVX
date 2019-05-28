//
// Created by furszy on 3/21/19.
//

#include "PIVXGUI.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <qt/guiutil.h>
#include "clientmodel.h"
#include "optionsmodel.h"
#include "networkstyle.h"
#include "notificator.h"
#include "ui_interface.h"
#include "qt/pivx/qtutils.h"

#include "qt/pivx/defaultdialog.h"


#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QColor>
#include <QShortcut>
#include <QKeySequence>

// TODO: Remove this..
#include <QMessageBox>


#include "util.h"


const QString PIVXGUI::DEFAULT_WALLET = "~Default";

PIVXGUI::PIVXGUI(const NetworkStyle* networkStyle, QWidget* parent) :
        QMainWindow(parent),
        clientModel(0){

    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setMinimumSize(1200, 700);
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
        this->setMinimumHeight(700);
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
        baseScreensContainer->setSpacing(0);
        baseScreensContainer->setContentsMargins(0,0,0,0);
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

    // TODO: Add event filter??
    // // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    //    this->installEventFilter(this);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

}



/**
 * Here add every event connection
 */
void PIVXGUI::connectActions() {

    QShortcut *consoleShort = new QShortcut(this);

    consoleShort->setKey(QKeySequence(SHORT_KEY + Qt::Key_C));
    connect(consoleShort, &QShortcut::activated, [this](){
        navMenu->selectSettings();
        settingsWidget->showDebugConsole();
        goToSettings();
    });

    connect(topBar, &TopBar::showHide, this, &PIVXGUI::showHide);
    connect(topBar, &TopBar::themeChanged, this, &PIVXGUI::changeTheme);
    connect(settingsWidget, &SettingsWidget::showHide, this, &PIVXGUI::showHide);
    connect(sendWidget, &SendWidget::showHide, this, &PIVXGUI::showHide);
    connect(receiveWidget, &ReceiveWidget::showHide, this, &PIVXGUI::showHide);
    connect(addressesWidget, &AddressesWidget::showHide, this, &PIVXGUI::showHide);
    connect(privacyWidget, &PrivacyWidget::showHide, this, &PIVXGUI::showHide);
    connect(settingsWidget, &SettingsWidget::execDialog, this, &PIVXGUI::execDialog);
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
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if (trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    MacDockIconHandler::cleanup();
#endif
}


/** Get restart command-line parameters and request restart */
void PIVXGUI::handleRestart(QStringList args){
    if (!ShutdownRequested())
        emit requestedRestart(args);
}


void PIVXGUI::setClientModel(ClientModel* clientModel) {
    this->clientModel = clientModel;
    if(this->clientModel) {
        topBar->setClientModel(clientModel);
        sendWidget->setClientModel(clientModel);
        settingsWidget->setClientModel(clientModel);

        // Receive and report messages from client model
        connect(clientModel, SIGNAL(message(QString, QString, unsigned int)), this, SLOT(message(QString, QString, unsigned int)));
        connect(topBar, SIGNAL(walletSynced(bool)), dashboard, SLOT(walletSynced(bool)));

        // Get restart command-line parameters and handle restart
        connect(settingsWidget, &SettingsWidget::handleRestart, [this](QStringList arg){handleRestart(arg);});

        if (rpcConsole) {
            rpcConsole->setClientModel(clientModel);
        }

        if (trayIcon) {
            trayIcon->show();
        }
    }
}


void PIVXGUI::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void PIVXGUI::closeEvent(QCloseEvent* event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if (clientModel && clientModel->getOptionsModel()) {
        if (!clientModel->getOptionsModel()->getMinimizeOnClose()) {
            QApplication::quit();
        }
    }
#endif
    QMainWindow::closeEvent(event);
}


void PIVXGUI::messageInfo(const QString& text){
    if(!this->snackBar) this->snackBar = new SnackBar(this, this);
    this->snackBar->setText(text);
    this->snackBar->resize(this->width(), snackBar->height());
    openDialog(this->snackBar, this);
}

/**
 * TODO remove QMessageBox for the snackbar..
 */
void PIVXGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret)
{
    try {
        QString strTitle = tr("PIVX Core"); // default title
        // Default to information icon
        int nMBoxIcon = QMessageBox::Information;
        int nNotifyIcon = Notificator::Information;

        QString msgType;

        // Prefer supplied title over style based title
        if (!title.isEmpty()) {
            msgType = title;
        } else {
            switch (style) {
                case CClientUIInterface::MSG_ERROR:
                    msgType = tr("Error");
                    break;
                case CClientUIInterface::MSG_WARNING:
                    msgType = tr("Warning");
                    break;
                case CClientUIInterface::MSG_INFORMATION:
                    msgType = tr("Information");
                    break;
                default:
                    break;
            }
        }
        // Append title to "PIVX - "
        if (!msgType.isEmpty())
            strTitle += " - " + msgType;

        // Check for error/warning icon
        if (style & CClientUIInterface::ICON_ERROR) {
            nMBoxIcon = QMessageBox::Critical;
            nNotifyIcon = Notificator::Critical;
        } else if (style & CClientUIInterface::ICON_WARNING) {
            nMBoxIcon = QMessageBox::Warning;
            nNotifyIcon = Notificator::Warning;
        }

        // Display message
        if (style & CClientUIInterface::MODAL) {
            // Check for buttons, use OK as default, if none was supplied
            int r = 0;
            // TODO: check this method..
            showNormalIfMinimized();
            if(style & CClientUIInterface::BTN_MASK){
                r = openStandardDialog(title, message, "OK", "CANCEL");
            }else{
                r = openStandardDialog(title, message, "OK");
            }
            if (ret != NULL)
                *ret = r;
        } else if(style & CClientUIInterface::MSG_INFORMATION_SNACK){
            messageInfo(message);
        }else
            notificator->notify((Notificator::Class) nNotifyIcon, strTitle, message);

    // TODO: Furszy remove this please..
    } catch (std::exception &e){
        LogPrintf("ERROR in message PIVXGUI.. %s\n", e.what());
    } catch (...){
        LogPrintf("ERROR in message  PIVXGUI..\n");
    }
}

bool PIVXGUI::openStandardDialog(QString title, QString body, QString okBtn, QString cancelBtn){
    showHide(true);
    DefaultDialog *dialog = new DefaultDialog(this);
    dialog->setText(title, body, okBtn, cancelBtn);
    dialog->adjustSize();
    openDialogWithOpaqueBackground(dialog, this);
    bool ret = dialog->isOk;
    dialog->deleteLater();
    return ret;
}


void PIVXGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if (!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden()) {
        show();
        activateWindow();
    } else if (isMinimized()) {
        showNormal();
        activateWindow();
    } else if (GUIUtil::isObscured(this)) {
        raise();
        activateWindow();
    } else if (fToggleHidden)
        hide();
}

void PIVXGUI::detectShutdown() {
    if (ShutdownRequested()) {
        if (rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
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

    QString css = GUIUtil::loadStyleSheet();
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

bool PIVXGUI::execDialog(QDialog *dialog, int xDiv, int yDiv){
    return openDialogWithOpaqueBackgroundY(dialog, this);
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
    topBar->setWalletModel(walletModel);
    receiveWidget->setWalletModel(walletModel);
    sendWidget->setWalletModel(walletModel);
    addressesWidget->setWalletModel(walletModel);
    privacyWidget->setWalletModel(walletModel);
    settingsWidget->setWalletModel(walletModel);

    // Connect actions..
    connect(privacyWidget, &PrivacyWidget::message, this, &PIVXGUI::message);
    connect(topBar, &TopBar::message, this, &PIVXGUI::message);
    connect(sendWidget, &SendWidget::message,this, &PIVXGUI::message);
    connect(receiveWidget, &ReceiveWidget::message,this, &PIVXGUI::message);
    connect(addressesWidget, &AddressesWidget::message,this, &PIVXGUI::message);
    connect(settingsWidget, &SettingsWidget::message, this, &PIVXGUI::message);

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


static bool ThreadSafeMessageBox(PIVXGUI* gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
              modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
              Q_ARG(QString, QString::fromStdString(caption)),
              Q_ARG(QString, QString::fromStdString(message)),
              Q_ARG(unsigned int, style),
              Q_ARG(bool*, &ret));
    return ret;
}


void PIVXGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void PIVXGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}
