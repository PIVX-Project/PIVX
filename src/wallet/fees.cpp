// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/fees.h"

#include "policy/policy.h"
#include "txmempool.h"
#include "util/system.h"
#include "validation.h"
#include "wallet/wallet.h"

CAmount GetRequiredFee(unsigned int nTxBytes)
{
    return std::max(CWallet::minTxFee.GetFee(nTxBytes), ::minRelayTxFee.GetFee(nTxBytes));
}

CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool)
{
    // payTxFee is user-set "I want to pay this much"
    CAmount nFeeNeeded = payTxFee.GetFee(nTxBytes);
    // User didn't set: use -txconfirmtarget to estimate...
    if (nFeeNeeded == 0) {
        int estimateFoundTarget = (int) nConfirmTarget;
        nFeeNeeded = pool.estimateSmartFee((int) nConfirmTarget, &estimateFoundTarget).GetFee(nTxBytes);
        // ... unless we don't have enough mempool data for our desired target
        // so we make sure we're paying at least minTxFee
        if (nFeeNeeded == 0 || (unsigned int) estimateFoundTarget > nConfirmTarget)
            nFeeNeeded = std::max(nFeeNeeded, GetRequiredFee(nTxBytes));
    }
    // prevent user from paying a non-sense fee (like 1 satoshi): 0 < fee < minRelayFee
    if (nFeeNeeded < ::minRelayTxFee.GetFee(nTxBytes))
        nFeeNeeded = ::minRelayTxFee.GetFee(nTxBytes);
    // But always obey the maximum
    if (nFeeNeeded > maxTxFee)
        nFeeNeeded = maxTxFee;
    return nFeeNeeded;
}
