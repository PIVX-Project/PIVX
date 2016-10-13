#ifndef BITTREXTRADINGTAB_H
#define BITTREXTRADINGTAB_H

#include "clientmodel.h"
#include "walletmodel.h"


#include <QWidget>
#include <QFrame>
#include <QJsonObject>
#include <QJsonArray>

#include <stdint.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

class WalletModel;

namespace Ui {
    class BittrexTradingTab;
}

QT_BEGIN_NAMESPACE
class QFrame;
class QTableView;
class QItemSelection;
class QTableWidget;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
class QLabel;
class QTimer;
class QString;
QT_END_NAMESPACE

/** Page for Bittrex trading */
class BittrexTradingTab : public QWidget
{
    Q_OBJECT

public:
    explicit BittrexTradingTab(QWidget *parent = 0);
    ~BittrexTradingTab();

    void setModel(WalletModel *walletModel);

private:
    Ui::BittrexTradingTab *ui;
    // QMenu *contextMenu;
    int timerid;
    QTimer *timer;
    QString ApiKey;
    QString SecretKey;
    WalletModel *model;

public Q_SLOTS:
    // void exportClicked();

private Q_SLOTS:

    void InitTrading();
    void on_TradingTabWidget_tabBarClicked(int index);
    void ParseAndPopulateOrderBookTables(QString Response);
    void ParseAndPopulateMarketHistoryTable(QString Response);
    void ParseAndPopulateAccountHistoryTable(QString Response);
    void ParseAndPopulateOpenOrdersTable(QString Response);
    void UpdaterFunction();
    void CreateOrderBookTables(QTableWidget& Table,QStringList TableHeader);
    void DisplayBalance(QLabel &BalanceLabel,QLabel &Available, QLabel &Pending, QString Currency,QString Response);
    void DisplayBalance(QLabel &BalanceLabel, QLabel &BalanceLabel2, QString Response, QString Response2);
    void DisplayBalance(QLabel &BalanceLabel, QString Response);
    void ActionsOnSwitch(int index);
    void CancelOrderSlot(int row, int col);
    void on_UpdateKeys_clicked(bool Save=false, bool Load=false);
    void on_LoadKeys_clicked();
    void on_SaveKeys_clicked();
    void on_GenDepositBTN_clicked();

    void CalculateBuyCostLabel();
    void on_Buy_Max_Amount_clicked();
    void on_BuyBidcomboBox_currentIndexChanged(const QString &arg1);
    void on_UnitsInput_textChanged(const QString &arg1);
    void on_BuyBidPriceEdit_textChanged(const QString &arg1);
    void on_BuyDNET_clicked();

    void CalculateSellCostLabel();
    void on_Sell_Max_Amount_clicked();
    void on_SellBidcomboBox_currentIndexChanged(const QString &arg1);
    void on_UnitsInputDNET_textChanged(const QString &arg1);
    void on_SellBidPriceEdit_textChanged(const QString &arg1);
    void on_SellDNETBTN_clicked();

    void CalculateCSReceiveLabel();
    void on_CSUnitsInput_textChanged(const QString &arg1);
    void on_CSUnitsBtn_clicked();
    void on_CS_Max_Amount_clicked();

    void on_Withdraw_Max_Amount_clicked();
    void on_WithdrawUnitsBtn_clicked();

    void on_KeyPasteButton_clicked();
    void on_SecretPasteButton_clicked();
    void on_CSPasteButton_clicked();
    void on_WithdrawPasteButton_clicked();
    void on_DepositCopyButton_clicked();

    int SetExchangeInfoTextLabels();

    QString BittrexTimeStampToReadable(QString DateTime);
    QString CancelOrder(QString Orderid);
    QString BuyDNET(QString OrderType, double Quantity, double Rate);
    QString SellDNET(QString OrderType, double Quantity, double Rate);
    QString Withdraw(double Amount, QString Address, QString Coin);
    QString GetMarketHistory();
    QString GetMarketSummary();
    QString GetOrderBook();
    QString GetOpenOrders();
    QString GetAccountHistory();
    QString GetBalance(QString Currency);
    QString GetDepositAddress();
    QString HMAC_SHA512_SIGNER(QString UrlToSign,QString Secretkey);
    QString sendRequest(QString url);
    std::string encryptDecrypt(std::string toEncrypt, std::string password);
    QJsonObject GetResultObjectFromJSONObject(QString response);
    QJsonObject GetResultObjectFromJSONArray(QString response);
    QJsonArray  GetResultArrayFromJSONObject(QString response);

};

#endif // BITTREXTRADINGTAB_H
