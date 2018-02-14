//
// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "../libzerocoin/Accumulator.h"
#include "../uint256.h"
#include "../primitives/zerocoin.h"
#include "../sync.h"
#include "../main.h"
#include "../crypter.h"
#include "../wallet.h"
#include <boost/test/unit_test.hpp>
#define DEBUG 0 //off by default to avoid terminal spam

using namespace libzerocoin;
using namespace std;

BOOST_AUTO_TEST_SUITE(zerocoin_encryption_tests)

    //pre declaration of helper methods at end of file
    void MockedEncryptZerocoinMint(const CZerocoinMint& mintIn, CZerocoinMint &mintEncryptedOut);
    void MockedDecryptZerocoinMint(const CZerocoinMint& mintCryptedIn, CZerocoinMint &mintDecryptedOut);

    //ZQ_ONE mint values
    std::string rawTxpub1 = "473ff507157523e74680ab37f586aae52e53f3f912492b19f7e14ab120d54238ae30b338f39662a410e6d707784d730f24d19dd9f75e85221b51b902a19d50c120844d15bf8a3b9e346355857e7381e5be19c6d3d22e01845565819aae7cacc93d75f1ef0c7b09d823865cdfa3671715e5bfc8dd8fc8baef26216e7941fa0c3";
    std::string rawTxRand1 = "9fc222b16be09eb88affbdfbcc02d1c8b28f5e843c72eb06c89dd7aff0c60838";
    std::string rawTxSerial1 = "b87754b165892c0f9634e3d03780ede24824125249cb8dfd4ad2c0be055cbead";

    //plain mint with static values to use across all tests cases
    static CZerocoinMint mint(CoinDenomination::ZQ_ONE, CBigNum(rawTxpub1), CBigNum(rawTxRand1), CBigNum(rawTxSerial1), false);

    //used as masterpassword (since using directly as cipher, must be 32 bytes since AES_256)
    SecureString strMasterKey("Thisisfakepassword11111111111111");

    //to pass into crypter methods
    CKeyingMaterial kmMasterKey(strMasterKey.begin(), strMasterKey.end());

    //for type conversion testing
    vector<unsigned char> vchMasterKey(strMasterKey.begin(), strMasterKey.end());


    BOOST_AUTO_TEST_CASE(zerocoin_cryption_test)
    {
        cout << "Running zerocoin_cryption_test" << endl;
        CZerocoinMint mintCrypted;
        CZerocoinMint mintPlain;

        MockedEncryptZerocoinMint(mint, mintCrypted);

#if DEBUG
        cout << "original value:" << mint.GetValue().ToString(16) << " \ncrypted value: "  << mintCrypted.GetValue().ToString(16) << endl<< endl;
        cout << "original ser:" << mint.GetSerialNumber().ToString(16) << " plain rand: "  << mint.GetRandomness().ToString(16) << endl<< endl;
        cout << "crypt ser:" << mintCrypted.GetSerialNumber().ToString(16) << " crypt rand: "  << mintCrypted.GetRandomness().ToString(16)<< endl<< endl;
#endif //DEBUG

        BOOST_CHECK_MESSAGE(mintCrypted.GetValue() == mint.GetValue(), "Values do not match. Value should stay the same through the cryption process.\n");
        BOOST_CHECK_MESSAGE(mintCrypted.GetSerialNumber() != mint.GetSerialNumber(), "Serials match. Encryption failed.\n");
        BOOST_CHECK_MESSAGE(mintCrypted.GetRandomness() != mint.GetRandomness(), "Randoms match. Encryption failed.\n");

        MockedDecryptZerocoinMint(mintCrypted, mintPlain);

        BOOST_CHECK_MESSAGE(mintPlain.GetValue() == mint.GetValue(), "Values do not match. Value should stay the same through the cryption process.\n");
        BOOST_CHECK_MESSAGE(mintPlain.GetSerialNumber() == mint.GetSerialNumber(), "Serials don't match. Decryption failed.\n");
        BOOST_CHECK_MESSAGE(mintPlain.GetRandomness() == mint.GetRandomness(), "Randoms don't match. Decryption failed.\n");
    }

    //test the conversion between all the wrapper objects used
    BOOST_AUTO_TEST_CASE(bn_vch_keymaterial_conversion_test)
    {
        cout << "Running bn_vch_keymaterial_conversion_test" << endl;

        vector<unsigned char> vchPlainSer = mint.GetSerialNumber().getvch();
        vector<unsigned char> vchPlainRandom = mint.GetRandomness().getvch();

        CKeyingMaterial kmSerialPlain(vchPlainSer.begin(), vchPlainSer.end());
        CKeyingMaterial kmRandomnessPlain(vchPlainRandom.begin(), vchPlainRandom.end());

        vector<unsigned char> vchPlainSerialFromKm(kmSerialPlain.begin(), kmSerialPlain.end());
        vector<unsigned char> vchPlainRandomFromKm(kmRandomnessPlain.begin(), kmRandomnessPlain.end());
        vector<unsigned char> vchMasterKeyFromKm(kmMasterKey.begin(), kmMasterKey.end());

        //test that back and forth from CKeyingMaterial doesn't fail
        BOOST_CHECK_MESSAGE(vchPlainSerialFromKm == mint.GetSerialNumber().getvch(), "Conversion of serial from CKeyingMaterial failed\n");
        BOOST_CHECK_MESSAGE(vchPlainRandomFromKm == mint.GetRandomness().getvch(), "Conversion of randomness from CKeyingMaterial failed\n");
        BOOST_CHECK_MESSAGE(vchMasterKeyFromKm == vchMasterKey, "Conversion of masterkey from CKeyingMaterial failed\n");
    }

    // modified version of CCryptoKeyStore::AddZerocoinMint that does encryption directly since can't get access to
    // needed fields for testing
    void MockedEncryptZerocoinMint(const CZerocoinMint& mintIn, CZerocoinMint &mintEncryptedOut)
    {
        const uint256 crypt = mintIn.GetValue().getuint256(); //use mint value as cipher
        const uint256 nIV = Hash(crypt.begin(), crypt.end()); //use hash as initialization vector

        vector<unsigned char> vchPlainSer = mintIn.GetSerialNumber().getvch();
        vector<unsigned char> vchPlainRandom = mintIn.GetRandomness().getvch();

        CKeyingMaterial kmSerialPlain(vchPlainSer.begin(), vchPlainSer.end());
        CKeyingMaterial kmRandomnessPlain(vchPlainRandom.begin(), vchPlainRandom.end());

        vector<unsigned char> serialOut;
        vector<unsigned char> randomOut;

        BOOST_CHECK_MESSAGE(EncryptSecret(kmMasterKey, kmSerialPlain, nIV, serialOut), "Encrypt secret failed for serial\n");
        BOOST_CHECK_MESSAGE(EncryptSecret(kmMasterKey, kmRandomnessPlain, nIV, randomOut), "Encrypt secret failed for serial\n");
#if DEBUG
       cout << "manual cryption ser out: " << CBigNum(serialOut).ToString(16) << " manual crypted rand out: "  << CBigNum(randomOut).ToString(16)<< endl<< endl;
#endif //DEBUG

        mintEncryptedOut = mintIn;
        mintEncryptedOut.SetSerialNumber(CBigNum(serialOut));
        mintEncryptedOut.SetRandomness(CBigNum(randomOut));

#if DEBUG
        cout << "plain val: " << mintIn.GetValue().ToString(16) << " \nmanual crypted value: "  << mintEncryptedOut.GetValue().ToString(16)<< endl<< endl;
        cout << "plain ser:" << mintIn.GetSerialNumber().ToString(16) << " plain rand: "  << mintIn.GetRandomness().ToString(16)<< endl<< endl;
        cout << "crypt ser:" << mintEncryptedOut.GetSerialNumber().ToString(16) << " crypt rand: "  << mintEncryptedOut.GetRandomness().ToString(16)<< endl<< endl;
#endif //DEBUG
    }

    // modified version of CCryptoKeyStore::GetZerocoinMint that does decryption directly since can't get access to
    // needed fields for testing
    void MockedDecryptZerocoinMint(const CZerocoinMint& mintCryptedIn, CZerocoinMint &mintDecryptedOut)
    {
        //crypted mint should still have the same value
        const uint256 &key = mintCryptedIn.GetValue().getuint256();
        const uint256 nIV = Hash(key.begin(), key.end());
        map<ZerocoinSecrets, vector<unsigned char> > mapSecrets;
        map<ZerocoinSecrets, vector<unsigned char> > mapPlain;

        mapSecrets[SERIAL] = mintCryptedIn.GetSerialNumber().getvch();
        mapSecrets[RANDOM] = mintCryptedIn.GetRandomness().getvch();

        CKeyingMaterial kmSerial;
        CKeyingMaterial kmRandomness;

        DecryptSecret(kmMasterKey, mapSecrets[SERIAL], nIV, kmSerial);
        DecryptSecret(kmMasterKey, mapSecrets[RANDOM], nIV, kmRandomness);

        mapPlain[SERIAL] = vector<unsigned char>(kmSerial.begin(), kmSerial.end());
        mapPlain[RANDOM] = vector<unsigned char>(kmRandomness.begin(), kmRandomness.end());

        mintDecryptedOut = mintCryptedIn;
        mintDecryptedOut.SetSerialNumber(CBigNum(mapPlain[SERIAL]));
        mintDecryptedOut.SetRandomness(CBigNum(mapPlain[RANDOM]));
        mintDecryptedOut.SetIsCrypted(false);

#if DEBUG
        cout << "plain ser:" << mint.GetSerialNumber().ToString(16) << " plain rand: "  << mint.GetRandomness().ToString(16)<< endl<< endl;
        cout << "decrypted ser:" << mintDecryptedOut.GetSerialNumber().ToString(16) << " decrypted rand: "  << mintDecryptedOut.GetRandomness().ToString(16)<< endl<< endl;
#endif //DEBUG
    }

BOOST_AUTO_TEST_SUITE_END()

