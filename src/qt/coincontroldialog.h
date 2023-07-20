// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2017-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINCONTROLDIALOG_H
#define BITCOIN_QT_COINCONTROLDIALOG_H

#include "amount.h"
#include "qt/hemis/snackbar.h"
#include "optional.h"
#include "uint256.h"

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QTreeWidgetItem>

class WalletModel;
class CCoinControl;
class CTxMemPool;

struct TotalAmounts {
    CAmount nPayAmount{0};      // sum of recipients send amount
    CAmount nAmount{0};         // sum of value of selected inputs
    CAmount nPayFee{0};         // expected tx fee
    CAmount nAfterFee{0};       // nAmount - nPayFee
    CAmount nChange{0};         // nAfterFee - nPayAmount
    unsigned int nQuantity{0};  // number of selected inputs
    unsigned int nBytes{0};     // expected tx size
    bool fDust{false};          // true if any output is below the dust threshold
};

namespace Ui
{
class CoinControlDialog;
}

class CCoinControlWidgetItem : public QTreeWidgetItem
{
public:
    explicit CCoinControlWidgetItem(QTreeWidget *parent, int type = Type) : QTreeWidgetItem(parent, type) {}
    explicit CCoinControlWidgetItem(int type = Type) : QTreeWidgetItem(type) {}
    explicit CCoinControlWidgetItem(QTreeWidgetItem *parent, int type = Type) : QTreeWidgetItem(parent, type) {}

    bool operator<(const QTreeWidgetItem &other) const override;
};

class CoinControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlDialog(QWidget* parent = nullptr, bool _forDelegation = false);
    ~CoinControlDialog() override;

    void setModel(WalletModel* model);
    void updateLabels();
    void updateView();
    void refreshDialog();
    void clearPayAmounts();
    void addPayAmount(const CAmount& amount, bool isShieldedRecipient);
    void setSelectionType(bool isTransparent) { fSelectTransparent = isTransparent; }
    bool hasModel() { return model; }

    CCoinControl* coinControl{nullptr};

private:
    Ui::CoinControlDialog* ui{nullptr};
    SnackBar *snackBar{nullptr};
    WalletModel* model{nullptr};
    int sortColumn;
    Qt::SortOrder sortOrder;
    bool forDelegation;
    int colCheckBoxWidth_treeMode{110};
    // pair (recipient amount, ishielded recipient)
    std::vector<std::pair<CAmount, bool>> payAmounts{};
    unsigned int nSelectableInputs{0};

    // whether should show available utxo or notes.
    bool fSelectTransparent{true};

    QMenu* contextMenu{nullptr};
    QTreeWidgetItem* contextMenuItem{nullptr};
    QAction* copyTransactionHashAction{nullptr};
    QAction* lockAction{nullptr};
    QAction* unlockAction{nullptr};

    void updatePushButtonSelectAll(bool checked);
    void sortView(int, Qt::SortOrder);
    void inform(const QString& text);
    void toggleItemLock(QTreeWidgetItem* item);
    void toggleCoinLock();

    // Load a row with coin's data
    void loadAvailableCoin(bool treeMode,
                           CCoinControlWidgetItem* itemWalletAddress,
                           QFlags<Qt::ItemFlag> flgCheckbox,
                           QFlags<Qt::ItemFlag> flgTristate,
                           int nDisplayUnit,
                           const QString& sWalletAddress,
                           const Optional<QString>& stakerAddress,
                           const QString& sWalletLabel,
                           const uint256& txhash,
                           const uint32_t outIndex,
                           const CAmount nValue,
                           const int64_t nTime,
                           const int nDepth,
                           const bool isChange);

    // calculate sums for selected amount, number of inputs, change, fee, after fee value, and transaction size
    TotalAmounts getTotals() const;


    enum {
        COLUMN_CHECKBOX,
        COLUMN_AMOUNT,
        COLUMN_LABEL,
        COLUMN_ADDRESS,
        COLUMN_DATE,
        COLUMN_CONFIRMATIONS,
        COLUMN_TXHASH,
        COLUMN_VOUT_INDEX,
    };
    friend class CCoinControlWidgetItem;

private Q_SLOTS:
    void showMenu(const QPoint&);
    void copyAmount();
    void copyLabel();
    void copyAddress();
    void copyTransactionHash();
    void lockCoin();
    void unlockCoin();
    void clipboardQuantity();
    void clipboardAmount();
    void clipboardFee();
    void clipboardAfterFee();
    void clipboardBytes();
    void clipboardLowOutput();
    void clipboardChange();
    void radioTreeMode(bool);
    void radioListMode(bool);
    void viewItemChanged(QTreeWidgetItem*, int);
    void headerSectionClicked(int);
    void buttonSelectAllClicked();
    void buttonToggleLockClicked();
    void updateLabelLocked();
};

#endif // BITCOIN_QT_COINCONTROLDIALOG_H
