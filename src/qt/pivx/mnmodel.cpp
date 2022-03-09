// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/mnmodel.h"

#include "bls/key_io.h"
#include "coincontrol.h"
#include "interfaces/tiertwo.h"
#include "evo/specialtx_utils.h"
#include "masternode.h"
#include "masternodeman.h"
#include "net.h"        // for validateMasternodeIP
#include "netbase.h"
#include "operationresult.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "uint256.h"
#include "qt/bitcoinunits.h"
#include "qt/optionsmodel.h"
#include "qt/pivx/guitransactionsutils.h"
#include "wallet/wallet.h" // TODO: Move to walletModel
#include "qt/walletmodel.h"
#include "qt/walletmodeltransaction.h"

#include <QFile>
#include <QHostAddress>

uint16_t MasternodeWrapper::getType() const
{
    if (!dmnView) {
        return LEGACY;
    }

    uint16_t type = 0;
    if (dmnView->hasOwnerKey) {
        type |= DMN_OWNER;
    }

    if (dmnView->hasVotingKey) {
        type |= DMN_VOTER;
    }

    // todo: add operator
    return type;
}

MNModel::MNModel(QObject *parent) : QAbstractTableModel(parent) {}

void MNModel::init()
{
    updateMNList();
}

void MNModel::updateMNList()
{
    int mnMinConf = getMasternodeCollateralMinConf();
    nodes.clear();
    collateralTxAccepted.clear();
    for (const CMasternodeConfig::CMasternodeEntry& mne : masternodeConfig.getEntries()) {
        int nIndex;
        if (!mne.castOutputIndex(nIndex)) continue;
        const uint256& txHash = uint256S(mne.getTxHash());
        CTxIn txIn(txHash, uint32_t(nIndex));
        CMasternode* pmn = mnodeman.Find(txIn.prevout);
        nodes.append(MasternodeWrapper(
                QString::fromStdString(mne.getAlias()),
                QString::fromStdString(mne.getIp()),
                pmn,
                pmn ? pmn->vin.prevout : txIn.prevout,
                Optional<QString>(QString::fromStdString(mne.getPubKeyStr())),
                nullptr) // dmn view
        );

        if (walletModel) {
            collateralTxAccepted.insert(mne.getTxHash(), walletModel->getWalletTxDepth(txHash) >= mnMinConf);
        }
    }

    // Now add DMNs
    for (const auto& dmn : interfaces::g_tiertwo->getKnownDMNs()) {
        // Try the owner address as "alias", if not found use the payout script, if not, use the voting address, if not use the service.
        std::string alias;
        if (dmn->hasOwnerKey && dmn->ownerAddrLabel) {
            alias = *dmn->ownerAddrLabel;
        } else if (dmn->hasPayoutScript && dmn->payoutAddrLabel) {
            alias = *dmn->payoutAddrLabel;
        } else if (dmn->hasVotingKey && dmn->votingAddrLabel) {
            alias = *dmn->votingAddrLabel;
        } else if (!dmn->service.empty()) {
            alias = dmn->service;
        } else {
            // future think: could use the proTxHash if no label is found.
            alias = "no alias available";
        }

        nodes.append(MasternodeWrapper(
                QString::fromStdString(alias),
                QString::fromStdString(dmn->service),
                nullptr,
                dmn->collateralOut,
                nullopt,
                dmn));

        if (walletModel) {
            const auto& txHash = dmn->collateralOut.hash;
            collateralTxAccepted.insert(txHash.GetHex(), walletModel->getWalletTxDepth(txHash) >= mnMinConf);
        }
    }

    Q_EMIT dataChanged(index(0, 0, QModelIndex()),
                       index(nodes.size(), ColumnIndex::COLUMN_COUNT, QModelIndex()));
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
    return ColumnIndex::COLUMN_COUNT;
}

static QString formatTooltip(const MasternodeWrapper& wrapper)
{
    return QObject::tr((wrapper.getType() == MNViewType::LEGACY) ?
            "Legacy Masternode\nIt will be disabled after v6.0 enforcement" :
            "Deterministic Masternode");
}

QVariant MNModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    const MasternodeWrapper& mnWrapper = nodes.at(row);
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole: {
        switch (index.column()) {
            case ALIAS:
                return mnWrapper.label;
            case ADDRESS:
                return mnWrapper.ipPort;
            case PUB_KEY:
                return mnWrapper.mnPubKey ? *mnWrapper.mnPubKey : "Not available";
            case COLLATERAL_ID:
                return mnWrapper.collateralId ? QString::fromStdString(mnWrapper.collateralId->hash.GetHex()) : "Not available";
            case COLLATERAL_OUT_INDEX:
                return mnWrapper.collateralId ? QString::number(mnWrapper.collateralId->n) : "Not available";
            case STATUS: {
                std::string status = "MISSING";
                if (mnWrapper.dmnView) {
                    // Deterministic MN
                    status = mnWrapper.dmnView->isPoSeBanned ? "PoSe BANNED" : "ENABLED";
                } else {
                    // Legacy MN
                    if (mnWrapper.masternode) {
                        status = mnWrapper.masternode->Status();
                        // Quick workaround to the current Masternode status types.
                        // If the status is REMOVE and there is no pubkey associated to the Masternode
                        // means that the MN is not in the network list and was created in
                        // updateMNList(). Which.. denotes a not started masternode.
                        // This will change in the future with the MasternodeWrapper introduction.
                        if (status == "REMOVE" && !mnWrapper.masternode->pubKeyCollateralAddress.IsValid()) {
                            return "MISSING";
                        }
                    }
                }
                return QString::fromStdString(status);
            }
            case PRIV_KEY: {
                if (mnWrapper.collateralId) {
                    for (const CMasternodeConfig::CMasternodeEntry& mne : masternodeConfig.getEntries()) {
                        if (mne.getTxHash() == mnWrapper.collateralId->hash.GetHex()) {
                            return QString::fromStdString(mne.getPrivKey());
                        }
                    }
                }
                return "Not available";
            }
            case WAS_COLLATERAL_ACCEPTED:{
                return mnWrapper.collateralId && collateralTxAccepted.value(mnWrapper.collateralId->hash.GetHex());
            }
            case TYPE:{
                return mnWrapper.getType();
            }
            case IS_POSE_ENABLED:{
                return mnWrapper.dmnView && !mnWrapper.dmnView->isPoSeBanned;
            }
            case PRO_TX_HASH:{
                if (mnWrapper.dmnView) return QString::fromStdString(mnWrapper.dmnView->proTxHash.GetHex());
            }
        }
    }
    case Qt::ToolTipRole:
        return formatTooltip(mnWrapper);
    } // end role switch
    return QVariant();
}

bool MNModel::removeMn(const QModelIndex& modelIndex)
{
    int idx = modelIndex.row();
    beginRemoveRows(QModelIndex(), idx, idx);
    auto mnWrapper = nodes.at(idx);
    if (mnWrapper.collateralId) collateralTxAccepted.remove(mnWrapper.collateralId->hash.GetHex());
    nodes.removeAt(idx);
    endRemoveRows();
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, 5, QModelIndex()) );
    return true;
}

bool MNModel::addMn(CMasternodeConfig::CMasternodeEntry* mne)
{
    beginInsertRows(QModelIndex(), nodes.size(), nodes.size());
    int nIndex;
    if (!mne->castOutputIndex(nIndex))
        return false;

    COutPoint collateralId = COutPoint(uint256S(mne->getTxHash()), uint32_t(nIndex));
    CMasternode* pmn = mnodeman.Find(collateralId);
    nodes.append(MasternodeWrapper(
                 QString::fromStdString(mne->getAlias()),
                 QString::fromStdString(mne->getIp()),
                 pmn, pmn ? pmn->vin.prevout : collateralId,
                 Optional<QString>(QString::fromStdString(mne->getPubKeyStr())),
                 nullptr));
    endInsertRows();
    return true;
}

const MasternodeWrapper* MNModel::getMNWrapper(const QString& mnAlias)
{
    for (const auto& it : nodes) {
        if (it.label == mnAlias) {
            return &it;
        }
    }
    return nullptr;
}

int MNModel::getMNState(const QString& mnAlias)
{
    const MasternodeWrapper* mn = getMNWrapper(mnAlias);
    if (!mn) {
        throw std::runtime_error(std::string("Masternode alias not found"));
    }
    return mn->masternode ? mn->masternode->GetActiveState() : -1;
}

bool MNModel::isMNInactive(const QString& mnAlias)
{
    int activeState = getMNState(mnAlias);
    return activeState == CMasternode::MASTERNODE_EXPIRED || activeState == CMasternode::MASTERNODE_REMOVE;
}

bool MNModel::isMNActive(const QString& mnAlias)
{
    int activeState = getMNState(mnAlias);
    return activeState == CMasternode::MASTERNODE_PRE_ENABLED || activeState == CMasternode::MASTERNODE_ENABLED;
}

bool MNModel::isMNCollateralMature(const QString& mnAlias)
{
    const MasternodeWrapper* mn = getMNWrapper(mnAlias);
    if (!mn) {
        throw std::runtime_error(std::string("Masternode alias not found"));
    }
    return mn->collateralId && collateralTxAccepted.value(mn->collateralId->hash.GetHex());
}

bool MNModel::isLegacySystemObsolete()
{
    return interfaces::g_tiertwo->isLegacySystemObsolete();
}

bool MNModel::isMNsNetworkSynced()
{
    return g_tiertwo_sync_state.IsSynced();
}

bool MNModel::validateMNIP(const QString& addrStr)
{
    return validateMasternodeIP(addrStr.toStdString());
}

CAmount MNModel::getMNCollateralRequiredAmount()
{
    return Params().GetConsensus().nMNCollateralAmt;
}

int MNModel::getMasternodeCollateralMinConf()
{
    return Params().GetConsensus().MasternodeCollateralMinConf();
}

// Add here only the errors that the user could face
std::string translateRejectionError(const std::string& rejection)
{
    if (rejection == "bad-protx-ipaddr-port") {
        return _("Invalid service IP address");
    } else if (rejection == "bad-protx-dup-IP-address") {
        return _("The provided service IP address is already in use by another registered Masternode");
    }
    return rejection;
}

static CallResult<uint256> createDMNInternal(const COutPoint& collateral,
                                             const CKey& keyCollateral,
                                             const CService& service,
                                             const CKeyID& ownerAddr,
                                             const CBLSPublicKey& operatorPubKey,
                                             const Optional<CKeyID>& votingAddr,
                                             const CKeyID& payoutAddr,
                                             const Optional<CBLSSecretKey>& operatorSk,
                                             const Optional<uint16_t>& operatorPercentage,
                                             const Optional<CKeyID>& operatorPayoutAddr)
{
    ProRegPL pl;
    pl.nVersion = ProRegPL::CURRENT_VERSION;
    pl.addr = service;
    pl.keyIDOwner = ownerAddr;
    pl.pubKeyOperator = operatorPubKey;
    pl.keyIDVoting = votingAddr ? *votingAddr : pl.keyIDOwner;
    pl.collateralOutpoint = collateral;
    pl.scriptPayout = GetScriptForDestination(payoutAddr);
    if (operatorPayoutAddr) {
        pl.nOperatorReward = *operatorPercentage;
        pl.scriptOperatorPayout = GetScriptForDestination(*operatorPayoutAddr);
    }
    // make sure fee calculation works
    pl.vchSig.resize(CPubKey::COMPACT_SIGNATURE_SIZE);

    CMutableTransaction tx;
    tx.nVersion = CTransaction::TxVersion::SAPLING;
    tx.nType = CTransaction::TxType::PROREG;

    auto wallet = vpwallets[0]; // TODO: Move to walletModel
    auto res = FundSpecialTx(wallet, tx, pl);
    if (!res) return {res.getError()};

    res = SignSpecialTxPayloadByString(pl, keyCollateral);
    if (!res) return {res.getError()};

    std::map<std::string, std::string> extraValues;
    if (operatorSk) {
        // Only if the operator sk was provided
        extraValues.emplace("operatorSk", bls::EncodeSecret(Params(), *operatorSk));
    }
    res = SignAndSendSpecialTx(wallet, tx, pl, &extraValues);
    return res ? CallResult<uint256>(tx.GetHash()) :
            CallResult<uint256>(translateRejectionError(res.getError()));
}

CallResult<uint256> MNModel::createDMN(const std::string& alias,
                                       const COutPoint& collateral,
                                       std::string& serviceAddr,
                                       const std::string& servicePort,
                                       const CKeyID& ownerAddr,
                                       const Optional<std::string>& operatorPubKey,
                                       const Optional<CKeyID>& votingAddr,
                                       const CKeyID& payoutKeyId,
                                       std::string& strError,
                                       const Optional<uint16_t>& operatorPercentage,
                                       const Optional<CKeyID>& operatorPayoutAddr)
{
    // Parse and validate inputs

    // Different DMN creation types:
    // 1. internal.
    // 2. external.
    // 3. fund.

    auto p_wallet = vpwallets[0]; // TODO: Move to walletModel
    const auto& chainparams = Params();

    // 1) Create the simplest DMN, the collateral was generated by this wallet.

    CService service;
    if (!serviceAddr.empty()) {
        if (!Lookup(serviceAddr+":"+servicePort, service, chainparams.GetDefaultPort(), false)) {
            strError = strprintf("invalid network address %s", serviceAddr);
            return {strError};
        }
    }

    CPubKey pubKeyCollateral;
    CKey keyCollateral;
    if (!p_wallet->GetMasternodeVinAndKeys(pubKeyCollateral, keyCollateral, collateral, false, strError)) {
        return {strError};
    }

    // parse operator pubkey or create one
    Optional<CBLSSecretKey> operatorSk{nullopt};
    CBLSPublicKey operatorPk;
    if (operatorPubKey) {
        auto opPk = bls::DecodePublic(Params(), *operatorPubKey);
        if (!opPk || !opPk->IsValid()) {
            strError = "invalid operator pubkey";
            return {strError};
        }
        operatorPk = *opPk;
    } else {
        // Stored within the register tx
        operatorSk = CBLSSecretKey();
        operatorSk->MakeNewKey();
        operatorPk = operatorSk->GetPublicKey();
    }

    auto res = createDMNInternal(collateral,
                                 keyCollateral,
                                 service,
                                 ownerAddr,
                                 operatorPk,
                                 votingAddr, // voting key
                                 payoutKeyId, // payout script
                                 operatorSk, // only if the operator was provided (or locally created)
                                 operatorPercentage,   // operator percentage
                                 operatorPayoutAddr);  // operator payout keyid
    if (!res) {
        strError = res.getError();
        return {strError};
    }

    // All good
    return res;
}

OperationResult MNModel::killDMN(const uint256& collateralHash, unsigned int outIndex)
{
    auto p_wallet = vpwallets[0]; // TODO: Move to walletModel
    const auto& tx = p_wallet->GetWalletTx(collateralHash);
    if (!tx || outIndex >= tx->tx->vout.size()) return {false, "collateral not found"};
    const auto& output = tx->tx->vout[outIndex];

    COutPoint collateral_output(collateralHash, outIndex);
    CCoinControl coinControl;
    coinControl.Select(collateral_output);
    QList<SendCoinsRecipient> recipients;
    auto ownAddr = walletModel->getNewAddress("");
    if (!ownAddr) return {false, ownAddr.getError()};
    CAmount amountToSend = output.nValue - CWallet::minTxFee.GetFeePerK();
    recipients.push_back(SendCoinsRecipient{QString::fromStdString(ownAddr.getObjResult()->ToString()), "", amountToSend, ""});
    WalletModelTransaction currentTransaction(recipients);
    walletModel->unlockCoin(collateral_output);
    WalletModel::SendCoinsReturn prepareStatus = walletModel->prepareTransaction(&currentTransaction, &coinControl, false);

    CClientUIInterface::MessageBoxFlags informType;
    QString returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(
            prepareStatus,
            walletModel,
            informType, // this flag is not needed
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                         currentTransaction.getTransactionFee()),
            true
    );

    if (prepareStatus.status != WalletModel::OK) {
        walletModel->lockCoin(collateral_output);
        return {false, returnMsg.toStdString()};
    }

    WalletModel::SendCoinsReturn sendStatus = walletModel->sendCoins(currentTransaction);
    returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(sendStatus, walletModel, informType);
    if (sendStatus.status != WalletModel::OK) {
        walletModel->lockCoin(collateral_output);
        return {false, returnMsg.toStdString()};
    }

    return {true};
}

bool MNModel::createMNCollateral(
        const QString& alias,
        const QString& addr,
        COutPoint& ret_outpoint,
        QString& ret_error)
{
    SendCoinsRecipient sendCoinsRecipient(addr, alias, getMNCollateralRequiredAmount(), "");

    // Send the 10 tx to one of your address
    QList<SendCoinsRecipient> recipients;
    recipients.append(sendCoinsRecipient);
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    // no coincontrol, no P2CS delegations
    prepareStatus = walletModel->prepareTransaction(&currentTransaction, nullptr, false);

    QString returnMsg = tr("Unknown error");
    // process prepareStatus and on error generate message shown to user
    CClientUIInterface::MessageBoxFlags informType;
    returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(
            prepareStatus,
            walletModel,
            informType, // this flag is not needed
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                         currentTransaction.getTransactionFee()),
            true
    );

    if (prepareStatus.status != WalletModel::OK) {
        ret_error = tr("Prepare master node failed.\n\n%1\n").arg(returnMsg);
        return false;
    }

    WalletModel::SendCoinsReturn sendStatus = walletModel->sendCoins(currentTransaction);
    // process sendStatus and on error generate message shown to user
    returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(sendStatus, walletModel, informType);

    if (sendStatus.status != WalletModel::OK) {
        ret_error = tr("Cannot send collateral transaction.\n\n%1").arg(returnMsg);
        return false;
    }

    // look for the tx index of the collateral
    CTransactionRef walletTx = currentTransaction.getTransaction();
    std::string txID = walletTx->GetHash().GetHex();
    int indexOut = -1;
    for (int i=0; i < (int)walletTx->vout.size(); i++) {
        const CTxOut& out = walletTx->vout[i];
        if (out.nValue == getMNCollateralRequiredAmount()) {
            indexOut = i;
            break;
        }
    }
    if (indexOut == -1) {
        ret_error = tr("Invalid collateral output index");
        return false;
    }
    // save the collateral outpoint
    ret_outpoint = COutPoint(walletTx->GetHash(), indexOut);
    return true;
}

bool MNModel::startLegacyMN(const CMasternodeConfig::CMasternodeEntry& mne, int chainHeight, std::string& strError)
{
    CMasternodeBroadcast mnb;
    if (!CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb, false, chainHeight))
        return false;

    mnodeman.UpdateMasternodeList(mnb);
    if (activeMasternode.pubKeyMasternode == mnb.GetPubKey()) {
        activeMasternode.EnableHotColdMasterNode(mnb.vin, mnb.addr);
    }
    mnb.Relay();
    return true;
}

void MNModel::startAllLegacyMNs(bool onlyMissing, int& amountOfMnFailed, int& amountOfMnStarted,
                                std::string* aliasFilter, std::string* error_ret)
{
    for (const auto& mne : masternodeConfig.getEntries()) {
        if (!aliasFilter) {
            // Check for missing only
            QString mnAlias = QString::fromStdString(mne.getAlias());
            if (onlyMissing && !isMNInactive(mnAlias)) {
                if (!isMNActive(mnAlias))
                    amountOfMnFailed++;
                continue;
            }

            if (!isMNCollateralMature(mnAlias)) {
                amountOfMnFailed++;
                continue;
            }
        } else if (*aliasFilter != mne.getAlias()){
            continue;
        }

        std::string ret_str;
        if (!startLegacyMN(mne, walletModel->getLastBlockProcessedNum(), ret_str)) {
            amountOfMnFailed++;
            if (error_ret) *error_ret = ret_str;
        } else {
            amountOfMnStarted++;
        }
    }
}

// Future: remove after v6.0
CMasternodeConfig::CMasternodeEntry* MNModel::createLegacyMN(COutPoint& collateralOut,
                             const std::string& alias,
                             std::string& serviceAddr,
                             const std::string& port,
                             const std::string& mnKeyString,
                             const std::string& mnPubKeyStr,
                             QString& ret_error)
{
    // Update the conf file
    QString strConfFileQt(PIVX_MASTERNODE_CONF_FILENAME);
    std::string strConfFile = strConfFileQt.toStdString();
    std::string strDataDir = GetDataDir().string();
    fs::path conf_file_path(strConfFile);
    if (strConfFile != conf_file_path.filename().string()) {
        throw std::runtime_error(strprintf(_("%s %s resides outside data directory %s"), strConfFile, strConfFile, strDataDir));
    }

    fs::path pathBootstrap = GetDataDir() / strConfFile;
    if (!fs::exists(pathBootstrap)) {
        ret_error = tr("%1 file doesn't exists").arg(strConfFileQt);
        return nullptr;
    }

    fs::path pathMasternodeConfigFile = GetMasternodeConfigFile();
    fsbridge::ifstream streamConfig(pathMasternodeConfigFile);

    if (!streamConfig.good()) {
        ret_error = tr("Invalid %1 file").arg(strConfFileQt);
        return nullptr;
    }

    int linenumber = 1;
    std::string lineCopy;
    for (std::string line; std::getline(streamConfig, line); linenumber++) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string comment, alias, ip, privKey, txHash, outputIndex;

        if (iss >> comment) {
            if (comment.at(0) == '#') continue;
            iss.str(line);
            iss.clear();
        }

        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                streamConfig.close();
                ret_error = tr("Error parsing %1 file").arg(strConfFileQt);
                return nullptr;
            }
        }
        lineCopy += line + "\n";
    }

    if (lineCopy.empty()) {
        lineCopy = "# Masternode config file\n"
                   "# Format: alias IP:port masternodeprivkey collateral_output_txid collateral_output_index\n"
                   "# Example: mn1 127.0.0.2:51472 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0"
                   "#";
    }
    lineCopy += "\n";

    streamConfig.close();

    std::string txID = collateralOut.hash.ToString();
    std::string indexOutStr = std::to_string(collateralOut.n);

    // Check IP address type
    QHostAddress hostAddress(QString::fromStdString(serviceAddr));
    QAbstractSocket::NetworkLayerProtocol layerProtocol = hostAddress.protocol();
    if (layerProtocol == QAbstractSocket::IPv6Protocol) {
        serviceAddr = "["+serviceAddr+"]";
    }

    fs::path pathConfigFile = AbsPathForConfigVal(fs::path("masternode_temp.conf"));
    FILE* configFile = fopen(pathConfigFile.string().c_str(), "w");
    lineCopy += alias+" "+serviceAddr+":"+port+" "+mnKeyString+" "+txID+" "+indexOutStr+"\n";
    fwrite(lineCopy.c_str(), std::strlen(lineCopy.c_str()), 1, configFile);
    fclose(configFile);

    fs::path pathOldConfFile = AbsPathForConfigVal(fs::path("old_masternode.conf"));
    if (fs::exists(pathOldConfFile)) {
        fs::remove(pathOldConfFile);
    }
    rename(pathMasternodeConfigFile, pathOldConfFile);

    fs::path pathNewConfFile = AbsPathForConfigVal(fs::path(strConfFile));
    rename(pathConfigFile, pathNewConfFile);

    auto ret_mn_entry = masternodeConfig.add(alias, serviceAddr+":"+port, mnKeyString, mnPubKeyStr, txID, indexOutStr);

    // Lock collateral output
    walletModel->lockCoin(collateralOut);
    return ret_mn_entry;
}

// Future: remove after v6.0
bool MNModel::removeLegacyMN(const std::string& alias_to_remove, const std::string& tx_id, unsigned int out_index, QString& ret_error)
{
    QString strConfFileQt(PIVX_MASTERNODE_CONF_FILENAME);
    std::string strConfFile = strConfFileQt.toStdString();
    std::string strDataDir = GetDataDir().string();
    fs::path conf_file_path(strConfFile);
    if (strConfFile != conf_file_path.filename().string()) {
        throw std::runtime_error(strprintf(_("%s %s resides outside data directory %s"), strConfFile, strConfFile, strDataDir));
    }

    fs::path pathBootstrap = GetDataDir() / strConfFile;
    if (!fs::exists(pathBootstrap)) {
        ret_error = tr("%1 file doesn't exists").arg(strConfFileQt);
        return false;
    }

    fs::path pathMasternodeConfigFile = GetMasternodeConfigFile();
    fsbridge::ifstream streamConfig(pathMasternodeConfigFile);

    if (!streamConfig.good()) {
        ret_error = tr("Invalid %1 file").arg(strConfFileQt);
        return false;
    }

    int lineNumToRemove = -1;
    int linenumber = 1;
    std::string lineCopy;
    for (std::string line; std::getline(streamConfig, line); linenumber++) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string comment, alias, ip, privKey, txHash, outputIndex;

        if (iss >> comment) {
            if (comment.at(0) == '#') continue;
            iss.str(line);
            iss.clear();
        }

        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                streamConfig.close();
                ret_error = tr("Error parsing %1 file").arg(strConfFileQt);
                return false;
            }
        }

        if (alias_to_remove == alias) {
            lineNumToRemove = linenumber;
        } else
            lineCopy += line + "\n";

    }

    if (lineCopy.empty()) {
        lineCopy = "# Masternode config file\n"
                   "# Format: alias IP:port masternodeprivkey collateral_output_txid collateral_output_index\n"
                   "# Example: mn1 127.0.0.2:51472 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0\n";
    }

    streamConfig.close();

    if (lineNumToRemove == -1) {
        ret_error = tr("MN alias %1 not found in %2 file").arg(QString::fromStdString(alias_to_remove)).arg(strConfFileQt);
        return false;
    }

    // Update file
    fs::path pathConfigFile = AbsPathForConfigVal(fs::path("masternode_temp.conf"));
    FILE* configFile = fsbridge::fopen(pathConfigFile, "w");
    fwrite(lineCopy.c_str(), std::strlen(lineCopy.c_str()), 1, configFile);
    fclose(configFile);

    fs::path pathOldConfFile = AbsPathForConfigVal(fs::path("old_masternode.conf"));
    if (fs::exists(pathOldConfFile)) {
        fs::remove(pathOldConfFile);
    }
    rename(pathMasternodeConfigFile, pathOldConfFile);

    fs::path pathNewConfFile = AbsPathForConfigVal(fs::path(strConfFile));
    rename(pathConfigFile, pathNewConfFile);

    // Unlock collateral
    COutPoint collateralOut(uint256S(tx_id), out_index);
    walletModel->unlockCoin(collateralOut);
    // Remove alias
    masternodeConfig.remove(alias_to_remove);
    return true;
}
