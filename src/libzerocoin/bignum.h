// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2017-2021 The PIVX Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_LIBZEROCOIN_BIGNUM_H
#define PIVX_LIBZEROCOIN_BIGNUM_H

#if defined HAVE_CONFIG_H
#include "config/pivx-config.h"
#endif

#include <gmp.h>

#include <stdexcept>
#include <vector>
#include <limits.h>

#include "arith_uint256.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"
#include "random.h"

/** Errors thrown by the bignum class */
class bignum_error : public std::runtime_error
{
public:
    explicit bignum_error(const std::string& str) : std::runtime_error(str) {}
};

/** C++ wrapper for BIGNUM */
class CBigNum
{
    mpz_t bn;
public:
    CBigNum();
    CBigNum(const CBigNum& b);
    CBigNum& operator=(const CBigNum& b);
    ~CBigNum();

    //CBigNum(char n) is not portable.  Use 'signed char' or 'unsigned char'.
    CBigNum(signed char n);
    CBigNum(short n);
    CBigNum(int n);
    CBigNum(long n);
    CBigNum(long long n);
    CBigNum(unsigned char n);
    CBigNum(unsigned short n);
    CBigNum(unsigned int n);
    CBigNum(unsigned long n);
    CBigNum(unsigned long long n);
    explicit CBigNum(uint256 n);
    explicit CBigNum(const std::vector<unsigned char>& vch);

    /** Generates a cryptographically secure random number between zero and range exclusive
    * i.e. 0 < returned number < range
    * @param range The upper bound on the number.
    * @return
    */
    static CBigNum randBignum(const CBigNum& range);

    /**Returns the size in bits of the underlying bignum.
     *
     * @return the size
     */
    int bitSize() const;
    void setulong(unsigned long n);
    unsigned long getulong() const;
    unsigned int getuint() const;
    int getint() const;
    void setint64(int64_t sn);
    void setuint64(uint64_t n);
    void setuint256(uint256 n);
    arith_uint256 getuint256() const;
    void setvch(const std::vector<unsigned char>& vch);
    std::vector<unsigned char> getvch() const;
    void SetDec(const std::string& str);
    void SetHex(const std::string& str);
    bool SetHexBool(const std::string& str);
    std::string ToString(int nBase=10) const;
    std::string GetHex() const;
    std::string GetDec() const;

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        ::Serialize(s, getvch());
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        std::vector<unsigned char> vch;
        ::Unserialize(s, vch);
        setvch(vch);
    }

    /**
        * exponentiation with an int. this^e
        * @param e the exponent as an int
        * @return
        */
    CBigNum pow(const int e) const;

    /**
     * exponentiation this^e
     * @param e the exponent
     * @return
     */
    CBigNum pow(const CBigNum& e) const;

    /**
     * modular multiplication: (this * b) mod m
     * @param b operand
     * @param m modulus
     */
    CBigNum mul_mod(const CBigNum& b, const CBigNum& m) const;

    /**
     * modular exponentiation: this^e mod n
     * @param e exponent
     * @param m modulus
     */
    CBigNum pow_mod(const CBigNum& e, const CBigNum& m) const;

    /**
    * Calculates the inverse of this element mod m.
    * i.e. i such this*i = 1 mod m
    * @param m the modu
    * @return the inverse
    */
    CBigNum inverse(const CBigNum& m) const;

    /**
     * Calculates the greatest common divisor (GCD) of two numbers.
     * @param m the second element
     * @return the GCD
     */
    CBigNum gcd( const CBigNum& b) const;

    /**
    * Miller-Rabin primality test on this element
    * @param checks: optional, the number of Miller-Rabin tests to run
    *                          default causes error rate of 2^-80.
    * @return true if prime
    */
    bool isPrime(const int checks=15) const;

    bool isOne() const;
    bool operator!() const;
    CBigNum& operator+=(const CBigNum& b);
    CBigNum& operator-=(const CBigNum& b);
    CBigNum& operator*=(const CBigNum& b);
    CBigNum& operator/=(const CBigNum& b);
    CBigNum& operator%=(const CBigNum& b);
    CBigNum& operator<<=(unsigned int shift);
    CBigNum& operator>>=(unsigned int shift);
    CBigNum& operator++();
    const CBigNum operator++(int);
    CBigNum& operator--();
    const CBigNum operator--(int);

    friend inline const CBigNum operator+(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator-(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator/(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator%(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator*(const CBigNum& a, const CBigNum& b);
    friend inline const CBigNum operator<<(const CBigNum& a, unsigned int shift);
    friend inline const CBigNum operator-(const CBigNum& a);
    friend inline bool operator==(const CBigNum& a, const CBigNum& b);
    friend inline bool operator!=(const CBigNum& a, const CBigNum& b);
    friend inline bool operator<=(const CBigNum& a, const CBigNum& b);
    friend inline bool operator>=(const CBigNum& a, const CBigNum& b);
    friend inline bool operator<(const CBigNum& a, const CBigNum& b);
    friend inline bool operator>(const CBigNum& a, const CBigNum& b);
};

inline const CBigNum operator+(const CBigNum& a, const CBigNum& b) {
    CBigNum r;
    mpz_add(r.bn, a.bn, b.bn);
    return r;
}
inline const CBigNum operator-(const CBigNum& a, const CBigNum& b) {
    CBigNum r;
    mpz_sub(r.bn, a.bn, b.bn);
    return r;
}
inline const CBigNum operator-(const CBigNum& a) {
    CBigNum r;
    mpz_neg(r.bn, a.bn);
    return r;
}
inline const CBigNum operator*(const CBigNum& a, const CBigNum& b) {
    CBigNum r;
    mpz_mul(r.bn, a.bn, b.bn);
    return r;
}
inline const CBigNum operator/(const CBigNum& a, const CBigNum& b) {
    CBigNum r;
    mpz_tdiv_q(r.bn, a.bn, b.bn);
    return r;
}
inline const CBigNum operator%(const CBigNum& a, const CBigNum& b) {
    CBigNum r;
    mpz_mmod(r.bn, a.bn, b.bn);
    return r;
}
inline const CBigNum operator<<(const CBigNum& a, unsigned int shift) {
    CBigNum r;
    mpz_mul_2exp(r.bn, a.bn, shift);
    return r;
}
inline const CBigNum operator>>(const CBigNum& a, unsigned int shift) {
    CBigNum r = a;
    r >>= shift;
    return r;
}
inline bool operator==(const CBigNum& a, const CBigNum& b) { return (mpz_cmp(a.bn, b.bn) == 0); }
inline bool operator!=(const CBigNum& a, const CBigNum& b) { return (mpz_cmp(a.bn, b.bn) != 0); }
inline bool operator<=(const CBigNum& a, const CBigNum& b) { return (mpz_cmp(a.bn, b.bn) <= 0); }
inline bool operator>=(const CBigNum& a, const CBigNum& b) { return (mpz_cmp(a.bn, b.bn) >= 0); }
inline bool operator<(const CBigNum& a, const CBigNum& b)  { return (mpz_cmp(a.bn, b.bn) < 0); }
inline bool operator>(const CBigNum& a, const CBigNum& b)  { return (mpz_cmp(a.bn, b.bn) > 0); }

inline std::ostream& operator<<(std::ostream &strm, const CBigNum &b) { return strm << b.ToString(10); }

typedef CBigNum Bignum;

/** constant bignum instances */
const CBigNum BN_ZERO = CBigNum(0);
const CBigNum BN_ONE = CBigNum(1);
const CBigNum BN_TWO = CBigNum(2);
const CBigNum BN_THREE = CBigNum(3);

#endif // PIVX_LIBZEROCOIN_BIGNUM_H
