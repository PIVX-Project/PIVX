// Copyright (c) 2014-2021 The Dash Core developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVEGAMEMASTER_H
#define ACTIVEGAMEMASTER_H

#include "key.h"
#include "evo/deterministicgms.h"
#include "operationresult.h"
#include "sync.h"
#include "validationinterface.h"

class CActiveDeterministicGamemasterManager;
class CBLSPublicKey;
class CBLSSecretKey;

#define ACTIVE_GAMEMASTER_INITIAL 0 // initial state
#define ACTIVE_GAMEMASTER_SYNC_IN_PROCESS 1
#define ACTIVE_GAMEMASTER_NOT_CAPABLE 3
#define ACTIVE_GAMEMASTER_STARTED 4

extern CActiveDeterministicGamemasterManager* activeGamemasterManager;

struct CActiveGamemasterInfo
{
    // Keys for the active Gamemaster
    CBLSPublicKey pubKeyOperator;
    CBLSSecretKey keyOperator;
    // Initialized while registering Gamemaster
    uint256 proTxHash{UINT256_ZERO};
    CService service;
};

class CActiveDeterministicGamemasterManager : public CValidationInterface
{
public:
    enum gamemaster_state_t {
        GAMEMASTER_WAITING_FOR_PROTX,
        GAMEMASTER_POSE_BANNED,
        GAMEMASTER_REMOVED,
        GAMEMASTER_OPERATOR_KEY_CHANGED,
        GAMEMASTER_PROTX_IP_CHANGED,
        GAMEMASTER_READY,
        GAMEMASTER_ERROR,
    };

private:
    gamemaster_state_t state{GAMEMASTER_WAITING_FOR_PROTX};
    std::string strError;
    CActiveGamemasterInfo info;

public:
    ~CActiveDeterministicGamemasterManager() override = default;
    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) override;

    void Init(const CBlockIndex* pindexTip);
    void Reset(gamemaster_state_t _state, const CBlockIndex* pindexTip);
    // Sets the Deterministic Gamemaster Operator's private/public key
    OperationResult SetOperatorKey(const std::string& strGMOperatorPrivKey);
    // If the active gamemaster is ready, and the keyID matches with the registered one,
    // return private key, keyID, and pointer to dgm.
    OperationResult GetOperatorKey(CBLSSecretKey& key, CDeterministicGMCPtr& dgm) const;
    // Directly return the operator secret key saved in the manager, without performing any validation
    const CBLSSecretKey* OperatorKey() const { return &info.keyOperator; }
    void SetNullProTx() { info.proTxHash = UINT256_ZERO; }
    const uint256 GetProTx() const { return info.proTxHash; }

    const CActiveGamemasterInfo* GetInfo() const { return &info; }
    gamemaster_state_t GetState() const { return state; }
    std::string GetStatus() const;
    bool IsReady() const { return state == GAMEMASTER_READY; }

    static bool IsValidNetAddr(const CService& addrIn);
};

// Responsible for initializing the gamemaster
OperationResult initGamemaster(const std::string& strMasterNodePrivKey, const std::string& strMasterNodeAddr, bool isFromInit);


// Responsible for activating the Gamemaster and pinging the network (legacy GM list)
class CActiveGamemaster
{
private:
    int status{ACTIVE_GAMEMASTER_INITIAL};
    std::string notCapableReason;

public:
    CActiveGamemaster() = default;

    // Initialized by init.cpp
    // Keys for the main Gamemaster
    CPubKey pubKeyGamemaster;
    CKey privKeyGamemaster;

    // Initialized while registering Gamemaster
    Optional<CTxIn> vin{nullopt};
    CService service;

    /// Manage status of main Gamemaster
    void ManageStatus();
    void ResetStatus();
    std::string GetStatusMessage() const;
    int GetStatus() const { return status; }

    /// Ping Gamemaster
    bool SendGamemasterPing(std::string& errorMessage);
    /// Enable cold wallet mode (run a Gamemaster with no funds)
    bool EnableHotColdMasterNode(CTxIn& vin, CService& addr);

    void GetKeys(CKey& privKeyGamemaster, CPubKey& pubKeyGamemaster) const;
};

// Compatibility code: get vin and keys for either legacy or deterministic gamemaster
bool GetActiveGamemasterKeys(CTxIn& vin, Optional<CKey>& key, CBLSSecretKey& blsKey);
// Get active gamemaster BLS operator keys for DGM
bool GetActiveDGMKeys(CBLSSecretKey& key, CTxIn& vin);

#endif
