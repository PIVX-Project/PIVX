// Copyright (c) 2018-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_QUORUMS_UTILS_H
#define hemis_QUORUMS_UTILS_H

#include "consensus/params.h"

#include <vector>

class CBLSPublicKey;

namespace llmq
{

namespace utils
{
uint256 BuildCommitmentHash(Consensus::LLMQType llmqType, const uint256& blockHash, const std::vector<bool>& validMembers, const CBLSPublicKey& pubKey, const uint256& vvecHash);
uint256 BuildSignHash(Consensus::LLMQType llmqType, const uint256& quorumHash, const uint256& id, const uint256& msgHash);

// works for sig shares and recovered sigs
template<typename T>
uint256 BuildSignHash(const T& s)
{
   return BuildSignHash((Consensus::LLMQType)s.llmqType, s.quorumHash, s.id, s.msgHash);
}

std::string ToHexStr(const std::vector<bool>& vBits);

bool IsQuorumActive(Consensus::LLMQType llmqType, const uint256& quorumHash);

template <typename NodesContainer, typename Continue, typename Callback>
static void IterateNodesRandom(NodesContainer& nodeStates, Continue&& cont, Callback&& callback, FastRandomContext& rnd)
{
   std::vector<typename NodesContainer::iterator> rndNodes;
   rndNodes.reserve(nodeStates.size());
   for (auto it = nodeStates.begin(); it != nodeStates.end(); ++it) {
       rndNodes.emplace_back(it);
   }
   if (rndNodes.empty()) {
       return;
   }
   Shuffle(rndNodes.begin(), rndNodes.end(), rnd);

   size_t idx = 0;
   while (!rndNodes.empty() && cont()) {
       auto nodeId = rndNodes[idx]->first;
       auto& ns = rndNodes[idx]->second;

       if (callback(nodeId, ns)) {
           idx = (idx + 1) % rndNodes.size();
       } else {
           rndNodes.erase(rndNodes.begin() + idx);
           if (rndNodes.empty()) {
               break;
           }
           idx %= rndNodes.size();
       }
   }
}

} // namespace llmq::utils

} // namespace llmq

#endif // hemis_QUORUMS_UTILS_H
