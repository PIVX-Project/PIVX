// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include "chainparamsbase.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "protocol.h"
#include "uint256.h"

#include <memory>
#include <vector>

struct CDNSSeedData {
    std::string host;
    bool supportsServiceBitsFiltering;
    CDNSSeedData(const std::string& strHost, bool supportsServiceBitsFilteringIn = false) : host(strHost), supportsServiceBitsFiltering(supportsServiceBitsFilteringIn) {}
};

typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
    const MapCheckpoints* mapCheckpoints;
    int64_t nTimeLastCheckpoint;
    int64_t nTransactionsLastCheckpoint;
    double fTransactionsPerDay;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * hemis system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    virtual ~CChainParams() {}
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,     // BIP16
        EXT_PUBLIC_KEY, // BIP32
        EXT_SECRET_KEY, // BIP32
        EXT_COIN_TYPE,  // BIP44
        STAKING_ADDRESS,

        MAX_BASE58_TYPES
    };

    enum Bech32Type {
        SAPLING_PAYMENT_ADDRESS,
        SAPLING_FULL_VIEWING_KEY,
        SAPLING_INCOMING_VIEWING_KEY,
        SAPLING_EXTENDED_SPEND_KEY,
        SAPLING_EXTENDED_FVK,

        BLS_SECRET_KEY,
        BLS_PUBLIC_KEY,

        MAX_BECH32_TYPES
    };

    const Consensus::Params& GetConsensus() const { return consensus; }
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    int GetDefaultPort() const { return nDefaultPort; }

    const CBlock& GenesisBlock() const { return genesis; }
    /** Policy: Filter transactions that do not match well-defined patterns */
    bool RequireStandard() const { return fRequireStandard; }
    /** How long to wait until we allow retrying of a LLMQ connection  */
    int LLMQConnectionRetryTimeout() const { return nLLMQConnectionRetryTimeout; }
    /** If this chain is exclusively used for testing */
    bool IsTestChain() const { return IsTestnet() || IsRegTestNet(); }
    /** Make miner wait to have peers to avoid wasting work */
    bool MiningRequiresPeers() const { return !IsRegTestNet(); }
    /** Headers first syncing is disabled */
    bool HeadersFirstSyncingActive() const { return false; };
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return IsRegTestNet(); }

    /** Return the network string */
    std::string NetworkIDString() const { return strNetworkID; }
    const std::vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    const std::string& Bech32HRP(Bech32Type type) const { return bech32HRPs[type]; }
    const std::vector<uint8_t>& FixedSeeds() const { return vFixedSeeds; }
    virtual const CCheckpointData& Checkpoints() const = 0;

    bool IsRegTestNet() const { return NetworkIDString() == CBaseChainParams::REGTEST; }
    bool IsTestnet() const { return NetworkIDString() == CBaseChainParams::TESTNET; }

    /** Tier two requests blockage mark expiration time */
    int FulfilledRequestExpireTime() const { return nFulfilledRequestExpireTime; }

    void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight);
protected:
    CChainParams() {}

    std::string strNetworkID;
    CBlock genesis;
    Consensus::Params consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    int nDefaultPort;
    std::vector<CDNSSeedData> vSeeds;
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    std::string bech32HRPs[MAX_BECH32_TYPES];
    std::vector<uint8_t> vFixedSeeds;
    bool fRequireStandard;

    // Tier two
    int nLLMQConnectionRetryTimeout;
    int nFulfilledRequestExpireTime;
};

/**
 * Creates and returns a std::unique_ptr<CChainParams> of the chosen chain.
 * @returns a CChainParams* of the chosen chain.
 * @throws a std::runtime_error if the chain is not supported.
 */
std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain);

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams& Params();

/**
 * Sets the params returned by Params() to those for the given chain name.
 * @throws std::runtime_error when the chain is not supported.
 */
void SelectParams(const std::string& chain);

/**
 * Allows modifying the network upgrade regtest parameters.
 */
void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight);

#endif // BITCOIN_CHAINPARAMS_H
