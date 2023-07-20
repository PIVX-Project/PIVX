// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021 The hemis Core developers
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_PROVIDERTX_H
#define hemis_PROVIDERTX_H

#include "bls/bls_wrapper.h"
#include "primitives/transaction.h"
#include "netaddress.h"

#include <univalue.h>

// Provider-Register tx payload

class ProRegPL
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION};                         // message version
    uint16_t nType{0};                                          // only 0 supported for now
    uint16_t nMode{0};                                          // only 0 supported for now
    COutPoint collateralOutpoint{UINT256_ZERO, (uint32_t)-1};   // if hash is null, we refer to a ProRegTx output
    CService addr;
    CKeyID keyIDOwner;
    CBLSPublicKey pubKeyOperator;
    CKeyID keyIDVoting;
    CScript scriptPayout;
    uint16_t nOperatorReward{0};
    CScript scriptOperatorPayout;
    uint256 inputsHash; // replay protection
    std::vector<unsigned char> vchSig;

public:

    SERIALIZE_METHODS(ProRegPL, obj)
    {
        READWRITE(obj.nVersion);
        READWRITE(obj.nType);
        READWRITE(obj.nMode);
        READWRITE(obj.collateralOutpoint);
        READWRITE(obj.addr);
        READWRITE(obj.keyIDOwner);
        READWRITE(obj.pubKeyOperator);
        READWRITE(obj.keyIDVoting);
        READWRITE(obj.scriptPayout);
        READWRITE(obj.nOperatorReward);
        READWRITE(obj.scriptOperatorPayout);
        READWRITE(obj.inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.vchSig);
        }
    }

    // When signing with the collateral key, we don't sign the hash but a generated message instead
    // This is needed for HW wallet support which can only sign text messages as of now
    std::string MakeSignString() const;

    std::string ToString() const;
    void ToJson(UniValue& obj) const;
};

// Provider-Update-Service tx payload

class ProUpServPL
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION}; // message version
    uint256 proTxHash{UINT256_ZERO};
    CService addr;
    CScript scriptOperatorPayout;
    uint256 inputsHash; // replay protection
    CBLSSignature sig;

public:
    SERIALIZE_METHODS(ProUpServPL, obj)
    {
        READWRITE(obj.nVersion, obj.proTxHash, obj.addr, obj.scriptOperatorPayout, obj.inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.sig);
        }
    }

public:
    std::string ToString() const;
    void ToJson(UniValue& obj) const;
};

// Provider-Update-Registrar tx payload
class ProUpRegPL
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION}; // message version
    uint256 proTxHash;
    uint16_t nMode{0}; // only 0 supported for now
    CBLSPublicKey pubKeyOperator;
    CKeyID keyIDVoting;
    CScript scriptPayout;
    uint256 inputsHash; // replay protection
    std::vector<unsigned char> vchSig;

public:
    SERIALIZE_METHODS(ProUpRegPL, obj)
    {
        READWRITE(obj.nVersion, obj.proTxHash, obj.nMode, obj.pubKeyOperator, obj.keyIDVoting, obj.scriptPayout, obj.inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.vchSig);
        }
    }

public:
    std::string ToString() const;
    void ToJson(UniValue& obj) const;
};

// Provider-Update-Revoke tx payload
class ProUpRevPL
{
public:
    static const uint16_t CURRENT_VERSION = 1;

    // these are just informational and do not have any effect on the revocation
    enum RevocationReason {
        REASON_NOT_SPECIFIED = 0,
        REASON_TERMINATION_OF_SERVICE = 1,
        REASON_COMPROMISED_KEYS = 2,
        REASON_CHANGE_OF_KEYS = 3,
        REASON_LAST = REASON_CHANGE_OF_KEYS
    };

public:
    uint16_t nVersion{CURRENT_VERSION}; // message version
    uint256 proTxHash;
    uint16_t nReason{REASON_NOT_SPECIFIED};
    uint256 inputsHash; // replay protection
    CBLSSignature sig;

public:
    SERIALIZE_METHODS(ProUpRevPL, obj)
    {
        READWRITE(obj.nVersion, obj.proTxHash, obj.nReason, obj.inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.sig);
        }
    }

public:
    std::string ToString() const;
    void ToJson(UniValue& obj) const;
};


// If tx is a ProRegTx, return the collateral outpoint in outRet.
bool GetProRegCollateral(const CTransactionRef& tx, COutPoint& outRet);

#endif  //hemis_PROVIDERTX_H
