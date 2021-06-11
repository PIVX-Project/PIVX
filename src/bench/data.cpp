// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/data.h>

namespace benchmark {
namespace data {

#include <bench/data/block2680960.raw.h>
const std::vector<uint8_t> block2680960{block2680960_raw, block2680960_raw + sizeof(block2680960_raw) / sizeof(block2680960_raw[0])};

} // namespace data
} // namespace benchmark
