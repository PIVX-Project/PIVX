// Copyright (c) 2017-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_BLOCKSIGNATURE_H
#define PIVX_BLOCKSIGNATURE_H

#include "key.h"
#include "keystore.h"
#include "optional.h"
#include "primitives/block.h"
#include "sapling/transaction_builder.h"

bool SignBlockWithKey(CBlock& block, const CKey& key);
bool SignBlock(CBlock& block, const CKeyStore& keystore, Optional<uint256> shieldStakeRandomness = boost::none, Optional<uint256> shieldStakePrivKey = boost::none);
bool CheckBlockSignature(const CBlock& block);

#endif //PIVX_BLOCKSIGNATURE_H
