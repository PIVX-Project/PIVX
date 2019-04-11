// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <streams.h>
#include "zerocoin.h"
#include "hash.h"
#include "util.h"
#include "utilstrencodings.h"

bool CMintMeta::operator <(const CMintMeta& a) const
{
    return this->hashPubcoin < a.hashPubcoin;
}

uint256 GetSerialHash(const CBigNum& bnSerial)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnSerial;
    return Hash(ss.begin(), ss.end());
}

uint256 GetPubCoinHash(const CBigNum& bnValue)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << bnValue;
    return Hash(ss.begin(), ss.end());
}

bool CZerocoinMint::GetKeyPair(CKey &key) const
{
    if (version < STAKABLE_VERSION)
        return error("%s: version is %d", __func__, version);

    if (privkey.empty())
        return error("%s: empty privkey %s", __func__, privkey.data());

    return key.SetPrivKey(privkey, true);
}

std::string CZerocoinMint::ToString() const
{
    std::string str = strprintf("\n  ZerocoinMint:\n   version=%d   \ntxfrom=%s   \nheight=%d \n   randomness: %s   \n serial %s   \n privkey %s\n",
                                version, txid.GetHex(), nHeight, randomness.GetHex(), serialNumber.GetHex(), HexStr(privkey));
    return str;
}

void CZerocoinSpendReceipt::AddSpend(const CZerocoinSpend& spend)
{
    vSpends.emplace_back(spend);
}

std::vector<CZerocoinSpend> CZerocoinSpendReceipt::GetSpends()
{
    return vSpends;
}

void CZerocoinSpendReceipt::SetStatus(std::string strStatus, int nStatus, int nNeededSpends)
{
    strStatusMessage = strStatus;
    this->nStatus = nStatus;
    this->nNeededSpends = nNeededSpends;
}

std::string CZerocoinSpendReceipt::GetStatusMessage()
{
    return strStatusMessage;
}

int CZerocoinSpendReceipt::GetStatus()
{
    return nStatus;
}

int CZerocoinSpendReceipt::GetNeededSpends()
{
    return nNeededSpends;
}


int GetWrapppedSerialInflation(libzerocoin::CoinDenomination denom){
    if(Params().NetworkID() == CBaseChainParams::MAIN) {
        switch (denom) {
            case libzerocoin::CoinDenomination::ZQ_ONE:
                return 7;
            case libzerocoin::CoinDenomination::ZQ_FIVE:
                return 6;
            case libzerocoin::CoinDenomination::ZQ_TEN:
                return 36;
            case libzerocoin::CoinDenomination::ZQ_FIFTY:
                return 22;
            case libzerocoin::CoinDenomination::ZQ_ONE_HUNDRED:
                return 244;
            case libzerocoin::CoinDenomination::ZQ_FIVE_HUNDRED:
                return 22;
            case libzerocoin::CoinDenomination::ZQ_ONE_THOUSAND:
                return 42;
            case libzerocoin::CoinDenomination::ZQ_FIVE_THOUSAND:
                return 98;
            default:
                throw std::runtime_error("GetWrapSerialInflation :: Invalid denom");
        }
    }else{
        // Testnet/Regtest is ok.
        return 0;
    }
}

int64_t GetWrapppedSerialInflationAmount(){
    int64_t amount = 0;
    for (auto& denom : libzerocoin::zerocoinDenomList){
        amount += GetWrapppedSerialInflation(denom) * libzerocoin::ZerocoinDenominationToAmount(denom);
    }
    return amount;
}

