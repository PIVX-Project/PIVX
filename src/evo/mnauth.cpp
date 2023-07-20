// Copyright (c) 2019-2021 The Dash Core developers
// Copyright (c) 2021-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "evo/mnauth.h"

#include "activemasternode.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "net.h" // for CSerializedNetMsg
#include "netmessagemaker.h"
#include "llmq/quorums_connections.h"
#include "tiertwo/masternode_meta_manager.h"
#include "tiertwo/net_masternodes.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "util/system.h" // for fMasternode and gArgs access

#include "version.h" // for MNAUTH_NODE_VER_VERSION

void CMNAuth::PushMNAUTH(CNode* pnode, CConnman& connman)
{
    const CActiveMasternodeInfo* activeMnInfo{nullptr};
    if (!fMasterNode || !activeMasternodeManager ||
        (activeMnInfo = activeMasternodeManager->GetInfo())->proTxHash.IsNull()) {
        return;
    }

    uint256 signHash;
    {
        LOCK(pnode->cs_mnauth);
        if (pnode->receivedMNAuthChallenge.IsNull()) {
            return;
        }
        // We include fInbound in signHash to forbid interchanging of challenges by a man in the middle (MITM). This way
        // we protect ourselves against MITM in this form:
        //   node1 <- Eve -> node2
        // It does not protect against:
        //   node1 -> Eve -> node2
        // This is ok as we only use MNAUTH as a DoS protection and not for sensitive stuff
        int nOurNodeVersion{PROTOCOL_VERSION};
        if (Params().NetworkIDString() != CBaseChainParams::MAIN && gArgs.IsArgSet("-pushversion")) {
            nOurNodeVersion = (int)gArgs.GetArg("-pushversion", PROTOCOL_VERSION);
        }
        if (pnode->nVersion < MNAUTH_NODE_VER_VERSION || nOurNodeVersion < MNAUTH_NODE_VER_VERSION) {
            signHash = ::SerializeHash(std::make_tuple(activeMnInfo->pubKeyOperator, pnode->receivedMNAuthChallenge, pnode->fInbound));
        } else {
            signHash = ::SerializeHash(std::make_tuple(activeMnInfo->pubKeyOperator, pnode->receivedMNAuthChallenge, pnode->fInbound, nOurNodeVersion));
        }
    }

    CMNAuth mnauth;
    mnauth.proRegTxHash = activeMnInfo->proTxHash;
    mnauth.sig = activeMnInfo->keyOperator.Sign(signHash);

    LogPrint(BCLog::NET_MN, "CMNAuth::%s -- Sending MNAUTH, peer=%d\n", __func__, pnode->GetId());
    connman.PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make(NetMsgType::MNAUTH, mnauth));
}

bool CMNAuth::ProcessMessage(CNode* pnode, const std::string& strCommand, CDataStream& vRecv, CConnman& connman, CValidationState& state)
{
    if (!g_tiertwo_sync_state.IsBlockchainSynced()) {
        // we can't verify MNAUTH messages when we don't have the latest MN list
        return true;
    }

    if (strCommand == NetMsgType::MNAUTH) {
        CMNAuth mnauth;
        vRecv >> mnauth;
        // only one MNAUTH allowed
        bool fAlreadyHaveMNAUTH = WITH_LOCK(pnode->cs_mnauth, return !pnode->verifiedProRegTxHash.IsNull(););
        if (fAlreadyHaveMNAUTH) {
            return state.DoS(100, false, REJECT_INVALID, "duplicate mnauth");
        }

        if ((~pnode->nServices) & (NODE_NETWORK | NODE_BLOOM)) {
            // either NODE_NETWORK or NODE_BLOOM bit is missing in node's services
            return state.DoS(100, false, REJECT_INVALID, "mnauth from a node with invalid services");
        }

        if (mnauth.proRegTxHash.IsNull()) {
            return state.DoS(100, false, REJECT_INVALID, "empty mnauth proRegTxHash");
        }

        if (!mnauth.sig.IsValid()) {
            return state.DoS(100, false, REJECT_INVALID, "invalid mnauth signature");
        }

        auto mnList = deterministicMNManager->GetListAtChainTip();
        auto dmn = mnList.GetMN(mnauth.proRegTxHash);
        if (!dmn) {
            // in case node was unlucky and not up to date, just let it be connected as a regular node, which gives it
            // a chance to get up-to-date and thus realize that it's not a MN anymore. We still give it a
            // low DoS score.
            return state.DoS(10, false, REJECT_INVALID, "missing mnauth masternode");
        }

        uint256 signHash;
        {
            LOCK(pnode->cs_mnauth);
            int nOurNodeVersion{PROTOCOL_VERSION};
            if (Params().NetworkIDString() != CBaseChainParams::MAIN && gArgs.IsArgSet("-pushversion")) {
                nOurNodeVersion = gArgs.GetArg("-pushversion", PROTOCOL_VERSION);
            }
            // See comment in PushMNAUTH (fInbound is negated here as we're on the other side of the connection)
            if (pnode->nVersion < MNAUTH_NODE_VER_VERSION || nOurNodeVersion < MNAUTH_NODE_VER_VERSION) {
                signHash = ::SerializeHash(std::make_tuple(dmn->pdmnState->pubKeyOperator, pnode->sentMNAuthChallenge, !pnode->fInbound));
            } else {
                signHash = ::SerializeHash(std::make_tuple(dmn->pdmnState->pubKeyOperator, pnode->sentMNAuthChallenge, !pnode->fInbound, pnode->nVersion.load()));
            }
            LogPrint(BCLog::NET_MN, "CMNAuth::%s -- constructed signHash for nVersion %d, peer=%d\n", __func__, pnode->nVersion, pnode->GetId());
        }

        if (!mnauth.sig.VerifyInsecure(dmn->pdmnState->pubKeyOperator.Get(), signHash)) {
            // Same as above, MN seems to not know its fate yet, so give it a chance to update. If this is a
            // malicious node (DoSing us), it'll get banned soon.
            return state.DoS(10, false, REJECT_INVALID, "mnauth signature verification failed");
        }

        if (!pnode->fInbound) {
            g_mmetaman.GetMetaInfo(mnauth.proRegTxHash)->SetLastOutboundSuccess(GetAdjustedTime());
            if (pnode->m_masternode_probe_connection) {
                LogPrint(BCLog::NET_MN, "%s -- Masternode probe successful for %s, disconnecting. peer=%d\n",
                         __func__, mnauth.proRegTxHash.ToString(), pnode->GetId());
                pnode->fDisconnect = true;
                return true;
            }
        }

        // future: Move this to the first line of this function..
        const CActiveMasternodeInfo* activeMnInfo{nullptr};
        if (!fMasterNode || !activeMasternodeManager ||
            (activeMnInfo = activeMasternodeManager->GetInfo())->proTxHash.IsNull()) {
            return true;
        }

        connman.ForEachNode([&](CNode* pnode2) {
            if (pnode->fDisconnect) {
                // we've already disconnected the new peer
                return;
            }

            if (pnode2->verifiedProRegTxHash == mnauth.proRegTxHash) {
                if (fMasterNode) {
                    auto deterministicOutbound = llmq::DeterministicOutboundConnection(activeMnInfo->proTxHash, mnauth.proRegTxHash);
                    LogPrint(BCLog::NET_MN, "CMNAuth::ProcessMessage -- Masternode %s has already verified as peer %d, deterministicOutbound=%s. peer=%d\n",
                             mnauth.proRegTxHash.ToString(), pnode2->GetId(), deterministicOutbound.ToString(), pnode->GetId());
                    if (deterministicOutbound == activeMnInfo->proTxHash) {
                        if (pnode2->fInbound) {
                            LogPrint(BCLog::NET_MN, "CMNAuth::ProcessMessage -- dropping old inbound, peer=%d\n", pnode2->GetId());
                            pnode2->fDisconnect = true;
                        } else if (pnode->fInbound) {
                            LogPrint(BCLog::NET_MN, "CMNAuth::ProcessMessage -- dropping new inbound, peer=%d\n", pnode->GetId());
                            pnode->fDisconnect = true;
                        }
                    } else {
                        if (!pnode2->fInbound) {
                            LogPrint(BCLog::NET_MN, "CMNAuth::ProcessMessage -- dropping old outbound, peer=%d\n", pnode2->GetId());
                            pnode2->fDisconnect = true;
                        } else if (!pnode->fInbound) {
                            LogPrint(BCLog::NET_MN, "CMNAuth::ProcessMessage -- dropping new outbound, peer=%d\n", pnode->GetId());
                            pnode->fDisconnect = true;
                        }
                    }
                } else {
                    LogPrint(BCLog::NET_MN, "CMNAuth::ProcessMessage -- Masternode %s has already verified as peer %d, dropping new connection. peer=%d\n",
                             mnauth.proRegTxHash.ToString(), pnode2->GetId(), pnode->GetId());
                    pnode->fDisconnect = true;
                }
            }
        });

        if (pnode->fDisconnect) {
            return true;
        }

        {
            LOCK(pnode->cs_mnauth);
            pnode->verifiedProRegTxHash = mnauth.proRegTxHash;
            pnode->verifiedPubKeyHash = dmn->pdmnState->pubKeyOperator.GetHash();
        }

        if (!pnode->m_masternode_iqr_connection && connman.GetTierTwoConnMan()->isMasternodeQuorumRelayMember(pnode->verifiedProRegTxHash)) {
            // Tell our peer that we're interested in plain LLMQ recovered signatures.
            // Otherwise, the peer would only announce/send messages resulting from QRECSIG,
            // future e.g. tx locks or chainlocks. SPV and regular full nodes should not send
            // this message as they are usually only interested in the higher level messages.
            CNetMsgMaker msgMaker(pnode->GetSendVersion());
            connman.PushMessage(pnode, msgMaker.Make(NetMsgType::QSENDRECSIGS, true));
            pnode->m_masternode_iqr_connection = true;
        }

        LogPrint(BCLog::NET_MN, "CMNAuth::%s -- Valid MNAUTH for %s, peer=%d\n", __func__, mnauth.proRegTxHash.ToString(), pnode->GetId());
    }
    return true;
}

void CMNAuth::NotifyMasternodeListChanged(bool undo, const CDeterministicMNList& oldMNList, const CDeterministicMNListDiff& diff)
{
    // we're only interested in updated/removed MNs. Added MNs are of no interest for us
    if (diff.updatedMNs.empty() && diff.removedMns.empty()) {
        return;
    }

    g_connman->ForEachNode([&](CNode* pnode) {
        LOCK(pnode->cs_mnauth);
        if (pnode->verifiedProRegTxHash.IsNull()) {
            return;
        }
        auto verifiedDmn = oldMNList.GetMN(pnode->verifiedProRegTxHash);
        if (!verifiedDmn) {
            return;
        }
        bool doRemove = false;
        if (diff.removedMns.count(verifiedDmn->GetInternalId())) {
            doRemove = true;
        } else {
            auto it = diff.updatedMNs.find(verifiedDmn->GetInternalId());
            if (it != diff.updatedMNs.end()) {
                if ((it->second.fields & CDeterministicMNStateDiff::Field_pubKeyOperator) && it->second.state.pubKeyOperator.GetHash() != pnode->verifiedPubKeyHash) {
                    doRemove = true;
                }
            }
        }

        if (doRemove) {
            LogPrint(BCLog::NET_MN, "CMNAuth::NotifyMasternodeListChanged -- Disconnecting MN %s due to key changed/removed, peer=%d\n",
                     pnode->verifiedProRegTxHash.ToString(), pnode->GetId());
            pnode->fDisconnect = true;
        }
    });
}
