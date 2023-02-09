// Copyright (c) 2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "interfaces/tiertwo.h"

#include "bls/key_io.h"
#include "evo/deterministicmns.h"
#include "optional.h"
#include "netbase.h"
#include "evo/specialtx_validation.h" // For CheckService
#include "validation.h"
#include "wallet/wallet.h"

namespace interfaces {

std::unique_ptr<TierTwo> g_tiertwo;

bool TierTwo::isLegacySystemObsolete()
{
    return deterministicMNManager->LegacyMNObsolete();
}

bool TierTwo::isBlsPubKeyValid(const std::string& blsKey)
{
    auto opKey = bls::DecodePublic(Params(), blsKey);
    return opKey && opKey->IsValid();
}

OperationResult TierTwo::isServiceValid(const std::string& serviceStr)
{
    if (serviceStr.empty()) return false;
    const auto& params = Params();
    CService service;
    if (!Lookup(serviceStr, service, params.GetDefaultPort(), false)) {
        return {false, strprintf("invalid network address %s", serviceStr)};
    }

    CValidationState state;
    if (!CheckService(service, state)) {
        return {false, state.GetRejectReason()};
    }
    // All good
    return {true};
}

Optional<DMNData> TierTwo::getDMNData(const uint256& pro_tx_hash, const CBlockIndex* tip)
{
    if (!tip) return nullopt;
    const auto& params = Params();
    CDeterministicMNCPtr ptr_mn = deterministicMNManager->GetListForBlock(tip).GetMN(pro_tx_hash);
    if (!ptr_mn) return nullopt;
    DMNData data;
    data.ownerMainAddr = EncodeDestination(ptr_mn->pdmnState->keyIDOwner);
    data.ownerPayoutAddr = EncodeDestination(ptr_mn->pdmnState->scriptPayout);
    data.operatorPk = bls::EncodePublic(params, ptr_mn->pdmnState->pubKeyOperator.Get());
    data.operatorPayoutAddr = EncodeDestination(ptr_mn->pdmnState->scriptOperatorPayout);
    data.operatorPayoutPercentage = ptr_mn->nOperatorReward;
    data.votingAddr = EncodeDestination(ptr_mn->pdmnState->keyIDVoting);
    if (!vpwallets.empty()) {
        CWallet* p_wallet = vpwallets[0];
        data.operatorSk = p_wallet->GetStrFromTxExtraData(pro_tx_hash, "operatorSk");
    }
    return {data};
}

std::shared_ptr<DMNView> createDMNViewIfMine(CWallet* pwallet, const CDeterministicMNCPtr& dmn)
{
    bool hasOwnerKey;
    bool hasVotingKey;
    bool hasPayoutScript;
    Optional<std::string> opOwnerLabel{nullopt};
    Optional<std::string> opVotingLabel{nullopt};
    Optional<std::string> opPayoutLabel{nullopt};
    {
        LOCK(pwallet->cs_wallet);
        hasOwnerKey = pwallet->HaveKey(dmn->pdmnState->keyIDOwner);
        hasVotingKey = pwallet->HaveKey(dmn->pdmnState->keyIDVoting);

        CTxDestination dest;
        if (ExtractDestination(dmn->pdmnState->scriptPayout, dest)) {
            if (auto payoutId = boost::get<CKeyID>(&dest)) {
                hasPayoutScript = pwallet->HaveKey(*payoutId);
                auto payoutLabel = pwallet->GetNameForAddressBookEntry(*payoutId);
                if (!payoutLabel.empty()) opPayoutLabel = payoutLabel;
            }
        }

        auto ownerLabel = pwallet->GetNameForAddressBookEntry(dmn->pdmnState->keyIDOwner);
        if (!ownerLabel.empty()) opOwnerLabel = ownerLabel;

        auto votingLabel = pwallet->GetNameForAddressBookEntry(dmn->pdmnState->keyIDVoting);
        if (!votingLabel.empty()) opVotingLabel = votingLabel;
    }
    if (!hasOwnerKey && !hasVotingKey) return nullptr;

    DMNView dmnView;
    dmnView.id = dmn->GetInternalId();
    dmnView.proTxHash = dmn->proTxHash;
    dmnView.hasOwnerKey = hasOwnerKey;
    dmnView.hasVotingKey = hasVotingKey;
    dmnView.hasPayoutScript = hasPayoutScript;
    dmnView.ownerAddrLabel = opOwnerLabel;
    dmnView.votingAddrLabel = opVotingLabel;
    dmnView.payoutAddrLabel = opPayoutLabel;
    dmnView.isPoSeBanned = dmn->IsPoSeBanned();
    dmnView.service = dmn->pdmnState->addr.IsValid() ? dmn->pdmnState->addr.ToStringIPPort() : "";
    dmnView.collateralOut = dmn->collateralOutpoint;
    return std::make_shared<DMNView>(dmnView);
}

void TierTwo::refreshCache(const CDeterministicMNList& mnList)
{
    if (vpwallets.empty()) return;
    CWallet* pwallet = vpwallets[0];
    std::vector<std::shared_ptr<DMNView>> vec_dmns;
    mnList.ForEachMN(false, [pwallet, &vec_dmns](const CDeterministicMNCPtr& dmn) {
        auto opDMN = createDMNViewIfMine(pwallet, dmn);
        if (opDMN) vec_dmns.emplace_back(opDMN);
    });

    LOCK(cs_cache);
    m_cached_dmns = vec_dmns;
    m_last_block_cached = mnList.GetBlockHash();
}

void TierTwo::init()
{
    // Init the DMNs cache
    refreshCache(deterministicMNManager->GetListAtChainTip());
}

void TierTwo::NotifyMasternodeListChanged(bool undo, const CDeterministicMNList& oldMNList, const CDeterministicMNListDiff& diff)
{
    if (vpwallets.empty()) return;
    // Refresh cache if reorg occurred
    if (WITH_LOCK(cs_cache, return m_last_block_cached) != oldMNList.GetBlockHash()) {
        refreshCache(oldMNList);
    }

    CWallet* pwallet = vpwallets[0];
    LOCK (cs_cache);

    // Remove dmns
    for (const auto& removed : diff.removedMns) {
        auto it = m_cached_dmns.begin();
        while (it != m_cached_dmns.end()) {
            if (it->get()->id == removed) it = m_cached_dmns.erase(it);
            else it++;
        }
    }

    // Add dmns
    for (const auto& add : diff.addedMNs) {
        auto opDMN = createDMNViewIfMine(pwallet, add);
        if (opDMN) m_cached_dmns.emplace_back(opDMN);
    }

    // TODO: updated DMNs.

    // Update cached hash
    m_last_block_cached = diff.blockHash;
}

} // end namespace interfaces