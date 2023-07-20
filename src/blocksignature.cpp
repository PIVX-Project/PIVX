// Copyright (c) 2017-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocksignature.h"

#include "script/standard.h"
#include "zpiv/zpivmodule.h"

static bool GetKeyIDFromUTXO(const CTxOut& utxo, CKeyID& keyIDRet)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (utxo.scriptPubKey.empty() || !Solver(utxo.scriptPubKey, whichType, vSolutions))
        return false;
    if (whichType == TX_PUBKEY) {
        keyIDRet = CPubKey(vSolutions[0]).GetID();
        return true;
    }
    if (whichType == TX_PUBKEYHASH || whichType == TX_COLDSTAKE) {
        keyIDRet = CKeyID(uint160(vSolutions[0]));
        return true;
    }
    return false;
}

bool SignBlockWithKey(CBlock& block, const CKey& key)
{
    if (!key.Sign(block.GetHash(), block.vchBlockSig))
        return error("%s: failed to sign block hash with key", __func__);

    return true;
}

bool SignBlock(CBlock& block, const CKeyStore& keystore)
{
    CKeyID keyID;
    if (!GetKeyIDFromUTXO(block.vtx[1]->vout[1], keyID)) {
        return error("%s: failed to find key for PoS", __func__);
    }

    CKey key;
    if (!keystore.GetKey(keyID, key))
        return error("%s: failed to get key from keystore", __func__);

    return SignBlockWithKey(block, key);
}

bool CheckBlockSignature(const CBlock& block)
{
    if (block.IsProofOfWork())
        return block.vchBlockSig.empty();

    if (block.vchBlockSig.empty())
        return error("%s: vchBlockSig is empty!", __func__);

    /** Each block is signed by the private key of the input that is staked. This can be either zPIV or normal UTXO
     *  zPIV: Each zPIV has a keypair associated with it. The serial number is a hash of the public key.
     *  UTXO: The public key that signs must match the public key associated with the first utxo of the coinstake tx.
     */
    CPubKey pubkey;
    bool fzPIVStake = block.vtx[1]->vin[0].IsZerocoinSpend();
    if (fzPIVStake) {
        libzerocoin::CoinSpend spend = ZPIVModule::TxInToZerocoinSpend(block.vtx[1]->vin[0]);
        pubkey = spend.getPubKey();
    } else {
        txnouttype whichType;
        std::vector<valtype> vSolutions;
        const CTxOut& txout = block.vtx[1]->vout[1];
        if (!Solver(txout.scriptPubKey, whichType, vSolutions))
            return false;

        if (whichType == TX_PUBKEY) {
            valtype& vchPubKey = vSolutions[0];
            pubkey = CPubKey(vchPubKey);
        } else if (whichType == TX_PUBKEYHASH) {
            const CTxIn& txin = block.vtx[1]->vin[0];
            // Check if the scriptSig is for a p2pk or a p2pkh
            if (txin.scriptSig.size() == 73) { // Sig size + DER signature size.
                // If the input is for a p2pk and the output is a p2pkh.
                // We don't have the pubkey to verify the block sig anywhere in this block.
                // p2pk scriptsig only contains the signature and p2pkh scriptpubkey only contain the hash.
                return false;
            } else {
                unsigned int start = 1 + (unsigned int) *txin.scriptSig.begin(); // skip sig
                if (start >= txin.scriptSig.size() - 1) return false;
                pubkey = CPubKey(txin.scriptSig.begin()+start+1, txin.scriptSig.end());
            }
        } else if (whichType == TX_COLDSTAKE) {
            // pick the public key from the P2CS input
            const CTxIn& txin = block.vtx[1]->vin[0];
            unsigned int start = 1 + (unsigned int) *txin.scriptSig.begin(); // skip sig
            if (start >= txin.scriptSig.size() - 1) return false;
            start += 1 + (int) *(txin.scriptSig.begin()+start); // skip flag
            if (start >= txin.scriptSig.size() - 1) return false;
            pubkey = CPubKey(txin.scriptSig.begin()+start+1, txin.scriptSig.end());
        }
    }

    if (!pubkey.IsValid())
        return error("%s: invalid pubkey %s", __func__, HexStr(pubkey));

    return pubkey.Verify(block.GetHash(), block.vchBlockSig);
}
