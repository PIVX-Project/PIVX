// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/topbar.h"
#include "qt/pivx/forms/ui_topbar.h"
#include <QPixmap>
#include "qt/pivx/lockunlock.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/receivedialog.h"
#include "askpassphrasedialog.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "qt/guiconstants.h"
#include "qt/guiutil.h"
#include "optionsmodel.h"
#include "qt/platformstyle.h"
#include "wallet/wallet.h"
#include "walletmodel.h"
#include "addresstablemodel.h"
#include "guiinterface.h"


TopBar::TopBar(PIVXGUI* _mainWindow, QWidget *parent) :
    PWidget(_mainWindow, parent),
    ui(new Ui::TopBar)
{
    ui->setupUi(this);

    // Set parent stylesheet
    this->setStyleSheet(_mainWindow->styleSheet());

    /* Containers */
    ui->containerTop->setProperty("cssClass", "container-top");
    ui->containerTop->setContentsMargins(10,4,10,10);

    std::initializer_list<QWidget*> lblTitles = {ui->labelTitle1, ui->labelTitle2, ui->labelTitle3, ui->labelTitle4, ui->labelTitle5, ui->labelTitle6};
    setCssProperty(lblTitles, "text-title-topbar");
    QFont font;
    font.setWeight(QFont::Light);
    foreach (QWidget* w, lblTitles) { w->setFont(font); }

    // Amount information top
    ui->widgetTopAmount->setVisible(false);
    setCssProperty({ui->labelAmountTopPiv, ui->labelAmountTopzPiv}, "amount-small-topbar");
    setCssProperty({ui->labelAmountPiv, ui->labelAmountzPiv}, "amount-topbar");
    setCssProperty({ui->labelPendingPiv, ui->labelPendingzPiv, ui->labelImmaturePiv, ui->labelImmaturezPiv}, "amount-small-topbar");

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

    // New button
    ui->pushButtonFAQ->setButtonClassStyle("cssClass", "btn-check-faq");
    ui->pushButtonFAQ->setButtonText("FAQ");

    ui->pushButtonConnection->setButtonClassStyle("cssClass", "btn-check-connect-inactive");
    ui->pushButtonConnection->setButtonText("No Connection");

    ui->pushButtonStack->setButtonClassStyle("cssClass", "btn-check-stack-inactive");
    ui->pushButtonStack->setButtonText("Staking Disabled");

    ui->pushButtonMint->setButtonClassStyle("cssClass", "btn-check-mint-inactive");
    ui->pushButtonMint->setButtonText("Automint Enabled");
    ui->pushButtonMint->setVisible(false);

    ui->pushButtonSync->setButtonClassStyle("cssClass", "btn-check-sync");
    ui->pushButtonSync->setButtonText(" %54 Synchronizing..");

    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-lock");

    if(isLightTheme()){
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-light");
        ui->pushButtonTheme->setButtonText("Light Theme");
    }else{
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-dark");
        ui->pushButtonTheme->setButtonText("Dark Theme");
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

    ui->pushButtonLock->setButtonText("Wallet Locked  ");
    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-lock");


    connect(ui->pushButtonQR, SIGNAL(clicked()), this, SLOT(onBtnReceiveClicked()));
    connect(ui->btnQr, SIGNAL(clicked()), this, SLOT(onBtnReceiveClicked()));
    connect(ui->pushButtonLock, SIGNAL(Mouse_Pressed()), this, SLOT(onBtnLockClicked()));
    connect(ui->pushButtonTheme, SIGNAL(Mouse_Pressed()), this, SLOT(onThemeClicked()));
    connect(ui->pushButtonFAQ, SIGNAL(Mouse_Pressed()), _mainWindow, SLOT(openFAQ()));
}

void TopBar::onThemeClicked(){
    // Store theme
    bool lightTheme = !isLightTheme();

    setTheme(lightTheme);

    if(lightTheme){
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-light",  true);
        ui->pushButtonTheme->setButtonText("Light Theme");
        updateStyle(ui->pushButtonTheme);
    }else{
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-dark", true);
        ui->pushButtonTheme->setButtonText("Dark Theme");
        updateStyle(ui->pushButtonTheme);
    }

    emit themeChanged(lightTheme);
}


void TopBar::onBtnLockClicked(){
    if(walletModel) {
        if (walletModel->getEncryptionStatus() == WalletModel::Unencrypted) {
            encryptWallet();
        } else {
            if (!lockUnlockWidget) {
                lockUnlockWidget = new LockUnlock(window);
                lockUnlockWidget->setStyleSheet("margin:0px; padding:0px;");
                connect(lockUnlockWidget, SIGNAL(Mouse_Leave()), this, SLOT(lockDropdownMouseLeave()));
                connect(ui->pushButtonLock, &ExpandableButton::Mouse_HoverLeave, [this](){
                    QMetaObject::invokeMethod(this, "lockDropdownMouseLeave", Qt::QueuedConnection);
                }); //, SLOT(lockDropdownMouseLeave()));
                connect(lockUnlockWidget, SIGNAL(lockClicked(
                const StateClicked&)),this, SLOT(lockDropdownClicked(
                const StateClicked&)));
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

void TopBar::openLockUnlock(){
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

void TopBar::encryptWallet() {
    if (!walletModel)
        return;

    showHideOp(true);
    AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::Encrypt, window,
                            walletModel, AskPassphraseDialog::Context::Encrypt);
    dlg->adjustSize();
    openDialogWithOpaqueBackgroundY(dlg, window);

    refreshStatus();
    dlg->deleteLater();
}

static bool isExecuting = false;
void TopBar::lockDropdownClicked(const StateClicked& state){
    lockUnlockWidget->close();
    if(walletModel && !isExecuting) {
        isExecuting = true;

        switch (lockUnlockWidget->lock) {
            case 0: {
                if (walletModel->getEncryptionStatus() == WalletModel::Locked)
                    break;
                walletModel->setWalletLocked(true);
                ui->pushButtonLock->setButtonText("Wallet Locked");
                ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-lock", true);
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
                if (this->walletModel->getEncryptionStatus() == WalletModel::Unlocked) {
                    ui->pushButtonLock->setButtonText("Wallet Unlocked");
                    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-unlock", true);
                }
                dlg->deleteLater();
                break;
            }
            case 2: {
                if (walletModel->getEncryptionStatus() == WalletModel::UnlockedForAnonymizationOnly)
                    break;
                showHideOp(true);
                AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::UnlockAnonymize, window, walletModel,
                                        AskPassphraseDialog::Context::ToggleLock);
                dlg->adjustSize();
                openDialogWithOpaqueBackgroundY(dlg, window);
                if (this->walletModel->getEncryptionStatus() == WalletModel::UnlockedForAnonymizationOnly) {
                    ui->pushButtonLock->setButtonText(tr("Wallet Unlocked for staking"));
                    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-staking", true);
                }
                dlg->deleteLater();
                break;
            }
        }

        ui->pushButtonLock->setKeepExpanded(false);
        ui->pushButtonLock->setSmall();
        ui->pushButtonLock->update();

        isExecuting = false;
    }
}

void TopBar::lockDropdownMouseLeave(){
    if (lockUnlockWidget->isVisible() && !lockUnlockWidget->isHovered()) {
        lockUnlockWidget->hide();
        ui->pushButtonLock->setKeepExpanded(false);
        ui->pushButtonLock->setSmall();
        ui->pushButtonLock->update();
    }
}

void TopBar::onBtnReceiveClicked(){
    if(walletModel) {
        showHideOp(true);
        ReceiveDialog *receiveDialog = new ReceiveDialog(window);

        receiveDialog->updateQr(walletModel->getAddressTableModel()->getLastUnusedAddress());
        if (openDialogWithOpaqueBackground(receiveDialog, window)) {
            inform(tr("Address Copied"));
        }
        receiveDialog->deleteLater();
    }
}

void TopBar::showTop(){
    if(ui->bottom_container->isVisible()){
        ui->bottom_container->setVisible(false);
        ui->widgetTopAmount->setVisible(true);
        this->setFixedHeight(75);
    }
}

void TopBar::showBottom(){
    ui->widgetTopAmount->setVisible(false);
    ui->bottom_container->setVisible(true);
    this->setFixedHeight(200);
    this->adjustSize();
}

TopBar::~TopBar(){
    if(timerStakingIcon){
        timerStakingIcon->stop();
    }
    delete ui;
}

void TopBar::loadClientModel(){
    if(clientModel){
        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));

        timerStakingIcon = new QTimer(ui->pushButtonStack);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingStatus()));
        timerStakingIcon->start(50000);
        updateStakingStatus();
    }
}

void TopBar::updateAutoMintStatus(){
    ui->pushButtonMint->setButtonText(fEnableZeromint ? tr("Automint enabled") : tr("Automint disabled"));
    ui->pushButtonMint->setChecked(fEnableZeromint);
}

void TopBar::updateStakingStatus(){
    if (nLastCoinStakeSearchInterval) {
        if (!ui->pushButtonStack->isChecked()) {
            ui->pushButtonStack->setButtonText(tr("Staking active"));
            ui->pushButtonStack->setChecked(true);
            ui->pushButtonStack->setButtonClassStyle("cssClass", "btn-check-stack", true);
        }
    }else{
        if (ui->pushButtonStack->isChecked()) {
            ui->pushButtonStack->setButtonText(tr("Staking not active"));
            ui->pushButtonStack->setChecked(false);
            ui->pushButtonStack->setButtonClassStyle("cssClass", "btn-check-stack-inactive", true);
        }
    }
}

void TopBar::setNumConnections(int count) {
    if(count > 0){
        if(!ui->pushButtonConnection->isChecked()) {
            ui->pushButtonConnection->setChecked(true);
            ui->pushButtonConnection->setButtonClassStyle("cssClass", "btn-check-connect", true);
        }
    }else{
        if(ui->pushButtonConnection->isChecked()) {
            ui->pushButtonConnection->setChecked(false);
            ui->pushButtonConnection->setButtonClassStyle("cssClass", "btn-check-connect-inactive", true);
        }
    }

    ui->pushButtonConnection->setButtonText(tr("%n active connection(s)", "", count));
}

void TopBar::setNumBlocks(int count) {
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
        emit walletSynced(true);
        if (masternodeSync.IsSynced()) {
            // Node synced
            // TODO: Set synced icon to pushButtonSync here..
            ui->pushButtonSync->setButtonText(tr("Synchronized"));
            progressBar->setRange(0,100);
            progressBar->setValue(100);
            return;
        }else{

            // TODO: Show out of sync warning
            int nAttempt = masternodeSync.RequestedMasternodeAttempt < MASTERNODE_SYNC_THRESHOLD ?
                       masternodeSync.RequestedMasternodeAttempt + 1 :
                       MASTERNODE_SYNC_THRESHOLD;
            int progress = nAttempt + (masternodeSync.RequestedMasternodeAssets - 1) * MASTERNODE_SYNC_THRESHOLD;
            if(progress >= 0){
                // todo: MN progress..
                text = std::string("Synchronizing additional data..");//: %p%", progress);
                //progressBar->setMaximum(4 * MASTERNODE_SYNC_THRESHOLD);
                //progressBar->setValue(progress);
                needState = false;
            }
        }
    } else {
        emit walletSynced(false);
    }

    if(needState) {
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

    if(text.empty()){
        text = "No block source available..";
    }

    ui->pushButtonSync->setButtonText(tr(text.data()));
}

void TopBar::loadWalletModel(){
    connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this,
            SLOT(updateBalances(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    connect(walletModel, &WalletModel::encryptionStatusChanged, this, &TopBar::refreshStatus);

    // update the display unit, to not use the default ("PIVX")
    updateDisplayUnit();

    refreshStatus();
}

void TopBar::refreshStatus(){
    // Check lock status
    if (!this->walletModel)
        return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    switch (encStatus){
        case WalletModel::EncryptionStatus::Unencrypted:
            ui->pushButtonLock->setButtonText("Wallet Unencrypted");
            ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-unlock", true);
            break;
        case WalletModel::EncryptionStatus::Locked:
            ui->pushButtonLock->setButtonText("Wallet Locked");
            ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-lock", true);
            break;
        case WalletModel::EncryptionStatus::UnlockedForAnonymizationOnly:
            ui->pushButtonLock->setButtonText("Wallet Unlocked for staking");
            ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-staking", true);
            break;
        case WalletModel::EncryptionStatus::Unlocked:
            ui->pushButtonLock->setButtonText("Wallet Unlocked");
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
                           walletModel->getZerocoinBalance(), walletModel->getUnconfirmedZerocoinBalance(), walletModel->getImmatureZerocoinBalance(),
                           walletModel->getWatchBalance(), walletModel->getWatchUnconfirmedBalance(), walletModel->getWatchImmatureBalance());
    }
}

void TopBar::updateBalances(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                            const CAmount& zerocoinBalance, const CAmount& unconfirmedZerocoinBalance, const CAmount& immatureZerocoinBalance,
                            const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance){

    CAmount nLockedBalance = 0;
    if (!walletModel) {
        nLockedBalance = walletModel->getLockedBalance();
    }

    // PIV Balance
    //CAmount nTotalBalance = balance + unconfirmedBalance;
    CAmount pivAvailableBalance = balance - immatureBalance - nLockedBalance;

    // zPIV Balance
    CAmount matureZerocoinBalance = zerocoinBalance - unconfirmedZerocoinBalance - immatureZerocoinBalance;

    // Set
    QString totalPiv = GUIUtil::formatBalance(pivAvailableBalance, nDisplayUnit);
    QString totalzPiv = GUIUtil::formatBalance(matureZerocoinBalance, nDisplayUnit, true);
    // Top
    ui->labelAmountTopPiv->setText(totalPiv);
    ui->labelAmountTopzPiv->setText(totalzPiv);

    // Expanded
    ui->labelAmountPiv->setText(totalPiv);
    ui->labelAmountzPiv->setText(totalzPiv);

    ui->labelPendingPiv->setText(GUIUtil::formatBalance(unconfirmedBalance, nDisplayUnit));
    ui->labelPendingzPiv->setText(GUIUtil::formatBalance(unconfirmedZerocoinBalance, nDisplayUnit, true));

    ui->labelImmaturePiv->setText(GUIUtil::formatBalance(immatureBalance, nDisplayUnit));
    ui->labelImmaturezPiv->setText(GUIUtil::formatBalance(immatureZerocoinBalance, nDisplayUnit, true));
}

void TopBar::resizeEvent(QResizeEvent *event){
    if (lockUnlockWidget && lockUnlockWidget->isVisible()) lockDropdownMouseLeave();
    QWidget::resizeEvent(event);
}