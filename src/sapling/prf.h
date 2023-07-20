// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
// Zcash uses SHA256Compress as a PRF for various components
// within the zkSNARK circuit.
*/

#ifndef ZC_PRF_H_
#define ZC_PRF_H_

#include "uint256.h"

#include <array>

//! Sapling functions
uint256 PRF_ask(const uint256& sk);
uint256 PRF_nsk(const uint256& sk);
uint256 PRF_ovk(const uint256& sk);

std::array<unsigned char, 11> default_diversifier(const uint256& sk);

#endif // ZC_PRF_H_
