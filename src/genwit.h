// Copyright (c) 2015-2018 The VPX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VPX_GENWIT_H
#define VPX_GENWIT_H


#include <iostream>
#include "bloom.h"
#include "libzerocoin/Denominations.h"
#include "net.h"

class CGenWit {

    public:

    CGenWit();

    CGenWit(const CBloomFilter &filter, int startingHeight, libzerocoin::CoinDenomination den, int requestNum, CBigNum accWitValue = 0);

    bool isValid(int chainActiveHeight);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(filter);
        filter.setNotFull();
        READWRITE(startingHeight);
        READWRITE(den);
        READWRITE(requestNum);
        READWRITE(accWitValue);
    }

    const CBloomFilter &getFilter() const;

    int getStartingHeight() const;

    libzerocoin::CoinDenomination getDen() const;

    int getRequestNum() const;

    CNode *getPfrom() const;

    void setPfrom(CNode *pfrom);

    const CBigNum &getAccWitValue() const;

    const std::string toString() const;

private:
    CBloomFilter filter;
    int startingHeight;
    libzerocoin::CoinDenomination den;
    int requestNum;
    CBigNum accWitValue;
    CNode* pfrom;
};


#endif //VPX_GENWIT_H
