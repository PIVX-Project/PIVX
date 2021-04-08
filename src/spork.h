// Copyright (c) 2014-2016 The Dash developers
//Copyright (c) 2016-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPORK_H
#define SPORK_H

#include "base58.h"
#include "hash.h"
#include "key.h"
#include "main.h"
#include "messagesigner.h"
#include "net.h"
#include "sporkid.h"
#include "sync.h"
#include "util.h"

#include "protocol.h"


class CSporkMessage;
class CSporkManager;

extern std::vector<CSporkDef> sporkDefs;
extern std::map<uint256, CSporkMessage> mapSporks;
extern CSporkManager sporkManager;

//
// Spork Classes
// Keep track of all of the network spork settings
//

class CSporkMessage : public CSignedMessage
{
public:
    SporkId nSporkID;
    int64_t nValue;
    int64_t nTimeSigned;

    CSporkMessage() :
        CSignedMessage(),
        nSporkID((SporkId)0),
        nValue(0),
        nTimeSigned(0)
    {}

    CSporkMessage(SporkId nSporkID, int64_t nValue, int64_t nTimeSigned) :
        CSignedMessage(),
        nSporkID(nSporkID),
        nValue(nValue),
        nTimeSigned(nTimeSigned)
    { }

    uint256 GetHash() const { return HashQuark(BEGIN(nSporkID), END(nTimeSigned)); }

    // override CSignedMessage functions
    uint256 GetSignatureHash() const override;
    std::string GetStrMessage() const override;
    const CTxIn GetVin() const override { return CTxIn(); };

    // override GetPublicKey - gets Params().SporkPubkey()
    const CPubKey GetPublicKey(std::string& strErrorRet) const override;
    const CPubKey GetPublicKeyOld() const;

    void Relay();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nSporkID);
        READWRITE(nValue);
        READWRITE(nTimeSigned);
        READWRITE(vchSig);
        try
        {
            READWRITE(nMessVersion);
        } catch (...) {
            nMessVersion = MessageVersion::MESS_VER_STRMESS;
        }
    }
};


class CSporkManager
{
private:
    mutable RecursiveMutex cs;
    std::string strMasterPrivKey;
    std::map<SporkId, CSporkDef*> sporkDefsById;
    std::map<std::string, CSporkDef*> sporkDefsByName;
    std::map<SporkId, CSporkMessage> mapSporksActive;

public:
    CSporkManager();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(mapSporksActive);
        // we don't serialize private key to prevent its leakage
    }

    void Clear();
    void LoadSporksFromDB();

    void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    int64_t GetSporkValue(SporkId nSporkID);
    void ExecuteSpork(SporkId nSporkID, int nValue);
    bool UpdateSpork(SporkId nSporkID, int64_t nValue);

    bool IsSporkActive(SporkId nSporkID);
    std::string GetSporkNameByID(SporkId id);
    SporkId GetSporkIDByName(std::string strName);

    bool SetPrivKey(std::string strPrivKey);
    std::string ToString() const;
};

#endif
