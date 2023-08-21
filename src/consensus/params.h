// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "amount.h"
#include "libzerocoin/Params.h"
#include "optional.h"
#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

/**
* Index into Params.vUpgrades and NetworkUpgradeInfo
*
* Being array indices, these MUST be numbered consecutively.
*
* The order of these indices MUST match the order of the upgrades on-chain, as
* several functions depend on the enum being sorted.
*/
enum UpgradeIndex : uint32_t {
    BASE_NETWORK,
    UPGRADE_POS,
    UPGRADE_POS_V2,
    UPGRADE_ZC,
    UPGRADE_ZC_V2,
    UPGRADE_BIP65,
    UPGRADE_ZC_PUBLIC,
    UPGRADE_V3_4,
    UPGRADE_V4_0,
    UPGRADE_V5_0,
    UPGRADE_V5_2,
    UPGRADE_V5_3,
    UPGRADE_V5_5,
    UPGRADE_V6_0,
    UPGRADE_TESTDUMMY,
    // NOTE: Also add new upgrades to NetworkUpgradeInfo in upgrades.cpp
    MAX_NETWORK_UPGRADES
};

struct NetworkUpgrade {
    /**
     * The first protocol version which will understand the new consensus rules
     */
    int nProtocolVersion;

    /**
     * Height of the first block for which the new consensus rules will be active
     */
    int nActivationHeight;

    /**
     * Special value for nActivationHeight indicating that the upgrade is always active.
     * This is useful for testing, as it means tests don't need to deal with the activation
     * process (namely, faking a chain of somewhat-arbitrary length).
     *
     * New blockchains that want to enable upgrade rules from the beginning can also use
     * this value. However, additional care must be taken to ensure the genesis block
     * satisfies the enabled rules.
     */
    static constexpr int ALWAYS_ACTIVE = 0;

    /**
     * Special value for nActivationHeight indicating that the upgrade will never activate.
     * This is useful when adding upgrade code that has a testnet activation height, but
     * should remain disabled on mainnet.
     */
    static constexpr int NO_ACTIVATION_HEIGHT = -1;

    /**
     * The hash of the block at height nActivationHeight, if known. This is set manually
     * after a network upgrade activates.
     *
     * We use this in IsInitialBlockDownload to detect whether we are potentially being
     * fed a fake alternate chain. We use NU activation blocks for this purpose instead of
     * the checkpoint blocks, because network upgrades (should) have significantly more
     * scrutiny than regular releases. nMinimumChainWork MUST be set to at least the chain
     * work of this block, otherwise this detection will have false positives.
     */
    Optional<uint256> hashActivationBlock;
};

enum LLMQType : uint8_t
{
    LLMQ_NONE = 0xff,

    LLMQ_50_60 = 1, // 50 members, 30 (60%) threshold, one per hour
    LLMQ_400_60 = 2, // 400 members, 240 (60%) threshold, one every 12 hours
    LLMQ_400_85 = 3, // 400 members, 340 (85%) threshold, one every 24 hours

    // for testing only
    LLMQ_TEST = 100, // 3 members, 2 (66%) threshold, one per hour. Params might differ when -llmqtestparams is used
};

// Configures a LLMQ and its DKG
// See https://github.com/dashpay/dips/blob/master/dip-0006.md for more details
struct LLMQParams {
    LLMQType type;

    // not consensus critical, only used in logging, RPC and UI
    std::string name;

    // the size of the quorum, e.g. 50 or 400
    int size;

    // The minimum number of valid members after the DKK. If less members are determined valid, no commitment can be
    // created. Should be higher then the threshold to allow some room for failing nodes, otherwise quorum might end up
    // not being able to ever created a recovered signature if more nodes fail after the DKG
    int minSize;

    // The threshold required to recover a final signature. Should be at least 50%+1 of the quorum size. This value
    // also controls the size of the public key verification vector and has a large influence on the performance of
    // recovery. It also influences the amount of minimum messages that need to be exchanged for a single signing session.
    // This value has the most influence on the security of the quorum. The number of total malicious masternodes
    // required to negatively influence signing sessions highly correlates to the threshold percentage.
    int threshold;

    // The interval in number blocks for DKGs and the creation of LLMQs. If set to 60 for example, a DKG will start
    // every 60 blocks, which is approximately once every hour.
    int dkgInterval;

    // The number of blocks per phase in a DKG session. There are 6 phases plus the mining phase that need to be processed
    // per DKG. Set this value to a number of blocks so that each phase has enough time to propagate all required
    // messages to all members before the next phase starts. If blocks are produced too fast, whole DKG sessions will
    // fail.
    int dkgPhaseBlocks;

    // The starting block inside the DKG interval for when mining of commitments starts. The value is inclusive.
    // Starting from this block, the inclusion of (possibly null) commitments is enforced until the first non-null
    // commitment is mined. The chosen value should be at least 5 * dkgPhaseBlocks so that it starts right after the
    // finalization phase.
    int dkgMiningWindowStart;

    // The ending block inside the DKG interval for when mining of commitments ends. The value is inclusive.
    // Choose a value so that miners have enough time to receive the commitment and mine it. Also take into consideration
    // that miners might omit real commitments and revert to always including null commitments. The mining window should
    // be large enough so that other miners have a chance to produce a block containing a non-null commitment. The window
    // should at the same time not be too large so that not too much space is wasted with null commitments in case a DKG
    // session failed.
    int dkgMiningWindowEnd;

    // In the complaint phase, members will vote on other members being bad (missing valid contribution). If at least
    // dkgBadVotesThreshold have voted for another member to be bad, it will considered to be bad by all other members
    // as well. This serves as a protection against late-comers who send their contribution on the bring of
    // phase-transition, which would otherwise result in inconsistent views of the valid members set
    int dkgBadVotesThreshold;

    // Number of quorums to consider "active" for signing sessions
    int signingActiveQuorumCount;

    // Used for inter-quorum communication. This is the number of quorums for which we should keep old connections. This
    // should be at least one more then the active quorums set.
    int keepOldConnections;

    // How many members should we try to send all sigShares to before we give up.
    int recoveryMembers;

    // The limit of blocks up until where the dkg qfc will be accepted.
    int cacheDkgInterval;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    uint256 powLimit;
    uint256 posLimitV1;
    uint256 posLimitV2;
    int nBudgetCycleBlocks;
    int nBudgetFeeConfirmations;
    int nCoinbaseMaturity;
    int nFutureTimeDriftPoW;
    int nFutureTimeDriftPoS;
    CAmount nMaxMoneyOut;
    CAmount nMNCollateralAmt;
    int nMNCollateralMinConf;
    CAmount nMNBlockReward;
    CAmount nNewMNBlockReward;
    int64_t nProposalEstablishmentTime;
    int nStakeMinAge;
    int nStakeMinDepth;
    int64_t nTargetTimespan;
    int64_t nTargetTimespanV2;
    int64_t nTargetSpacing;
    int nTimeSlotLength;
    int nMaxProposalPayments;

    // spork keys
    std::string strSporkPubKey;
    std::string strSporkPubKeyOld;
    int64_t nTime_EnforceNewSporkKey;
    int64_t nTime_RejectOldSporkKey;

    // height-based activations
    int height_last_invalid_UTXO;
    int height_last_ZC_AccumCheckpoint;
    int height_last_ZC_WrappedSerials;

    // validation by-pass
    int64_t nHemisBadBlockTime;
    unsigned int nHemisBadBlockBits;

    // Map with network updates
    NetworkUpgrade vUpgrades[MAX_NETWORK_UPGRADES];

    int64_t TargetTimespan(const bool fV2 = true) const { return fV2 ? nTargetTimespanV2 : nTargetTimespan; }
    uint256 ProofOfStakeLimit(const bool fV2) const { return fV2 ? posLimitV2 : posLimitV1; }
    bool MoneyRange(const CAmount& nValue) const { return (nValue >= 0 && nValue <= nMaxMoneyOut); }
    bool IsTimeProtocolV2(const int nHeight) const { return NetworkUpgradeActive(nHeight, UPGRADE_V4_0); }
    int MasternodeCollateralMinConf() const { return nMNCollateralMinConf; }

    int FutureBlockTimeDrift(const int nHeight) const
    {
        // PoS (TimeV2): 14 seconds
        if (IsTimeProtocolV2(nHeight)) return nTimeSlotLength - 1;
        // PoS (TimeV1): 3 minutes - PoW: 2 hours
        return (NetworkUpgradeActive(nHeight, UPGRADE_POS) ? nFutureTimeDriftPoS : nFutureTimeDriftPoW);
    }

    bool IsValidBlockTimeStamp(const int64_t nTime, const int nHeight) const
    {
        // Before time protocol V2, blocks can have arbitrary timestamps
        if (!IsTimeProtocolV2(nHeight)) return true;
        // Time protocol v2 requires time in slots
        return (nTime % nTimeSlotLength) == 0;
    }

    bool HasStakeMinAgeOrDepth(const int contextHeight, const uint32_t contextTime,
            const int utxoFromBlockHeight, const uint32_t utxoFromBlockTime) const
    {
        // before stake modifier V2, we require the utxo to be nStakeMinAge old
        if (!NetworkUpgradeActive(contextHeight, Consensus::UPGRADE_V3_4))
            return (utxoFromBlockTime + nStakeMinAge <= contextTime);
        // with stake modifier V2+, we require the utxo to be nStakeMinDepth deep in the chain
        return (contextHeight - utxoFromBlockHeight >= nStakeMinDepth);
    }


    /*
     * (Legacy) Zerocoin consensus params
     */
    std::string ZC_Modulus;  // parsed in Zerocoin_Params (either as hex or dec string)
    int ZC_MaxPublicSpendsPerTx;
    int ZC_MaxSpendsPerTx;
    int ZC_MinMintConfirmations;
    CAmount ZC_MinMintFee;
    int ZC_MinStakeDepth;
    int ZC_TimeStart;
    int ZC_HeightStart;

    libzerocoin::ZerocoinParams* Zerocoin_Params(bool useModulusV1) const
    {
        static CBigNum bnHexModulus = 0;
        if (!bnHexModulus) bnHexModulus.SetHex(ZC_Modulus);
        static libzerocoin::ZerocoinParams ZCParamsHex = libzerocoin::ZerocoinParams(bnHexModulus);
        static CBigNum bnDecModulus = 0;
        if (!bnDecModulus) bnDecModulus.SetDec(ZC_Modulus);
        static libzerocoin::ZerocoinParams ZCParamsDec = libzerocoin::ZerocoinParams(bnDecModulus);
        return (useModulusV1 ? &ZCParamsHex : &ZCParamsDec);
    }

    /**
     * Returns true if the given network upgrade is active as of the given block
     * height. Caller must check that the height is >= 0 (and handle unknown
     * heights).
     */
    bool NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const;

    // LLMQ
    std::map<LLMQType, LLMQParams> llmqs;
    Optional<LLMQParams> GetLLMQParams(uint8_t llmqtype) const;
    LLMQType llmqChainLocks;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
