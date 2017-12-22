// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpivwallet.h"
#include "main.h"
#include "txdb.h"
#include "walletdb.h"
#include "init.h"

using namespace libzerocoin;


CzPIVWallet::CzPIVWallet(std::string strWalletFile, bool fFirstRun)
{
    this->strWalletFile = strWalletFile;
    CWalletDB walletdb(strWalletFile);

    uint256 seed;
    if (!walletdb.ReadZPIVSeed(seed))
        fFirstRun = true;

    //First time running, generate master seed
    if (fFirstRun)
        seed = CBigNum::randBignum(CBigNum(~uint256(0))).getuint256();

    SetMasterSeed(seed);
    this->mintPool = CMintPool(nCount);
}

bool CzPIVWallet::SetMasterSeed(const uint256& seedMaster, bool fResetCount)
{
    this->seedMaster = seedMaster;

    CWalletDB walletdb(strWalletFile);
    if (!walletdb.WriteZPIVSeed(seedMaster))
        return false;

    nCount = 0;
    if (fResetCount)
        walletdb.WriteZPIVCount(nCount);
    else if (!walletdb.ReadZPIVCount(nCount))
        nCount = 0;

    //TODO remove this leak of seed from logs before merge to master
    LogPrintf("%s : seed=%s count=%d\n", __func__, seedMaster.GetHex(), nCount);

    //todo fix to sync with count above
    mintPool.Reset();

    return true;
}

//Add the next 10 mints to the mint pool
void CzPIVWallet::GenerateMintPool()
{
    LogPrintf("%s\n", __func__);
    int n = std::max(mintPool.CountOfLastGenerated(), nCount);

    int nStop = std::max(nCount + 10, mintPool.CountOfLastGenerated());
    LogPrintf("%s : n=%d nStop=%d\n", __func__, n, nStop);
    for (int i = n; i < nStop; i++) {
        uint512 seedZerocoin = GetZerocoinSeed(i);
        CBigNum bnSerial;
        CBigNum bnRandomness;
        SeedToZPIV(seedZerocoin, bnSerial, bnRandomness);

        PrivateCoin coin(Params().Zerocoin_Params(), CoinDenomination::ZQ_ONE, bnSerial, bnRandomness);
        mintPool.Add(coin.getPublicCoin().getValue(), i);
        LogPrintf("%s : %s count=%d\n", __func__, coin.getPublicCoin().getValue().GetHex().substr(0, 6), i);
    }
}

//Catch the counter up with the chain
map<CBigNum, uint32_t> mapMissingMints;
void CzPIVWallet::SyncWithChain()
{
    uint32_t nLastCountUsed = 0;
    bool found = true;
    while (found) {
        found = false;
        GenerateMintPool();

        for (pair<CBigNum, uint32_t> pMint : mintPool.List()) {
            if (ShutdownRequested())
                return;

            if (mapMissingMints.count(pMint.first))
                continue;

            uint256 txHash;
            if (zerocoinDB->ReadCoinMint(pMint.first, txHash)) {
                //this mint has already occured on the chain, increment counter's state to reflect this
                LogPrintf("%s : Found used coin mint %s \n", __func__, pMint.first.GetHex());
                found = true;

                uint256 hashBlock;
                CTransaction tx;
                if (!GetTransaction(txHash, tx, hashBlock)) {
                    LogPrintf("%s : failed to get transaction for mint %s!\n", __func__, pMint.first.GetHex());
                    found = false;
                    nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                    mapMissingMints.insert(pMint);
                    continue;
                }

                //Find the denomination
                CoinDenomination denomination = CoinDenomination::ZQ_ERROR;
                bool fFoundMint = false;
                for (const CTxOut out : tx.vout) {
                    if (!out.scriptPubKey.IsZerocoinMint())
                        continue;

                    PublicCoin pubcoin(Params().Zerocoin_Params());
                    CValidationState state;
                    if (!TxOutToPublicCoin(out, pubcoin, state)) {
                        LogPrintf("%s : failed to get mint from txout for %s!\n", __func__, pMint.first.GetHex());
                        continue;
                    }

                    // See if this is the mint that we are looking for
                    if (pMint.first == pubcoin.getValue()) {
                        denomination = pubcoin.getDenomination();
                        fFoundMint = true;
                        break;
                    }
                }

                if (!fFoundMint || denomination == ZQ_ERROR) {
                    LogPrintf("%s : failed to get mint %s from tx %s!\n", __func__, pMint.first.GetHex(), tx.GetHash().GetHex());
                    found = false;
                    nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                    mapMissingMints.insert(pMint);
                    break;
                }

                //The mint was found in the chain, so recalculate the randomness and serial and DB it
                CBigNum bnSerial;
                CBigNum bnRandomness;
                uint512 seedZerocoin = GetZerocoinSeed(pMint.second);
                SeedToZPIV(seedZerocoin, bnSerial, bnRandomness);
                PrivateCoin coin(Params().Zerocoin_Params(), denomination, bnSerial, bnRandomness);

                CZerocoinMint mint(denomination, pMint.first, coin.getRandomness(), coin.getSerialNumber(), false);
                int nHeight = 0;
                if (mapBlockIndex.count(hashBlock))
                    nHeight = mapBlockIndex.at(hashBlock)->nHeight;

                mint.SetHeight(nHeight);
                mint.SetTxHash(txHash);
                SetMintSeen(mint);
                nLastCountUsed = std::max(pMint.second, nLastCountUsed);
            }
        }
    }
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteZPIVCount(++nLastCountUsed);
}

bool CzPIVWallet::SetMintSeen(CZerocoinMint mint)
{
    if (!mintPool.Has(mint.GetValue()))
        return false;
    pair<CBigNum, uint32_t> pMint = mintPool.Get(mint.GetValue());

    //If this mint comes in from main.cpp, we have no knowledge of the serial or rand
    if (mint.GetSerialNumber() == 0) {
        uint512 seedZerocoin = GetZerocoinSeed(pMint.second);
        CBigNum bnSerial;
        CBigNum bnRandomness;
        SeedToZPIV(seedZerocoin, bnSerial, bnRandomness);
        mint.SetSerialNumber(bnSerial);
        mint.SetRandomness(bnRandomness);
    }

    //Store the mint to DB
    if (!CWalletDB(strWalletFile).WriteZerocoinMint(mint)) {
        LogPrintf("%s : failed to database mint %s!\n", __func__, mint.GetValue().GetHex());
        return false;
    }

    //Update the count if it is less than the mint's count
    if (nCount <= pMint.second) {
        CWalletDB walletdb(strWalletFile);
        nCount = pMint.second + 1;
        walletdb.WriteZPIVCount(nCount);
    }

    //remove from the pool
    mintPool.Remove(mint.GetValue());

    //fill the pool up with the next value(s)
    //GenerateMintPool();

    return true;
}

// Check if the value of the commitment meets requirements
bool IsValidCoinValue(const CBigNum& bnValue)
{
    return bnValue >= Params().Zerocoin_Params()->accumulatorParams.minCoinValue &&
    bnValue <= Params().Zerocoin_Params()->accumulatorParams.maxCoinValue &&
    bnValue.isPrime(ZEROCOIN_MINT_PRIME_PARAM);
}

void CzPIVWallet::SeedToZPIV(uint512 seedZerocoin, CBigNum& bnSerial, CBigNum& bnRandomness)
{
    ZerocoinParams* params = Params().Zerocoin_Params();

    //convert state seed into a seed for the serial and one for randomness
    uint256 serialSeed = seedZerocoin.trim256();
    bnSerial.setuint256(serialSeed);
    bnSerial = bnSerial % params->coinCommitmentGroup.groupOrder;

    //hash randomness seed with Bottom 256 bits of seedZerocoin & attempts256 which is initially 0
    uint256 randomnessSeed = uint512(seedZerocoin >> 256).trim256();
    uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end());
    bnRandomness.setuint256(hashRandomness);
    bnRandomness = bnRandomness % params->coinCommitmentGroup.groupOrder;

    //See if serial and randomness make a valid commitment
    // Generate a Pedersen commitment to the serial number
    CBigNum commitmentValue = params->coinCommitmentGroup.g.pow_mod(bnSerial, params->coinCommitmentGroup.modulus).mul_mod(
                        params->coinCommitmentGroup.h.pow_mod(bnRandomness, params->coinCommitmentGroup.modulus),
                        params->coinCommitmentGroup.modulus);

    CBigNum BigNumRandomness;
    uint256 attempts256 = 0;
    // Iterate on Randomness until a valid commitmentValue is found
    while (true) {

        // Now verify that the commitment is a prime number
        // in the appropriate range. If not, we'll throw this coin
        // away and generate a new one.
        if (IsValidCoinValue(commitmentValue))
            return;

        //Did not create a valid commitment value.
        //Change randomness to something new and random and try again
        attempts256++;
        hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end(),
                              attempts256.begin(), attempts256.end());
        BigNumRandomness.setuint256(hashRandomness);
        bnRandomness = (bnRandomness + BigNumRandomness) % params->coinCommitmentGroup.groupOrder;
        commitmentValue = commitmentValue.mul_mod(params->coinCommitmentGroup.h.pow_mod(BigNumRandomness, params->coinCommitmentGroup.modulus), params->coinCommitmentGroup.modulus);
    }
}

uint512 CzPIVWallet::GetNextZerocoinSeed()
{
    return GetZerocoinSeed(nCount);
}

uint512 CzPIVWallet::GetZerocoinSeed(uint32_t n)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << seedMaster << n;
    uint512 zerocoinSeed = Hash512(ss.begin(), ss.end());
    return zerocoinSeed;
}

void CzPIVWallet::UpdateCount()
{
    nCount++;
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteZPIVCount(nCount);
}

void CzPIVWallet::GenerateDeterministicZPIV(CoinDenomination denom, PrivateCoin& coin, bool fGenerateOnly)
{
    uint512 seedZerocoin = GetNextZerocoinSeed();
    CBigNum bnSerial;
    CBigNum bnRandomness;
    SeedToZPIV(seedZerocoin, bnSerial, bnRandomness);
    coin = PrivateCoin(Params().Zerocoin_Params(), denom, bnSerial, bnRandomness);

    if (fGenerateOnly)
        return;

    //TODO remove this leak of seed from logs before merge to master
    LogPrintf("%s : Generated new deterministic mint. Count=%d pubcoin=%s seed=%s\n", __func__, nCount, coin.getPublicCoin().getValue().GetHex().substr(0,6), seedZerocoin.GetHex().substr(0, 4));

    //set to the next count
    UpdateCount();
}
