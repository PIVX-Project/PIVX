// Copyright (c) 2021-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PROPOSALCARD_H
#define PROPOSALCARD_H

#include "qt/pivx/governancemodel.h"

#include <QGridLayout>
#include <QWidget>

namespace Ui {
class ProposalCard;
}

class ProposalCard : public QWidget
{
    Q_OBJECT

public:
    explicit ProposalCard(QWidget *parent = nullptr);
    ~ProposalCard() override;

    void setProposal(const ProposalInfo& _proposalInfo);
    ProposalInfo getProposal() { return proposalInfo; }

    // Update-only functions
    void setNeedsUpdate(bool _update) { needsUpdate = _update; }
    bool isUpdateNeeded() const { return needsUpdate; }

public Q_SLOTS:
    void onCopyUrlClicked();

Q_SIGNALS:
    void voteClicked(const ProposalInfo& proposalInfo);
    void inform(const QString& text);
    void onMenuClicked(ProposalCard* card);

private:
    Ui::ProposalCard *ui;
    ProposalInfo proposalInfo;
    bool needsUpdate{false};

    void setStatusAndVotes(const QString& msg, int value);
};

#endif // PROPOSALCARD_H
