// Copyright (c) 2017-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_STAKEINPUT_H
#define PIVX_STAKEINPUT_H

#include "amount.h"
#include "chain.h"
#include "primitives/transaction.h"
#include "sapling/sapling_transaction.h"
#include "streams.h"
#include "uint256.h"
#include "validation.h"
#include <stdexcept>
#include <utility>

class CKeyStore;
class CWallet;
class CWalletTx;

class CStakeInput
{
protected:
    const CBlockIndex* pindexFrom = nullptr;

public:
    CStakeInput(const CBlockIndex* _pindexFrom) : pindexFrom(_pindexFrom) {}
    virtual ~CStakeInput(){};
    virtual const CBlockIndex* GetIndexFrom() const = 0;
    virtual CAmount GetValue() const = 0;
    virtual bool IsZPIV() const = 0;
    virtual bool IsShieldPIV() const = 0;
    virtual CDataStream GetUniqueness() const = 0;
    virtual bool GetTxOutFrom(CTxOut& out) const = 0;
    virtual CTxIn GetTxIn() const = 0;
    // Return the basic info to understand if a CStakeInput has been spent or not
    virtual std::pair<uint256, uint32_t> GetSpendInfo() const = 0;
};


class CPivStake : public CStakeInput
{
private:
    const CTxOut outputFrom;
    const COutPoint outpointFrom;

public:
    CPivStake(const CTxOut& _from, const COutPoint& _outPointFrom, const CBlockIndex* _pindexFrom) :
            CStakeInput(_pindexFrom), outputFrom(_from), outpointFrom(_outPointFrom) {}

    static CPivStake* NewPivStake(const CTxIn& txin, int nHeight, uint32_t nTime);

    const CBlockIndex* GetIndexFrom() const override;
    bool GetTxOutFrom(CTxOut& out) const override;
    CAmount GetValue() const override;
    CDataStream GetUniqueness() const override;
    CTxIn GetTxIn() const override;
    bool IsZPIV() const override { return false; }
    bool IsShieldPIV() const override { return false; };
    // The pair (tx hash, vout index) is enough to understand if the utxo has been spent
    std::pair<uint256, uint32_t> GetSpendInfo() const override { return {outpointFrom.hash, outpointFrom.n}; };
};

class CShieldStake : public CStakeInput
{
private:
    // The nullifier is a unique identifier for the note
    const uint256 nullifier;
    // Despite what the name suggests, this is NOT the note value, but just a lower bound
    const CAmount noteValue;

public:
    CShieldStake(uint256 _nullifier, CAmount _noteValue) : CStakeInput(nullptr), nullifier(_nullifier), noteValue(_noteValue) {}
    static CShieldStake* NewShieldStake(const SpendDescription& spendDescription, CAmount noteValue, int nHeight, uint32_t nTime);
    CAmount GetValue() const override { return noteValue; };
    CDataStream GetUniqueness() const override;

    const CBlockIndex* GetIndexFrom() const override { throw std::runtime_error("Cannot find the BlockIndex for a shield note"); };
    bool IsZPIV() const override { return false; }
    bool IsShieldPIV() const override { return true; };
    bool GetTxOutFrom(CTxOut& out) const override
    {
        return false;
    }
    CTxIn GetTxIn() const override
    {
        throw new std::runtime_error("Cannot find Txin in a shield note");
    }
    // The nullifier is enough to understand if the note has been spent
    std::pair<uint256, uint32_t> GetSpendInfo() const override { return {nullifier, 0}; };
};

#endif //PIVX_STAKEINPUT_H
