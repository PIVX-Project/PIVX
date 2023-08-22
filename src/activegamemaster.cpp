// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activegamemaster.h"

#include "addrman.h"
#include "bls/key_io.h"
#include "bls/bls_wrapper.h"
#include "gamemaster.h"
#include "gamemasterconfig.h"
#include "gamemasterman.h"
#include "messagesigner.h"
#include "netbase.h"
#include "protocol.h"
#include "tiertwo/net_gamemasters.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "validation.h"

// Keep track of the active Gamemaster
CActiveDeterministicGamemasterManager* activeGamemasterManager{nullptr};

static bool GetLocalAddress(CService& addrRet)
{
    // First try to find whatever our own local address is known internally.
    // Addresses could be specified via 'externalip' or 'bind' option, discovered via UPnP
    // or added by TorController. Use some random dummy IPv4 peer to prefer the one
    // reachable via IPv4.
    CNetAddr addrDummyPeer;
    bool fFound{false};
    if (LookupHost("8.8.8.8", addrDummyPeer, false)) {
        fFound = GetLocal(addrRet, &addrDummyPeer) && CActiveDeterministicGamemasterManager::IsValidNetAddr(addrRet);
    }
    if (!fFound && Params().IsRegTestNet()) {
        if (Lookup("127.0.0.1", addrRet, GetListenPort(), false)) {
            fFound = true;
        }
    }
    if (!fFound) {
        // If we have some peers, let's try to find our local address from one of them
        g_connman->ForEachNodeContinueIf([&fFound, &addrRet](CNode* pnode) {
            if (pnode->addr.IsIPv4())
                fFound = GetLocal(addrRet, &pnode->addr) && CActiveDeterministicGamemasterManager::IsValidNetAddr(addrRet);
            return !fFound;
        });
    }
    return fFound;
}

std::string CActiveDeterministicGamemasterManager::GetStatus() const
{
    switch (state) {
        case GAMEMASTER_WAITING_FOR_PROTX:    return "Waiting for ProTx to appear on-chain";
        case GAMEMASTER_POSE_BANNED:          return "Gamemaster was PoSe banned";
        case GAMEMASTER_REMOVED:              return "Gamemaster removed from list";
        case GAMEMASTER_OPERATOR_KEY_CHANGED: return "Operator key changed or revoked";
        case GAMEMASTER_PROTX_IP_CHANGED:     return "IP address specified in ProTx changed";
        case GAMEMASTER_READY:                return "Ready";
        case GAMEMASTER_ERROR:                return "Error. " + strError;
        default:                              return "Unknown";
    }
}

OperationResult CActiveDeterministicGamemasterManager::SetOperatorKey(const std::string& strGMOperatorPrivKey)
{
    LOCK(cs_main); // Lock cs_main so the node doesn't perform any action while we setup the Gamemaster
    LogPrintf("Initializing deterministic gamemaster...\n");
    if (strGMOperatorPrivKey.empty()) {
        return errorOut("ERROR: Gamemaster operator priv key cannot be empty.");
    }

    auto opSk = bls::DecodeSecret(Params(), strGMOperatorPrivKey);
    if (!opSk) {
        return errorOut(_("Invalid gmoperatorprivatekey. Please see the documentation."));
    }
    info.keyOperator = *opSk;
    info.pubKeyOperator = info.keyOperator.GetPublicKey();
    return {true};
}

OperationResult CActiveDeterministicGamemasterManager::GetOperatorKey(CBLSSecretKey& key, CDeterministicGMCPtr& dgm) const
{
    if (!IsReady()) {
        return errorOut("Active gamemaster not ready");
    }
    dgm = deterministicGMManager->GetListAtChainTip().GetValidGM(info.proTxHash);
    if (!dgm) {
        return errorOut(strprintf("Active gamemaster %s not registered or PoSe banned", info.proTxHash.ToString()));
    }
    if (info.pubKeyOperator != dgm->pdgmState->pubKeyOperator.Get()) {
        return errorOut("Active gamemaster operator key changed or revoked");
    }
    // return key
    key = info.keyOperator;
    return {true};
}

void CActiveDeterministicGamemasterManager::Init(const CBlockIndex* pindexTip)
{
    // set gamemaster arg if called from RPC
    if (!fMasterNode) {
        gArgs.ForceSetArg("-gamemaster", "1");
        fMasterNode = true;
    }

    if (!deterministicGMManager->IsDIP3Enforced(pindexTip->nHeight)) {
        state = GAMEMASTER_ERROR;
        strError = "Evo upgrade is not active yet.";
        LogPrintf("%s -- ERROR: %s\n", __func__, strError);
        return;
    }

    LOCK(cs_main);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        state = GAMEMASTER_ERROR;
        strError = "Gamemaster must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        LogPrintf("%s ERROR: %s\n", __func__, strError);
        return;
    }

    if (!GetLocalAddress(info.service)) {
        state = GAMEMASTER_ERROR;
        strError = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        LogPrintf("%s ERROR: %s\n", __func__, strError);
        return;
    }

    CDeterministicGMList gmList = deterministicGMManager->GetListForBlock(pindexTip);

    CDeterministicGMCPtr dgm = gmList.GetGMByOperatorKey(info.pubKeyOperator);
    if (!dgm) {
        // GM not appeared on the chain yet
        return;
    }

    if (dgm->IsPoSeBanned()) {
        state = GAMEMASTER_POSE_BANNED;
        return;
    }

    LogPrintf("%s: proTxHash=%s, proTx=%s\n", __func__, dgm->proTxHash.ToString(), dgm->ToString());

    if (info.service != dgm->pdgmState->addr) {
        state = GAMEMASTER_ERROR;
        strError = strprintf("Local address %s does not match the address from ProTx (%s)",
                             info.service.ToStringIPPort(), dgm->pdgmState->addr.ToStringIPPort());
        LogPrintf("%s ERROR: %s\n", __func__, strError);
        return;
    }

    // Check socket connectivity
    const std::string& strService = info.service.ToString();
    LogPrintf("%s: Checking inbound connection to '%s'\n", __func__, strService);
    SOCKET hSocket = CreateSocket(info.service);
    if (hSocket == INVALID_SOCKET) {
        state = GAMEMASTER_ERROR;
        strError = "DGM connectivity check failed, could not create socket to DGM running at " + strService;
        LogPrintf("%s -- ERROR: %s\n", __func__, strError);
        return;
    }
    bool fConnected = ConnectSocketDirectly(info.service, hSocket, nConnectTimeout, true) && IsSelectableSocket(hSocket);
    CloseSocket(hSocket);

    if (!fConnected) {
        state = GAMEMASTER_ERROR;
        strError = "DGM connectivity check failed, could not connect to DGM running at " + strService;
        LogPrintf("%s ERROR: %s\n", __func__, strError);
        return;
    }

    info.proTxHash = dgm->proTxHash;
    g_connman->GetTierTwoConnMan()->setLocalDGM(info.proTxHash);
    state = GAMEMASTER_READY;
    LogPrintf("Deterministic Gamemaster initialized\n");
}

void CActiveDeterministicGamemasterManager::Reset(gamemaster_state_t _state, const CBlockIndex* pindexTip)
{
    state = _state;
    SetNullProTx();
    // GM might have reappeared in same block with a new ProTx
    Init(pindexTip);
}

void CActiveDeterministicGamemasterManager::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload)
{
    if (fInitialDownload)
        return;

    if (!fMasterNode || !deterministicGMManager->IsDIP3Enforced(pindexNew->nHeight))
        return;

    if (state == GAMEMASTER_READY) {
        auto newDgm = deterministicGMManager->GetListForBlock(pindexNew).GetValidGM(info.proTxHash);
        if (newDgm == nullptr) {
            // GM disappeared from GM list
            Reset(GAMEMASTER_REMOVED, pindexNew);
            return;
        }

        auto oldDgm = deterministicGMManager->GetListForBlock(pindexNew->pprev).GetGM(info.proTxHash);
        if (oldDgm == nullptr) {
            // should never happen if state is GAMEMASTER_READY
            LogPrintf("%s: WARNING: unable to find active gm %s in prev block list %s\n",
                      __func__, info.proTxHash.ToString(), pindexNew->pprev->GetBlockHash().ToString());
            return;
        }

        if (newDgm->pdgmState->pubKeyOperator != oldDgm->pdgmState->pubKeyOperator) {
            // GM operator key changed or revoked
            Reset(GAMEMASTER_OPERATOR_KEY_CHANGED, pindexNew);
            return;
        }

        if (newDgm->pdgmState->addr != oldDgm->pdgmState->addr) {
            // GM IP changed
            Reset(GAMEMASTER_PROTX_IP_CHANGED, pindexNew);
            return;
        }
    } else {
        // GM might have (re)appeared with a new ProTx or we've found some peers
        // and figured out our local address
        Init(pindexNew);
    }
}

bool CActiveDeterministicGamemasterManager::IsValidNetAddr(const CService& addrIn)
{
    // TODO: check IPv6 and TOR addresses
    return Params().IsRegTestNet() || (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}


/********* LEGACY *********/

OperationResult initGamemaster(const std::string& _strMasterNodePrivKey, const std::string& _strMasterNodeAddr, bool isFromInit)
{
    if (!isFromInit && fMasterNode) {
        return errorOut( "ERROR: Gamemaster already initialized.");
    }

    LOCK(cs_main); // Lock cs_main so the node doesn't perform any action while we setup the Gamemaster
    LogPrintf("Initializing gamemaster, addr %s..\n", _strMasterNodeAddr.c_str());

    if (_strMasterNodePrivKey.empty()) {
        return errorOut("ERROR: Gamemaster priv key cannot be empty.");
    }

    if (_strMasterNodeAddr.empty()) {
        return errorOut("ERROR: Empty gamemasteraddr");
    }

    // Address parsing.
    const CChainParams& params = Params();
    int nPort = 0;
    int nDefaultPort = params.GetDefaultPort();
    std::string strHost;
    SplitHostPort(_strMasterNodeAddr, nPort, strHost);

    // Allow for the port number to be omitted here and just double check
    // that if a port is supplied, it matches the required default port.
    if (nPort == 0) nPort = nDefaultPort;
    if (nPort != nDefaultPort && !params.IsRegTestNet()) {
        return errorOut(strprintf(_("Invalid -gamemasteraddr port %d, only %d is supported on %s-net."),
                                           nPort, nDefaultPort, Params().NetworkIDString()));
    }
    CService addrTest(LookupNumeric(strHost, nPort));
    if (!addrTest.IsValid()) {
        return errorOut(strprintf(_("Invalid -gamemasteraddr address: %s"), _strMasterNodeAddr));
    }

    // Peer port needs to match the gamemaster public one for IPv4 and IPv6.
    // Onion can run in other ports because those are behind a hidden service which has the public port fixed to the default port.
    if (nPort != GetListenPort() && !addrTest.IsTor()) {
        return errorOut(strprintf(_("Invalid -gamemasteraddr port %d, isn't the same as the peer port %d"),
                                  nPort, GetListenPort()));
    }

    CKey key;
    CPubKey pubkey;
    if (!CMessageSigner::GetKeysFromSecret(_strMasterNodePrivKey, key, pubkey)) {
        return errorOut(_("Invalid gamemasterprivkey. Please see the documentation."));
    }

    activeGamemaster.pubKeyGamemaster = pubkey;
    activeGamemaster.privKeyGamemaster = key;
    activeGamemaster.service = addrTest;
    fMasterNode = true;

    if (g_tiertwo_sync_state.IsBlockchainSynced()) {
        // Check if the gamemaster already exists in the list
        CGamemaster* pgm = gamemasterman.Find(pubkey);
        if (pgm) activeGamemaster.EnableHotColdMasterNode(pgm->vin, pgm->addr);
    }

    return {true};
}

//
// Bootup the Gamemaster, look for a 10000 hemis input and register on the network
//
void CActiveGamemaster::ManageStatus()
{
    if (!fMasterNode) return;
    if (activeGamemasterManager != nullptr) {
        // Deterministic gamemaster
        return;
    }

    // !TODO: Legacy gamemasters - remove after enforcement
    LogPrint(BCLog::GAMEMASTER, "CActiveGamemaster::ManageStatus() - Begin\n");

    // If a DGM has been registered with same collateral, disable me.
    CGamemaster* pgm = gamemasterman.Find(pubKeyGamemaster);
    if (pgm && deterministicGMManager->GetListAtChainTip().HasGMByCollateral(pgm->vin.prevout)) {
        LogPrintf("%s: Disabling active legacy Gamemaster %s as the collateral is now registered with a DGM\n",
                         __func__, pgm->vin.prevout.ToString());
        status = ACTIVE_GAMEMASTER_NOT_CAPABLE;
        notCapableReason = "Collateral registered with DGM";
        return;
    }

    //need correct blocks to send ping
    if (!Params().IsRegTestNet() && !g_tiertwo_sync_state.IsBlockchainSynced()) {
        status = ACTIVE_GAMEMASTER_SYNC_IN_PROCESS;
        LogPrintf("CActiveGamemaster::ManageStatus() - %s\n", GetStatusMessage());
        return;
    }

    if (status == ACTIVE_GAMEMASTER_SYNC_IN_PROCESS) status = ACTIVE_GAMEMASTER_INITIAL;

    if (status == ACTIVE_GAMEMASTER_INITIAL || (pgm && status == ACTIVE_GAMEMASTER_NOT_CAPABLE)) {
        if (pgm) {
            if (pgm->protocolVersion != PROTOCOL_VERSION) {
                LogPrintf("%s: ERROR Trying to start a gamemaster running an old protocol version, "
                          "the controller and gamemaster wallets need to be running the latest release version.\n", __func__);
                return;
            }
            // Update vin and service
            EnableHotColdMasterNode(pgm->vin, pgm->addr);
        }
    }

    if (status != ACTIVE_GAMEMASTER_STARTED) {
        // Set defaults
        status = ACTIVE_GAMEMASTER_NOT_CAPABLE;
        notCapableReason = "";

        LogPrintf("%s - Checking inbound connection for gamemaster to '%s'\n", __func__ , service.ToString());

        CAddress addr(service, NODE_NETWORK);
        if (!g_connman->IsNodeConnected(addr)) {
            CNode* node = g_connman->ConnectNode(addr);
            if (!node) {
                notCapableReason =
                        "Gamemaster address:port connection availability test failed, could not open a connection to the public gamemaster address (" +
                        service.ToString() + ")";
                LogPrintf("%s - not capable: %s\n", __func__, notCapableReason);
            } else {
                // don't leak allocated object in memory
                delete node;
            }
            return;
        }

        notCapableReason = "Waiting for start message from controller.";
        return;
    }

    //send to all peers
    std::string errorMessage;
    if (!SendGamemasterPing(errorMessage)) {
        LogPrintf("CActiveGamemaster::ManageStatus() - Error on Ping: %s\n", errorMessage);
    }
}

void CActiveGamemaster::ResetStatus()
{
    status = ACTIVE_GAMEMASTER_INITIAL;
    ManageStatus();
}

std::string CActiveGamemaster::GetStatusMessage() const
{
    switch (status) {
    case ACTIVE_GAMEMASTER_INITIAL:
        return "Node just started, not yet activated";
    case ACTIVE_GAMEMASTER_SYNC_IN_PROCESS:
        return "Sync in progress. Must wait until sync is complete to start Gamemaster";
    case ACTIVE_GAMEMASTER_NOT_CAPABLE:
        return "Not capable gamemaster: " + notCapableReason;
    case ACTIVE_GAMEMASTER_STARTED:
        return "Gamemaster successfully started";
    default:
        return "unknown";
    }
}

bool CActiveGamemaster::SendGamemasterPing(std::string& errorMessage)
{
    if (vin == nullopt) {
        errorMessage = "Active Gamemaster not initialized";
        return false;
    }

    if (status != ACTIVE_GAMEMASTER_STARTED) {
        errorMessage = "Gamemaster is not in a running status";
        return false;
    }

    if (!privKeyGamemaster.IsValid() || !pubKeyGamemaster.IsValid()) {
        errorMessage = "Error upon gamemaster key.\n";
        return false;
    }

    LogPrintf("CActiveGamemaster::SendGamemasterPing() - Relay Gamemaster Ping vin = %s\n", vin->ToString());

    const uint256& nBlockHash = gamemasterman.GetBlockHashToPing();
    CGamemasterPing gmp(*vin, nBlockHash, GetAdjustedTime());
    if (!gmp.Sign(privKeyGamemaster, pubKeyGamemaster.GetID())) {
        errorMessage = "Couldn't sign Gamemaster Ping";
        return false;
    }

    // Update lastPing for our gamemaster in Gamemaster list
    CGamemaster* pgm = gamemasterman.Find(vin->prevout);
    if (pgm != nullptr) {
        if (pgm->IsPingedWithin(GamemasterPingSeconds(), gmp.sigTime)) {
            errorMessage = "Too early to send Gamemaster Ping";
            return false;
        }

        // SetLastPing locks the gamemaster cs, be careful with the lock order.
        pgm->SetLastPing(gmp);
        gamemasterman.mapSeenGamemasterPing.emplace(gmp.GetHash(), gmp);

        //gamemasterman.mapSeenGamemasterBroadcast.lastPing is probably outdated, so we'll update it
        CGamemasterBroadcast gmb(*pgm);
        uint256 hash = gmb.GetHash();
        if (gamemasterman.mapSeenGamemasterBroadcast.count(hash)) {
            // SetLastPing locks the gamemaster cs, be careful with the lock order.
            // TODO: check why are we double setting the last ping here..
            gamemasterman.mapSeenGamemasterBroadcast[hash].SetLastPing(gmp);
        }

        gmp.Relay();
        return true;

    } else {
        // Seems like we are trying to send a ping while the Gamemaster is not registered in the network
        errorMessage = "Gamemaster List doesn't include our Gamemaster, shutting down Gamemaster pinging service! " + vin->ToString();
        status = ACTIVE_GAMEMASTER_NOT_CAPABLE;
        notCapableReason = errorMessage;
        return false;
    }
}

// when starting a Gamemaster, this can enable to run as a hot wallet with no funds
bool CActiveGamemaster::EnableHotColdMasterNode(CTxIn& newVin, CService& newService)
{
    if (!fMasterNode) return false;

    status = ACTIVE_GAMEMASTER_STARTED;

    //The values below are needed for signing gmping messages going forward
    vin = newVin;
    service = newService;

    LogPrintf("CActiveGamemaster::EnableHotColdMasterNode() - Enabled! You may shut down the cold daemon.\n");

    return true;
}

void CActiveGamemaster::GetKeys(CKey& _privKeyGamemaster, CPubKey& _pubKeyGamemaster) const
{
    if (!privKeyGamemaster.IsValid() || !pubKeyGamemaster.IsValid()) {
        throw std::runtime_error("Error trying to get gamemaster keys");
    }
    _privKeyGamemaster = privKeyGamemaster;
    _pubKeyGamemaster = pubKeyGamemaster;
}

bool GetActiveDGMKeys(CBLSSecretKey& key, CTxIn& vin)
{
    if (activeGamemasterManager == nullptr) {
        return error("%s: Active Gamemaster not initialized", __func__);
    }
    CDeterministicGMCPtr dgm;
    auto res = activeGamemasterManager->GetOperatorKey(key, dgm);
    if (!res) {
        return error("%s: %s", __func__, res.getError());
    }
    vin = CTxIn(dgm->collateralOutpoint);
    return true;
}

bool GetActiveGamemasterKeys(CTxIn& vin, Optional<CKey>& key, CBLSSecretKey& blsKey)
{
    if (activeGamemasterManager != nullptr) {
        // deterministic gm
        key = nullopt;
        return GetActiveDGMKeys(blsKey, vin);
    }
    // legacy gm
    if (activeGamemaster.vin == nullopt) {
        return error("%s: Active Gamemaster not initialized", __func__);
    }
    if (activeGamemaster.GetStatus() != ACTIVE_GAMEMASTER_STARTED) {
        return error("%s: GM not started (%s)", __func__, activeGamemaster.GetStatusMessage());
    }
    vin = *activeGamemaster.vin;
    CKey sk;
    CPubKey pk;
    activeGamemaster.GetKeys(sk, pk);
    key = Optional<CKey>(sk);
    blsKey.Reset();
    return true;
}
