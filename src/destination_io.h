// Copyright (c) 2020-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_DESTINATION_IO_H
#define PIVX_DESTINATION_IO_H

#include "chainparams.h"
#include "script/standard.h"

// Regular + shielded addresses variant.
typedef boost::variant<CTxDestination, libzcash::SaplingPaymentAddress> CWDestination;

namespace Standard {

    std::string EncodeDestination(const CWDestination &address, const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS);

    CWDestination DecodeDestination(const std::string& strAddress);
    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking, bool& isExchange);
    CWDestination DecodeDestination(const std::string& strAddress, bool& isStaking, bool& isExchange, bool& isShielded);

    bool IsValidDestination(const CWDestination& dest);

    // boost::get wrapper
    const libzcash::SaplingPaymentAddress* GetShieldedDestination(const CWDestination& dest);
    const CTxDestination * GetTransparentDestination(const CWDestination& dest);

} // End Standard namespace

/**
 * Wrapper class for every supported address
 */
class Destination {
public:
    explicit Destination() {}
    explicit Destination(const CTxDestination& _dest, bool _isP2CS) : dest(_dest), isP2CS(_isP2CS) {}
    explicit Destination(const CTxDestination& _dest, bool _isP2CS, bool _isEXCHANGE = false) : dest(_dest), isP2CS(_isP2CS), isExchange(_isEXCHANGE) {}
    explicit Destination(const libzcash::SaplingPaymentAddress& _dest) : dest(_dest) {}

    CWDestination dest{CNoDestination()};
    bool isP2CS{false};
    bool isExchange{false};

    Destination& operator=(const Destination& from);
    // Returns the key ID if Destination is a transparent "regular" destination
    const CKeyID* getKeyID();
    // Returns the encoded string address
    std::string ToString() const;
};

#endif // PIVX_DESTINATION_IO_H
