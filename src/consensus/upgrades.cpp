// Copyright (c) 2018 The Zcash developers
// Copyright (c) 2020-2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/upgrades.h"

/**
 * General information about each network upgrade.
 * Ordered by Consensus::UpgradeIndex.
 *
 * If the upgrade name has many words, use the '_' character to divide them.
 * We are using it in the -nuparams startup arg and input it with spaces is just ugly.
 */
const struct NUInfo NetworkUpgradeInfo[Consensus::MAX_NETWORK_UPGRADES] = {
        {
                /*.strName =*/ "Base",
                /*.strInfo =*/ "hemis network",
        },
        {
                /*.strName =*/ "PoS",
                /*.strInfo =*/ "Proof of Stake Consensus activation",
        },
        {
                /*.strName =*/ "PoS_v2",
                /*.strInfo =*/ "New selection for stake modifier",
        },
        {
                /*.strName =*/ "Zerocoin",
                /*.strInfo =*/ "ZeroCoin protocol activation - start block v4",
        },
        {
                /*.strName =*/ "Zerocoin_v2",
                /*.strInfo =*/ "New zerocoin serials and zPOS start",
        },
        {
                /*.strName =*/ "BIP65",
                /*.strInfo =*/ "CLTV (BIP65) activation - start block v5",
        },
        {
                /*.strName =*/ "Zerocoin_Public",
                /*.strInfo =*/ "Activation of zerocoin public spends (spend v3)",
        },
        {
                /*.strName =*/ "hemis_v3.4",
                /*.strInfo =*/ "New 256-bit stake modifier - start block v6",
        },
        {
                /*.strName =*/ "hemis_v4.0",
                /*.strInfo =*/ "New message sigs - start block v7 - time protocol - zc spend v4",
        },
        {
                /*.strName =*/ "v5_shield",
                /*.strInfo =*/ "Sapling Shield - start block v8 - start transaction v3",
        },
        {
                /*.strName =*/ "hemis_v5.2",
                /*.strInfo =*/ "New cold-staking rules",
        },
        {
                /*.strName =*/ "hemis_v5.3",
                /*.strInfo =*/ "New staking rules",
        },
        {
                /*.strName =*/ "hemis_v5.5",
                /*.strInfo =*/ "New rewards structure",
        },
        {
                /*.strName =*/ "v6_evo",
                /*.strInfo =*/ "Deterministic Masternodes",
        },
        {
                /*.strName =*/ "Test_dummy",
                /*.strInfo =*/ "Test dummy info",
        },
};

UpgradeState NetworkUpgradeState(
        int nHeight,
        const Consensus::Params& params,
        Consensus::UpgradeIndex idx)
{
    assert(nHeight >= 0);
    assert(idx >= Consensus::BASE_NETWORK && idx < Consensus::MAX_NETWORK_UPGRADES);
    auto nActivationHeight = params.vUpgrades[idx].nActivationHeight;

    if (nActivationHeight == Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT) {
        return UPGRADE_DISABLED;
    } else if (nHeight >= nActivationHeight) {
        // From ZIP200:
        //
        // ACTIVATION_HEIGHT
        //     The block height at which the network upgrade rules will come into effect.
        //
        //     For removal of ambiguity, the block at height ACTIVATION_HEIGHT - 1 is
        //     subject to the pre-upgrade consensus rules.
        return UPGRADE_ACTIVE;
    } else {
        return UPGRADE_PENDING;
    }
}

bool NetworkUpgradeActive(
        int nHeight,
        const Consensus::Params& params,
        Consensus::UpgradeIndex idx)
{
    return NetworkUpgradeState(nHeight, params, idx) == UPGRADE_ACTIVE;
}

int CurrentEpoch(int nHeight, const Consensus::Params& params) {
    for (auto idxInt = Consensus::MAX_NETWORK_UPGRADES - 1; idxInt > Consensus::BASE_NETWORK; idxInt--) {
        if (NetworkUpgradeActive(nHeight, params, Consensus::UpgradeIndex(idxInt))) {
            return idxInt;
        }
    }
    return Consensus::BASE_NETWORK;
}

bool IsActivationHeight(
        int nHeight,
        const Consensus::Params& params,
        Consensus::UpgradeIndex idx)
{
    assert(idx >= Consensus::BASE_NETWORK && idx < Consensus::MAX_NETWORK_UPGRADES);

    // Don't count BASE_NETWORK as an activation height
    if (idx == Consensus::BASE_NETWORK) {
        return false;
    }

    return nHeight >= 0 && nHeight == params.vUpgrades[idx].nActivationHeight;
}

bool IsActivationHeightForAnyUpgrade(
        int nHeight,
        const Consensus::Params& params)
{
    if (nHeight < 0) {
        return false;
    }

    for (int idx = Consensus::BASE_NETWORK + 1; idx < (int) Consensus::MAX_NETWORK_UPGRADES; idx++) {
        if (nHeight == params.vUpgrades[idx].nActivationHeight)
            return true;
    }

    return false;
}

Optional<int> NextEpoch(int nHeight, const Consensus::Params& params) {
    if (nHeight < 0) {
        return nullopt;
    }

    // BASE_NETWORK is never pending
    for (auto idx = Consensus::BASE_NETWORK + 1; idx < Consensus::MAX_NETWORK_UPGRADES; idx++) {
        if (NetworkUpgradeState(nHeight, params, Consensus::UpgradeIndex(idx)) == UPGRADE_PENDING) {
            return idx;
        }
    }

    return nullopt;
}

Optional<int> NextActivationHeight(
        int nHeight,
        const Consensus::Params& params)
{
    auto idx = NextEpoch(nHeight, params);
    if (idx) {
        return params.vUpgrades[idx.get()].nActivationHeight;
    }
    return nullopt;
}
