// Copyright (c) 2013 The Bitcoin Core developers
// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"
#include <boost/test/unit_test.hpp>
#include "wallet/wallet.h"
#include "libzerocoin/bignum.h"

BOOST_FIXTURE_TEST_SUITE(signcompact_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(low_r_malleability_test)
{
    //const int NUM_TESTS = 1;

    // get a key from the keystore
    CBasicKeyStore keystore;
    CKey key;
    key.MakeNewKey(true);
    keystore.AddKey(key);
    CPubKey pubKey = key.GetPubKey();

    // get a random message
    uint256 msgHash = CBigNum::randKBitBignum(256).getuint256();

    // sign it
    std::vector<unsigned char> vchSigRet;
    BOOST_CHECK(key.SignCompact(msgHash, vchSigRet));

    // check sig
    CPubKey pubkeyFromSig;
    BOOST_CHECK(pubkeyFromSig.RecoverCompact(msgHash, vchSigRet));
    BOOST_CHECK(pubkeyFromSig == pubKey);
    std::cout << "Sig: " << std::endl;
    std::cout << "  R = " << HexStr(vchSigRet.begin()+1, vchSigRet.begin()+33) << std::endl;
    std::cout << "  S = " << HexStr(vchSigRet.begin()+33, vchSigRet.end()) << std::endl;
    std::cout << "  recovered key = " << HexStr(pubkeyFromSig.Raw()) << std::endl;

    // new sig
    CBigNum S_num(std::vector<unsigned char>(vchSigRet.begin()+33, vchSigRet.end()));
    CBigNum order;
    order.SetHex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");
    CBigNum newSig_num = order - S_num;
    std::vector<unsigned char> newSig;
    newSig.resize(CPubKey::COMPACT_SIGNATURE_SIZE);
    std::copy(vchSigRet.begin(), vchSigRet.begin()+33, newSig.begin());
    std::vector<unsigned char> newS = newSig_num.getvch();
    std::copy(newS.begin(), newS.end(), newSig.begin()+33);

    // check new sig
    CPubKey pubkeyFromSig2;
    BOOST_CHECK(pubkeyFromSig2.RecoverCompact(msgHash, newSig));
    BOOST_CHECK(pubkeyFromSig2 == pubKey);
    std::cout << "newSig: " << std::endl;
    std::cout << "  R = " << HexStr(newSig.begin()+1, newSig.begin()+33) << std::endl;
    std::cout << "  S = " << HexStr(newSig.begin()+33, newSig.end()) << std::endl;
    std::cout << "  recovered key = " << HexStr(pubkeyFromSig2.Raw()) << std::endl;


}

BOOST_AUTO_TEST_SUITE_END()

