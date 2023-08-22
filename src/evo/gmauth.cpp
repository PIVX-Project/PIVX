// Copyright (c) 2019-2021 The Dash Core developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "evo/gmauth.h"

#include "activegamemaster.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "net.h" // for CSerializedNetMsg
#include "netmessagemaker.h"
#include "llmq/quorums_connections.h"
#include "tiertwo/gamemaster_meta_manager.h"
#include "tiertwo/net_gamemasters.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "util/system.h" // for fGamemaster and gArgs access

#include "version.h" // for GMAUTH_NODE_VER_VERSION

void CGMAuth::PushGMAUTH(CNode* pnode, CConnman& connman)
{
    const CActiveGamemasterInfo* activeGmInfo{nullptr};
    if (!fGameMaster || !activeGamemasterManager ||
        (activeGmInfo = activeGamemasterManager->GetInfo())->proTxHash.IsNull()) {
        return;
    }

    uint256 signHash;
    {
        LOCK(pnode->cs_gmauth);
        if (pnode->receivedGMAuthChallenge.IsNull()) {
            return;
        }
        // We include fInbound in signHash to forbid interchanging of challenges by a man in the middle (MITM). This way
        // we protect ourselves against MITM in this form:
        //   node1 <- Eve -> node2
        // It does not protect against:
        //   node1 -> Eve -> node2
        // This is ok as we only use GMAUTH as a DoS protection and not for sensitive stuff
        int nOurNodeVersion{PROTOCOL_VERSION};
        if (Params().NetworkIDString() != CBaseChainParams::MAIN && gArgs.IsArgSet("-pushversion")) {
            nOurNodeVersion = (int)gArgs.GetArg("-pushversion", PROTOCOL_VERSION);
        }
        if (pnode->nVersion < GMAUTH_NODE_VER_VERSION || nOurNodeVersion < GMAUTH_NODE_VER_VERSION) {
            signHash = ::SerializeHash(std::make_tuple(activeGmInfo->pubKeyOperator, pnode->receivedGMAuthChallenge, pnode->fInbound));
        } else {
            signHash = ::SerializeHash(std::make_tuple(activeGmInfo->pubKeyOperator, pnode->receivedGMAuthChallenge, pnode->fInbound, nOurNodeVersion));
        }
    }

    CGMAuth gmauth;
    gmauth.proRegTxHash = activeGmInfo->proTxHash;
    gmauth.sig = activeGmInfo->keyOperator.Sign(signHash);

    LogPrint(BCLog::NET_GM, "CGMAuth::%s -- Sending GMAUTH, peer=%d\n", __func__, pnode->GetId());
    connman.PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::GMAUTH, gmauth));
}

bool CGMAuth::ProcessMessage(CNode* pnode, const std::string& strCommand, CDataStream& vRecv, CConnman& connman, CValidationState& state)
{
    if (!g_tiertwo_sync_state.IsBlockchainSynced()) {
        // we can't verify GMAUTH messages when we don't have the latest GM list
        return true;
    }

    if (strCommand == NetMsgType::GMAUTH) {
        CGMAuth gmauth;
        vRecv >> gmauth;
        // only one GMAUTH allowed
        bool fAlreadyHaveGMAUTH = WITH_LOCK(pnode->cs_gmauth, return !pnode->verifiedProRegTxHash.IsNull(););
        if (fAlreadyHaveGMAUTH) {
            return state.DoS(100, false, REJECT_INVALID, "duplicate gmauth");
        }

        if ((~pnode->nServices) & (NODE_NETWORK | NODE_BLOOM)) {
            // either NODE_NETWORK or NODE_BLOOM bit is missing in node's services
            return state.DoS(100, false, REJECT_INVALID, "gmauth from a node with invalid services");
        }

        if (gmauth.proRegTxHash.IsNull()) {
            return state.DoS(100, false, REJECT_INVALID, "empty gmauth proRegTxHash");
        }

        if (!gmauth.sig.IsValid()) {
            return state.DoS(100, false, REJECT_INVALID, "invalid gmauth signature");
        }

        auto gmList = deterministicGMManager->GetListAtChainTip();
        auto dgm = gmList.GetGM(gmauth.proRegTxHash);
        if (!dgm) {
            // in case node was unlucky and not up to date, just let it be connected as a regular node, which gives it
            // a chance to get up-to-date and thus realize that it's not a GM anymore. We still give it a
            // low DoS score.
            return state.DoS(10, false, REJECT_INVALID, "missing gmauth gamemaster");
        }

        uint256 signHash;
        {
            LOCK(pnode->cs_gmauth);
            int nOurNodeVersion{PROTOCOL_VERSION};
            if (Params().NetworkIDString() != CBaseChainParams::MAIN && gArgs.IsArgSet("-pushversion")) {
                nOurNodeVersion = gArgs.GetArg("-pushversion", PROTOCOL_VERSION);
            }
            // See comment in PushGMAUTH (fInbound is negated here as we're on the other side of the connection)
            if (pnode->nVersion < GMAUTH_NODE_VER_VERSION || nOurNodeVersion < GMAUTH_NODE_VER_VERSION) {
                signHash = ::SerializeHash(std::make_tuple(dgm->pdgmState->pubKeyOperator, pnode->sentGMAuthChallenge, !pnode->fInbound));
            } else {
                signHash = ::SerializeHash(std::make_tuple(dgm->pdgmState->pubKeyOperator, pnode->sentGMAuthChallenge, !pnode->fInbound, pnode->nVersion.load()));
            }
            LogPrint(BCLog::NET_GM, "CGMAuth::%s -- constructed signHash for nVersion %d, peer=%d\n", __func__, pnode->nVersion, pnode->GetId());
        }

        if (!gmauth.sig.VerifyInsecure(dgm->pdgmState->pubKeyOperator.Get(), signHash)) {
            // Same as above, GM seems to not know its fate yet, so give it a chance to update. If this is a
            // malicious node (DoSing us), it'll get banned soon.
            return state.DoS(10, false, REJECT_INVALID, "gmauth signature verification failed");
        }

        if (!pnode->fInbound) {
            g_mmetaman.GetMetaInfo(gmauth.proRegTxHash)->SetLastOutboundSuccess(GetAdjustedTime());
            if (pnode->m_gamemaster_probe_connection) {
                LogPrint(BCLog::NET_GM, "%s -- Gamemaster probe successful for %s, disconnecting. peer=%d\n",
                         __func__, gmauth.proRegTxHash.ToString(), pnode->GetId());
                pnode->fDisconnect = true;
                return true;
            }
        }

        // future: Move this to the first line of this function..
        const CActiveGamemasterInfo* activeGmInfo{nullptr};
        if (!fGameMaster || !activeGamemasterManager ||
            (activeGmInfo = activeGamemasterManager->GetInfo())->proTxHash.IsNull()) {
            return true;
        }

        connman.ForEachNode([&](CNode* pnode2) {
            if (pnode->fDisconnect) {
                // we've already disconnected the new peer
                return;
            }

            if (pnode2->verifiedProRegTxHash == gmauth.proRegTxHash) {
                if (fGameMaster) {
                    auto deterministicOutbound = llmq::DeterministicOutboundConnection(activeGmInfo->proTxHash, gmauth.proRegTxHash);
                    LogPrint(BCLog::NET_GM, "CGMAuth::ProcessMessage -- Gamemaster %s has already verified as peer %d, deterministicOutbound=%s. peer=%d\n",
                             gmauth.proRegTxHash.ToString(), pnode2->GetId(), deterministicOutbound.ToString(), pnode->GetId());
                    if (deterministicOutbound == activeGmInfo->proTxHash) {
                        if (pnode2->fInbound) {
                            LogPrint(BCLog::NET_GM, "CGMAuth::ProcessMessage -- dropping old inbound, peer=%d\n", pnode2->GetId());
                            pnode2->fDisconnect = true;
                        } else if (pnode->fInbound) {
                            LogPrint(BCLog::NET_GM, "CGMAuth::ProcessMessage -- dropping new inbound, peer=%d\n", pnode->GetId());
                            pnode->fDisconnect = true;
                        }
                    } else {
                        if (!pnode2->fInbound) {
                            LogPrint(BCLog::NET_GM, "CGMAuth::ProcessMessage -- dropping old outbound, peer=%d\n", pnode2->GetId());
                            pnode2->fDisconnect = true;
                        } else if (!pnode->fInbound) {
                            LogPrint(BCLog::NET_GM, "CGMAuth::ProcessMessage -- dropping new outbound, peer=%d\n", pnode->GetId());
                            pnode->fDisconnect = true;
                        }
                    }
                } else {
                    LogPrint(BCLog::NET_GM, "CGMAuth::ProcessMessage -- Gamemaster %s has already verified as peer %d, dropping new connection. peer=%d\n",
                             gmauth.proRegTxHash.ToString(), pnode2->GetId(), pnode->GetId());
                    pnode->fDisconnect = true;
                }
            }
        });

        if (pnode->fDisconnect) {
            return true;
        }

        {
            LOCK(pnode->cs_gmauth);
            pnode->verifiedProRegTxHash = gmauth.proRegTxHash;
            pnode->verifiedPubKeyHash = dgm->pdgmState->pubKeyOperator.GetHash();
        }

        if (!pnode->m_gamemaster_iqr_connection && connman.GetTierTwoConnMan()->isGamemasterQuorumRelayMember(pnode->verifiedProRegTxHash)) {
            // Tell our peer that we're interested in plain LLMQ recovered signatures.
            // Otherwise, the peer would only announce/send messages resulting from QRECSIG,
            // future e.g. tx locks or chainlocks. SPV and regular full nodes should not send
            // this message as they are usually only interested in the higher level messages.
            CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman.PushMessage(pnode, msgMaker.Make(NetMsgType::QSENDRECSIGS, true));
            pnode->m_gamemaster_iqr_connection = true;
        }

        LogPrint(BCLog::NET_GM, "CGMAuth::%s -- Valid GMAUTH for %s, peer=%d\n", __func__, gmauth.proRegTxHash.ToString(), pnode->GetId());
    }
    return true;
}

void CGMAuth::NotifyGamemasterListChanged(bool undo, const CDeterministicGMList& oldGMList, const CDeterministicGMListDiff& diff)
{
    // we're only interested in updated/removed GMs. Added GMs are of no interest for us
    if (diff.updatedGMs.empty() && diff.removedGms.empty()) {
        return;
    }

    g_connman->ForEachNode([&](CNode* pnode) {
        LOCK(pnode->cs_gmauth);
        if (pnode->verifiedProRegTxHash.IsNull()) {
            return;
        }
        auto verifiedDgm = oldGMList.GetGM(pnode->verifiedProRegTxHash);
        if (!verifiedDgm) {
            return;
        }
        bool doRemove = false;
        if (diff.removedGms.count(verifiedDgm->GetInternalId())) {
            doRemove = true;
        } else {
            auto it = diff.updatedGMs.find(verifiedDgm->GetInternalId());
            if (it != diff.updatedGMs.end()) {
                if ((it->second.fields & CDeterministicGMStateDiff::Field_pubKeyOperator) && it->second.state.pubKeyOperator.GetHash() != pnode->verifiedPubKeyHash) {
                    doRemove = true;
                }
            }
        }

        if (doRemove) {
            LogPrint(BCLog::NET_GM, "CGMAuth::NotifyGamemasterListChanged -- Disconnecting GM %s due to key changed/removed, peer=%d\n",
                     pnode->verifiedProRegTxHash.ToString(), pnode->GetId());
            pnode->fDisconnect = true;
        }
    });
}
