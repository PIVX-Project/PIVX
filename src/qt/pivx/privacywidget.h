#ifndef PRIVACYWIDGET_H
#define PRIVACYWIDGET_H

#include "qt/pivx/furabstractlistitemdelegate.h"
#include "qt/pivx/txviewholder.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"

#include <QLabel>
#include <QWidget>

class PIVXGUI;
class WalletModel;

namespace Ui {
class PrivacyWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class PrivacyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PrivacyWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~PrivacyWidget();

    void setWalletModel(WalletModel *_model);
private slots:
    void changeTheme(bool isLightTheme, QString &theme);
    void onCoinControlClicked();
    void onDenomClicked();
    void onRescanMintsClicked();
    void onResetZeroClicked();
    void onTotalZpivClicked();
    void updateDisplayUnit();
    void showList();
    void onSendClicked();
    void onMintSelected(bool isMint);

signals:
    void message(const QString& title, const QString& message, unsigned int style);
private:
    Ui::PrivacyWidget *ui;
    PIVXGUI* window;
    WalletModel* walletModel;
    FurAbstractListItemDelegate *delegate = nullptr;
    TransactionTableModel* txModel = nullptr;
    TxViewHolder *txHolder = nullptr;
    TransactionFilterProxy* filter = nullptr;


    int nDisplayUnit;
    void mint(CAmount value);
    void spend(CAmount value);
    void updateDenomsSupply();
};

#endif // PRIVACYWIDGET_H
