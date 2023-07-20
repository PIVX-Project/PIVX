// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/quorums_init.h"

#include "bls/bls_worker.h"
#include "llmq/quorums.h"
#include "llmq/quorums_blockprocessor.h"
#include "llmq/quorums_debug.h"
#include "llmq/quorums_dkgsessionmgr.h"
#include "quorums_chainlocks.h"
#include "quorums_signing.h"
#include "quorums_signing_shares.h"

namespace llmq
{

CBLSWorker* blsWorker;

void InitLLMQSystem(CEvoDB& evoDb, CScheduler* scheduler, bool unitTests)
{
    blsWorker = new CBLSWorker();

    quorumDKGDebugManager.reset(new CDKGDebugManager());
    quorumBlockProcessor.reset(new CQuorumBlockProcessor(evoDb));
    quorumDKGSessionManager.reset(new CDKGSessionManager(evoDb, *blsWorker));
    quorumManager.reset(new CQuorumManager(evoDb, *blsWorker, *quorumDKGSessionManager));
    quorumSigSharesManager = new CSigSharesManager();
    quorumSigningManager = new CSigningManager(unitTests);
    chainLocksHandler = new CChainLocksHandler(scheduler);
}

void DestroyLLMQSystem()
{
    delete chainLocksHandler;
    chainLocksHandler = nullptr;
    delete quorumSigningManager;
    quorumSigningManager = nullptr;
    delete quorumSigSharesManager;
    quorumSigSharesManager = nullptr;
    quorumDKGSessionManager.reset();
    quorumBlockProcessor.reset();
    quorumDKGDebugManager.reset();
    quorumManager.reset();
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
    if (quorumSigSharesManager) {
        quorumSigSharesManager->StartWorkerThread();
    }
}

void StopLLMQSystem()
{
    if (quorumDKGSessionManager) {
        quorumDKGSessionManager->StopThreads();
    }
    if (quorumSigSharesManager) {
        quorumSigSharesManager->StopWorkerThread();
    }
    if (blsWorker) {
        blsWorker->Stop();
    }
}

} // namespace llmq
