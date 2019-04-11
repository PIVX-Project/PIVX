// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PRIVACYDIALOG_H
#define BITCOIN_QT_PRIVACYDIALOG_H

#include "guiutil.h"

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QTimer>
#include <QVariant>

class OptionsModel;
class WalletModel;

namespace Ui
{
class PrivacyDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog for requesting payment of bitcoins */
class PrivacyDialog : public QDialog
{
    Q_OBJECT

public:
    enum ColumnWidths {
        DATE_COLUMN_WIDTH = 130,
        LABEL_COLUMN_WIDTH = 120,
        AMOUNT_MINIMUM_COLUMN_WIDTH = 160,
        MINIMUM_COLUMN_WIDTH = 130
    };

    explicit PrivacyDialog(QWidget* parent = 0);
    ~PrivacyDialog();

    void setModel(WalletModel* model);
    void showOutOfSyncWarning(bool fShow);
    void setZPivControlLabels(int64_t nAmount, int nQuantity);

public slots:
    void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                    const CAmount& zerocoinBalance, const CAmount& unconfirmedZerocoinBalance, const CAmount& immatureZerocoinBalance,
                    const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);
protected:
    virtual void keyPressEvent(QKeyEvent* event);

private:
    Ui::PrivacyDialog* ui;
    QTimer* timer;
    GUIUtil::TableViewLastColumnResizingFixer* columnResizingFixer;
    WalletModel* walletModel;
    QMenu* contextMenu;
    CAmount currentBalance;
    CAmount currentUnconfirmedBalance;
    CAmount currentImmatureBalance;
    CAmount currentZerocoinBalance;
    CAmount currentUnconfirmedZerocoinBalance;
    CAmount currentImmatureZerocoinBalance;
    CAmount currentWatchOnlyBalance;
    CAmount currentWatchUnconfBalance;
    CAmount currentWatchImmatureBalance;

    bool fMinimizeChange = false;
    bool fDenomsMinimized;

    int nDisplayUnit;
    bool updateLabel(const QString& address);
    void sendzPIV();

private slots:
    void on_payTo_textChanged(const QString& address);
    void on_addressBookButton_clicked();
//    void coinControlFeatureChanged(bool);
    void coinControlButtonClicked();
//    void coinControlChangeChecked(int);
//    void coinControlChangeEdited(const QString&);
    void coinControlUpdateLabels();

    void coinControlClipboardQuantity();
    void coinControlClipboardAmount();
//    void coinControlClipboardFee();
//    void coinControlClipboardAfterFee();
//    void coinControlClipboardBytes();
//    void coinControlClipboardPriority();
//    void coinControlClipboardLowOutput();
//    void coinControlClipboardChange();

    void on_pushButtonMintzPIV_clicked();
    void on_pushButtonMintReset_clicked();
    void on_pushButtonSpentReset_clicked();
    void on_pushButtonSpendzPIV_clicked();
    void on_pushButtonZPivControl_clicked();
    void on_pushButtonHideDenoms_clicked();
    void on_pushButtonShowDenoms_clicked();
    void on_pasteButton_clicked();
    void minimizeDenomsSection(bool fMinimize);
    void updateDisplayUnit();
    void updateAutomintStatus();
    void updateSPORK16Status();
};

#endif // BITCOIN_QT_PRIVACYDIALOG_H
