// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_ISMINE_H
#define BITCOIN_WALLET_ISMINE_H

#include "key.h"
#include "script/standard.h"

class CKeyStore;
class CScript;

/** IsMine() return codes */
enum isminetype {
    ISMINE_NO = 0,
    //! Indicates that we dont know how to create a scriptSig that would solve this if we were given the appropriate private keys
    ISMINE_WATCH_UNSOLVABLE = 1,
    //! Indicates that we know how to create a scriptSig that would solve this if we were given the appropriate private keys
    ISMINE_WATCH_SOLVABLE = 2,
    ISMINE_WATCH_ONLY = ISMINE_WATCH_SOLVABLE | ISMINE_WATCH_UNSOLVABLE,
    ISMINE_SPENDABLE  = 4,
    //! Indicates that we have the staking key of a P2CS
    ISMINE_COLD = 8,
    //! Indicates that we have the spending key of a P2CS
    ISMINE_SPENDABLE_DELEGATED = 16,
    ISMINE_SPENDABLE_ALL = ISMINE_SPENDABLE_DELEGATED | ISMINE_SPENDABLE,
    ISMINE_SPENDABLE_STAKEABLE = ISMINE_SPENDABLE_DELEGATED | ISMINE_COLD,
    ISMINE_ALL = ISMINE_WATCH_ONLY | ISMINE_SPENDABLE | ISMINE_COLD | ISMINE_SPENDABLE_DELEGATED
};
/** used for bitflags of isminetype */
typedef uint8_t isminefilter;

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey);
isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest);

#endif // BITCOIN_WALLET_ISMINE_H
