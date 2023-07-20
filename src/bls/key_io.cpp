// Copyright (c) 2022 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "bls/key_io.h"

#include "chainparams.h"
#include "bech32.h"
#include "bls/bls_wrapper.h"

namespace bls {

template<typename BLSKey>
static std::string EncodeBLS(const CChainParams& params,
                             const BLSKey& key,
                             CChainParams::Bech32Type type)
{
    if (!key.IsValid()) return "";
    std::vector<unsigned char> vec{key.ToByteVector()};
    std::vector<unsigned char> data;
    data.reserve((vec.size() * 8 + 4) / 5);
    ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, vec.begin(), vec.end());
    auto res = bech32::Encode(params.Bech32HRP(type), data);
    memory_cleanse(vec.data(), vec.size());
    memory_cleanse(data.data(), data.size());
    return res;
}

template<typename BLSKey>
static Optional<BLSKey> DecodeBLS(const CChainParams& params,
                                  const std::string& keyStr,
                                  CChainParams::Bech32Type type,
                                  unsigned int keySize)
{
    if (keyStr.empty()) return nullopt;
    auto bech = bech32::Decode(keyStr);
    if (bech.first == params.Bech32HRP(type) && bech.second.size() == keySize) {
        std::vector<unsigned char> data;
        data.reserve((bech.second.size() * 5) / 8);
        if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin(), bech.second.end())) {
            CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
            BLSKey key;
            ss >> key;
            if (key.IsValid()) return {key};
        }
    }
    return nullopt;
}

std::string EncodeSecret(const CChainParams& params, const CBLSSecretKey& key)
{
    return EncodeBLS<CBLSSecretKey>(params, key, CChainParams::BLS_SECRET_KEY);
}

std::string EncodePublic(const CChainParams& params, const CBLSPublicKey& pk)
{
    return EncodeBLS<CBLSPublicKey>(params, pk, CChainParams::BLS_PUBLIC_KEY);
}

const size_t ConvertedBlsSkSize = (BLS_CURVE_SECKEY_SIZE * 8 + 4) / 5;
const size_t ConvertedBlsPkSize = (BLS_CURVE_PUBKEY_SIZE * 8 + 4) / 5;

Optional<CBLSSecretKey> DecodeSecret(const CChainParams& params, const std::string& keyStr)
{
    return DecodeBLS<CBLSSecretKey>(params, keyStr, CChainParams::BLS_SECRET_KEY, ConvertedBlsSkSize);
}

Optional<CBLSPublicKey> DecodePublic(const CChainParams& params, const std::string& keyStr)
{
    return DecodeBLS<CBLSPublicKey>(params, keyStr, CChainParams::BLS_PUBLIC_KEY, ConvertedBlsPkSize);
}

} // end bls namespace
