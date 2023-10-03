// Copyright (c) 2020-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_LEGACY_ZPOS_H
#define PIVX_LEGACY_ZPOS_H

#include "stakeinput.h"
#include "txdb.h"
#include <stdexcept>

class CLegacyZPivStake : public CStakeInput
{
private:
    uint32_t nChecksum{0};
    libzerocoin::CoinDenomination denom{libzerocoin::ZQ_ERROR};
    uint256 hashSerial{UINT256_ZERO};

public:
    CLegacyZPivStake(const CBlockIndex* _pindexFrom, uint32_t _nChecksum, libzerocoin::CoinDenomination _denom, const uint256& _hashSerial) :
        CStakeInput(_pindexFrom),
        nChecksum(_nChecksum),
        denom(_denom),
        hashSerial(_hashSerial)
    {}

    static CLegacyZPivStake* NewZPivStake(const CTxIn& txin, int nHeight);

    bool IsZPIV() const override { return true; }
    bool IsShieldPIV() const override { return false; };
    uint32_t GetChecksum() const { return nChecksum; }
    const CBlockIndex* GetIndexFrom() const override;
    CAmount GetValue() const override;
    CDataStream GetUniqueness() const override;
    bool GetTxOutFrom(CTxOut& out) const override { return false; /* not available */ }
    CTxIn GetTxIn() const override
    {
        throw "can't be bothered";
    }
    std::pair<uint256, uint32_t> GetSpendInfo() const override { throw new std::runtime_error{"Function non defined for zPiv"}; };
};

#endif //PIVX_LEGACY_ZPOS_H
