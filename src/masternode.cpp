// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gamemaster.h"

#include "addrman.h"
#include "gamemasterman.h"
#include "netbase.h"
#include "sync.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "wallet/wallet.h"

#define GAMEMASTER_MIN_GMP_SECONDS_REGTEST 90
#define GAMEMASTER_MIN_GMB_SECONDS_REGTEST 25
#define GAMEMASTER_PING_SECONDS_REGTEST 25
#define GAMEMASTER_EXPIRATION_SECONDS_REGTEST 12 * 60
#define GAMEMASTER_REMOVAL_SECONDS_REGTEST 13 * 60

#define GAMEMASTER_MIN_GMP_SECONDS (10 * 60)
#define GAMEMASTER_MIN_GMB_SECONDS (5 * 60)
#define GAMEMASTER_PING_SECONDS (5 * 60)
#define GAMEMASTER_EXPIRATION_SECONDS (120 * 60)
#define GAMEMASTER_REMOVAL_SECONDS (130 * 60)
#define GAMEMASTER_CHECK_SECONDS 5

int GamemasterMinPingSeconds()
{
    return Params().IsRegTestNet() ? GAMEMASTER_MIN_GMP_SECONDS_REGTEST : GAMEMASTER_MIN_GMP_SECONDS;
}

int GamemasterBroadcastSeconds()
{
    return Params().IsRegTestNet() ? GAMEMASTER_MIN_GMB_SECONDS_REGTEST : GAMEMASTER_MIN_GMB_SECONDS;
}

int GamemasterPingSeconds()
{
    return Params().IsRegTestNet() ? GAMEMASTER_PING_SECONDS_REGTEST : GAMEMASTER_PING_SECONDS;
}

int GamemasterExpirationSeconds()
{
    return Params().IsRegTestNet() ? GAMEMASTER_EXPIRATION_SECONDS_REGTEST : GAMEMASTER_EXPIRATION_SECONDS;
}

int GamemasterRemovalSeconds()
{
    return Params().IsRegTestNet() ? GAMEMASTER_REMOVAL_SECONDS_REGTEST : GAMEMASTER_REMOVAL_SECONDS;
}

// Used for sigTime < maxTimeWindow
int64_t GetMaxTimeWindow()
{
    return GetAdjustedTime() + 60 * 2;
}


CGamemaster::CGamemaster() :
        CSignedMessage()
{
    LOCK(cs);
    vin = CTxIn();
    addr = CService();
    pubKeyCollateralAddress = CPubKey();
    pubKeyGamemaster = CPubKey();
    sigTime = 0;
    lastPing = CGamemasterPing();
    protocolVersion = PROTOCOL_VERSION;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
    gmPayeeScript.clear();
}

CGamemaster::CGamemaster(const CGamemaster& other) :
        CSignedMessage(other)
{
    LOCK(cs);
    vin = other.vin;
    addr = other.addr;
    pubKeyCollateralAddress = other.pubKeyCollateralAddress;
    pubKeyGamemaster = other.pubKeyGamemaster;
    sigTime = other.sigTime;
    lastPing = other.lastPing;
    protocolVersion = other.protocolVersion;
    nScanningErrorCount = other.nScanningErrorCount;
    nLastScanningErrorBlockHeight = other.nLastScanningErrorBlockHeight;
    gmPayeeScript = other.gmPayeeScript;
}

CGamemaster::CGamemaster(const CDeterministicGMCPtr& dgm, int64_t registeredTime, const uint256& registeredHash) :
        CSignedMessage()
{
    LOCK(cs);
    vin = CTxIn(dgm->collateralOutpoint);
    addr = dgm->pdgmState->addr;
    pubKeyCollateralAddress = CPubKey();
    pubKeyGamemaster = CPubKey();
    sigTime = registeredTime;
    lastPing = CGamemasterPing(vin, registeredHash, registeredTime);
    protocolVersion = PROTOCOL_VERSION;
    nScanningErrorCount = 0;
    nLastScanningErrorBlockHeight = 0;
    gmPayeeScript = dgm->pdgmState->scriptPayout;
}

uint256 CGamemaster::GetSignatureHash() const
{
    int version = !addr.IsAddrV1Compatible() ? PROTOCOL_VERSION | ADDRV2_FORMAT : PROTOCOL_VERSION;
    CHashWriter ss(SER_GETHASH, version);
    ss << nMessVersion;
    ss << addr;
    ss << sigTime;
    ss << pubKeyCollateralAddress;
    ss << pubKeyGamemaster;
    ss << protocolVersion;
    return ss.GetHash();
}

std::string CGamemaster::GetStrMessage() const
{
    return (addr.ToString() +
            std::to_string(sigTime) +
            pubKeyCollateralAddress.GetID().ToString() +
            pubKeyGamemaster.GetID().ToString() +
            std::to_string(protocolVersion)
    );
}

//
// When a new gamemaster broadcast is sent, update our information
//
bool CGamemaster::UpdateFromNewBroadcast(CGamemasterBroadcast& gmb)
{
    if (gmb.sigTime > sigTime) {
        // TODO: lock cs. Need to be careful as gmb.lastPing.CheckAndUpdate locks cs_main internally.
        nMessVersion = gmb.nMessVersion;
        pubKeyGamemaster = gmb.pubKeyGamemaster;
        pubKeyCollateralAddress = gmb.pubKeyCollateralAddress;
        sigTime = gmb.sigTime;
        vchSig = gmb.vchSig;
        protocolVersion = gmb.protocolVersion;
        addr = gmb.addr;
        int nDoS = 0;
        if (gmb.lastPing.IsNull() || (!gmb.lastPing.IsNull() && gmb.lastPing.CheckAndUpdate(nDoS, false))) {
            lastPing = gmb.lastPing;
            gamemasterman.mapSeenGamemasterPing.emplace(lastPing.GetHash(), lastPing);
        }
        return true;
    }
    return false;
}

//
// Deterministically calculate a given "score" for a Gamemaster depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CGamemaster::CalculateScore(const uint256& hash) const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << hash;
    const arith_uint256& hash2 = UintToArith256(ss.GetHash());

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << hash;
    const arith_uint256& aux = UintToArith256(vin.prevout.hash) + vin.prevout.n;
    ss2 << aux;
    const arith_uint256& hash3 = UintToArith256(ss2.GetHash());

    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
}

CGamemaster::state CGamemaster::GetActiveState() const
{
    LOCK(cs);
    if (fCollateralSpent) {
        return GAMEMASTER_VIN_SPENT;
    }
    if (!IsPingedWithin(GamemasterRemovalSeconds())) {
        return GAMEMASTER_REMOVE;
    }
    if (!IsPingedWithin(GamemasterExpirationSeconds())) {
        return GAMEMASTER_EXPIRED;
    }
    if(lastPing.sigTime - sigTime < GamemasterMinPingSeconds()){
        return GAMEMASTER_PRE_ENABLED;
    }
    return GAMEMASTER_ENABLED;
}

bool CGamemaster::IsValidNetAddr() const
{
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().IsRegTestNet() ||
           (IsReachable(addr) && addr.IsRoutable());
}

CGamemasterBroadcast::CGamemasterBroadcast() :
        CGamemaster()
{ }

CGamemasterBroadcast::CGamemasterBroadcast(CService newAddr, CTxIn newVin, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyGamemasterNew, int protocolVersionIn, const CGamemasterPing& _lastPing) :
        CGamemaster()
{
    vin = newVin;
    addr = newAddr;
    pubKeyCollateralAddress = pubKeyCollateralAddressNew;
    pubKeyGamemaster = pubKeyGamemasterNew;
    protocolVersion = protocolVersionIn;
    lastPing = _lastPing;
    sigTime = lastPing.sigTime;
}

CGamemasterBroadcast::CGamemasterBroadcast(const CGamemaster& gm) :
        CGamemaster(gm)
{ }

bool CGamemasterBroadcast::Create(const std::string& strService,
                                  const std::string& strKeyGamemaster,
                                  const std::string& strTxHash,
                                  const std::string& strOutputIndex,
                                  std::string& strErrorRet,
                                  CGamemasterBroadcast& gmbRet,
                                  bool fOffline,
                                  int chainHeight)
{
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyGamemasterNew;
    CKey keyGamemasterNew;

    //need correct blocks to send ping
    if (!fOffline && !g_tiertwo_sync_state.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Gamemaster";
        LogPrint(BCLog::GAMEMASTER,"CGamemasterBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    std::string strError;
    if (strTxHash.empty() || strOutputIndex.empty()) {
        strError = "Invalid gamemaster collateral hash or output index";
        return error("%s: %s", __func__, strError);
    }

    const uint256 collateralHash = uint256S(strTxHash);
    int collateralOutputIndex;
    try {
        collateralOutputIndex = std::stoi(strOutputIndex.c_str());
    } catch (const std::exception& e) {
        strError = "Invalid gamemaster output index";
        return error("%s: %s on strOutputIndex", __func__, e.what());
    }

    if (!CMessageSigner::GetKeysFromSecret(strKeyGamemaster, keyGamemasterNew, pubKeyGamemasterNew)) {
        strErrorRet = strprintf("Invalid gamemaster key %s", strKeyGamemaster);
        LogPrint(BCLog::GAMEMASTER,"CGamemasterBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    // Use wallet-0 here. Legacy gmb creation can be removed after transition to DGM
    COutPoint collateralOut(collateralHash, collateralOutputIndex);
    if (vpwallets.empty() || !vpwallets[0]->GetGamemasterVinAndKeys(pubKeyCollateralAddressNew,
                                                                    keyCollateralAddressNew,
                                                                    collateralOut,
                                                                    true, // fValidateCollateral
                                                                    strError)) {
        strErrorRet = strError; // GetGamemasterVinAndKeys logs this error. Only returned for GUI error notification.
        LogPrint(BCLog::GAMEMASTER,"CGamemasterBroadcast::Create -- %s\n", strprintf("Could not allocate txin %s:%s for gamemaster %s", strTxHash, strOutputIndex, strService));
        return false;
    }

    int nPort = 0;
    int nDefaultPort = Params().GetDefaultPort();
    std::string strHost;
    SplitHostPort(strService, nPort, strHost);
    if (nPort == 0) nPort = nDefaultPort;
    CService _service(LookupNumeric(strHost.c_str(), nPort));

    // The service needs the correct default port to work properly
    if (!CheckDefaultPort(_service, strErrorRet, "CGamemasterBroadcast::Create"))
        return false;

    CTxIn txin(collateralOut.hash, collateralOut.n);
    return Create(txin, _service, keyCollateralAddressNew, pubKeyCollateralAddressNew, keyGamemasterNew, pubKeyGamemasterNew, strErrorRet, gmbRet);
}

bool CGamemasterBroadcast::Create(const CTxIn& txin,
                                  const CService& service,
                                  const CKey& keyCollateralAddressNew,
                                  const CPubKey& pubKeyCollateralAddressNew,
                                  const CKey& keyGamemasterNew,
                                  const CPubKey& pubKeyGamemasterNew,
                                  std::string& strErrorRet,
                                  CGamemasterBroadcast& gmbRet)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    LogPrint(BCLog::GAMEMASTER, "CGamemasterBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyGamemasterNew.GetID() = %s\n",
             EncodeDestination(pubKeyCollateralAddressNew.GetID()),
        pubKeyGamemasterNew.GetID().ToString());

    // Get block hash to ping (TODO: move outside of this function)
    const uint256& nBlockHashToPing = gamemasterman.GetBlockHashToPing();
    CGamemasterPing gmp(txin, nBlockHashToPing, GetAdjustedTime());
    if (!gmp.Sign(keyGamemasterNew, pubKeyGamemasterNew.GetID())) {
        strErrorRet = strprintf("Failed to sign ping, gamemaster=%s", txin.prevout.hash.ToString());
        LogPrint(BCLog::GAMEMASTER,"CGamemasterBroadcast::Create -- %s\n", strErrorRet);
        gmbRet = CGamemasterBroadcast();
        return false;
    }

    gmbRet = CGamemasterBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyGamemasterNew, PROTOCOL_VERSION, gmp);

    if (!gmbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address %s, gamemaster=%s", gmbRet.addr.ToStringIP (), txin.prevout.hash.ToString());
        LogPrint(BCLog::GAMEMASTER,"CGamemasterBroadcast::Create -- %s\n", strErrorRet);
        gmbRet = CGamemasterBroadcast();
        return false;
    }

    if (!gmbRet.Sign(keyCollateralAddressNew, pubKeyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, gamemaster=%s", txin.prevout.hash.ToString());
        LogPrint(BCLog::GAMEMASTER,"CGamemasterBroadcast::Create -- %s\n", strErrorRet);
        gmbRet = CGamemasterBroadcast();
        return false;
    }

    return true;
}

bool CGamemasterBroadcast::Sign(const CKey& key, const CPubKey& pubKey)
{
    std::string strError = "";
    nMessVersion = MessageVersion::MESS_VER_HASH;
    const std::string strMessage = GetSignatureHash().GetHex();

    if (!CMessageSigner::SignMessage(strMessage, vchSig, key)) {
        return error("%s : SignMessage() (nMessVersion=%d) failed", __func__, nMessVersion);
    }

    if (!CMessageSigner::VerifyMessage(pubKey, vchSig, strMessage, strError)) {
        return error("%s : VerifyMessage() (nMessVersion=%d) failed, error: %s\n",
                __func__, nMessVersion, strError);
    }

    return true;
}

bool CGamemasterBroadcast::CheckSignature() const
{
    std::string strError = "";
    std::string strMessage = (
                            nMessVersion == MessageVersion::MESS_VER_HASH ?
                            GetSignatureHash().GetHex() :
                            GetStrMessage()
                            );

    if(!CMessageSigner::VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError))
        return error("%s : VerifyMessage (nMessVersion=%d) failed: %s", __func__, nMessVersion, strError);

    return true;
}

bool CGamemasterBroadcast::CheckDefaultPort(CService service, std::string& strErrorRet, const std::string& strContext)
{
    int nDefaultPort = Params().GetDefaultPort();

    if (service.GetPort() != nDefaultPort && !Params().IsRegTestNet()) {
        strErrorRet = strprintf("Invalid port %u for gamemaster %s, only %d is supported on %s-net.",
            service.GetPort(), service.ToString(), nDefaultPort, Params().NetworkIDString());
        LogPrintf("%s - %s\n", strContext, strErrorRet);
        return false;
    }

    return true;
}

bool CGamemasterBroadcast::CheckAndUpdate(int& nDos)
{
    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetMaxTimeWindow()) {
        LogPrint(BCLog::GAMEMASTER,"gmb - Signature rejected, too far into the future %s\n", vin.prevout.hash.ToString());
        nDos = 1;
        return false;
    }

    // reject old signature version
    if (nMessVersion != MessageVersion::MESS_VER_HASH) {
        LogPrint(BCLog::GAMEMASTER, "gmb - rejecting old message version for gm %s\n", vin.prevout.hash.ToString());
        nDos = 30;
        return false;
    }

    if (protocolVersion < ActiveProtocol()) {
        LogPrint(BCLog::GAMEMASTER,"gmb - ignoring outdated Gamemaster %s protocol version %d\n", vin.prevout.hash.ToString(), protocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrint(BCLog::GAMEMASTER,"gmb - pubkey the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyGamemaster.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrint(BCLog::GAMEMASTER,"gmb - pubkey2 the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!vin.scriptSig.empty()) {
        LogPrint(BCLog::GAMEMASTER,"gmb - Ignore Not Empty ScriptSig %s\n", vin.prevout.hash.ToString());
        return false;
    }

    std::string strError = "";
    if (!CheckSignature()) {
        // For now (till v6.0), let's be "naive" and not fully ban nodes when the node is syncing
        // This could be a bad parsed BIP155 address that got stored on db on an old software version.
        nDos = g_tiertwo_sync_state.IsSynced() ? 100 : 5;
        return error("%s : Got bad Gamemaster address signature", __func__);
    }

    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != 49165) return false;
    } else if (addr.GetPort() == 49165)
        return false;

    // incorrect ping or its sigTime
    if(lastPing.IsNull() || !lastPing.CheckAndUpdate(nDos, false, true)) {
        return false;
    }

    //search existing Gamemaster list, this is where we update existing Gamemasters with new gmb broadcasts
    CGamemaster* pgm = gamemasterman.Find(vin.prevout);

    // no such gamemaster, nothing to update
    if (pgm == nullptr) return true;

    // this broadcast is older or equal than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    // (mapSeenGamemasterBroadcast in CGamemasterMan::ProcessMessage should filter legit duplicates)
    if(pgm->sigTime >= sigTime) {
        return error("%s : Bad sigTime %d for Gamemaster %20s %105s (existing broadcast is at %d)",
                      __func__, sigTime, addr.ToString(), vin.ToString(), pgm->sigTime);
    }

    // gamemaster is not enabled yet/already, nothing to update
    if (!pgm->IsEnabled()) return true;

    // gm.pubkey = pubkey, IsVinAssociatedWithPubkey is validated once below,
    //   after that they just need to match
    if (pgm->pubKeyCollateralAddress == pubKeyCollateralAddress && !pgm->IsBroadcastedWithin(GamemasterBroadcastSeconds())) {
        //take the newest entry
        LogPrint(BCLog::GAMEMASTER,"gmb - Got updated entry for %s\n", vin.prevout.hash.ToString());
        if (pgm->UpdateFromNewBroadcast((*this))) {
            if (pgm->IsEnabled()) Relay();
        }
        g_tiertwo_sync_state.AddedGamemasterList(GetHash());
    }

    return true;
}

void CGamemasterBroadcast::Relay()
{
    CInv inv(MSG_GAMEMASTER_ANNOUNCE, GetHash());
    g_connman->RelayInv(inv);
}

uint256 CGamemasterBroadcast::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << sigTime;
    ss << pubKeyCollateralAddress;
    return ss.GetHash();
}

CGamemasterPing::CGamemasterPing() :
        CSignedMessage(),
        vin(),
        blockHash(),
        sigTime(0)
{ }

CGamemasterPing::CGamemasterPing(const CTxIn& newVin, const uint256& nBlockHash, uint64_t _sigTime) :
        CSignedMessage(),
        vin(newVin),
        blockHash(nBlockHash),
        sigTime(_sigTime)
{ }

uint256 CGamemasterPing::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    if (nMessVersion == MessageVersion::MESS_VER_HASH) ss << blockHash;
    ss << sigTime;
    return ss.GetHash();
}

std::string CGamemasterPing::GetStrMessage() const
{
    return vin.ToString() + blockHash.ToString() + std::to_string(sigTime);
}

bool CGamemasterPing::CheckAndUpdate(int& nDos, bool fRequireAvailable, bool fCheckSigTimeOnly)
{
    if (sigTime > GetMaxTimeWindow()) {
        LogPrint(BCLog::GMPING,"%s: Signature rejected, too far into the future %s\n", __func__, vin.prevout.hash.ToString());
        nDos = 30;
        return false;
    }

    if (sigTime <= GetAdjustedTime() - 60 * 60) {
        LogPrint(BCLog::GMPING,"%s: Signature rejected, too far into the past %s - %d %d \n", __func__, vin.prevout.hash.ToString(), sigTime, GetAdjustedTime());
        nDos = 30;
        return false;
    }

    // reject old signature version
    if (nMessVersion != MessageVersion::MESS_VER_HASH) {
        LogPrint(BCLog::GMPING, "gmp - rejecting old message version for gm %s\n", vin.prevout.hash.ToString());
        nDos = 30;
        return false;
    }

    // Check if the ping block hash exists and it's within 24 blocks from the tip
    if (!gamemasterman.IsWithinDepth(blockHash, 2 * GMPING_DEPTH)) {
        LogPrint(BCLog::GMPING,"%s: Gamemaster %s block hash %s is too old or has an invalid block hash\n",
                                        __func__, vin.prevout.hash.ToString(), blockHash.ToString());
        // don't ban peers relaying stale data before the active protocol enforcement
        nDos = 33;
        return false;
    }

    // see if we have this Gamemaster
    CGamemaster* pgm = gamemasterman.Find(vin.prevout);
    const bool isGamemasterFound = (pgm != nullptr);
    const bool isSignatureValid = (isGamemasterFound && CheckSignature(pgm->pubKeyGamemaster.GetID()));

    if(fCheckSigTimeOnly) {
        if (isGamemasterFound && !isSignatureValid) {
            nDos = 33;
            return false;
        }
        return true;
    }

    LogPrint(BCLog::GMPING, "%s: New Ping - %s - %s - %lli\n", __func__, GetHash().ToString(), blockHash.ToString(), sigTime);

    if (isGamemasterFound && pgm->protocolVersion >= ActiveProtocol()) {

        // Update ping only if the gamemaster is in available state (pre-enabled or enabled)
        if (fRequireAvailable && !pgm->IsAvailableState()) {
            nDos = 20;
            return false;
        }

        // update only if there is no known ping for this gamemaster or
        // last ping was more then GAMEMASTER_MIN_GMP_SECONDS-60 ago comparing to this one
        if (!pgm->IsPingedWithin(GamemasterMinPingSeconds() - 60, sigTime)) {
            if (!isSignatureValid) {
                nDos = 33;
                return false;
            }

            // ping have passed the basic checks, can be updated now
            gamemasterman.mapSeenGamemasterPing.emplace(GetHash(), *this);

            // SetLastPing locks gamemaster cs. Be careful with the lock ordering.
            pgm->SetLastPing(*this);

            //gamemasterman.mapSeenGamemasterBroadcast.lastPing is probably outdated, so we'll update it
            CGamemasterBroadcast gmb(*pgm);
            const uint256& hash = gmb.GetHash();
            if (gamemasterman.mapSeenGamemasterBroadcast.count(hash)) {
                gamemasterman.mapSeenGamemasterBroadcast[hash].lastPing = *this;
            }

            if (!pgm->IsEnabled()) return false;

            LogPrint(BCLog::GMPING, "%s: Gamemaster ping accepted, vin: %s\n", __func__, vin.prevout.hash.ToString());

            Relay();
            return true;
        }
        LogPrint(BCLog::GMPING, "%s: Gamemaster ping arrived too early, vin: %s\n", __func__, vin.prevout.hash.ToString());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }
    LogPrint(BCLog::GMPING, "%s: Couldn't find compatible Gamemaster entry, vin: %s\n", __func__, vin.prevout.hash.ToString());

    return false;
}

void CGamemasterPing::Relay()
{
    CInv inv(MSG_GAMEMASTER_PING, GetHash());
    g_connman->RelayInv(inv);
}

GamemasterRef MakeGamemasterRefForDGM(const CDeterministicGMCPtr& dgm)
{
    // create legacy gamemaster for DGM
    int refHeight = std::max(dgm->pdgmState->nRegisteredHeight, dgm->pdgmState->nPoSeRevivedHeight);
    const CBlockIndex* pindex = WITH_LOCK(cs_main, return mapBlockIndex.at(chainActive[refHeight]->GetBlockHash()); );
    return std::make_shared<CGamemaster>(CGamemaster(dgm, pindex->GetBlockTime(), pindex->GetBlockHash()));
}
