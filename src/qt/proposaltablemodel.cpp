// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018 The PHORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "proposaltablemodel.h"

#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "proposalrecord.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "rpc/server.h"
#include "obfuscation.h"
#include "core_io.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QIcon>
#include <QList>
#include <univalue.h>

static int column_alignments[] = {
    Qt::AlignLeft|Qt::AlignVCenter,
    Qt::AlignRight|Qt::AlignVCenter,
    Qt::AlignRight|Qt::AlignVCenter,
    Qt::AlignRight|Qt::AlignVCenter,
    Qt::AlignRight|Qt::AlignVCenter,
    Qt::AlignRight|Qt::AlignVCenter,
    Qt::AlignRight|Qt::AlignVCenter,
    Qt::AlignRight|Qt::AlignVCenter
};

ProposalTableModel::ProposalTableModel(QObject *parent) : QAbstractTableModel(parent)
{
    columns << tr("Proposal") << tr("Amount") << tr("Start Block") << tr("End Block") << tr("Yes") << tr("No") << tr("Abstain") << tr("Percentage");

    networkManager = new QNetworkAccessManager(this);

    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    refreshProposals();
}

ProposalTableModel::~ProposalTableModel()
{}

void budgetToST(CBudgetProposal* pbudgetProposal, UniValue& bObj)
{
    CTxDestination address;
    ExtractDestination(pbudgetProposal->GetPayee(), address);

    bObj.push_back(pbudgetProposal->GetName());
    bObj.push_back(pbudgetProposal->GetURL());
    bObj.push_back(pbudgetProposal->GetHash().ToString());
    bObj.push_back(pbudgetProposal->nFeeTXHash.ToString());
    bObj.push_back(pbudgetProposal->GetBlockStart());
    bObj.push_back(pbudgetProposal->GetBlockEnd());
    bObj.push_back(pbudgetProposal->GetTotalPaymentCount());
    bObj.push_back(pbudgetProposal->GetRemainingPaymentCount());
    bObj.push_back(CBitcoinAddress(address).ToString());
    bObj.push_back(pbudgetProposal->GetYeas());
    bObj.push_back(pbudgetProposal->GetNays());
    bObj.push_back(pbudgetProposal->GetAbstains());
    bObj.push_back(ValueFromAmount(pbudgetProposal->GetAmount() * pbudgetProposal->GetTotalPaymentCount()));
    bObj.push_back(pbudgetProposal->GetAmount());
    bObj.push_back(pbudgetProposal->IsEstablished());

    std::string strError = "";
    bObj.push_back(Pair("IsValid", pbudgetProposal->IsValid(strError)));
    bObj.push_back(Pair("IsValidReason", strError.c_str()));
    bObj.push_back(Pair("fValid", pbudgetProposal->fValid));
}

void ProposalTableModel::refreshProposals()
{
    beginResetModel();
    proposalRecords.clear();

    int mnCount = mnodeman.CountEnabled();
    std::vector<CBudgetProposal*> bObj = budget.GetAllProposals();

    for (CBudgetProposal* pbudgetProposal : bObj)
    {
        UniValue bObj(UniValue::VOBJ);
        budgetToST(pbudgetProposal, bObj);

        int percentage = 0;

        if (mnCount > 0) percentage = round(pbudgetProposal->GetYeas() * 100 / mnCount);

        proposalRecords.append(new ProposalRecord(
                        QString::fromStdString(pbudgetProposal->GetHash().ToString()),
                        pbudgetProposal->GetBlockStart(),
                        pbudgetProposal->GetBlockEnd(),
                        QString::fromStdString(pbudgetProposal->GetURL()),
                        QString::fromStdString(pbudgetProposal->GetName()),
                        pbudgetProposal->GetYeas(),
                        pbudgetProposal->GetNays(),
                        pbudgetProposal->GetAbstains(),
                        pbudgetProposal->GetAmount(),
                        percentage));
    }
    endResetModel();
}

void ProposalTableModel::onResult(QNetworkReply *result)
{}

int ProposalTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return proposalRecords.size();
}

int ProposalTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant ProposalTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    ProposalRecord *rec = static_cast<ProposalRecord*>(index.internalPointer());

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column())
        {
        case Proposal:
            return rec->name;
        case YesVotes:
            return rec->yesVotes;
        case NoVotes:
            return rec->noVotes;
        case AbstainVotes:
            return rec->abstainVotes;
        case StartDate:
            return rec->start_epoch;
        case EndDate:
            return rec->end_epoch;
        case Percentage:
            return QString("%1\%").arg(rec->percentage);
        case Amount:
            return BitcoinUnits::format(BitcoinUnits::PIV, rec->amount);
        }
        break;
    case Qt::EditRole:
        switch (index.column())
        {
        case Proposal:
            return rec->name;
        case StartDate:
            return rec->start_epoch;
        case EndDate:
            return rec->end_epoch;
        case YesVotes:
            return rec->yesVotes;
        case NoVotes:
            return rec->noVotes;
        case AbstainVotes:
            return rec->abstainVotes;
        case Amount:
            return qint64(rec->amount);
        case Percentage:
            return rec->percentage;
        }
        break;
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
        if(index.column() == Percentage) {
            if (rec->percentage < 10) {
                return COLOR_NEGATIVE;
            } else {
                return QColor(23, 168, 26);
            }
        }
        return COLOR_BAREADDRESS;
    case ProposalRole:
        return rec->name;
    case AmountRole:
        return qint64(rec->amount);
    case StartDateRole:
        return rec->start_epoch;
    case EndDateRole:
        return rec->end_epoch;
    case YesVotesRole:
        return rec->yesVotes;
    case NoVotesRole:
        return rec->noVotes;
    case AbstainVotesRole:
        return rec->abstainVotes;
    case PercentageRole:
        return rec->percentage;
    case ProposalUrlRole:
        return rec->url;
    case ProposalHashRole:
        return rec->hash;
    }
    return QVariant();
}

QVariant ProposalTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignCenter;
        }
        else if (role == Qt::ToolTipRole)
        {
            switch (section)
            {
            case Proposal:
                return tr("Proposal Name");
            case StartDate:
                return tr("Date and time that the proposal starts.");
            case EndDate:
                return tr("Date and time that the proposal ends.");
            case YesVotes:
                return tr("Obtained yes votes.");
            case NoVotes:
                return tr("Obtained no votes.");
            case AbstainVotes:
                return tr("Obtained abstain votes.");
            case Amount:
                return tr("Proposed amount.");
            case Percentage:
                return tr("Current vote percentage.");
            }
        }
    }
    return QVariant();
}

QModelIndex ProposalTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if(row >= 0 && row < proposalRecords.size()) {
        ProposalRecord *rec = proposalRecords[row];
        return createIndex(row, column, rec);
    }
    return QModelIndex();
}
