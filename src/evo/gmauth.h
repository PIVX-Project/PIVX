// Copyright (c) 2019-2020 The Dash Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_EVO_GMAUTH_H
#define hemis_EVO_GMAUTH_H

#include "bls/bls_wrapper.h"
#include "serialize.h"

class CConnman;
class CDataStream;
class CDeterministicGMList;
class CDeterministicGMListDiff;
class CNode;
class CValidationState;

/**
 * This class handles the p2p message GMAUTH. GMAUTH is sent directly after VERACK and authenticates the sender as a
 * gamemaster. It is only sent when the sender is actually a gamemaster.
 *
 * GMAUTH signs a challenge that was previously sent via VERSION. The challenge is signed differently depending on
 * the connection being an inbound or outbound connection, which avoids MITM of this form:
 *   node1 <- Eve -> node2
 * while still allowing:
 *   node1 -> Eve -> node2
 *
 * This is fine as we only use this mechanism for DoS protection. It allows us to keep gamemaster connections open for
 * a very long time without evicting the connections when inbound connection limits are hit (non-GMs will then be evicted).
 *
 * If we ever want to add transfer of sensitive data, THIS AUTHENTICATION MECHANISM IS NOT ENOUGH!! We'd need to implement
 * proper encryption for these connections first.
 */

class CGMAuth
{
public:
    uint256 proRegTxHash;
    CBLSSignature sig;
    SERIALIZE_METHODS(CGMAuth, obj) {
        READWRITE(obj.proRegTxHash, obj.sig);
    }

    static void PushGMAUTH(CNode* pnode, CConnman& connman);
    static bool ProcessMessage(CNode* pnode, const std::string& strCommand, CDataStream& vRecv, CConnman& connman, CValidationState& state);
    static void NotifyGamemasterListChanged(bool undo, const CDeterministicGMList& oldGMList, const CDeterministicGMListDiff& diff);
};


#endif // hemis_EVO_GMAUTH_H
