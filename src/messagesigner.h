// Copyright (c) 2014-2018 The Dash Core developers
//Copyright (c) 2018-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MESSAGESIGNER_H
#define MESSAGESIGNER_H

#include "key.h"
#include "primitives/transaction.h" // for CTxIn

enum MessageVersion {
        MESS_VER_STRMESS    = 0,
        MESS_VER_HASH       = 1,
};

/** Helper class for signing messages and checking their signatures
 */
class CMessageSigner
{
public:
    /// Set the private/public key values, returns true if successful
    static bool GetKeysFromSecret(const std::string& strSecret, CKey& keyRet, CPubKey& pubkeyRet);
    /// Get the hash based on the input message
    static uint256 GetMessageHash(const std::string& strMessage);
    /// Sign the message, returns true if successful
    static bool SignMessage(const std::string& strMessage, std::vector<unsigned char>& vchSigRet, const CKey& key);
    /// Verify the message signature, returns true if successful
    static bool VerifyMessage(const CPubKey& pubkey, const std::vector<unsigned char>& vchSig, const std::string& strMessage, std::string& strErrorRet);
    /// Verify the message signature, returns true if successful
    static bool VerifyMessage(const CKeyID& keyID, const std::vector<unsigned char>& vchSig, const std::string& strMessage, std::string& strErrorRet);
};

/** Helper class for signing hashes and checking their signatures
 */
class CHashSigner
{
public:
    /// Sign the hash, returns true if successful
    static bool SignHash(const uint256& hash, const CKey& key, std::vector<unsigned char>& vchSigRet);
    /// Verify the hash signature, returns true if successful
    static bool VerifyHash(const uint256& hash, const CPubKey& pubkey, const std::vector<unsigned char>& vchSig, std::string& strErrorRet);
    /// Verify the hash signature, returns true if successful
    static bool VerifyHash(const uint256& hash, const CKeyID& keyID, const std::vector<unsigned char>& vchSig, std::string& strErrorRet);
};

/** Base Class for all signed messages on the network
 */
class CSignedMessage
{
protected:
    std::vector<unsigned char> vchSig;
    void swap(CSignedMessage& first, CSignedMessage& second); // Swap two messages

public:
    int nMessVersion;

    CSignedMessage() :
        vchSig(),
        nMessVersion(MessageVersion::MESS_VER_HASH)
    {}
    CSignedMessage(const CSignedMessage& other)
    {
        vchSig = other.GetVchSig();
        nMessVersion = other.nMessVersion;
    }
    virtual ~CSignedMessage() {};

    // Sign-Verify message
    bool Sign(const CKey& key, const CPubKey& pubKey, const bool fNewSigs);
    bool Sign(const std::string strSignKey, const bool fNewSigs);
    bool CheckSignature(const CPubKey& pubKey) const;
    bool CheckSignature() const;

    // Pure virtual functions (used in Sign-Verify functions)
    // Must be implemented in child classes
    virtual uint256 GetSignatureHash() const = 0;
    virtual std::string GetStrMessage() const = 0;
    virtual const CTxIn GetVin() const = 0;

    // GetPublicKey defaults to public key of masternode with vin from GetVin.
    // Child classes can override if public key is directly accessible.
    virtual const CPubKey GetPublicKey(std::string& strErrorRet) const;

    // Setters and getters
    void SetVchSig(const std::vector<unsigned char>& vchSigIn) { vchSig = vchSigIn; }
    std::vector<unsigned char> GetVchSig() const { return vchSig; }
    std::string GetSignatureBase64() const;
};

#endif
