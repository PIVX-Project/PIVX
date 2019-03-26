#include "qt/pivx/txrow.h"
#include "qt/pivx/forms/ui_txrow.h"

TxRow::TxRow(bool isLightTheme, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TxRow)
{
    ui->setupUi(this);

    ui->lblAddress->setProperty("cssClass", "text-list-body1");
    ui->lblDate->setProperty("cssClass", "text-list-caption");

    if(isLightTheme)
        ui->lblDivisory->setStyleSheet("background-color:#bababa");
    else
        ui->lblDivisory->setStyleSheet("background-color:#40ffffff");

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
