// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/mnmodel.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "activemasternode.h"
#include "sync.h"
#include "uint256.h"
#include "wallet/wallet.h"

MNModel::MNModel(QObject *parent) : QAbstractTableModel(parent){
    updateMNList();
}

void MNModel::updateMNList(){
    int end = nodes.size();
    nodes.clear();
    collateralTxAccepted.clear();
    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        int nIndex;
        if(!mne.castOutputIndex(nIndex))
            continue;

        uint256 txHash(mne.getTxHash());
        CTxIn txIn(txHash, uint32_t(nIndex));
        CMasternode* pmn = mnodeman.Find(txIn);
        if (!pmn){
            pmn = new CMasternode();
            pmn->vin = txIn;
            pmn->activeState = CMasternode::MASTERNODE_MISSING;
        }
        nodes.insert(QString::fromStdString(mne.getAlias()), std::make_pair(QString::fromStdString(mne.getIp()), pmn));
        if(pwalletMain) {
            bool txAccepted = false;
            {
                LOCK2(cs_main, pwalletMain->cs_wallet);
                const CWalletTx *walletTx = pwalletMain->GetWalletTx(txHash);
                if (walletTx && walletTx->GetDepthInMainChain() > 0) {
                    txAccepted = true;
                }
            }
            collateralTxAccepted.insert(mne.getTxHash(), txAccepted);
        }
    }
    emit dataChanged(index(0, 0, QModelIndex()), index(end, 5, QModelIndex()) );
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
    return 6;
}


QVariant MNModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
            return QVariant();

    // rec could be null, always verify it.
    CMasternode* rec = static_cast<CMasternode*>(index.internalPointer());
    bool isAvailable = rec;
    int row = index.row();
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case ALIAS:
                return nodes.uniqueKeys().value(row);
            case ADDRESS:
                return nodes.values().value(row).first;
            case PUB_KEY:
                return (isAvailable) ? QString::fromStdString(nodes.values().value(row).second->pubKeyMasternode.GetHash().GetHex()) : "Not available";
            case COLLATERAL_ID:
                return (isAvailable) ? QString::fromStdString(rec->vin.prevout.hash.GetHex()) : "Not available";
            case COLLATERAL_OUT_INDEX:
                return (isAvailable) ? QString::number(rec->vin.prevout.n) : "Not available";
            case STATUS: {
                std::pair<QString, CMasternode*> pair = nodes.values().value(row);
                return (pair.second) ? QString::fromStdString(pair.second->Status()) : "MISSING";
            }
            case PRIV_KEY: {
                for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
                    if (mne.getTxHash().compare(rec->vin.prevout.hash.GetHex()) == 0){
                        return QString::fromStdString(mne.getPrivKey());
                    }
                }
                return "Not available";
            }
            case WAS_COLLATERAL_ACCEPTED:{
                if (!isAvailable) return false;
                std::string txHash = rec->vin.prevout.hash.GetHex();
                if(!collateralTxAccepted.value(txHash)){
                    bool txAccepted = false;
                    {
                        LOCK2(cs_main, pwalletMain->cs_wallet);
                        const CWalletTx *walletTx = pwalletMain->GetWalletTx(rec->vin.prevout.hash);
                        txAccepted = walletTx && walletTx->GetDepthInMainChain() > 0;
                    }
                    return txAccepted;
                }
                return true;
            }
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


bool MNModel::removeMn(const QModelIndex& modelIndex) {
    QString alias = modelIndex.data(Qt::DisplayRole).toString();
    int idx = modelIndex.row();
    beginRemoveRows(QModelIndex(), idx, idx);
    nodes.take(alias);
    endRemoveRows();
    emit dataChanged(index(idx, 0, QModelIndex()), index(idx, 5, QModelIndex()) );
    return true;
}

bool MNModel::addMn(CMasternodeConfig::CMasternodeEntry* mne){
    beginInsertRows(QModelIndex(), nodes.size(), nodes.size());
    int nIndex;
    if(!mne->castOutputIndex(nIndex))
        return false;

    CMasternode* pmn = mnodeman.Find(CTxIn(uint256S(mne->getTxHash()), uint32_t(nIndex)));
    nodes.insert(QString::fromStdString(mne->getAlias()), std::make_pair(QString::fromStdString(mne->getIp()), pmn));
    endInsertRows();
    return true;
}