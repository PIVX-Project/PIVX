// Copyright (c) 2016-2018 The Zcash developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sapling/key_io_sapling.h"

#include "utilstrencodings.h"
#include "bech32.h"
#include "script/script.h"
#include "streams.h"

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <assert.h>
#include <string.h>
#include <algorithm>

class PaymentAddressEncoder : public boost::static_visitor<std::string>
{
private:
    const CChainParams& m_params;

public:
    PaymentAddressEncoder(const CChainParams& params) : m_params(params) {}

    std::string operator()(const libzcash::SaplingPaymentAddress& zaddr) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zaddr;
        // ConvertBits requires unsigned char, but CDataStream uses char
        std::vector<unsigned char> seraddr(ss.begin(), ss.end());
        std::vector<unsigned char> data;
        // See calculation comment below
        data.reserve((seraddr.size() * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, seraddr.begin(), seraddr.end());
        return bech32::Encode(m_params.Bech32HRP(CChainParams::SAPLING_PAYMENT_ADDRESS), data);
    }

    std::string operator()(const libzcash::InvalidEncoding& no) const { return {}; }
};

class ViewingKeyEncoder : public boost::static_visitor<std::string>
{
private:
    const CChainParams& m_params;

public:
    ViewingKeyEncoder(const CChainParams& params) : m_params(params) {}

    std::string operator()(const libzcash::SaplingExtendedFullViewingKey& extfvk) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << extfvk;
        // ConvertBits requires unsigned char, but CDataStream uses char
        std::vector<unsigned char> serkey(ss.begin(), ss.end());
        std::vector<unsigned char> data;
        // See calculation comment below
        data.reserve((serkey.size() * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, serkey.begin(), serkey.end());
        std::string ret = bech32::Encode(m_params.Bech32HRP(CChainParams::SAPLING_EXTENDED_FVK), data);
        memory_cleanse(serkey.data(), serkey.size());
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    std::string operator()(const libzcash::InvalidEncoding& no) const { return {}; }
};

class SpendingKeyEncoder : public boost::static_visitor<std::string>
{
private:
    const CChainParams& m_params;

public:
    SpendingKeyEncoder(const CChainParams& params) : m_params(params) {}

    std::string operator()(const libzcash::SaplingExtendedSpendingKey& zkey) const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << zkey;
        // ConvertBits requires unsigned char, but CDataStream uses char
        std::vector<unsigned char> serkey(ss.begin(), ss.end());
        std::vector<unsigned char> data;
        // See calculation comment below
        data.reserve((serkey.size() * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, serkey.begin(), serkey.end());
        std::string ret = bech32::Encode(m_params.Bech32HRP(CChainParams::SAPLING_EXTENDED_SPEND_KEY), data);
        memory_cleanse(serkey.data(), serkey.size());
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    std::string operator()(const libzcash::InvalidEncoding& no) const { return {}; }
};

namespace KeyIO {

    // Sizes of SaplingPaymentAddress, SaplingExtendedFullViewingKey, and
    // SaplingExtendedSpendingKey after ConvertBits<8, 5, true>(). The calculations
    // below take the regular serialized size in bytes, convert to bits, and then
    // perform ceiling division to get the number of 5-bit clusters.
    const size_t ConvertedSaplingPaymentAddressSize = ((32 + 11) * 8 + 4) / 5;
    const size_t ConvertedSaplingExtendedFullViewingKeySize = (ZIP32_XFVK_SIZE * 8 + 4) / 5;
    const size_t ConvertedSaplingExtendedSpendingKeySize = (ZIP32_XSK_SIZE * 8 + 4) / 5;

    std::string EncodePaymentAddress(const libzcash::PaymentAddress& zaddr)
    {
        return boost::apply_visitor(PaymentAddressEncoder(Params()), zaddr);
    }

    libzcash::PaymentAddress DecodePaymentAddress(const std::string& str)
    {
        std::vector<unsigned char> data;
        auto bech = bech32::Decode(str);
        if (bech.first == Params().Bech32HRP(CChainParams::SAPLING_PAYMENT_ADDRESS) &&
            bech.second.size() == ConvertedSaplingPaymentAddressSize) {
            // Bech32 decoding
            data.reserve((bech.second.size() * 5) / 8);
            if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin(), bech.second.end())) {
                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                libzcash::SaplingPaymentAddress ret;
                ss >> ret;
                return ret;
            }
        }
        return libzcash::InvalidEncoding();
    }

    Optional<libzcash::SaplingPaymentAddress> DecodeSaplingPaymentAddress(const std::string& strAddress)
    {
        libzcash::PaymentAddress addr = KeyIO::DecodePaymentAddress(strAddress);
        const auto dest = boost::get<libzcash::SaplingPaymentAddress>(&addr);
        return (dest) ? Optional<libzcash::SaplingPaymentAddress>(*dest) : nullopt;
    }

    bool IsValidPaymentAddressString(const std::string& str) {
        return IsValidPaymentAddress(DecodePaymentAddress(str));
    }

    std::string EncodeViewingKey(const libzcash::ViewingKey& vk)
    {
        return boost::apply_visitor(ViewingKeyEncoder(Params()), vk);
    }

    libzcash::ViewingKey DecodeViewingKey(const std::string& str)
    {
        auto bech = bech32::Decode(str);
        if (bech.first == Params().Bech32HRP(CChainParams::SAPLING_EXTENDED_FVK) &&
            bech.second.size() == ConvertedSaplingExtendedFullViewingKeySize) {
            std::vector<unsigned char> data;
            // Bech32 decoding
            data.reserve((bech.second.size() * 5) / 8);
            if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin(), bech.second.end())) {
                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                libzcash::SaplingExtendedFullViewingKey ret;
                ss >> ret;
                memory_cleanse(data.data(), data.size());
                return ret;
            }
        }
        return libzcash::InvalidEncoding();
    }

    std::string EncodeSpendingKey(const libzcash::SpendingKey& zkey)
    {
        return boost::apply_visitor(SpendingKeyEncoder(Params()), zkey);
    }

    libzcash::SpendingKey DecodeSpendingKey(const std::string& str)
    {
        std::vector<unsigned char> data;
        auto bech = bech32::Decode(str);
        if (bech.first == Params().Bech32HRP(CChainParams::SAPLING_EXTENDED_SPEND_KEY) &&
            bech.second.size() == ConvertedSaplingExtendedSpendingKeySize) {
            // Bech32 decoding
            data.reserve((bech.second.size() * 5) / 8);
            if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin(), bech.second.end())) {
                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                libzcash::SaplingExtendedSpendingKey ret;
                ss >> ret;
                memory_cleanse(data.data(), data.size());
                return ret;
            }
        }
        memory_cleanse(data.data(), data.size());
        return libzcash::InvalidEncoding();
    }

} // KeyIO namespace
