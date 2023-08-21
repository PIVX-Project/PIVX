// Copyright (c) 2014-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evo/evonotificationinterface.h"

#include "evo/deterministicgms.h"
#include "evo/gmauth.h"
#include "llmq/quorums.h"
#include "llmq/quorums_chainlocks.h"
#include "llmq/quorums_dkgsessionmgr.h"
#include "validation.h"

void EvoNotificationInterface::InitializeCurrentBlockTip()
{
    LOCK(cs_main);
    deterministicGMManager->SetTipIndex(chainActive.Tip());
}

void EvoNotificationInterface::AcceptedBlockHeader(const CBlockIndex* pindexNew)
{
    llmq::chainLocksHandler->AcceptedBlockHeader(pindexNew);
}

void EvoNotificationInterface::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload)
{
    // background thread updates
    llmq::chainLocksHandler->UpdatedBlockTip(pindexNew, pindexFork);
    llmq::quorumDKGSessionManager->UpdatedBlockTip(pindexNew, fInitialDownload);
    llmq::quorumManager->UpdatedBlockTip(pindexNew, pindexFork, fInitialDownload);
}

void EvoNotificationInterface::NotifyGamemasterListChanged(bool undo, const CDeterministicGMList& oldGMList, const CDeterministicGMListDiff& diff)
{
    CGMAuth::NotifyGamemasterListChanged(undo, oldGMList, diff);
}
