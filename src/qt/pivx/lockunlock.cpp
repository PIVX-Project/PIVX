// Copyright (c) 2019-2020 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/lockunlock.h"
#include "qt/pivx/forms/ui_lockunlock.h"

LockUnlock::LockUnlock(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LockUnlock)
{
    ui->setupUi(this);

    // Load css.
    this->setStyleSheet(parent->styleSheet());

    ui->container->setProperty("cssClass", "top-sub-menu");

    ui->pushButtonUnlocked->setProperty("cssClass", "btn-check-lock-sub-menu-unlocked");
    ui->pushButtonUnlocked->setStyleSheet("padding-left: 34px;");
    ui->pushButtonLocked->setProperty("cssClass", "btn-check-lock-sub-menu-locked");
    ui->pushButtonLocked->setStyleSheet("padding-left: 34px;");
    ui->pushButtonStaking->setProperty("cssClass", "btn-check-lock-sub-menu-staking");
    ui->pushButtonStaking->setStyleSheet("padding-left: 34px;");

    // Connect
    connect(ui->pushButtonUnlocked, &QPushButton::clicked, this, &LockUnlock::onUnlockClicked);
    connect(ui->pushButtonLocked, &QPushButton::clicked, this, &LockUnlock::onLockClicked);
    connect(ui->pushButtonStaking, &QPushButton::clicked, this, &LockUnlock::onStakingClicked);
}

LockUnlock::~LockUnlock()
{
    delete ui;
}

void LockUnlock::updateStatus(WalletModel::EncryptionStatus status)
{
    switch (status) {
        case WalletModel::EncryptionStatus::Unlocked:
            ui->pushButtonUnlocked->setChecked(true);
            ui->pushButtonLocked->setChecked(false);
            ui->pushButtonStaking->setChecked(false);
            break;
        case WalletModel::EncryptionStatus::UnlockedForStaking:
            ui->pushButtonUnlocked->setChecked(false);
            ui->pushButtonLocked->setChecked(false);
            ui->pushButtonStaking->setChecked(true);
            break;
        case WalletModel::EncryptionStatus::Locked:
            ui->pushButtonUnlocked->setChecked(false);
            ui->pushButtonLocked->setChecked(true);
            ui->pushButtonStaking->setChecked(false);
            break;
        default:
            break;
    }
}

void LockUnlock::onLockClicked()
{
    lock = 0;
    Q_EMIT lockClicked(StateClicked::LOCK);
}

void LockUnlock::onUnlockClicked()
{
    lock = 1;
    Q_EMIT lockClicked(StateClicked::UNLOCK);
}

void LockUnlock::onStakingClicked()
{
    lock = 2;
    Q_EMIT lockClicked(StateClicked::UNLOCK_FOR_STAKING);
}

void LockUnlock::enterEvent(QEvent *)
{
    isOnHover = true;
    Q_EMIT Mouse_Entered();
}

void LockUnlock::leaveEvent(QEvent *)
{
    isOnHover = false;
    Q_EMIT Mouse_Leave();
}

bool LockUnlock::isHovered()
{
    return isOnHover;
}
