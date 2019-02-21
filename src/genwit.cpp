//
// Copyright (c) 2015-2018 The VPX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iostream>
#include "genwit.h"
#include "chainparams.h"
#include "util.h"

CGenWit::CGenWit() : accWitValue(0) {}

CGenWit::CGenWit(const CBloomFilter &filter, int startingHeight, libzerocoin::CoinDenomination den, int requestNum, CBigNum accWitValue)
        : filter(filter), startingHeight(startingHeight), den(den), requestNum(requestNum), accWitValue(accWitValue) {}

bool CGenWit::isValid(int chainActiveHeight) {
    if (den == libzerocoin::CoinDenomination::ZQ_ERROR){
        return error("%s: ERROR: invalid denomination", __func__);
    }
    if(!filter.IsWithinSizeConstraints()){
        return error("%s: ERROR: filter not within size constraints", __func__);
    }

    if (startingHeight < Params().Zerocoin_Block_V2_Start()){
        return error("%s: ERROR: starting height before V2 activation", __func__);
    }

    if (accWitValue == 0){
        return error("%s: ERROR: invalid accWit value", __func__);
    }

    return (startingHeight < chainActiveHeight - 20);
}

const CBloomFilter &CGenWit::getFilter() const {
    return filter;
}

int CGenWit::getStartingHeight() const {
    return startingHeight;
}

libzerocoin::CoinDenomination CGenWit::getDen() const {
    return den;
}

int CGenWit::getRequestNum() const {
    return requestNum;
}

CNode *CGenWit::getPfrom() const {
    return pfrom;
}

void CGenWit::setPfrom(CNode *pfrom) {
    CGenWit::pfrom = pfrom;
}

const CBigNum &CGenWit::getAccWitValue() const {
    return accWitValue;
}

const std::string CGenWit::toString() const {
    return "From: " + pfrom->addrName + ",\n" +
           "Height: " + std::to_string(startingHeight) + ",\n" +
           "accWit: " + accWitValue.GetHex();
}
