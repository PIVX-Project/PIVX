// Copyright (c) 2019 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LOCKUNLOCK_H
#define LOCKUNLOCK_H

#include <QWidget>
#include "walletmodel.h"

namespace Ui {
class LockUnlock;
}

enum StateClicked{
    LOCK,UNLOCK,UNLOCK_FOR_STAKING
};


class LockUnlock : public QWidget
{
    Q_OBJECT

public:
    explicit LockUnlock(QWidget *parent = nullptr);
    ~LockUnlock();
    void updateStatus(WalletModel::EncryptionStatus status);
    int lock = 0;
    bool isHovered();
Q_SIGNALS:
    void Mouse_Entered();
    void Mouse_Leave();

    void lockClicked(const StateClicked& state);
protected:
    virtual void enterEvent(QEvent *);
    virtual void leaveEvent(QEvent *);

public Q_SLOTS:
    void onLockClicked();
    void onUnlockClicked();
    void onStakingClicked();

private:
    Ui::LockUnlock *ui;
    bool isOnHover = false;
};

#endif // LOCKUNLOCK_H
