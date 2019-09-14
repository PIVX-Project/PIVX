// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINCONTROLDIALOG_H
#define BITCOIN_QT_COINCONTROLDIALOG_H

#include "amount.h"

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QTreeWidgetItem>

class WalletModel;

class MultisigDialog;
class CCoinControl;
class CTxMemPool;

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

    bool operator<(const QTreeWidgetItem &other) const;
};

class CoinControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlDialog(QWidget* parent = nullptr, bool fMultisigEnabled = false);
    ~CoinControlDialog();

    void setModel(WalletModel* model);
    void updateDialogLabels();
    void updateView();

    // static because also called from sendcoinsdialog
    static void updateLabels(WalletModel*, QDialog*);
    static QString getPriorityLabel(double dPriority, double mempoolEstimatePriority);

    static QList<CAmount> payAmounts;
    static CCoinControl* coinControl;
    static int nSplitBlockDummy;

private:
    Ui::CoinControlDialog* ui;
    WalletModel* model;
    int sortColumn;
    Qt::SortOrder sortOrder;
    bool fMultisigEnabled;

    QMenu* contextMenu;
    QTreeWidgetItem* contextMenuItem;
    QAction* copyTransactionHashAction;
    QAction* lockAction;
    QAction* unlockAction;

    void sortView(int, Qt::SortOrder);

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

private slots:
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
