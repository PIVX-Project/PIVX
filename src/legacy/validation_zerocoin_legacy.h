// Copyright (c) 2020-2021 The PIVX Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_LEGACY_VALIDATION_ZEROCOIN_LEGACY_H
#define PIVX_LEGACY_VALIDATION_ZEROCOIN_LEGACY_H

#include "amount.h"
#include "primitives/transaction.h"
#include "txdb.h" // for the zerocoinDB implementation.
#include "validationinterface.h"

bool DisconnectZerocoinTx(const CTransaction& tx);
void CacheAccChecksum(const CBlockIndex* pindex, bool fWrite);

#endif // PIVX_LEGACY_VALIDATION_ZEROCOIN_LEGACY_H
