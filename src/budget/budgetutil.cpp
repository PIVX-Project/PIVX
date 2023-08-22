// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "budget/budgetutil.h"

#include "budget/budgetmanager.h"
#include "gamemasterman.h"
#include "gamemasterconfig.h"
#include "util/validation.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h" // future: use interface instead.
#endif


static UniValue packRetStatus(const std::string& nodeType, const std::string& result, const std::string& error)
{
    UniValue statusObj(UniValue::VOBJ);
    statusObj.pushKV("node", nodeType);
    statusObj.pushKV("result", result);
    statusObj.pushKV("error", error);
    return statusObj;
}

static UniValue packErrorRetStatus(const std::string& nodeType, const std::string& error)
{
    return packRetStatus(nodeType, "failed", error);
}

static UniValue packVoteReturnValue(const UniValue& details, int success, int failed)
{
    UniValue returnObj(UniValue::VOBJ);
    returnObj.pushKV("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed));
    returnObj.pushKV("detail", details);
    return returnObj;
}

// key, alias and collateral outpoint of a gamemaster. Struct used to sign proposal/budget votes
struct GmKeyData
{
    std::string gmAlias;
    const COutPoint* collateralOut;

    GmKeyData() = delete;
    GmKeyData(const std::string& _gmAlias, const COutPoint* _collateralOut, const CKey& _key):
        gmAlias(_gmAlias),
        collateralOut(_collateralOut),
        key(_key),
        use_bls(false)
    {}
    GmKeyData(const std::string& _gmAlias, const COutPoint* _collateralOut, const CBLSSecretKey& _key):
        gmAlias(_gmAlias),
        collateralOut(_collateralOut),
        blsKey(_key),
        use_bls(true)
    {}

    bool Sign(CSignedMessage* msg) const
    {
        return use_bls ? msg->Sign(blsKey)
                       : msg->Sign(key, key.GetPubKey().GetID());
    }

private:
    CKey key;
    CBLSSecretKey blsKey;
    bool use_bls;   // whether to use a CKey (mbv) or blsKey (fbv, gmw) to sign
};

typedef std::list<GmKeyData> gmKeyList;

static UniValue voteProposal(const uint256& propHash, const CBudgetVote::VoteDirection& nVote,
                             const gmKeyList& gmKeys, UniValue resultsObj, int failed)
{
    int success = 0;
    for (const auto& k : gmKeys) {
        CBudgetVote vote(CTxIn(*k.collateralOut), propHash, nVote);
        if (!k.Sign(&vote)) {
            resultsObj.push_back(packErrorRetStatus(k.gmAlias, "Failure to sign."));
            failed++;
            continue;
        }
        CValidationState state;
        if (!g_budgetman.ProcessProposalVote(vote, nullptr, state)) {
            resultsObj.push_back(packErrorRetStatus(k.gmAlias, FormatStateMessage(state)));
            failed++;
            continue;
        }
        resultsObj.push_back(packRetStatus(k.gmAlias, "success", ""));
        success++;
    }

    return packVoteReturnValue(resultsObj, success, failed);
}

static UniValue voteFinalBudget(const uint256& budgetHash,
                                const gmKeyList& gmKeys, UniValue resultsObj, int failed)
{
    int success = 0;
    for (const auto& k : gmKeys) {
        CFinalizedBudgetVote vote(CTxIn(*k.collateralOut), budgetHash);
        if (!k.Sign(&vote)) {
            resultsObj.push_back(packErrorRetStatus(k.gmAlias, "Failure to sign."));
            failed++;
            continue;
        }
        CValidationState state;
        if (!g_budgetman.ProcessFinalizedBudgetVote(vote, nullptr, state)) {
            resultsObj.push_back(packErrorRetStatus(k.gmAlias, FormatStateMessage(state)));
            failed++;
            continue;
        }
        resultsObj.push_back(packRetStatus(k.gmAlias, "success", ""));
        success++;
    }

    return packVoteReturnValue(resultsObj, success, failed);
}

// Legacy gamemasters
static gmKeyList getGMKeys(const Optional<std::string>& gmAliasFilter,
                           UniValue& resultsObj, int& failed)
{
    gmKeyList gmKeys;
    for (const CGamemasterConfig::CGamemasterEntry& gme : gamemasterConfig.getEntries()) {
        if (gmAliasFilter && *gmAliasFilter != gme.getAlias()) continue;
        CKey gmKey; CPubKey gmPubKey;
        const std::string& gmAlias = gme.getAlias();
        if (!CMessageSigner::GetKeysFromSecret(gme.getPrivKey(), gmKey, gmPubKey)) {
            resultsObj.push_back(packErrorRetStatus(gmAlias, "Could not get key from gamemaster.conf"));
            failed++;
            continue;
        }
        CGamemaster* pgm = gamemasterman.Find(gmPubKey);
        if (!pgm) {
            resultsObj.push_back(packErrorRetStatus(gmAlias, "Can't find gamemaster by pubkey"));
            failed++;
            continue;
        }
        gmKeys.emplace_back(gmAlias, &pgm->vin.prevout, gmKey);
    }
    return gmKeys;
}

static gmKeyList getGMKeysForActiveGamemaster(UniValue& resultsObj)
{
    // local node must be a gamemaster
    if (!fGameMaster) {
        throw std::runtime_error(_("This is not a gamemaster. 'local' option disabled."));
    }

    if (activeGamemaster.vin == nullopt) {
        throw std::runtime_error(_("Active Gamemaster not initialized."));
    }

    CKey gmKey; CPubKey gmPubKey;
    activeGamemaster.GetKeys(gmKey, gmPubKey);
    CGamemaster* pgm = gamemasterman.Find(gmPubKey);
    if (!pgm) {
        resultsObj.push_back(packErrorRetStatus("local", "Can't find gamemaster by pubkey"));
        return gmKeyList();
    }

    return {GmKeyData("local", &pgm->vin.prevout, gmKey)};
}

// Deterministic gamemasters
static gmKeyList getDGMVotingKeys(CWallet* const pwallet, const Optional<std::string>& gmAliasFilter, bool fFinal, UniValue& resultsObj, int& failed)
{
    if (!pwallet) {
        throw std::runtime_error( "Wallet (with voting key) not found.");
    }

    auto gmList = deterministicGMManager->GetListAtChainTip();

    CDeterministicGMCPtr gmFilter{nullptr};
    if (gmAliasFilter) {
        // vote with a single gamemaster (identified by ProTx)
        const uint256& proTxHash = uint256S(*gmAliasFilter);
        gmFilter = gmList.GetValidGM(proTxHash);
        if (!gmFilter) {
            resultsObj.push_back(packErrorRetStatus(*gmAliasFilter, "Invalid or unknown proTxHash"));
            failed++;
            return {};
        }
    }

    gmKeyList gmKeys;
    gmList.ForEachGM(true, [&](const CDeterministicGMCPtr& dgm) {
        bool filtered = gmFilter && dgm->proTxHash == gmFilter->proTxHash;
        if (!gmFilter || filtered) {
            if (fFinal) {
                // We should never get here. BLS operator key (for active gm) is needed.
                throw std::runtime_error("Finalized budget voting is allowed only locally, from the gamemaster");
            }
            // Get voting key from the wallet
            LOCK(pwallet->cs_wallet);
            CKey gmKey;
            if (pwallet->GetKey(dgm->pdgmState->keyIDVoting, gmKey)) {
                gmKeys.emplace_back(dgm->proTxHash.ToString(), &dgm->collateralOutpoint, gmKey);
            } else if (filtered) {
                resultsObj.push_back(packErrorRetStatus(*gmAliasFilter, strprintf(
                                        "Private key for voting address %s not known by this wallet",
                                        EncodeDestination(dgm->pdgmState->keyIDVoting)))
                                    );
                failed++;
            }
        }
    });

    return gmKeys;
}

static gmKeyList getDGMKeysForActiveGamemaster(UniValue& resultsObj)
{
    // local node must be a gamemaster
    if (!activeGamemasterManager) {
        throw std::runtime_error(_("This is not a deterministic gamemaster. 'local' option disabled."));
    }

    CBLSSecretKey sk; CDeterministicGMCPtr dgm;
    auto res = activeGamemasterManager->GetOperatorKey(sk, dgm);
    if (!res) {
        resultsObj.push_back(packErrorRetStatus("local", res.getError()));
        return {};
    }

    return {GmKeyData("local", &dgm->collateralOutpoint, sk)};
}

// vote on proposal (finalized budget, if fFinal=true) with all possible keys or a single gm (gmAliasFilter)
// Note: for DGMs only proposal voting is allowed with the voting key
// (finalized budget voting requires the operator BLS key)
UniValue gmBudgetVoteInner(CWallet* const pwallet, bool fLegacyGM, const uint256& budgetHash, bool fFinal,
                                  const CBudgetVote::VoteDirection& nVote, const Optional<std::string>& gmAliasFilter)
{
    if (fFinal && !fLegacyGM) {
        throw std::runtime_error("Finalized budget voting is allowed only locally, from the gamemaster");
    }
    UniValue resultsObj(UniValue::VARR);
    int failed = 0;

    gmKeyList gmKeys = fLegacyGM ? getGMKeys(gmAliasFilter, resultsObj, failed)
                                 : getDGMVotingKeys(pwallet, gmAliasFilter, fFinal, resultsObj, failed);

    if (gmKeys.empty()) {
        return packVoteReturnValue(resultsObj, 0, failed);
    }

    return (fFinal ? voteFinalBudget(budgetHash, gmKeys, resultsObj, failed)
                   : voteProposal(budgetHash, nVote, gmKeys, resultsObj, failed));
}

// vote on proposal (finalized budget, if fFinal=true) with the active local gamemaster
// Note: for DGMs only finalized budget voting is allowed with the operator key
// (proposal voting requires the voting key)
UniValue gmLocalBudgetVoteInner(bool fLegacyGM, const uint256& budgetHash, bool fFinal,
                                       const CBudgetVote::VoteDirection& nVote)
{
    UniValue resultsObj(UniValue::VARR);

    gmKeyList gmKeys = fLegacyGM ? getGMKeysForActiveGamemaster(resultsObj)
                                 : getDGMKeysForActiveGamemaster(resultsObj);

    if (gmKeys.empty()) {
        return packVoteReturnValue(resultsObj, 0, 1);
    }

    return (fFinal ? voteFinalBudget(budgetHash, gmKeys, resultsObj, 0)
                   : voteProposal(budgetHash, nVote, gmKeys, resultsObj, 0));
}