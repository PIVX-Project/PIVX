#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Encode and decode BASE58, P2PKH and P2SH addresses."""

from .script import hash256, hash160, sha256, CScript, OP_0
from .util import hex_str_to_bytes

# --- Base58Check encoding
__b58chars = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
__b58base = len(__b58chars)
b58chars = __b58chars

long = int
_bchr = lambda x: bytes([x])
_bord = lambda x: x

def b58decode(v, length=None):
    """ decode v into a string of len bytes
    """
    long_value = 0
    for (i, c) in enumerate(v[::-1]):
        long_value += __b58chars.find(c) * (__b58base**i)

    result = bytes()
    while long_value >= 256:
        div, mod = divmod(long_value, 256)
        result = _bchr(mod) + result
        long_value = div
    result = _bchr(long_value) + result

    nPad = 0
    for c in v:
        if c == __b58chars[0]:
            nPad += 1
        else:
            break

    result = _bchr(0) * nPad + result
    if length is not None and len(result) != length:
        return None

    return result

def b58encode(v):
    """
    encode v, which is a string of bytes, to base58.
    """
    long_value = 0
    for (i, c) in enumerate(v[::-1]):
        long_value += (256**i) * _bord(c)

    result = ''
    while long_value >= __b58base:
        div, mod = divmod(long_value, __b58base)
        result = __b58chars[mod] + result
        long_value = div
    result = __b58chars[long_value] + result
    # Bitcoin does a little leading-zero-compression:
    # leading 0-bytes in the input become leading-1s
    nPad = 0
    for c in v:
        #        if c == '\0': nPad += 1
        if c == 0:
            nPad += 1
        else:
            break

    return (__b58chars[0] * nPad) + result

WIF_PREFIX = 212            #d4
TESTNET_WIF_PREFIX = 239    #ef

def wif_to_privkey(secretString):
    wif_compressed = 52 == len(secretString)
    pvkeyencoded = b58decode(secretString).hex()
    wifversion = pvkeyencoded[:2]
    checksum = pvkeyencoded[-8:]
    vs = bytes.fromhex(pvkeyencoded[:-8])
    check = hash256(vs)[0:4]

    if (wifversion == WIF_PREFIX.to_bytes(1, byteorder='big').hex() and checksum == check.hex()) \
            or (wifversion == TESTNET_WIF_PREFIX.to_bytes(1, byteorder='big').hex() and checksum == check.hex()):

        if wif_compressed:
            privkey = pvkeyencoded[2:-10]

        else:
            privkey = pvkeyencoded[2:-8]

        return privkey, wif_compressed

    else:
        return None

PK_ADD_VERSION = 30
COLD_ADD_VERSION = 63
TNET_PK_ADD_VERSION = 139
TNET_COLD_ADD_VERSION = 73
SCRIPT_VERSION = 13
TNET_SCRIPT_VERSION = 19

def byte_to_base58(b, version):
    data = bytes([version]) + b
    checksum = hash256(data)[0:4]
    return b58encode(data + checksum)

def keyhash_to_p2pkh(hash, main=False, isCold=False):
    assert (len(hash) == 20)
    if isCold:
        version = COLD_ADD_VERSION if main else TNET_COLD_ADD_VERSION
    else:
        version = PK_ADD_VERSION if main else TNET_PK_ADD_VERSION
    return byte_to_base58(hash, version)

def scripthash_to_p2sh(hash, main = False):
    assert (len(hash) == 20)
    version = SCRIPT_VERSION if main else TNET_SCRIPT_VERSION
    return byte_to_base58(hash, version)

def key_to_p2pkh(key, main = False, isCold=False):
    key = check_key(key)
    return keyhash_to_p2pkh(hash160(key), main, isCold)

def script_to_p2sh(script, main = False):
    script = check_script(script)
    return scripthash_to_p2sh(hash160(script), main)

def key_to_p2sh_p2wpkh(key, main = False):
    key = check_key(key)
    p2shscript = CScript([OP_0, hash160(key)])
    return script_to_p2sh(p2shscript, main)

def script_to_p2sh_p2wsh(script, main = False):
    script = check_script(script)
    p2shscript = CScript([OP_0, sha256(script)])
    return script_to_p2sh(p2shscript, main)

def check_key(key):
    if (type(key) is str):
        key = hex_str_to_bytes(key) # Assuming this is hex string
    if (type(key) is bytes and (len(key) == 33 or len(key) == 65)):
        return key
    assert(False)

def check_script(script):
    if (type(script) is str):
        script = hex_str_to_bytes(script) # Assuming this is hex string
    if (type(script) is bytes or type(script) is CScript):
        return script
    assert(False)
