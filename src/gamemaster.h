// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GAMEMASTER_H
#define GAMEMASTER_H

#include "key_io.h"
#include "key.h"
#include "messagesigner.h"
#include "net.h"
#include "primitives/transaction.h"
#include "serialize.h"
#include "sync.h"
#include "timedata.h"
#include "util/system.h"

/* Depth of the block pinged by gamemasters */
static const unsigned int GMPING_DEPTH = 12;

class CGamemaster;
class CGamemasterBroadcast;
class CGamemasterPing;

typedef std::shared_ptr<CGamemaster> GamemasterRef;

class CDeterministicGM;
typedef std::shared_ptr<const CDeterministicGM> CDeterministicGMCPtr;

int GamemasterMinPingSeconds();
int GamemasterBroadcastSeconds();
int GamemasterPingSeconds();
int GamemasterExpirationSeconds();
int GamemasterRemovalSeconds();

//
// The Gamemaster Ping Class : Contains a different serialize method for sending pings from gamemasters throughout the network
//

class CGamemasterPing : public CSignedMessage
{
public:
    CTxIn vin;
    uint256 blockHash;
    int64_t sigTime; //gmb message times

    CGamemasterPing();
    CGamemasterPing(const CTxIn& newVin, const uint256& nBlockHash, uint64_t _sigTime);

    SERIALIZE_METHODS(CGamemasterPing, obj) { READWRITE(obj.vin, obj.blockHash, obj.sigTime, obj.vchSig, obj.nMessVersion); }

    uint256 GetHash() const;

    // override CSignedMessage functions
    uint256 GetSignatureHash() const override { return GetHash(); }
    std::string GetStrMessage() const override;
    const CTxIn GetVin() const { return vin; };
    bool IsNull() const { return blockHash.IsNull() || vin.prevout.IsNull(); }

    bool CheckAndUpdate(int& nDos, bool fRequireAvailable = true, bool fCheckSigTimeOnly = false);
    void Relay();

    CGamemasterPing& operator=(const CGamemasterPing& other) = default;

    friend bool operator==(const CGamemasterPing& a, const CGamemasterPing& b)
    {
        return a.vin == b.vin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CGamemasterPing& a, const CGamemasterPing& b)
    {
        return !(a == b);
    }
};

//
// The Gamemaster Class. It contains the input of the 10000 HMS, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CGamemaster : public CSignedMessage
{
private:
    // critical section to protect the inner data structures
    mutable RecursiveMutex cs;
    bool fCollateralSpent{false};

public:
    enum state {
        GAMEMASTER_PRE_ENABLED,
        GAMEMASTER_ENABLED,
        GAMEMASTER_EXPIRED,
        GAMEMASTER_REMOVE,
        GAMEMASTER_VIN_SPENT,
    };

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyGamemaster;
    int64_t sigTime; //gmb message time
    int protocolVersion;
    int nScanningErrorCount;
    int nLastScanningErrorBlockHeight;
    CGamemasterPing lastPing;

    explicit CGamemaster();
    CGamemaster(const CGamemaster& other);

    // Initialize from DGM. Used by the compatibility code.
    CGamemaster(const CDeterministicGMCPtr& dgm, int64_t registeredTime, const uint256& registeredHash);

    // override CSignedMessage functions
    uint256 GetSignatureHash() const override;
    std::string GetStrMessage() const override;
    const CTxIn GetVin() const { return vin; };
    CPubKey GetPubKey() const { return pubKeyGamemaster; }

    void SetLastPing(const CGamemasterPing& _lastPing) { WITH_LOCK(cs, lastPing = _lastPing;); }

    CGamemaster& operator=(const CGamemaster& other)
    {
        nMessVersion = other.nMessVersion;
        vchSig = other.vchSig;
        vin = other.vin;
        addr = other.addr;
        pubKeyCollateralAddress = other.pubKeyCollateralAddress;
        pubKeyGamemaster = other.pubKeyGamemaster;
        sigTime = other.sigTime;
        lastPing = other.lastPing;
        protocolVersion = other.protocolVersion;
        nScanningErrorCount = other.nScanningErrorCount;
        nLastScanningErrorBlockHeight = other.nLastScanningErrorBlockHeight;
        return *this;
    }

    friend bool operator==(const CGamemaster& a, const CGamemaster& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CGamemaster& a, const CGamemaster& b)
    {
        return !(a.vin == b.vin);
    }

    arith_uint256 CalculateScore(const uint256& hash) const;

    SERIALIZE_METHODS(CGamemaster, obj)
    {
        LOCK(obj.cs);
        READWRITE(obj.vin, obj.addr, obj.pubKeyCollateralAddress);
        READWRITE(obj.pubKeyGamemaster, obj.vchSig, obj.sigTime, obj.protocolVersion);
        READWRITE(obj.lastPing, obj.nScanningErrorCount, obj.nLastScanningErrorBlockHeight);

        if (obj.protocolVersion == MIN_BIP155_PROTOCOL_VERSION) {
            bool dummyIsBIP155Addr = false;
            READWRITE(dummyIsBIP155Addr);
        }
    }

    template <typename Stream>
    CGamemaster(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    bool UpdateFromNewBroadcast(CGamemasterBroadcast& gmb);

    CGamemaster::state GetActiveState() const;

    bool IsBroadcastedWithin(int seconds)
    {
        return (GetAdjustedTime() - sigTime) < seconds;
    }

    bool IsPingedWithin(int seconds, int64_t now = -1) const
    {
        now == -1 ? now = GetAdjustedTime() : now;
        return lastPing.IsNull() ? false : now - lastPing.sigTime < seconds;
    }

    void SetSpent()
    {
        LOCK(cs);
        fCollateralSpent = true;
    }

    void Disable()
    {
        LOCK(cs);
        sigTime = 0;
        lastPing = CGamemasterPing();
    }

    bool IsEnabled() const
    {
        return GetActiveState() == GAMEMASTER_ENABLED;
    }

    bool IsPreEnabled() const
    {
        return GetActiveState() == GAMEMASTER_PRE_ENABLED;
    }

    bool IsAvailableState() const
    {
        state s = GetActiveState();
        return s == GAMEMASTER_ENABLED || s == GAMEMASTER_PRE_ENABLED;
    }

    std::string Status() const
    {
        auto activeState = GetActiveState();
        if (activeState == CGamemaster::GAMEMASTER_PRE_ENABLED) return "PRE_ENABLED";
        if (activeState == CGamemaster::GAMEMASTER_ENABLED)     return "ENABLED";
        if (activeState == CGamemaster::GAMEMASTER_EXPIRED)     return "EXPIRED";
        if (activeState == CGamemaster::GAMEMASTER_VIN_SPENT)   return "VIN_SPENT";
        if (activeState == CGamemaster::GAMEMASTER_REMOVE)      return "REMOVE";
        return strprintf("INVALID_%d", activeState);
    }

    bool IsValidNetAddr() const;

    /*
     * This is used only by the compatibility code for DGM, which don't share the public key (but the keyid).
     * Used by the payment-logic to include the necessary information in a temporary GamemasterRef object
     * (which is not indexed in the maps of the legacy manager).
     * A non-empty gmPayeeScript identifies this object as a "deterministic" gamemaster.
     * Note: this is the single payout for the gamemaster (if the dgm is configured to pay a portion of the reward
     * to the operator, this is done only after the disabling of the legacy system).
     */
    CScript gmPayeeScript{};
    CScript GetPayeeScript() const {
        return gmPayeeScript.empty() ? GetScriptForDestination(pubKeyCollateralAddress.GetID())
                                     : gmPayeeScript;
    }
};


//
// The Gamemaster Broadcast Class : Contains a different serialize method for sending gamemasters through the network
//

class CGamemasterBroadcast : public CGamemaster
{
public:
    CGamemasterBroadcast();
    CGamemasterBroadcast(CService newAddr, CTxIn newVin, CPubKey newPubkey, CPubKey newPubkey2, int protocolVersionIn, const CGamemasterPing& _lastPing);
    CGamemasterBroadcast(const CGamemaster& gm);

    bool CheckAndUpdate(int& nDoS);

    uint256 GetHash() const;

    void Relay();

    // special sign/verify
    bool Sign(const CKey& key, const CPubKey& pubKey);
    bool CheckSignature() const;

    SERIALIZE_METHODS(CGamemasterBroadcast, obj)
    {
        READWRITE(obj.vin);
        READWRITE(obj.addr);
        READWRITE(obj.pubKeyCollateralAddress);
        READWRITE(obj.pubKeyGamemaster);
        READWRITE(obj.vchSig);
        READWRITE(obj.sigTime);
        READWRITE(obj.protocolVersion);
        READWRITE(obj.lastPing);
        READWRITE(obj.nMessVersion);
    }

    /// Create Gamemaster broadcast, needs to be relayed manually after that
    static bool Create(const CTxIn& vin, const CService& service, const CKey& keyCollateralAddressNew, const CPubKey& pubKeyCollateralAddressNew, const CKey& keyGamemasterNew, const CPubKey& pubKeyGamemasterNew, std::string& strErrorRet, CGamemasterBroadcast& gmbRet);
    static bool Create(const std::string& strService, const std::string& strKey, const std::string& strTxHash, const std::string& strOutputIndex, std::string& strErrorRet, CGamemasterBroadcast& gmbRet, bool fOffline, int chainHeight);
    static bool CheckDefaultPort(CService service, std::string& strErrorRet, const std::string& strContext);
};

// Temporary function used for payment compatibility code.
// Returns a shared pointer to a gamemaster object initialized from a DGM.
GamemasterRef MakeGamemasterRefForDGM(const CDeterministicGMCPtr& dgm);

#endif
