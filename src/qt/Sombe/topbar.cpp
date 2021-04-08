//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Sombe/topbar.h"
#include "qt/Sombe/forms/ui_topbar.h"
#include "qt/Sombe/lockunlock.h"
#include "qt/Sombe/qtutils.h"
#include "qt/Sombe/receivedialog.h"
#include "qt/Sombe/loadingdialog.h"
#include "askpassphrasedialog.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "qt/guiconstants.h"
#include "qt/guiutil.h"
#include "optionsmodel.h"
#include "qt/platformstyle.h"
#include "walletmodel.h"
#include "addresstablemodel.h"
#include "guiinterface.h"

#include "masternode-sync.h"
#include "wallet/wallet.h"

#include <QPixmap>

#define REQUEST_UPGRADE_WALLET 1

TopBar::TopBar(SombeGUI* _mainWindow, QWidget *parent) :
    PWidget(_mainWindow, parent),
    ui(new Ui::TopBar)
{
    ui->setupUi(this);

    // Set parent stylesheet
    this->setStyleSheet(_mainWindow->styleSheet());
    /* Containers */
    ui->containerTop->setContentsMargins(10, 4, 10, 10);
#ifdef Q_OS_MAC
    ui->containerTop->load("://bg-dashboard-banner");
    setCssProperty(ui->containerTop,"container-topbar-no-image");
#else
    ui->containerTop->setProperty("cssClass", "container-top");
#endif

    std::initializer_list<QWidget*> lblTitles = {ui->labelTitle1, ui->labelTitle3, ui->labelTitle4};
    setCssProperty(lblTitles, "text-title-topbar");
    QFont font;
    font.setWeight(QFont::Light);
    Q_FOREACH (QWidget* w, lblTitles) { w->setFont(font); }

    // Amount information top
    ui->widgetTopAmount->setVisible(false);
    setCssProperty({ui->labelAmountTopSBE}, "amount-small-topbar");
    setCssProperty({ui->labelAmountSBE}, "amount-topbar");
    setCssProperty({ui->labelPendingSBE, ui->labelImmatureSBE}, "amount-small-topbar");

    // Progress Sync
    progressBar = new QProgressBar(ui->layoutSync);
    progressBar->setRange(1, 10);
    progressBar->setValue(4);
    progressBar->setTextVisible(false);
    progressBar->setMaximumHeight(2);
    progressBar->setMaximumWidth(36);
    setCssProperty(progressBar, "progress-sync");
    progressBar->show();
    progressBar->raise();
    progressBar->move(0, 34);

    ui->pushButtonFAQ->setButtonClassStyle("cssClass", "btn-check-faq");
    ui->pushButtonFAQ->setButtonText(tr("FAQ"));

    ui->pushButtonHDUpgrade->setButtonClassStyle("cssClass", "btn-check-hd-upgrade");
    ui->pushButtonHDUpgrade->setButtonText(tr("Upgrade to HD Wallet"));
    ui->pushButtonHDUpgrade->setNoIconText("HD");

    ui->pushButtonConnection->setButtonClassStyle("cssClass", "btn-check-connect-inactive");
    ui->pushButtonConnection->setButtonText(tr("No Connection"));

    ui->pushButtonTor->setButtonClassStyle("cssClass", "btn-check-tor-inactive");
    ui->pushButtonTor->setButtonText(tr("Tor Disabled"));
    ui->pushButtonTor->setChecked(false);

    ui->pushButtonStack->setButtonClassStyle("cssClass", "btn-check-stack-inactive");
    ui->pushButtonStack->setButtonText(tr("Staking Disabled"));

    ui->pushButtonColdStaking->setButtonClassStyle("cssClass", "btn-check-cold-staking-inactive");
    ui->pushButtonColdStaking->setButtonText(tr("Cold Staking Disabled"));

    ui->pushButtonSync->setButtonClassStyle("cssClass", "btn-check-sync");
    ui->pushButtonSync->setButtonText(tr(" %54 Synchronizing.."));

    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-lock");

    if (isLightTheme()) {
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-light");
        ui->pushButtonTheme->setButtonText(tr("Light Theme"));
    } else {
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-dark");
        ui->pushButtonTheme->setButtonText(tr("Dark Theme"));
    }

    setCssProperty(ui->qrContainer, "container-qr");
    setCssProperty(ui->pushButtonQR, "btn-qr");

    // QR image
    QPixmap pixmap("://img-qr-test");
    ui->btnQr->setIcon(
                QIcon(pixmap.scaled(
                         70,
                         70,
                         Qt::KeepAspectRatio))
                );

    ui->pushButtonLock->setButtonText(tr("Wallet Locked "));
    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-lock");


    connect(ui->pushButtonQR, &QPushButton::clicked, this, &TopBar::onBtnReceiveClicked);
    connect(ui->btnQr, &QPushButton::clicked, this, &TopBar::onBtnReceiveClicked);
    connect(ui->pushButtonLock, &ExpandableButton::Mouse_Pressed, this, &TopBar::onBtnLockClicked);
    connect(ui->pushButtonTheme, &ExpandableButton::Mouse_Pressed, this, &TopBar::onThemeClicked);
    connect(ui->pushButtonFAQ, &ExpandableButton::Mouse_Pressed, [this](){window->openFAQ();});
    connect(ui->pushButtonColdStaking, &ExpandableButton::Mouse_Pressed, this, &TopBar::onColdStakingClicked);
    connect(ui->pushButtonSync, &ExpandableButton::Mouse_HoverLeave, this, &TopBar::refreshProgressBarSize);
    connect(ui->pushButtonSync, &ExpandableButton::Mouse_Hover, this, &TopBar::refreshProgressBarSize);
    connect(ui->pushButtonSync, &ExpandableButton::Mouse_Pressed, [this](){window->goToSettingsInfo();});
    connect(ui->pushButtonConnection, &ExpandableButton::Mouse_Pressed, [this](){window->openNetworkMonitor();});
}

void TopBar::onThemeClicked()
{
    // Store theme
    bool lightTheme = !isLightTheme();

    setTheme(lightTheme);

    if (lightTheme) {
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-light",  true);
        ui->pushButtonTheme->setButtonText(tr("Light Theme"));
    } else {
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-dark", true);
        ui->pushButtonTheme->setButtonText(tr("Dark Theme"));
    }
    updateStyle(ui->pushButtonTheme);

    Q_EMIT themeChanged(lightTheme);
}

void TopBar::onBtnLockClicked()
{
    if (walletModel) {
        if (walletModel->getEncryptionStatus() == WalletModel::Unencrypted) {
            encryptWallet();
        } else {
            if (!lockUnlockWidget) {
                lockUnlockWidget = new LockUnlock(window);
                lockUnlockWidget->setStyleSheet("margin:0px; padding:0px;");
                connect(lockUnlockWidget, &LockUnlock::Mouse_Leave, this, &TopBar::lockDropdownMouseLeave);
                connect(ui->pushButtonLock, &ExpandableButton::Mouse_HoverLeave, [this]() {
                    QMetaObject::invokeMethod(this, "lockDropdownMouseLeave", Qt::QueuedConnection);
                });
                connect(lockUnlockWidget, &LockUnlock::lockClicked ,this, &TopBar::lockDropdownClicked);
            }

            lockUnlockWidget->updateStatus(walletModel->getEncryptionStatus());
            if (ui->pushButtonLock->width() <= 40) {
                ui->pushButtonLock->setExpanded();
            }
            // Keep it open
            ui->pushButtonLock->setKeepExpanded(true);
            QMetaObject::invokeMethod(this, "openLockUnlock", Qt::QueuedConnection);
        }
    }
}

void TopBar::openLockUnlock()
{
    lockUnlockWidget->setFixedWidth(ui->pushButtonLock->width());
    lockUnlockWidget->adjustSize();

    lockUnlockWidget->move(
            ui->pushButtonLock->pos().rx() + window->getNavWidth() + 10,
            ui->pushButtonLock->y() + 36
    );

    lockUnlockWidget->raise();
    lockUnlockWidget->activateWindow();
    lockUnlockWidget->show();
}

void TopBar::openPassPhraseDialog(AskPassphraseDialog::Mode mode, AskPassphraseDialog::Context ctx)
{
    if (!walletModel)
        return;

    showHideOp(true);
    AskPassphraseDialog *dlg = new AskPassphraseDialog(mode, window, walletModel, ctx);
    dlg->adjustSize();
    openDialogWithOpaqueBackgroundY(dlg, window);

    refreshStatus();
    dlg->deleteLater();
}

void TopBar::encryptWallet()
{
    return openPassPhraseDialog(AskPassphraseDialog::Mode::Encrypt, AskPassphraseDialog::Context::Encrypt);
}

void TopBar::unlockWallet()
{
    if (!walletModel)
        return;
    // Unlock wallet when requested by wallet model (if unlocked or unlocked for staking only)
    if (walletModel->isWalletLocked(false))
        return openPassPhraseDialog(AskPassphraseDialog::Mode::Unlock, AskPassphraseDialog::Context::Unlock_Full);
}

static bool isExecuting = false;

void TopBar::lockDropdownClicked(const StateClicked& state)
{
    lockUnlockWidget->close();
    if (walletModel && !isExecuting) {
        isExecuting = true;

        switch (lockUnlockWidget->lock) {
            case 0: {
                if (walletModel->getEncryptionStatus() == WalletModel::Locked)
                    break;
                walletModel->setWalletLocked(true);
                ui->pushButtonLock->setButtonText(tr("Wallet Locked"));
                ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-lock", true);
                // Directly update the staking status icon when the wallet is manually locked here
                // so the feedback is instant (no need to wait for the polling timeout)
                setStakingStatusActive(false);
                break;
            }
            case 1: {
                if (walletModel->getEncryptionStatus() == WalletModel::Unlocked)
                    break;
                showHideOp(true);
                AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::Unlock, window, walletModel,
                                        AskPassphraseDialog::Context::ToggleLock);
                dlg->adjustSize();
                openDialogWithOpaqueBackgroundY(dlg, window);
                if (walletModel->getEncryptionStatus() == WalletModel::Unlocked) {
                    ui->pushButtonLock->setButtonText(tr("Wallet Unlocked"));
                    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-unlock", true);
                }
                dlg->deleteLater();
                break;
            }
            case 2: {
                WalletModel::EncryptionStatus status = walletModel->getEncryptionStatus();
                if (status == WalletModel::UnlockedForStaking)
                    break;

                if (status == WalletModel::Unlocked) {
                    walletModel->lockForStakingOnly();
                } else {
                    showHideOp(true);
                    AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::UnlockAnonymize,
                                                                       window, walletModel,
                                                                       AskPassphraseDialog::Context::ToggleLock);
                    dlg->adjustSize();
                    openDialogWithOpaqueBackgroundY(dlg, window);
                    dlg->deleteLater();
                }
                if (walletModel->getEncryptionStatus() == WalletModel::UnlockedForStaking) {
                    ui->pushButtonLock->setButtonText(tr("Wallet Unlocked for staking"));
                    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-staking", true);
                }
                break;
            }
        }

        ui->pushButtonLock->setKeepExpanded(false);
        ui->pushButtonLock->setSmall();
        ui->pushButtonLock->update();

        isExecuting = false;
    }
}

void TopBar::lockDropdownMouseLeave()
{
    if (lockUnlockWidget->isVisible() && !lockUnlockWidget->isHovered()) {
        lockUnlockWidget->hide();
        ui->pushButtonLock->setKeepExpanded(false);
        ui->pushButtonLock->setSmall();
        ui->pushButtonLock->update();
    }
}

void TopBar::onBtnReceiveClicked()
{
    if (walletModel) {
        QString addressStr = walletModel->getAddressTableModel()->getAddressToShow();
        if (addressStr.isNull()) {
            inform(tr("Error generating address"));
            return;
        }
        showHideOp(true);
        ReceiveDialog *receiveDialog = new ReceiveDialog(window);
        receiveDialog->updateQr(addressStr);
        if (openDialogWithOpaqueBackground(receiveDialog, window)) {
            inform(tr("Address Copied"));
        }
        receiveDialog->deleteLater();
    }
}

void TopBar::showTop()
{
    if (ui->bottom_container->isVisible()) {
        ui->bottom_container->setVisible(false);
        ui->widgetTopAmount->setVisible(true);
        this->setFixedHeight(75);
    }
}

void TopBar::showBottom()
{
    ui->widgetTopAmount->setVisible(false);
    ui->bottom_container->setVisible(true);
    this->setFixedHeight(200);
    this->adjustSize();
}

void TopBar::onColdStakingClicked()
{
    bool isColdStakingEnabled = walletModel->isColdStaking();
    ui->pushButtonColdStaking->setChecked(isColdStakingEnabled);

    bool show = (isInitializing) ? walletModel->getOptionsModel()->isColdStakingScreenEnabled() :
            walletModel->getOptionsModel()->invertColdStakingScreenStatus();
    QString className;
    QString text;

    if (isColdStakingEnabled) {
        text = "Cold Staking Active";
        className = (show) ? "btn-check-cold-staking-checked" : "btn-check-cold-staking-unchecked";
    } else if (show) {
        className = "btn-check-cold-staking";
        text = "Cold Staking Enabled";
    } else {
        className = "btn-check-cold-staking-inactive";
        text = "Cold Staking Disabled";
    }

    ui->pushButtonColdStaking->setButtonClassStyle("cssClass", className, true);
    ui->pushButtonColdStaking->setButtonText(text);
    updateStyle(ui->pushButtonColdStaking);

    Q_EMIT onShowHideColdStakingChanged(show);
}

TopBar::~TopBar()
{
    if (timerStakingIcon) {
        timerStakingIcon->stop();
    }
    delete ui;
}

void TopBar::loadClientModel()
{
    if (clientModel) {
        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, &ClientModel::numConnectionsChanged, this, &TopBar::setNumConnections);

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, &ClientModel::numBlocksChanged, this, &TopBar::setNumBlocks);

        timerStakingIcon = new QTimer(ui->pushButtonStack);
        connect(timerStakingIcon, &QTimer::timeout, this, &TopBar::updateStakingStatus);
        timerStakingIcon->start(50000);
        updateStakingStatus();
    }
}

void TopBar::setStakingStatusActive(bool fActive)
{
    if (ui->pushButtonStack->isChecked() != fActive) {
        ui->pushButtonStack->setButtonText(fActive ? tr("Staking active") : tr("Staking not active"));
        ui->pushButtonStack->setChecked(fActive);
        ui->pushButtonStack->setButtonClassStyle("cssClass", (fActive ?
                                                                "btn-check-stack" :
                                                                "btn-check-stack-inactive"), true);
    }
}
void TopBar::updateStakingStatus()
{
    setStakingStatusActive(walletModel &&
                           !walletModel->isWalletLocked() &&
                           walletModel->isStakingStatusActive());

    // Taking advantage of this timer to update Tor status if needed.
    updateTorIcon();
}

void TopBar::setNumConnections(int count)
{
    if (count > 0) {
        if (!ui->pushButtonConnection->isChecked()) {
            ui->pushButtonConnection->setChecked(true);
            ui->pushButtonConnection->setButtonClassStyle("cssClass", "btn-check-connect", true);
        }
    } else {
        if (ui->pushButtonConnection->isChecked()) {
            ui->pushButtonConnection->setChecked(false);
            ui->pushButtonConnection->setButtonClassStyle("cssClass", "btn-check-connect-inactive", true);
        }
    }

    ui->pushButtonConnection->setButtonText(tr("%n active connection(s)", "", count));
}

void TopBar::setNumBlocks(int count)
{
    if (!clientModel)
        return;

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    std::string text = "";
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            text = "Synchronizing..";
            break;
        case BLOCK_SOURCE_DISK:
            text = "Importing blocks from disk..";
            break;
        case BLOCK_SOURCE_REINDEX:
            text = "Reindexing blocks on disk..";
            break;
        case BLOCK_SOURCE_NONE:
            // Case: not Importing, not Reindexing and no network connection
            text = "No block source available..";
            ui->pushButtonSync->setChecked(false);
            break;
    }

    bool needState = true;
    if (masternodeSync.IsBlockchainSynced()) {
        // chain synced
        Q_EMIT walletSynced(true);
        if (masternodeSync.IsSynced()) {
            // Node synced
            ui->pushButtonSync->setButtonText(tr("Synchronized - Block: %1").arg(QString::number(count)));
            progressBar->setRange(0,100);
            progressBar->setValue(100);
            return;
        } else {

            // TODO: Show out of sync warning
            int nAttempt = masternodeSync.RequestedMasternodeAttempt < MASTERNODE_SYNC_THRESHOLD ?
                       masternodeSync.RequestedMasternodeAttempt + 1 :
                       MASTERNODE_SYNC_THRESHOLD;
            int progress = nAttempt + (masternodeSync.RequestedMasternodeAssets - 1) * MASTERNODE_SYNC_THRESHOLD;
            if (progress >= 0) {
                // todo: MN progress..
                text = strprintf("%s - Block: %d", masternodeSync.GetSyncStatus(), count);
                //progressBar->setMaximum(4 * MASTERNODE_SYNC_THRESHOLD);
                //progressBar->setValue(progress);
                needState = false;
            }
        }
    } else {
        Q_EMIT walletSynced(false);
    }

    if (needState) {
        // Represent time from last generated block in human readable text
        QDateTime lastBlockDate = clientModel->getLastBlockDate();
        QDateTime currentDate = QDateTime::currentDateTime();
        int secs = lastBlockDate.secsTo(currentDate);

        QString timeBehindText;
        const int HOUR_IN_SECONDS = 60 * 60;
        const int DAY_IN_SECONDS = 24 * 60 * 60;
        const int WEEK_IN_SECONDS = 7 * 24 * 60 * 60;
        const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
        if (secs < 2 * DAY_IN_SECONDS) {
            timeBehindText = tr("%n hour(s)", "", secs / HOUR_IN_SECONDS);
        } else if (secs < 2 * WEEK_IN_SECONDS) {
            timeBehindText = tr("%n day(s)", "", secs / DAY_IN_SECONDS);
        } else if (secs < YEAR_IN_SECONDS) {
            timeBehindText = tr("%n week(s)", "", secs / WEEK_IN_SECONDS);
        } else {
            int years = secs / YEAR_IN_SECONDS;
            int remainder = secs % YEAR_IN_SECONDS;
            timeBehindText = tr("%1 and %2").arg(tr("%n year(s)", "", years)).arg(
                    tr("%n week(s)", "", remainder / WEEK_IN_SECONDS));
        }
        QString timeBehind(" behind. Scanning block ");
        QString str = timeBehindText + timeBehind + QString::number(count);
        text = str.toStdString();

        progressBar->setMaximum(1000000000);
        progressBar->setValue(clientModel->getVerificationProgress() * 1000000000.0 + 0.5);
    }

    if (text.empty()) {
        text = "No block source available..";
    }

    ui->pushButtonSync->setButtonText(tr(text.data()));
}

void TopBar::showUpgradeDialog()
{
    QString title = tr("Wallet Upgrade");
    if (ask(title,
            tr("Upgrading to HD wallet will improve\nthe wallet's reliability and security.\n\n\n"
                    "NOTE: after the upgrade, a new \nbackup will be created.\n"))) {

        std::unique_ptr<WalletModel::UnlockContext> pctx = MakeUnique<WalletModel::UnlockContext>(walletModel->requestUnlock());
        if (!pctx->isValid()) {
            warn(tr("Upgrade Wallet"), tr("Wallet unlock cancelled"));
            return;
        }
        // Action performed on a separate thread, it's locking cs_main and cs_wallet.
        LoadingDialog *dialog = new LoadingDialog(window);
        dialog->execute(this, REQUEST_UPGRADE_WALLET, std::move(pctx));
        openDialogWithOpaqueBackgroundFullScreen(dialog, window);
    }
}

void TopBar::loadWalletModel()
{
    // Upgrade wallet.
    if (walletModel->isHDEnabled()) {
        ui->pushButtonHDUpgrade->setVisible(false);
    } else {
        connect(ui->pushButtonHDUpgrade, &ExpandableButton::Mouse_Pressed, this, &TopBar::showUpgradeDialog);

        // Upgrade wallet timer, only once. launched 4 seconds after the wallet started.
        QTimer::singleShot(4000, [this](){
            showUpgradeDialog();
        });
    }

    connect(walletModel, &WalletModel::balanceChanged, this, &TopBar::updateBalances);
    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &TopBar::updateDisplayUnit);
    connect(walletModel, &WalletModel::encryptionStatusChanged, this, &TopBar::refreshStatus);
    // Ask for passphrase if needed
    connect(walletModel, &WalletModel::requireUnlock, this, &TopBar::unlockWallet);
    // update the display unit, to not use the default ("Sombe")
    updateDisplayUnit();

    refreshStatus();
    onColdStakingClicked();

    isInitializing = false;
}

void TopBar::updateTorIcon()
{
    std::string ip_port;
    bool torEnabled = clientModel->getTorInfo(ip_port);

    if (torEnabled) {
        if (!ui->pushButtonTor->isChecked()) {
            ui->pushButtonTor->setChecked(true);
            ui->pushButtonTor->setButtonClassStyle("cssClass", "btn-check-tor", true);
        }
        QString ip_port_q = QString::fromStdString(ip_port);
        ui->pushButtonTor->setButtonText(tr("Tor Active: %1").arg(ip_port_q));
    } else {
        if (ui->pushButtonTor->isChecked()) {
            ui->pushButtonTor->setChecked(false);
            ui->pushButtonTor->setButtonClassStyle("cssClass", "btn-check-tor-inactive", true);
            ui->pushButtonTor->setButtonText(tr("Tor Disabled"));
        }
    }
}

void TopBar::refreshStatus()
{
    // Check lock status
    if (!this->walletModel)
        return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    switch (encStatus) {
        case WalletModel::EncryptionStatus::Unencrypted:
            ui->pushButtonLock->setButtonText(tr("Wallet Unencrypted"));
            ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-unlock", true);
            break;
        case WalletModel::EncryptionStatus::Locked:
            ui->pushButtonLock->setButtonText(tr("Wallet Locked"));
            ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-lock", true);
            break;
        case WalletModel::EncryptionStatus::UnlockedForStaking:
            ui->pushButtonLock->setButtonText(tr("Wallet Unlocked for staking"));
            ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-staking", true);
            break;
        case WalletModel::EncryptionStatus::Unlocked:
            ui->pushButtonLock->setButtonText(tr("Wallet Unlocked"));
            ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-unlock", true);
            break;
    }
    updateStyle(ui->pushButtonLock);
}

void TopBar::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        int displayUnitPrev = nDisplayUnit;
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        if (displayUnitPrev != nDisplayUnit)
            updateBalances(walletModel->getBalance(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(),
                           walletModel->getWatchBalance(), walletModel->getWatchUnconfirmedBalance(), walletModel->getWatchImmatureBalance(),
                           walletModel->getDelegatedBalance(), walletModel->getColdStakedBalance());
    }
}

void TopBar::updateBalances(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                            const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance,
                            const CAmount& delegatedBalance, const CAmount& coldStakedBalance)
{
    // Locked balance. //TODO move this to the signal properly in the future..
    CAmount nLockedBalance = 0;
    if (walletModel) {
        nLockedBalance = walletModel->getLockedBalance();
    }
    ui->labelTitle1->setText(nLockedBalance > 0 ? tr("Available (Locked included)") : tr("Available"));

    // SBE Total
    QString totalSBE = GUIUtil::formatBalance(balance, nDisplayUnit);

    // SBE
    // Top
    ui->labelAmountTopSBE->setText(totalSBE);
    // Expanded
    ui->labelAmountSBE->setText(totalSBE);
    ui->labelPendingSBE->setText(GUIUtil::formatBalance(unconfirmedBalance, nDisplayUnit));
    ui->labelImmatureSBE->setText(GUIUtil::formatBalance(immatureBalance, nDisplayUnit));
}

void TopBar::resizeEvent(QResizeEvent *event)
{
    if (lockUnlockWidget && lockUnlockWidget->isVisible()) lockDropdownMouseLeave();
    QWidget::resizeEvent(event);
}

void TopBar::refreshProgressBarSize()
{
    QMetaObject::invokeMethod(this, "expandSync", Qt::QueuedConnection);
}

void TopBar::expandSync()
{
    if (progressBar) {
        progressBar->setMaximumWidth(ui->pushButtonSync->maximumWidth());
        progressBar->setFixedWidth(ui->pushButtonSync->width());
        progressBar->setMinimumWidth(ui->pushButtonSync->width() - 2);
    }
}

void TopBar::updateHDState(const bool& upgraded, const QString& upgradeError)
{
    if (upgraded) {
        ui->pushButtonHDUpgrade->setVisible(false);
        if (ask("HD Upgrade Complete", tr("The wallet has been successfully upgraded to HD.") + "\n" +
                tr("It is advised to make a backup.") + "\n\n" + tr("Do you wish to backup now?") + "\n\n")) {
            // backup wallet
            QString filename = GUIUtil::getSaveFileName(this,
                                                tr("Backup Wallet"), QString(),
                                                tr("Wallet Data (*.dat)"), NULL);
            if (!filename.isEmpty()) {
                inform(walletModel->backupWallet(filename) ? tr("Backup created") : tr("Backup creation failed"));
            } else {
                warn(tr("Backup creation failed"), tr("no file selected"));
            }
        } else {
            inform(tr("Wallet upgraded successfully, but no backup created.") + "\n" +
                    tr("WARNING: remember to make a copy of your wallet.dat file!"));
        }
    } else {
        warn(tr("Upgrade Wallet Error"), upgradeError);
    }
}

void TopBar::run(int type)
{
    if (type == REQUEST_UPGRADE_WALLET) {
        std::string upgradeError;
        bool ret = this->walletModel->upgradeWallet(upgradeError);
        QMetaObject::invokeMethod(this,
                "updateHDState",
                Qt::QueuedConnection,
                Q_ARG(bool, ret),
                Q_ARG(QString, QString::fromStdString(upgradeError))
        );
    }
}

void TopBar::onError(QString error, int type)
{
    if (type == REQUEST_UPGRADE_WALLET) {
        warn(tr("Upgrade Wallet Error"), error);
    }
}
