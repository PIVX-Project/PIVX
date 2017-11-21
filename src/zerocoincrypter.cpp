//
// Created by rejectedpromise on 11/25/17.
//

#include "zerocoincrypter.h"
#include <openssl/evp.h>

bool CZerocoinCrypter::SetKey(const std::vector<unsigned char> cipherKey, const std::vector<unsigned char>& chNewIV)
{
    if (cipherKey.size() != CZerocoinCrypter::KEY_SIZE || chNewIV.size() != CZerocoinCrypter::KEY_SIZE)
        return false;

    memcpy(&chKey[0], &cipherKey[0], sizeof chKey);
    memcpy(&chIV[0], &chNewIV[0], sizeof chIV);

    fKeySet = true;
    return true;
}

bool CZerocoinCrypter::Encrypt(CBigNum& bnPlaintext)
{
    if (!fKeySet)
        return false;

    std::vector<unsigned char> vchPlaintext = bnPlaintext.getvch();

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    std::vector<unsigned char> vchCiphertext(nCLen);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_EncryptUpdate(&ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen) != 0;
    if (fOk) fOk = EVP_EncryptFinal_ex(&ctx, (&vchCiphertext[0]) + nCLen, &nFLen) != 0;
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    vchCiphertext.resize(nCLen + nFLen);
    bnPlaintext.setvch(vchCiphertext);

    return true;
}

bool CZerocoinCrypter::Decrypt(CBigNum& bnCiphertext)
{
    if (!fKeySet)
        return false;
    std::vector<unsigned char> vchCiphertext = bnCiphertext.getvch();

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    std::vector<unsigned char> vchPlaintext(nPLen);

    EVP_CIPHER_CTX ctx;

    bool fOk = true;

    EVP_CIPHER_CTX_init(&ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_DecryptUpdate(&ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen) != 0;
    if (fOk) fOk = EVP_DecryptFinal_ex(&ctx, (&vchPlaintext[0]) + nPLen, &nFLen) != 0;
    EVP_CIPHER_CTX_cleanup(&ctx);

    if (!fOk) return false;

    vchPlaintext.resize(nPLen + nFLen);
    bnCiphertext.setvch(vchPlaintext);

    return true;
}