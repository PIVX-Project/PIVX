// Copyright (c) 2017-2018 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <regex>
#include <boost/algorithm/string/split.hpp>
#include "multisignature.h"
#include "utilstrencodings.h"
#include "tinyformat.h"
#include "util.h"
#include "streams.h"
#include "wallet.h"


using namespace std;

CMultisignatureAddress::CMultisignatureAddress(int nSignaturesRequired, const vector<string>& vAddressOwners)
{
    nSigsRequired = nSignaturesRequired;
    nOwners = (int)vAddressOwners.size();

    if(!ValidateConfiguration()) {
        return;
    }

    if(!ConvertAndValidatePubKeys(vAddressOwners)){
        return;
    }

    CreateRedeemScript();
}


CMultisignatureAddress::CMultisignatureAddress(string strRedeemScript)
{
    ParseRedeemScript(strRedeemScript);
}

bool CMultisignatureAddress::ValidateConfiguration()
{//gather pub keys
    try {
        if(this->nOwners == -1 || this->nSigsRequired == -1) {
            throw CMultisignatureException("Failed to read input");
        }
        if (nOwners < 1) {
            throw CMultisignatureException("a Multisignature address requires at least one key to redeem");
        }
        if (nOwners < nSigsRequired) {
            throw CMultisignatureException(
                strprintf("not enough keys supplied (got %d keys, but need at least %d to redeem)", nOwners,
                          nSigsRequired
                ));
        }
        if (nOwners > 15) {
            throw CMultisignatureException(
                "Number of addresses involved in the Multisignature address creation > 15\nReduce the number"
            );
        }
        return true;
    } catch (const CMultisignatureException& ex) {
        return HandleError(ex.what());
    }
}

bool CMultisignatureAddress::ConvertAndValidatePubKeys(vector<string> vstrPubKeys)
{
    try {
        for (auto &strKey : vstrPubKeys) {
#ifdef ENABLE_WALLET
            // Case 1: PIVX address and we have full public key:
            CBitcoinAddress address(strKey);
            if (pwalletMain && address.IsValid()) {
                CKeyID keyID;
                if (!address.GetKeyID(keyID)) {
                    throw CMultisignatureException(strprintf("%s does not refer to a key", strKey));
                }
                CPubKey vchPubKey;
                if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                    throw CMultisignatureException(strprintf("no full public key for address %s", strKey));
                if (!vchPubKey.IsFullyValid()){
                    string sKey = strKey.empty()?"(empty)":strKey;
                    throw CMultisignatureException("Invalid public key: " + sKey);
                }
                vOwners.emplace_back(vchPubKey);
            }

            else
#endif
            if (IsHex(strKey)) {
                CPubKey vchPubKey(ParseHex(strKey));
                if (vchPubKey.IsFullyValid()) {
                    vOwners.emplace_back(vchPubKey);
                }
            } else {
                throw CMultisignatureException(strprintf(" Invalid public key: %s", strKey));
            }
        }
        return true;
    } catch (const CMultisignatureException& ex) {
        return HandleError(ex.what());
    }
}

void CMultisignatureAddress::CreateRedeemScript()
{
    scriptRedeem << CScript::EncodeOP_N(nSigsRequired);
    //public keys
    for(const CPubKey& key : vOwners) {
        scriptRedeem << ToByteVector(key);
    }
    //OP_N for total pubkeys
    scriptRedeem << CScript::EncodeOP_N(nOwners);
    scriptRedeem << OP_CHECKMULTISIG;

    addressFull = CBitcoinAddress(CScriptID(scriptRedeem));
}

bool CMultisignatureAddress::ParseRedeemScript(const string& strRedeemScript)
{
    try {
        size_t open = strRedeemScript.find_first_of('[');
        size_t close = strRedeemScript.find_first_of(']');

        if (open != string::npos && close != string::npos) {
            ParseRPCRedeem(strRedeemScript);
        } else if (IsHex(strRedeemScript)) {
            ParseHexRedeem(strRedeemScript);
        } else { // if neither RPC or Hex assume spaced
            ParseSpacedRedeem(strRedeemScript);
        }
        return true;
    } catch(const CMultisignatureException& ex) {
        return HandleError(ex.what());
    }
}

void CMultisignatureAddress::ParseRPCRedeem(const string& strRedeemScript)
{
    //extract sigs required
    regex sigsRequiredExtractor("([0-9][0-9]?) \\[");
    cmatch charactersMatched;
    regex_search(strRedeemScript.data(), charactersMatched, sigsRequiredExtractor);

    //match result +1 because don't want full match, just the number
    int nSignature;
    try {
        nSignature = stoi((*(charactersMatched.begin()+1)).str());
    } catch (...) {
        throw CMultisignatureException("failed to get amount of signatures required");
    }

    //extract pubkey array
    string substrPubKeys(strRedeemScript.substr(strRedeemScript.find_first_of('['), strRedeemScript.find_first_of(']')));

    //extract pubkeys
    vector<string> vstrPubKeys;
    smatch sm;
    regex pubKeyExtractor(R"(\"([A-Z0-9a-z]+)\")");
    sregex_iterator it(substrPubKeys.begin(), substrPubKeys.end(), pubKeyExtractor);

    //iterate through matches and retrieve keys
    while(it != sregex_iterator{}) {
        sm = smatch(*it);
        for(auto i = sm.begin()+1; i != sm.end(); i++) {
            auto str = (*it).str();
            string result;
            remove_copy(str.begin(), str.end(), back_inserter(result), '"');
            vstrPubKeys.emplace_back(result);
        }
        it++;
    }

    *this = CMultisignatureAddress(nSignature, vstrPubKeys);
}

void CMultisignatureAddress::ParseHexRedeem(const string& strRedeemScript)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << ParseHex(strRedeemScript);
    CScript redeem;
    ss >> redeem;
    ParseSpacedRedeem(redeem.ToString());
}

void CMultisignatureAddress::ParseSpacedRedeem(const string& strRedeemScript)
{
    //trim leading and trailing spaces
    string trimmed = regex_replace(strRedeemScript, regex("^\\s+"), "");
    trimmed = regex_replace(strRedeemScript, regex("\\s+$"), "");

    vector<string> vstrSplitRedeem;
    boost::split(vstrSplitRedeem, trimmed, [](char c){return c == ' ';});

    int nSignatures = -1;
    try {
        nSignatures = stoi(vstrSplitRedeem.at(0));
        vstrSplitRedeem.erase(vstrSplitRedeem.begin());
    } catch (...) {
        HandleError("failed to get amount of signatures required");
    }

    if(vstrSplitRedeem.back() == "OP_CHECKMULTISIG") {
        vstrSplitRedeem.pop_back(); //remove OP code
    }

    if(!IsHex(vstrSplitRedeem.back())) {
        vstrSplitRedeem.pop_back(); //remove total owners if present
    }

    *this = CMultisignatureAddress(nSignatures, vector<string>(vstrSplitRedeem.begin(), vstrSplitRedeem.end()));
}

bool CMultisignatureAddress::HandleError(const string& err)
{
    strErrorStatus = err;
    LogPrintf(err.data());
    return false;
}

//wallet only
#ifdef ENABLE_WALLET
bool CMultisignatureAddress::AddToWallet(const std::string addressLabel)
{
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        try {
            if (::IsMine(*pwalletMain, scriptRedeem) == ISMINE_SPENDABLE) {
                return HandleError("The wallet already contains this script");
            }

            if(!pwalletMain->AddCScript(scriptRedeem)) {
                return HandleError("address invalid or already exists");
            }

            CScriptID scriptID(scriptRedeem);
            if(!pwalletMain->AddMultiSig(scriptRedeem)) {
                return HandleError("Failed to add script to wallet");
            }

            pwalletMain->SetAddressBook(scriptID, addressLabel, "receive");
            return true;
        } catch(...) {
            return false;
        }
    }
}
#endif