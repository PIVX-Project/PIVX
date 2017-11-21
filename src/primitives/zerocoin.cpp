// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/zerocoin.h"
#include "init.h"
#include "util.h"
#include "wallet.h"

void CZerocoinMint::ZerocoinCrypterSetup() {
    CKey key;
    CPubKey vchPubKey;

    if(pwalletMain->IsLocked()) {
        LogPrintf("%s : failed to encrypt zerocoin mint since the wallet is locked\n", __func__);
        return;
    }

    assert(CReserveKey(pwalletMain).GetReservedKey(vchPubKey)); //should never fail

    if (!pwalletMain->GetKey(vchPubKey.GetID(), key)) {
        LogPrintf("%s : failed to encrypt zerocoin mint. Could not find key %s\n", __func__,
                  vchPubKey.GetHash().ToString().c_str());
        return;
    }

    //inialize IV for 32 bytes
    vector<unsigned char> chIV(crypter.KEY_SIZE);
    vector<unsigned char> vchSecret(key.size());

    //populate initialization vector (IV) with hash of pubKey
    memcpy(&chIV[0], vchPubKey.GetHash().begin(), crypter.KEY_SIZE);
    memcpy(&vchSecret[0], key.begin(), crypter.KEY_SIZE);

    if (!crypter.SetKey(vchSecret, chIV)) {
        LogPrintf("%s : failed to encrypt zerocoin mint. Could not set key in crypter\n", __func__);
    }
}

bool CZerocoinMint::Encrypt()
{
    if(!crypter.HasKey()) {
        LogPrintf("%s : failed to encrypt zerocoin mint. Crypter not setup", __func__);
        return false;
    }

    CKey cipherKey;
    if(!pwalletMain->GetKey(crypter.GetKeyID(), cipherKey)) {
        LogPrintf("%s : failed to encrypt zerocoin mint. Could not GetKey for ID: %s\n", __func__, crypter.GetKeyID().ToString().c_str());
        return false;
    }

    if(!crypter.Encrypt(serialNumber) ||
       !crypter.Encrypt(randomness)) {

        LogPrintf("%s : failed to encrypt zerocoin mint\n", __func__);
        return false;
    }

    return true;
}


bool CZerocoinMint::Decrypt()
{
    if(pwalletMain->IsCrypted()) {
        if(!crypter.HasKey()) {
            LogPrintf("%s : failed to decrypt zerocoin mint. Crypter not setup", __func__);
            return false;
        }

        CKey cipherKey;
        if(!pwalletMain->GetKey(crypter.GetKeyID(), cipherKey)){
            LogPrintf("%s : failed to decrypt zerocoin mint. Could not GetKey for ID: %s\n", __func__, crypter.GetKeyID().ToString().c_str());
            return false;
        }

        if(!crypter.Decrypt(serialNumber) ||
           !crypter.Decrypt(randomness)) {

            LogPrintf("%s : failed to encrypt zerocoin mint\n", __func__);
            return false;
        }

    }
    return true;
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
