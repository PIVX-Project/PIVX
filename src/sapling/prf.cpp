// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "prf.h"
#include "crypto/sha256.h"
#include "hash.h"

#include <array>
#include <sodium.h>
#include <librustzcash.h>

const unsigned char ZCASH_EXPANDSEED_PERSONALIZATION[crypto_generichash_blake2b_PERSONALBYTES] = {'Z','c','a','s','h','_','E','x','p','a','n','d','S','e','e','d'};

// Sapling
std::array<unsigned char, 64> PRF_expand(const uint256& sk, unsigned char t)
{
    std::array<unsigned char, 64> res;
    unsigned char blob[33];

    memcpy(&blob[0], sk.begin(), 32);
    blob[32] = t;

    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init_salt_personal(&state, nullptr, 0, 64, nullptr, ZCASH_EXPANDSEED_PERSONALIZATION);
    crypto_generichash_blake2b_update(&state, blob, 33);
    crypto_generichash_blake2b_final(&state, res.data(), 64);

    return res;
}

uint256 PRF_ask(const uint256& sk)
{
    uint256 ask;
    auto tmp = PRF_expand(sk, 0);
    librustzcash_to_scalar(tmp.data(), ask.begin());
    return ask;
}

uint256 PRF_nsk(const uint256& sk)
{
    uint256 nsk;
    auto tmp = PRF_expand(sk, 1);
    librustzcash_to_scalar(tmp.data(), nsk.begin());
    return nsk;
}

uint256 PRF_ovk(const uint256& sk)
{
    uint256 ovk;
    auto tmp = PRF_expand(sk, 2);
    memcpy(ovk.begin(), tmp.data(), 32);
    return ovk;
}

std::array<unsigned char, 11> default_diversifier(const uint256& sk)
{
    std::array<unsigned char, 11> res;
    unsigned char blob[34];

    memcpy(&blob[0], sk.begin(), 32);
    blob[32] = 3;

    blob[33] = 0;
    while (true) {
        crypto_generichash_blake2b_state state;
        crypto_generichash_blake2b_init_salt_personal(&state, nullptr, 0, 64, nullptr, ZCASH_EXPANDSEED_PERSONALIZATION);
        crypto_generichash_blake2b_update(&state, blob, 34);
        crypto_generichash_blake2b_final(&state, res.data(), 11);

        if (librustzcash_check_diversifier(res.data())) {
            break;
        } else if (blob[33] == 255) {
            throw std::runtime_error("librustzcash_check_diversifier did not return valid diversifier");
        }
        blob[33] += 1;
    }

    return res;
}
