//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Sombe/txrow.h"
#include "qt/Sombe/forms/ui_txrow.h"

#include "guiutil.h"
#include "qt/Sombe/qtutils.h"

TxRow::TxRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TxRow)
{
    ui->setupUi(this);
}

void TxRow::init(bool isLightTheme)
{
    setConfirmStatus(true);
    updateStatus(isLightTheme, false, false);
}

void TxRow::setConfirmStatus(bool isConfirm)
{
    if (isConfirm) {
        setCssProperty(ui->lblAddress, "text-list-body1");
        setCssProperty(ui->lblDate, "text-list-caption");
    } else {
        setCssProperty(ui->lblAddress, "text-list-body-unconfirmed");
        setCssProperty(ui->lblDate,"text-list-caption-unconfirmed");
    }
}

void TxRow::updateStatus(bool isLightTheme, bool isHover, bool isSelected)
{
    if (isLightTheme)
        ui->lblDivisory->setStyleSheet("background-color:#bababa");
    else
        ui->lblDivisory->setStyleSheet("background-color:#40ffffff");
}

void TxRow::setDate(QDateTime date)
{
    ui->lblDate->setText(GUIUtil::dateTimeStr(date));
}

void TxRow::setLabel(QString str)
{
    ui->lblAddress->setText(str);
}

void TxRow::setAmount(QString str)
{
    ui->lblAmount->setText(str);
}

void TxRow::setType(bool isLightTheme, int type, bool isConfirmed)
{
    QString path;
    QString css;
    bool sameIcon = false;
    switch (type) {
        case TransactionRecord::Generated:
        case TransactionRecord::MNReward:
        case TransactionRecord::StakeMint:
            path = "://ic-transaction-staked";
            css = "text-list-amount-receive";
            break;
        case TransactionRecord::RecvWithAddress:
        case TransactionRecord::RecvFromOther:
            path = "://ic-transaction-received";
            css = "text-list-amount-receive";
            break;
        case TransactionRecord::SendToAddress:
        case TransactionRecord::SendToOther:
            path = "://ic-transaction-sent";
            css = "text-list-amount-send";
            break;
        case TransactionRecord::SendToSelf:
            path = "://ic-transaction-mint";
            css = "text-list-amount-send";
            break;
        case TransactionRecord::StakeDelegated:
            path = "://ic-transaction-stake-delegated";
            css = "text-list-amount-receive";
            break;
        case TransactionRecord::StakeHot:
            path = "://ic-transaction-stake-hot";
            css = "text-list-amount-unconfirmed";
            break;
        case TransactionRecord::P2CSDelegationSent:
        case TransactionRecord::P2CSDelegationSentOwner:
            path = "://ic-transaction-cs-contract";
            css = "text-list-amount-send";
            break;
        case TransactionRecord::P2CSDelegation:
            path = "://ic-transaction-cs-contract";
            css = "text-list-amount-unconfirmed";
            break;
        case TransactionRecord::P2CSUnlockOwner:
        case TransactionRecord::P2CSUnlockStaker:
            path = "://ic-transaction-cs-contract";
            css = "text-list-amount-send";
            break;
        default:
            path = "://ic-pending";
            sameIcon = true;
            css = "text-list-amount-unconfirmed";
            break;
    }

    if (!isLightTheme && !sameIcon){
        path += "-dark";
    }

    if (!isConfirmed){
        css = "text-list-amount-unconfirmed";
        path += "-inactive";
        setConfirmStatus(false);
    } else {
        setConfirmStatus(true);
    }
    setCssProperty(ui->lblAmount, css, true);
    ui->icon->setIcon(QIcon(path));
}

TxRow::~TxRow()
{
    delete ui;
}
