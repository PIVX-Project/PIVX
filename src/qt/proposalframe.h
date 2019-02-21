// Copyright (c) 2018 The VPX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VPX_QT_PROPOSALFRAME_H
#define VPX_QT_PROPOSALFRAME_H

#include <QFrame>
#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>

#include "walletmodel.h"
#include "governancepage.h"

class CBudgetProposal;

class ProposalFrame : public QFrame
{
    Q_OBJECT

public:
    explicit ProposalFrame(QWidget* parent = 0);
    ~ProposalFrame();

    void setProposal(CBudgetProposal* proposal);
    void setWalletModel(WalletModel* walletModel);
    void SendVote(std::string strHash, int nVote);
    void setGovernancePage(GovernancePage* governancePage);
    void refresh();
    void close();
    void extend();

protected:
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);

private:
    bool extended;
    CBudgetProposal* proposal;
    WalletModel* walletModel;
    GovernancePage* governancePage;

    QVBoxLayout* proposalItem;
    QHBoxLayout* proposalURL;
    QLabel* strRemainingPaymentCount;
    QHBoxLayout* proposalVotes;

private Q_SLOTS:
    void voteButton_clicked(int nVote);
    void proposalLink_clicked(const QString &link);
};

#endif //VPX_QT_PROPOSALFRAME_H
