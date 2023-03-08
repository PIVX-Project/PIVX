// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/pivx-config.h"
#endif

#include "qt/pivx/splash.h"
#include "qt/pivx/forms/ui_splash.h"
#include "QFile"

#include "interfaces/handler.h"
#include "guiinterface.h"
#include "networkstyle.h"
#include "shutdown.h"
#include "util/system.h"
#include "version.h"
#include "guiutil.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <QCloseEvent>
#include <QDesktopWidget>
#include <QScreen>

#include <iostream>

Splash::Splash(const NetworkStyle* networkStyle) :
    QWidget(nullptr), ui(new Ui::Splash)
{
    ui->setupUi(this);
    QString titleText = PACKAGE_NAME;
    QString titleAddText = networkStyle->getTitleAddText();
    setWindowTitle(titleText + " " + titleAddText);

    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setAttribute( Qt::WA_TranslucentBackground, true );
    ui->progressBar->setAttribute( Qt::WA_TranslucentBackground, true );

    ui->progressBar->setTextVisible(false);
    ui->progressBar->setMaximum(0);
    ui->progressBar->setMinimum(0);
    ui->progressBar->setProperty("cssClass", "progress-splash");
    ui->frame->setProperty("cssClass", "container-splash");
    ui->layoutProgress->setProperty("cssClass", "bg-progress");
    ui->imgLogo->setProperty("cssClass", "img-splash-logo");
    ui->lblVersion->setText(QString("v") + QString::fromStdString(FormatVersionFriendly()));

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), size());
    resize(r.size());
    setFixedSize(r.size());
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());

    subscribeToCoreSignals();
}

Splash::~Splash(){
    unsubscribeFromCoreSignals();
    delete ui;
}

void Splash::slotFinish(QWidget* mainWin){
    Q_UNUSED(mainWin);
    hide();
}

static void InitMessage(Splash* splash, const std::string& message){
    QMetaObject::invokeMethod(splash, "showMessage",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(message)),
                              Q_ARG(int, Qt::AlignBottom | Qt::AlignHCenter),
                              Q_ARG(QColor, QColor(100, 100, 100)));
}

static void ShowProgress(Splash* splash, const std::string& title, int nProgress){
    InitMessage(splash, title + strprintf("%d", nProgress) + "%");
}

#ifdef ENABLE_WALLET
std::unique_ptr<interfaces::Handler> m_handler_show_progress_wallet;
static void ConnectWallet(Splash* splash, CWallet* wallet){
    m_handler_show_progress_wallet = interfaces::MakeHandler(wallet->ShowProgress.connect(std::bind(ShowProgress, splash, std::placeholders::_1, std::placeholders::_2)));
}
#endif

void Splash::subscribeToCoreSignals(){
    // Connect signals to client
    m_handler_init_message = interfaces::MakeHandler(uiInterface.InitMessage.connect(std::bind(InitMessage, this, std::placeholders::_1)));
    m_handler_show_progress = interfaces::MakeHandler(uiInterface.ShowProgress.connect(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2)));
#ifdef ENABLE_WALLET
    m_handler_load_wallet = interfaces::MakeHandler(uiInterface.LoadWallet.connect(std::bind(ConnectWallet, this, std::placeholders::_1)));
#endif
}

void Splash::unsubscribeFromCoreSignals(){
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
#ifdef ENABLE_WALLET
    m_handler_load_wallet->disconnect();
    if (m_handler_show_progress_wallet) m_handler_show_progress_wallet->disconnect();
#endif
}

void Splash::showMessage(const QString& message, int alignment, const QColor& color){
    ui->lblMessage->setText(message);
}

void Splash::closeEvent(QCloseEvent* event){
    StartShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
