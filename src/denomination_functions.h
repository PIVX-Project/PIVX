// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "reverse_iterate.h"
#include "util.h"
#include "libzerocoin/Denominations.h"
#include "zpiv/zerocoin.h"
#include <list>
#include <map>
std::vector<CMintMeta> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue,
                                               int nMaxNumberOfSpends,
                                               bool fMinimizeChange,
                                               int& nCoinsReturned,
                                               const std::list<CMintMeta>& listMints,
                                               const std::map<libzerocoin::CoinDenomination, CAmount> mapDenomsHeld,
                                               int& nNeededSpends
                                               );

int calculateChange(
    int nMaxNumberOfSpends,
    bool fMinimizeChange,
    const CAmount nValueTarget,
    const std::map<libzerocoin::CoinDenomination, CAmount>& mapOfDenomsHeld,
    std::map<libzerocoin::CoinDenomination, CAmount>& mapOfDenomsUsed);

void listSpends(const std::vector<CMintMeta>& vSelectedMints);
