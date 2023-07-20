#!/usr/bin/env python3
# Copyright (c) 2017 Pieter Wuille
# Copyright (c) 2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reference implementation for Bech32 encoding/decoding"""

CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"
BECH32_CONST = 1

def bech32_polymod(values):
    """Internal function that computes the Bech32 checksum."""
    generator = [0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3]
    chk = 1
    for value in values:
        top = chk >> 25
        chk = (chk & 0x1ffffff) << 5 ^ value
        for i in range(5):
            chk ^= generator[i] if ((top >> i) & 1) else 0
    return chk

def bech32_hrp_expand(hrp):
    """Expand the HRP into values for checksum computation."""
    return [ord(x) >> 5 for x in hrp] + [0] + [ord(x) & 31 for x in hrp]

def bech32_verify_checksum(hrp, data):
    """Verify a checksum given HRP and converted data characters."""
    return bech32_polymod(bech32_hrp_expand(hrp) + data) == BECH32_CONST

def bech32_create_checksum(hrp, data):
    """Compute the checksum values given HRP and data."""
    values = bech32_hrp_expand(hrp) + data
    polymod = bech32_polymod(values + [0, 0, 0, 0, 0, 0]) ^ BECH32_CONST
    return [(polymod >> 5 * (5 - i)) & 31 for i in range(6)]

def bech32_encode(hrp, data):
    """Compute a Bech32 string given HRP and data values."""
    combined = data + bech32_create_checksum(hrp, data)
    return hrp + '1' + ''.join([CHARSET[d] for d in combined])

def bech32_decode(bech):
    """Validate a Bech32 string, and determine HRP and data."""
    if ((any(ord(x) < 33 or ord(x) > 126 for x in bech)) or
            (bech.lower() != bech and bech.upper() != bech)):
        return None, None
    bech = bech.lower()
    pos = bech.rfind('1')
    if pos < 1 or pos + 7 > len(bech) or len(bech) > 95:
        return None, None
    if not all(x in CHARSET for x in bech[pos+1:]):
        return None, None
    hrp = bech[:pos]
    data = [CHARSET.find(x) for x in bech[pos+1:]]
    if not bech32_verify_checksum(hrp, data):
        return None, None
    return hrp, data[:-6]

def convertbits(data, frombits, tobits, pad=True):
    """General power-of-2 base conversion."""
    acc = 0
    bits = 0
    ret = []
    maxv = (1 << tobits) - 1
    max_acc = (1 << (frombits + tobits - 1)) - 1
    for value in data:
        if value < 0 or (value >> frombits):
            return None
        acc = ((acc << frombits) | value) & max_acc
        bits += frombits
        while bits >= tobits:
            bits -= tobits
            ret.append((acc >> bits) & maxv)
    if pad:
        if bits:
            ret.append((acc << (tobits - bits)) & maxv)
    elif bits >= frombits or ((acc << (tobits - bits)) & maxv):
        return None
    return ret

def bech32_str_to_bytes(bech_str):
    _, data = bech32_decode(bech_str)
    if data is None:
        return None
    decoded = convertbits(data, 5, 8, False)
    if decoded is None:
        return None
    return bytes(decoded)



if __name__ == '__main__':
    def test_bls_bech32(bech32_str, hrp, bytes_data):
        # test Bech32 encoding/decoding for BLS secret keys
        h, d = bech32_decode(bech32_str)
        assert h == hrp
        assert bech32_str == bech32_encode(h, d)
        assert bech32_str_to_bytes(bech32_str) == bytes_data
        assert bech32_decode(bech32_str + CHARSET[0]) == (None, None)   # add 1 char
        assert bech32_decode(bech32_str[:-1]) == (None, None)           # remove 1 char
        bech32_str = bech32_str[:-1] + CHARSET[(CHARSET.index(bech32_str[-1]) + 1) % len(CHARSET)]
        assert bech32_decode(bech32_str) == (None, None)                # change 1 char

    # BLS secret key
    sk_bls = "bls-sk-test1y8z22ksxta8hwqh9r6jsyc42frqft43t2uav6s0vqrjjf67metnq2mwkm0"
    sk = b'!\xc4\xa5Z\x06_Ow\x02\xe5\x1e\xa5\x02b\xaaH\xc0\x95\xd6+W:\xcdA\xec\x00\xe5$\xeb\xdb\xca\xe6'
    test_bls_bech32(sk_bls, "bls-sk-test", sk)

    # BLS public key
    pk_bls = "bls-pk-test132ngslan8s5ha4k74wynyez3drzkw3fa2d60676ch3ac6gzx6ttd0xg8pqj0x5n5tgajvuycqmgtgnsep90"
    pk = b'\x8a\xa6\x88\x7f\xb3<)~\xd6\xde\xab\x892dQh\xc5gE=St\xfd{X\xbc{\x8d F\xd2\xd6\xd7\x99\x07\x08$\xf3RtZ;&p\x98\x06\xd0\xb4'
    test_bls_bech32(pk_bls, "bls-pk-test", pk)

    print("All good.")
