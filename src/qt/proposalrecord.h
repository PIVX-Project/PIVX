// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018 The PHORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_QT_PROPOSALRECORD_H
#define PIVX_QT_PROPOSALRECORD_H

#include "amount.h"
#include "uint256.h"

#include <QList>
#include <QString>

class CWallet;

class ProposalRecord
{
public:
    ProposalRecord() :
        hash(""),
        start_epoch(0),
        end_epoch(0),
        url(""),
        name(""),
        yesVotes(0),
        noVotes(0),
        abstainVotes(0),
        amount(0),
        percentage(0)
    {}

    ProposalRecord(QString hash, const CAmount& start_epoch, const CAmount& end_epoch, QString url, QString name, const CAmount& yesVotes, const CAmount& noVotes, const CAmount& abstainVotes, const CAmount& amount, const CAmount& percentage) :
        hash(hash),
        start_epoch(start_epoch),
        end_epoch(end_epoch),
        url(url),
        name(name),
        yesVotes(yesVotes),
        noVotes(noVotes),
        abstainVotes(abstainVotes),
        amount(amount),
        percentage(percentage)
    {}

    QString hash;
    int start_epoch;
    int end_epoch;
    QString url;
    QString name;
    int yesVotes;
    int noVotes;
    int abstainVotes;
    CAmount amount;
    double percentage;
};

#endif // PIVX_QT_PROPOSALRECORD_H
