// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Based on the public domain implementation 'merged' by D. J. Bernstein
// See https://cr.yp.to/chacha.html.

#include "crypto/common.h"
#include "crypto/chacha20.h"

#include <string.h>

constexpr static inline uint32_t rotl32(uint32_t v, int c) { return (v << c) | (v >> (32 - c)); }

#define QUARTERROUND(a,b,c,d) \
  a += b; d = rotl32(d ^ a, 16); \
  c += d; b = rotl32(b ^ c, 12); \
  a += b; d = rotl32(d ^ a, 8); \
  c += d; b = rotl32(b ^ c, 7);

static const unsigned char sigma[] = "expand 32-byte k";
static const unsigned char tau[] = "expand 16-byte k";

void ChaCha20::SetKey(const unsigned char* k, size_t keylen)
{
    const unsigned char *constants;

    input[4] = ReadLE32(k + 0);
    input[5] = ReadLE32(k + 4);
    input[6] = ReadLE32(k + 8);
    input[7] = ReadLE32(k + 12);
    if (keylen == 32) { /* recommended */
        k += 16;
        constants = sigma;
    } else { /* keylen == 16 */
        constants = tau;
    }
    input[8] = ReadLE32(k + 0);
    input[9] = ReadLE32(k + 4);
    input[10] = ReadLE32(k + 8);
    input[11] = ReadLE32(k + 12);
    input[0] = ReadLE32(constants + 0);
    input[1] = ReadLE32(constants + 4);
    input[2] = ReadLE32(constants + 8);
    input[3] = ReadLE32(constants + 12);
    input[12] = 0;
    input[13] = 0;
    input[14] = 0;
    input[15] = 0;
}

ChaCha20::ChaCha20()
{
    memset(input, 0, sizeof(input));
}

ChaCha20::ChaCha20(const unsigned char* k, size_t keylen)
{
    SetKey(k, keylen);
}

void ChaCha20::SetIV(uint64_t iv)
{
    input[14] = iv;
    input[15] = iv >> 32;
}

void ChaCha20::Seek(uint64_t pos)
{
    input[12] = pos;
    input[13] = pos >> 32;
}

void ChaCha20::Output(unsigned char* c, size_t bytes)
{
    uint32_t x[16];
    uint32_t j[16];
    unsigned char *ctarget = nullptr;
    unsigned char tmp[64];
    unsigned int i;

    if (!bytes) return;

    for (uint32_t i=0; i<16; i++) {
        j[i] = input[i];
    }

    for (;;) {
        if (bytes < 64) {
            ctarget = c;
            c = tmp;
        }
        for (uint32_t i=0; i<16; i++) {
            x[i] = j[i];
        }
        for (i = 20;i > 0;i -= 2) {
            QUARTERROUND( x[0], x[4], x[8],x[12])
            QUARTERROUND( x[1], x[5], x[9],x[13])
            QUARTERROUND( x[2], x[6],x[10],x[14])
            QUARTERROUND( x[3], x[7],x[11],x[15])
            QUARTERROUND( x[0], x[5],x[10],x[15])
            QUARTERROUND( x[1], x[6],x[11],x[12])
            QUARTERROUND( x[2], x[7], x[8],x[13])
            QUARTERROUND( x[3], x[4], x[9],x[14])
        }
        for (uint32_t i=0; i<16; i++) {
            x[i] += j[i];
        }

        ++j[12];
        if (!j[12]) ++j[13];

        for (uint32_t i=0; i<16; i++) {
            WriteLE32(c + 4*i, x[i]);
        }

        if (bytes <= 64) {
            if (bytes < 64) {
                for (i = 0;i < bytes;++i) ctarget[i] = c[i];
            }
            input[12] = j[12];
            input[13] = j[13];
            return;
        }
        bytes -= 64;
        c += 64;
    }
}
