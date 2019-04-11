// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZPIVCONTROLDIALOG_H
#define ZPIVCONTROLDIALOG_H

#include <QDialog>
#include <QTreeWidgetItem>
#include "zpiv/zerocoin.h"
#include "privacydialog.h"

class CZerocoinMint;
class WalletModel;

namespace Ui {
class ZPivControlDialog;
}

class CZPivControlWidgetItem : public QTreeWidgetItem
{
public:
    explicit CZPivControlWidgetItem(QTreeWidget *parent, int type = Type) : QTreeWidgetItem(parent, type) {}
    explicit CZPivControlWidgetItem(int type = Type) : QTreeWidgetItem(type) {}
    explicit CZPivControlWidgetItem(QTreeWidgetItem *parent, int type = Type) : QTreeWidgetItem(parent, type) {}

    bool operator<(const QTreeWidgetItem &other) const;
};

class ZPivControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ZPivControlDialog(QWidget *parent);
    ~ZPivControlDialog();

    void setModel(WalletModel* model);

    static std::set<std::string> setSelectedMints;
    static std::set<CMintMeta> setMints;
    static std::vector<CMintMeta> GetSelectedMints();

private:
    Ui::ZPivControlDialog *ui;
    WalletModel* model;
    PrivacyDialog* privacyDialog;

    void updateList();
    void updateLabels();

    enum {
        COLUMN_CHECKBOX,
        COLUMN_DENOMINATION,
        COLUMN_PUBCOIN,
        COLUMN_VERSION,
        COLUMN_PRECOMPUTE,
        COLUMN_CONFIRMATIONS,
        COLUMN_ISSPENDABLE
    };
    friend class CZPivControlWidgetItem;

private slots:
    void updateSelection(QTreeWidgetItem* item, int column);
    void ButtonAllClicked();
};

#endif // ZPIVCONTROLDIALOG_H
