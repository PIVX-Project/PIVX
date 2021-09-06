// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include "policy/policy.h"

#include "consensus/tx_verify.h" // for IsFinal()
#include "tinyformat.h"
#include "util/system.h"
#include "utilstrencodings.h"
#include "validation.h"


bool fIsBareMultisigStd = DEFAULT_PERMIT_BAREMULTISIG;

CFeeRate dustRelayFee = CFeeRate(DUST_RELAY_TX_FEE);

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    // "Dust" is defined in terms of dustRelayFee,
    // which has units satoshis-per-kilobyte.
    // If you'd pay more in fees than the value of the output
    // to spend something, then we consider it dust.
    // A typical spendable txout is 34 bytes big, and will
    // need a CTxIn of at least 148 bytes to spend:
    // so dust is a spendable txout less than
    // 546*dustRelayFee/1000 (in satoshis).
    if (txout.scriptPubKey.IsUnspendable())
        return 0;

    size_t nSize = GetSerializeSize(txout, 0);
    nSize += (32 + 4 + 1 + 107 + 4); // the 148 mentioned above
    return dustRelayFeeIn.GetFee(nSize);
}

CAmount GetDustThreshold(const CFeeRate& dustRelayFeeIn)
{
    // return the dust threshold for a typical 34 bytes output
    return dustRelayFeeIn.GetFee(182);
}

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    return (txout.nValue < GetDustThreshold(txout, dustRelayFeeIn));
}

CAmount GetShieldedDustThreshold(const CFeeRate& dustRelayFeeIn)
{
    unsigned int K = DEFAULT_SHIELDEDTXFEE_K;   // Fixed (100) for now
    return K * dustRelayFeeIn.GetFee(SPENDDESCRIPTION_SIZE +
                                     CTXOUT_REGULAR_SIZE +
                                     BINDINGSIG_SIZE);
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 *
 * Check transaction inputs, and make sure any
 * pay-to-script-hash transactions are evaluating IsStandard scripts
 *
 * Why bother? To avoid denial-of-service attacks; an attacker
 * can submit a standard HASH... OP_EQUAL transaction,
 * which will get accepted into blocks. The redemption
 * script can be anything; an attacker could use a very
 * expensive-to-check-upon-redemption script like:
 *   DUP CHECKSIG DROP ... repeated 100 times... OP_1
 */

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType)
{
    std::vector<valtype> vSolutions;
    whichType = Solver(scriptPubKey, vSolutions);
    if (whichType == TX_NONSTANDARD) {
        return false;
    } else if (whichType == TX_MULTISIG) {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        // Support up to x-of-3 multisig txns as standard
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    } else if (whichType == TX_NULL_DATA &&
              (!gArgs.GetBoolArg("-datacarrier", DEFAULT_ACCEPT_DATACARRIER) || scriptPubKey.size() > nMaxDatacarrierBytes)) {
        return false;
    }

    return true;
}

bool IsStandardTx(const CTransactionRef& tx, int nBlockHeight, std::string& reason)
{
    AssertLockHeld(cs_main);
    if (!Params().GetConsensus().NetworkUpgradeActive(nBlockHeight, Consensus::UPGRADE_V5_0)) {
        // Before v5, all txes with version other than STANDARD_VERSION (1) are considered non-standard
        if (tx->nVersion != CTransaction::TxVersion::LEGACY) {
            reason = "version";
            return false;
        }
    }
    // After v5, all txes with a version number accepted by consensus are considered standard.

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
    if (!IsFinalTx(tx, nBlockHeight)) {
        reason = "non-final";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx->GetTotalSize();
    unsigned int nMaxSize = tx->IsShieldedTx() ? MAX_TX_SIZE_AFTER_SAPLING : MAX_STANDARD_TX_SIZE;
    if (sz >= nMaxSize) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx->vin) {
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
    for (const CTxOut& txout : tx->vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (IsDust(txout, dustRelayFee)) {
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

bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase()) {
        return true; // coinbases don't use vin normally
    }

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prev = mapInputs.AccessCoin(tx.vin[i].prevout).out;

        std::vector<std::vector<unsigned char> > vSolutions;
        txnouttype whichType = Solver(prev.scriptPubKey, vSolutions);
        if (whichType == TX_NONSTANDARD) {
            return false;
        } else if (whichType == TX_SCRIPTHASH) {
            std::vector<std::vector<unsigned char> > stack;
            // convert the scriptSig into a stack, so we can inspect the redeemScript
            if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker(), tx.GetRequiredSigVersion()))
                return false;
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            if (subscript.GetSigOpCount(true) > MAX_P2SH_SIGOPS) {
                return false;
            }
        }
    }

    return true;
}
