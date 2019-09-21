// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2019 barrystyle
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validation.h"

#include "zpiv/accumulators.h"
#include "zpiv/accumulatormap.h"

#include "addrman.h"
#include "amount.h"
#include "alert.h"
#include "blocksignature.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "checkqueue.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "init.h"
#include "invalid.h"
#include "kernel.h"
#include "libzerocoin/Denominations.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "masternodeman.h"
#include "net.h"
#include "netbase.h"
#include "net_processing.h"
#include "obfuscation.h"
#include "pow.h"
#include "spork.h"
#include "sporkdb.h"
#include "swifttx.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "random.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "timedata.h"
#include "tinyformat.h"
#include "txdb.h"
#include "txmempool.h"
#include "guiinterface.h"
#include "undo.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "zpivchain.h"
#include "zpiv/zerocoin.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>

#include <atomic>
#include <queue>
#include <sstream>

using namespace boost;
using namespace std;

#if defined(NDEBUG)
#error "PIVX cannot be compiled without assertions."
#endif

/**
 * Global state
 */

CCriticalSection cs_main;

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex *pindexBestHeader = NULL;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;
std::atomic_bool fImporting(false);
bool fReindex = false;
bool fTxIndex = true;
bool fCheckBlockIndex = false;
bool fVerifyingBlocks = false;
unsigned int nCoinCacheSize = 5000;
bool fHavePruned = false;
bool fPruneMode = false;
bool fIsBareMultisigStd = DEFAULT_PERMIT_BAREMULTISIG;
bool fRequireStandard = true;
bool fClearSpendCache = false;
bool fCheckpointsEnabled = DEFAULT_CHECKPOINTS_ENABLED;
size_t nCoinCacheUsage = 5000 * 300;
bool fAlerts = DEFAULT_ALERTS;
int64_t nMaxTipAge = DEFAULT_MAX_TIP_AGE;
uint64_t nPruneTarget = 0;
map<uint256, int64_t> mapZerocoinspends;
map<uint256, uint256> mapProofOfStake;
std::set<std::pair<COutPoint, unsigned int> > setStakeSeen;
map<unsigned int, unsigned int> mapHashedBlocks;

uint256 hashAssumeValid;

CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;
CFeeRate minRelayTxFee = CFeeRate(10000);
CTxMemPool mempool(::minRelayTxFee);

int64_t nReserveBalance = 0;

struct IteratorComparator
{
    template<typename I>
    bool operator()(const I& a, const I& b)
    {
        return &(*a) < &(*b);
    }
};

struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
};

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

const string strMessageMagic = "DarkNet Signed Message:\n";

// Internal stuff
namespace {

    struct CBlockIndexWorkComparator
    {
        bool operator()(CBlockIndex *pa, CBlockIndex *pb) const {
            // First sort by most total work, ...
            if (pa->nChainWork > pb->nChainWork) return false;
            if (pa->nChainWork < pb->nChainWork) return true;

            // ... then by earliest time received, ...
            if (pa->nSequenceId < pb->nSequenceId) return false;
            if (pa->nSequenceId > pb->nSequenceId) return true;

            // Use pointer address as tie breaker (should only happen with blocks
            // loaded from disk, as those all have id 0).
            if (pa < pb) return false;
            if (pa > pb) return true;

            // Identical blocks.
            return false;
        }
    };

    CBlockIndex *pindexBestInvalid;

    /**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though, and pruning nodes may be
     * missing the data for the block.
     */
    set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;
    /** All pairs A->B, where A (or one of its ancestors) misses transactions, but B has transactions.
     * Pruned nodes may have entries where B is missing data.
     */
    multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

    CCriticalSection cs_LastBlockFile;
    vector<CBlockFileInfo> vinfoBlockFile;
    int nLastBlockFile = 0;
    /** Global flag to indicate we should check to see if there are
     *  block/undo files that should be deleted.  Set on startup
     *  or if we allocate more file space when we're in prune mode
     */
    bool fCheckForPruning = false;

    /**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
    CCriticalSection cs_nBlockSequenceId;
    /** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
    int32_t nBlockSequenceId = 1;
    /** chainwork for the last block that preciousblock has been applied to. */
    uint256 nLastPreciousChainwork = 0;

    /**
         * Sources of received blocks, to be able to send them reject messages or ban
         * them, if processing happens afterwards. Protected by cs_main.
         */
    map<uint256, NodeId> mapBlockSource;

    /** Dirty block index entries. */
    set<CBlockIndex*> setDirtyBlockIndex;

    /** Dirty block file entries. */
    set<int> setDirtyFileInfo;

} // anon namespace

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH(const uint256& hash, locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = (*mi).second;
            if (chain.Contains(pindex))
                return pindex;
            if (pindex->GetAncestor(chain.Height()) == chain.Tip()) {
                return chain.Tip();
            }
        }
    }
    return chain.Genesis();
}

CCoinsViewCache* pcoinsTip = NULL;
CBlockTreeDB* pblocktree = NULL;
CZerocoinDB* zerocoinDB = NULL;
CSporkDB* pSporkDB = NULL;

enum FlushStateMode {
    FLUSH_STATE_NONE,
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};

// See definition for documentation
void FindFilesToPruneManual(set<int>& setFilesToPrune, int nManualPruneHeight);
void MarkBlockAsReceived(const uint256& hash);

bool IsFinalTx(const CTransaction& tx, int nBlockHeight, int64_t nBlockTime)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Height();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (const CTxIn& txin : tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase() || tx.HasZerocoinSpendInputs())
        return true; // coinbase has no inputs and zerocoinspend has a special input
    //todo should there be a check for a 'standard' zerocoinspend here?

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prev = mapInputs.GetOutputFor(tx.vin[i]);

        vector<vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig
        // IsStandard() will have already returned false
        // and this method isn't called.
        vector<vector<unsigned char> > stack;
        if (!EvalScript(stack, tx.vin[i].scriptSig, false, BaseSignatureChecker()))
            return false;

        if (whichType == TX_SCRIPTHASH) {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (Solver(subscript, whichType2, vSolutions2)) {
                int tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
                if (tmpExpected < 0)
                    return false;
                nArgsExpected += tmpExpected;
            } else {
                // Any other Script with less than 15 sigops OK:
                unsigned int sigops = subscript.GetSigOpCount(true);
                // ... extra data left on the stack after execution is OK, too:
                return (sigops <= MAX_P2SH_SIGOPS);
            }
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    for (const CTxIn& txin : tx.vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const CTxOut& txout : tx.vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase() || tx.HasZerocoinSpendInputs())
        // a tx containing a zc spend can have only zc inputs
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prevout = inputs.GetOutputFor(tx.vin[i]);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

int GetInputAge(CTxIn& vin)
{
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewMemPool viewMempool(pcoinsTip, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        const CCoins* coins = view.AccessCoins(vin.prevout.hash);

        if (coins) {
            if (coins->nHeight < 0) return 0;
            return (chainActive.Tip()->nHeight + 1) - coins->nHeight;
        } else
            return -1;
    }
}

int GetInputAgeIX(uint256 nTXHash, CTxIn& vin)
{
    int sigs = 0;
    int nResult = GetInputAge(vin);
    if (nResult < 0) nResult = 0;

    if (nResult < 6) {
        map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(nTXHash);
        if (i != mapTxLocks.end()) {
            sigs = (*i).second.CountSignatures();
        }
        if (sigs >= SWIFTTX_SIGNATURES_REQUIRED) {
            return nSwiftTXDepth + nResult;
        }
    }

    return -1;
}

int GetIXConfirmations(uint256 nTXHash)
{
    int sigs = 0;

    map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(nTXHash);
    if (i != mapTxLocks.end()) {
        sigs = (*i).second.CountSignatures();
    }
    if (sigs >= SWIFTTX_SIGNATURES_REQUIRED) {
        return nSwiftTXDepth;
    }

    return 0;
}

bool CheckZerocoinMint(const uint256& txHash, const CTxOut& txout, CValidationState& state, bool fCheckOnly)
{
    libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(false));
    if(!TxOutToPublicCoin(txout, pubCoin, state))
        return state.DoS(100, error("CheckZerocoinMint(): TxOutToPublicCoin() failed"));

    if (!pubCoin.validate())
        return state.DoS(100, error("CheckZerocoinMint() : PubCoin does not validate"));

    return true;
}

bool ContextualCheckZerocoinMint(const CTransaction& tx, const libzerocoin::PublicCoin& coin, const CBlockIndex* pindex)
{
    if (pindex->nHeight >= Params().Zerocoin_Block_Public_Spend_Enabled()) {
        // Zerocoin MINTs have been disabled
        return error("%s: Mints disabled at height %d - unable to add pubcoin %s", __func__,
                pindex->nHeight, coin.getValue().GetHex().substr(0, 10));
    }
    if (pindex->nHeight >= Params().Zerocoin_Block_V2_Start() && Params().NetworkID() != CBaseChainParams::TESTNET) {
        //See if this coin has already been added to the blockchain
        uint256 txid;
        int nHeight;
        if (zerocoinDB->ReadCoinMint(coin.getValue(), txid) && IsTransactionInChain(txid, nHeight))
            return error("%s: pubcoin %s was already accumulated in tx %s", __func__,
                         coin.getValue().GetHex().substr(0, 10),
                         txid.GetHex());
    }

    return true;
}

bool isBlockBetweenFakeSerialAttackRange(int nHeight)
{
    if (Params().NetworkID() != CBaseChainParams::MAIN)
        return false;

    return nHeight <= Params().Zerocoin_Block_EndFakeSerial();
}

bool CheckPublicCoinSpendEnforced(int blockHeight, bool isPublicSpend) {
    if (blockHeight >= Params().Zerocoin_Block_Public_Spend_Enabled()) {
        // reject old coin spend
        if (!isPublicSpend) {
            return error("%s: failed to add block with older zc spend version", __func__);
        }

    } else {
        if (isPublicSpend) {
            return error("%s: failed to add block, public spend enforcement not activated", __func__);
        }
    }
    return true;
}

bool ContextualCheckZerocoinSpend(const CTransaction& tx, const libzerocoin::CoinSpend* spend, CBlockIndex* pindex, const uint256& hashBlock)
{
    if(!ContextualCheckZerocoinSpendNoSerialCheck(tx, spend, pindex, hashBlock)){
        return false;
    }

    //Reject serial's that are already in the blockchain
    int nHeightTx = 0;
    if (IsSerialInBlockchain(spend->getCoinSerialNumber(), nHeightTx))
        return error("%s : zPIV spend with serial %s is already in block %d\n", __func__,
                     spend->getCoinSerialNumber().GetHex(), nHeightTx);

    return true;
}

bool ContextualCheckZerocoinSpendNoSerialCheck(const CTransaction& tx, const libzerocoin::CoinSpend* spend, CBlockIndex* pindex, const uint256& hashBlock)
{
    //Check to see if the zPIV is properly signed
    if (pindex->nHeight >= Params().Zerocoin_Block_V2_Start()) {
        try {
            if (!spend->HasValidSignature())
                return error("%s: V2 zPIV spend does not have a valid signature\n", __func__);
        } catch (libzerocoin::InvalidSerialException &e) {
            // Check if we are in the range of the attack
            if(!isBlockBetweenFakeSerialAttackRange(pindex->nHeight))
                return error("%s: Invalid serial detected, txid %s, in block %d\n", __func__, tx.GetHash().GetHex(), pindex->nHeight);
            else
                LogPrintf("%s: Invalid serial detected within range in block %d\n", __func__, pindex->nHeight);
        }

        libzerocoin::SpendType expectedType = libzerocoin::SpendType::SPEND;
        if (tx.IsCoinStake())
            expectedType = libzerocoin::SpendType::STAKE;
        if (spend->getSpendType() != expectedType) {
            return error("%s: trying to spend zPIV without the correct spend type. txid=%s\n", __func__,
                         tx.GetHash().GetHex());
        }
    }

    bool v1Serial = spend->getVersion() < libzerocoin::PrivateCoin::PUBKEY_VERSION;
    if (pindex->nHeight >= Params().Zerocoin_Block_Public_Spend_Enabled()) {
        //Reject V1 old serials.
        if (v1Serial) {
            return error("%s : zPIV v1 serial spend not spendable, serial %s, tx %s\n", __func__,
                         spend->getCoinSerialNumber().GetHex(), tx.GetHash().GetHex());
        }
    }

    //Reject serial's that are not in the acceptable value range
    if (!spend->HasValidSerial(Params().Zerocoin_Params(v1Serial)))  {
        // Up until this block our chain was not checking serials correctly..
        if (!isBlockBetweenFakeSerialAttackRange(pindex->nHeight))
            return error("%s : zPIV spend with serial %s from tx %s is not in valid range\n", __func__,
                     spend->getCoinSerialNumber().GetHex(), tx.GetHash().GetHex());
        else
            LogPrintf("%s:: HasValidSerial :: Invalid serial detected within range in block %d\n", __func__, pindex->nHeight);
    }


    return true;
}


bool CheckZerocoinSpend(const CTransaction& tx, bool fVerifySignature, CValidationState& state, bool fFakeSerialAttack)
{
    //max needed non-mint outputs should be 2 - one for redemption address and a possible 2nd for change
    if (tx.vout.size() > 2) {
        int outs = 0;
        for (const CTxOut& out : tx.vout) {
            if (out.IsZerocoinMint())
                continue;
            outs++;
        }
        if (outs > 2 && !tx.IsCoinStake())
            return state.DoS(100, error("CheckZerocoinSpend(): over two non-mint outputs in a zerocoinspend transaction"));
    }

    //compute the txout hash that is used for the zerocoinspend signatures
    CMutableTransaction txTemp;
    for (const CTxOut& out : tx.vout) {
        txTemp.vout.push_back(out);
    }
    uint256 hashTxOut = txTemp.GetHash();

    bool fValidated = false;
    set<CBigNum> serials;
    CAmount nTotalRedeemed = 0;
    for (const CTxIn& txin : tx.vin) {

        //only check txin that is a zcspend
        bool isPublicSpend = txin.IsZerocoinPublicSpend();
        if (!txin.IsZerocoinSpend() && !isPublicSpend)
            continue;

        libzerocoin::CoinSpend newSpend;
        CTxOut prevOut;
        if (isPublicSpend) {
            if(!GetOutput(txin.prevout.hash, txin.prevout.n, state, prevOut)){
                return state.DoS(100, error("CheckZerocoinSpend(): public zerocoin spend prev output not found, prevTx %s, index %d", txin.prevout.hash.GetHex(), txin.prevout.n));
            }
            libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
            PublicCoinSpend publicSpend(params);
            if (!ZPIVModule::parseCoinSpend(txin, tx, prevOut, publicSpend)){
                return state.DoS(100, error("CheckZerocoinSpend(): public zerocoin spend parse failed"));
            }
            newSpend = publicSpend;
        }else {
            newSpend = TxInToZerocoinSpend(txin);
        }

        //check that the denomination is valid
        if (newSpend.getDenomination() == libzerocoin::ZQ_ERROR)
            return state.DoS(100, error("Zerocoinspend does not have the correct denomination"));

        //check that denomination is what it claims to be in nSequence
        if (newSpend.getDenomination() != txin.nSequence)
            return state.DoS(100, error("Zerocoinspend nSequence denomination does not match CoinSpend"));

        //make sure the txout has not changed
        if (newSpend.getTxOutHash() != hashTxOut)
            return state.DoS(100, error("Zerocoinspend does not use the same txout that was used in the SoK"));

        if (isPublicSpend) {
            libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
            PublicCoinSpend ret(params);
            if (!ZPIVModule::validateInput(txin, prevOut, tx, ret)){
                return state.DoS(100, error("CheckZerocoinSpend(): public zerocoin spend did not verify"));
            }
        } else
            // Skip signature verification during initial block download
            if (fVerifySignature) {
                //see if we have record of the accumulator used in the spend tx
                CBigNum bnAccumulatorValue = 0;
                if (!zerocoinDB->ReadAccumulatorValue(newSpend.getAccumulatorChecksum(), bnAccumulatorValue)) {
                    uint32_t nChecksum = newSpend.getAccumulatorChecksum();
                    return state.DoS(100, error("%s: Zerocoinspend could not find accumulator associated with checksum %s", __func__, HexStr(BEGIN(nChecksum), END(nChecksum))));
                }

                libzerocoin::Accumulator accumulator(Params().Zerocoin_Params(chainActive.Height() < Params().Zerocoin_Block_V2_Start()),
                                        newSpend.getDenomination(), bnAccumulatorValue);

                //Check that the coin has been accumulated
                if(!newSpend.Verify(accumulator, !fFakeSerialAttack))
                        return state.DoS(100, error("CheckZerocoinSpend(): zerocoin spend did not verify"));
            }

        if (serials.count(newSpend.getCoinSerialNumber()))
            return state.DoS(100, error("Zerocoinspend serial is used twice in the same tx"));
        serials.insert(newSpend.getCoinSerialNumber());

        //make sure that there is no over redemption of coins
        nTotalRedeemed += libzerocoin::ZerocoinDenominationToAmount(newSpend.getDenomination());
        fValidated = true;
    }

    if (!tx.IsCoinStake() && nTotalRedeemed < tx.GetValueOut()) {
        LogPrintf("redeemed = %s , spend = %s \n", FormatMoney(nTotalRedeemed), FormatMoney(tx.GetValueOut()));
        return state.DoS(100, error("Transaction spend more than was redeemed in zerocoins"));
    }

    return fValidated;
}

bool CheckTransaction(const CTransaction& tx, bool fZerocoinActive, bool fRejectBadUTXO, CValidationState& state, bool fFakeSerialAttack)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, error("CheckTransaction() : vin empty"),
            REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, error("CheckTransaction() : vout empty"),
            REJECT_INVALID, "bad-txns-vout-empty");

    // Size limits
    unsigned int nMaxSize = MAX_ZEROCOIN_TX_SIZE;

    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > nMaxSize)
        return state.DoS(100, error("CheckTransaction() : size limits failed"),
            REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    for (const CTxOut& txout : tx.vout) {
        if (txout.IsEmpty() && !tx.IsCoinBase() && !tx.IsCoinStake())
            return state.DoS(100, error("CheckTransaction(): txout empty for user transaction"));

        if (txout.nValue < 0)
            return state.DoS(100, error("CheckTransaction() : txout.nValue negative"),
                REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > Params().MaxMoneyOut())
            return state.DoS(100, error("CheckTransaction() : txout.nValue too high"),
                REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CheckTransaction() : txout total out of range"),
                REJECT_INVALID, "bad-txns-txouttotal-toolarge");
        if (fZerocoinActive && txout.IsZerocoinMint()) {
            if(!CheckZerocoinMint(tx.GetHash(), txout, state, true))
                return state.DoS(100, error("CheckTransaction() : invalid zerocoin mint"));
        }
    }

    set<COutPoint> vInOutPoints;
    set<CBigNum> vZerocoinSpendSerials;
    int nZCSpendCount = 0;
    for (const CTxIn& txin : tx.vin) {
        // Check for duplicate inputs
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, error("CheckTransaction() : duplicate inputs"),
                REJECT_INVALID, "bad-txns-inputs-duplicate");

        //duplicate zcspend serials are checked in CheckZerocoinSpend()
        if (!txin.IsZerocoinSpend()) {
            vInOutPoints.insert(txin.prevout);
        } else if (!txin.IsZerocoinPublicSpend()) {
            nZCSpendCount++;
        }
    }

    if (fZerocoinActive) {
        if (nZCSpendCount > Params().Zerocoin_MaxSpendsPerTransaction())
            return state.DoS(100, error("CheckTransaction() : there are more zerocoin spends than are allowed in one transaction"));

        //require that a zerocoinspend only has inputs that are zerocoins
        if (tx.HasZerocoinSpendInputs()) {
            for (const CTxIn& in : tx.vin) {
                if (!in.IsZerocoinSpend() && !in.IsZerocoinPublicSpend())
                    return state.DoS(100,
                                     error("CheckTransaction() : zerocoinspend contains inputs that are not zerocoins"));
            }

            // Do not require signature verification if this is initial sync and a block over 24 hours old
            bool fVerifySignature = !IsInitialBlockDownload() && (GetTime() - chainActive.Tip()->GetBlockTime() < (60*60*24));
            if (!CheckZerocoinSpend(tx, fVerifySignature, state, fFakeSerialAttack))
                return state.DoS(100, error("CheckTransaction() : invalid zerocoin spend"));
        }
    }

    if (tx.IsCoinBase()) {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 150)
            return state.DoS(100, error("CheckTransaction() : coinbase script size=%d", tx.vin[0].scriptSig.size()),
                REJECT_INVALID, "bad-cb-length");
    } else if (fZerocoinActive && tx.HasZerocoinSpendInputs()) {
        if (tx.vin.size() < 1)
            return state.DoS(10, error("CheckTransaction() : Zerocoin Spend has less than allowed txin's"), REJECT_INVALID, "bad-zerocoinspend");
        if (tx.HasZerocoinPublicSpendInputs()) {
            // tx has public zerocoin spend inputs
            if(static_cast<int>(tx.vin.size()) > Params().Zerocoin_MaxPublicSpendsPerTransaction())
                return state.DoS(10, error("CheckTransaction() : Zerocoin Spend has more than allowed txin's"), REJECT_INVALID, "bad-zerocoinspend");
        } else {
            // tx has regular zerocoin spend inputs
            if(static_cast<int>(tx.vin.size()) > Params().Zerocoin_MaxSpendsPerTransaction())
                return state.DoS(10, error("CheckTransaction() : Zerocoin Spend has more than allowed txin's"), REJECT_INVALID, "bad-zerocoinspend");
        }

    } else {
        for (const CTxIn& txin : tx.vin)
            if (txin.prevout.IsNull() && (fZerocoinActive && !txin.IsZerocoinSpend()))
                return state.DoS(10, error("CheckTransaction() : prevout is null"),
                    REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}

bool CheckFinalTx(const CTransaction& tx, int flags)
{
    AssertLockHeld(cs_main);

    // By convention a negative value for flags indicates that the
    // current network-enforced consensus rules should be used. In
    // a future soft-fork scenario that would mean checking which
    // rules would be enforced for the next block and setting the
    // appropriate flags. At the present time no soft-forks are
    // scheduled, so no flags are set.
    flags = max(flags, 0);

    // CheckFinalTx() uses chainActive.Height()+1 to evaluate
    // nLockTime because when IsFinalTx() is called within
    // CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a
    // transaction can be part of the *next* block, we need to call
    // IsFinalTx() with one more than chainActive.Height().
    const int nBlockHeight = chainActive.Height() + 1;

    // BIP113 will require that time-locked transactions have nLockTime set to
    // less than the median time of the previous block they're contained in.
    // When the next block is created its previous block will be the current
    // chain tip, so we use that to calculate the median time passed to
    // IsFinalTx() if LOCKTIME_MEDIAN_TIME_PAST is set.
    const int64_t nBlockTime = (flags & LOCKTIME_MEDIAN_TIME_PAST) ? chainActive.Tip()->GetMedianTimePast() : GetAdjustedTime();

    return IsFinalTx(tx, nBlockHeight, nBlockTime);
}

CAmount GetMinRelayFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree)
{
    {
        LOCK(mempool.cs);
        uint256 hash = tx.GetHash();
        double dPriorityDelta = 0;
        CAmount nFeeDelta = 0;
        mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
        if (dPriorityDelta > 0 || nFeeDelta > 0)
            return 0;
    }

    CAmount nMinFee = ::minRelayTxFee.GetFee(nBytes);

    if (fAllowFree) {
        // There is a free transaction area in blocks created by most miners,
        // * If we are relaying we allow transactions up to DEFAULT_BLOCK_PRIORITY_SIZE - 1000
        //   to be considered to fall into this category. We don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        if (nBytes < (DEFAULT_BLOCK_PRIORITY_SIZE - 1000))
            nMinFee = 0;
    }

    if (!MoneyRange(nMinFee))
        nMinFee = Params().MaxMoneyOut();
    return nMinFee;
}


bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, bool fLimitFree, bool* pfMissingInputs, bool fRejectInsaneFee, bool ignoreFees)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    //Temporarily disable zerocoin for maintenance
    if (GetAdjustedTime() > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE) && tx.ContainsZerocoins())
        return state.DoS(10, error("AcceptToMemoryPool : Zerocoin transactions are temporarily disabled for maintenance"), REJECT_INVALID, "bad-tx");

    int chainHeight = chainActive.Height();
    if (!CheckTransaction(tx, chainHeight >= Params().Zerocoin_StartHeight(), true, state, isBlockBetweenFakeSerialAttackRange(chainHeight)))
        return state.DoS(100, error("AcceptToMemoryPool: : CheckTransaction failed"), REJECT_INVALID, "bad-tx");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, error("AcceptToMemoryPool: : coinbase as individual tx"),
            REJECT_INVALID, "coinbase");

    //Coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return state.DoS(100, error("AcceptToMemoryPool: coinstake as individual tx. txid=%s", tx.GetHash().GetHex()),
            REJECT_INVALID, "coinstake");

    // Only accept nLockTime-using transactions that can be mined in the next
    // block; we don't want our mempool filled up with transactions that can't
    // be mined yet.
    if (!CheckFinalTx(tx, STANDARD_LOCKTIME_VERIFY_FLAGS))
        return state.DoS(0, false, REJECT_NONSTANDARD, "non-final");

    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    if (Params().RequireStandard() && !IsStandardTx(tx, reason))
        return state.DoS(0,
            error("AcceptToMemoryPool : nonstandard transaction: %s", reason),
            REJECT_NONSTANDARD, reason);
    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash)) {
        LogPrintf("%s tx already in mempool\n", __func__);
        return false;
    }

    // ----------- swiftTX transaction scanning -----------

    for (const CTxIn& in : tx.vin) {
        if (mapLockedInputs.count(in.prevout)) {
            if (mapLockedInputs[in.prevout] != tx.GetHash()) {
                return state.DoS(0,
                    error("AcceptToMemoryPool : conflicts with existing transaction lock: %s", reason),
                    REJECT_INVALID, "tx-lock-conflict");
            }
        }
    }

    // Check for conflicts with in-memory transactions
    if (!tx.HasZerocoinSpendInputs()) {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint)) {
                // Disable replacement feature for now
                return false;
            }
        }
    }


    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        CAmount nValueIn = 0;
        if (tx.HasZerocoinSpendInputs()) {
            nValueIn = tx.GetZerocoinSpent();

            //Check that txid is not already in the chain
            int nHeightTx = 0;
            if (IsTransactionInChain(tx.GetHash(), nHeightTx))
                return state.Invalid(error("AcceptToMemoryPool : zPIV spend tx %s already in block %d",
                                           tx.GetHash().GetHex(), nHeightTx), REJECT_DUPLICATE, "bad-txns-inputs-spent");

            //Check for double spending of serial #'s
            for (const CTxIn& txIn : tx.vin) {
                // Only allow for zc spends inputs
                bool isPublicSpend = txIn.IsZerocoinPublicSpend();
                bool isPrivZerocoinSpend = txIn.IsZerocoinSpend();
                if (!isPrivZerocoinSpend && !isPublicSpend) {
                    return state.Invalid(error("%s: AcceptToMemoryPool failed for tx %s, every input must be a zcspend or zcpublicspend", __func__,
                                        tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zpiv");
                }

                // Check enforcement
                if (!CheckPublicCoinSpendEnforced(chainActive.Height(), isPublicSpend)){
                    return state.Invalid(error("%s: AcceptToMemoryPool failed for tx %s", __func__,
                                               tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zpiv");
                }

                if (isPublicSpend) {
                    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
                    PublicCoinSpend publicSpend(params);
                    if (!ZPIVModule::ParseZerocoinPublicSpend(txIn, tx, state, publicSpend)){
                        return false;
                    }
                    if (!ContextualCheckZerocoinSpend(tx, &publicSpend, chainActive.Tip(), 0))
                        return state.Invalid(error("%s: ContextualCheckZerocoinSpend failed for tx %s", __func__,
                                                   tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zpiv");
                } else {
                    libzerocoin::CoinSpend spend = TxInToZerocoinSpend(txIn);
                    if (!ContextualCheckZerocoinSpend(tx, &spend, chainActive.Tip(), 0))
                        return state.Invalid(error("%s: ContextualCheckZerocoinSpend failed for tx %s", __func__,
                                                   tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zpiv");
                }

            }
        } else {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            // do we already have it?
            if (view.HaveCoins(hash))
                return false;

            // do all inputs exist?
            // Note that this does not check for the presence of actual outputs (see the next check for that),
            // only helps filling in pfMissingInputs (to determine missing vs spent).
            for (const CTxIn& txin : tx.vin) {
                if (!view.HaveCoins(txin.prevout.hash)) {
                    if (pfMissingInputs)
                        *pfMissingInputs = true;
                    return false;
                }

                //Check for invalid/fraudulent inputs
                if (!ValidOutPoint(txin.prevout, chainActive.Height())) {
                    return state.Invalid(error("%s : tried to spend invalid input %s in tx %s", __func__, txin.prevout.ToString(),
                                                tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-inputs");
                }
            }

            // Check that zPIV mints (if included) are not already known
            for (auto& out : tx.vout) {
                if (!out.IsZerocoinMint())
                    continue;

                libzerocoin::PublicCoin coin(Params().Zerocoin_Params(false));
                if (!TxOutToPublicCoin(out, coin, state))
                    return state.Invalid(error("%s: failed final check of zerocoinmint for tx %s", __func__, tx.GetHash().GetHex()));

                if (!ContextualCheckZerocoinMint(tx, coin, chainActive.Tip()))
                    return state.Invalid(error("%s: zerocoin mint failed contextual check", __func__));
            }

            // are the actual inputs available?
            if (!view.HaveInputs(tx))
                return state.Invalid(error("AcceptToMemoryPool : inputs already spent"),
                    REJECT_DUPLICATE, "bad-txns-inputs-spent");

            // Bring the best block into scope
            view.GetBestBlock();

            nValueIn = view.GetValueIn(tx);

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (Params().RequireStandard() && !AreInputsStandard(tx, view))
            return error("AcceptToMemoryPool: : nonstandard transaction input");

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        if (!tx.HasZerocoinSpendInputs()) {
        unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);
        if (nSigOps > MAX_TX_SIGOPS)
            return state.DoS(0,
                error("AcceptToMemoryPool : too many sigops %s, %d > %d",
                      hash.ToString(), nSigOps, MAX_TX_SIGOPS),
                REJECT_NONSTANDARD, "bad-txns-too-many-sigops");
        }

        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;
        double dPriority = 0;
        if (!tx.HasZerocoinSpendInputs())
            view.GetPriority(tx, chainActive.Height());

        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height());
        unsigned int nSize = entry.GetTxSize();

        // Don't accept it if it can't get into a block
        // but prioritise dstx and don't check fees for it
        if (mapObfuscationBroadcastTxes.count(hash)) {
            mempool.PrioritiseTransaction(hash, hash.ToString(), 1000, 0.1 * COIN);
        } else if (!ignoreFees) {
            CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
            if (fLimitFree && nFees < txMinFee && !tx.HasZerocoinSpendInputs())
                return state.DoS(0, error("AcceptToMemoryPool : not enough fees %s, %d < %d",
                                        hash.ToString(), nFees, txMinFee),
                    REJECT_INSUFFICIENTFEE, "insufficient fee");

            // Require that free transactions have sufficient priority to be mined in the next block.
            if (tx.HasZerocoinMintOutputs()) {
                if(nFees < Params().Zerocoin_MintFee() * tx.GetZerocoinMintCount())
                    return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient fee for zerocoinmint");
            } else if (!tx.HasZerocoinSpendInputs() && GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(view.GetPriority(tx, chainActive.Height() + 1))) {
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
            }

            // Continuously rate-limit free (really, very-low-fee) transactions
            // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
            // be annoying or make others' transactions take longer to confirm.
            if (fLimitFree && nFees < ::minRelayTxFee.GetFee(nSize) && !tx.HasZerocoinSpendInputs()) {
                static CCriticalSection csFreeLimiter;
                static double dFreeCount;
                static int64_t nLastTime;
                int64_t nNow = GetTime();

                LOCK(csFreeLimiter);

                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0 / 600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount >= GetArg("-limitfreerelay", 30) * 10 * 1000)
                    return state.DoS(0, error("AcceptToMemoryPool : free transaction rejected by rate limiter"),
                        REJECT_INSUFFICIENTFEE, "rate limited free transaction");
                LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount + nSize);
                dFreeCount += nSize;
            }
        }

        if (fRejectInsaneFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000)
            return error("AcceptToMemoryPool: : insane fees %s, %d > %d",
                hash.ToString(),
                nFees, ::minRelayTxFee.GetFee(nSize) * 10000);

        // As zero fee transactions are not going to be accepted in the near future (4.0) and the code will be fully refactored soon.
        // This is just a quick inline towards that goal, the mempool by default will not accept them. Blocking
        // any subsequent network relay.
        if ((Params().NetworkID() != CBaseChainParams::REGTEST) &&
            nFees == 0 && !tx.HasZerocoinSpendInputs()) {
            return error("%s: zero fees not accepted %s, %d > %d",
                         __func__,
                         hash.ToString(),
                         nFees, ::minRelayTxFee.GetFee(nSize) * 10000);
        }

        bool fCLTVHasMajority = CBlockIndex::IsSuperMajority(5, chainActive.Tip(), Params().EnforceBlockUpgradeMajority());

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        int flags = STANDARD_SCRIPT_VERIFY_FLAGS;
        if (fCLTVHasMajority)
            flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
        if (!CheckInputs(tx, state, view, true, flags, true)) {
            return error("AcceptToMemoryPool: : ConnectInputs failed %s", hash.ToString());
        }

        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        flags = MANDATORY_SCRIPT_VERIFY_FLAGS;
        if (fCLTVHasMajority)
            flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
        if (!CheckInputs(tx, state, view, true, flags, true)) {
            return error("AcceptToMemoryPool: : BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString());
        }

        // Store transaction in memory
        pool.addUnchecked(hash, entry);
    }

    SyncWithWallets(tx, NULL);

    //Track zerocoinspends and ensure that they are given priority to make it into the blockchain
    if (tx.HasZerocoinSpendInputs())
        mapZerocoinspends[tx.GetHash()] = GetAdjustedTime();

    return true;
}

bool AcceptableInputs(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, bool fLimitFree, bool* pfMissingInputs, bool fRejectInsaneFee, bool isDSTX)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;


    if (!CheckTransaction(tx, chainActive.Height() >= Params().Zerocoin_StartHeight(), true, state))
        return error("AcceptableInputs: : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, error("AcceptableInputs: : coinbase as individual tx"),
            REJECT_INVALID, "coinbase");

    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    // for any real tx this will be checked on AcceptToMemoryPool anyway
    //    if (Params().RequireStandard() && !IsStandardTx(tx, reason))
    //        return state.DoS(0,
    //                         error("AcceptableInputs : nonstandard transaction: %s", reason),
    //                         REJECT_NONSTANDARD, reason);

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // ----------- swiftTX transaction scanning -----------

    for (const CTxIn& in : tx.vin) {
        if (mapLockedInputs.count(in.prevout)) {
            if (mapLockedInputs[in.prevout] != tx.GetHash()) {
                return state.DoS(0,
                    error("AcceptableInputs : conflicts with existing transaction lock: %s", reason),
                    REJECT_INVALID, "tx-lock-conflict");
            }
        }
    }

    // Check for conflicts with in-memory transactions
    if (!tx.HasZerocoinSpendInputs()) {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint)) {
                // Disable replacement feature for now
                return false;
            }
        }
    }


    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        CAmount nValueIn = 0;
        {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            // do we already have it?
            if (view.HaveCoins(hash))
                return false;

            // do all inputs exist?
            // Note that this does not check for the presence of actual outputs (see the next check for that),
            // only helps filling in pfMissingInputs (to determine missing vs spent).
            for (const CTxIn& txin : tx.vin) {
                if (!view.HaveCoins(txin.prevout.hash)) {
                    if (pfMissingInputs)
                        *pfMissingInputs = true;
                    return false;
                }

                // check for invalid/fraudulent inputs
                if (!ValidOutPoint(txin.prevout, chainActive.Height())) {
                    return state.Invalid(error("%s : tried to spend invalid input %s in tx %s", __func__, txin.prevout.ToString(),
                                                tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-inputs");
                }
            }

            // are the actual inputs available?
            if (!view.HaveInputs(tx))
                return state.Invalid(error("AcceptableInputs : inputs already spent"),
                    REJECT_DUPLICATE, "bad-txns-inputs-spent");

            // Bring the best block into scope
            view.GetBestBlock();

            nValueIn = view.GetValueIn(tx);

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }

        // Check for non-standard pay-to-script-hash in inputs
        // for any real tx this will be checked on AcceptToMemoryPool anyway
        //        if (Params().RequireStandard() && !AreInputsStandard(tx, view))
        //            return error("AcceptableInputs: : nonstandard transaction input");

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSigOps = GetLegacySigOpCount(tx);
        unsigned int nMaxSigOps = MAX_TX_SIGOPS_CURRENT;
        nSigOps += GetP2SHSigOpCount(tx, view);
        if (nSigOps > nMaxSigOps)
            return state.DoS(0,
                error("AcceptableInputs : too many sigops %s, %d > %d",
                    hash.ToString(), nSigOps, nMaxSigOps),
                REJECT_NONSTANDARD, "bad-txns-too-many-sigops");

        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;
        double dPriority = view.GetPriority(tx, chainActive.Height());

        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height());
        unsigned int nSize = entry.GetTxSize();

        // Don't accept it if it can't get into a block
        // but prioritise dstx and don't check fees for it
        if (isDSTX) {
            mempool.PrioritiseTransaction(hash, hash.ToString(), 1000, 0.1 * COIN);
        } else { // same as !ignoreFees for AcceptToMemoryPool
            CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
            if (fLimitFree && nFees < txMinFee && !tx.HasZerocoinSpendInputs())
                return state.DoS(0, error("AcceptableInputs : not enough fees %s, %d < %d", hash.ToString(), nFees, txMinFee),
                    REJECT_INSUFFICIENTFEE, "insufficient fee");

            // Require that free transactions have sufficient priority to be mined in the next block.
            if (GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(view.GetPriority(tx, chainActive.Height() + 1))) {
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
            }

            // Continuously rate-limit free (really, very-low-fee) transactions
            // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
            // be annoying or make others' transactions take longer to confirm.
            if (fLimitFree && nFees < ::minRelayTxFee.GetFee(nSize) && !tx.HasZerocoinSpendInputs()) {
                static CCriticalSection csFreeLimiter;
                static double dFreeCount;
                static int64_t nLastTime;
                int64_t nNow = GetTime();

                LOCK(csFreeLimiter);

                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0 / 600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount >= GetArg("-limitfreerelay", 30) * 10 * 1000)
                    return state.DoS(0, error("AcceptableInputs : free transaction rejected by rate limiter"),
                        REJECT_INSUFFICIENTFEE, "rate limited free transaction");
                LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount + nSize);
                dFreeCount += nSize;
            }
        }

        if (fRejectInsaneFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000)
            return error("AcceptableInputs: : insane fees %s, %d > %d",
                hash.ToString(),
                nFees, ::minRelayTxFee.GetFee(nSize) * 10000);

        bool fCLTVHasMajority = CBlockIndex::IsSuperMajority(5, chainActive.Tip(), Params().EnforceBlockUpgradeMajority());

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        int flags = STANDARD_SCRIPT_VERIFY_FLAGS;
        if (fCLTVHasMajority)
            flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
        if (!CheckInputs(tx, state, view, false, flags, true)) {
            return error("AcceptableInputs: : ConnectInputs failed %s", hash.ToString());
        }

        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        // for any real tx this will be checked on AcceptToMemoryPool anyway
        //        if (!CheckInputs(tx, state, view, false, MANDATORY_SCRIPT_VERIFY_FLAGS, true))
        //        {
        //            return error("AcceptableInputs: : BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString());
        //        }

        // Store transaction in memory
        // pool.addUnchecked(hash, entry);
    }

    // SyncWithWallets(tx, NULL);

    return true;
}

bool GetOutput(const uint256& hash, unsigned int index, CValidationState& state, CTxOut& out)
{
    CTransaction txPrev;
    uint256 hashBlock;
    if (!GetTransaction(hash, txPrev, hashBlock, true)) {
        return state.DoS(100, error("Output not found"));
    }
    if (index > txPrev.vout.size()) {
        return state.DoS(100, error("Output not found, invalid index %d for %s",index, hash.GetHex()));
    }
    out = txPrev.vout[index];
    return true;
}

/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256& hash, CTransaction& txOut, uint256& hashBlock, bool fAllowSlow, CBlockIndex* blockIndex)
{
    CBlockIndex* pindexSlow = blockIndex;

    LOCK(cs_main);

    if (!blockIndex) {
        if (mempool.lookup(hash, txOut)) {
            return true;
        }

        if (fTxIndex) {
            CDiskTxPos postx;
            if (pblocktree->ReadTxIndex(hash, postx)) {
                CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
                if (file.IsNull())
                    return error("%s: OpenBlockFile failed", __func__);
                CBlockHeader header;
                try {
                    file >> header;
                    fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                    file >> txOut;
                } catch (std::exception& e) {
                    return error("%s : Deserialize or I/O error - %s", __func__, e.what());
                }
                hashBlock = header.GetHash();
                if (txOut.GetHash() != hash)
                    return error("%s : txid mismatch", __func__);
                return true;
            }

            // transaction not found in the index, nothing more can be done
            return false;
        }

        if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
            int nHeight = -1;
            {
                CCoinsViewCache& view = *pcoinsTip;
                const CCoins* coins = view.AccessCoins(hash);
                if (coins)
                    nHeight = coins->nHeight;
            }
            if (nHeight > 0)
                pindexSlow = chainActive[nHeight];
        }
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow)) {
            for (const CTransaction& tx : block.vtx) {
                if (tx.GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHash();
                    return true;
                }
            }
        }
    }

    return false;
}






//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool WriteBlockToDisk(CBlock& block, CDiskBlockPos& pos)
{
    // Open history file to append
    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk : OpenBlockFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(block);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos)
{
    block.SetNull();

    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk : OpenBlockFile failed");

    // Read block
    try {
        filein >> block;
    } catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Check the header
    if (block.IsProofOfWork()) {
        if (!CheckProofOfWork(block.GetHash(), block.nBits))
            return error("ReadBlockFromDisk : Errors in block header");
    }

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex)
{
    if (!ReadBlockFromDisk(block, pindex->GetBlockPos()))
        return false;
    if (block.GetHash() != pindex->GetBlockHash()) {
        LogPrintf("%s : block=%s index=%s\n", __func__, block.GetHash().GetHex(), pindex->GetBlockHash().GetHex());
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*) : GetHash() doesn't match index");
    }
    return true;
}


double ConvertBitsToDouble(unsigned int nBits)
{
    int nShift = (nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(nBits & 0x00ffffff);

    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

int64_t GetBlockValue(int nHeight)
{
    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        if (nHeight < 200 && nHeight > 0)
            return 250000 * COIN;
    }

    if (Params().NetworkID() == CBaseChainParams::REGTEST) {
        if (nHeight == 0)
            return 250 * COIN;

    }

    int64_t nSubsidy = 0;
    if (nHeight == 0) {
        nSubsidy = 60001 * COIN;
    } else if (nHeight < 86400 && nHeight > 0) {
        nSubsidy = 250 * COIN;
    } else if (nHeight < (Params().NetworkID() == CBaseChainParams::TESTNET ? 145000 : 151200) && nHeight >= 86400) {
        nSubsidy = 225 * COIN;
    } else if (nHeight <= Params().LAST_POW_BLOCK() && nHeight >= 151200) {
        nSubsidy = 45 * COIN;
    } else if (nHeight <= 302399 && nHeight > Params().LAST_POW_BLOCK()) {
        nSubsidy = 45 * COIN;
    } else if (nHeight <= 345599 && nHeight >= 302400) {
        nSubsidy = 40.5 * COIN;
    } else if (nHeight <= 388799 && nHeight >= 345600) {
        nSubsidy = 36 * COIN;
    } else if (nHeight <= 431999 && nHeight >= 388800) {
        nSubsidy = 31.5 * COIN;
    } else if (nHeight <= 475199 && nHeight >= 432000) {
        nSubsidy = 27 * COIN;
    } else if (nHeight <= 518399 && nHeight >= 475200) {
        nSubsidy = 22.5 * COIN;
    } else if (nHeight <= 561599 && nHeight >= 518400) {
        nSubsidy = 18 * COIN;
    } else if (nHeight <= 604799 && nHeight >= 561600) {
        nSubsidy = 13.5 * COIN;
    } else if (nHeight <= 647999 && nHeight >= 604800) {
        nSubsidy = 9 * COIN;
    } else if (nHeight < Params().Zerocoin_Block_V2_Start()) {
        nSubsidy = 4.5 * COIN;
    } else {
        nSubsidy = 5 * COIN;
    }
    return nSubsidy;
}

CAmount GetSeeSaw(const CAmount& blockValue, int nMasternodeCount, int nHeight)
{
    //if a mn count is inserted into the function we are looking for a specific result for a masternode count
    if (nMasternodeCount < 1){
        if (IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT))
            nMasternodeCount = mnodeman.stable_size();
        else
            nMasternodeCount = mnodeman.size();
    }

    int64_t nMoneySupply = chainActive.Tip()->nMoneySupply;
    int64_t mNodeCoins = nMasternodeCount * 10000 * COIN;

    // Use this log to compare the masternode count for different clients
    //LogPrintf("Adjusting seesaw at height %d with %d masternodes (without drift: %d) at %ld\n", nHeight, nMasternodeCount, nMasternodeCount - Params().MasternodeCountDrift(), GetTime());

    if (fDebug)
        LogPrintf("GetMasternodePayment(): moneysupply=%s, nodecoins=%s \n", FormatMoney(nMoneySupply).c_str(),
                  FormatMoney(mNodeCoins).c_str());

    CAmount ret = 0;
    if (mNodeCoins == 0) {
        ret = 0;
    } else if (nHeight <= 325000) {
        if (mNodeCoins <= (nMoneySupply * .05) && mNodeCoins > 0) {
            ret = blockValue * .85;
        } else if (mNodeCoins <= (nMoneySupply * .1) && mNodeCoins > (nMoneySupply * .05)) {
            ret = blockValue * .8;
        } else if (mNodeCoins <= (nMoneySupply * .15) && mNodeCoins > (nMoneySupply * .1)) {
            ret = blockValue * .75;
        } else if (mNodeCoins <= (nMoneySupply * .2) && mNodeCoins > (nMoneySupply * .15)) {
            ret = blockValue * .7;
        } else if (mNodeCoins <= (nMoneySupply * .25) && mNodeCoins > (nMoneySupply * .2)) {
            ret = blockValue * .65;
        } else if (mNodeCoins <= (nMoneySupply * .3) && mNodeCoins > (nMoneySupply * .25)) {
            ret = blockValue * .6;
        } else if (mNodeCoins <= (nMoneySupply * .35) && mNodeCoins > (nMoneySupply * .3)) {
            ret = blockValue * .55;
        } else if (mNodeCoins <= (nMoneySupply * .4) && mNodeCoins > (nMoneySupply * .35)) {
            ret = blockValue * .5;
        } else if (mNodeCoins <= (nMoneySupply * .45) && mNodeCoins > (nMoneySupply * .4)) {
            ret = blockValue * .45;
        } else if (mNodeCoins <= (nMoneySupply * .5) && mNodeCoins > (nMoneySupply * .45)) {
            ret = blockValue * .4;
        } else if (mNodeCoins <= (nMoneySupply * .55) && mNodeCoins > (nMoneySupply * .5)) {
            ret = blockValue * .35;
        } else if (mNodeCoins <= (nMoneySupply * .6) && mNodeCoins > (nMoneySupply * .55)) {
            ret = blockValue * .3;
        } else if (mNodeCoins <= (nMoneySupply * .65) && mNodeCoins > (nMoneySupply * .6)) {
            ret = blockValue * .25;
        } else if (mNodeCoins <= (nMoneySupply * .7) && mNodeCoins > (nMoneySupply * .65)) {
            ret = blockValue * .2;
        } else if (mNodeCoins <= (nMoneySupply * .75) && mNodeCoins > (nMoneySupply * .7)) {
            ret = blockValue * .15;
        } else {
            ret = blockValue * .1;
        }
    } else if (nHeight > 325000) {
        if (mNodeCoins <= (nMoneySupply * .01) && mNodeCoins > 0) {
            ret = blockValue * .90;
        } else if (mNodeCoins <= (nMoneySupply * .02) && mNodeCoins > (nMoneySupply * .01)) {
            ret = blockValue * .88;
        } else if (mNodeCoins <= (nMoneySupply * .03) && mNodeCoins > (nMoneySupply * .02)) {
            ret = blockValue * .87;
        } else if (mNodeCoins <= (nMoneySupply * .04) && mNodeCoins > (nMoneySupply * .03)) {
            ret = blockValue * .86;
        } else if (mNodeCoins <= (nMoneySupply * .05) && mNodeCoins > (nMoneySupply * .04)) {
            ret = blockValue * .85;
        } else if (mNodeCoins <= (nMoneySupply * .06) && mNodeCoins > (nMoneySupply * .05)) {
            ret = blockValue * .84;
        } else if (mNodeCoins <= (nMoneySupply * .07) && mNodeCoins > (nMoneySupply * .06)) {
            ret = blockValue * .83;
        } else if (mNodeCoins <= (nMoneySupply * .08) && mNodeCoins > (nMoneySupply * .07)) {
            ret = blockValue * .82;
        } else if (mNodeCoins <= (nMoneySupply * .09) && mNodeCoins > (nMoneySupply * .08)) {
            ret = blockValue * .81;
        } else if (mNodeCoins <= (nMoneySupply * .10) && mNodeCoins > (nMoneySupply * .09)) {
            ret = blockValue * .80;
        } else if (mNodeCoins <= (nMoneySupply * .11) && mNodeCoins > (nMoneySupply * .10)) {
            ret = blockValue * .79;
        } else if (mNodeCoins <= (nMoneySupply * .12) && mNodeCoins > (nMoneySupply * .11)) {
            ret = blockValue * .78;
        } else if (mNodeCoins <= (nMoneySupply * .13) && mNodeCoins > (nMoneySupply * .12)) {
            ret = blockValue * .77;
        } else if (mNodeCoins <= (nMoneySupply * .14) && mNodeCoins > (nMoneySupply * .13)) {
            ret = blockValue * .76;
        } else if (mNodeCoins <= (nMoneySupply * .15) && mNodeCoins > (nMoneySupply * .14)) {
            ret = blockValue * .75;
        } else if (mNodeCoins <= (nMoneySupply * .16) && mNodeCoins > (nMoneySupply * .15)) {
            ret = blockValue * .74;
        } else if (mNodeCoins <= (nMoneySupply * .17) && mNodeCoins > (nMoneySupply * .16)) {
            ret = blockValue * .73;
        } else if (mNodeCoins <= (nMoneySupply * .18) && mNodeCoins > (nMoneySupply * .17)) {
            ret = blockValue * .72;
        } else if (mNodeCoins <= (nMoneySupply * .19) && mNodeCoins > (nMoneySupply * .18)) {
            ret = blockValue * .71;
        } else if (mNodeCoins <= (nMoneySupply * .20) && mNodeCoins > (nMoneySupply * .19)) {
            ret = blockValue * .70;
        } else if (mNodeCoins <= (nMoneySupply * .21) && mNodeCoins > (nMoneySupply * .20)) {
            ret = blockValue * .69;
        } else if (mNodeCoins <= (nMoneySupply * .22) && mNodeCoins > (nMoneySupply * .21)) {
            ret = blockValue * .68;
        } else if (mNodeCoins <= (nMoneySupply * .23) && mNodeCoins > (nMoneySupply * .22)) {
            ret = blockValue * .67;
        } else if (mNodeCoins <= (nMoneySupply * .24) && mNodeCoins > (nMoneySupply * .23)) {
            ret = blockValue * .66;
        } else if (mNodeCoins <= (nMoneySupply * .25) && mNodeCoins > (nMoneySupply * .24)) {
            ret = blockValue * .65;
        } else if (mNodeCoins <= (nMoneySupply * .26) && mNodeCoins > (nMoneySupply * .25)) {
            ret = blockValue * .64;
        } else if (mNodeCoins <= (nMoneySupply * .27) && mNodeCoins > (nMoneySupply * .26)) {
            ret = blockValue * .63;
        } else if (mNodeCoins <= (nMoneySupply * .28) && mNodeCoins > (nMoneySupply * .27)) {
            ret = blockValue * .62;
        } else if (mNodeCoins <= (nMoneySupply * .29) && mNodeCoins > (nMoneySupply * .28)) {
            ret = blockValue * .61;
        } else if (mNodeCoins <= (nMoneySupply * .30) && mNodeCoins > (nMoneySupply * .29)) {
            ret = blockValue * .60;
        } else if (mNodeCoins <= (nMoneySupply * .31) && mNodeCoins > (nMoneySupply * .30)) {
            ret = blockValue * .59;
        } else if (mNodeCoins <= (nMoneySupply * .32) && mNodeCoins > (nMoneySupply * .31)) {
            ret = blockValue * .58;
        } else if (mNodeCoins <= (nMoneySupply * .33) && mNodeCoins > (nMoneySupply * .32)) {
            ret = blockValue * .57;
        } else if (mNodeCoins <= (nMoneySupply * .34) && mNodeCoins > (nMoneySupply * .33)) {
            ret = blockValue * .56;
        } else if (mNodeCoins <= (nMoneySupply * .35) && mNodeCoins > (nMoneySupply * .34)) {
            ret = blockValue * .55;
        } else if (mNodeCoins <= (nMoneySupply * .363) && mNodeCoins > (nMoneySupply * .35)) {
            ret = blockValue * .54;
        } else if (mNodeCoins <= (nMoneySupply * .376) && mNodeCoins > (nMoneySupply * .363)) {
            ret = blockValue * .53;
        } else if (mNodeCoins <= (nMoneySupply * .389) && mNodeCoins > (nMoneySupply * .376)) {
            ret = blockValue * .52;
        } else if (mNodeCoins <= (nMoneySupply * .402) && mNodeCoins > (nMoneySupply * .389)) {
            ret = blockValue * .51;
        } else if (mNodeCoins <= (nMoneySupply * .415) && mNodeCoins > (nMoneySupply * .402)) {
            ret = blockValue * .50;
        } else if (mNodeCoins <= (nMoneySupply * .428) && mNodeCoins > (nMoneySupply * .415)) {
            ret = blockValue * .49;
        } else if (mNodeCoins <= (nMoneySupply * .441) && mNodeCoins > (nMoneySupply * .428)) {
            ret = blockValue * .48;
        } else if (mNodeCoins <= (nMoneySupply * .454) && mNodeCoins > (nMoneySupply * .441)) {
            ret = blockValue * .47;
        } else if (mNodeCoins <= (nMoneySupply * .467) && mNodeCoins > (nMoneySupply * .454)) {
            ret = blockValue * .46;
        } else if (mNodeCoins <= (nMoneySupply * .48) && mNodeCoins > (nMoneySupply * .467)) {
            ret = blockValue * .45;
        } else if (mNodeCoins <= (nMoneySupply * .493) && mNodeCoins > (nMoneySupply * .48)) {
            ret = blockValue * .44;
        } else if (mNodeCoins <= (nMoneySupply * .506) && mNodeCoins > (nMoneySupply * .493)) {
            ret = blockValue * .43;
        } else if (mNodeCoins <= (nMoneySupply * .519) && mNodeCoins > (nMoneySupply * .506)) {
            ret = blockValue * .42;
        } else if (mNodeCoins <= (nMoneySupply * .532) && mNodeCoins > (nMoneySupply * .519)) {
            ret = blockValue * .41;
        } else if (mNodeCoins <= (nMoneySupply * .545) && mNodeCoins > (nMoneySupply * .532)) {
            ret = blockValue * .40;
        } else if (mNodeCoins <= (nMoneySupply * .558) && mNodeCoins > (nMoneySupply * .545)) {
            ret = blockValue * .39;
        } else if (mNodeCoins <= (nMoneySupply * .571) && mNodeCoins > (nMoneySupply * .558)) {
            ret = blockValue * .38;
        } else if (mNodeCoins <= (nMoneySupply * .584) && mNodeCoins > (nMoneySupply * .571)) {
            ret = blockValue * .37;
        } else if (mNodeCoins <= (nMoneySupply * .597) && mNodeCoins > (nMoneySupply * .584)) {
            ret = blockValue * .36;
        } else if (mNodeCoins <= (nMoneySupply * .61) && mNodeCoins > (nMoneySupply * .597)) {
            ret = blockValue * .35;
        } else if (mNodeCoins <= (nMoneySupply * .623) && mNodeCoins > (nMoneySupply * .61)) {
            ret = blockValue * .34;
        } else if (mNodeCoins <= (nMoneySupply * .636) && mNodeCoins > (nMoneySupply * .623)) {
            ret = blockValue * .33;
        } else if (mNodeCoins <= (nMoneySupply * .649) && mNodeCoins > (nMoneySupply * .636)) {
            ret = blockValue * .32;
        } else if (mNodeCoins <= (nMoneySupply * .662) && mNodeCoins > (nMoneySupply * .649)) {
            ret = blockValue * .31;
        } else if (mNodeCoins <= (nMoneySupply * .675) && mNodeCoins > (nMoneySupply * .662)) {
            ret = blockValue * .30;
        } else if (mNodeCoins <= (nMoneySupply * .688) && mNodeCoins > (nMoneySupply * .675)) {
            ret = blockValue * .29;
        } else if (mNodeCoins <= (nMoneySupply * .701) && mNodeCoins > (nMoneySupply * .688)) {
            ret = blockValue * .28;
        } else if (mNodeCoins <= (nMoneySupply * .714) && mNodeCoins > (nMoneySupply * .701)) {
            ret = blockValue * .27;
        } else if (mNodeCoins <= (nMoneySupply * .727) && mNodeCoins > (nMoneySupply * .714)) {
            ret = blockValue * .26;
        } else if (mNodeCoins <= (nMoneySupply * .74) && mNodeCoins > (nMoneySupply * .727)) {
            ret = blockValue * .25;
        } else if (mNodeCoins <= (nMoneySupply * .753) && mNodeCoins > (nMoneySupply * .74)) {
            ret = blockValue * .24;
        } else if (mNodeCoins <= (nMoneySupply * .766) && mNodeCoins > (nMoneySupply * .753)) {
            ret = blockValue * .23;
        } else if (mNodeCoins <= (nMoneySupply * .779) && mNodeCoins > (nMoneySupply * .766)) {
            ret = blockValue * .22;
        } else if (mNodeCoins <= (nMoneySupply * .792) && mNodeCoins > (nMoneySupply * .779)) {
            ret = blockValue * .21;
        } else if (mNodeCoins <= (nMoneySupply * .805) && mNodeCoins > (nMoneySupply * .792)) {
            ret = blockValue * .20;
        } else if (mNodeCoins <= (nMoneySupply * .818) && mNodeCoins > (nMoneySupply * .805)) {
            ret = blockValue * .19;
        } else if (mNodeCoins <= (nMoneySupply * .831) && mNodeCoins > (nMoneySupply * .818)) {
            ret = blockValue * .18;
        } else if (mNodeCoins <= (nMoneySupply * .844) && mNodeCoins > (nMoneySupply * .831)) {
            ret = blockValue * .17;
        } else if (mNodeCoins <= (nMoneySupply * .857) && mNodeCoins > (nMoneySupply * .844)) {
            ret = blockValue * .16;
        } else if (mNodeCoins <= (nMoneySupply * .87) && mNodeCoins > (nMoneySupply * .857)) {
            ret = blockValue * .15;
        } else if (mNodeCoins <= (nMoneySupply * .883) && mNodeCoins > (nMoneySupply * .87)) {
            ret = blockValue * .14;
        } else if (mNodeCoins <= (nMoneySupply * .896) && mNodeCoins > (nMoneySupply * .883)) {
            ret = blockValue * .13;
        } else if (mNodeCoins <= (nMoneySupply * .909) && mNodeCoins > (nMoneySupply * .896)) {
            ret = blockValue * .12;
        } else if (mNodeCoins <= (nMoneySupply * .922) && mNodeCoins > (nMoneySupply * .909)) {
            ret = blockValue * .11;
        } else if (mNodeCoins <= (nMoneySupply * .935) && mNodeCoins > (nMoneySupply * .922)) {
            ret = blockValue * .10;
        } else if (mNodeCoins <= (nMoneySupply * .945) && mNodeCoins > (nMoneySupply * .935)) {
            ret = blockValue * .09;
        } else if (mNodeCoins <= (nMoneySupply * .961) && mNodeCoins > (nMoneySupply * .945)) {
            ret = blockValue * .08;
        } else if (mNodeCoins <= (nMoneySupply * .974) && mNodeCoins > (nMoneySupply * .961)) {
            ret = blockValue * .07;
        } else if (mNodeCoins <= (nMoneySupply * .987) && mNodeCoins > (nMoneySupply * .974)) {
            ret = blockValue * .06;
        } else if (mNodeCoins <= (nMoneySupply * .99) && mNodeCoins > (nMoneySupply * .987)) {
            ret = blockValue * .05;
        } else {
            ret = blockValue * .01;
        }
    }
    return ret;
}

int64_t GetMasternodePayment(int nHeight, int64_t blockValue, int nMasternodeCount, bool isZPIVStake)
{
    int64_t ret = 0;

    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        if (nHeight < 200)
            return 0;
    }

    if (nHeight <= 43200) {
        ret = blockValue / 5;
    } else if (nHeight < 86400 && nHeight > 43200) {
        ret = blockValue / (100 / 30);
    } else if (nHeight < (Params().NetworkID() == CBaseChainParams::TESTNET ? 145000 : 151200) && nHeight >= 86400) {
        ret = 50 * COIN;
    } else if (nHeight <= Params().LAST_POW_BLOCK() && nHeight >= 151200) {
        ret = blockValue / 2;
    } else if (nHeight < Params().Zerocoin_Block_V2_Start()) {
        return GetSeeSaw(blockValue, nMasternodeCount, nHeight);
    } else {
        //When zPIV is staked, masternode only gets 2 PIV
        ret = 3 * COIN;
        if (isZPIVStake)
            ret = 2 * COIN;
    }

    return ret;
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    if (fImporting || fReindex || fVerifyingBlocks || chainActive.Height() < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static bool lockIBDState = false;
    if (lockIBDState)
        return false;
    bool state = (chainActive.Height() < pindexBestHeader->nHeight - 24 * 6 ||
                  pindexBestHeader->GetBlockTime() < GetTime() - nMaxTipAge);
    if (!state)
        lockIBDState = true;
    return state;
}

bool fLargeWorkForkFound = false;
bool fLargeWorkInvalidChainFound = false;
CBlockIndex *pindexBestForkTip = NULL, *pindexBestForkBase = NULL;

void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);
    // Before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before the last checkpoint)
    if (IsInitialBlockDownload())
        return;

    // If our best fork is no longer within 72 blocks (+/- 3 hours if no one mines it)
    // of our head, drop it
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 72)
        pindexBestForkTip = NULL;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6))) {
        if (!fLargeWorkForkFound && pindexBestForkBase) {
            if (pindexBestForkBase->phashBlock) {
                string warning = string("'Warning: Large-work fork detected, forking after block ") +
                                        pindexBestForkBase->phashBlock->ToString() + string("'");
                CAlert::Notify(warning, true);
            }
        }
        if (pindexBestForkTip && pindexBestForkBase) {
            if (pindexBestForkBase->phashBlock) {
                LogPrintf("CheckForkWarningConditions: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n",
                    pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                    pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
                fLargeWorkForkFound = true;
            }
        } else {
            LogPrintf("CheckForkWarningConditions: Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.\n");
            fLargeWorkInvalidChainFound = true;
        }
    } else {
        fLargeWorkForkFound = false;
        fLargeWorkInvalidChainFound = false;
    }
}

void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);
    // If we are on a fork that is sufficiently large, set a warning flag
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger) {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // We define a condition which we should warn the user about as a fork of at least 7 blocks
    // who's tip is within 72 blocks (+/- 3 hours if no one mines it) of ours
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We use 7 blocks rather arbitrarily as it represents just under 10% of sustained network
    // hash rate operating on the fork.
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 7-block condition and from this always have the most-likely-to-cause-warning fork
    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->nHeight > pindexBestForkTip->nHeight)) &&
        pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 7) &&
        chainActive.Height() - pindexNewForkTip->nHeight < 72) {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;

    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n",
        pindexNew->GetBlockHash().ToString(), pindexNew->nHeight,
        log(pindexNew->nChainWork.getdouble()) / log(2.0), DateTimeStrFormat("%Y-%m-%d %H:%M:%S",
                                                               pindexNew->GetBlockTime()));
    LogPrintf("InvalidChainFound:  current best=%s  height=%d  log2_work=%.8g  date=%s\n",
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), log(chainActive.Tip()->nChainWork.getdouble()) / log(2.0),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()));
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex* pindex, const CValidationState& state)
{
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CValidationState& state, CCoinsViewCache& inputs, CTxUndo& txundo, int nHeight)
{
    // mark inputs spent
    if (!tx.IsCoinBase()) {
        txundo.vprevout.reserve(tx.vin.size());
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            txundo.vprevout.push_back(CTxInUndo());
            bool ret = inputs.ModifyCoins(txin.prevout.hash)->Spend(txin.prevout, txundo.vprevout.back());
            assert(ret);
        }
    }

    // add outputs
    inputs.ModifyCoins(tx.GetHash())->FromTx(tx, nHeight);
}

bool CScriptCheck::operator()()
{
    const CScript& scriptSig = ptxTo->vin[nIn].scriptSig;
    if (!VerifyScript(scriptSig, scriptPubKey, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, cacheStore), &error)) {
        return ::error("CScriptCheck(): %s:%d VerifySignature failed: %s", ptxTo->GetHash().ToString(), nIn, ScriptErrorString(error));
    }
    return true;
}

CBitcoinAddress addressExp1("DQZzqnSR6PXxagep1byLiRg9ZurCZ5KieQ");
CBitcoinAddress addressExp2("DTQYdnNqKuEHXyNeeYhPQGGGdqHbXYwjpj");

map<COutPoint, COutPoint> mapInvalidOutPoints;
map<CBigNum, CAmount> mapInvalidSerials;
void AddInvalidSpendsToMap(const CBlock& block)
{
    for (const CTransaction& tx : block.vtx) {
        if (!tx.ContainsZerocoins())
            continue;

        //Check all zerocoinspends for bad serials
        for (const CTxIn& in : tx.vin) {
            bool isPublicSpend = in.IsZerocoinPublicSpend();
            if (in.IsZerocoinSpend() || isPublicSpend) {

                libzerocoin::CoinSpend* spend;
                if (isPublicSpend) {
                    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
                    PublicCoinSpend publicSpend(params);
                    CValidationState state;
                    if (!ZPIVModule::ParseZerocoinPublicSpend(in, tx, state, publicSpend)){
                        throw runtime_error("Failed to parse public spend");
                    }
                    spend = &publicSpend;
                } else {
                    libzerocoin::CoinSpend spendObj = TxInToZerocoinSpend(in);
                    spend = &spendObj;
                }

                //If serial is not valid, mark all outputs as bad
                if (!spend->HasValidSerial(Params().Zerocoin_Params(false))) {
                    mapInvalidSerials[spend->getCoinSerialNumber()] = spend->getDenomination() * COIN;

                    // Derive the actual valid serial from the invalid serial if possible
                    CBigNum bnActualSerial = spend->CalculateValidSerial(Params().Zerocoin_Params(false));
                    uint256 txHash;

                    if (zerocoinDB->ReadCoinSpend(bnActualSerial, txHash)) {
                        mapInvalidSerials[bnActualSerial] = spend->getDenomination() * COIN;

                        CTransaction txPrev;
                        uint256 hashBlock;
                        if (!GetTransaction(txHash, txPrev, hashBlock, true))
                            continue;

                        //Record all txouts from txPrev as invalid
                        for (unsigned int i = 0; i < txPrev.vout.size(); i++) {
                            //map to an empty outpoint to represent that this is the first in the chain of bad outs
                            mapInvalidOutPoints[COutPoint(txPrev.GetHash(), i)] = COutPoint();
                        }
                    }

                    //Record all txouts from this invalid zerocoin spend tx as invalid
                    for (unsigned int i = 0; i < tx.vout.size(); i++) {
                        //map to an empty outpoint to represent that this is the first in the chain of bad outs
                        mapInvalidOutPoints[COutPoint(tx.GetHash(), i)] = COutPoint();
                    }
                }
            }
        }
    }
}

bool ValidOutPoint(const COutPoint out, int nHeight)
{
    bool isInvalid = nHeight >= Params().Block_Enforce_Invalid() && invalid_out::ContainsOutPoint(out);
    return !isInvalid;
}

CAmount GetInvalidUTXOValue()
{
    CAmount nValue = 0;
    for (auto out : invalid_out::setInvalidOutPoints) {
        bool fSpent = false;
        CCoinsViewCache cache(pcoinsTip);
        const CCoins *coins = cache.AccessCoins(out.hash);
        if(!coins || !coins->IsAvailable(out.n))
            fSpent = true;

        if (!fSpent)
            nValue += coins->vout[out.n].nValue;
    }

    return nValue;
}

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

int static inline GetSkipHeight(int height)
{
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}


bool IsStandardTx(const CTransaction& tx, string& reason)
{
    AssertLockHeld(cs_main);
    if (tx.nVersion > CTransaction::CURRENT_VERSION || tx.nVersion < 1) {
        reason = "version";
        return false;
    }

    // Treat non-final transactions as non-standard to prevent a specific type
    // of double-spend attack, as well as DoS attacks. (if the transaction
    // can't be mined, the attacker isn't expending resources broadcasting it)
    // Basically we don't want to propagate transactions that can't be included in
    // the next block.
    //
    // However, IsFinalTx() is confusing... Without arguments, it uses
    // chainActive.Height() to evaluate nLockTime; when a block is accepted, chainActive.Height()
    // is set to the value of nHeight in the block. However, when IsFinalTx()
    // is called within CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a transaction can
    // be part of the *next* block, we need to call IsFinalTx() with one more
    // than chainActive.Height().
    //
    // Timestamps on the other hand don't get any special treatment, because we
    // can't know what timestamp the next block will have, and there aren't
    // timestamp applications where it matters.
    if (!IsFinalTx(tx, chainActive.Height() + 1)) {
        reason = "non-final";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    unsigned int nMaxSize = tx.ContainsZerocoins() ? MAX_ZEROCOIN_TX_SIZE : MAX_STANDARD_TX_SIZE;
    if (sz >= nMaxSize) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx.vin) {
        if (txin.IsZerocoinSpend() || txin.IsZerocoinPublicSpend())
            continue;
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)+3=1627
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)
        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    for (const CTxOut& txout : tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (txout.IsDust(::minRelayTxFee)) {
            reason = "dust";
            return false;
        }
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool CheckInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, vector<CScriptCheck>* pvChecks)
{
    if (!tx.IsCoinBase() && !tx.HasZerocoinSpendInputs()) {
        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

        // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
        // for an attacker to attempt to split the network.
        if (!inputs.HaveInputs(tx))
            return state.Invalid(error("CheckInputs() : %s inputs unavailable", tx.GetHash().ToString()));

        // While checking, GetBestBlock() refers to the parent block.
        // This is also true for mempool checks.
        CBlockIndex* pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
        int nSpendHeight = pindexPrev->nHeight + 1;
        CAmount nValueIn = 0;
        CAmount nFees = 0;
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            const COutPoint& prevout = tx.vin[i].prevout;
            const CCoins* coins = inputs.AccessCoins(prevout.hash);
            assert(coins);

            // If prev is coinbase, check that it's matured
            if (coins->IsCoinBase() || coins->IsCoinStake()) {
                if (nSpendHeight - coins->nHeight < Params().COINBASE_MATURITY())
                    return state.Invalid(
                        error("CheckInputs() : tried to spend coinbase at depth %d, coinstake=%d", nSpendHeight - coins->nHeight, coins->IsCoinStake()),
                        REJECT_INVALID, "bad-txns-premature-spend-of-coinbase");
            }

            // Check for negative or overflow input values
            nValueIn += coins->vout[prevout.n].nValue;
            if (!MoneyRange(coins->vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return state.DoS(100, error("CheckInputs() : txin values out of range"),
                    REJECT_INVALID, "bad-txns-inputvalues-outofrange");
        }

        if (!tx.IsCoinStake()) {
            if (nValueIn < tx.GetValueOut())
                return state.DoS(100, error("CheckInputs() : %s value in (%s) < value out (%s)",
                                          tx.GetHash().ToString(), FormatMoney(nValueIn), FormatMoney(tx.GetValueOut())),
                    REJECT_INVALID, "bad-txns-in-belowout");

            // Tally transaction fees
            CAmount nTxFee = nValueIn - tx.GetValueOut();
            if (nTxFee < 0)
                return state.DoS(100, error("CheckInputs() : %s nTxFee < 0", tx.GetHash().ToString()),
                    REJECT_INVALID, "bad-txns-fee-negative");
            nFees += nTxFee;
            if (!MoneyRange(nFees))
                return state.DoS(100, error("CheckInputs() : nFees out of range"),
                    REJECT_INVALID, "bad-txns-fee-outofrange");
        }
        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.

        // Skip ECDSA signature verification when connecting blocks
        // before the last block chain checkpoint. This is safe because block merkle hashes are
        // still computed and checked, and any change will be caught at the next checkpoint.
        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint& prevout = tx.vin[i].prevout;
                const CCoins* coins = inputs.AccessCoins(prevout.hash);
                assert(coins);

                // Verify signature
                CScriptCheck check(*coins, tx, i, flags, cacheStore);
                if (pvChecks) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS) {
                        // Check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard DER encodings or non-null dummy
                        // arguments; if so, don't trigger DoS protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        CScriptCheck check(*coins, tx, i,
                            flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore);
                        if (check())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
                    // Failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid P2SH. We DoS ban
                    // such nodes as they are not following the protocol. That
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after a soft-fork
                    // super-majority vote has passed.
                    return state.DoS(100, false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }
        }
    }

    return true;
}

bool DisconnectBlock(CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool* pfClean)
{
    if (pindex->GetBlockHash() != view.GetBestBlock())
        LogPrintf("%s : pindex=%s view=%s\n", __func__, pindex->GetBlockHash().GetHex(), view.GetBestBlock().GetHex());
    assert(pindex->GetBlockHash() == view.GetBestBlock());

    if (pfClean)
        *pfClean = false;

    bool fClean = true;

    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull())
        return error("DisconnectBlock() : no undo data available");
    if (!blockUndo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
        return error("DisconnectBlock() : failure reading undo data");

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size())
        return error("DisconnectBlock() : block and undo data inconsistent");

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction& tx = block.vtx[i];

        /** UNDO ZEROCOIN DATABASING
         * note we only undo zerocoin databasing in the following statement, value to and from PIVX
         * addresses should still be handled by the typical bitcoin based undo code
         * */
        if (tx.ContainsZerocoins()) {
            if (tx.HasZerocoinSpendInputs()) {
                //erase all zerocoinspends in this transaction
                for (const CTxIn &txin : tx.vin) {
                    bool isPublicSpend = txin.IsZerocoinPublicSpend();
                    if (txin.scriptSig.IsZerocoinSpend() || isPublicSpend) {
                        CBigNum serial;
                        if (isPublicSpend) {
                            libzerocoin::ZerocoinParams *params = Params().Zerocoin_Params(false);
                            PublicCoinSpend publicSpend(params);
                            CValidationState state;
                            if (!ZPIVModule::ParseZerocoinPublicSpend(txin, tx, state, publicSpend)) {
                                return error("Failed to parse public spend");
                            }
                            serial = publicSpend.getCoinSerialNumber();
                        } else {
                            libzerocoin::CoinSpend spend = TxInToZerocoinSpend(txin);
                            serial = spend.getCoinSerialNumber();
                        }

                        if (!zerocoinDB->EraseCoinSpend(serial))
                            return error("failed to erase spent zerocoin in block");

                        //if this was our spend, then mark it unspent now
                        if (pwalletMain) {
                            if (pwalletMain->IsMyZerocoinSpend(serial)) {
                                if (!pwalletMain->SetMintUnspent(serial))
                                    LogPrintf("%s: failed to automatically reset mint", __func__);
                            }
                        }
                    }

                }
            }

            if (tx.HasZerocoinMintOutputs()) {
                //erase all zerocoinmints in this transaction
                for (const CTxOut &txout : tx.vout) {
                    if (txout.scriptPubKey.empty() || !txout.IsZerocoinMint())
                        continue;

                    libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(false));
                    if (!TxOutToPublicCoin(txout, pubCoin, state))
                        return error("DisconnectBlock(): TxOutToPublicCoin() failed");

                    if (!zerocoinDB->EraseCoinMint(pubCoin.getValue()))
                        return error("DisconnectBlock(): Failed to erase coin mint");
                }
            }
        }

        uint256 hash = tx.GetHash();

        // Check that all outputs are available and match the outputs in the block itself
        // exactly. Note that transactions with only provably unspendable outputs won't
        // have outputs available even in the block itself, so we handle that case
        // specially with outsEmpty.
        {
            CCoins outsEmpty;
            CCoinsModifier outs = view.ModifyCoins(hash);
            outs->ClearUnspendable();

            CCoins outsBlock(tx, pindex->nHeight);
            // The CCoins serialization does not serialize negative numbers.
            // No network rules currently depend on the version here, so an inconsistency is harmless
            // but it must be corrected before txout nversion ever influences a network rule.
            if (outsBlock.nVersion < 0)
                outs->nVersion = outsBlock.nVersion;
            if (*outs != outsBlock)
                fClean = fClean && error("DisconnectBlock() : added transaction mismatch? database corrupted");

            // remove outputs
            outs->Clear();
        }

        // restore inputs
        if (!tx.IsCoinBase() && !tx.HasZerocoinSpendInputs()) { // not coinbases or zerocoinspend because they dont have traditional inputs
            const CTxUndo& txundo = blockUndo.vtxundo[i - 1];
            if (txundo.vprevout.size() != tx.vin.size())
                return error("DisconnectBlock() : transaction and undo data inconsistent - txundo.vprevout.siz=%d tx.vin.siz=%d", txundo.vprevout.size(), tx.vin.size());
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const COutPoint& out = tx.vin[j].prevout;
                const CTxInUndo& undo = txundo.vprevout[j];
                CCoinsModifier coins = view.ModifyCoins(out.hash);
                if (undo.nHeight != 0) {
                    // undo data contains height: this is the last output of the prevout tx being spent
                    if (!coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data overwriting existing transaction");
                    coins->Clear();
                    coins->fCoinBase = undo.fCoinBase;
                    coins->nHeight = undo.nHeight;
                    coins->nVersion = undo.nVersion;
                } else {
                    if (coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data adding output to missing transaction");
                }
                if (coins->IsAvailable(out.n))
                    fClean = fClean && error("DisconnectBlock() : undo data overwriting existing output");
                if (coins->vout.size() < out.n + 1)
                    coins->vout.resize(out.n + 1);
                coins->vout[out.n] = undo.txout;
            }
        }
    }

    // move best block pointer to prevout block
    view.SetBestBlock(pindex->pprev->GetBlockHash());

    if (!fVerifyingBlocks) {
        //if block is an accumulator checkpoint block, remove checkpoint and checksums from db
        uint256 nCheckpoint = pindex->nAccumulatorCheckpoint;
        if(nCheckpoint != pindex->pprev->nAccumulatorCheckpoint) {
            if(!EraseAccumulatorValues(nCheckpoint, pindex->pprev->nAccumulatorCheckpoint))
                return error("DisconnectBlock(): failed to erase checkpoint");
        }
    }

    if (pfClean) {
        *pfClean = fClean;
        return true;
    } else {
        return fClean;
    }
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE* fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }
}

bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck()
{
    RenameThread("pivx-scriptch");
    scriptcheckqueue.Thread();
}

void AddWrappedSerialsInflation()
{
    CBlockIndex* pindex = chainActive[Params().Zerocoin_Block_EndFakeSerial()];
    if (pindex->nHeight > chainActive.Height())
        return;

    uiInterface.ShowProgress(_("Adding Wrapped Serials supply..."), 0);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = max(1, min(99, (int)((double)(pindex->nHeight - Params().Zerocoin_Block_EndFakeSerial()) * 100 / (chainActive.Height() - Params().Zerocoin_Block_EndFakeSerial()))));
            uiInterface.ShowProgress(_("Adding Wrapped Serials supply..."), percent);
        }

        // Add inflated denominations to block index mapSupply
        for (auto denom : libzerocoin::zerocoinDenomList) {
            pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
        }
        // Update current block index to disk
        assert(pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)));
        // next block
        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
}

void RecalculateZPIVMinted()
{
    CBlockIndex *pindex = chainActive[Params().Zerocoin_StartHeight()];
    uiInterface.ShowProgress(_("Recalculating minted ZPIV..."), 0);
    while (true) {
        // Log Message and feedback message every 1000 blocks
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = max(1, min(99, (int)((double)(pindex->nHeight - Params().Zerocoin_StartHeight()) * 100 / (chainActive.Height() - Params().Zerocoin_StartHeight()))));
            uiInterface.ShowProgress(_("Recalculating minted ZPIV..."), percent);
        }

        //overwrite possibly wrong vMintsInBlock data
        CBlock block;
        assert(ReadBlockFromDisk(block, pindex));

        list<CZerocoinMint> listMints;
        BlockToZerocoinMintList(block, listMints, true);

        vector<libzerocoin::CoinDenomination> vDenomsBefore = pindex->vMintDenominationsInBlock;
        pindex->vMintDenominationsInBlock.clear();
        for (auto mint : listMints)
            pindex->vMintDenominationsInBlock.emplace_back(mint.GetDenomination());

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
}

void RecalculateZPIVSpent()
{
    CBlockIndex* pindex = chainActive[Params().Zerocoin_StartHeight()];
    uiInterface.ShowProgress(_("Recalculating spent ZPIV..."), 0);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = max(1, min(99, (int)((double)(pindex->nHeight - Params().Zerocoin_StartHeight()) * 100 / (chainActive.Height() - Params().Zerocoin_StartHeight()))));
            uiInterface.ShowProgress(_("Recalculating spent ZPIV..."), percent);
        }

        //Rewrite zPIV supply
        CBlock block;
        assert(ReadBlockFromDisk(block, pindex));

        list<libzerocoin::CoinDenomination> listDenomsSpent = ZerocoinSpendListFromBlock(block, true);

        //Reset the supply to previous block
        pindex->mapZerocoinSupply = pindex->pprev->mapZerocoinSupply;

        //Add mints to zPIV supply
        for (auto denom : libzerocoin::zerocoinDenomList) {
            long nDenomAdded = count(pindex->vMintDenominationsInBlock.begin(), pindex->vMintDenominationsInBlock.end(), denom);
            pindex->mapZerocoinSupply.at(denom) += nDenomAdded;
        }

        //Remove spends from zPIV supply
        for (auto denom : listDenomsSpent)
            pindex->mapZerocoinSupply.at(denom)--;

        // Add inflation from Wrapped Serials if block is Zerocoin_Block_EndFakeSerial()
        if (pindex->nHeight == Params().Zerocoin_Block_EndFakeSerial() + 1)
            for (auto denom : libzerocoin::zerocoinDenomList) {
                pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
            }

        //Rewrite money supply
        assert(pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)));

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
}

bool RecalculatePIVSupply(int nHeightStart)
{
    if (nHeightStart > chainActive.Height())
        return false;

    CBlockIndex* pindex = chainActive[nHeightStart];
    CAmount nSupplyPrev = pindex->pprev->nMoneySupply;
    if (nHeightStart == Params().Zerocoin_StartHeight())
        nSupplyPrev = CAmount(5449796547496199);

    uiInterface.ShowProgress(_("Recalculating PIV supply..."), 0);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = max(1, min(99, (int)((double)((pindex->nHeight - nHeightStart) * 100) / (chainActive.Height() - nHeightStart))));
            uiInterface.ShowProgress(_("Recalculating PIV supply..."), percent);
        }

        CBlock block;
        assert(ReadBlockFromDisk(block, pindex));

        CAmount nValueIn = 0;
        CAmount nValueOut = 0;
        for (const CTransaction& tx : block.vtx) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                if (tx.IsCoinBase())
                    break;

                if (tx.vin[i].IsZerocoinSpend()) {
                    nValueIn += tx.vin[i].nSequence * COIN;
                    continue;
                }

                COutPoint prevout = tx.vin[i].prevout;
                CTransaction txPrev;
                uint256 hashBlock;
                assert(GetTransaction(prevout.hash, txPrev, hashBlock, true));
                nValueIn += txPrev.vout[prevout.n].nValue;
            }

            for (unsigned int i = 0; i < tx.vout.size(); i++) {
                if (i == 0 && tx.IsCoinStake())
                    continue;

                nValueOut += tx.vout[i].nValue;
            }
        }

        // Rewrite money supply
        pindex->nMoneySupply = nSupplyPrev + nValueOut - nValueIn;
        nSupplyPrev = pindex->nMoneySupply;

        // Add fraudulent funds to the supply and remove any recovered funds.
        if (pindex->nHeight == Params().Zerocoin_Block_RecalculateAccumulators()) {
            LogPrintf("%s : Original money supply=%s\n", __func__, FormatMoney(pindex->nMoneySupply));

            pindex->nMoneySupply += Params().InvalidAmountFiltered();
            LogPrintf("%s : Adding filtered funds to supply + %s : supply=%s\n", __func__, FormatMoney(Params().InvalidAmountFiltered()), FormatMoney(pindex->nMoneySupply));

            CAmount nLocked = GetInvalidUTXOValue();
            pindex->nMoneySupply -= nLocked;
            LogPrintf("%s : Removing locked from supply - %s : supply=%s\n", __func__, FormatMoney(nLocked), FormatMoney(pindex->nMoneySupply));
        }

        assert(pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)));

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100);
    return true;
}

bool ReindexAccumulators(list<uint256>& listMissingCheckpoints, string& strError)
{
    // PIVX: recalculate Accumulator Checkpoints that failed to database properly
    if (!listMissingCheckpoints.empty()) {
        uiInterface.ShowProgress(_("Calculating missing accumulators..."), 0);
        LogPrintf("%s : finding missing checkpoints\n", __func__);

        //search the chain to see when zerocoin started
        int nZerocoinStart = Params().Zerocoin_Block_V2_Start();

        // find each checkpoint that is missing
        CBlockIndex* pindex = chainActive[nZerocoinStart];
        while (pindex) {
            uiInterface.ShowProgress(_("Calculating missing accumulators..."), max(1, min(99, (int)((double)(pindex->nHeight - nZerocoinStart) / (double)(chainActive.Height() - nZerocoinStart) * 100))));

            if (ShutdownRequested())
                return false;

            // find checkpoints by iterating through the blockchain beginning with the first zerocoin block
            if (pindex->nAccumulatorCheckpoint != pindex->pprev->nAccumulatorCheckpoint) {
                if (find(listMissingCheckpoints.begin(), listMissingCheckpoints.end(), pindex->nAccumulatorCheckpoint) != listMissingCheckpoints.end()) {
                    uint256 nCheckpointCalculated = 0;
                    AccumulatorMap mapAccumulators(Params().Zerocoin_Params(false));
                    if (!CalculateAccumulatorCheckpoint(pindex->nHeight, nCheckpointCalculated, mapAccumulators)) {
                        // GetCheckpoint could have terminated due to a shutdown request. Check this here.
                        if (ShutdownRequested())
                            break;
                        strError = _("Failed to calculate accumulator checkpoint");
                        return error("%s: %s", __func__, strError);
                    }

                    //check that the calculated checkpoint is what is in the index.
                    if (nCheckpointCalculated != pindex->nAccumulatorCheckpoint) {
                        LogPrintf("%s : height=%d calculated_checkpoint=%s actual=%s\n", __func__, pindex->nHeight, nCheckpointCalculated.GetHex(), pindex->nAccumulatorCheckpoint.GetHex());
                        strError = _("Calculated accumulator checkpoint is not what is recorded by block index");
                        return error("%s: %s", __func__, strError);
                    }

                    DatabaseChecksums(mapAccumulators);
                    auto it = find(listMissingCheckpoints.begin(), listMissingCheckpoints.end(), pindex->nAccumulatorCheckpoint);
                    listMissingCheckpoints.erase(it);
                }
            }
            pindex = chainActive.Next(pindex);
        }
        uiInterface.ShowProgress("", 100);
    }
    return true;
}

bool UpdateZPIVSupply(const CBlock& block, CBlockIndex* pindex, bool fJustCheck)
{
    list<CZerocoinMint> listMints;
    bool fFilterInvalid = pindex->nHeight >= Params().Zerocoin_Block_RecalculateAccumulators();
    BlockToZerocoinMintList(block, listMints, fFilterInvalid);
    list<libzerocoin::CoinDenomination> listSpends = ZerocoinSpendListFromBlock(block, fFilterInvalid);

    // Initialize zerocoin supply to the supply from previous block
    if (pindex->pprev && pindex->pprev->GetBlockHeader().nVersion > 3) {
        for (auto& denom : libzerocoin::zerocoinDenomList) {
            pindex->mapZerocoinSupply.at(denom) = pindex->pprev->GetZcMints(denom);
        }
    }

    // Track zerocoin money supply
    CAmount nAmountZerocoinSpent = 0;
    pindex->vMintDenominationsInBlock.clear();
    if (pindex->pprev) {
        set<uint256> setAddedToWallet;
        for (auto& m : listMints) {
            libzerocoin::CoinDenomination denom = m.GetDenomination();
            pindex->vMintDenominationsInBlock.push_back(m.GetDenomination());
            pindex->mapZerocoinSupply.at(denom)++;

            //Remove any of our own mints from the mintpool
            if (!fJustCheck && pwalletMain) {
                if (pwalletMain->IsMyMint(m.GetValue())) {
                    pwalletMain->UpdateMint(m.GetValue(), pindex->nHeight, m.GetTxHash(), m.GetDenomination());

                    // Add the transaction to the wallet
                    for (auto& tx : block.vtx) {
                        uint256 txid = tx.GetHash();
                        if (setAddedToWallet.count(txid))
                            continue;
                        if (txid == m.GetTxHash()) {
                            CWalletTx wtx(pwalletMain, tx);
                            wtx.nTimeReceived = block.GetBlockTime();
                            wtx.SetMerkleBranch(block);
                            pwalletMain->AddToWallet(wtx);
                            setAddedToWallet.insert(txid);
                        }
                    }
                }
            }
        }

        for (auto& denom : listSpends) {
            pindex->mapZerocoinSupply.at(denom)--;
            nAmountZerocoinSpent += libzerocoin::ZerocoinDenominationToAmount(denom);

            // zerocoin failsafe
            if (pindex->GetZcMints(denom) < 0)
                return error("Block contains zerocoins that spend more than are in the available supply to spend");
        }
    }

    for (auto& denom : libzerocoin::zerocoinDenomList)
        LogPrint("zero", "%s coins for denomination %d pubcoin %s\n", __func__, denom, pindex->mapZerocoinSupply.at(denom));

    // Update Wrapped Serials amount
    // A one-time event where only the zPIV supply was off (due to serial duplication off-chain on main net)
    if (Params().NetworkID() == CBaseChainParams::MAIN && pindex->nHeight == Params().Zerocoin_Block_EndFakeSerial() + 1
            && pindex->GetZerocoinSupply() < Params().GetSupplyBeforeFakeSerial() + GetWrapppedSerialInflationAmount()) {
        for (auto denom : libzerocoin::zerocoinDenomList) {
            pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
        }
    }
    return true;
}

static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool fJustCheck, bool fAlreadyChecked)
{
    AssertLockHeld(cs_main);
    // Check it again in case a previous version let a bad block in
    if (!fAlreadyChecked && !CheckBlock(block, state, !fJustCheck, !fJustCheck))
        return false;

    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256(0) : pindex->pprev->GetBlockHash();
    if (hashPrevBlock != view.GetBestBlock())
        LogPrintf("%s: hashPrev=%s view=%s\n", __func__, hashPrevBlock.GetHex(), view.GetBestBlock().GetHex());
    assert(hashPrevBlock == view.GetBestBlock());

    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    if (block.GetHash() == Params().HashGenesisBlock()) {
        view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

    if (pindex->nHeight <= Params().LAST_POW_BLOCK() && block.IsProofOfStake())
        return state.DoS(100, error("ConnectBlock() : PoS period not active"),
            REJECT_INVALID, "PoS-early");

    if (pindex->nHeight > Params().LAST_POW_BLOCK() && block.IsProofOfWork())
        return state.DoS(100, error("ConnectBlock() : PoW period ended"),
            REJECT_INVALID, "PoW-ended");

    bool fScriptChecks = pindex->nHeight >= Checkpoints::GetTotalBlocksEstimate();

    // If scripts won't be checked anyways, don't bother seeing if CLTV is activated
    bool fCLTVHasMajority = false;
    if (fScriptChecks && pindex->pprev) {
        fCLTVHasMajority = CBlockIndex::IsSuperMajority(5, pindex->pprev, Params().EnforceBlockUpgradeMajority());
    }

    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
    // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. This prevents exploiting the issue against nodes in their
    // initial block download.
    bool fEnforceBIP30 = (!pindex->phashBlock) || // Enforce on CreateNewBlock invocations which don't have a hash.
                         !((pindex->nHeight == 91842 && pindex->GetBlockHash() == uint256("0x00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec")) ||
                             (pindex->nHeight == 91880 && pindex->GetBlockHash() == uint256("0x00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721")));
    if (fEnforceBIP30) {
        for (const CTransaction& tx : block.vtx) {
            const CCoins* coins = view.AccessCoins(tx.GetHash());
            if (coins && !coins->IsPruned())
                return state.DoS(100, error("ConnectBlock() : tried to overwrite transaction"),
                    REJECT_INVALID, "bad-txns-BIP30");
        }
    }

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    int64_t nTimeStart = GetTimeMicros();
    CAmount nFees = 0;
    int nInputs = 0;
    unsigned int nSigOps = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    vector<pair<uint256, CDiskTxPos> > vPos;
    vector<pair<libzerocoin::CoinSpend, uint256> > vSpends;
    vector<pair<libzerocoin::PublicCoin, uint256> > vMints;
    vPos.reserve(block.vtx.size());
    CBlockUndo blockundo;
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    CAmount nValueOut = 0;
    CAmount nValueIn = 0;
    unsigned int nMaxBlockSigOps = MAX_BLOCK_SIGOPS_CURRENT;
    vector<uint256> vSpendsInBlock;
    uint256 hashBlock = block.GetHash();
    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = block.vtx[i];

        nInputs += tx.vin.size();
        nSigOps += GetLegacySigOpCount(tx);
        if (nSigOps > nMaxBlockSigOps)
            return state.DoS(100, error("ConnectBlock() : too many sigops"), REJECT_INVALID, "bad-blk-sigops");

        //Temporarily disable zerocoin transactions for maintenance
        if (block.nTime > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE) && !IsInitialBlockDownload() && tx.ContainsZerocoins()) {
            return state.DoS(100, error("ConnectBlock() : zerocoin transactions are currently in maintenance mode"));
        }

        if (tx.HasZerocoinSpendInputs()) {
            int nHeightTx = 0;
            uint256 txid = tx.GetHash();
            vSpendsInBlock.emplace_back(txid);
            if (IsTransactionInChain(txid, nHeightTx)) {
                //when verifying blocks on init, the blocks are scanned without being disconnected - prevent that from causing an error
                if (!fVerifyingBlocks || (fVerifyingBlocks && pindex->nHeight > nHeightTx))
                    return state.DoS(100, error("%s : txid %s already exists in block %d , trying to include it again in block %d", __func__,
                                                tx.GetHash().GetHex(), nHeightTx, pindex->nHeight),
                                     REJECT_INVALID, "bad-txns-inputs-missingorspent");
            }

            //Check for double spending of serial #'s
            set<CBigNum> setSerials;
            for (const CTxIn& txIn : tx.vin) {
                bool isPublicSpend = txIn.IsZerocoinPublicSpend();
                bool isPrivZerocoinSpend = txIn.IsZerocoinSpend();
                if (!isPrivZerocoinSpend && !isPublicSpend)
                    continue;

                // Check enforcement
                if (!CheckPublicCoinSpendEnforced(pindex->nHeight, isPublicSpend)){
                    return false;
                }

                if (isPublicSpend) {
                    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
                    PublicCoinSpend publicSpend(params);
                    if (!ZPIVModule::ParseZerocoinPublicSpend(txIn, tx, state, publicSpend)){
                        return false;
                    }
                    nValueIn += publicSpend.getDenomination() * COIN;
                    //queue for db write after the 'justcheck' section has concluded
                    vSpends.emplace_back(make_pair(publicSpend, tx.GetHash()));
                    if (!ContextualCheckZerocoinSpend(tx, &publicSpend, pindex, hashBlock))
                        return state.DoS(100, error("%s: failed to add block %s with invalid public zc spend", __func__, tx.GetHash().GetHex()), REJECT_INVALID);
                } else {
                    libzerocoin::CoinSpend spend = TxInToZerocoinSpend(txIn);
                    nValueIn += spend.getDenomination() * COIN;
                    //queue for db write after the 'justcheck' section has concluded
                    vSpends.emplace_back(make_pair(spend, tx.GetHash()));
                    if (!ContextualCheckZerocoinSpend(tx, &spend, pindex, hashBlock))
                        return state.DoS(100, error("%s: failed to add block %s with invalid zerocoinspend", __func__, tx.GetHash().GetHex()), REJECT_INVALID);
                }
            }

            // Check that zPIV mints are not already known
            if (tx.HasZerocoinMintOutputs()) {
                for (auto& out : tx.vout) {
                    if (!out.IsZerocoinMint())
                        continue;

                    libzerocoin::PublicCoin coin(Params().Zerocoin_Params(false));
                    if (!TxOutToPublicCoin(out, coin, state))
                        return state.DoS(100, error("%s: failed final check of zerocoinmint for tx %s", __func__, tx.GetHash().GetHex()));

                    if (!ContextualCheckZerocoinMint(tx, coin, pindex))
                        return state.DoS(100, error("%s: zerocoin mint failed contextual check", __func__));

                    vMints.emplace_back(make_pair(coin, tx.GetHash()));
                }
            }
        } else if (!tx.IsCoinBase()) {
            if (!view.HaveInputs(tx))
                return state.DoS(100, error("ConnectBlock() : inputs missing/spent"),
                    REJECT_INVALID, "bad-txns-inputs-missingorspent");

            // Check that the inputs are not marked as invalid/fraudulent
            for (CTxIn in : tx.vin) {
                if (!ValidOutPoint(in.prevout, pindex->nHeight)) {
                    return state.DoS(100, error("%s : tried to spend invalid input %s in tx %s", __func__, in.prevout.ToString(),
                                  tx.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-inputs");
                }
            }

            // Check that zPIV mints are not already known
            if (tx.HasZerocoinMintOutputs()) {
                for (auto& out : tx.vout) {
                    if (!out.IsZerocoinMint())
                        continue;

                    libzerocoin::PublicCoin coin(Params().Zerocoin_Params(false));
                    if (!TxOutToPublicCoin(out, coin, state))
                        return state.DoS(100, error("%s: failed final check of zerocoinmint for tx %s", __func__, tx.GetHash().GetHex()));

                    if (!ContextualCheckZerocoinMint(tx, coin, pindex))
                        return state.DoS(100, error("%s: zerocoin mint failed contextual check", __func__));

                    vMints.emplace_back(make_pair(coin, tx.GetHash()));
                }
            }

            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += GetP2SHSigOpCount(tx, view);
            if (nSigOps > nMaxBlockSigOps)
                return state.DoS(100, error("ConnectBlock() : too many sigops"), REJECT_INVALID, "bad-blk-sigops");

            if (!tx.IsCoinStake())
                nFees += view.GetValueIn(tx) - tx.GetValueOut();
            nValueIn += view.GetValueIn(tx);

            vector<CScriptCheck> vChecks;
            unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG;
            if (fCLTVHasMajority)
                flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;

            if (!CheckInputs(tx, state, view, fScriptChecks, flags, false, nScriptCheckThreads ? &vChecks : NULL))
                return false;
            control.Add(vChecks);
        }
        nValueOut += tx.GetValueOut();

        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, state, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->nHeight);

        vPos.push_back(make_pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }

    //A one-time event where money supply counts were off and recalculated on a certain block.
    if (pindex->nHeight == Params().Zerocoin_Block_RecalculateAccumulators() + 1) {
        RecalculateZPIVMinted();
        RecalculateZPIVSpent();
        RecalculatePIVSupply(Params().Zerocoin_StartHeight());
    }

    //Track zPIV money supply in the block index
    if (!UpdateZPIVSupply(block, pindex, fJustCheck))
        return state.DoS(100, error("%s: Failed to calculate new zPIV supply for block=%s height=%d", __func__,
                                    block.GetHash().GetHex(), pindex->nHeight), REJECT_INVALID);

    // track money supply and mint amount info
    CAmount nMoneySupplyPrev = pindex->pprev ? pindex->pprev->nMoneySupply : 0;
    pindex->nMoneySupply = nMoneySupplyPrev + nValueOut - nValueIn;
    pindex->nMint = pindex->nMoneySupply - nMoneySupplyPrev + nFees;

//    LogPrintf("XX69----------> ConnectBlock(): nValueOut: %s, nValueIn: %s, nFees: %s, nMint: %s zPivSpent: %s\n",
//              FormatMoney(nValueOut), FormatMoney(nValueIn),
//              FormatMoney(nFees), FormatMoney(pindex->nMint), FormatMoney(nAmountZerocoinSpent));

    int64_t nTime1 = GetTimeMicros();
    nTimeConnect += nTime1 - nTimeStart;
    LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime1 - nTimeStart), 0.001 * (nTime1 - nTimeStart) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime1 - nTimeStart) / (nInputs - 1), nTimeConnect * 0.000001);

    //PoW phase redistributed fees to miner. PoS stage destroys fees.
    CAmount nExpectedMint = GetBlockValue(pindex->pprev->nHeight);
    if (block.IsProofOfWork())
        nExpectedMint += nFees;

    //Check that the block does not overmint
    if (!IsBlockValueValid(block, nExpectedMint, pindex->nMint)) {
        return state.DoS(100, error("ConnectBlock() : reward pays too much (actual=%s vs limit=%s)",
                FormatMoney(pindex->nMint), FormatMoney(nExpectedMint)),
            REJECT_INVALID, "bad-cb-amount");
    }

    // Ensure that accumulator checkpoints are valid and in the same state as this instance of the chain
    AccumulatorMap mapAccumulators(Params().Zerocoin_Params(pindex->nHeight < Params().Zerocoin_Block_V2_Start()));
    if (!ValidateAccumulatorCheckpoint(block, pindex, mapAccumulators)) {
        if (!ShutdownRequested()) {
            return state.DoS(100, error("%s: Failed to validate accumulator checkpoint for block=%s height=%d", __func__,
                                   block.GetHash().GetHex(), pindex->nHeight), REJECT_INVALID, "bad-acc-checkpoint");
        }
        return error("%s: Failed to validate accumulator checkpoint for block=%s height=%d because wallet is shutting down", __func__,
                block.GetHash().GetHex(), pindex->nHeight);
    }

    if (!control.Wait())
        return state.DoS(100, error("%s: CheckQueue failed", __func__), REJECT_INVALID, "block-validation-failed");
    int64_t nTime2 = GetTimeMicros();
    nTimeVerify += nTime2 - nTimeStart;
    LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime2 - nTimeStart), nInputs <= 1 ? 0 : 0.001 * (nTime2 - nTimeStart) / (nInputs - 1), nTimeVerify * 0.000001);

    //IMPORTANT NOTE: Nothing before this point should actually store to disk (or even memory)
    if (fJustCheck)
        return true;

    // Write undo information to disk
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos pos;
            if (!FindUndoPos(state, pindex->nFile, pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock() : FindUndoPos failed");
            if (!blockundo.WriteToDisk(pos, pindex->pprev->GetBlockHash()))
                return AbortNode("Failed to write undo data");

            // update nUndoPos in block index
            pindex->nUndoPos = pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    //Record zPIV serials
    if (pwalletMain) {
        set<uint256> setAddedTx;
        for (pair<libzerocoin::CoinSpend, uint256> pSpend : vSpends) {
            // Send signal to wallet if this is ours
            if (pwalletMain->IsMyZerocoinSpend(pSpend.first.getCoinSerialNumber())) {
                LogPrintf("%s: %s detected zerocoinspend in transaction %s \n", __func__,
                          pSpend.first.getCoinSerialNumber().GetHex(), pSpend.second.GetHex());
                pwalletMain->NotifyZerocoinChanged(pwalletMain, pSpend.first.getCoinSerialNumber().GetHex(), "Used",
                                                   CT_UPDATED);

                //Don't add the same tx multiple times
                if (setAddedTx.count(pSpend.second))
                    continue;

                //Search block for matching tx, turn into wtx, set merkle branch, add to wallet
                for (CTransaction tx : block.vtx) {
                    if (tx.GetHash() == pSpend.second) {
                        CWalletTx wtx(pwalletMain, tx);
                        wtx.nTimeReceived = pindex->GetBlockTime();
                        wtx.SetMerkleBranch(block);
                        pwalletMain->AddToWallet(wtx);
                        setAddedTx.insert(pSpend.second);
                    }
                }
            }
        }
    }

    // Flush spend/mint info to disk
    if (!zerocoinDB->WriteCoinSpendBatch(vSpends)) return AbortNode(("Failed to record coin serials to database"));
    if (!zerocoinDB->WriteCoinMintBatch(vMints)) return AbortNode(("Failed to record new mints to database"));

    //Record accumulator checksums
    DatabaseChecksums(mapAccumulators);

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return AbortNode("Failed to write transaction index");

    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime3 = GetTimeMicros();
    nTimeIndex += nTime3 - nTime2;
    LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2), nTimeIndex * 0.000001);

    // Watch for changes to the previous coinbase transaction.
    static uint256 hashPrevBestCoinBase;
    GetMainSignals().UpdatedTransaction(hashPrevBestCoinBase);
    hashPrevBestCoinBase = block.vtx[0].GetHash();

    int64_t nTime4 = GetTimeMicros();
    nTimeCallbacks += nTime4 - nTime3;
    LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3), nTimeCallbacks * 0.000001);

    //Continue tracking possible movement of fraudulent funds until they are completely frozen
    if (pindex->nHeight >= Params().Zerocoin_Block_FirstFraudulent() && pindex->nHeight <= Params().Zerocoin_Block_RecalculateAccumulators() + 1)
        AddInvalidSpendsToMap(block);

    //Remove zerocoinspends from the pending map
    for (const uint256& txid : vSpendsInBlock) {
        auto it = mapZerocoinspends.find(txid);
        if (it != mapZerocoinspends.end())
            mapZerocoinspends.erase(it);
    }

    return true;
}

bool static FlushStateToDisk(CValidationState& state, FlushStateMode mode)
{
    LOCK(cs_main);
    static int64_t nLastWrite = 0;
    try {
        if ((mode == FLUSH_STATE_ALWAYS) ||
            ((mode == FLUSH_STATE_PERIODIC || mode == FLUSH_STATE_IF_NEEDED) && pcoinsTip->GetCacheSize() > nCoinCacheSize) ||
            (mode == FLUSH_STATE_PERIODIC && GetTimeMicros() > nLastWrite + DATABASE_WRITE_INTERVAL * 1000000)) {
            // Typical CCoins structures on disk are around 100 bytes in size.
            // Pushing a new one to the database can cause it to be written
            // twice (once in the log, and once in the tables). This is already
            // an overestimation, as most will delete an existing entry or
            // overwrite one. Still, use a conservative safety factor of 2.
            if (!CheckDiskSpace(100 * 2 * 2 * pcoinsTip->GetCacheSize()))
                return state.Error("out of disk space");
            // First make sure all block and undo data is flushed to disk.
            FlushBlockFile();
            // Then update all block file information (which may refer to block and undo files).
            {
                vector<pair<int, const CBlockFileInfo*> > vFiles;
                vFiles.reserve(setDirtyFileInfo.size());
                for (set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end(); ) {
                    vFiles.push_back(make_pair(*it, &vinfoBlockFile[*it]));
                setDirtyFileInfo.erase(it++);
            }
                vector<const CBlockIndex*> vBlocks;
                vBlocks.reserve(setDirtyBlockIndex.size());
                for (set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end(); ) {
                    vBlocks.push_back(*it);
                    setDirtyBlockIndex.erase(it++);
            }
                if (!pblocktree->WriteBatchSync(vFiles, nLastBlockFile, vBlocks)) {
                    return AbortNode("Files to write to block index database");
                }
            }
            // Finally flush the chainstate (which may refer to block index entries).
            if (!pcoinsTip->Flush())
                return AbortNode("Failed to write to coin database");
            // Update best block in wallet (so we can detect restored wallets).
            if (mode != FLUSH_STATE_IF_NEEDED) {
                GetMainSignals().SetBestChain(chainActive.GetLocator());
            }
            nLastWrite = GetTimeMicros();
        }
    } catch (const runtime_error& e) {
        return AbortNode(string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk()
{
    CValidationState state;
    FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
}

void PruneAndFlush() {
    CValidationState state;
    fCheckForPruning = true;
    FlushStateToDisk(state, FLUSH_STATE_NONE);
}

void static UpdateTip(CBlockIndex* pindexNew)
{
    chainActive.SetTip(pindexNew);

    // New best block
    mempool.AddTransactionsUpdated(1);

    LogPrintf("UpdateTip: new best=%s  height=%d  log2_work=%.8g  tx=%lu  date=%s progress=%f  cache=%u\n",
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), log(chainActive.Tip()->nChainWork.getdouble()) / log(2.0), (unsigned long)chainActive.Tip()->nChainTx,
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        Checkpoints::GuessVerificationProgress(chainActive.Tip()), (unsigned int)pcoinsTip->GetCacheSize());

    cvBlockChange.notify_all();

    // Check the version of the last 100 blocks to see if we need to upgrade:
    static bool fWarned = false;
    if (!IsInitialBlockDownload() && !fWarned) {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int i = 0; i < 100 && pindex != NULL; i++) {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            LogPrintf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded, (int)CBlock::CURRENT_VERSION);
        if (nUpgraded > 100 / 2) {
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
            CAlert::Notify(strMiscWarning, true);
            fWarned = true;
        }
    }
}

bool static DisconnectTip(CValidationState& state)
{
    CBlockIndex* pindexDelete = chainActive.Tip();
    assert(pindexDelete);
    mempool.check(pcoinsTip);
    // Read block from disk.
    CBlock block;
    if (!ReadBlockFromDisk(block, pindexDelete))
        return AbortNode("Failed to read block");
    // Apply the block atomically to the chain state.
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);
        if (!DisconnectBlock(block, state, pindexDelete, view, NULL))
            return error("DisconnectTip() : DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        assert(view.Flush());
    }
    LogPrint("bench", "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_ALWAYS))
        return false;
    // Resurrect mempool transactions from the disconnected block.
    for (const CTransaction& tx : block.vtx) {
        // ignore validation errors in resurrected transactions
        list<CTransaction> removed;
        CValidationState stateDummy;
        if (tx.IsCoinBase() || tx.IsCoinStake() || !AcceptToMemoryPool(mempool, stateDummy, tx, false, NULL))
            mempool.remove(tx, removed, true);
    }
    mempool.removeCoinbaseSpends(pcoinsTip, pindexDelete->nHeight);
    mempool.check(pcoinsTip);
    // Update chainActive and related variables.
    UpdateTip(pindexDelete->pprev);
    // Let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    for (const CTransaction& tx : block.vtx) {
        SyncWithWallets(tx, NULL);
    }
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

bool static ConnectTip(CValidationState& state, CBlockIndex* pindexNew, CBlock* pblock)
{
    assert(pindexNew->pprev == chainActive.Tip());
    mempool.check(pcoinsTip);
    CCoinsViewCache view(pcoinsTip);

    // Read block from disk.
    int64_t nTime1 = GetTimeMicros();
    CBlock block;
    if (!pblock) {
        if (!ReadBlockFromDisk(block, pindexNew))
            return AbortNode("Failed to read block");
        pblock = &block;
    }
    // Apply the block atomically to the chain state.
    int64_t nTime2 = GetTimeMicros();
    nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint("bench", "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
    {
        CInv inv(MSG_BLOCK, pindexNew->GetBlockHash());
        bool rv = ConnectBlock(*pblock, state, pindexNew, view, false);
        GetMainSignals().BlockChecked(*pblock, state);
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("ConnectTip() : ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        mapBlockSource.erase(inv.hash);
        nTime3 = GetTimeMicros();
        nTimeConnectTotal += nTime3 - nTime2;
        LogPrint("bench", "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        assert(view.Flush());
    }
    int64_t nTime4 = GetTimeMicros();
    nTimeFlush += nTime4 - nTime3;
    LogPrint("bench", "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);

    // Write the chain state to disk, if necessary. Always write to disk if this is the first of a new file.
    FlushStateMode flushMode = FLUSH_STATE_IF_NEEDED;
    if (pindexNew->pprev && (pindexNew->GetBlockPos().nFile != pindexNew->pprev->GetBlockPos().nFile))
        flushMode = FLUSH_STATE_ALWAYS;
    if (!FlushStateToDisk(state, flushMode))
        return false;
    int64_t nTime5 = GetTimeMicros();
    nTimeChainState += nTime5 - nTime4;
    LogPrint("bench", "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);

    // Remove conflicting transactions from the mempool.
    list<CTransaction> txConflicted;
    mempool.removeForBlock(pblock->vtx, pindexNew->nHeight, txConflicted);
    mempool.check(pcoinsTip);
    // Update chainActive & related variables.
    UpdateTip(pindexNew);
    // Tell wallet about transactions that went from mempool
    // to conflicted:
    for (const CTransaction& tx : txConflicted) {
        SyncWithWallets(tx, NULL);
    }
    // ... and about transactions that got confirmed:
    for (const CTransaction& tx : pblock->vtx) {
        SyncWithWallets(tx, pblock);
    }

    int64_t nTime6 = GetTimeMicros();
    nTimePostConnect += nTime6 - nTime5;
    nTimeTotal += nTime6 - nTime1;
    LogPrint("bench", "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    LogPrint("bench", "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);
    return true;
}

bool DisconnectBlocksAndReprocess(int blocks)
{
    LOCK(cs_main);

    CValidationState state;

    LogPrintf("DisconnectBlocksAndReprocess: Got command to replay %d blocks\n", blocks);
    for (int i = 0; i <= blocks; i++)
        DisconnectTip(state);

    return true;
}

/*
    DisconnectBlockAndInputs

    Remove conflicting blocks for successful SwiftX transaction locks
    This should be very rare (Probably will never happen)
*/
// ***TODO*** clean up here
bool DisconnectBlockAndInputs(CValidationState& state, CTransaction txLock)
{
    // All modifications to the coin state will be done in this cache.
    // Only when all have succeeded, we push it to pcoinsTip.
    //    CCoinsViewCache view(*pcoinsTip, true);

    CBlockIndex* BlockReading = chainActive.Tip();
    CBlockIndex* pindexNew = NULL;

    bool foundConflictingTx = false;

    //remove anything conflicting in the memory pool
    list<CTransaction> txConflicted;
    mempool.removeConflicts(txLock, txConflicted);


    // List of what to disconnect (typically nothing)
    vector<CBlockIndex*> vDisconnect;

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0 && !foundConflictingTx && i < 6; i++) {
        vDisconnect.push_back(BlockReading);
        pindexNew = BlockReading->pprev; //new best block

        CBlock block;
        if (!ReadBlockFromDisk(block, BlockReading))
            return AbortNode(_("Failed to read block"));

        // Queue memory transactions to resurrect.
        // We only do this for blocks after the last checkpoint (reorganisation before that
        // point should only happen with -reindex/-loadblock, or a misbehaving peer.
        for (const CTransaction& tx : block.vtx) {
            if (!tx.IsCoinBase()) {
                for (const CTxIn& in1 : txLock.vin) {
                    for (const CTxIn& in2 : tx.vin) {
                        if (in1.prevout == in2.prevout) foundConflictingTx = true;
                    }
                }
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    if (!foundConflictingTx) {
        LogPrintf("DisconnectBlockAndInputs: Can't find a conflicting transaction to inputs\n");
        return false;
    }

    if (vDisconnect.size() > 0) {
        LogPrintf("REORGANIZE: Disconnect Conflicting Blocks %lli blocks; %s..\n", vDisconnect.size(), pindexNew->GetBlockHash().ToString());
        for (CBlockIndex* pindex : vDisconnect) {
            LogPrintf(" -- disconnect %s\n", pindex->GetBlockHash().ToString());
            DisconnectTip(state);
        }
    }

    return true;
}


/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain()
{
    do {
        CBlockIndex* pindexNew = NULL;

        // Find the best candidate header.
        {
            set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
        }

        // Check whether all blocks on the path between the currently active chain and the candidate are valid.
        // Just going until the active chain is an optimization, as we know all blocks in it are valid already.
        CBlockIndex* pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nChainTx || pindexTest->nHeight == 0);

            // Pruned nodes may have entries in setBlockIndexCandidates for
            // which block files have been deleted.  Remove those as candidates
            // for the most work chain if we come across them; we can't switch
            // to a chain unless we have all the non-active-chain parent blocks.
            bool fFailedChain = pindexTest->nStatus & BLOCK_FAILED_MASK;
            bool fMissingData = !(pindexTest->nStatus & BLOCK_HAVE_DATA);
            if (fFailedChain || fMissingData) {
                // Candidate chain is not usable (either invalid or missing data)
                if (fFailedChain && (pindexBestInvalid == NULL || pindexNew->nChainWork > pindexBestInvalid->nChainWork))
                    pindexBestInvalid = pindexNew;
                CBlockIndex* pindexFailed = pindexNew;
                // Remove the entire chain from the set.
                while (pindexTest != pindexFailed) {
                    if (fFailedChain) {
                        pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    } else if (fMissingData) {
                        // If we're missing data, then add back to mapBlocksUnlinked,
                        // so that if the block arrives in the future we can try adding
                        // to setBlockIndexCandidates again.
                        mapBlocksUnlinked.insert(make_pair(pindexFailed->pprev, pindexFailed));
                    }
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while (true);
}

uint64_t CalculateCurrentUsage()
{
    uint64_t retval = 0;
    BOOST_FOREACH(const CBlockFileInfo &file, vinfoBlockFile) {
        retval += file.nSize + file.nUndoSize;
    }
    return retval;
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates()
{
    // Note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.Tip())) {
        setBlockIndexCandidates.erase(it++);
    }
    // Either the current tip or a successor of it we're working towards is left in setBlockIndexCandidates.
    assert(!setBlockIndexCandidates.empty());
}

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(CValidationState& state, CBlockIndex* pindexMostWork, CBlock* pblock, bool fAlreadyChecked)
{
    AssertLockHeld(cs_main);
    if (pblock == NULL)
        fAlreadyChecked = false;
    bool fInvalidFound = false;
    const CBlockIndex* pindexOldTip = chainActive.Tip();
    const CBlockIndex* pindexFork = chainActive.FindFork(pindexMostWork);

    // Disconnect active blocks which are no longer in the best chain.
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state))
            return false;
    }

    // Build list of new blocks to connect.
    vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
        // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
        // a few blocks along the way.
        int nTargetHeight = min(nHeight + 32, pindexMostWork->nHeight);
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex* pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->nHeight != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

        // Connect new blocks.
        BOOST_REVERSE_FOREACH (CBlockIndex* pindexConnect, vpindexToConnect) {
            if (!ConnectTip(state, pindexConnect, pindexConnect == pindexMostWork ? pblock : NULL)) {
                if (state.IsInvalid()) {
                    // The block violates a consensus rule.
                    if (!state.CorruptionPossible())
                        InvalidChainFound(vpindexToConnect.back());
                    state = CValidationState();
                    fInvalidFound = true;
                    fContinue = false;
                    break;
                } else {
                    // A system error occurred (disk space, database error, ...).
                    return false;
                }
            } else {
                PruneBlockIndexCandidates();
                if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {
                    // We're in a better position than we were. Return temporarily to release the lock.
                    fContinue = false;
                    break;
                }
            }
        }
    }

    // Callbacks/notifications for a new best chain.
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(CValidationState& state, CBlock* pblock, bool fAlreadyChecked)
{
    CBlockIndex* pindexNewTip = NULL;
    CBlockIndex* pindexMostWork = NULL;
    do {
        boost::this_thread::interruption_point();

        bool fInitialDownload;
        while (true) {
            TRY_LOCK(cs_main, lockMain);
            if (!lockMain) {
                MilliSleep(50);
                continue;
            }

            pindexMostWork = FindMostWorkChain();

            // Whether we have anything to do at all.
            if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                return true;

            if (!ActivateBestChainStep(state, pindexMostWork, pblock && pblock->GetHash() == pindexMostWork->GetBlockHash() ? pblock : NULL, fAlreadyChecked))
                return false;

            pindexNewTip = chainActive.Tip();
            fInitialDownload = IsInitialBlockDownload();
            break;
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).

        // Notifications/callbacks that can run without cs_main
        if (!fInitialDownload) {
            uint256 hashNewTip = pindexNewTip->GetBlockHash();
            // Relay inventory, but don't relay old inventory during initial block download.
            int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
            {
                LOCK(cs_vNodes);
                for (CNode* pnode : vNodes)
                    if (chainActive.Height() > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                        pnode->PushInventory(CInv(MSG_BLOCK, hashNewTip));
            }
            // Notify external listeners about the new tip.
            // Note: uiInterface, should switch main signals.
            uiInterface.NotifyBlockTip(hashNewTip);
            GetMainSignals().UpdatedBlockTip(pindexNewTip);

            unsigned size = 0;
            if (pblock)
                size = GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION);
            // If the size is over 1 MB notify external listeners, and it is within the last 5 minutes
            if (size > MAX_BLOCK_SIZE_LEGACY && pblock->GetBlockTime() > GetAdjustedTime() - 300) {
                uiInterface.NotifyBlockSize(static_cast<int>(size), hashNewTip);
            }
        }
    } while (pindexMostWork != chainActive.Tip());
    CheckBlockIndex();

    // Write changes periodically to disk, after relay.
    if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}

bool InvalidateBlock(CValidationState& state, CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    while (chainActive.Contains(pindex)) {
        CBlockIndex* pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!DisconnectTip(state)) {
            return false;
        }
    }

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add them again.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);
    return true;
}

bool ReconsiderBlock(CValidationState& state, CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

CBlockIndex* AddToBlockIndex(const CBlock& block)
{
    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end())
        return it->second;

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(block);
    assert(pindexNew);
    // We assign the sequence id to blocks only when the full data is available,
    // to avoid miners withholding blocks but broadcasting headers, to get a
    // competitive advantage.
    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;

    //mark as PoS seen
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));

    pindexNew->phashBlock = &((*mi).first);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end()) {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        pindexNew->BuildSkip();

        //update previous block pointer
        pindexNew->pprev->pnext = pindexNew;

        // ppcoin: compute chain trust score
        pindexNew->bnChainTrust = (pindexNew->pprev ? pindexNew->pprev->bnChainTrust : 0) + pindexNew->GetBlockTrust();

        // ppcoin: compute stake entropy bit for stake modifier
        if (!pindexNew->SetStakeEntropyBit(pindexNew->GetStakeEntropyBit()))
            LogPrintf("AddToBlockIndex() : SetStakeEntropyBit() failed \n");

        // ppcoin: record proof-of-stake hash value
        if (pindexNew->IsProofOfStake()) {
            if (!mapProofOfStake.count(hash))
                LogPrintf("AddToBlockIndex() : hashProofOfStake not found in map \n");
            pindexNew->hashProofOfStake = mapProofOfStake[hash];
        }

        if (!Params().IsStakeModifierV2(pindexNew->nHeight)) {
            uint64_t nStakeModifier = 0;
            bool fGeneratedStakeModifier = false;
            if (!ComputeNextStakeModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier))
                LogPrintf("AddToBlockIndex() : ComputeNextStakeModifier() failed \n");
            pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
            pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);
            if (!CheckStakeModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
                LogPrintf("AddToBlockIndex() : Rejected by stake modifier checkpoint height=%d, modifier=%s \n", pindexNew->nHeight, to_string(nStakeModifier));
        } else {
            // compute v2 stake modifier
            pindexNew->nStakeModifierV2 = ComputeStakeModifier(pindexNew->pprev, block.vtx[1].vin[0].prevout.hash);
        }
    }
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == NULL || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    //update previous block pointer
    if (pindexNew->nHeight)
        pindexNew->pprev->pnext = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);

    return pindexNew;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS). */
bool ReceivedBlockTransactions(const CBlock& block, CValidationState& state, CBlockIndex* pindexNew, const CDiskBlockPos& pos)
{
    if (block.IsProofOfStake())
        pindexNew->SetProofOfStake();
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {
        // If pindexNew is the genesis block or all parents are BLOCK_VALID_TRANSACTIONS.
        deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        // Recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            CBlockIndex* pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                LOCK(cs_nBlockSequenceId);
                pindex->nSequenceId = nBlockSequenceId++;
            }
            if (chainActive.Tip() == NULL || !setBlockIndexCandidates.value_comp()(pindex, chainActive.Tip())) {
                setBlockIndexCandidates.insert(pindex);
            }
            pair<multimap<CBlockIndex*, CBlockIndex*>::iterator, multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(make_pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

bool FindBlockPos(CValidationState& state, CDiskBlockPos& pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
{
    LOCK(cs_LastBlockFile);

    unsigned int nFile = fKnown ? pos.nFile : nLastBlockFile;
    if (vinfoBlockFile.size() <= nFile) {
        vinfoBlockFile.resize(nFile + 1);
    }

    if (!fKnown) {
        while (vinfoBlockFile[nFile].nSize + nAddSize >= MAX_BLOCKFILE_SIZE) {
            LogPrintf("Leaving block file %i: %s\n", nFile, vinfoBlockFile[nFile].ToString());
            FlushBlockFile(true);
            nFile++;
            if (vinfoBlockFile.size() <= nFile) {
                vinfoBlockFile.resize(nFile + 1);
            }
        }
        pos.nFile = nFile;
        pos.nPos = vinfoBlockFile[nFile].nSize;
    }

    nLastBlockFile = nFile;
    vinfoBlockFile[nFile].AddBlock(nHeight, nTime);
    if (fKnown)
        vinfoBlockFile[nFile].nSize = max(pos.nPos + nAddSize, vinfoBlockFile[nFile].nSize);
    else
        vinfoBlockFile[nFile].nSize += nAddSize;

    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = (vinfoBlockFile[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos)) {
                FILE* file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            } else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

bool FindUndoPos(CValidationState& state, int nFile, CDiskBlockPos& pos, unsigned int nAddSize)
{
    pos.nFile = nFile;

    LOCK(cs_LastBlockFile);

    unsigned int nNewSize;
    pos.nPos = vinfoBlockFile[nFile].nUndoSize;
    nNewSize = vinfoBlockFile[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos)) {
            FILE* file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        } else
            return state.Error("out of disk space");
    }

    return true;
}

bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, bool fCheckPOW)
{
    // Check proof of work matches claimed amount
    if (fCheckPOW && !CheckProofOfWork(block.GetHash(), block.nBits))
        return state.DoS(50, error("CheckBlockHeader() : proof of work failed"),
            REJECT_INVALID, "high-hash");

    // Version 4 header must be used after Params().Zerocoin_StartHeight(). And never before.
    if (block.GetBlockTime() > Params().Zerocoin_StartTime()) {
        if(block.nVersion < Params().Zerocoin_HeaderVersion() && Params().NetworkID() != CBaseChainParams::REGTEST)
            return state.DoS(50, error("CheckBlockHeader() : block version must be above 4 after ZerocoinStartHeight"),
            REJECT_INVALID, "block-version");
    } else {
        if (block.nVersion >= Params().Zerocoin_HeaderVersion())
            return state.DoS(50, error("CheckBlockHeader() : block version must be below 4 before ZerocoinStartHeight"),
            REJECT_INVALID, "block-version");
    }

    return true;
}

bool CheckBlock(const CBlock& block, CValidationState& state, bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig)
{
    // These are checks that are independent of context.
    const bool IsPoS = block.IsProofOfStake();
    LogPrint("debug", "%s: block=%s  is proof of stake=%d\n", __func__, block.GetHash().ToString().c_str(), IsPoS);


    // Check that the header is valid (particularly PoW).  This is mostly
    // redundant with the call in AcceptBlockHeader.
    if (!CheckBlockHeader(block, state, !IsPoS))
        return state.DoS(100, error("%s : CheckBlockHeader failed", __func__), REJECT_INVALID, "bad-header", true);

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.

    // Check timestamp
    if (Params().NetworkID() != CBaseChainParams::REGTEST &&
            block.GetBlockTime() > Params().MaxFutureBlockTime(GetAdjustedTime(), IsPoS)) // 3 minute future drift for PoS
        return state.Invalid(error("%s : block timestamp too far in the future", __func__),
            REJECT_INVALID, "time-too-new");

    // Check the merkle root.
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = block.BuildMerkleTree(&mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, error("%s : hashMerkleRoot mismatch", __func__),
                REJECT_INVALID, "bad-txnmrklroot", true);

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.DoS(100, error("%s : duplicate transaction", __func__),
                REJECT_INVALID, "bad-txns-duplicate", true);
    }

    // Size limits
    unsigned int nMaxBlockSize = MAX_BLOCK_SIZE_CURRENT;
    if (block.vtx.empty() || block.vtx.size() > nMaxBlockSize || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION) > nMaxBlockSize)
        return state.DoS(100, error("%s : size limits failed", __func__),
            REJECT_INVALID, "bad-blk-length");

    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0].IsCoinBase())
        return state.DoS(100, error("%s : first tx is not coinbase", __func__),
            REJECT_INVALID, "bad-cb-missing");
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].IsCoinBase())
            return state.DoS(100, error("%s : more than one coinbase", __func__),
                REJECT_INVALID, "bad-cb-multiple");

    if (IsPoS) {
        // Coinbase output should be empty if proof-of-stake block
        if (block.vtx[0].vout.size() != 1 || !block.vtx[0].vout[0].IsEmpty())
            return state.DoS(100, error("%s : coinbase output not empty for proof-of-stake block", __func__));

        // Second transaction must be coinstake, the rest must not be
        if (block.vtx.empty() || !block.vtx[1].IsCoinStake())
            return state.DoS(100, error("%s : second tx is not coinstake", __func__));
        for (unsigned int i = 2; i < block.vtx.size(); i++)
            if (block.vtx[i].IsCoinStake())
                return state.DoS(100, error("%s : more than one coinstake", __func__));
    }

    // ----------- swiftTX transaction scanning -----------
    if (IsSporkActive(SPORK_3_SWIFTTX_BLOCK_FILTERING)) {
        for (const CTransaction& tx : block.vtx) {
            if (!tx.IsCoinBase()) {
                //only reject blocks when it's based on complete consensus
                for (const CTxIn& in : tx.vin) {
                    if (mapLockedInputs.count(in.prevout)) {
                        if (mapLockedInputs[in.prevout] != tx.GetHash()) {
                            mapRejectedBlocks.insert(make_pair(block.GetHash(), GetTime()));
                            LogPrintf("%s : found conflicting transaction with transaction lock %s %s\n", __func__,
                                    mapLockedInputs[in.prevout].ToString(), tx.GetHash().GetHex());
                            return state.DoS(0, error("%s : found conflicting transaction with transaction lock", __func__),
                                REJECT_INVALID, "conflicting-tx-ix");
                        }
                    }
                }
            }
        }
    } else {
        LogPrintf("%s : skipping transaction locking checks\n", __func__);
    }

    // masternode payments / budgets
    CBlockIndex* pindexPrev = chainActive.Tip();
    int nHeight = 0;
    if (pindexPrev != NULL) {
        if (pindexPrev->GetBlockHash() == block.hashPrevBlock) {
            nHeight = pindexPrev->nHeight + 1;
        } else { //out of order
            BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
            if (mi != mapBlockIndex.end() && (*mi).second)
                nHeight = (*mi).second->nHeight + 1;
        }

        // PIVX
        // It is entierly possible that we don't have enough data and this could fail
        // (i.e. the block could indeed be valid). Store the block for later consideration
        // but issue an initial reject message.
        // The case also exists that the sending peer could not have enough data to see
        // that this block is invalid, so don't issue an outright ban.
        if (nHeight != 0 && !IsInitialBlockDownload()) {
            if (!IsBlockPayeeValid(block, nHeight)) {
                mapRejectedBlocks.insert(make_pair(block.GetHash(), GetTime()));
                return state.DoS(0, error("%s : Couldn't find masternode/budget payment", __func__),
                        REJECT_INVALID, "bad-cb-payee");
            }
        } else {
            if (fDebug)
                LogPrintf("%s: Masternode payment check skipped on sync - skipping IsBlockPayeeValid()\n", __func__);
        }
    }

    // Check transactions
    bool fZerocoinActive = block.GetBlockTime() > Params().Zerocoin_StartTime();
    vector<CBigNum> vBlockSerials;
    // TODO: Check if this is ok... blockHeight is always the tip or should we look for the prevHash and get the height?
    int blockHeight = chainActive.Height() + 1;
    for (const CTransaction& tx : block.vtx) {
        if (!CheckTransaction(
                tx,
                fZerocoinActive,
                blockHeight >= Params().Zerocoin_Block_EnforceSerialRange(),
                state,
                isBlockBetweenFakeSerialAttackRange(blockHeight)
        ))
            return error("%s : CheckTransaction failed", __func__);

        // double check that there are no double spent zPIV spends in this block
        if (tx.HasZerocoinSpendInputs()) {
            for (const CTxIn& txIn : tx.vin) {
                bool isPublicSpend = txIn.IsZerocoinPublicSpend();
                if (txIn.IsZerocoinSpend() || isPublicSpend) {
                    libzerocoin::CoinSpend spend;
                    if (isPublicSpend) {
                        libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
                        PublicCoinSpend publicSpend(params);
                        if (!ZPIVModule::ParseZerocoinPublicSpend(txIn, tx, state, publicSpend)){
                            return false;
                        }
                        spend = publicSpend;
                    } else {
                        spend = TxInToZerocoinSpend(txIn);
                    }
                    if (count(vBlockSerials.begin(), vBlockSerials.end(), spend.getCoinSerialNumber()))
                        return state.DoS(100, error("%s : Double spending of zPIV serial %s in block\n Block: %s",
                                                    __func__, spend.getCoinSerialNumber().GetHex(), block.ToString()));
                    vBlockSerials.emplace_back(spend.getCoinSerialNumber());
                }
            }
        }
    }


    unsigned int nSigOps = 0;
    for (const CTransaction& tx : block.vtx) {
        nSigOps += GetLegacySigOpCount(tx);
    }
    unsigned int nMaxBlockSigOps = fZerocoinActive ? MAX_BLOCK_SIGOPS_CURRENT : MAX_BLOCK_SIGOPS_LEGACY;
    if (nSigOps > nMaxBlockSigOps)
        return state.DoS(100, error("%s : out-of-bounds SigOpCount", __func__),
            REJECT_INVALID, "bad-blk-sigops", true);

    return true;
}

bool CheckWork(const CBlock block, CBlockIndex* const pindexPrev)
{
    if (pindexPrev == NULL)
        return error("%s : null pindexPrev for block %s", __func__, block.GetHash().GetHex());

    unsigned int nBitsRequired = GetNextWorkRequired(pindexPrev, &block);

    if ((Params().NetworkID() != CBaseChainParams::REGTEST) && block.IsProofOfWork() && (pindexPrev->nHeight + 1 <= 68589)) {
        double n1 = ConvertBitsToDouble(block.nBits);
        double n2 = ConvertBitsToDouble(nBitsRequired);

        if (abs(n1 - n2) > n1 * 0.5)
            return error("%s : incorrect proof of work (DGW pre-fork) - %f %f %f at %d", __func__, abs(n1 - n2), n1, n2, pindexPrev->nHeight + 1);

        return true;
    }

    if (block.nBits != nBitsRequired) {
        // Pivx Specific reference to the block with the wrong threshold was used.
        if ((block.nTime == (uint32_t) Params().PivxBadBlockTime()) && (block.nBits == (uint32_t) Params().PivxBadBlocknBits())) {
            // accept PIVX block minted with incorrect proof of work threshold
            return true;
        }

        return error("%s : incorrect proof of work at %d", __func__, pindexPrev->nHeight + 1);
    }

    return true;
}

bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, CBlockIndex* const pindexPrev)
{
    uint256 hash = block.GetHash();

    if (hash == Params().HashGenesisBlock())
        return true;

    assert(pindexPrev);

    int nHeight = pindexPrev->nHeight + 1;

    if ((Params().NetworkID() == CBaseChainParams::REGTEST) && block.nBits != GetNextWorkRequired(pindexPrev, &block))
        return state.DoS(100, error("%s : incorrect proof of work", __func__),
                REJECT_INVALID, "bad-diffbits");


    //If this is a reorg, check that it is not too deep
    int nMaxReorgDepth = GetArg("-maxreorg", Params().MaxReorganizationDepth());
    if (chainActive.Height() - nHeight >= nMaxReorgDepth)
        return state.DoS(1, error("%s: forked chain older than max reorganization depth (height %d)", __func__, chainActive.Height() - nHeight));

    // Check blocktime against prev (WANT: blk_time > MedianTimePast)
    if (Params().NetworkID() != CBaseChainParams::REGTEST && block.GetBlockTime() <= pindexPrev->GetMedianTimePast())
        return state.DoS(50, error("%s : block timestamp too old", __func__), REJECT_INVALID, "time-too-old");

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckBlock(nHeight, hash))
        return state.DoS(100, error("%s : rejected by checkpoint lock-in at %d", __func__, nHeight),
            REJECT_CHECKPOINT, "checkpoint mismatch");

    // Don't accept any forks from the main chain prior to last checkpoint
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint();
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(0, error("%s : forked chain older than last checkpoint (height %d)", __func__, nHeight));

    // Reject block.nVersion=1, ..., CURRENT_VERSION-1 blocks when 95% (75% on testnet) of the network has upgraded:
    for (int version = 2; version <= CBlockHeader::CURRENT_VERSION; version++) {
        if (block.nVersion < version && CBlockIndex::IsSuperMajority(version, pindexPrev, Params().RejectBlockOutdatedMajority())) {
            return state.Invalid(error("%s : rejected nVersion=%d block", __func__, block.nVersion), REJECT_OBSOLETE, "bad-version");
        }
    }

    return true;
}

bool IsBlockHashInChain(const uint256& hashBlock)
{
    if (hashBlock == 0 || !mapBlockIndex.count(hashBlock))
        return false;

    return chainActive.Contains(mapBlockIndex[hashBlock]);
}

bool IsTransactionInChain(const uint256& txId, int& nHeightTx, CTransaction& tx)
{
    uint256 hashBlock;
    if (!GetTransaction(txId, tx, hashBlock, true))
        return false;
    if (!IsBlockHashInChain(hashBlock))
        return false;

    nHeightTx = mapBlockIndex.at(hashBlock)->nHeight;
    return true;
}

bool IsTransactionInChain(const uint256& txId, int& nHeightTx)
{
    CTransaction tx;
    return IsTransactionInChain(txId, nHeightTx, tx);
}

bool ContextualCheckBlock(const CBlock& block, CValidationState& state, CBlockIndex* const pindexPrev)
{
    const int nHeight = pindexPrev == NULL ? 0 : pindexPrev->nHeight + 1;

    // Check that all transactions are finalized
    for (const CTransaction& tx : block.vtx)
        if (!IsFinalTx(tx, nHeight, block.GetBlockTime())) {
            return state.DoS(10, error("%s : contains a non-final transaction", __func__), REJECT_INVALID, "bad-txns-nonfinal");
        }

    // Enforce block.nVersion=2 rule that the coinbase starts with serialized block height
    // if 750 of the last 1,000 blocks are version 2 or greater (51/100 if testnet):
    if (block.nVersion >= 2 &&
        CBlockIndex::IsSuperMajority(2, pindexPrev, Params().EnforceBlockUpgradeMajority())) {
        CScript expect = CScript() << nHeight;
        if (block.vtx[0].vin[0].scriptSig.size() < expect.size() ||
            !equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin())) {
            return state.DoS(100, error("%s : block height mismatch in coinbase", __func__), REJECT_INVALID, "bad-cb-height");
        }
    }

    return true;
}

bool AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, CBlockIndex** ppindex)
{
    AssertLockHeld(cs_main);
    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex* pindex = NULL;

    // TODO : ENABLE BLOCK CACHE IN SPECIFIC CASES
    if (miSelf != mapBlockIndex.end()) {
        // Block header is already known.
        pindex = miSelf->second;
        if (ppindex)
            *ppindex = pindex;
        if (pindex->nStatus & BLOCK_FAILED_MASK)
            return state.Invalid(error("%s : block is marked invalid", __func__), 0, "duplicate");
        return true;
    }

    if (!CheckBlockHeader(block, state, false)) {
        LogPrintf("AcceptBlockHeader(): CheckBlockHeader failed \n");
        return false;
    }

    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    if (hash != Params().HashGenesisBlock()) {
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(0, error("%s : prev block %s not found", __func__, block.hashPrevBlock.GetHex()), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
        if (pindexPrev->nStatus & BLOCK_FAILED_MASK) {
            //If this "invalid" block is an exact match from the checkpoints, then reconsider it
            if (pindex && Checkpoints::CheckBlock(pindex->nHeight - 1, block.hashPrevBlock, true)) {
                LogPrintf("%s : Reconsidering block %s height %d\n", __func__, pindexPrev->GetBlockHash().GetHex(), pindexPrev->nHeight);
                CValidationState statePrev;
                ReconsiderBlock(statePrev, pindexPrev);
                if (statePrev.IsValid()) {
                    ActivateBestChain(statePrev);
                    return true;
                }
            }

            return state.DoS(100, error("%s : prev block height=%d hash=%s is invalid, unable to add block %s", __func__, pindexPrev->nHeight, block.hashPrevBlock.GetHex(), block.GetHash().GetHex()),
                             REJECT_INVALID, "bad-prevblk");
        }

    }

    if (!ContextualCheckBlockHeader(block, state, pindexPrev))
        return false;

    if (pindex == NULL)
        pindex = AddToBlockIndex(block);

    if (ppindex)
        *ppindex = pindex;

    return true;
}

bool AcceptBlock(CBlock& block, CValidationState& state, CBlockIndex** ppindex, CDiskBlockPos* dbp, bool fAlreadyCheckedBlock)
{
    AssertLockHeld(cs_main);

    CBlockIndex*& pindex = *ppindex;

    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    if (block.GetHash() != Params().HashGenesisBlock()) {
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(0, error("%s : prev block %s not found", __func__, block.hashPrevBlock.GetHex()), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
        if (pindexPrev->nStatus & BLOCK_FAILED_MASK) {
            //If this "invalid" block is an exact match from the checkpoints, then reconsider it
            if (Checkpoints::CheckBlock(pindexPrev->nHeight, block.hashPrevBlock, true)) {
                LogPrintf("%s : Reconsidering block %s height %d\n", __func__, pindexPrev->GetBlockHash().GetHex(), pindexPrev->nHeight);
                CValidationState statePrev;
                ReconsiderBlock(statePrev, pindexPrev);
                if (statePrev.IsValid()) {
                    ActivateBestChain(statePrev);
                    return true;
                }
            }
            return state.DoS(100, error("%s : prev block %s is invalid, unable to add block %s", __func__, block.hashPrevBlock.GetHex(), block.GetHash().GetHex()),
                             REJECT_INVALID, "bad-prevblk");
        }
    }

    if (block.GetHash() != Params().HashGenesisBlock() && !CheckWork(block, pindexPrev))
        return false;

    bool isPoS = false;
    if (block.IsProofOfStake()) {
        isPoS = true;
        uint256 hashProofOfStake = 0;
        unique_ptr<CStakeInput> stake;

        if (!CheckProofOfStake(block, hashProofOfStake, stake, pindexPrev->nHeight))
            return state.DoS(100, error("%s: proof of stake check failed", __func__));

        if (!stake)
            return error("%s: null stake ptr", __func__);

        uint256 hash = block.GetHash();
        if(!mapProofOfStake.count(hash)) // add to mapProofOfStake
            mapProofOfStake.insert(make_pair(hash, hashProofOfStake));
    }

    if (!AcceptBlockHeader(block, state, &pindex))
        return false;

    if (pindex->nStatus & BLOCK_HAVE_DATA) {
        // TODO: deal better with duplicate blocks.
        // return state.DoS(20, error("AcceptBlock() : already have block %d %s", pindex->nHeight, pindex->GetBlockHash().ToString()), REJECT_DUPLICATE, "duplicate");
        LogPrintf("AcceptBlock() : already have block %d %s", pindex->nHeight, pindex->GetBlockHash().ToString());
        return true;
    }

    if ((!fAlreadyCheckedBlock && !CheckBlock(block, state)) || !ContextualCheckBlock(block, state, pindex->pprev)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return false;
    }

    int nHeight = pindex->nHeight;
    int splitHeight = -1;

    if (isPoS) {
        LOCK(cs_main);

        // Blocks arrives in order, so if prev block is not the tip then we are on a fork.
        // Extra info: duplicated blocks are skipping this checks, so we don't have to worry about those here.
        bool isBlockFromFork = pindexPrev != nullptr && chainActive.Tip() != pindexPrev;

        // Coin stake
        CTransaction &stakeTxIn = block.vtx[1];

        // Inputs
        vector<CTxIn> pivInputs;
        vector<CTxIn> zPIVInputs;

        for (const CTxIn& stakeIn : stakeTxIn.vin) {
            if(stakeIn.IsZerocoinSpend()){
                zPIVInputs.push_back(stakeIn);
            }else{
                pivInputs.push_back(stakeIn);
            }
        }
        const bool hasPIVInputs = !pivInputs.empty();
        const bool hasZPIVInputs = !zPIVInputs.empty();

        // ZC started after PoS.
        // Check for serial double spent on the same block, TODO: Move this to the proper method..

        vector<CBigNum> inBlockSerials;
        for (const CTransaction& tx : block.vtx) {
            for (const CTxIn& in: tx.vin) {
                if(nHeight >= Params().Zerocoin_StartHeight()) {
                    bool isPublicSpend = in.IsZerocoinPublicSpend();
                    bool isPrivZerocoinSpend = in.IsZerocoinSpend();
                    if (isPrivZerocoinSpend || isPublicSpend) {

                        // Check enforcement
                        if (!CheckPublicCoinSpendEnforced(pindex->nHeight, isPublicSpend)){
                            return false;
                        }

                        libzerocoin::CoinSpend spend;
                        if (isPublicSpend) {
                            libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
                            PublicCoinSpend publicSpend(params);
                            if (!ZPIVModule::ParseZerocoinPublicSpend(in, tx, state, publicSpend)){
                                return false;
                            }
                            spend = publicSpend;
                        } else {
                            spend = TxInToZerocoinSpend(in);
                        }
                        // Check for serials double spending in the same block
                        if (find(inBlockSerials.begin(), inBlockSerials.end(), spend.getCoinSerialNumber()) !=
                            inBlockSerials.end()) {
                            return state.DoS(100, error("%s: serial double spent on the same block", __func__));
                        }
                        inBlockSerials.push_back(spend.getCoinSerialNumber());
                    }
                }
                if(tx.IsCoinStake()) continue;
                if(hasPIVInputs)
                    // Check if coinstake input is double spent inside the same block
                    for (const CTxIn& pivIn : pivInputs){
                        if(pivIn.prevout == in.prevout){
                            // double spent coinstake input inside block
                            return error("%s: double spent coinstake input inside block", __func__);
                        }
                    }
            }
        }
        inBlockSerials.clear();


        // Check whether is a fork or not
        if (isBlockFromFork) {

            // Start at the block we're adding on to
            CBlockIndex *prev = pindexPrev;

            CBlock bl;
            if (!ReadBlockFromDisk(bl, prev))
                return error("%s: previous block %s not on disk", __func__, prev->GetBlockHash().GetHex());

            vector<CBigNum> vBlockSerials;
            int readBlock = 0;
            // Go backwards on the forked chain up to the split
            while (!chainActive.Contains(prev)) {

                // Increase amount of read blocks
                readBlock++;
                // Check if the forked chain is longer than the max reorg limit
                if (readBlock == Params().MaxReorganizationDepth()) {
                    // TODO: Remove this chain from disk.
                    return error("%s: forked chain longer than maximum reorg limit", __func__);
                }

                // Loop through every input from said block
                for (const CTransaction &t : bl.vtx) {
                    for (const CTxIn &in: t.vin) {
                        // Loop through every input of the staking tx
                        for (const CTxIn &stakeIn : pivInputs) {
                            // if it's already spent

                            // First regular staking check
                            if (hasPIVInputs) {
                                if (stakeIn.prevout == in.prevout) {
                                    return state.DoS(100, error("%s: input already spent on a previous block",
                                                                __func__));
                                }

                                // Second, if there is zPoS staking then store the serials for later check
                                if(in.IsZerocoinSpend()){
                                    vBlockSerials.push_back(TxInToZerocoinSpend(in).getCoinSerialNumber());
                                }
                            }
                        }
                    }
                }

                // Prev block
                prev = prev->pprev;
                if (!ReadBlockFromDisk(bl, prev))
                    // Previous block not on disk
                    return error("%s: previous block %s not on disk", __func__, prev->GetBlockHash().GetHex());

            }

            // Split height
            splitHeight = prev->nHeight;

            // Now that this loop if completed. Check if we have zPIV inputs.
            if(hasZPIVInputs){
                for (const CTxIn& zPivInput : zPIVInputs) {
                    libzerocoin::CoinSpend spend = TxInToZerocoinSpend(zPivInput);

                    // First check if the serials were not already spent on the forked blocks.
                    CBigNum coinSerial = spend.getCoinSerialNumber();
                    for(const CBigNum& serial : vBlockSerials){
                        if(serial == coinSerial){
                            return state.DoS(100, error("%s: serial double spent on fork", __func__));
                        }
                    }

                    // Now check if the serial exists before the chain split.
                    int nHeightTx = 0;
                    if (IsSerialInBlockchain(spend.getCoinSerialNumber(), nHeightTx)){
                        // if the height is nHeightTx > chainSplit means that the spent occurred after the chain split
                        if(nHeightTx <= splitHeight)
                            return state.DoS(100, error("%s: serial double spent on main chain", __func__));
                    }

                    if (!ContextualCheckZerocoinSpendNoSerialCheck(stakeTxIn, &spend, pindex, 0))
                        return state.DoS(100,error("%s: forked chain ContextualCheckZerocoinSpend failed for tx %s", __func__,
                                                   stakeTxIn.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zpiv");

                    // Now only the ZKP left..
                    // As the spend maturity is 200, the acc value must be accumulated, otherwise it's not ready to be spent
                    CBigNum bnAccumulatorValue = 0;
                    if (!zerocoinDB->ReadAccumulatorValue(spend.getAccumulatorChecksum(), bnAccumulatorValue)) {
                        return state.DoS(100, error("%s: stake zerocoinspend not ready to be spent", __func__));
                    }

                    libzerocoin::Accumulator accumulator(Params().Zerocoin_Params(chainActive.Height() < Params().Zerocoin_Block_V2_Start()),
                                            spend.getDenomination(), bnAccumulatorValue);

                    //Check that the coinspend is valid
                    bool isInInvalidRange = isBlockBetweenFakeSerialAttackRange(pindex->nHeight);
                    if(!spend.Verify(accumulator, !isInInvalidRange))
                        return state.DoS(100, error("%s: zerocoin spend did not verify", __func__));

                }
            }

        }


        // If the stake is not a zPoS then let's check if the inputs were spent on the main chain
        const CCoinsViewCache coins(pcoinsTip);
        if(!stakeTxIn.HasZerocoinSpendInputs()) {
            for (const CTxIn& in: stakeTxIn.vin) {
                const CCoins* coin = coins.AccessCoins(in.prevout.hash);

                if(!coin && !isBlockFromFork){
                    // No coins on the main chain
                    return error("%s: coin stake inputs not available on main chain, received height %d vs current %d", __func__, nHeight, chainActive.Height());
                }
                if(coin && !coin->IsAvailable(in.prevout.n)){
                    if(!isBlockFromFork){
                        // Coins not available
                        return error("%s: coin stake inputs already spent in main chain", __func__);
                    }
                }
            }
        } else {
            if(!isBlockFromFork)
                for (const CTxIn& zPivInput : zPIVInputs) {
                        libzerocoin::CoinSpend spend = TxInToZerocoinSpend(zPivInput);
                        if (!ContextualCheckZerocoinSpend(stakeTxIn, &spend, pindex, 0))
                            return state.DoS(100,error("%s: main chain ContextualCheckZerocoinSpend failed for tx %s", __func__,
                                    stakeTxIn.GetHash().GetHex()), REJECT_INVALID, "bad-txns-invalid-zpiv");
                }

        }

    }

    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(state, blockPos, nBlockSize + 8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("AcceptBlock() : FindBlockPos failed");
        if (dbp == NULL)
            if (!WriteBlockToDisk(block, blockPos))
                return AbortNode("Failed to write block");
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("AcceptBlock() : ReceivedBlockTransactions failed");
    } catch (runtime_error& e) {
        return AbortNode(string("System error: ") + e.what());
    }

    return true;
}

int GetHeight()
{
    while (true) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            MilliSleep(50);
            continue;
        }
        return chainActive.Height();
    }
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired)
{
    unsigned int nToCheck = Params().ToCheckBlockUpgradeMajority();
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++) {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    if (height > nHeight || height < 0)
        return NULL;

    CBlockIndex* pindexWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (heightSkip == height ||
            (heightSkip > height && !(heightSkipPrev < heightSkip - 2 && heightSkipPrev >= height))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    return const_cast<CBlockIndex*>(this)->GetAncestor(height);
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

bool ProcessNewBlock(CValidationState& state, CNode* pfrom, CBlock* pblock, CDiskBlockPos* dbp)
{
    if (pblock->GetHash() != Params().HashGenesisBlock() && pfrom != NULL) {
        //if we get this far, check if the prev block is our prev block, if not then request sync and return false
        BlockMap::iterator mi = mapBlockIndex.find(pblock->hashPrevBlock);
        if (mi == mapBlockIndex.end()) {
            pfrom->PushMessage("getblocks", chainActive.GetLocator(), uint256(0));
            return false;
        }
    }

    // Preliminary checks
    int64_t nStartTime = GetTimeMillis();

    // check block
    bool checked = CheckBlock(*pblock, state);

    if (!CheckBlockSignature(*pblock))
        return error("ProcessNewBlock() : bad proof-of-stake block signature");

    {
        LOCK(cs_main);

        MarkBlockAsReceived(pblock->GetHash());
        if (!checked) {
            return error ("%s : CheckBlock FAILED for block %s", __func__, pblock->GetHash().GetHex());
        }

        // Store to disk
        CBlockIndex* pindex = nullptr;
        bool ret = AcceptBlock(*pblock, state, &pindex, dbp, checked);
        if (pindex && pfrom) {
            mapBlockSource[pindex->GetBlockHash ()] = pfrom->GetId ();
        }
        CheckBlockIndex ();
        if (!ret) {
            // Check spamming
            if(pindex && pfrom && GetBoolArg("-blockspamfilter", DEFAULT_BLOCK_SPAM_FILTER)) {
                CNodeState *nodestate = State(pfrom->GetId());
                if(nodestate != nullptr) {
                    nodestate->nodeBlocks.onBlockReceived(pindex->nHeight);
                    bool nodeStatus = true;
                    // UpdateState will return false if the node is attacking us or update the score and return true.
                    nodeStatus = nodestate->nodeBlocks.updateState(state, nodeStatus);
                    int nDoS = 0;
                    if (state.IsInvalid(nDoS)) {
                        if (nDoS > 0)
                            Misbehaving(pfrom->GetId(), nDoS);
                        nodeStatus = false;
                    }
                    if (!nodeStatus)
                        return error("%s : AcceptBlock FAILED - block spam protection", __func__);
                }
            }
            return error("%s : AcceptBlock FAILED", __func__);
        }
    }

    if (!ActivateBestChain(state, pblock, checked))
        return error("%s : ActivateBestChain failed", __func__);

    if (!fLiteMode) {
        if (masternodeSync.RequestedMasternodeAssets > MASTERNODE_SYNC_LIST) {
            obfuScationPool.NewBlock();
            masternodePayments.ProcessBlock(GetHeight() + 10);
            budget.NewBlock();
        }
    }

    if (pwalletMain) {
        // If turned on MultiSend will send a transaction (or more) on the after maturity of a stake
        if (pwalletMain->isMultiSendEnabled())
            pwalletMain->MultiSend();

        // If turned on Auto Combine will scan wallet for dust to combine
        if (pwalletMain->fCombineDust)
            pwalletMain->AutoCombineDust();
    }

    LogPrintf("%s : ACCEPTED Block %ld in %ld milliseconds with size=%d\n", __func__, GetHeight(), GetTimeMillis() - nStartTime,
              pblock->GetSerializeSize(SER_DISK, CLIENT_VERSION));

    return true;
}

bool TestBlockValidity(CValidationState& state, const CBlock& block, CBlockIndex* const pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev);
    if (pindexPrev != chainActive.Tip()) {
        LogPrintf("%s : No longer working on chain tip\n", __func__);
        return false;
    }

    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex indexDummy(block);
    indexDummy.pprev = pindexPrev;
    indexDummy.nHeight = pindexPrev->nHeight + 1;

    // NOTE: CheckBlockHeader is called by CheckBlock
    if (!ContextualCheckBlockHeader(block, state, pindexPrev))
        return false;
    if (!CheckBlock(block, state, fCheckPOW, fCheckMerkleRoot))
        return false;
    if (!ContextualCheckBlock(block, state, pindexPrev))
        return false;
    if (!ConnectBlock(block, state, &indexDummy, viewNew, true))
        return false;
    assert(state.IsValid());

    return true;
}


bool AbortNode(const string& strMessage, const string& userMessage)
{
    strMiscWarning = strMessage;
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        userMessage.empty() ? _("Error: A fatal internal error occured, see debug.log for details") : userMessage,
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

FILE* OpenDiskFile(const CDiskBlockPos& pos, const char* prefix, bool fReadOnly)
{
    if (pos.IsNull())
        return NULL;
    boost::filesystem::path path = GetBlockPosFilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    FILE* file = fopen(path.string().c_str(), "rb+");
    if (!file && !fReadOnly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return NULL;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "blk", fReadOnly);
}

FILE* OpenUndoFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "rev", fReadOnly);
}

boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos& pos, const char* prefix)
{
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}

CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool static LoadBlockIndexDB(string& strError)
{
    if (!pblocktree->LoadBlockIndexGuts())
        return false;

    boost::this_thread::interruption_point();

    // Calculate nChainWork
    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    for (const pair<const uint256, CBlockIndex*>& item : mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    for (const PAIRTYPE(int, CBlockIndex*) & item : vSortedByHeight) {
        CBlockIndex* pindex = item.second;
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }

    // Load block file info
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

    // Check presence of blk files
    LogPrintf("Checking all blk files are present...\n");
    set<int> setBlkDataFiles;
    for (const pair<const uint256, CBlockIndex*>& item : mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++) {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    //Check if the shutdown procedure was followed on last client exit
    bool fLastShutdownWasPrepared = true;
    pblocktree->ReadFlag("shutdown", fLastShutdownWasPrepared);
    LogPrintf("%s: Last shutdown was prepared: %s\n", __func__, fLastShutdownWasPrepared);

    // Check whether we need to continue reindexing
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    fReindex |= fReindexing;

    // Check whether we have a transaction index
    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("LoadBlockIndexDB(): transaction index %s\n", fTxIndex ? "enabled" : "disabled");

    // If this is written true before the next client init, then we know the shutdown process failed
    pblocktree->WriteFlag("shutdown", false);

    // Load pointer to end of best chain
    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    if (it == mapBlockIndex.end())
        return true;
    chainActive.SetTip(it->second);

    PruneBlockIndexCandidates();

    LogPrintf("LoadBlockIndexDB(): hashBestChain=%s height=%d date=%s progress=%f\n",
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        Checkpoints::GuessVerificationProgress(chainActive.Tip()));

    return true;
}

CVerifyDB::CVerifyDB()
{
    uiInterface.ShowProgress(_("Verifying blocks..."), 0);
}

CVerifyDB::~CVerifyDB()
{
    uiInterface.ShowProgress("", 100);
}

bool CVerifyDB::VerifyDB(CCoinsView* coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

    // Verify blocks in the best chain
    if (nCheckDepth <= 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = max(0, min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(coinsview);
    CBlockIndex* pindexState = chainActive.Tip();
    CBlockIndex* pindexFailure = NULL;
    int nGoodTransactions = 0;
    CValidationState state;
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev) {
        boost::this_thread::interruption_point();
        uiInterface.ShowProgress(_("Verifying blocks..."), max(1, min(99, (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * (nCheckLevel >= 4 ? 50 : 100)))));
        if (pindex->nHeight < chainActive.Height() - nCheckDepth)
            break;
        CBlock block;
        // check level 0: read from disk
        if (!ReadBlockFromDisk(block, pindex))
            return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        // check level 1: verify block validity
        if (nCheckLevel >= 1 && !CheckBlock(block, state))
            return error("VerifyDB() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
        // check level 2: verify undo validity
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!undo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
                    return error("VerifyDB() : *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nCheckLevel >= 3 && pindex == pindexState && (coins.GetCacheSize() + pcoinsTip->GetCacheSize()) <= nCoinCacheSize) {
            bool fClean = true;
            if (!DisconnectBlock(block, state, pindex, coins, &fClean))
                return error("VerifyDB() : *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            pindexState = pindex->pprev;
            if (!fClean) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else
                nGoodTransactions += block.vtx.size();
        }
        if (ShutdownRequested())
            return true;
    }
    if (pindexFailure)
        return error("VerifyDB() : *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

    // check level 4: try reconnecting blocks
    if (nCheckLevel >= 4) {
        CBlockIndex* pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), max(1, min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex))
                return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            if (!ConnectBlock(block, state, pindex, coins, false))
                return error("VerifyDB() : *** found unconnectable block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        }
    }

    LogPrintf("No coin database inconsistencies in last %i blocks (%i transactions)\n", chainActive.Height() - pindexState->nHeight, nGoodTransactions);

    return true;
}

void UnloadBlockIndex()
{
    LOCK(cs_main);
    setBlockIndexCandidates.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
    pindexBestHeader = NULL;
    mempool.clear();
    mapBlocksUnlinked.clear();
    vinfoBlockFile.clear();
    nLastBlockFile = 0;
    nBlockSequenceId = 1;
    setDirtyBlockIndex.clear();
    setDirtyFileInfo.clear();
    for (BlockMap::value_type& entry : mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
}

bool LoadBlockIndex(string& strError)
{
    // Load block index from databases
    if (!fReindex && !LoadBlockIndexDB(strError))
        return false;
    return true;
}


bool InitBlockIndex()
{
    LOCK(cs_main);
    // Check whether we're already initialized
    if (chainActive.Genesis() != NULL)
        return true;

    // Use the provided setting for -txindex in the new database
    fTxIndex = GetBoolArg("-txindex", true);
    pblocktree->WriteFlag("txindex", fTxIndex);
    LogPrintf("Initializing databases...\n");

    // Only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!fReindex) {
        try {
            CBlock& block = const_cast<CBlock&>(Params().GenesisBlock());
            // Start new block file
            unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
            CDiskBlockPos blockPos;
            CValidationState state;
            if (!FindBlockPos(state, blockPos, nBlockSize + 8, 0, block.GetBlockTime()))
                return error("LoadBlockIndex() : FindBlockPos failed");
            if (!WriteBlockToDisk(block, blockPos))
                return error("LoadBlockIndex() : writing genesis block to disk failed");
            CBlockIndex* pindex = AddToBlockIndex(block);
            if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
                return error("LoadBlockIndex() : genesis block not accepted");
            if (!ActivateBestChain(state, &block))
                return error("LoadBlockIndex() : genesis block cannot be activated");
            // Force a chainstate write so that when we VerifyDB in a moment, it doesnt check stale data
            return FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
        } catch (runtime_error& e) {
            return error("LoadBlockIndex() : failed to initialize block database: %s", e.what());
        }
    }

    return true;
}


bool LoadExternalBlockFile(FILE* fileIn, CDiskBlockPos* dbp)
{
    // Map of disk positions for blocks with unknown parent (only used for reindex)
    static multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {
        // This takes over fileIn and calls fclose() on it in the CBufferedFile destructor
        CBufferedFile blkdat(fileIn, 2 * MAX_BLOCK_SIZE_CURRENT, MAX_BLOCK_SIZE_CURRENT + 8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++;         // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {
                // locate a header
                unsigned char buf[MESSAGE_START_SIZE];
                blkdat.FindByte(Params().MessageStart()[0]);
                nRewind = blkdat.GetPos() + 1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, Params().MessageStart(), MESSAGE_START_SIZE))
                    continue;
                // read size
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SIZE_CURRENT)
                    continue;
            } catch (const std::exception&) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                CBlock block;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                // detect out of order blocks, and store them for later
                uint256 hash = block.GetHash();
                if (hash != Params().HashGenesisBlock() && mapBlockIndex.find(block.hashPrevBlock) == mapBlockIndex.end()) {
                    LogPrint("reindex", "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                        block.hashPrevBlock.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(make_pair(block.hashPrevBlock, *dbp));
                    continue;
                }

                // process in case the block isn't known yet
                if (mapBlockIndex.count(hash) == 0 || (mapBlockIndex[hash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    CValidationState state;
                    if (ProcessNewBlock(state, NULL, &block, dbp))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (hash != Params().HashGenesisBlock() && mapBlockIndex[hash]->nHeight % 1000 == 0) {
                    LogPrintf("Block Import: already had block %s at height %d\n", hash.ToString(), mapBlockIndex[hash]->nHeight);
                }

                // Recursively process earlier encountered successors of this block
                deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    pair<multimap<uint256, CDiskBlockPos>::iterator, multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        if (ReadBlockFromDisk(block, it->second)) {
                            LogPrintf("%s: Processing out of order child %s of %s\n", __func__, block.GetHash().ToString(),
                                head.ToString());
                            CValidationState dummy;
                            if (ProcessNewBlock(dummy, NULL, &block, &it->second)) {
                                nLoaded++;
                                queue.push_back(block.GetHash());
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                    }
                }
            } catch (std::exception& e) {
                LogPrintf("%s : Deserialize or I/O error - %s", __func__, e.what());
            }
        }
    } catch (runtime_error& e) {
        AbortNode(string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

void CheckBlockIndex()
{
    if (!fCheckBlockIndex) {
        return;
    }

    LOCK(cs_main);

    // During a reindex, we read the genesis block and call CheckBlockIndex before ActivateBestChain,
    // so we have the genesis block in mapBlockIndex but no active chain.  (A few of the tests when
    // iterating the block tree require that chainActive has been initialized.)
    if (chainActive.Height() < 0) {
        assert(mapBlockIndex.size() <= 1);
        return;
    }

    // Build forward-pointing map of the entire block tree.
    multimap<CBlockIndex*, CBlockIndex*> forward;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        forward.insert(make_pair(it->second->pprev, it->second));
    }

    assert(forward.size() == mapBlockIndex.size());

    pair<multimap<CBlockIndex*, CBlockIndex*>::iterator, multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(NULL);
    CBlockIndex* pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
    assert(rangeGenesis.first == rangeGenesis.second); // There is only one index entry with parent NULL.

    // Iterate over the entire block tree, using depth-first search.
    // Along the way, remember whether there are blocks on the path from genesis
    // block being explored which are the first to have certain properties.
    size_t nNodes = 0;
    int nHeight = 0;
    CBlockIndex* pindexFirstInvalid = NULL;         // Oldest ancestor of pindex which is invalid.
    CBlockIndex* pindexFirstMissing = NULL;         // Oldest ancestor of pindex which does not have BLOCK_HAVE_DATA.
    CBlockIndex* pindexFirstNotTreeValid = NULL;    // Oldest ancestor of pindex which does not have BLOCK_VALID_TREE (regardless of being valid or not).
    CBlockIndex* pindexFirstNotChainValid = NULL;   // Oldest ancestor of pindex which does not have BLOCK_VALID_CHAIN (regardless of being valid or not).
    CBlockIndex* pindexFirstNotScriptsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_SCRIPTS (regardless of being valid or not).
    while (pindex != NULL) {
        nNodes++;
        if (pindexFirstInvalid == NULL && pindex->nStatus & BLOCK_FAILED_VALID) pindexFirstInvalid = pindex;
        if (pindexFirstMissing == NULL && !(pindex->nStatus & BLOCK_HAVE_DATA)) pindexFirstMissing = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTreeValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE) pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotChainValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN) pindexFirstNotChainValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotScriptsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS) pindexFirstNotScriptsValid = pindex;

        // Begin: actual consistency checks.
        if (pindex->pprev == NULL) {
            // Genesis block checks.
            assert(pindex->GetBlockHash() == Params().HashGenesisBlock()); // Genesis block's hash must match.
            assert(pindex == chainActive.Genesis());                       // The current active chain's genesis block must be this block.
        }
        // HAVE_DATA is equivalent to VALID_TRANSACTIONS and equivalent to nTx > 0 (we stored the number of transactions in the block)
        assert(!(pindex->nStatus & BLOCK_HAVE_DATA) == (pindex->nTx == 0));
        assert(((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TRANSACTIONS) == (pindex->nTx > 0));
        if (pindex->nChainTx == 0) assert(pindex->nSequenceId == 0); // nSequenceId can't be set for blocks that aren't linked
        // All parents having data is equivalent to all parents being VALID_TRANSACTIONS, which is equivalent to nChainTx being set.
        assert((pindexFirstMissing != NULL) == (pindex->nChainTx == 0));                                             // nChainTx == 0 is used to signal that all parent block's transaction data is available.
        assert(pindex->nHeight == nHeight);                                                                          // nHeight must be consistent.
        assert(pindex->pprev == NULL || pindex->nChainWork >= pindex->pprev->nChainWork);                            // For every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->nHeight < nHeight)));                                // The pskip pointer must point back for all but the first 2 blocks.
        assert(pindexFirstNotTreeValid == NULL);                                                                     // All mapBlockIndex entries must at least be TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE) assert(pindexFirstNotTreeValid == NULL);       // TREE valid implies all parents are TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN) assert(pindexFirstNotChainValid == NULL);     // CHAIN valid implies all parents are CHAIN valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS) assert(pindexFirstNotScriptsValid == NULL); // SCRIPTS valid implies all parents are SCRIPTS valid
        if (pindexFirstInvalid == NULL) {
            // Checks for not-invalid blocks.
            assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); // The failed mask cannot be set for blocks without invalid parents.
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstMissing == NULL) {
            if (pindexFirstInvalid == NULL) { // If this block sorts at least as good as the current tip and is valid, it must be in setBlockIndexCandidates.
                assert(setBlockIndexCandidates.count(pindex));
            }
        } else { // If this block sorts worse than the current tip, it cannot be in setBlockIndexCandidates.
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }
        // Check whether this block is in mapBlocksUnlinked.
        pair<multimap<CBlockIndex*, CBlockIndex*>::iterator, multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
        bool foundInUnlinked = false;
        while (rangeUnlinked.first != rangeUnlinked.second) {
            assert(rangeUnlinked.first->first == pindex->pprev);
            if (rangeUnlinked.first->second == pindex) {
                foundInUnlinked = true;
                break;
            }
            rangeUnlinked.first++;
        }
        if (pindex->pprev && pindex->nStatus & BLOCK_HAVE_DATA && pindexFirstMissing != NULL) {
            if (pindexFirstInvalid == NULL) { // If this block has block data available, some parent doesn't, and has no invalid parents, it must be in mapBlocksUnlinked.
                assert(foundInUnlinked);
            }
        } else { // If this block does not have block data available, or all parents do, it cannot be in mapBlocksUnlinked.
            assert(!foundInUnlinked);
        }
        // assert(pindex->GetBlockHash() == pindex->GetBlockHeader().GetHash()); // Perhaps too slow
        // End: actual consistency checks.

        // Try descending into the first subnode.
        pair<multimap<CBlockIndex*, CBlockIndex*>::iterator, multimap<CBlockIndex*, CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
            // A subnode was found.
            pindex = range.first->second;
            nHeight++;
            continue;
        }
        // This is a leaf node.
        // Move upwards until we reach a node of which we have not yet visited the last child.
        while (pindex) {
            // We are going to either move to a parent or a sibling of pindex.
            // If pindex was the first with a certain property, unset the corresponding variable.
            if (pindex == pindexFirstInvalid) pindexFirstInvalid = NULL;
            if (pindex == pindexFirstMissing) pindexFirstMissing = NULL;
            if (pindex == pindexFirstNotTreeValid) pindexFirstNotTreeValid = NULL;
            if (pindex == pindexFirstNotChainValid) pindexFirstNotChainValid = NULL;
            if (pindex == pindexFirstNotScriptsValid) pindexFirstNotScriptsValid = NULL;
            // Find our parent.
            CBlockIndex* pindexPar = pindex->pprev;
            // Find which child we just visited.
            pair<multimap<CBlockIndex*, CBlockIndex*>::iterator, multimap<CBlockIndex*, CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
                assert(rangePar.first != rangePar.second); // Our parent must have at least the node we're coming from as child.
                rangePar.first++;
            }
            // Proceed to the next one.
            rangePar.first++;
            if (rangePar.first != rangePar.second) {
                // Move to the sibling.
                pindex = rangePar.first->second;
                break;
            } else {
                // Move up further.
                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

    // Check that we actually traversed the entire map.
    assert(nNodes == forward.size());
}

bool CBlockUndo::WriteToDisk(CDiskBlockPos& pos, const uint256& hashBlock)
{
    // Open history file to append
    CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("CBlockUndo::WriteToDisk : OpenUndoFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(*this);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write undo data
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("CBlockUndo::WriteToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << *this;

    // calculate & write checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    fileout << hasher.GetHash();

    return true;
}

bool CBlockUndo::ReadFromDisk(const CDiskBlockPos& pos, const uint256& hashBlock)
{
    // Open history file to read
    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("CBlockUndo::ReadFromDisk : OpenBlockFile failed");

    // Read block
    uint256 hashChecksum;
    try {
        filein >> *this;
        filein >> hashChecksum;
    } catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Verify checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    if (hashChecksum != hasher.GetHash())
        return error("CBlockUndo::ReadFromDisk : Checksum mismatch");

    return true;
}

string CBlockFileInfo::ToString() const
{
    return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
}

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup()
    {
        // block headers
        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();
    }
} instance_of_cmaincleanup;
