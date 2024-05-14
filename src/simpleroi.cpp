// simpleroi.cpp
#include "simpleroi.h"
#include "chainparams.h"
#include "masternodeman.h"
#include <inttypes.h>
#include <univalue.h>
#include "validation.h"

const int CSimpRoiArgs::nStakeRoiHrs = 3;	// 3 hour averaging for smooth stake values

int64_t CSimpleRoi::getTimeDiff(CSimpRoiArgs& csra, uint64_t n_blocks, int nHeight)
{
    if (nHeight < n_blocks) n_blocks = nHeight;
    csra.pb0 = chainActive[nHeight - n_blocks];
// return timeDiff
    return csra.pb->GetBlockTime() - csra.pb0->GetBlockTime();
}

// returns count enabled or zero on error
//int CSimpleRoi::getsroi(float * nMnRoi, float * nStakingRoi, float * nBlocksPerDay, CAmount * mnCoins, CAmount * stakedCoins)
int CSimpleRoi::getsroi(CSimpRoiArgs& csra)
{
    csra.pb = chainActive.Tip();
    if (!csra.pb || !csra.pb->nHeight) return 0;

// calculation values
    int nHeight			= csra.pb->nHeight;				// height of tip
    int nTargetSpacing		= Params().GetConsensus().nTargetSpacing;	// MINUTES per block in seconds
    int nTimeSlotLength		= Params().GetConsensus().nTimeSlotLength;	// seconds for time slot
    int64_t nTargetTimespan	= Params().GetConsensus().TargetTimespan(nHeight);	// MINUTES in seconds to measure 'hashes per sec'
//    CAmount nMNCollateral	= CMasternode::GetMasternodeNodeCollateral(nHeight);	// masternode collateral
    CAmount nMNCollateral	= Params().GetConsensus().nMNCollateralAmt;
    CAmount nMNreward		= GetMasternodePayment(nHeight);				// masternode reward
    CAmount nBlockValue		= GetBlockValue(nHeight);				// block reward
    CAmount nStakeReward	= nBlockValue - nMNreward;
    int nMNblocks		= 2 * 1440 * 60;				// 2 days for blocks per day average

// calculate network hashes per second over ~3 hours -- set above
//                                  hours in sec / minutes in sec
    uint64_t n_blocks = csra.nStakeRoiHrs * 3600 / nTargetSpacing;
    int64_t timeDiff = getTimeDiff(csra, n_blocks, nHeight);
    if (timeDiff <= 0) return -2;	// no negative or divide by zero exceptions
    arith_uint256 workDiff = csra.pb->nChainWork - csra.pb0->nChainWork;
    int64_t nSmoothNetHashPS = (int64_t)(workDiff.getdouble() / timeDiff);
//LogPrintf("STAKE nb %d td %2" PRIu64 " smoothhash %4" PRId64 " \n", n_blocks, timeDiff, nSmoothNetHashPS);
// -----------------------------------------------------------------------
// calculate network hashes per second over TargetTimespan
    n_blocks = nTargetTimespan / nTargetSpacing;
    timeDiff = getTimeDiff(csra, n_blocks, nHeight);
    if (timeDiff <= 0) return -1;	// no negative or divide by zero exceptions
    workDiff = csra.pb->nChainWork - csra.pb0->nChainWork;
    int64_t networkHashPS = (int64_t)(workDiff.getdouble() / timeDiff);
//LogPrintf("STAKE nb %d td %2" PRIu64 " networkhsh %4" PRId64 " \n", n_blocks, timeDiff, networkHashPS);
// --------------------------------------------------------------------
// calculate total staked coins
    csra.stakedCoins = (CAmount)(networkHashPS * nTimeSlotLength / 1000000);
// calculate smoothed staked coins
    csra.smoothCoins = (CAmount)(nSmoothNetHashPS * nTimeSlotLength / 1000000);
// ----------------------------------------------------------------------------
// calculate average blocks per day
    n_blocks = nMNblocks / nTargetSpacing;
    timeDiff = getTimeDiff(csra, n_blocks, nHeight);
    if (timeDiff <= 0) return -3;	// no negative or divide by zero exceptions
    csra.nBlocksPerDay = (float)86400 * (n_blocks -1) / timeDiff;
// --------------------------------------------------------------
// calculate staking ROI -- StakedRewardsPerYear / stakedCoins
//    csra.nStakingRoi = (float)(nStakeReward * csra.nBlocksPerDay * 365 / csra.stakedCoins);
    csra.nStakingRoi = (float)(nStakeReward * csra.nBlocksPerDay * 365 / (networkHashPS * nTimeSlotLength));
// calculate smooth staking ROI
//    csra.nSmoothRoi = (float)(nStakeReward * csra.nBlocksPerDay * 365 / csra.smoothCoins);
    csra.nSmoothRoi = (float)(nStakeReward * csra.nBlocksPerDay * 365 / (nSmoothNetHashPS * nTimeSlotLength));
// -----------------------------------------------------------------------------------------------------------
// calculate total masternode collateral
    int nEnabled = mnodeman.CountEnabled();
    if (nEnabled <= 0) return 0;
    csra.mnCoins = (CAmount)(nMNCollateral * nEnabled / 100000000);
// ---------------------------------------------------------------
// calculate masternode ROI -- reward * blocks_per_day * 365 / collateral
    csra.nMnRoi = (float)(nMNreward * csra.nBlocksPerDay * 36500 / (nMNCollateral * nEnabled));
// return enabled masternodes
    return nEnabled;
}

// convert COIN to string with thousands comma seperators
std::string CSimpleRoi::CAmount2Kwithcommas(CAmount koin) {
    std::string s = strprintf("%" PRId64, (int64_t)koin);
    int j = 0;
    std::string k;

    for (int i = s.size() - 1; i >= 0;) {
        k.push_back(s[i]);
        j++;
        i--;
        if (j % 3 == 0 && i >= 0) k.push_back(',');
    }
    reverse(k.begin(), k.end());
    return k;
};


bool CSimpleRoi::generateROI(UniValue& roi, std::string& sGerror)
{
    CSimpRoiArgs csra;
    int nEnabled = getsroi(csra);
    if (nEnabled <= 0) {
        sGerror = strprintf("Not enough valid data %d", nEnabled);
         return false;
    }
    roi.pushKV(strprintf("%d hour avg ROI", csra.nStakeRoiHrs), strprintf("%4.1f%%", csra.nSmoothRoi));
    roi.pushKV(strprintf("%2d min stk ROI", Params().GetConsensus().TargetTimespan(csra.pb->nHeight) / 60), strprintf("%4.1f%%", csra.nStakingRoi));
    roi.pushKV("network  stake", CAmount2Kwithcommas(csra.smoothCoins));
    roi.pushKV("--------------","--------------");
    roi.pushKV("masternode ROI", strprintf("%4.1f%%", csra.nMnRoi));
    roi.pushKV("tot collateral", CAmount2Kwithcommas(csra.mnCoins));
    roi.pushKV("enabled  nodes", strprintf("%d", nEnabled));
    roi.pushKV("blocks per day", strprintf("%4.1f", csra.nBlocksPerDay));
    return true;
}
