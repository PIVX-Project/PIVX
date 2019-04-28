#include "qt/pivx/topbar.h"
#include "qt/pivx/forms/ui_topbar.h"
#include <QPixmap>
#include <QFile>
#include "qt/pivx/lockunlock.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/receivedialog.h"
#include "qt/pivx/walletpassworddialog.h"
#include "askpassphrasedialog.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "qt/guiconstants.h"
#include "qt/guiutil.h"
#include "optionsmodel.h"
#include "qt/platformstyle.h"
#include "wallet.h"
#include "walletmodel.h"
#include "addresstablemodel.h"
#include "ui_interface.h"


TopBar::TopBar(PIVXGUI* _mainWindow, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TopBar),
    mainWindow(_mainWindow)
{
    ui->setupUi(this);

    // Set parent stylesheet
    this->setStyleSheet(_mainWindow->styleSheet());

    /* Containers */
    ui->containerTop->setProperty("cssClass", "container-top");
    ui->containerTop->setContentsMargins(10,4,10,10);

    QFont font;
    font.setWeight(QFont::Light);


    ui->labelTitle1->setProperty("cssClass", "text-title-topbar");
    ui->labelTitle1->setFont(font);
    ui->labelTitle2->setProperty("cssClass", "text-title-topbar");
    ui->labelTitle1->setFont(font);
    ui->labelTitle3->setProperty("cssClass", "text-title-topbar");
    ui->labelTitle1->setFont(font);
    ui->labelTitle4->setProperty("cssClass", "text-title-topbar");
    ui->labelTitle1->setFont(font);
    ui->labelTitle5->setProperty("cssClass", "text-title-topbar");
    ui->labelTitle1->setFont(font);
    ui->labelTitle6->setProperty("cssClass", "text-title-topbar");
    ui->labelTitle1->setFont(font);

    // Amount information top

    ui->widgetTopAmount->setVisible(false);
    ui->labelAmountTopPiv->setProperty("cssClass", "amount-small-topbar");
    ui->labelAmountTopzPiv->setProperty("cssClass", "amount-small-topbar");

    ui->labelAmountPiv->setProperty("cssClass", "amount-topbar");
    ui->labelAmountzPiv->setProperty("cssClass", "amount-topbar");

    ui->labelPendingPiv->setProperty("cssClass", "amount-small-topbar");
    ui->labelPendingzPiv->setProperty("cssClass", "amount-small-topbar");

    ui->labelImmaturePiv->setProperty("cssClass", "amount-small-topbar");
    ui->labelImmaturezPiv->setProperty("cssClass", "amount-small-topbar");


    //ui->pushButtonSync->setProperty("cssClass", "sync-status");

    // Buttons


    // New button

    ui->pushButtonPeers->setButtonClassStyle("cssClass", "btn-check-peers");
    ui->pushButtonPeers->setButtonText("No Online Peers");

    ui->pushButtonConnection->setButtonClassStyle("cssClass", "btn-check-connect-inactive");
    ui->pushButtonConnection->setButtonText("No Connection");

    ui->pushButtonStack->setButtonClassStyle("cssClass", "btn-check-stack-inactive");
    ui->pushButtonStack->setButtonText("Staking Disabled");

    ui->pushButtonMint->setButtonClassStyle("cssClass", "btn-check-mint");
    ui->pushButtonMint->setButtonText("Automint Enabled");

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


    ui->qrContainer->setProperty("cssClass", "container-qr");
    ui->pushButtonQR->setProperty("cssClass", "btn-qr");

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
    //connect(ui->pushButtonLock, SIGNAL(Mouse_Hover()), this, SLOT(onBtnLockHover()));
    //connect(ui->pushButtonLock, SIGNAL(Mouse_HoverLeave()), this, SLOT(onBtnLockHoverLeave()));

    connect(ui->pushButtonTheme, SIGNAL(Mouse_Pressed()), this, SLOT(onThemeClicked()));
}

void TopBar::onThemeClicked(){
    // Store theme
    bool lightTheme = !ui->pushButtonTheme->isChecked();
    setTheme(lightTheme);
    // TODO: Complete me..
    //mainWindow->changeTheme(lightTheme);

    if(lightTheme){
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-light",  true);
        ui->pushButtonTheme->setButtonText("Light Theme");
        updateStyle(ui->pushButtonTheme);
    }else{
        ui->pushButtonTheme->setButtonClassStyle("cssClass", "btn-check-theme-dark", true);
        ui->pushButtonTheme->setButtonText("Dark Theme");
        updateStyle(ui->pushButtonTheme);
    }
}


void TopBar::onBtnLockClicked(){
    if(walletModel) {
        if (walletModel->getEncryptionStatus() == WalletModel::Unencrypted) {
            // Encrypt dialog
            encryptWallet();
        } else {
            if (!lockUnlockWidget) {
                lockUnlockWidget = new LockUnlock(this->mainWindow);
                connect(lockUnlockWidget, SIGNAL(Mouse_Leave()), this, SLOT(lockDropdownMouseLeave()));
                connect(lockUnlockWidget, SIGNAL(lockClicked(const StateClicked&)),this, SLOT(lockDropdownClicked(const StateClicked&)));
            }

            lockUnlockWidget->updateStatus(walletModel->getEncryptionStatus());
            lockUnlockWidget->setFixedWidth(ui->pushButtonLock->width());
            lockUnlockWidget->adjustSize();

            lockUnlockWidget->move(
                    ui->pushButtonLock->pos().rx() + this->mainWindow->getNavWidth() + 10,
                    ui->pushButtonLock->y() + 36
            );

            lockUnlockWidget->setStyleSheet("margin:0px; padding:0px;");

            lockUnlockWidget->raise();
            lockUnlockWidget->activateWindow();
            lockUnlockWidget->show();

            // Keep open
            ui->pushButtonLock->setKeepExpanded(true);
        }
    }
}


void TopBar::encryptWallet() {
    if (!walletModel)
        return;

    mainWindow->showHide(true);
    AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::Encrypt, mainWindow,
                            walletModel, AskPassphraseDialog::Context::Encrypt);
    dlg->adjustSize();
    openDialogWithOpaqueBackgroundY(dlg, mainWindow);

    refreshStatus();
}

static bool isExecuting = false;
void TopBar::lockDropdownClicked(const StateClicked& state){
    lockUnlockWidget->close();
    if(walletModel && !isExecuting) {
        isExecuting = true;

        switch (lockUnlockWidget->lock) {
            case 0: {
                walletModel->setWalletLocked(true);
                ui->pushButtonLock->setButtonText("Wallet Locked");
                ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-lock", true);
                break;
            }
            case 1: {
                mainWindow->showHide(true);
                AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::Unlock, mainWindow, walletModel,
                                        AskPassphraseDialog::Context::ToggleLock);
                dlg->adjustSize();
                openDialogWithOpaqueBackgroundY(dlg, mainWindow);
                if (this->walletModel->getEncryptionStatus() == WalletModel::Unlocked) {
                    ui->pushButtonLock->setButtonText("Wallet Unlocked");
                    ui->pushButtonLock->setButtonClassStyle("cssClass", "btn-check-status-unlock", true);
                }
                break;
            }
            case 2: {
                mainWindow->showHide(true);
                AskPassphraseDialog *dlg = new AskPassphraseDialog(AskPassphraseDialog::Mode::UnlockAnonymize, mainWindow, walletModel,
                                        AskPassphraseDialog::Context::ToggleLock);
                dlg->adjustSize();
                openDialogWithOpaqueBackgroundY(dlg, mainWindow);
                if (this->walletModel->getEncryptionStatus() == WalletModel::UnlockedForAnonymizationOnly) {
                    ui->pushButtonLock->setButtonText("Wallet Unlocked for staking");
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

void TopBar::showPasswordDialog() {
    mainWindow->showHide(true);
    WalletPasswordDialog* dialog = new WalletPasswordDialog(mainWindow);
    openDialogWithOpaqueBackgroundY(dialog, mainWindow, 3, 5);
}

void TopBar::lockDropdownMouseLeave(){
    lockUnlockWidget->hide();
    ui->pushButtonLock->setKeepExpanded(false);
    ui->pushButtonLock->setSmall();
    ui->pushButtonLock->update();
}

void TopBar::onBtnReceiveClicked(){
    if(walletModel) {
        mainWindow->showHide(true);
        ReceiveDialog *receiveDialog = new ReceiveDialog(mainWindow);

        receiveDialog->updateQr(walletModel->getAddressTableModel()->getLastUnusedAddress());
        if (openDialogWithOpaqueBackground(receiveDialog, mainWindow)) {
            // TODO: Complete me..
            emit message("",tr("Address Copied"), CClientUIInterface::MSG_INFORMATION);
        }
    }
}

void TopBar::showTop(){
    if(ui->bottom_container->isVisible()){
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

TopBar::~TopBar()
{
    if(timerStakingIcon){
        timerStakingIcon->stop();
    }
    delete ui;
}

void TopBar::setClientModel(ClientModel *model){
    this->clientModel = model;
    if(clientModel){
        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));

        connect(clientModel->getOptionsModel(), SIGNAL(zeromintEnableChanged(bool)), this, SLOT(updateAutoMintStatus()));
        updateAutoMintStatus();

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
        if (masternodeSync.IsSynced()) {
            // Node synced
            // TODO: Set synced icon to pushButtonSync here..
            ui->pushButtonSync->setButtonText(tr("Synchronized"));
            return;
        }else{

            // TODO: Show out of sync warning

            int nAttempt = masternodeSync.RequestedMasternodeAttempt < MASTERNODE_SYNC_THRESHOLD ?
                       masternodeSync.RequestedMasternodeAttempt + 1 :
                       MASTERNODE_SYNC_THRESHOLD;
            int progress = nAttempt + (masternodeSync.RequestedMasternodeAssets - 1) * MASTERNODE_SYNC_THRESHOLD;
            if(progress >= 0){
                text = std::string("Synchronizing additional data: %p%", progress);
                needState = false;
            }
        }
        //strSyncStatus = QString(masternodeSync.GetSyncStatus().c_str());
        //progressBarLabel->setText(strSyncStatus);
        //tooltip = strSyncStatus + QString("<br>") + tooltip;
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
        text = str.toUtf8().constData();
    }

    if(text.empty()){
        text = "No block source available..";
    }

    ui->pushButtonSync->setButtonText(tr(text.data()));


}

void TopBar::setWalletModel(WalletModel *model){
    this->walletModel = model;

    connect(model, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this,
            SLOT(updateBalances(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
    connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    connect(model, SIGNAL(encryptionStatusChanged(int status)), this, SLOT(refreshStatus()));

    // update the display unit, to not use the default ("PIVX")
    updateDisplayUnit();

    // TODO: Complete me..
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
    CAmount nTotalBalance = balance + unconfirmedBalance;
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
