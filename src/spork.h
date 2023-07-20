// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPORK_H
#define SPORK_H

#include "hash.h"
#include "key.h"
#include "key_io.h"
#include "messagesigner.h"
#include "net.h"
#include "sporkid.h"
#include "sync.h"
#include "util/system.h"

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

    // - gets Params().SporkPubkey()
    const CPubKey GetPublicKey() const;
    const CPubKey GetPublicKeyOld() const;

    void Relay();

    SERIALIZE_METHODS(CSporkMessage, obj) { READWRITE(obj.nSporkID, obj.nValue, obj.nTimeSigned, obj.vchSig, obj.nMessVersion); }
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

    SERIALIZE_METHODS(CSporkManager, obj) { READWRITE(obj.mapSporksActive); }

    void Clear();
    void LoadSporksFromDB();

    bool ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv, int& dosScore);
    int64_t GetSporkValue(SporkId nSporkID);
    // Create/Sign/Relay the spork message, and update the maps
    bool UpdateSpork(SporkId nSporkID, int64_t nValue);
    // Add spork message to mapSporks and mapSporksActive.
    // if flush=true, save to DB as well
    void AddOrUpdateSporkMessage(const CSporkMessage& spork, bool flush = false);

    bool IsSporkActive(SporkId nSporkID);
    std::string GetSporkNameByID(SporkId id);
    SporkId GetSporkIDByName(std::string strName);

    bool SetPrivKey(std::string strPrivKey);
    std::string ToString() const;

    // Process SPORK message, returning the banning score (or 0 if no banning is needed)
    int ProcessSporkMsg(CDataStream& vRecv);
    int ProcessSporkMsg(CSporkMessage& spork);
    // Process GETSPORKS message
    void ProcessGetSporks(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
};

#endif
