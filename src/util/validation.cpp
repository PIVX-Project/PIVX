// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util/validation.h"

#include "consensus/validation.h"
#include "tinyformat.h"

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState& state)
{
    if (state.IsValid()) {
        return "Valid";
    }

    const std::string& debug_message = state.GetDebugMessage();
    if (!debug_message.empty()) {
        return strprintf("%s, %s", state.GetRejectReason(), debug_message);
    }

    return state.GetRejectReason();
}

const std::string strMessageMagic = "DarkNet Signed Message:\n";
