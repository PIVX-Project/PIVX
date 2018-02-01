// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2017 The ColossusCoinXT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

//   What makes a good checkpoint block?
// + Is surrounded by blocks with reasonable timestamps
//   (no blocks before with a timestamp after, none after with
//    timestamp before)
// + Contains no strange transactions
static Checkpoints::MapCheckpoints mapCheckpoints =
    boost::assign::map_list_of(0, uint256("000003fda0c59eb5958a22d789984021960b289a34f970ae149af8d10ec08ab8"));
static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints,
    1454124731, // * UNIX timestamp of last checkpoint block
    0,    // * total number of transactions between genesis and last checkpoint
                //   (the tx=... number in the SetBestChain debug.log lines)
    250        // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of(0, uint256("0x001"));
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1454124731,
    0,
    250};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of(0, uint256("0x001"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1454124731,
    0,
    100};

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0x92;
        pchMessageStart[1] = 0xc5;
        pchMessageStart[2] = 0xfe;
        pchMessageStart[3] = 0xea;
        vAlertPubKey = ParseHex("0000098d3ba6ba6e7423fa5cbd6a89e0a9a5300f88d33000005cb1a8b7ed2c1000335fc8dc4f012cb8241cc0bdafd6ca70c5f5448916e4e6f511ffffffffffffff");
        nDefaultPort = 51572;
        bnProofOfWorkLimit = ~uint256(0) >> 20; // ColossusCoinXT starting difficulty is 1 / 2^12
        nSubsidyHalvingInterval = 210000;
        nMaxReorganizationDepth = 30;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 0;
        nTargetTimespan = 1 * 120; // ColossusCoinXT: 2 minute
        nTargetSpacing = 1 * 120;  // ColossusCoinXT: 2 minute
        nLastPOWBlock = 10080;
        nMaturity = 90;
        nMasternodeCountDrift = 20;
        nModifierUpdateBlock = 0;
        nMaxMoneyOut = int64_t(20000000000) * COIN;
        nModifierInterval = 60;
        nModifierIntervalRatio = 3;
        nBudgetPercent = 5;
        nMinStakeAge = 60*60*24*7; //7 days
        nMasternodeRewardPercent = 60; // % of block reward that goes to masternodes
        nRequiredMasternodeCollateral = 10000000 * COIN; //10,000,000

        /**
         * Build the genesis block. Note that the output of the genesis coinbase cannot
         * be spent as it did not originally exist in the database.
         *
         * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
         *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
         *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
         *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
         *   vMerkleTree: e0028e
         */
        const char* pszTimestamp = "2018-01-28 : Bitcoin Block Hash for Height 506541 : 000000000000000000468f7a653fbc8ace9dc5bc01e51183122d2810ae9fe729";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 250 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04c10e83b2703ccf322f7dbd62dd5855ac7c10bd055814ce121ba32607d573b8810c02c0582aed05b4deb9c4b77b26d92428c61256cd42774babea0a073b2ed0c9") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime = 1454124731;
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 46752;
/*
        if (true && genesis.GetHash() != hashGenesisBlock)
            {
            printf("Searching for genesis block...\n");
            uint256 hashTarget = uint256().SetCompact(genesis.nBits);
            uint256 thash;
            while(true)
                {
                thash = genesis.GetHash();
                if(thash <= hashTarget){
                    break;
                }
                if ((genesis.nNonce & 0xFFF) == 0)
                    {
                    printf("none %08X: hash = %s (target = %s\n", genesis.nNonce, thash.ToString().c_str(), hashTarget.ToString().c_str());
                }
                ++genesis.nNonce;
                if (genesis.nNonce == 0)
                    {
                    printf("NONCE WRAPPED, incrementing time\n");
                    ++genesis.nTime;
                }
            }
            printf("genesis.nTime = %u \n", genesis.nTime);
            printf("genesis.nNonce = %u \n", genesis.nNonce);
            printf("genesis.gethash = %s\n", genesis.GetHash().ToString().c_str());
            printf("genesis.hashMerkleRoot = %s \n", genesis.hashMerkleRoot.ToString().c_str());
        }
*/
        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000003fda0c59eb5958a22d789984021960b289a34f970ae149af8d10ec08ab8"));
        assert(genesis.hashMerkleRoot == uint256("0xad5285d846cc72bcbb9ab14d1f867994f1337b765b49662f55856019470e927a"));

        vSeeds.push_back(CDNSSeedData("presstab", "colxseed.presstab.pw"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 30);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 13);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 212);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x02)(0x2D)(0x25)(0x33).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x02)(0x21)(0x31)(0x2B).convert_to_container<std::vector<unsigned char> >();
        // 	BIP44 coin type is from https://github.com/satoshilabs/slips/blob/master/slip-0044.md
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x77).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fHeadersFirstSyncingActive = false;

        nPoolMaxTransactions = 3;

        strSporkKey = "0423f2b48d99f15a0bceedbe9b05a06d028aca587c3a0f0ee4a7dff6b0859181c1225b5842a17e8bb74758b8f1757a82025631f3276bec0734c6f61de71c1e4d28";
        strObfuscationPoolDummyAddress = "D87q2gC9j6nNrnzCsg4aY6bHMLsT9nUhEw";
        nStartMasternodePayments = 1403728576; //Wed, 25 Jun 2014 20:36:16 GMT
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0x46;
        pchMessageStart[1] = 0x77;
        pchMessageStart[2] = 0x66;
        pchMessageStart[3] = 0xbb;
        vAlertPubKey = ParseHex("000010e83b2703ccf322f7dbd62dd5855ac7c10bd055814ce121ba32607d573b8810c02c0582aed05b4deb9c4b77b26d92428c61256cd42774babea0a073b2ed0c9");
        nDefaultPort = 51374;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nMinerThreads = 0;
        nTargetTimespan = 1 * 60; // ColossusCoinXT: 1 day
        nTargetSpacing = 1 * 60;  // ColossusCoinXT: 1 minute
        nLastPOWBlock = 200;
        nMaturity = 15;
        nMasternodeCountDrift = 4;
        nModifierUpdateBlock = 0; //approx Mon, 17 Apr 2017 04:00:00 GMT
        nMaxMoneyOut = 1200000000 * COIN;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1454124731;
        genesis.nNonce = 2452017;

        hashGenesisBlock = genesis.GetHash();
        //assert(hashGenesisBlock == uint256("0x1d411bb2686ab8aea4c3a80ea9812f84cf665c164878978af916830d92a5e00f"));

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 139); // Testnet colx addresses start with 'x' or 'y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);  // Testnet colx script addresses start with '8' or '9'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);     // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        // Testnet colx BIP32 pubkeys start with 'DRKV'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x3a)(0x80)(0x61)(0xa0).convert_to_container<std::vector<unsigned char> >();
        // Testnet colx BIP32 prvkeys start with 'DRKP'
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x3a)(0x80)(0x58)(0x37).convert_to_container<std::vector<unsigned char> >();
        // Testnet colx BIP44 coin type is '1' (All coin's testnet default)
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x01).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 2;
        strSporkKey = "04348C2F50F90267E64FACC65BFDC9D0EB147D090872FB97ABAE92E9A36E6CA60983E28E741F8E7277B11A7479B626AC115BA31463AC48178A5075C5A9319D4A38";
        strObfuscationPoolDummyAddress = "y57cqfGRkekRyDRNeJiLtYVEbvhXrNbmox";
        nStartMasternodePayments = 1420837558; //Fri, 09 Jan 2015 21:05:58 GMT
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xa1;
        pchMessageStart[1] = 0xcf;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0xac;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 24 * 60 * 60; // Pivx: 1 day
        nTargetSpacing = 1 * 60;        // Pivx: 1 minutes
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        genesis.nTime = 1454124731;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 12345;

        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 51476;
        //assert(hashGenesisBlock == uint256("0x41d203d900885c5ff18d2c550957743a164060a184182fa17ad1d8cff46c7eac"));

        vFixedSeeds.clear(); //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Testnet mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams
{
public:
    CUnitTestParams()
    {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 51478;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Unit test mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    virtual void setSubsidyHalvingInterval(int anSubsidyHalvingInterval) { nSubsidyHalvingInterval = anSubsidyHalvingInterval; }
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority) { nEnforceBlockUpgradeMajority = anEnforceBlockUpgradeMajority; }
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority) { nRejectBlockOutdatedMajority = anRejectBlockOutdatedMajority; }
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority) { nToCheckBlockUpgradeMajority = anToCheckBlockUpgradeMajority; }
    virtual void setDefaultConsistencyChecks(bool afDefaultConsistencyChecks) { fDefaultConsistencyChecks = afDefaultConsistencyChecks; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) { fAllowMinDifficultyBlocks = afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams* pCurrentParams = 0;

CModifiableParams* ModifiableParams()
{
    assert(pCurrentParams);
    assert(pCurrentParams == &unitTestParams);
    return (CModifiableParams*)&unitTestParams;
}

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    case CBaseChainParams::UNITTEST:
        return unitTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
