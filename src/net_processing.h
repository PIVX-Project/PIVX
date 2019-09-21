// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include "net.h"
#include "lightzpivthread.h"
#include "validationinterface.h"

extern CLightWorker lightWorker;

struct CBlockReject {
    unsigned char chRejectCode;
    std::string strRejectReason;
    uint256 hashBlock;
};

struct QueuedBlock {
    uint256 hash;
    CBlockIndex* pindex;     //!< Optional.
    bool fValidatedHeaders;  //!< Whether this block has validated headers at the time of request.
};

/**
 * Maintain validation-specific state about nodes, protected by cs_main, instead
 * by CNode's own locks. This simplifies asynchronous operation, where
 * processing of incoming data is done after the ProcessMessage call returns,
 * and we're no longer holding the node's locks.
 */
struct CNodeState {
	//! The peer's address
	CService address;
	//! Whether we have a fully established connection.
	bool fCurrentlyConnected;
	//! Accumulated misbehaviour score for this peer.
	int nMisbehavior;
	//! Whether this peer should be disconnected and banned (unless whitelisted).
	bool fShouldBan;
	//! String name of this peer (debugging/logging purposes).
	std::string name;
	//! List of asynchronously-determined block rejections to notify this peer about.
	std::vector<CBlockReject> rejects;
	//! The best known block we know this peer has announced.
	CBlockIndex* pindexBestKnownBlock;
	//! The hash of the last unknown block this peer has announced.
	uint256 hashLastUnknownBlock;
	//! The last full block we both have.
	CBlockIndex* pindexLastCommonBlock;
	//! Whether we've started headers synchronization with this peer.
	bool fSyncStarted;
	//! Since when we're stalling block download progress (in microseconds), or 0.
	int64_t nStallingSince;
	std::list<QueuedBlock> vBlocksInFlight;
	int nBlocksInFlight;
	//! Whether we consider this a preferred download peer.
	bool fPreferredDownload;

	CNodeBlocks nodeBlocks;

	CNodeState()
	{
		fCurrentlyConnected = false;
		nMisbehavior = 0;
		fShouldBan = false;
		pindexBestKnownBlock = NULL;
		hashLastUnknownBlock = uint256(0);
		pindexLastCommonBlock = NULL;
		fSyncStarted = false;
		nStallingSince = 0;
		nBlocksInFlight = 0;
		fPreferredDownload = false;
	}
};

/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** Expiration time for orphan transactions in seconds */
static const int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;
/** Minimum time between orphan transactions expire time checks in seconds */
static const int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;
/** Default number of orphan+recently-replaced txn to keep around for block reconstruction */
static const unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN = 100;

/** Register with a network node to receive its signals */
void RegisterNodeSignals(CNodeSignals& nodeSignals);
/** Unregister a network node */
void UnregisterNodeSignals(CNodeSignals& nodeSignals);

CNodeState *State(NodeId pnode);

struct CNodeStateStats {
    int nMisbehavior;
    int nSyncHeight;
    int nCommonHeight;
    std::vector<int> vHeightInFlight;
};

/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);
/** Increase a node's misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch);

/** Process protocol messages received from a given node */
bool ProcessMessages(CNode* pfrom);

/**
 * Send queued protocol messages to be sent to a give node.
 *
 * @param[in]   pto             The node which we are sending messages to.
 * @param[in]   connman         The connection manager for that node.
 * @param[in]   interrupt       Interrupt condition for processing threads
 * @return                      True if there is more work to be done
 */
bool SendMessages(CNode* pto, bool fSendTrickle);
int ActiveProtocol();

#endif // BITCOIN_NET_PROCESSING_H
