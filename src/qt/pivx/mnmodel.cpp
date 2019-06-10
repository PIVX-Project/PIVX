#include "qt/pivx/mnmodel.h"

#include "masternode-sync.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "activemasternode.h"
#include "sync.h"

MNModel::MNModel(QObject *parent) : QAbstractTableModel(parent){
    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        int nIndex;
        if(!mne.castOutputIndex(nIndex))
            continue;

        CTxIn txin = CTxIn(uint256S(mne.getTxHash()), uint32_t(nIndex));
        CMasternode* pmn = mnodeman.Find(txin);
        nodes.insert(QString::fromStdString(mne.getAlias()), std::make_pair(QString::fromStdString(mne.getIp()), pmn));
    }
}

int MNModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return nodes.size();
}

int MNModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 5;
}


QVariant MNModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
            return QVariant();

    // rec could be null, always verify it.
    CMasternode* rec = static_cast<CMasternode*>(index.internalPointer());
    int row = index.row();
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case ALIAS:
                return nodes.uniqueKeys().value(row);
            case ADDRESS:
                return nodes.values().value(row).first;
        }
    }
    return QVariant();
}

QModelIndex MNModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    std::pair<QString, CMasternode*> pair = nodes.values().value(row);
    CMasternode* data = pair.second;
    if (data) {
        return createIndex(row, column, data);
    } else if (!pair.first.isEmpty()){
        return createIndex(row, column, nullptr);
    } else {
        return QModelIndex();
    }
}
