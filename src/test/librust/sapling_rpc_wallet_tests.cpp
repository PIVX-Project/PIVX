// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fs.h"

#include "wallet/test/wallet_test_fixture.h"
#include "test/librust/utiltest.h"

#include "rpc/server.h"
#include "rpc/client.h"

#include "core_io.h"
#include "consensus/merkle.h"
#include "wallet/wallet.h"

#include "sapling/key_io_sapling.h"
#include "sapling/address.hpp"
#include "sapling/sapling_operation.h"

#include <unordered_set>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

extern UniValue CallRPC(std::string args); // Implemented in rpc_tests.cpp

namespace {

    /** Set the working directory for the duration of the scope. */
    class PushCurrentDirectory {
    public:
        PushCurrentDirectory(const std::string &new_cwd)
                : old_cwd(fs::current_path()) {
            fs::current_path(new_cwd);
        }

        ~PushCurrentDirectory() {
            fs::current_path(old_cwd);
        }
    private:
        fs::path old_cwd;
    };

}

BOOST_FIXTURE_TEST_SUITE(sapling_rpc_wallet_tests, WalletTestingSetup)

/**
 * This test covers RPC command validateaddress
 */

BOOST_AUTO_TEST_CASE(rpc_wallet_sapling_validateaddress)
{
    SelectParams(CBaseChainParams::MAIN);
    UniValue retValue;

    // Check number of args
    BOOST_CHECK_THROW(CallRPC("validateaddress"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("validateaddress toomany args"), std::runtime_error);

    // Wallet should be empty:
    std::set<libzcash::SaplingPaymentAddress> addrs;
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // This Sapling address is not valid, it belongs to another network
    BOOST_CHECK_NO_THROW(retValue = CallRPC("validateaddress ptestsapling1nrn6exksuqtpld9gu6fwdz4hwg54h2x37gutdds89pfyg6mtjf63km45a8eare5qla45cj75vs8"));
    UniValue resultObj = retValue.get_obj();
    bool b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, false);

    // This Sapling address is valid, but the spending key is not in this wallet
    BOOST_CHECK_NO_THROW(retValue = CallRPC("validateaddress ps1u87kylcmn28yclnx2uy0psnvuhs2xn608ukm6n2nshrpg2nzyu3n62ls8j77m9cgp40dx40evej"));
    resultObj = retValue.get_obj();
    b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, true);
    b = find_value(resultObj, "ismine").get_bool();
    BOOST_CHECK_EQUAL(b, false);
    BOOST_CHECK_EQUAL(find_value(resultObj, "diversifier").get_str(), "e1fd627f1b9a8e4c7e6657");
    BOOST_CHECK_EQUAL(find_value(resultObj, "diversifiedtransmissionkey").get_str(), "d35e0d0897edbd3cf02b3d2327622a14c685534dbd2d3f4f4fa3e0e56cc2f008");
}

BOOST_AUTO_TEST_CASE(rpc_wallet_getbalance)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    BOOST_CHECK_THROW(CallRPC("getshieldbalance too many args"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("getshieldbalance invalidaddress"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("getshieldbalance tmC6YZnCUhm19dEXxh3Jb7srdBJxDawaCab"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("getshieldbalance ptestsapling1h0w73csah2aq0a32h42kr7tq4htlt5wfn4ejxfnm56f6ehjvek7k4e244g6v8v3pgylmz5ea8jh"));
    BOOST_CHECK_THROW(CallRPC("getshieldbalance ptestsapling1h0w73csah2aq0a32h42kr7tq4htlt5wfn4ejxfnm56f6ehjvek7k4e244g6v8v3pgylmz5ea8jh -1"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("getshieldbalance ptestsapling1nrn6exksuqtpld9gu6fwdz4hwg54h2x37gutdds89pfyg6mtjf63km45a8eare5qla45cj75vs8 0"));
    BOOST_CHECK_THROW(CallRPC("getshieldbalance tnRZ8bPq2pff3xBWhTJhNkVUkm2uhzksDeW5PvEa7aFKGT9Qi3YgTALZfjaY4jU3HLVKBtHdSXxoPoLA3naMPcHBcY88FcF 1"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("getshieldbalance *"));
    BOOST_CHECK_NO_THROW(CallRPC("getshieldbalance * 6"));
    BOOST_CHECK_THROW(CallRPC("getshieldbalance * -1"), std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("listreceivedbyshieldaddress too many args"), std::runtime_error);
    // negative minconf not allowed
    BOOST_CHECK_THROW(CallRPC("listreceivedbyshieldaddress yBYhwgzufrZ6F5VVuK9nEChENArq934mqC -1"), std::runtime_error);
    // invalid zaddr, taddr not allowed
    BOOST_CHECK_THROW(CallRPC("listreceivedbyshieldaddress yBYhwgzufrZ6F5VVuK9nEChENArq934mqC 0"), std::runtime_error);
    // don't have the spending key
    BOOST_CHECK_THROW(CallRPC("listreceivedbyshieldaddress ptestsapling1nrn6exksuqtpld9gu6fwdz4hwg54h2x37gutdds89pfyg6mtjf63km45a8eare5qla45cj75vs8 1"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_wallet_sapling_importkey_paymentaddress) {
    SelectParams(CBaseChainParams::MAIN);
    {
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->SetMinVersion(FEATURE_SAPLING);
        pwalletMain->SetupSPKM(false);
    }

    auto testAddress = [](const std::string& key) {
        UniValue ret;
        BOOST_CHECK_NO_THROW(ret = CallRPC("importsaplingkey " + key));
        auto defaultAddr = find_value(ret, "address").get_str();
        BOOST_CHECK_NO_THROW(ret = CallRPC("validateaddress " + defaultAddr));
        ret = ret.get_obj();
        BOOST_CHECK_EQUAL(true, find_value(ret, "isvalid").get_bool());
        BOOST_CHECK_EQUAL(true, find_value(ret, "ismine").get_bool());
    };

    testAddress("p-secret-spending-key-main1qv09u0wlqqqqpqp75kpmat6l3ce29k"
                "g9half9epsm80wya5n92j4d8mtmesrukzxlsmm2f74v3nvvx2shxy4z5v5x39p"
                "eelsy5y2uxmvadaku8crd20q6vt8cvd68wp08cjyec6cku0dcf5lc9c2kykg5c"
                "8uqmqlx8ccxpsw7ae243quhwr0zyekrrc520gs9z0j8pm954c3cev2yvp29vrc"
                "0zweu7stxkwhp593p6drheps9uhz9pvkrfgvpxzte8d60uzw0qxadnsc77tcd");

}

/*
 * This test covers RPC commands listsaplingaddresses, importsaplingkey, exportsaplingkey
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_sapling_importexport)
{
    {
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->SetMinVersion(FEATURE_SAPLING);
        pwalletMain->SetupSPKM(false);
    }
    UniValue retValue;
    int n1 = 1000; // number of times to import/export
    int n2 = 1000; // number of addresses to create and list

    // error if no args
    BOOST_CHECK_THROW(CallRPC("importsaplingkey"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("exportsaplingkey"), std::runtime_error);

    // error if too many args
    BOOST_CHECK_THROW(CallRPC("importsaplingkey way too many args"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("exportsaplingkey toomany args"), std::runtime_error);

    // error if invalid args
    auto m = GetTestMasterSaplingSpendingKey();
    std::string prefix = std::string("importsaplingkey ") + KeyIO::EncodeSpendingKey(m) + " yes ";
    BOOST_CHECK_THROW(CallRPC(prefix + "-1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(prefix + "2147483647"), std::runtime_error); // allowed, but > height of active chain tip
    BOOST_CHECK_THROW(CallRPC(prefix + "2147483648"), std::runtime_error); // not allowed, > int32 used for nHeight
    BOOST_CHECK_THROW(CallRPC(prefix + "100badchars"), std::runtime_error);

    // wallet should currently be empty
    std::set<libzcash::SaplingPaymentAddress> saplingAddrs;
    pwalletMain->GetSaplingPaymentAddresses(saplingAddrs);
    BOOST_CHECK(saplingAddrs.empty());

    // verify import and export key
    for (int i = 0; i < n1; i++) {
        // create a random Sapling key locally
        auto testSaplingSpendingKey = m.Derive(i);
        auto testSaplingPaymentAddress = testSaplingSpendingKey.DefaultAddress();
        std::string testSaplingAddr = KeyIO::EncodePaymentAddress(testSaplingPaymentAddress);
        std::string testSaplingKey = KeyIO::EncodeSpendingKey(testSaplingSpendingKey);
        BOOST_CHECK_NO_THROW(CallRPC(std::string("importsaplingkey ") + testSaplingKey));
        BOOST_CHECK_NO_THROW(retValue = CallRPC(std::string("exportsaplingkey ") + testSaplingAddr));
        BOOST_CHECK_EQUAL(retValue.get_str(), testSaplingKey);
    }

    // Verify we can list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("listshieldaddresses"));
    UniValue arr = retValue.get_array();
    BOOST_CHECK((int) arr.size() == n1);

    // Put addresses into a set
    std::unordered_set<std::string> myaddrs;
    for (const UniValue& element : arr.getValues()) {
        myaddrs.insert(element.get_str());
    }

    // Make new addresses for the set
    for (int i=0; i<n2; i++) {
        myaddrs.insert(KeyIO::EncodePaymentAddress(pwalletMain->GenerateNewSaplingZKey()));
    }

    // Verify number of addresses stored in wallet is n1+n2
    int numAddrs = myaddrs.size();
    BOOST_CHECK(numAddrs == n1 + n2);
    pwalletMain->GetSaplingPaymentAddresses(saplingAddrs);
    BOOST_CHECK((int) saplingAddrs.size() == numAddrs);

    // Ask wallet to list addresses
    BOOST_CHECK_NO_THROW(retValue = CallRPC("listshieldaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK((int) arr.size() == numAddrs);

    // Create a set from them
    std::unordered_set<std::string> listaddrs;
    for (const UniValue& element : arr.getValues()) {
        listaddrs.insert(element.get_str());
    }

    // Verify the two sets of addresses are the same
    BOOST_CHECK((int) listaddrs.size() == numAddrs);
    BOOST_CHECK(myaddrs == listaddrs);

}

// Check if address is of given type and spendable from our wallet.
void CheckHaveAddr(const libzcash::PaymentAddress& addr) {

    BOOST_CHECK(IsValidPaymentAddress(addr));
    auto addr_of_type = boost::get<libzcash::SaplingPaymentAddress>(&addr);
    BOOST_ASSERT(addr_of_type != nullptr);
    BOOST_CHECK(pwalletMain->HaveSpendingKeyForPaymentAddress(*addr_of_type));
}

BOOST_AUTO_TEST_CASE(rpc_wallet_getnewshieldaddress) {
    UniValue addr;
    {
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->SetMinVersion(FEATURE_SAPLING);
        pwalletMain->SetupSPKM(false);
    }

    // No parameter defaults to sapling address
    addr = CallRPC("getnewshieldaddress");
    CheckHaveAddr(KeyIO::DecodePaymentAddress(addr.get_str()));
    // Too many arguments will throw with the help
    BOOST_CHECK_THROW(CallRPC("getnewshieldaddress many args"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_shieldsendmany_parameters)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    BOOST_CHECK_THROW(CallRPC("shieldsendmany"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("shieldsendmany toofewargs"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("shieldsendmany just too many args here"), std::runtime_error);

    // bad from address
    BOOST_CHECK_THROW(CallRPC("shieldsendmany "
                              "INVALIDyBYhwgzufrZ6F5VVuK9nEChENArq934mqC []"), std::runtime_error);
    // empty amounts
    BOOST_CHECK_THROW(CallRPC("shieldsendmany "
                              "yBYhwgzufrZ6F5VVuK9nEChENArq934mqC []"), std::runtime_error);

    // don't have the spending key for this address
    BOOST_CHECK_THROW(CallRPC("shieldsendmany "
                              "ptestsapling1wpurflqllgkcs48m46yu9ktlfe3ahndely20dpaanqq3lw9l5xw7yfehst68yclvlpz7x8cltxe"
                              "UkJ1oSfbhTJhm72WiZizvkZz5aH1 []"), std::runtime_error);

    // duplicate address
    BOOST_CHECK_THROW(CallRPC("shieldsendmany "
                              "yBYhwgzufrZ6F5VVuK9nEChENArq934mqC "
                              "[{\"address\":\"yAJ4bGeDFcEtx24kbr413fBLpWQcdR5F2z\", \"amount\":50.0},"
                              " {\"address\":\"yAJ4bGeDFcEtx24kbr413fBLpWQcdR5F2z\", \"amount\":12.0} ]"
    ), std::runtime_error);

    // invalid fee amount, cannot be negative
    BOOST_CHECK_THROW(CallRPC("shieldsendmany "
                              "yBYhwgzufrZ6F5VVuK9nEChENArq934mqC "
                              "[{\"address\":\"yAJ4bGeDFcEtx24kbr413fBLpWQcdR5F2z\", \"amount\":50.0}] "
                              "1 -0.0001"
    ), std::runtime_error);

    // invalid fee amount, bigger than MAX_MONEY
    BOOST_CHECK_THROW(CallRPC("shieldsendmany "
                              "yBYhwgzufrZ6F5VVuK9nEChENArq934mqC "
                              "[{\"address\":\"yAJ4bGeDFcEtx24kbr413fBLpWQcdR5F2z\", \"amount\":50.0}] "
                              "1 21000001"
    ), std::runtime_error);

    // fee amount is bigger than sum of outputs
    BOOST_CHECK_THROW(CallRPC("shieldsendmany "
                              "yBYhwgzufrZ6F5VVuK9nEChENArq934mqC "
                              "[{\"address\":\"yAJ4bGeDFcEtx24kbr413fBLpWQcdR5F2z\", \"amount\":50.0}] "
                              "1 50.00000001"
    ), std::runtime_error);

    // memo bigger than allowed length of ZC_MEMO_SIZE
    std::vector<char> v (2 * (ZC_MEMO_SIZE+1));     // x2 for hexadecimal string format
    std::fill(v.begin(),v.end(), 'A');
    std::string badmemo(v.begin(), v.end());
    pwalletMain->SetupSPKM(false);
    auto pa = pwalletMain->GenerateNewSaplingZKey();
    std::string zaddr1 = KeyIO::EncodePaymentAddress(pa);
    BOOST_CHECK_THROW(CallRPC(std::string("shieldsendmany yBYhwgzufrZ6F5VVuK9nEChENArq934mqC ")
                              + "[{\"address\":\"" + zaddr1 + "\", \"amount\":123.456}]"), std::runtime_error);
}

// TODO: test private methods
BOOST_AUTO_TEST_CASE(saplingOperationTests) {
    RegtestActivateSapling();
    auto consensusParams = Params().GetConsensus();

    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->SetupSPKM(false);

    UniValue retValue;

    // add keys manually
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getnewaddress"));
    const std::string& taddrStr = retValue.get_str();
    const CTxDestination& taddr1 = DecodeDestination(taddrStr);
    const auto& zaddr1 = pwalletMain->GenerateNewSaplingZKey();
    std::string ret;

    // there are no utxos to spend
    {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(zaddr1, COIN, "DEADBEEF") };
        SaplingOperation operation(consensusParams, 1);
        operation.setFromAddress(taddr1);
        auto res = operation.setRecipients(recipients)->buildAndSend(ret);
        BOOST_CHECK(!res);
        BOOST_CHECK(res.getError().find("Insufficient funds, no available UTXO to spend") != std::string::npos);
    }

    // minconf cannot be zero when sending from zaddr
    {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(zaddr1, COIN, "DEADBEEF") };
        SaplingOperation operation(consensusParams, 1);
        operation.setFromAddress(zaddr1);
        auto res = operation.setRecipients(recipients)->setMinDepth(0)->buildAndSend(ret);
        BOOST_CHECK(!res);
        BOOST_CHECK(res.getError().find("Minconf cannot be zero when sending from shield address") != std::string::npos);
    }

    // there are no unspent notes to spend
    {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(taddr1, COIN) };
        SaplingOperation operation(consensusParams, 1);
        operation.setFromAddress(zaddr1);
        auto res = operation.setRecipients(recipients)->buildAndSend(ret);
        BOOST_CHECK(!res);
        BOOST_CHECK(res.getError().find("Insufficient funds, no available notes to spend") != std::string::npos);
    }

    // GetMemoFromString
    {
        std::string memoStr = "Sapling memo!";
        std::array<unsigned char, ZC_MEMO_SIZE> memo;

        BOOST_CHECK(GetMemoFromString(memoStr, memo));
        BOOST_CHECK_EQUAL(memo[0], 0x53);   // S
        BOOST_CHECK_EQUAL(memo[1], 0x61);   // a
        BOOST_CHECK_EQUAL(memo[2], 0x70);   // p
        BOOST_CHECK_EQUAL(memo[3], 0x6C);   // l
        BOOST_CHECK_EQUAL(memo[4], 0x69);   // i
        BOOST_CHECK_EQUAL(memo[5], 0x6E);   // n
        BOOST_CHECK_EQUAL(memo[6], 0x67);   // g
        BOOST_CHECK_EQUAL(memo[12], 0x21);  // !
        for (int i = 13; i < ZC_MEMO_SIZE; i++) {
            BOOST_CHECK_EQUAL(memo[i], 0x00);  // zero padding
        }

        // memo is longer than allowed
        std::vector<char> v (2 * (ZC_MEMO_SIZE+1));
        std::fill(v.begin(),v.end(), 'A');
        std::string bigmemo(v.begin(), v.end());

        OperationResult res = GetMemoFromString(bigmemo, memo);
        BOOST_CHECK(!res);
        const std::string& errStr = res.getError();
        BOOST_CHECK(errStr.find("too big") != std::string::npos);
    }
    RegtestDeactivateSapling();
}


BOOST_AUTO_TEST_CASE(rpc_shieldsendmany_taddr_to_sapling)
{
    SelectParams(CBaseChainParams::REGTEST);
    RegtestActivateSapling();

    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->SetupSPKM(false);

    UniValue retValue;

    // add keys manually
    CTxDestination taddr;
    pwalletMain->getNewAddress(taddr, "");
    std::string taddr1 = EncodeDestination(taddr);
    auto zaddr1 = pwalletMain->GenerateNewSaplingZKey();

    auto consensusParams = Params().GetConsensus();
    retValue = CallRPC("getblockcount");
    int nextBlockHeight = retValue.get_int() + 1;

    // Add a fake transaction to the wallet
    CMutableTransaction mtx;
    mtx.vout.emplace_back(5 * COIN, GetScriptForDestination(taddr));
    CWalletTx wtx(pwalletMain, mtx);
    pwalletMain->LoadToWallet(wtx);

    // Fake-mine the transaction
    BOOST_CHECK_EQUAL(0, chainActive.Height());
    CBlock block;
    block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    block.vtx.emplace_back(MakeTransactionRef(wtx));
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    fakeIndex.nHeight = 1;
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    BOOST_CHECK(chainActive.Contains(&fakeIndex));
    BOOST_CHECK_EQUAL(1, chainActive.Height());
    wtx.SetMerkleBranch(blockHash, 0);
    pwalletMain->LoadToWallet(wtx);

    // Context that shieldsendmany requires
    auto builder = TransactionBuilder(consensusParams, nextBlockHeight, pwalletMain);

    std::vector<SendManyRecipient> recipients = { SendManyRecipient(zaddr1, 1 * COIN, "ABCD") };
    SaplingOperation operation(builder);
    operation.setFromAddress(taddr);
    BOOST_CHECK(operation.setRecipients(recipients)
                         ->setMinDepth(0)
                         ->build());

    // try from auto-selected transparent address
    std::vector<SendManyRecipient> recipients2 = { SendManyRecipient(zaddr1, 1 * COIN, "ABCD") };
    SaplingOperation operation2(builder);
    BOOST_CHECK(operation2.setSelectTransparentCoins(true)
                          ->setRecipients(recipients2)
                          ->setMinDepth(0)
                          ->build());

    // Get the transaction
    // Test mode does not send the transaction to the network.
    auto hexTx = EncodeHexTx(operation.getFinalTx());
    CDataStream ss(ParseHex(hexTx), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    ss >> tx;
    BOOST_ASSERT(!tx.sapData->vShieldOutput.empty());

    // We shouldn't be able to decrypt with the empty ovk
    BOOST_CHECK(!libzcash::AttemptSaplingOutDecryption(
            tx.sapData->vShieldOutput[0].outCiphertext,
            uint256(),
            tx.sapData->vShieldOutput[0].cv,
            tx.sapData->vShieldOutput[0].cmu,
            tx.sapData->vShieldOutput[0].ephemeralKey));

    BOOST_CHECK(libzcash::AttemptSaplingOutDecryption(
            tx.sapData->vShieldOutput[0].outCiphertext,
            pwalletMain->GetSaplingScriptPubKeyMan()->getCommonOVK(),
            tx.sapData->vShieldOutput[0].cv,
            tx.sapData->vShieldOutput[0].cmu,
            tx.sapData->vShieldOutput[0].ephemeralKey));

    // Tear down
    chainActive.SetTip(nullptr);
    mapBlockIndex.erase(blockHash);

    // Revert to default
    RegtestDeactivateSapling();
}

BOOST_AUTO_TEST_CASE(rpc_wallet_encrypted_wallet_sapzkeys)
{
    UniValue retValue;
    int n = 100;

    {
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->SetMinVersion(FEATURE_SAPLING);
        pwalletMain->SetupSPKM(false);
    }

    // wallet should currently be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK(addrs.empty());

    // create keys
    for (int i = 0; i < n; i++) {
        CallRPC("getnewshieldaddress");
    }

    // Verify we can list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("listshieldaddresses"));
    UniValue arr = retValue.get_array();
    BOOST_CHECK((int) arr.size() == n);

    // Verify that the wallet encryption RPC is disabled
    // TODO: We don't have the experimental mode to disable the encryptwallet disable.
    //BOOST_CHECK_THROW(CallRPC("encryptwallet passphrase"), std::runtime_error);

    // Encrypt the wallet (we can't call RPC encryptwallet as that shuts down node)
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = "hello";

    PushCurrentDirectory push_dir(gArgs.GetArg("-datadir","/tmp/thisshouldnothappen"));
    BOOST_CHECK(pwalletMain->EncryptWallet(strWalletPass));

    // Verify we can still list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("listshieldaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK((int) arr.size() == n);

    // Try to add a new key, but we can't as the wallet is locked
    BOOST_CHECK_THROW(CallRPC("getnewshieldaddress"), std::runtime_error);

    // We can't call RPC walletpassphrase as that invokes RPCRunLater which breaks tests.
    // So we manually unlock.
    BOOST_CHECK(pwalletMain->Unlock(strWalletPass));

    // Now add a key
    BOOST_CHECK_NO_THROW(CallRPC("getnewshieldaddress"));

    // Verify the key has been added
    BOOST_CHECK_NO_THROW(retValue = CallRPC("listshieldaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK((int) arr.size() == n+1);

    // We can't simulate over RPC the wallet closing and being reloaded
    // but there are tests for this in gtest.
}

BOOST_AUTO_TEST_CASE(rpc_listshieldunspent_parameters)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->SetupSPKM(false);

    UniValue retValue;

    // too many args
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 1 2 3 4 5"), std::runtime_error);

    // minconf must be >= 0
    BOOST_CHECK_THROW(CallRPC("listshieldunspent -1"), std::runtime_error);

    // maxconf must be > minconf
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 2 1"), std::runtime_error);

    // maxconf must not be out of range
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 1 9999999999"), std::runtime_error);

    // must be an array of addresses
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 1 999 false ptestsapling1wpurflqllgkcs48m46yu9ktlfe3ahndely20dpaanqq3lw9l5xw7yfehst68yclvlpz7x8cltxe"), std::runtime_error);

    // address must be string
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 1 999 false [123456]"), std::runtime_error);

    // no spending key
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 1 999 false [\"ptestsapling1wpurflqllgkcs48m46yu9ktlfe3ahndely20dpaanqq3lw9l5xw7yfehst68yclvlpz7x8cltxe\"]"), std::runtime_error);

    // allow watch only
    BOOST_CHECK_NO_THROW(CallRPC("listshieldunspent 1 999 true [\"ptestsapling1wpurflqllgkcs48m46yu9ktlfe3ahndely20dpaanqq3lw9l5xw7yfehst68yclvlpz7x8cltxe\"]"));

    // wrong network, mainnet instead of testnet
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 1 999 true [\"ps1qenk9kapr0crx7lmdl4yclx78spc36wh7d5hm9hglp85f43k9dupyf0c5836h42wq2ejv0ef2v3\"]"), std::runtime_error);

    // create shield address so we have the spending key
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getnewshieldaddress"));
    std::string myzaddr = retValue.get_str();

    // return empty array for this address
    BOOST_CHECK_NO_THROW(retValue = CallRPC("listshieldunspent 1 999 false [\"" + myzaddr + "\"]"));
    UniValue arr = retValue.get_array();
    BOOST_CHECK_EQUAL(0, arr.size());

    // duplicate address error
    BOOST_CHECK_THROW(CallRPC("listshieldunspent 1 999 false [\"" + myzaddr + "\", \"" + myzaddr + "\"]"), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
