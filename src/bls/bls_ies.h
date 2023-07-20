// Copyright (c) 2018 The Dash Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_CRYPTO_BLS_IES_H
#define hemis_CRYPTO_BLS_IES_H

#include "bls/bls_wrapper.h"
#include "streams.h"

// Base class to handle encryption/decryption of a binary object.
// No padding: the size of the object must be a multiple of AES_BLOCKSIZE (16)
class CBLSIESEncryptedBlob
{
public:
    CBLSPublicKey ephemeralPubKey;
    unsigned char iv[16];
    std::vector<unsigned char> data;
    bool valid{false};

    bool Encrypt(const CBLSPublicKey& peerPubKey, const void* data, size_t dataSize);
    bool Decrypt(const CBLSSecretKey& secretKey, CDataStream& decryptedDataRet) const;

    SERIALIZE_METHODS(CBLSIESEncryptedBlob, obj)
    {
        SER_WRITE(obj, assert(obj.valid));

        READWRITE(obj.ephemeralPubKey);
        READWRITE(obj.iv);
        READWRITE(obj.data);

        SER_READ(obj, obj.valid = true);
    }
};

// Encryption/Decryption of an object of type Object
// (serialized size of Object must be multiple of AES_BLOCKSIZE)
template <typename Object>
class CBLSIESEncryptedObject : public CBLSIESEncryptedBlob
{
public:
    CBLSIESEncryptedObject()
    {
    }

    bool Encrypt(const CBLSPublicKey& peerPubKey, const Object& obj, int nVersion)
    {
        try {
            CDataStream ds(SER_NETWORK, nVersion);
            ds << obj;
            return CBLSIESEncryptedBlob::Encrypt(peerPubKey, ds.data(), ds.size());
        } catch (std::exception&) {
            return false;
        }
    }

    bool Decrypt(const CBLSSecretKey& secretKey, Object& objRet, int nVersion) const
    {
        CDataStream ds(SER_NETWORK, nVersion);
        if (!CBLSIESEncryptedBlob::Decrypt(secretKey, ds)) {
            return false;
        }
        try {
            ds >> objRet;
        } catch (std::exception& e) {
            return false;
        }
        return true;
    }
};

// Base class to handle encryption/decryption of a vector of binary objects.
// No padding: the size of each object must be a multiple of AES_BLOCKSIZE (16)
class CBLSIESMultiRecipientBlobs
{
public:
    typedef std::vector<unsigned char> Blob;
    typedef std::vector<Blob> BlobVector;

public:
    CBLSPublicKey ephemeralPubKey;
    uint256 ivSeed;
    BlobVector blobs;

    // Used while encrypting. Temporary and only in-memory
    CBLSSecretKey ephemeralSecretKey;
    std::vector<uint256> ivVector;

public:
    bool Encrypt(const std::vector<CBLSPublicKey>& recipients, const BlobVector& _blobs);

    void InitEncrypt(size_t count);
    bool Encrypt(size_t idx, const CBLSPublicKey& recipient, const Blob& blob);
    bool Decrypt(size_t idx, const CBLSSecretKey& sk, Blob& blobRet) const;

    SERIALIZE_METHODS(CBLSIESMultiRecipientBlobs, obj)
    {
        READWRITE(obj.ephemeralPubKey);
        READWRITE(obj.ivSeed);
        READWRITE(obj.blobs);
    }
};

// Encryption/Decryption of a vector of objects of type Object
// (the serialized size of Object must be multiple of AES_BLOCKSIZE)
template <typename Object>
class CBLSIESMultiRecipientObjects : public CBLSIESMultiRecipientBlobs
{
public:
    typedef std::vector<Object> ObjectVector;

public:
    bool Encrypt(const std::vector<CBLSPublicKey>& recipients, const ObjectVector& _objects, int nVersion)
    {
        BlobVector blobs;
        blobs.resize(_objects.size());

        try {
            CDataStream ds(SER_NETWORK, nVersion);
            for (size_t i = 0; i < _objects.size(); i++) {
                ds.clear();

                ds << _objects[i];
                blobs[i].assign(ds.begin(), ds.end());
            }
        } catch (std::exception&) {
            return false;
        }

        return CBLSIESMultiRecipientBlobs::Encrypt(recipients, blobs);
    }

    bool Encrypt(size_t idx, const CBLSPublicKey& recipient, const Object& obj, int nVersion)
    {
        CDataStream ds(SER_NETWORK, nVersion);
        ds << obj;
        Blob blob(ds.begin(), ds.end());
        return CBLSIESMultiRecipientBlobs::Encrypt(idx, recipient, blob);
    }

    bool Decrypt(size_t idx, const CBLSSecretKey& sk, Object& objectRet, int nVersion) const
    {
        Blob blob;
        if (!CBLSIESMultiRecipientBlobs::Decrypt(idx, sk, blob)) {
            return false;
        }

        try {
            CDataStream ds(blob, SER_NETWORK, nVersion);
            ds >> objectRet;
            return true;
        } catch (std::exception&) {
            return false;
        }
    }
};

#endif // hemis_CRYPTO_BLS_IES_H
