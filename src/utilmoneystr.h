// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Money parsing/formatting utilities.
 */
#ifndef PIVX_UTILMONEYSTR_H
#define PIVX_UTILMONEYSTR_H

#include <stdint.h>
#include <string>

#include "amount.h"

std::string FormatMoney(const CAmount& n, bool fPlus = false);
bool ParseMoney(const std::string& str, CAmount& nRet);
bool ParseMoney(const char* pszIn, CAmount& nRet);

#endif // PIVX_UTILMONEYSTR_H
