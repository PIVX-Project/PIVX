// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2022 The PIVX Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_init.h"

#include "bls/bls_worker.h"
#include "llmq/quorums_blockprocessor.h"
#include "llmq/quorums_commitment.h"
#include "llmq/quorums_debug.h"
#include "llmq/quorums_dkgsessionmgr.h"


namespace llmq
{

CBLSWorker* blsWorker;

void InitLLMQSystem(CEvoDB& evoDb)
{
    blsWorker = new CBLSWorker();

    quorumDKGDebugManager.reset(new CDKGDebugManager());
    quorumBlockProcessor.reset(new CQuorumBlockProcessor(evoDb));
    quorumDKGSessionManager.reset(new CDKGSessionManager(evoDb, *blsWorker));
}

void DestroyLLMQSystem()
{
    quorumDKGSessionManager.reset();
    quorumBlockProcessor.reset();
    quorumDKGDebugManager.reset();
    delete blsWorker;
    blsWorker = nullptr;
}

void StartLLMQSystem()
{
    if (blsWorker) {
        blsWorker->Start();
    }
    if (quorumDKGSessionManager) {
        quorumDKGSessionManager->StartThreads();
    }
}

void StopLLMQSystem()
{
    if (quorumDKGSessionManager) {
        quorumDKGSessionManager->StopThreads();
    }
    if (blsWorker) {
        blsWorker->Stop();
    }
}

} // namespace llmq
