// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validationinterface.h"

#include "chain.h"
#include "consensus/validation.h"
#include "evo/deterministicmns.h"
#include "logging.h"
#include "scheduler.h"
#include "util/validation.h"
#include "validation.h" // cs_main

#include <future>
#include <list>
#include <unordered_map>
#include <boost/signals2/signal.hpp>

struct ValidationInterfaceConnections {
    boost::signals2::scoped_connection UpdatedBlockTip;
    boost::signals2::scoped_connection TransactionAddedToMempool;
    boost::signals2::scoped_connection BlockConnected;
    boost::signals2::scoped_connection BlockDisconnected;
    boost::signals2::scoped_connection TransactionRemovedFromMempool;
    boost::signals2::scoped_connection SetBestChain;
    boost::signals2::scoped_connection Broadcast;
    boost::signals2::scoped_connection BlockChecked;
    boost::signals2::scoped_connection NotifyMasternodeListChanged;
};

struct MainSignalsInstance {

    /** Notifies listeners of updated block chain tip */
    boost::signals2::signal<void (const CBlockIndex *, const CBlockIndex *, bool fInitialDownload)> UpdatedBlockTip;
    /** Notifies listeners of a transaction having been added to mempool. */
    boost::signals2::signal<void (const CTransactionRef &)> TransactionAddedToMempool;
    /**
     * Notifies listeners of a block being connected.
     * Provides a vector of transactions evicted from the mempool as a result.
     */
    boost::signals2::signal<void (const std::shared_ptr<const CBlock> &, const CBlockIndex *pindex)> BlockConnected;
    /** Notifies listeners of a block being disconnected */
    boost::signals2::signal<void (const std::shared_ptr<const CBlock> &, const uint256& blockHash, int nBlockHeight, int64_t blockTime)> BlockDisconnected;
    /** Notifies listeners of a transaction removal from the mempool */
    boost::signals2::signal<void (const CTransactionRef &, MemPoolRemovalReason reason)> TransactionRemovedFromMempool;
    /** Notifies listeners of a new active block chain. */
    boost::signals2::signal<void (const CBlockLocator &)> SetBestChain;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void (CConnman* connman)> Broadcast;
    /** Notifies listeners of a block validation result */
    boost::signals2::signal<void (const CBlock&, const CValidationState&)> BlockChecked;
    /** Notifies listeners of updated deterministic masternode list */
    boost::signals2::signal<void (bool undo, const CDeterministicMNList& oldMNList, const CDeterministicMNListDiff& diff)> NotifyMasternodeListChanged;

    std::unordered_map<CValidationInterface*, ValidationInterfaceConnections> m_connMainSignals;

    // We are not allowed to assume the scheduler only runs in one thread,
    // but must ensure all callbacks happen in-order, so we end up creating
    // our own queue here :(
    SingleThreadedSchedulerClient m_schedulerClient;

    explicit MainSignalsInstance(CScheduler *pscheduler) : m_schedulerClient(pscheduler) {}
};

static CMainSignals g_signals;

void CMainSignals::RegisterBackgroundSignalScheduler(CScheduler& scheduler) {
    assert(!m_internals);
    m_internals.reset(new MainSignalsInstance(&scheduler));
}

void CMainSignals::UnregisterBackgroundSignalScheduler() {
    m_internals.reset(nullptr);
}

void CMainSignals::FlushBackgroundCallbacks() {
    if (m_internals) {
        m_internals->m_schedulerClient.EmptyQueue();
    }
}

size_t CMainSignals::CallbacksPending() {
    if (!m_internals) return 0;
    return m_internals->m_schedulerClient.CallbacksPending();
}

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterValidationInterface(CValidationInterface* pwalletIn)
{
    ValidationInterfaceConnections& conns = g_signals.m_internals->m_connMainSignals[pwalletIn];
    conns.UpdatedBlockTip = g_signals.m_internals->UpdatedBlockTip.connect(std::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    conns.TransactionAddedToMempool = g_signals.m_internals->TransactionAddedToMempool.connect(std::bind(&CValidationInterface::TransactionAddedToMempool, pwalletIn, std::placeholders::_1));
    conns.BlockConnected = g_signals.m_internals->BlockConnected.connect(std::bind(&CValidationInterface::BlockConnected, pwalletIn, std::placeholders::_1, std::placeholders::_2));
    conns.BlockDisconnected = g_signals.m_internals->BlockDisconnected.connect(std::bind(&CValidationInterface::BlockDisconnected, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    conns.TransactionRemovedFromMempool = g_signals.m_internals->TransactionRemovedFromMempool.connect(std::bind(&CValidationInterface::TransactionRemovedFromMempool, pwalletIn, std::placeholders::_1, std::placeholders::_2));
    conns.SetBestChain = g_signals.m_internals->SetBestChain.connect(std::bind(&CValidationInterface::SetBestChain, pwalletIn, std::placeholders::_1));
    conns.Broadcast = g_signals.m_internals->Broadcast.connect(std::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, std::placeholders::_1));
    conns.BlockChecked = g_signals.m_internals->BlockChecked.connect(std::bind(&CValidationInterface::BlockChecked, pwalletIn, std::placeholders::_1, std::placeholders::_2));
    conns.NotifyMasternodeListChanged = g_signals.m_internals->NotifyMasternodeListChanged.connect(std::bind(&CValidationInterface::NotifyMasternodeListChanged, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn)
{
    if (g_signals.m_internals) {
        g_signals.m_internals->m_connMainSignals.erase(pwalletIn);
    }
}

void UnregisterAllValidationInterfaces()
{
    if (!g_signals.m_internals) {
        return;
    }
    g_signals.m_internals->m_connMainSignals.clear();
}

void CallFunctionInValidationInterfaceQueue(std::function<void ()> func) {
    g_signals.m_internals->m_schedulerClient.AddToProcessQueue(std::move(func));
}

void SyncWithValidationInterfaceQueue() {
    AssertLockNotHeld(cs_main);
    // if queue is empty, do not wait for nothing.s
    if (g_signals.CallbacksPending() == 0) return;

    // Block until the validation queue drains
    std::promise<void> promise;
    CallFunctionInValidationInterfaceQueue([&promise] {
        promise.set_value();
    });
    promise.get_future().wait();
}

// Use a macro instead of a function for conditional logging to prevent
// evaluating arguments when logging is not enabled.
//
// NOTE: The lambda captures all local variables by value.
#define ENQUEUE_AND_LOG_EVENT(event, fmt, name, ...)           \
    do {                                                       \
        auto local_name = (name);                              \
        LOG_EVENT("Enqueuing " fmt, local_name, __VA_ARGS__);  \
        m_internals->m_schedulerClient.AddToProcessQueue([=] { \
            LOG_EVENT(fmt, local_name, __VA_ARGS__);           \
            event();                                           \
        });                                                    \
    } while (0)

#define LOG_EVENT(fmt, ...) \
    LogPrint(BCLog::VALIDATION, fmt "\n", __VA_ARGS__)

void CMainSignals::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) {
    // Dependencies exist that require UpdatedBlockTip events to be delivered in the order in which
    // the chain actually updates. One way to ensure this is for the caller to invoke this signal
    // in the same critical section where the chain is updated

    auto event = [pindexNew, pindexFork, fInitialDownload, this] {
        m_internals->UpdatedBlockTip(pindexNew, pindexFork, fInitialDownload);
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: new block hash=%s, fork block hash=%s (in IBD=%s)", __func__,
                          pindexNew->GetBlockHash().ToString(),
                          pindexFork ? pindexFork->GetBlockHash().ToString() : "null",
                          fInitialDownload);
}

void CMainSignals::TransactionAddedToMempool(const CTransactionRef &ptx) {
    auto event = [ptx, this] {
        m_internals->TransactionAddedToMempool(ptx);
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: txid=%s", __func__, ptx->GetHash().ToString());
}

void CMainSignals::TransactionRemovedFromMempool(const CTransactionRef& ptx, MemPoolRemovalReason reason) {
    auto event = [ptx, reason, this] {
        m_internals->TransactionRemovedFromMempool(ptx, reason);
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: txid=%s", __func__, ptx->GetHash().ToString());
}

void CMainSignals::BlockConnected(const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex) {
    auto event = [pblock, pindex, this] {
        m_internals->BlockConnected(pblock, pindex);
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: block hash=%s, block height=%d", __func__,
                          pblock->GetHash().ToString(), pindex->nHeight);
}

void CMainSignals::BlockDisconnected(const std::shared_ptr<const CBlock> &pblock, const uint256& blockHash, int nBlockHeight, int64_t blockTime) {
    auto event = [pblock, blockHash, nBlockHeight, blockTime, this] {
        m_internals->BlockDisconnected(pblock, blockHash, nBlockHeight, blockTime);
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: block hash=%s, block height=%d, block time=%d", __func__,
                          blockHash.ToString(), nBlockHeight, blockTime);
}

void CMainSignals::SetBestChain(const CBlockLocator& locator) {
    auto event = [locator, this] {
        m_internals->SetBestChain(locator);
    };
    ENQUEUE_AND_LOG_EVENT(event, "%s: block hash=%s", __func__,
                          locator.IsNull() ? "null" : locator.vHave.front().ToString());
}

void CMainSignals::Broadcast(CConnman* connman) {
    m_internals->Broadcast(connman);
}

void CMainSignals::BlockChecked(const CBlock& block, const CValidationState& state) {
    m_internals->BlockChecked(block, state);
    LOG_EVENT("%s: block hash=%s (state=%s)", __func__,
              block.GetHash().ToString(), FormatStateMessage(state));
}

void CMainSignals::NotifyMasternodeListChanged(bool undo, const CDeterministicMNList& oldMNList, const CDeterministicMNListDiff& diff) {
    auto event = [undo, oldMNList, diff, this] {
        m_internals->NotifyMasternodeListChanged(undo, oldMNList, diff);
    };
    ENQUEUE_AND_LOG_EVENT(event,
                          "%s: (undo=%d) old list for=%s, added=%d, updated=%d, removed=%d", __func__,
                          undo,
                          oldMNList.GetBlockHash().ToString(),
                          diff.addedMNs.size(),
                          diff.updatedMNs.size(),
                          diff.removedMns.size());
}
