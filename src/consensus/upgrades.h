// Copyright (c) 2018 The Zcash developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_CONSENSUS_UPGRADES_H
#define hemis_CONSENSUS_UPGRADES_H

#include "consensus/params.h"
#include "optional.h"

enum UpgradeState {
    UPGRADE_DISABLED,
    UPGRADE_PENDING,
    UPGRADE_ACTIVE
};

struct NUInfo {
    /** User-facing name for the upgrade */
    std::string strName;
    /** User-facing information string about the upgrade */
    std::string strInfo;
};

extern const struct NUInfo NetworkUpgradeInfo[];

/**
 * Checks the state of a given network upgrade based on block height.
 * Caller must check that the height is >= 0 (and handle unknown heights).
 */
UpgradeState NetworkUpgradeState(
        int nHeight,
        const Consensus::Params& params,
        Consensus::UpgradeIndex idx);

/**
 * Returns true if the given network upgrade is active as of the given block
 * height. Caller must check that the height is >= 0 (and handle unknown
 * heights).
 */
bool NetworkUpgradeActive(
        int nHeight,
        const Consensus::Params& params,
        Consensus::UpgradeIndex idx);

/**
 * Returns the index of the most recent upgrade as of the given block height
 * (corresponding to the current "epoch"). Consensus::BASE_NETWORK is the
 * default value if no upgrades are active. Caller must check that the height
 * is >= 0 (and handle unknown heights).
 */
int CurrentEpoch(int nHeight, const Consensus::Params& params);

/**
 * Returns true if the given block height is the activation height for the given
 * upgrade.
 */
bool IsActivationHeight(
        int nHeight,
        const Consensus::Params& params,
        Consensus::UpgradeIndex upgrade);

/**
 * Returns true if the given block height is the activation height for any upgrade.
 */
bool IsActivationHeightForAnyUpgrade(
        int nHeight,
        const Consensus::Params& params);

/**
 * Returns the index of the next upgrade after the given block height, or
 * nullopt if there are no more known upgrades.
 */
Optional<int> NextEpoch(int nHeight, const Consensus::Params& params);

/**
 * Returns the activation height for the next upgrade after the given block height,
 * or nullopt if there are no more known upgrades.
 */
Optional<int> NextActivationHeight(
        int nHeight,
        const Consensus::Params& params);

#endif // hemis_CONSENSUS_UPGRADES_H
