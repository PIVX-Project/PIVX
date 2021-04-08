// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
//Copyright (c) 2015-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Sombe_UINT256_H
#define Sombe_UINT256_H

#include "arith_uint256.h"
#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

//
// This is a migration file class, as soon as we move every
// uint256 field used, invalidly, as a number to the proper arith_uint256, this will be replaced
// with the blob_uint256 file.
//

/** 160-bit unsigned big integer. */
class uint160 : public base_uint<160>
{
public:
    uint160() {}
    uint160(const base_uint<160>& b) : base_uint<160>(b) {}
    uint160(uint64_t b) : base_uint<160>(b) {}
    explicit uint160(const std::string& str) : base_uint<160>(str) {}
    explicit uint160(const std::vector<unsigned char>& vch) : base_uint<160>(vch) {}
};

/** 256-bit unsigned big integer. */
class uint256 : public base_uint<256>
{
public:
    uint256() {}
    uint256(const base_uint<256>& b) : base_uint<256>(b) {}
    uint256(uint64_t b) : base_uint<256>(b) {}
    explicit uint256(const std::string& str) : base_uint<256>(str) {}
    explicit uint256(const std::vector<unsigned char>& vch) : base_uint<256>(vch) {}

    /**
     * The "compact" format is a representation of a whole
     * number N using an unsigned 32bit number similar to a
     * floating point format.
     * The most significant 8 bits are the unsigned exponent of base 256.
     * This exponent can be thought of as "number of bytes of N".
     * The lower 23 bits are the mantissa.
     * Bit number 24 (0x800000) represents the sign of N.
     * N = (-1^sign) * mantissa * 256^(exponent-3)
     *
     * Satoshi's original implementation used BN_bn2mpi() and BN_mpi2bn().
     * MPI uses the most significant bit of the first byte as sign.
     * Thus 0x1234560000 is compact (0x05123456)
     * and  0xc0de000000 is compact (0x0600c0de)
     *
     * Bitcoin only uses this "compact" format for encoding difficulty
     * targets, which are unsigned 256bit quantities.  Thus, all the
     * complexities of the sign bit and using base 256 are probably an
     * implementation accident.
     */
    uint256& SetCompact(uint32_t nCompact, bool* pfNegative = nullptr, bool* pfOverflow = nullptr);
    uint32_t GetCompact(bool fNegative = false) const;
    uint64_t GetHash(const uint256& salt) const;
};

/** 512-bit unsigned big integer. */
class uint512 : public base_uint<512>
{
public:
    uint512() {}
    uint512(const base_uint<512>& b) : base_uint<512>(b) {}
    uint512(uint64_t b) : base_uint<512>(b) {}
    explicit uint512(const std::string& str) : base_uint<512>(str) {}
    explicit uint512(const std::vector<unsigned char>& vch) : base_uint<512>(vch) {}

    uint256 trim256() const
    {
        uint256 ret;
        for (unsigned int i = 0; i < uint256::WIDTH; i++) {
            ret.pn[i] = pn[i];
        }
        return ret;
    }

    friend arith_uint512 UintToArith512(const uint512 &a);
    friend uint512 ArithToUint512(const arith_uint512 &a);
};

/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching UINT256_ZERO.
 */
inline uint256 uint256S(const char* str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}
/* uint256 from std::string.
 * This is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching UINT256_ZERO via std::string(const char*).
 */
inline uint256 uint256S(const std::string& str)
{
    uint256 rv;
    rv.SetHex(str);
    return rv;
}

inline uint512 uint512S(const std::string& str)
{
    uint512 rv;
    rv.SetHex(str);
    return rv;
}

uint256 ArithToUint256(const arith_uint256 &);
arith_uint256 UintToArith256(const uint256 &);
uint512 ArithToUint512(const arith_uint512 &);
arith_uint512 UintToArith512(const uint512 &);

/** constant uint256 instances */
const uint256 UINT256_ZERO = uint256();
const uint256 UINT256_ONE = uint256("0000000000000000000000000000000000000000000000000000000000000001");

#endif // Sombe_UINT256_H
