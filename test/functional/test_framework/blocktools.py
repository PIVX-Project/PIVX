#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Utilities for manipulating blocks and transactions."""

from .messages import (
    CBlock,
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    NullOutPoint
)
from .script import CScript, CScriptNum, CScriptOp, OP_TRUE, OP_CHECKSIG, OP_1


# Create a block (with regtest difficulty)
def create_block(hashprev, coinbase, nTime=None, nVersion=None, hashFinalSaplingRoot=None):
    block = CBlock()
    if nTime is None:
        import time
        block.nTime = int(time.time()+600)
    else:
        block.nTime = nTime
    if nVersion is not None:
        block.nVersion = nVersion
    if hashFinalSaplingRoot is not None:
        block.hashFinalSaplingRoot = hashFinalSaplingRoot
    block.hashPrevBlock = hashprev
    block.nBits = 0x207fffff  # difficulty retargeting is disabled in REGTEST chainparams
    block.vtx.append(coinbase)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block

def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(int(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

def cbase_value(height):
    #return ((50 * COIN) >> int(height/150))
    return (250 * COIN)

def script_BIP34_coinbase_height(height):
    if height <= 16:
        res = CScriptOp.encode_op_n(height)
        # Append dummy to increase scriptSig size above 2 (see bad-cb-length consensus rule)
        return CScript([res, OP_1])
    return CScript([CScriptNum(height)])

# Create a coinbase transaction, assuming no miner fees.
# If pubkey is passed in, the coinbase output will be a P2PK output;
# otherwise an anyone-can-spend output.
def create_coinbase(height, pubkey = None):
    coinbase = CTransaction()
    coinbase.vin = [CTxIn(NullOutPoint, script_BIP34_coinbase_height(height), 0xffffffff)]
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = cbase_value(height)
    if (pubkey is not None):
        coinbaseoutput.scriptPubKey = CScript([pubkey, OP_CHECKSIG])
    else:
        coinbaseoutput.scriptPubKey = CScript([OP_TRUE])
    coinbase.vout = [coinbaseoutput]
    coinbase.calc_sha256()
    return coinbase

# Create a transaction.
# If the scriptPubKey is not specified, make it anyone-can-spend.
def create_transaction(prevtx, n, sig, value, scriptPubKey=CScript()):
    tx = CTransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(CTxOut(value, scriptPubKey))
    tx.calc_sha256()
    return tx

def create_transaction_from_outpoint(outPoint, sig, value, scriptPubKey=CScript()):
    tx = CTransaction()
    tx.vin.append(CTxIn(outPoint, sig, 0xffffffff))
    tx.vout.append(CTxOut(value, scriptPubKey))
    tx.calc_sha256()
    return tx

def get_legacy_sigopcount_block(block, fAccurate=True):
    count = 0
    for tx in block.vtx:
        count += get_legacy_sigopcount_tx(tx, fAccurate)
    return count

def get_legacy_sigopcount_tx(tx, fAccurate=True):
    count = 0
    for i in tx.vout:
        count += i.scriptPubKey.GetSigOpCount(fAccurate)
    for j in tx.vin:
        # scriptSig might be of type bytes, so convert to CScript for the moment
        count += CScript(j.scriptSig).GetSigOpCount(fAccurate)
    return count

# hemis specific blocktools
def create_coinbase_pos(height):
    coinbase = CTransaction()
    coinbase.vin = [CTxIn(NullOutPoint, script_BIP34_coinbase_height(height), 0xffffffff)]
    coinbase.vout = [CTxOut(0, b"")]
    coinbase.calc_sha256()
    return coinbase
