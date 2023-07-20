// Copyright (c) 2014-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key_io.h"

#include "base58.h"
#include "script/script.h"
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <assert.h>
#include <string.h>
#include <algorithm>

namespace
{
    class DestinationEncoder : public boost::static_visitor<std::string>
    {
    private:
        const CChainParams& m_params;
        const CChainParams::Base58Type m_addrType;

    public:
        DestinationEncoder(const CChainParams& params, const CChainParams::Base58Type _addrType = CChainParams::PUBKEY_ADDRESS) : m_params(params), m_addrType(_addrType) {}

        std::string operator()(const CKeyID& id) const
        {
            std::vector<unsigned char> data = m_params.Base58Prefix(m_addrType);
            data.insert(data.end(), id.begin(), id.end());
            return EncodeBase58Check(data);
        }

        std::string operator()(const CScriptID& id) const
        {
            std::vector<unsigned char> data = m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
            data.insert(data.end(), id.begin(), id.end());
            return EncodeBase58Check(data);
        }

        std::string operator()(const CNoDestination& no) const { return ""; }
    };

    CTxDestination DecodeDestination(const std::string& str, const CChainParams& params, bool& isStaking)
    {
        std::vector<unsigned char> data;
        uint160 hash;
        if (DecodeBase58Check(str, data, 21)) {
            // base58-encoded hemis addresses.
            // Public-key-hash-addresses have version 30 (or 139 testnet).
            // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
            const std::vector<unsigned char>& pubkey_prefix = params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
            if (data.size() == hash.size() + pubkey_prefix.size() && std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
                std::copy(data.begin() + pubkey_prefix.size(), data.end(), hash.begin());
                return CKeyID(hash);
            }
            // Public-key-hash-coldstaking-addresses have version 63 (or 73 testnet).
            const std::vector<unsigned char>& staking_prefix = params.Base58Prefix(CChainParams::STAKING_ADDRESS);
            if (data.size() == hash.size() + staking_prefix.size() && std::equal(staking_prefix.begin(), staking_prefix.end(), data.begin())) {
                isStaking = true;
                std::copy(data.begin() + staking_prefix.size(), data.end(), hash.begin());
                return CKeyID(hash);
            }
            // Script-hash-addresses have version 13 (or 19 testnet).
            // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
            const std::vector<unsigned char>& script_prefix = params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
            if (data.size() == hash.size() + script_prefix.size() && std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
                std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
                return CScriptID(hash);
            }
        }
        return CNoDestination();
    }

} // anon namespace

std::string EncodeDestination(const CTxDestination& dest, bool isStaking)
{
    return EncodeDestination(dest, isStaking ? CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS);
}

std::string EncodeDestination(const CTxDestination& dest, const CChainParams::Base58Type addrType)
{
    return boost::apply_visitor(DestinationEncoder(Params(), addrType), dest);
}

CTxDestination DecodeDestination(const std::string& str)
{
    bool isStaking;
    return DecodeDestination(str, Params(), isStaking);
}

CTxDestination DecodeDestination(const std::string& str, bool& isStaking)
{
    return DecodeDestination(str, Params(), isStaking);
}

bool IsValidDestinationString(const std::string& str, bool fStaking, const CChainParams& params)
{
    bool isStaking = false;
    return IsValidDestination(DecodeDestination(str, params, isStaking)) && (isStaking == fStaking);
}

bool IsValidDestinationString(const std::string& str, bool isStaking)
{
    return IsValidDestinationString(str, isStaking, Params());
}

namespace KeyIO {

    CKey DecodeSecret(const std::string &str) {
        CKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data, 34)) {
            const std::vector<unsigned char> &privkey_prefix = Params().Base58Prefix(CChainParams::SECRET_KEY);
            if ((data.size() == 32 + privkey_prefix.size() ||
                 (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
                std::equal(privkey_prefix.begin(), privkey_prefix.end(), data.begin())) {
                bool compressed = data.size() == 33 + privkey_prefix.size();
                key.Set(data.begin() + privkey_prefix.size(), data.begin() + privkey_prefix.size() + 32, compressed);
            }
        }
        if (!data.empty()) {
            memory_cleanse(data.data(), data.size());
        }
        return key;
    }

    std::string EncodeSecret(const CKey &key) {
        assert(key.IsValid());
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::SECRET_KEY);
        data.insert(data.end(), key.begin(), key.end());
        if (key.IsCompressed()) {
            data.push_back(1);
        }
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    CExtKey DecodeExtKey(const std::string &str) {
        CExtKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data, 78)) {
            const std::vector<unsigned char> &prefix = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
            if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
                std::equal(prefix.begin(), prefix.end(), data.begin())) {
                key.Decode(data.data() + prefix.size());
            }
        }
        return key;
    }

    std::string EncodeExtKey(const CExtKey &key) {
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
        size_t size = data.size();
        data.resize(size + BIP32_EXTKEY_SIZE);
        key.Encode(data.data() + size);
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    CExtPubKey DecodeExtPubKey(const std::string& str)
    {
        CExtPubKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data, 78)) {
            const std::vector<unsigned char>& prefix = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
            if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
                key.Decode(data.data() + prefix.size());
            }
        }
        return key;
    }

    std::string EncodeExtPubKey(const CExtPubKey& key)
    {
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
        size_t size = data.size();
        data.resize(size + BIP32_EXTKEY_SIZE);
        key.Encode(data.data() + size);
        std::string ret = EncodeBase58Check(data);
        return ret;
    }

}// namespace