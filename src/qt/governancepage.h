// Copyright (c) 2018 The VPX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GOVERNANCEPAGE_H
#define BITCOIN_QT_GOVERNANCEPAGE_H

#include "masternode.h"
#include "platformstyle.h"
#include "sync.h"
#include "util.h"

#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>
#include <QWidget>

class ClientModel;
class WalletModel;
class CBudgetProposal;

namespace Ui
{
    class GovernancePage;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class GovernancePage : public QWidget
{
    Q_OBJECT

public:
    explicit GovernancePage(QWidget* parent = 0);
    ~GovernancePage();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);
    void setExtendedProposal(CBudgetProposal* proposal);
    void lockUpdating(bool lock);

private:
    QMenu* contextMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;
    bool fLockUpdating;

public Q_SLOTS:
    void updateProposalList();

Q_SIGNALS:

private:
    QTimer* timer;
    Ui::GovernancePage* ui;
    ClientModel* clientModel;
    WalletModel* walletModel;
    QString strCurrentFilter;
    CBudgetProposal* extendedProposal;

private Q_SLOTS:
    void on_UpdateButton_clicked();
};

#endif // BITCOIN_QT_GOVERNANCEPAGE_H
