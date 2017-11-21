//
// Created by rejectedpromise on 11/25/17.
//

#ifndef PIVX_ZEROCOINCRYPTER_H
#define PIVX_ZEROCOINCRYPTER_H

#include "allocators.h"
#include "pubkey.h"
#include "libzerocoin/bignum.h"
#include <openssl/aes.h>
#include <string>

#define ZERO_KEY_SIZE 32

class CZerocoinCrypter
{
private:
    unsigned char chKey[ZERO_KEY_SIZE];
    unsigned char chIV[ZERO_KEY_SIZE];

    bool fKeySet;
public:
    CPubKey cipherPubKey;
    bool Encrypt(CBigNum& bnPlaintext);
    bool Decrypt(CBigNum& bnCiphertext);
    bool SetKey(const std::vector<unsigned char> cipherKey, const std::vector<unsigned char>& chNewIV);
    bool HasKey() const { return this->fKeySet; }
    CKeyID GetKeyID() const { return this->cipherPubKey.GetID(); }
    const unsigned int KEY_SIZE = ZERO_KEY_SIZE;

    void CleanKey()
    {
        OPENSSL_cleanse(chKey, sizeof(chKey));
        OPENSSL_cleanse(chIV, sizeof(chIV));
        fKeySet = false;
    }

    CZerocoinCrypter()
    {
        fKeySet = false;

        // Try to keep the key data out of swap (and be a bit over-careful to keep the IV that we don't even use out of swap)
        // Note that this does nothing about suspend-to-disk (which will put all our key data on disk)
        // Note as well that at no point in this program is any attempt made to prevent stealing of keys by reading the memory of the running process.
        LockedPageManager::Instance().LockRange(&chKey[0], sizeof chKey);
        LockedPageManager::Instance().LockRange(&chIV[0], sizeof chIV);
    }

    ~CZerocoinCrypter()
    {
        CleanKey();

        LockedPageManager::Instance().UnlockRange(&chKey[0], sizeof chKey);
        LockedPageManager::Instance().UnlockRange(&chIV[0], sizeof chIV);
    }

};


#endif //PIVX_ZEROCOINCRYPTER_H
