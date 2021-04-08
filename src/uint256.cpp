// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
//Copyright (c) 2017-2019 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uint256.h"
#include "crypto/common.h"

/** Old classes definitions **/

// This implementation directly uses shifts instead of going
// through an intermediate MPI representation.
uint256& uint256::SetCompact(uint32_t nCompact, bool* pfNegative, bool* pfOverflow)
{
    int nSize = nCompact >> 24;
    uint32_t nWord = nCompact & 0x007fffff;
    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
        *this = nWord;
    } else {
        *this = nWord;
        *this <<= 8 * (nSize - 3);
    }
    if (pfNegative)
        *pfNegative = nWord != 0 && (nCompact & 0x00800000) != 0;
    if (pfOverflow)
        *pfOverflow = nWord != 0 && ((nSize > 34) ||
                                     (nWord > 0xff && nSize > 33) ||
                                     (nWord > 0xffff && nSize > 32));
    return *this;
}

uint32_t uint256::GetCompact(bool fNegative) const
{
    int nSize = (bits() + 7) / 8;
    uint32_t nCompact = 0;
    if (nSize <= 3) {
        nCompact = GetLow64() << 8 * (3 - nSize);
    } else {
        uint256 bn = *this >> 8 * (nSize - 3);
        nCompact = bn.GetLow64();
    }
    // The 0x00800000 bit denotes the sign.
    // Thus, if it is already set, divide the mantissa by 256 and increase the exponent.
    if (nCompact & 0x00800000) {
        nCompact >>= 8;
        nSize++;
    }
    assert((nCompact & ~0x007fffff) == 0);
    assert(nSize < 256);
    nCompact |= nSize << 24;
    nCompact |= (fNegative && (nCompact & 0x007fffff) ? 0x00800000 : 0);
    return nCompact;
}

static void inline HashMix(uint32_t& a, uint32_t& b, uint32_t& c)
{
    // Taken from lookup3, by Bob Jenkins.
    a -= c;
    a ^= ((c << 4) | (c >> 28));
    c += b;
    b -= a;
    b ^= ((a << 6) | (a >> 26));
    a += c;
    c -= b;
    c ^= ((b << 8) | (b >> 24));
    b += a;
    a -= c;
    a ^= ((c << 16) | (c >> 16));
    c += b;
    b -= a;
    b ^= ((a << 19) | (a >> 13));
    a += c;
    c -= b;
    c ^= ((b << 4) | (b >> 28));
    b += a;
}

static void inline HashFinal(uint32_t& a, uint32_t& b, uint32_t& c)
{
    // Taken from lookup3, by Bob Jenkins.
    c ^= b;
    c -= ((b << 14) | (b >> 18));
    a ^= c;
    a -= ((c << 11) | (c >> 21));
    b ^= a;
    b -= ((a << 25) | (a >> 7));
    c ^= b;
    c -= ((b << 16) | (b >> 16));
    a ^= c;
    a -= ((c << 4) | (c >> 28));
    b ^= a;
    b -= ((a << 14) | (a >> 18));
    c ^= b;
    c -= ((b << 24) | (b >> 8));
}

uint64_t uint256::GetHash(const uint256& salt) const
{
    uint32_t a, b, c;
    a = b = c = 0xdeadbeef + (WIDTH << 2);

    a += pn[0] ^ salt.pn[0];
    b += pn[1] ^ salt.pn[1];
    c += pn[2] ^ salt.pn[2];
    HashMix(a, b, c);
    a += pn[3] ^ salt.pn[3];
    b += pn[4] ^ salt.pn[4];
    c += pn[5] ^ salt.pn[5];
    HashMix(a, b, c);
    a += pn[6] ^ salt.pn[6];
    b += pn[7] ^ salt.pn[7];
    HashFinal(a, b, c);

    return ((((uint64_t)b) << 32) | c);
}

uint256 ArithToUint256(const arith_uint256 &a)
{
    uint256 b;
    for(int x=0; x<a.WIDTH; ++x)
        WriteLE32(b.begin() + x*4, a.pn[x]);
    return b;
}
arith_uint256 UintToArith256(const uint256 &a)
{
    arith_uint256 b;
    for(int x=0; x<b.WIDTH; ++x)
        b.pn[x] = ReadLE32(a.begin() + x*4);
    return b;
}

uint512 ArithToUint512(const arith_uint512 &a)
{
    uint512 b;
    for(int x=0; x<a.WIDTH; ++x)
        WriteLE32(b.begin() + x*4, a.pn[x]);
    return b;
}

arith_uint512 UintToArith512(const uint512 &a)
{
    arith_uint512 b;
    for(int x=0; x<b.WIDTH; ++x)
        b.pn[x] = ReadLE32(a.begin() + x*4);
    return b;
}
