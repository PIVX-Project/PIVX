// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2016-2018 The Syndicate developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Params.h"
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
    boost::assign::map_list_of
    // SyndicateDevs - RELEASE CHANGE - Checkpoins, timestamp of last checkpoint, total nr. of transactions
    (       0, uint256("000007bd0316ca5a1ebd628cbc910946031f25a65d469de48a74daab256b52a5"))          
    (       2, uint256("00000d9261d37ddd0280371317b4c41a3b179b3d31e4f5082b7e70a85d69effe"))          
    (     512, uint256("000003db9cbd239ab8724c6aa07451c3d975bbb8aa5d89868317714a69a3ef37"))          
    (    4559, uint256("fae475552886efefe299bb71c1d6b3764239056c4941464049ef6f6cabef0b6b"))          
    (    5530, uint256("8a5fe6dcd0bf8decf5a24789244748beeeb216c83e4458826fd67831047230f6"))          
    (    6160, uint256("2f94a6e0bf2d8ba2d84247ba400ffc04234a901377d08924ee6cf680aa11af0c"))          
    (   12588, uint256("93aa8321f7e5764df1cfd39b27dd4e08e36e8017abd1d92d00effc0257858f47"))          
    (   14374, uint256("681fef91feeae0dbfc2cc5335507bcaa6b896833ed5824e6a3fd426a4921616d"))          
    (   31000, uint256("f740eb8bba126e0898c4fb78b51539d8a0fae0f760bfedd338b5f6d708e2f12c"));         
static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints,
    1523732885, // * UNIX timestamp of last checkpoint block
    98314,      // * total number of transactions between genesis and last checkpoint
                //   (the tx=... number in the SetBestChain debug.log lines)
    2000        // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of
    (0, uint256("000007bd0316ca5a1ebd628cbc910946031f25a65d469de48a74daab256b52a5"));
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    152051740,
    0,
    2000
};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of
    (0, uint256("0000076a9b516adb26752e18f83c39add580e57c0669ab1bb379a27e1a268704"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    152051740,
    0,
    2000
};
libzerocoin::ZerocoinParams* CChainParams::Zerocoin_Params(bool useModulusV1) const
{
    assert(this);
    static CBigNum bnHexModulus = 0;
    if (!bnHexModulus)
        bnHexModulus.SetHex(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsHex = libzerocoin::ZerocoinParams(bnHexModulus);
    static CBigNum bnDecModulus = 0;
    if (!bnDecModulus)
        bnDecModulus.SetDec(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsDec = libzerocoin::ZerocoinParams(bnDecModulus);

    if (useModulusV1)
        return &ZCParamsHex;

    return &ZCParamsDec;
}

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
        /* f22f1ce5 */
        pchMessageStart[0] = 0xf2;
        pchMessageStart[1] = 0x2f;
        pchMessageStart[2] = 0x1c;
        pchMessageStart[3] = 0xe5;
        vAlertPubKey = ParseHex("0494b3488594838306c1b91a8e4a802de046bd3f815c707f56a95f31ce8c55c947f4c2e878633fe288a35974952be12e819fe591677f845a99a85273142864b993");
        nDefaultPort = 25992;                   
        bnProofOfWorkLimit = ~uint256(0) >> 20; // Syndicate starting difficulty is 1 / 2^12
        nSubsidyHalvingInterval = 210000;       // Halving interval
        nMaxReorganizationDepth = 100;          
        nEnforceBlockUpgradeMajority = 750;     
        nRejectBlockOutdatedMajority = 950;     
        nToCheckBlockUpgradeMajority = 1000;    
        nMinerThreads = 0;                      // Obsolete (**TODO**)
        nTargetTimespan = 1 * 60;               // Syndicate: 1 day
        nTargetSpacing = 1 * 60;                // Syndicate: 1 minute
        nMaturity = 100;                        // Block maturity
        nMasternodeCountDrift = 20;             
        nMaxMoneyOut = 398360470 * COIN;        // Max. Money is calculated by 2x premine value

        /** Height or Time Based Activations **/
        nLastPOWBlock = 512;                        // PoW End
        nModifierUpdateBlock = 2147483000;          // Modify block on height (currently we do not have any)
        nZerocoinStartHeight = 2147483000;          // Zerocoin start height (currently we do not have any)
        nZerocoinStartTime = 2147483000;            // GMT: Tuesday, January 19, 2038 3:03:20 AM
        nBlockEnforceSerialRange = 2147483000;      // Enforce serial range starting this block
        nBlockRecalculateAccumulators = 2147483000; // Trigger a recalculation of accumulators
        nBlockFirstFraudulent = 2147483000;         // First block that bad serials emerged (currently we do not have any)
        nBlockLastGoodCheckpoint = 2147483000;      // Last valid accumulator checkpoint (currently we do not have any)
        nBlockEnforceInvalidUTXO = 2147483000;      // Start enforcing the invalid UTXO's
        nBlockZerocoinV2 = 2147483000;              // !> The block that zerocoin v2 becomes active - roughly Tuesday, May 8, 2018 4:00:00 AM GMT
        // SYNX BEGIN
        nMasternodeCollateralMinimum = 5000;     // starting MN collateral
        nMasternodeCollateralMaximum = 25000;    // MN collateral at infinity
        // SYNX END 

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
        const char* pszTimestamp = "All Funds Are Safe: Binance Denies Crypto Hack Rumors";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime = 152051740;
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 3704342;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000007bd0316ca5a1ebd628cbc910946031f25a65d469de48a74daab256b52a5"));
        assert(genesis.hashMerkleRoot == uint256("0x813325b3464787f4ca44cac15ae7b1e4cbe193048086e0e1ef89d9cd2a2afd2a"));

        vSeeds.push_back(CDNSSeedData("synx.online", "seed.synx.online"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 63);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 85);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 153);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x02)(0x2D)(0x25)(0x33).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x02)(0x21)(0x31)(0x2B).convert_to_container<std::vector<unsigned char> >();
        // 	BIP44 coin type is from https://github.com/satoshilabs/slips/blob/master/slip-0044.md
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x77).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fHeadersFirstSyncingActive = false;

        nPoolMaxTransactions = 3;
        strSporkKey = "0494b3488594838306c1b91a8e4a802de046bd3f815c707f56a95f31ce8c55c947f4c2e878633fe288a35974952be12e819fe591677f845a99a85273142864b993";
        strObfuscationPoolDummyAddress = "SXmCHzNQXcgiHsS5fukBf95jxyPzqQP2it";
        nStartMasternodePayments = 1518696182; // GMT: Thursday, 15. February 2018 12:03:02

        /** Zerocoin */
        zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";
        nMaxZerocoinSpendsPerTransaction = 7; // Assume about 20kb each
        nMinZerocoinMintFee = 1 * CENT; //high fee required for zerocoin mints
        nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        nRequiredAccumulation = 1;
        nDefaultSecurityLevel = 100; //full security level for accumulators
        nZerocoinHeaderVersion = 4; //Block headers must be this version once zerocoin is active
        nZerocoinRequiredStakeDepth = 200; //The required confirmations for a zsynx to be stakable

        nBudget_Fee_Confirmations = 6; // Number of confirmations for the finalization fee
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
        /* 2eca4d3f */
        pchMessageStart[0] = 0x2e;
        pchMessageStart[1] = 0xca;
        pchMessageStart[2] = 0x4d;
        pchMessageStart[3] = 0x3f;
        vAlertPubKey = ParseHex("043c24f895d703d1a0837cff95d81c240e9c2a4e66e68ba5d7a607c971e131034b14747d8e7c08b2f3a7a7268eed34ad22b92a97a27eac494bedfbdfee3433959e");
        nDefaultPort = 25994;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nMinerThreads = 0;
        nTargetTimespan = 1 * 60; // Syndicate: 1 day
        nTargetSpacing = 1 * 60;  // Syndicate: 1 minute
        nMaturity = 15;
        nMasternodeCountDrift = 4;
        nMaxMoneyOut = 398360470 * COIN;

        /** Height or Time Based Activations **/
        nLastPOWBlock = 512;                        // PoW End
        nModifierUpdateBlock = 2147483000;          // Modify block on height (currently we do not have any)
        nZerocoinStartHeight = 2147483000;          // Zerocoin start height (currently we do not have any)
        nZerocoinStartTime = 2147483000;            // GMT: Tuesday, January 19, 2038 3:03:20 AM
        nBlockEnforceSerialRange = 2147483000;      // Enforce serial range starting this block
        nBlockRecalculateAccumulators = 2147483000; // Trigger a recalculation of accumulators
        nBlockFirstFraudulent = 2147483000;         // First block that bad serials emerged (currently we do not have any)
        nBlockLastGoodCheckpoint = 2147483000;      // Last valid accumulator checkpoint (currently we do not have any)
        nBlockEnforceInvalidUTXO = 2147483000;      // Start enforcing the invalid UTXO's
        nBlockZerocoinV2 = 2147483000;              // !> The block that zerocoin v2 becomes active
        // SYNX BEGIN
        nMasternodeCollateralMinimum = 5000;     // starting MN collateral
        nMasternodeCollateralMaximum = 25000;    // MN collateral at infinity
        // SYNX END 

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 152051740;
        genesis.nNonce = 3704342;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000007bd0316ca5a1ebd628cbc910946031f25a65d469de48a74daab256b52a5"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("synx.online", "seed.synx.online"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 97);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x3a)(0x80)(0x61)(0xa0).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x3a)(0x80)(0x58)(0x37).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x01).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 2;
        strSporkKey = "043c24f895d703d1a0837cff95d81c240e9c2a4e66e68ba5d7a607c971e131034b14747d8e7c08b2f3a7a7268eed34ad22b92a97a27eac494bedfbdfee3433959e";
        strObfuscationPoolDummyAddress = "gDkRMa4ceYWWN9VYK4ZDnQN21XHjTj45yY";
        nStartMasternodePayments = 1518696183;

        /** Zerocoin */
        zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";

        nMaxZerocoinSpendsPerTransaction = 7; // Assume about 20kb each
        nMinZerocoinMintFee = 1 * CENT; //high fee required for zerocoin mints
        nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        nRequiredAccumulation = 1;
        nDefaultSecurityLevel = 100; //full security level for accumulators
        nZerocoinHeaderVersion = 4; //Block headers must be this version once zerocoin is active
        nBudget_Fee_Confirmations = 3; // Number of confirmations for the finalization fee. We have to make this very short
                                       // here because we only have a 8 block finalization window on testnet
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
        /* 1276a1ff */
        pchMessageStart[0] = 0x12;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0xa1;
        pchMessageStart[3] = 0xff;
        nDefaultPort = 25996;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 24 * 60 * 60;         // Syndicate: 1 day
        nTargetSpacing = 1 * 60;                // Syndicate: 1 minutes
        bnProofOfWorkLimit = ~uint256(0) >> 1;

        genesis.nTime = 152051740;
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 671421;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x0000076a9b516adb26752e18f83c39add580e57c0669ab1bb379a27e1a268704"));

        vFixedSeeds.clear();                    //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();                         //! Testnet mode doesn't have any DNS seeds.

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
        nDefaultPort = 25998;

        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Unit test mode doesn't have any DNS seeds.

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
