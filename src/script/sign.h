// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_SCRIPT_SIGN_H
#define PIVX_SCRIPT_SIGN_H

#include "script/interpreter.h"

class CKey;
class CKeyID;
class CScript;
class CScriptID;
class CTransaction;

struct CMutableTransaction;

/** An interface to be implemented by providers that support signing. */
class SigningProvider
{
public:
    virtual ~SigningProvider() {}
    virtual bool GetCScript(const CScriptID &scriptid, CScript& script) const { return false; }
    virtual bool GetPubKey(const CKeyID &address, CPubKey& pubkey) const { return false; }
    virtual bool GetKey(const CKeyID &address, CKey& key) const { return false; }
};

class PublicOnlySigningProvider : public SigningProvider
{
private:
    const SigningProvider* m_provider;

public:
    PublicOnlySigningProvider(const SigningProvider* provider) : m_provider(provider) {}
    bool GetCScript(const CScriptID &scriptid, CScript& script) const;
    bool GetPubKey(const CKeyID &address, CPubKey& pubkey) const;
};

struct FlatSigningProvider final : public SigningProvider
{
    std::map<CScriptID, CScript> scripts;
    std::map<CKeyID, CPubKey> pubkeys;
    std::map<CKeyID, CKey> keys;

    bool GetCScript(const CScriptID& scriptid, CScript& script) const override;
    bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const override;
    bool GetKey(const CKeyID& keyid, CKey& key) const override;
};

FlatSigningProvider Merge(const FlatSigningProvider& a, const FlatSigningProvider& b);

/** Virtual base class for signature creators. */
class BaseSignatureCreator {

protected:
    const SigningProvider* m_provider;

public:
    virtual ~BaseSignatureCreator() {}
    virtual const BaseSignatureChecker& Checker() const =0;

    /** Create a singular (non-script) signature. */
    virtual bool CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const =0;
};

/** A signature creator for transactions. */
class TransactionSignatureCreator : public BaseSignatureCreator {
    const CTransaction* txTo;
    unsigned int nIn;
    int nHashType;
    CAmount amount;
    const TransactionSignatureChecker checker;

public:
    TransactionSignatureCreator(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, int nHashTypeIn=SIGHASH_ALL);
    const BaseSignatureChecker& Checker() const override { return checker; }
    bool CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const override;
};

class MutableTransactionSignatureCreator : public TransactionSignatureCreator {
    CTransaction tx;

public:
    MutableTransactionSignatureCreator(const CMutableTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, int nHashTypeIn) : TransactionSignatureCreator(&tx, nInIn, amountIn, nHashTypeIn), tx(*txToIn) {}
};

/** A signature creator that just produces 72-byte empty signatyres. */
extern const BaseSignatureCreator& DUMMY_SIGNATURE_CREATOR;

struct SignatureData {
    CScript scriptSig;

    SignatureData() {}
    explicit SignatureData(const CScript& script) : scriptSig(script) {}
};

/** Produce a script signature using a generic signature creator. */
bool ProduceSignature(const SigningProvider& provider, const BaseSignatureCreator& creator, const CScript& fromPubKey, SignatureData& sigdata, SigVersion sigversion, bool fColdStake, ScriptError* serror = nullptr);

/** Produce a script signature for a transaction. */
bool SignSignature(const SigningProvider &provider, const CScript& fromPubKey, CMutableTransaction& txTo, unsigned int nIn, const CAmount& amount, int nHashType, bool fColdStake = false);
bool SignSignature(const SigningProvider &provider, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, int nHashType, bool fColdStake = false);

/** Combine two script signatures using a generic signature checker, intelligently, possibly with OP_0 placeholders. */
SignatureData CombineSignatures(const CScript& scriptPubKey, const BaseSignatureChecker& checker, const SignatureData& scriptSig1, const SignatureData& scriptSig2);

/** Extract signature data from a transaction, and insert it. */
SignatureData DataFromTransaction(const CMutableTransaction& tx, unsigned int nIn);
void UpdateTransaction(CMutableTransaction& tx, unsigned int nIn, const SignatureData& data);

/* Check whether we know how to sign for an output like this, assuming we
  * have all private keys. While this function does not need private keys, the passed
  * provider is used to look up public keys and redeemscripts by hash.
  * Solvability is unrelated to whether we consider this output to be ours. */
bool IsSolvable(const SigningProvider& provider, const CScript& script, bool fColdStaking);

#endif // PIVX_SCRIPT_SIGN_H
