// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_WALLET_WALLETUTIL_H
#define PIVX_WALLET_WALLETUTIL_H

#include "fs.h"
#include "operationresult.h"

//! Get the path of the wallet directory.
fs::path GetWalletDir();
//! Verify the wallet db's path
OperationResult VerifyWalletPath(const std::string& walletFile);

#endif // PIVX_WALLET_WALLETUTIL_H
