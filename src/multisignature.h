// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_MULTISIGNATURE_H
#define PIVX_MULTISIGNATURE_H


#include <script/script.h>
#include <utility>
#include "base58.h"
#include "init.h"


class CMultisignatureAddress
{
private:
    CBitcoinAddress addressFull;
    CScript scriptRedeem;
    std::vector<CPubKey> vOwners;
    int nOwners = -1;
    int nSigsRequired = -1;
    std::string strErrorStatus = "";

    CMultisignatureAddress() = default;
    bool HandleError(const std::string& err);
    void CreateEmptyInstance();
    void CreateRedeemScript();
    bool ValidateConfiguration();
    bool ConvertAndValidatePubKeys(std::vector<std::string> vstrPubKeys);
    bool ParseRedeemScript(const std::string& strRedeemScript);
    void ParseRPCRedeem(const std::string& strRedeemScript);
    void ParseHexRedeem(const std::string& strRedeemScript);
    void ParseSpacedRedeem(const std::string& strRedeemScript);

public:
    explicit CMultisignatureAddress(std::string strRedeemScript);
    CMultisignatureAddress(int nSignaturesRequired, const std::vector<std::string>& vAddressOwners);
    CScript GetRedeemScript() const { return scriptRedeem; }
    CBitcoinAddress GetAddress() const { return addressFull; }
    int GetSignaturesRequired() const { return nSigsRequired; }
    std::string GetErrorStatus() const { return strErrorStatus; }
    std::vector<CPubKey> GetAddressOwners() const { return vOwners; }

#ifdef ENABLE_WALLET
    bool AddToWallet(const std::string addressLabel);
#endif
};


class CMultisignatureException : public std::exception
{
private:
    const std::string strErr;
public:
    explicit CMultisignatureException(std::string strError) : strErr(std::move(strError)) {}
    const char * what() const noexcept override { return strErr.data(); }
};


#endif //PIVX_MULTISIGNATURE_H
