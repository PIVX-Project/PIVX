// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_OPTIONAL_H
#define PIVX_OPTIONAL_H

#include <boost/optional.hpp>

//! Substitute for C++17 std::optional
template <typename T>
using Optional = boost::optional<T>;

//! Substitute for C++17 std::nullopt
static auto& nullopt = boost::none;

#endif // PIVX_OPTIONAL_H
