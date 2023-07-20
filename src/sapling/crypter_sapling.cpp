// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"

#include "script/script.h"
#include "script/standard.h"
#include "util/system.h"
#include "uint256.h"

bool CCryptoKeyStore::AddCryptedSaplingSpendingKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char> &vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted()) {
        return false;
    }

    // if extfvk is not in SaplingFullViewingKeyMap, add it
    if (!AddSaplingFullViewingKey(extfvk)) {
        return false;
    }

    mapCryptedSaplingSpendingKeys[extfvk] = vchCryptedSecret;
    return true;
}

static bool DecryptSaplingSpendingKey(const CKeyingMaterial& vMasterKey,
                                      const std::vector<unsigned char>& vchCryptedSecret,
                                      const libzcash::SaplingExtendedFullViewingKey& extfvk,
                                      libzcash::SaplingExtendedSpendingKey& sk)
{
    CKeyingMaterial vchSecret;
    if (!DecryptSecret(vMasterKey, vchCryptedSecret, extfvk.fvk.GetFingerprint(), vchSecret))
        return false;

    if (vchSecret.size() != ZIP32_XSK_SIZE)
        return false;

    CSecureDataStream ss(vchSecret, SER_NETWORK, PROTOCOL_VERSION);
    ss >> sk;
    return sk.expsk.full_viewing_key() == extfvk.fvk;
}

bool CCryptoKeyStore::GetSaplingSpendingKey(
        const libzcash::SaplingExtendedFullViewingKey& extfvk,
        libzcash::SaplingExtendedSpendingKey& skOut) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::GetSaplingSpendingKey(extfvk, skOut);
    }

    auto it = mapCryptedSaplingSpendingKeys.find(extfvk);
    if (it == mapCryptedSaplingSpendingKeys.end()) return false;
    const std::vector<unsigned char>& vchCryptedSecret = it->second;
    return DecryptSaplingSpendingKey(vMasterKey, vchCryptedSecret, it->first, skOut);
}

bool CCryptoKeyStore::HaveSaplingSpendingKey(const libzcash::SaplingExtendedFullViewingKey& extfvk) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::HaveSaplingSpendingKey(extfvk);
    }
    return mapCryptedSaplingSpendingKeys.count(extfvk) > 0;
}

bool CCryptoKeyStore::UnlockSaplingKeys(const CKeyingMaterial& vMasterKeyIn, bool fDecryptionThoroughlyChecked)
{
    if (mapCryptedSaplingSpendingKeys.empty()) {
        LogPrintf("%s: mapCryptedSaplingSpendingKeys empty. No need to unlock anything.\n", __func__);
        return true;
    }

    bool keyFail = false;
    bool keyPass = false;
    CryptedSaplingSpendingKeyMap::const_iterator miSapling = mapCryptedSaplingSpendingKeys.begin();
    for (; miSapling != mapCryptedSaplingSpendingKeys.end(); ++miSapling) {
        const libzcash::SaplingExtendedFullViewingKey &extfvk = (*miSapling).first;
        const std::vector<unsigned char> &vchCryptedSecret = (*miSapling).second;
        libzcash::SaplingExtendedSpendingKey sk;
        if (!DecryptSaplingSpendingKey(vMasterKeyIn, vchCryptedSecret, extfvk, sk)) {
            keyFail = true;
            break;
        }
        keyPass = true;
        if (fDecryptionThoroughlyChecked)
            break;
    }

    if (keyPass && keyFail) {
        LogPrintf("Sapling wallet is probably corrupted: Some keys decrypt but not all.");
        throw std::runtime_error("Error unlocking sapling wallet: some keys decrypt but not all. Your wallet file may be corrupt.");
    }
    if (keyFail || !keyPass)
        return false;

    return true;
}
