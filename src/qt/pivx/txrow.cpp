#include "qt/pivx/txrow.h"
#include "qt/pivx/forms/ui_txrow.h"

#include "guiutil.h"
#include <iostream>

TxRow::TxRow(bool isLightTheme, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TxRow)
{
    ui->setupUi(this);

    ui->lblAddress->setProperty("cssClass", "text-list-body1");
    ui->lblDate->setProperty("cssClass", "text-list-caption");

    updateStatus(isLightTheme, false, false);
}

void TxRow::updateStatus(bool isLightTheme, bool isHover, bool isSelected){
    if(isLightTheme)
        ui->lblDivisory->setStyleSheet("background-color:#bababa");
    else
        ui->lblDivisory->setStyleSheet("background-color:#40ffffff");
}

void TxRow::setDate(QDateTime date){
    ui->lblDate->setText(GUIUtil::dateTimeStr(date));
}

void TxRow::setLabel(QString str){
    ui->lblAddress->setText(str);
}

void TxRow::setAmount(QString str){
    ui->lblAmount->setText(str);
}

// TODO: Agregar send to zPIV and receive zPIV icons..
void TxRow::setType(bool isLightTheme, TransactionRecord::Type type){
    QString path;
    QString css;
    switch (type) {
        case TransactionRecord::ZerocoinMint:
            path = "://ic-transaction-mint";
            css = "text-list-amount-send";
            break;
        case TransactionRecord::Generated:
        case TransactionRecord::StakeZPIV:
        case TransactionRecord::MNReward:
        case TransactionRecord::StakeMint:
            path = "://ic-transaction-staked";
            css = "text-list-amount-send";
            break;
        case TransactionRecord::RecvWithObfuscation:
        case TransactionRecord::RecvWithAddress:
        case TransactionRecord::RecvFromOther:
        case TransactionRecord::RecvFromZerocoinSpend:
            path = "://ic-transaction-received";
            css = "text-list-amount-receive";
            break;
        case TransactionRecord::SendToAddress:
        case TransactionRecord::SendToOther:
        case TransactionRecord::ZerocoinSpend:
        case TransactionRecord::ZerocoinSpend_Change_zPiv:
        case TransactionRecord::ZerocoinSpend_FromMe:
            path = "://ic-transaction-sent";
            css = "text-list-amount-send";
            break;
        case TransactionRecord::SendToSelf:
            path = "://ic-transaction-mint";
            css = "text-list-amount-send";
            break;
        default:
            path = ":/icons/tx_inout";
            break;
    }
    if (!isLightTheme){
        path += "-dark";
    }
    ui->lblAmount->setProperty("cssClass", css);
    QPixmap pixmap(path);
    QIcon buttonIcon(pixmap);
    ui->icon->setIcon(buttonIcon);
}

TxRow::~TxRow()
{
    delete ui;
}

void TxRow::sendRow(bool isLightTheme){
    ui->lblAddress->setText("Sent to DN6i46dytMPV..hV1FuQBh7BZZ6nN");
    ui->lblDate->setText("10/04/2019");
    QPixmap pixmap(isLightTheme ? "://ic-transaction-sent" : "://ic-transaction-sent-dark");
    ui->lblAmount->setText("-5.00 PIV");
    ui->lblAmount->setProperty("cssClass", "text-list-amount-send");
    QIcon buttonIcon(pixmap);
    ui->icon->setIcon(buttonIcon);
}

void TxRow::receiveRow(bool isLightTheme){
    ui->lblAddress->setText("Received from DN6i46dytMPVhVBh7BZZD23");
    ui->lblDate->setText("10/04/2019");
    ui->lblAmount->setText("+0.00543 PIV");
    ui->lblAmount->setProperty("cssClass", "text-list-amount-receive");
    QPixmap pixmap(isLightTheme ? "://ic-transaction-received" : "://ic-transaction-received-dark");
    QIcon buttonIcon(pixmap);
    ui->icon->setIcon(buttonIcon);
}
void TxRow::stakeRow(bool isLightTheme){
    ui->lblAddress->setText("PIV Staked");
    ui->lblDate->setText("10/04/2019");
    ui->lblAmount->setText("+2.00 PIV");
    ui->lblAmount->setProperty("cssClass", "text-list-amount-send");
    QPixmap pixmap(isLightTheme ? "://ic-transaction-staked" : "://ic-transaction-staked-dark");
    QIcon buttonIcon(pixmap);
    ui->icon->setIcon(buttonIcon);
}
void TxRow::zPIVStakeRow(bool isLightTheme){
    ui->lblAddress->setText("zPIV Staked");
    ui->lblDate->setText("10/04/2019");
    ui->lblAmount->setText("+3.00 zPIV");
    ui->lblAmount->setProperty("cssClass", "text-list-amount-send");
    QPixmap pixmap(isLightTheme ? "://ic-transaction-staked" : "://ic-transaction-staked-dark");
    QIcon buttonIcon(pixmap);
    ui->icon->setIcon(buttonIcon);
}
void TxRow::mintRow(bool isLightTheme){
    ui->lblAddress->setText("Mint");
    ui->lblDate->setText("10/04/2019");
    ui->lblAmount->setText("+0.00 PIV");
    ui->lblAmount->setProperty("cssClass", "text-list-amount-send");
    QPixmap pixmap(isLightTheme ? "://ic-transaction-mint" : "://ic-transaction-mint-dark");
    QIcon buttonIcon(pixmap);
    ui->icon->setIcon(buttonIcon);
}
