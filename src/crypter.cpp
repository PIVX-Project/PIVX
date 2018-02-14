// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"

#include "script/script.h"
#include "script/standard.h"
#include "util.h"
#include "primitives/zerocoin.h"
#include "libzerocoin/bignum.h"
#include "init.h"
#include "wallet.h"

#include <boost/foreach.hpp>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <string>
#include <vector>

bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
            (unsigned char*)&strKeyData[0], strKeyData.size(), nRounds, chKey, chIV);

    if (i != (int)WALLET_CRYPTO_KEY_SIZE) {
        OPENSSL_cleanse(chKey, sizeof(chKey));
        OPENSSL_cleanse(chIV, sizeof(chIV));
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_KEY_SIZE)
        return false;

    memcpy(&chKey[0], &chNewKey[0], sizeof chKey);
    memcpy(&chIV[0], &chNewIV[0], sizeof chIV);

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char>& vchCiphertext)
{
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char>(nCLen);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_EncryptUpdate(&ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen) != 0;
    if (fOk) fOk = EVP_EncryptFinal_ex(&ctx, (&vchCiphertext[0]) + nCLen, &nFLen) != 0;
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext)
{
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    vchPlaintext = CKeyingMaterial(nPLen);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_DecryptUpdate(&ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen) != 0;
    if (fOk) fOk = EVP_DecryptFinal_ex(&ctx, (&vchPlaintext[0]) + nPLen, &nFLen) != 0;
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    vchPlaintext.resize(nPLen + nFLen);
    return true;
}


bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial& vchPlaintext, const uint256& nIV, std::vector<unsigned char>& vchCiphertext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt(*((const CKeyingMaterial*)&vchPlaintext), vchCiphertext);
}

bool CCrypter::CryptZerocoinMint(const CZerocoinMint &mintIn, CZerocoinMint& mintOut, CryptionMethod method)
{
    LogPrintf("inside zercoin crypt xyz123");
    if (!fKeySet)
        return false;

    //already encrypted
    if(mintIn.IsCrypted() && method == ENC) {
        mintOut = mintIn;
        return true;
    }

    //already decryted
    if(!mintIn.IsCrypted() && method == DEC) {
        mintOut = mintIn;
        return true;
    }

    vector< vector<unsigned char> > vchSecrets;

    vchSecrets[SERIAL] = mintIn.GetSerialNumber().getvch();
    vchSecrets[RANDOM] = mintIn.GetRandomness().getvch();

    for(auto& secret : vchSecrets) {
        int nSecretLength = (int) secret.size();
        int nCipherLength = nSecretLength + AES_BLOCK_SIZE, nFLen = 0;

        vector<unsigned char> secretCiphered;

        EVP_CIPHER_CTX ctx;

        bool fOk = true;
        switch(method) {
            case CryptionMethod::ENC:
                EVP_CIPHER_CTX_init(&ctx);
                if (fOk) fOk = EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
                if (fOk) fOk = EVP_EncryptUpdate(&ctx, &secretCiphered[0], &nCipherLength, &secret[0], nSecretLength) != 0;
                if (fOk) fOk = EVP_EncryptFinal_ex(&ctx, (&secretCiphered[0]) + nSecretLength, &nFLen) != 0;
                EVP_CIPHER_CTX_cleanup(&ctx);
                break;
            case CryptionMethod::DEC:
                EVP_CIPHER_CTX_init(&ctx);
                if (fOk) fOk = EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
                if (fOk) fOk = EVP_DecryptUpdate(&ctx, &secretCiphered[0], &nCipherLength, &secret[0], nSecretLength) != 0;
                if (fOk) fOk = EVP_DecryptFinal_ex(&ctx, (&secretCiphered[0]) + nCipherLength, &nFLen) != 0;
                EVP_CIPHER_CTX_cleanup(&ctx);
                break;
        }
        if (!fOk) return false;
    }

    mintOut = CZerocoinMint(mintIn);
    mintOut.SetSerialNumber(CBigNum(vchSecrets[SERIAL]));
    mintOut.SetRandomness(CBigNum(vchSecrets[RANDOM]));
    method == ENC ? mintOut.SetIsCrypted(true) : mintOut.SetIsCrypted(false);

    return true;
}

// General secure AES 256 CBC encryption routine
bool EncryptAES256(const SecureString& sKey, const SecureString& sPlaintext, const std::string& sIV, std::string& sCiphertext)
{
    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = sPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE;
    int nFLen = 0;

    // Verify key sizes
    if (sKey.size() != 32 || sIV.size() != AES_BLOCK_SIZE) {
        LogPrintf("crypter EncryptAES256 - Invalid key or block size: Key: %d sIV:%d\n", sKey.size(), sIV.size());
        return false;
    }

    // Prepare output buffer
    sCiphertext.resize(nCLen);

    // Perform the encryption
    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)&sKey[0], (const unsigned char*)&sIV[0]);
    if (fOk) fOk = EVP_EncryptUpdate(&ctx, (unsigned char*)&sCiphertext[0], &nCLen, (const unsigned char*)&sPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(&ctx, (unsigned char*)(&sCiphertext[0]) + nCLen, &nFLen);
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    sCiphertext.resize(nCLen + nFLen);
    return true;
}


bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CKeyingMaterial& vchPlaintext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, *((CKeyingMaterial*)&vchPlaintext));
}

bool DecryptAES256(const SecureString& sKey, const std::string& sCiphertext, const std::string& sIV, SecureString& sPlaintext)
{
    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = sCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    // Verify key sizes
    if (sKey.size() != 32 || sIV.size() != AES_BLOCK_SIZE) {
        LogPrintf("crypter DecryptAES256 - Invalid key or block size\n");
        return false;
    }

    sPlaintext.resize(nPLen);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)&sKey[0], (const unsigned char*)&sIV[0]);
    if (fOk) fOk = EVP_DecryptUpdate(&ctx, (unsigned char*)&sPlaintext[0], &nPLen, (const unsigned char*)&sCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(&ctx, (unsigned char*)(&sPlaintext[0]) + nPLen, &nFLen);
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    sPlaintext.resize(nPLen + nFLen);
    return true;
}


bool CCryptoKeyStore::SetCrypted()
{
    LOCK(cs_KeyStore);
    if (fUseCrypto)
        return true;
    if (!mapKeys.empty())
        return false;
    fUseCrypto = true;
    return true;
}

bool CCryptoKeyStore::Lock()
{
    if (!SetCrypted())
        return false;

    {
        LOCK(cs_KeyStore);
        vMasterKey.clear();
    }

    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        bool keyPass = false;
        bool keyFail = false;
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi) {
            const CPubKey& vchPubKey = (*mi).second.first;
            const std::vector<unsigned char>& vchCryptedSecret = (*mi).second.second;
            CKeyingMaterial vchSecret;
            if (!DecryptSecret(vMasterKeyIn, vchCryptedSecret, vchPubKey.GetHash(), vchSecret)) {
                keyFail = true;
                break;
            }
            if (vchSecret.size() != 32) {
                keyFail = true;
                break;
            }
            CKey key;
            key.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
            if (key.GetPubKey() != vchPubKey) {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }
        if (keyPass && keyFail) {
            LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.");
            assert(false);
        }
        if (keyFail || !keyPass)
            return false;
        vMasterKey = vMasterKeyIn;
        fDecryptionThoroughlyChecked = true;
    }
    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::AddKeyPubKey(const CKey& key, const CPubKey& pubkey)
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::AddKeyPubKey(key, pubkey);

        if (IsLocked())
            return false;

        std::vector<unsigned char> vchCryptedSecret;
        CKeyingMaterial vchSecret(key.begin(), key.end());
        if (!EncryptSecret(vMasterKey, vchSecret, pubkey.GetHash(), vchCryptedSecret))
            return false;

        if (!AddCryptedKey(pubkey, vchCryptedSecret))
            return false;
    }
    return true;
}


bool CCryptoKeyStore::AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    }
    return true;
}

bool CCryptoKeyStore::GetKey(const CKeyID& address, CKey& keyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetKey(address, keyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end()) {
            const CPubKey& vchPubKey = (*mi).second.first;
            const std::vector<unsigned char>& vchCryptedSecret = (*mi).second.second;
            CKeyingMaterial vchSecret;
            if (!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
                return false;
            if (vchSecret.size() != 32)
                return false;
            keyOut.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CKeyStore::GetPubKey(address, vchPubKeyOut);

        auto mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end()) {
            vchPubKeyOut = (*mi).second.first;
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!mapCryptedKeys.empty() || IsCrypted()) {
            return false;
        }

        fUseCrypto = true;
        for(auto& mKey : mapKeys) {
            const CKey& key = mKey.second;
            CPubKey vchPubKey = key.GetPubKey();
            CKeyingMaterial vchSecret(key.begin(), key.end());
            std::vector<unsigned char> vchCryptedSecret;
            if (!EncryptSecret(vMasterKeyIn, vchSecret, vchPubKey.GetHash(), vchCryptedSecret))
                return false;
            if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
                return false;
        }
        mapKeys.clear();

        if(!EncryptZerocoinMints(vMasterKeyIn)) {
            return false;
        }
    }

    return true;
}

bool CCryptoKeyStore::AddCryptedZerocoinMint(const CZerocoinMint &mintCrypted)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted()) {
            LogPrintf("%s : wallet is not encrypted\n", __func__);
            return false;
        }
//        LogPrintf("%s - %i\n", __func__, 1);


        if(!mintCrypted.IsCrypted()) {
            LogPrintf("%s : zerocoin mint is not encrypted\n", __func__);
            return false;
        }
//        LogPrintf("%s - %i\n", __func__, 2);

        mapCryptedMints[mintCrypted.GetValue().getuint256()] = mintCrypted;
//        LogPrintf("%s - %i\n", __func__, 3);

        CWalletDB(pwalletMain->strWalletFile).WriteZerocoinMint(mintCrypted);
//        LogPrintf("%s - %i\n", __func__, 4);
    }
    return true;
}

bool CCryptoKeyStore::AddZerocoinMint(const CZerocoinMint& mint)
{
    CWalletDB db(pwalletMain->strWalletFile);
    if (!IsCrypted()) {
        return db.WriteZerocoinMint(mint);
    } else {
        CZerocoinMint mintEncrypted;

        if(!mint.IsCrypted()) {
            if(!EncryptZerocoinMint(mint, mintEncrypted)) {
                return false;
            }
        } else {
            return false;
        }

        return AddCryptedZerocoinMint(mintEncrypted);
    }
}


bool CCryptoKeyStore::GetMintFromArchive(const CBigNum &bnPubcoinValue, CZerocoinMint& mintOut)
{
    CWalletDB db(pwalletMain->strWalletFile);

    CZerocoinMint mintTemp;

    if(!db.ReadArchivedMint(bnPubcoinValue, mintTemp)) {
        LogPrintf("%s : failed to find archived zerocoin mint\n", __func__);
        return false;
    }

    if(mintTemp.IsCrypted()) {
        if(DecryptZerocoinMint(mintTemp, mintOut)) {
            return true;
        } else {
            LogPrintf("%s : failed to decrypt archived zerocoin mint\n", __func__);
            return false;
        }
    } else {
        mintOut = CZerocoinMint(mintTemp);
        return true;
    }
}

bool CCryptoKeyStore::AddMintToArchive(const CZerocoinMint &mint)
{
    CZerocoinMint mintTemp;

    if(IsCrypted()) {
        if(!EncryptZerocoinMint(mint, mintTemp)) {
            LogPrintf("%s : failed to encrypt zerocoin mint to archive\n", __func__);
            return false;
        }
    } else {
        mintTemp = CZerocoinMint(mint);
    }

    return CWalletDB(pwalletMain->strWalletFile).ArchiveMintOrphan(mintTemp);
}

bool CCryptoKeyStore::GetZerocoinMint(const CBigNum &bnPubcoinValue, CZerocoinMint &mintOut, const bool fForUnarchive)
{
    CWalletDB db(pwalletMain->strWalletFile);

    if(IsLocked()) {
        return false;
    }

    CZerocoinMint mintTemp;

    if(!fForUnarchive) {
        if(!db.ReadZerocoinMint(bnPubcoinValue, mintTemp)) {
            return false;
        }
    } else {
        if(!db.UnarchiveZerocoin(bnPubcoinValue, mintTemp)) {
            return false;
        }
    }

    if(!IsCrypted() && !mintTemp.IsCrypted()) { // both should be decrypted
        mintOut = CZerocoinMint(mintTemp);
    } else if(IsCrypted() && mintTemp.IsCrypted()) { // both should be encrypted
        if(!DecryptZerocoinMint(mintTemp, mintOut)) {
            return false;
        }
    } else {
        LogPrintf("%s : ERROR: wallet in invalid state! Wallet in encrypted, but zerocoin mint is not.\n", __func__);
        assert(false);
    }

    return true;
}

bool CCryptoKeyStore::EncryptZerocoinMint(const CZerocoinMint& mintPlain, CZerocoinMint& mintEncrypted)
{
    map<ZerocoinSecrets, vector<unsigned char>> mapPlain;
    map<ZerocoinSecrets, vector<unsigned char>> mapSecrets;

//    int i = 0;
//    LogPrintf("%s - %i\n", __func__, i++);
    const uint256 key = mintPlain.GetValue().getuint256();
    const uint256 nIV = Hash(key.begin(), key.end());
//    LogPrintf("%s - %i\n", __func__, i++);

    mapPlain[SERIAL] = mintPlain.GetSerialNumber().getvch();
    mapPlain[RANDOM] = mintPlain.GetRandomness().getvch();
//    LogPrintf("%s - %i\n", __func__, i++);

    CKeyingMaterial kmSerialPlain(mapPlain[SERIAL].begin(), mapPlain[SERIAL].end());
    CKeyingMaterial kmRandomnessPlain(mapPlain[RANDOM].begin(), mapPlain[RANDOM].end());

    if(!EncryptSecret(vMasterKey, kmSerialPlain, nIV, mapSecrets[SERIAL]) || !EncryptSecret(vMasterKey, kmRandomnessPlain, nIV, mapSecrets[RANDOM])) {
        LogPrintf("Failed to encrypt mint with value:\n%s\n", mintPlain.GetValue());
        return false;
    }

//    LogPrintf("%s - %i\n", __func__, i++);

    mintEncrypted = CZerocoinMint(mintPlain);
    mintEncrypted.SetSerialNumber(CBigNum(mapSecrets[SERIAL]));
    mintEncrypted.SetRandomness(CBigNum(mapSecrets[RANDOM]));
    mintEncrypted.SetIsCrypted(true);
//    LogPrintf("%s - %i\n", __func__, i++);

    return true;
}

bool CCryptoKeyStore::DecryptZerocoinMint(const CZerocoinMint &mintEncrypted, CZerocoinMint &mintPlain)
{
    {
        LOCK(cs_KeyStore);

        const uint256 &key = mintEncrypted.GetValue().getuint256();

        if (mapCryptedMints.find(key) != mapCryptedMints.end()) {
            auto mint = mapCryptedMints.at(key);

            const uint256 nIV = Hash(key.begin(), key.end());
            map<ZerocoinSecrets, vector<unsigned char>> mapSecrets;
            map<ZerocoinSecrets, vector<unsigned char>> mapPlain;

            mapSecrets[SERIAL] = mint.GetSerialNumber().getvch();
            mapSecrets[RANDOM] = mint.GetRandomness().getvch();

            CKeyingMaterial kmSerial;
            CKeyingMaterial kmRandomness;

            DecryptSecret(vMasterKey, mapSecrets[SERIAL], nIV, kmSerial);
            DecryptSecret(vMasterKey, mapSecrets[RANDOM], nIV, kmRandomness);

            mapPlain[SERIAL] = vector<unsigned char>(kmSerial.begin(), kmSerial.end());
            mapPlain[RANDOM] = vector<unsigned char>(kmRandomness.begin(), kmRandomness.end());

            mintPlain = CZerocoinMint(mint);
            mintPlain.SetSerialNumber(CBigNum(mapPlain[SERIAL]));
            mintPlain.SetRandomness(CBigNum(mapPlain[RANDOM]));
            mintPlain.SetIsCrypted(false);

            return true;
        } else {
            LogPrintf("%s : failed to find encrypted mint:\n%s\n", __func__, mintEncrypted.GetValue());
            return false;
        }
    }
}


bool CCryptoKeyStore::EncryptZerocoinMints(CKeyingMaterial& vMasterKeyIn)
{
//    LogPrintf("%s - %i\n", __func__, i++);

//    LogPrintf("%s - %i\n", __func__, i++);

    list<CZerocoinMint> listMints = CWalletDB(pwalletMain->strWalletFile).ListMintedCoins(true, false, false);

//    LogPrintf("%s - %i\n", __func__, i++);

    // if no mints tracked consider success
    if(listMints.empty()) {
        return true;
    }
//    LogPrintf("%s - %i\n", __func__, i++);

    for (auto& mint : listMints) {
        if(!mint.IsCrypted()) { //encrypt any non encrypted mints
            if(!AddZerocoinMint(mint)) {
                return false;
            }
        }
    }

//    LogPrintf("%s - %i\n", __func__, i++);

    return true;
}

bool CCryptoKeyStore::RemoveZerocoinMint(const CZerocoinMint &mint)
{
//    LogPrintf("%s - %i\n", __func__, 0);

    if(IsCrypted()) { //delete reference in heap
//        LogPrintf("%s - %i\n", __func__, 1);
        mapCryptedMints.erase(mint.GetValue().getuint256());
    }
//    LogPrintf("%s - %i\n", __func__, 2);
    //delete reference from wallet.dat
    return CWalletDB(pwalletMain->strWalletFile).EraseZerocoinMint(mint);
}


