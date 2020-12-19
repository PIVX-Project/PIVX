// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef DESTINATION_IO_H
#define DESTINATION_IO_H

#include "script/standard.h"

// Regular + shield addresses variant.
typedef boost::variant<CTxDestination, libzcash::SaplingPaymentAddress> CWDestination;

namespace Standard {

    std::string EncodeDestination(const CWDestination &address, const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS);

    CWDestination DecodeDestination(const std::string& strAddress);
    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking);
    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking, bool& isShield);

    bool IsValidDestination(const CWDestination& dest);

    // boost::get wrapper
    const libzcash::SaplingPaymentAddress* GetShieldDestination(const CWDestination& dest);
    const CTxDestination * GetTransparentDestination(const CWDestination& dest);

} // End Standard namespace

#endif //DESTINATION_IO_H
