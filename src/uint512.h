//Copyright (c) 2017-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Sombe_UINT512_H
#define Sombe_UINT512_H

#include "arith_uint256.h"
#include "blob_uint256.h"

/** 512-bit unsigned big integer. */
class blob_uint512 : public base_blob<512>
{
public:
    blob_uint512() {}
    blob_uint512(const base_blob<512>& b) : base_blob<512>(b) {}
    explicit blob_uint512(const std::vector<unsigned char>& vch) : base_blob<512>(vch) {}

    blob_uint256 trim256() const
    {
        std::vector<unsigned char> vch;
        const unsigned char* p = this->begin();
        for (unsigned int i = 0; i < 32; i++) {
            vch.push_back(*p++);
        }
        return blob_uint256(vch);
    }
};


/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching UINT256_ZERO.
 */
inline blob_uint512 blob_uint512S(const char* str)
{
    blob_uint512 rv;
    rv.SetHex(str);
    return rv;
}

#endif // Sombe_UINT512_H
