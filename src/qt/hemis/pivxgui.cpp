<<<<<<< HEAD
// Copyright (c) 2019-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/pivxgui.h"
=======
// Copyright (c) 2019-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/hemis/hemisgui.h"
>>>>>>> e92f29f73 (init)

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "qt/guiutil.h"
#include "clientmodel.h"
#include "interfaces/handler.h"
#include "optionsmodel.h"
#include "networkstyle.h"
#include "notificator.h"
#include "guiinterface.h"
<<<<<<< HEAD
#include "qt/pivx/qtutils.h"
#include "qt/pivx/defaultdialog.h"
=======
#include "qt/hemis/qtutils.h"
#include "qt/hemis/defaultdialog.h"
>>>>>>> e92f29f73 (init)
#include "shutdown.h"
#include "util/system.h"

#include <QApplication>
#include <QColor>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QScreen>
#include <QShortcut>
#include <QWindowStateChangeEvent>


#define BASE_WINDOW_WIDTH 1200
#define BASE_WINDOW_HEIGHT 740
#define BASE_WINDOW_MIN_HEIGHT 620
#define BASE_WINDOW_MIN_WIDTH 1100


<<<<<<< HEAD
const QString PIVXGUI::DEFAULT_WALLET = "~Default";

PIVXGUI::PIVXGUI(const NetworkStyle* networkStyle, QWidget* parent) :
=======
const QString hemisGUI::DEFAULT_WALLET = "~Default";

hemisGUI::hemisGUI(const NetworkStyle* networkStyle, QWidget* parent) :
>>>>>>> e92f29f73 (init)
        QMainWindow(parent),
        clientModel(0){

    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setMinimumSize(BASE_WINDOW_MIN_WIDTH, BASE_WINDOW_MIN_HEIGHT);


    // Adapt screen size
    QRect rec = QGuiApplication::primaryScreen()->geometry();
    int adaptedHeight = (rec.height() < BASE_WINDOW_HEIGHT) ?  BASE_WINDOW_MIN_HEIGHT : BASE_WINDOW_HEIGHT;
    int adaptedWidth = (rec.width() < BASE_WINDOW_WIDTH) ?  BASE_WINDOW_MIN_WIDTH : BASE_WINDOW_WIDTH;
    GUIUtil::restoreWindowGeometry(
            "nWindow",
            QSize(adaptedWidth, adaptedHeight),
            this
    );

#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET);
#else
    enableWallet = false;
#endif // ENABLE_WALLET

    QString windowTitle = QString::fromStdString(gArgs.GetArg("-windowtitle", ""));
    if (windowTitle.isEmpty()) {
        windowTitle = QString{PACKAGE_NAME} + " - ";
        windowTitle += ((enableWallet) ? tr("Wallet") : tr("Node"));
    }
    windowTitle += " " + networkStyle->getTitleAddText();
    setWindowTitle(windowTitle);

    QApplication::setWindowIcon(networkStyle->getAppIcon());
    setWindowIcon(networkStyle->getAppIcon());

#ifdef ENABLE_WALLET
    // Create wallet frame
    if (enableWallet) {
        QFrame* centralWidget = new QFrame(this);
        this->setMinimumWidth(BASE_WINDOW_MIN_WIDTH);
        this->setMinimumHeight(BASE_WINDOW_MIN_HEIGHT);
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
        dashboard = new DashboardWidget(this);
        sendWidget = new SendWidget(this);
        receiveWidget = new ReceiveWidget(this);
        addressesWidget = new AddressesWidget(this);
        masterNodesWidget = new MasterNodesWidget(this);
        coldStakingWidget = new ColdStakingWidget(this);
        governancewidget = new GovernanceWidget(this);
        settingsWidget = new SettingsWidget(this);

        // Add to parent
        stackedContainer->addWidget(dashboard);
        stackedContainer->addWidget(sendWidget);
        stackedContainer->addWidget(receiveWidget);
        stackedContainer->addWidget(addressesWidget);
        stackedContainer->addWidget(masterNodesWidget);
        stackedContainer->addWidget(coldStakingWidget);
        stackedContainer->addWidget(governancewidget);
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

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions(networkStyle);

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

<<<<<<< HEAD
void PIVXGUI::createActions(const NetworkStyle* networkStyle)
=======
void hemisGUI::createActions(const NetworkStyle* networkStyle)
>>>>>>> e92f29f73 (init)
{
    toggleHideAction = new QAction(networkStyle->getAppIcon(), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);

<<<<<<< HEAD
    connect(toggleHideAction, &QAction::triggered, this, &PIVXGUI::toggleHidden);
=======
    connect(toggleHideAction, &QAction::triggered, this, &hemisGUI::toggleHidden);
>>>>>>> e92f29f73 (init)
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

/**
 * Here add every event connection
 */
<<<<<<< HEAD
void PIVXGUI::connectActions()
=======
void hemisGUI::connectActions()
>>>>>>> e92f29f73 (init)
{
    QShortcut *consoleShort = new QShortcut(this);
    consoleShort->setKey(QKeySequence(SHORT_KEY + Qt::Key_C));
    connect(consoleShort, &QShortcut::activated, [this](){
        navMenu->selectSettings();
        settingsWidget->showDebugConsole();
        goToSettings();
    });
<<<<<<< HEAD
    connect(topBar, &TopBar::showHide, this, &PIVXGUI::showHide);
    connect(topBar, &TopBar::themeChanged, this, &PIVXGUI::changeTheme);
    connect(topBar, &TopBar::onShowHideColdStakingChanged, navMenu, &NavMenuWidget::onShowHideColdStakingChanged);
    connect(settingsWidget, &SettingsWidget::showHide, this, &PIVXGUI::showHide);
    connect(sendWidget, &SendWidget::showHide, this, &PIVXGUI::showHide);
    connect(receiveWidget, &ReceiveWidget::showHide, this, &PIVXGUI::showHide);
    connect(addressesWidget, &AddressesWidget::showHide, this, &PIVXGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::showHide, this, &PIVXGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::execDialog, this, &PIVXGUI::execDialog);
    connect(coldStakingWidget, &ColdStakingWidget::showHide, this, &PIVXGUI::showHide);
    connect(coldStakingWidget, &ColdStakingWidget::execDialog, this, &PIVXGUI::execDialog);
    connect(governancewidget, &GovernanceWidget::showHide, this, &PIVXGUI::showHide);
    connect(governancewidget, &GovernanceWidget::execDialog, this, &PIVXGUI::execDialog);
    connect(settingsWidget, &SettingsWidget::execDialog, this, &PIVXGUI::execDialog);
}


void PIVXGUI::createTrayIcon(const NetworkStyle* networkStyle)
=======
    connect(topBar, &TopBar::showHide, this, &hemisGUI::showHide);
    connect(topBar, &TopBar::themeChanged, this, &hemisGUI::changeTheme);
    connect(topBar, &TopBar::onShowHideColdStakingChanged, navMenu, &NavMenuWidget::onShowHideColdStakingChanged);
    connect(settingsWidget, &SettingsWidget::showHide, this, &hemisGUI::showHide);
    connect(sendWidget, &SendWidget::showHide, this, &hemisGUI::showHide);
    connect(receiveWidget, &ReceiveWidget::showHide, this, &hemisGUI::showHide);
    connect(addressesWidget, &AddressesWidget::showHide, this, &hemisGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::showHide, this, &hemisGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::execDialog, this, &hemisGUI::execDialog);
    connect(coldStakingWidget, &ColdStakingWidget::showHide, this, &hemisGUI::showHide);
    connect(coldStakingWidget, &ColdStakingWidget::execDialog, this, &hemisGUI::execDialog);
    connect(governancewidget, &GovernanceWidget::showHide, this, &hemisGUI::showHide);
    connect(governancewidget, &GovernanceWidget::execDialog, this, &hemisGUI::execDialog);
    connect(settingsWidget, &SettingsWidget::execDialog, this, &hemisGUI::execDialog);
}


void hemisGUI::createTrayIcon(const NetworkStyle* networkStyle)
>>>>>>> e92f29f73 (init)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("%1 client").arg(PACKAGE_NAME) + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getAppIcon());
    trayIcon->hide();
#endif
    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

<<<<<<< HEAD
PIVXGUI::~PIVXGUI()
=======
hemisGUI::~hemisGUI()
>>>>>>> e92f29f73 (init)
{
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
<<<<<<< HEAD
void PIVXGUI::handleRestart(QStringList args)
=======
void hemisGUI::handleRestart(QStringList args)
>>>>>>> e92f29f73 (init)
{
    if (!ShutdownRequested())
        Q_EMIT requestedRestart(args);
}


<<<<<<< HEAD
void PIVXGUI::setClientModel(ClientModel* _clientModel)
=======
void hemisGUI::setClientModel(ClientModel* _clientModel)
>>>>>>> e92f29f73 (init)
{
    this->clientModel = _clientModel;
    if (this->clientModel) {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        topBar->setClientModel(clientModel);
        dashboard->setClientModel(clientModel);
        sendWidget->setClientModel(clientModel);
        masterNodesWidget->setClientModel(clientModel);
        settingsWidget->setClientModel(clientModel);
        governancewidget->setClientModel(clientModel);

        // Receive and report messages from client model
<<<<<<< HEAD
        connect(clientModel, &ClientModel::message, this, &PIVXGUI::message);
=======
        connect(clientModel, &ClientModel::message, this, &hemisGUI::message);
>>>>>>> e92f29f73 (init)
        connect(clientModel, &ClientModel::alertsChanged, [this](const QString& _alertStr) {
            message(tr("Alert!"), _alertStr, CClientUIInterface::MSG_WARNING);
        });
        connect(topBar, &TopBar::walletSynced, dashboard, &DashboardWidget::walletSynced);
        connect(topBar, &TopBar::walletSynced, coldStakingWidget, &ColdStakingWidget::walletSynced);
        connect(topBar, &TopBar::tierTwoSynced, governancewidget, &GovernanceWidget::tierTwoSynced);

        // Get restart command-line parameters and handle restart
        connect(settingsWidget, &SettingsWidget::handleRestart, [this](QStringList arg){handleRestart(arg);});

        if (rpcConsole) {
            rpcConsole->setClientModel(clientModel);
        }

        if (trayIcon) {
            trayIcon->show();
        }
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if (trayIconMenu) {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
    }
}

<<<<<<< HEAD
void PIVXGUI::createTrayIconMenu()
=======
void hemisGUI::createTrayIconMenu()
>>>>>>> e92f29f73 (init)
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-macOSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

<<<<<<< HEAD
    connect(trayIcon, &QSystemTrayIcon::activated, this, &PIVXGUI::trayIconActivated);
#else
    // Note: On macOS, the Dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    connect(dockIconHandler, &MacDockIconHandler::dockIconClicked, this, &PIVXGUI::macosDockIconActivated);
=======
    connect(trayIcon, &QSystemTrayIcon::activated, this, &hemisGUI::trayIconActivated);
#else
    // Note: On macOS, the Dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    connect(dockIconHandler, &MacDockIconHandler::dockIconClicked, this, &hemisGUI::macosDockIconActivated);
>>>>>>> e92f29f73 (init)

    trayIconMenu = new QMenu(this);
    trayIconMenu->setAsDockMenu();
#endif

    // Configuration of the tray icon (or Dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();

#ifndef Q_OS_MAC // This is built-in on macOS
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
<<<<<<< HEAD
void PIVXGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
=======
void hemisGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
>>>>>>> e92f29f73 (init)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#else
<<<<<<< HEAD
void PIVXGUI::macosDockIconActivated()
=======
void hemisGUI::macosDockIconActivated()
>>>>>>> e92f29f73 (init)
 {
     show();
     activateWindow();
 }
#endif

<<<<<<< HEAD
void PIVXGUI::changeEvent(QEvent* e)
=======
void hemisGUI::changeEvent(QEvent* e)
>>>>>>> e92f29f73 (init)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
<<<<<<< HEAD
                QTimer::singleShot(0, this, &PIVXGUI::hide);
                e->ignore();
            } else if ((wsevt->oldState() & Qt::WindowMinimized) && !isMinimized()) {
                QTimer::singleShot(0, this, &PIVXGUI::show);
=======
                QTimer::singleShot(0, this, &hemisGUI::hide);
                e->ignore();
            } else if ((wsevt->oldState() & Qt::WindowMinimized) && !isMinimized()) {
                QTimer::singleShot(0, this, &hemisGUI::show);
>>>>>>> e92f29f73 (init)
                e->ignore();
            }
        }
    }
#endif
}

<<<<<<< HEAD
void PIVXGUI::closeEvent(QCloseEvent* event)
=======
void hemisGUI::closeEvent(QCloseEvent* event)
>>>>>>> e92f29f73 (init)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if (clientModel && clientModel->getOptionsModel()) {
        if (!clientModel->getOptionsModel()->getMinimizeOnClose()) {
            QApplication::quit();
        } else {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    QMainWindow::closeEvent(event);
#endif
}


<<<<<<< HEAD
void PIVXGUI::messageInfo(const QString& text)
=======
void hemisGUI::messageInfo(const QString& text)
>>>>>>> e92f29f73 (init)
{
    if (!this->snackBar) this->snackBar = new SnackBar(this, this);
    this->snackBar->setText(text);
    this->snackBar->resize(this->width(), snackBar->height());
    openDialog(this->snackBar, this);
}


<<<<<<< HEAD
void PIVXGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret)
=======
void hemisGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret)
>>>>>>> e92f29f73 (init)
{
    QString strTitle = QString{PACKAGE_NAME}; // default title
    // Default to information icon
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
                msgType = tr("System Message");
                break;
        }
    }

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        int r = 0;
        showNormalIfMinimized();
        if (style & CClientUIInterface::BTN_MASK) {
            r = openStandardDialog(
                    (title.isEmpty() ? strTitle : title), message, "OK", "CANCEL"
                );
        } else {
            r = openStandardDialog((title.isEmpty() ? strTitle : title), message, "OK");
        }
        if (ret != nullptr)
            *ret = r;
    } else if (style & CClientUIInterface::MSG_INFORMATION_SNACK) {
        messageInfo(message);
    } else {
<<<<<<< HEAD
        // Append title to "PIVX - "
=======
        // Append title to "hemis - "
>>>>>>> e92f29f73 (init)
        if (!msgType.isEmpty())
            strTitle += " - " + msgType;
        notificator->notify(static_cast<Notificator::Class>(nNotifyIcon), strTitle, message);
    }
}

<<<<<<< HEAD
bool PIVXGUI::openStandardDialog(QString title, QString body, QString okBtn, QString cancelBtn)
=======
bool hemisGUI::openStandardDialog(QString title, QString body, QString okBtn, QString cancelBtn)
>>>>>>> e92f29f73 (init)
{
    DefaultDialog *dialog;
    if (isVisible()) {
        showHide(true);
        dialog = new DefaultDialog(this);
        dialog->setText(title, body, okBtn, cancelBtn);
        dialog->adjustSize();
        openDialogWithOpaqueBackground(dialog, this);
    } else {
        dialog = new DefaultDialog();
        dialog->setText(title, body, okBtn);
        dialog->setWindowTitle(PACKAGE_NAME);
        dialog->adjustSize();
        dialog->raise();
        dialog->exec();
    }
    bool ret = dialog->isOk;
    dialog->deleteLater();
    return ret;
}


<<<<<<< HEAD
void PIVXGUI::showNormalIfMinimized(bool fToggleHidden)
=======
void hemisGUI::showNormalIfMinimized(bool fToggleHidden)
>>>>>>> e92f29f73 (init)
{
    if (!clientModel)
        return;
    if (!isHidden() && !isMinimized() && !GUIUtil::isObscured(this) && fToggleHidden) {
        hide();
    } else {
        GUIUtil::bringToFront(this);
    }
}

<<<<<<< HEAD
void PIVXGUI::toggleHidden()
=======
void hemisGUI::toggleHidden()
>>>>>>> e92f29f73 (init)
{
    showNormalIfMinimized(true);
}

<<<<<<< HEAD
void PIVXGUI::detectShutdown()
=======
void hemisGUI::detectShutdown()
>>>>>>> e92f29f73 (init)
{
    if (ShutdownRequested()) {
        if (rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

<<<<<<< HEAD
void PIVXGUI::goToDashboard()
=======
void hemisGUI::goToDashboard()
>>>>>>> e92f29f73 (init)
{
    if (stackedContainer->currentWidget() != dashboard) {
        stackedContainer->setCurrentWidget(dashboard);
        topBar->showBottom();
    }
}

<<<<<<< HEAD
void PIVXGUI::goToSend()
=======
void hemisGUI::goToSend()
>>>>>>> e92f29f73 (init)
{
    showTop(sendWidget);
}

<<<<<<< HEAD
void PIVXGUI::goToAddresses()
=======
void hemisGUI::goToAddresses()
>>>>>>> e92f29f73 (init)
{
    showTop(addressesWidget);
}

<<<<<<< HEAD
void PIVXGUI::goToMasterNodes()
=======
void hemisGUI::goToMasterNodes()
>>>>>>> e92f29f73 (init)
{
    masterNodesWidget->resetCoinControl();
    showTop(masterNodesWidget);
}

<<<<<<< HEAD
void PIVXGUI::goToColdStaking()
=======
void hemisGUI::goToColdStaking()
>>>>>>> e92f29f73 (init)
{
    showTop(coldStakingWidget);
}

<<<<<<< HEAD
void PIVXGUI::goToGovernance()
=======
void hemisGUI::goToGovernance()
>>>>>>> e92f29f73 (init)
{
    showTop(governancewidget);
}

<<<<<<< HEAD
void PIVXGUI::goToSettings(){
    showTop(settingsWidget);
}

void PIVXGUI::goToSettingsInfo()
=======
void hemisGUI::goToSettings(){
    showTop(settingsWidget);
}

void hemisGUI::goToSettingsInfo()
>>>>>>> e92f29f73 (init)
{
    navMenu->selectSettings();
    settingsWidget->showInformation();
    goToSettings();
}

<<<<<<< HEAD
void PIVXGUI::goToReceive()
=======
void hemisGUI::goToReceive()
>>>>>>> e92f29f73 (init)
{
    showTop(receiveWidget);
}

<<<<<<< HEAD
void PIVXGUI::openNetworkMonitor()
=======
void hemisGUI::openNetworkMonitor()
>>>>>>> e92f29f73 (init)
{
    settingsWidget->openNetworkMonitor();
}

<<<<<<< HEAD
void PIVXGUI::showTop(QWidget* view)
=======
void hemisGUI::showTop(QWidget* view)
>>>>>>> e92f29f73 (init)
{
    if (stackedContainer->currentWidget() != view) {
        stackedContainer->setCurrentWidget(view);
        topBar->showTop();
    }
}

<<<<<<< HEAD
void PIVXGUI::changeTheme(bool isLightTheme)
=======
void hemisGUI::changeTheme(bool isLightTheme)
>>>>>>> e92f29f73 (init)
{

    QString css = GUIUtil::loadStyleSheet();
    this->setStyleSheet(css);

    // Notify
    Q_EMIT themeChanged(isLightTheme, css);

    // Update style
    updateStyle(this);
}

<<<<<<< HEAD
void PIVXGUI::resizeEvent(QResizeEvent* event)
=======
void hemisGUI::resizeEvent(QResizeEvent* event)
>>>>>>> e92f29f73 (init)
{
    // Parent..
    QMainWindow::resizeEvent(event);
    // background
    showHide(opEnabled);
    // Notify
    Q_EMIT windowResizeEvent(event);
}

<<<<<<< HEAD
bool PIVXGUI::execDialog(QDialog *dialog, int xDiv, int yDiv)
=======
bool hemisGUI::execDialog(QDialog *dialog, int xDiv, int yDiv)
>>>>>>> e92f29f73 (init)
{
    return openDialogWithOpaqueBackgroundY(dialog, this);
}

<<<<<<< HEAD
void PIVXGUI::showHide(bool show)
=======
void hemisGUI::showHide(bool show)
>>>>>>> e92f29f73 (init)
{
    if (!op) op = new QLabel(this);
    if (!show) {
        op->setVisible(false);
        opEnabled = false;
    } else {
        QColor bg("#000000");
        bg.setAlpha(200);
        if (!isLightTheme()) {
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

<<<<<<< HEAD
int PIVXGUI::getNavWidth()
=======
int hemisGUI::getNavWidth()
>>>>>>> e92f29f73 (init)
{
    return this->navMenu->width();
}

<<<<<<< HEAD
void PIVXGUI::openFAQ(SettingsFaqWidget::Section section)
=======
void hemisGUI::openFAQ(SettingsFaqWidget::Section section)
>>>>>>> e92f29f73 (init)
{
    showHide(true);
    SettingsFaqWidget* dialog = new SettingsFaqWidget(this, mnModel);
    dialog->setSection(section);
    openDialogWithOpaqueBackgroundFullScreen(dialog, this);
    dialog->deleteLater();
}


#ifdef ENABLE_WALLET
<<<<<<< HEAD
void PIVXGUI::setGovModel(GovernanceModel* govModel)
=======
void hemisGUI::setGovModel(GovernanceModel* govModel)
>>>>>>> e92f29f73 (init)
{
    if (!stackedContainer || !clientModel) return;
    governancewidget->setGovModel(govModel);
}

<<<<<<< HEAD
void PIVXGUI::setMNModel(MNModel* _mnModel)
=======
void hemisGUI::setMNModel(MNModel* _mnModel)
>>>>>>> e92f29f73 (init)
{
    if (!stackedContainer || !clientModel) return;
    mnModel = _mnModel;
    governancewidget->setMNModel(mnModel);
    masterNodesWidget->setMNModel(mnModel);
}

<<<<<<< HEAD
bool PIVXGUI::addWallet(const QString& name, WalletModel* walletModel)
=======
bool hemisGUI::addWallet(const QString& name, WalletModel* walletModel)
>>>>>>> e92f29f73 (init)
{
    // Single wallet supported for now..
    if (!stackedContainer || !clientModel || !walletModel)
        return false;

    // set the model for every view
    navMenu->setWalletModel(walletModel);
    dashboard->setWalletModel(walletModel);
    topBar->setWalletModel(walletModel);
    receiveWidget->setWalletModel(walletModel);
    sendWidget->setWalletModel(walletModel);
    addressesWidget->setWalletModel(walletModel);
    masterNodesWidget->setWalletModel(walletModel);
    coldStakingWidget->setWalletModel(walletModel);
    governancewidget->setWalletModel(walletModel);
    settingsWidget->setWalletModel(walletModel);

    // Connect actions..
<<<<<<< HEAD
    connect(walletModel, &WalletModel::message, this, &PIVXGUI::message);
    connect(masterNodesWidget, &MasterNodesWidget::message, this, &PIVXGUI::message);
    connect(coldStakingWidget, &ColdStakingWidget::message, this, &PIVXGUI::message);
    connect(topBar, &TopBar::message, this, &PIVXGUI::message);
    connect(sendWidget, &SendWidget::message,this, &PIVXGUI::message);
    connect(receiveWidget, &ReceiveWidget::message,this, &PIVXGUI::message);
    connect(addressesWidget, &AddressesWidget::message,this, &PIVXGUI::message);
    connect(governancewidget, &GovernanceWidget::message,this, &PIVXGUI::message);
    connect(settingsWidget, &SettingsWidget::message, this, &PIVXGUI::message);

    // Pass through transaction notifications
    connect(dashboard, &DashboardWidget::incomingTransaction, this, &PIVXGUI::incomingTransaction);
=======
    connect(walletModel, &WalletModel::message, this, &hemisGUI::message);
    connect(masterNodesWidget, &MasterNodesWidget::message, this, &hemisGUI::message);
    connect(coldStakingWidget, &ColdStakingWidget::message, this, &hemisGUI::message);
    connect(topBar, &TopBar::message, this, &hemisGUI::message);
    connect(sendWidget, &SendWidget::message,this, &hemisGUI::message);
    connect(receiveWidget, &ReceiveWidget::message,this, &hemisGUI::message);
    connect(addressesWidget, &AddressesWidget::message,this, &hemisGUI::message);
    connect(governancewidget, &GovernanceWidget::message,this, &hemisGUI::message);
    connect(settingsWidget, &SettingsWidget::message, this, &hemisGUI::message);

    // Pass through transaction notifications
    connect(dashboard, &DashboardWidget::incomingTransaction, this, &hemisGUI::incomingTransaction);
>>>>>>> e92f29f73 (init)

    return true;
}

<<<<<<< HEAD
bool PIVXGUI::setCurrentWallet(const QString& name)
=======
bool hemisGUI::setCurrentWallet(const QString& name)
>>>>>>> e92f29f73 (init)
{
    // Single wallet supported.
    return true;
}

<<<<<<< HEAD
void PIVXGUI::removeAllWallets()
=======
void hemisGUI::removeAllWallets()
>>>>>>> e92f29f73 (init)
{
    // Single wallet supported.
}

<<<<<<< HEAD
void PIVXGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address)
=======
void hemisGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address)
>>>>>>> e92f29f73 (init)
{
    // Only send notifications when not disabled
    if (!bdisableSystemnotifications) {
        // On new transaction, make an info balloon
        message(amount < 0 ? tr("Sent transaction") : tr("Incoming transaction"),
            tr("Date: %1\n"
               "Amount: %2\n"
               "Type: %3\n"
               "Address: %4\n")
                .arg(date)
                .arg(BitcoinUnits::formatWithUnit(unit, amount, true))
                .arg(type)
                .arg(address),
            CClientUIInterface::MSG_INFORMATION);
    }
}

#endif // ENABLE_WALLET


<<<<<<< HEAD
static bool ThreadSafeMessageBox(PIVXGUI* gui, const std::string& message, const std::string& caption, unsigned int style)
=======
static bool ThreadSafeMessageBox(hemisGUI* gui, const std::string& message, const std::string& caption, unsigned int style)
>>>>>>> e92f29f73 (init)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    std::cout << "thread safe box: " << message << std::endl;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
              modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
              Q_ARG(QString, QString::fromStdString(caption)),
              Q_ARG(QString, QString::fromStdString(message)),
              Q_ARG(unsigned int, style),
              Q_ARG(bool*, &ret));
    return ret;
}


<<<<<<< HEAD
void PIVXGUI::subscribeToCoreSignals()
=======
void hemisGUI::subscribeToCoreSignals()
>>>>>>> e92f29f73 (init)
{
    // Connect signals to client
    m_handler_message_box = interfaces::MakeHandler(uiInterface.ThreadSafeMessageBox.connect(std::bind(ThreadSafeMessageBox, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
}

<<<<<<< HEAD
void PIVXGUI::unsubscribeFromCoreSignals()
=======
void hemisGUI::unsubscribeFromCoreSignals()
>>>>>>> e92f29f73 (init)
{
    // Disconnect signals from client
    m_handler_message_box->disconnect();
}
