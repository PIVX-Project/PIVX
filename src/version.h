// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_VERSION_H
#define PIVX_VERSION_H

/**
 * network protocol versioning
 */

static const int PROTOCOL_VERSION = 70928;

//! initial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 209;

//! disconnect from peers older than this proto version
static const int MIN_PEER_PROTO_VERSION_BEFORE_ENFORCEMENT = 70926;
static const int MIN_PEER_PROTO_VERSION_AFTER_ENFORCEMENT = 70927;

//! Version where BIP155 was introduced
static const int MIN_BIP155_PROTOCOL_VERSION = 70923;

//! Version where MNAUTH was introduced
static const int MNAUTH_NODE_VER_VERSION = 70925;

//! Version where LLMQ was introduced
static const int LLMQS_PROTO_VERSION = 70928;

// Make sure that none of the values above collide with
// `ADDRV2_FORMAT`.

#endif // PIVX_VERSION_H
