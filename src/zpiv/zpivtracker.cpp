// Copyright (c) 2018-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <zpiv/deterministicmint.h>
#include "zpivtracker.h"
#include "util.h"
#include "sync.h"
#include "main.h"
#include "txdb.h"
#include "wallet/walletdb.h"
#include "zpiv/accumulators.h"
#include "zpiv/zpivwallet.h"
#include "witness.h"


CzPIVTracker::CzPIVTracker(std::string strWalletFile)
{
    this->strWalletFile = strWalletFile;
    mapSerialHashes.clear();
    mapPendingSpends.clear();
    fInitialized = false;
}

CzPIVTracker::~CzPIVTracker()
{
    mapSerialHashes.clear();
    mapPendingSpends.clear();
}

void CzPIVTracker::Init()
{
    //Load all CZerocoinMints and CDeterministicMints from the database
    if (!fInitialized) {
        ListMints(false, false, true);
        fInitialized = true;
    }
}

bool CzPIVTracker::Archive(CMintMeta& meta)
{
    if (mapSerialHashes.count(meta.hashSerial))
        mapSerialHashes.at(meta.hashSerial).isArchived = true;

    CWalletDB walletdb(strWalletFile);
    CZerocoinMint mint;
    if (walletdb.ReadZerocoinMint(meta.hashPubcoin, mint)) {
        if (!CWalletDB(strWalletFile).ArchiveMintOrphan(mint))
            return error("%s: failed to archive zerocoinmint", __func__);
    } else {
        //failed to read mint from DB, try reading deterministic
        CDeterministicMint dMint;
        if (!walletdb.ReadDeterministicMint(meta.hashPubcoin, dMint))
            return error("%s: could not find pubcoinhash %s in db", __func__, meta.hashPubcoin.GetHex());
        if (!walletdb.ArchiveDeterministicOrphan(dMint))
            return error("%s: failed to archive deterministic ophaned mint", __func__);
    }

    LogPrintf("%s: archived pubcoinhash %s\n", __func__, meta.hashPubcoin.GetHex());
    return true;
}

bool CzPIVTracker::UnArchive(const uint256& hashPubcoin, bool isDeterministic)
{
    CWalletDB walletdb(strWalletFile);
    if (isDeterministic) {
        CDeterministicMint dMint;
        if (!walletdb.UnarchiveDeterministicMint(hashPubcoin, dMint))
            return error("%s: failed to unarchive deterministic mint", __func__);
        Add(dMint, false);
    } else {
        CZerocoinMint mint;
        if (!walletdb.UnarchiveZerocoinMint(hashPubcoin, mint))
            return error("%s: failed to unarchivezerocoin mint", __func__);
        Add(mint, false);
    }

    LogPrintf("%s: unarchived %s\n", __func__, hashPubcoin.GetHex());
    return true;
}

CMintMeta CzPIVTracker::Get(const uint256 &hashSerial)
{
    if (!mapSerialHashes.count(hashSerial))
        return CMintMeta();

    return mapSerialHashes.at(hashSerial);
}

CMintMeta CzPIVTracker::GetMetaFromPubcoin(const uint256& hashPubcoin)
{
    for (auto it : mapSerialHashes) {
        CMintMeta meta = it.second;
        if (meta.hashPubcoin == hashPubcoin)
            return meta;
    }

    return CMintMeta();
}

bool CzPIVTracker::GetMetaFromStakeHash(const uint256& hashStake, CMintMeta& meta) const
{
    for (auto& it : mapSerialHashes) {
        if (it.second.hashStake == hashStake) {
            meta = it.second;
            return true;
        }
    }

    return false;
}

CoinWitnessData* CzPIVTracker::GetSpendCache(const uint256& hashStake)
{
    AssertLockHeld(cs_spendcache);
    if (!mapStakeCache.count(hashStake)) {
        std::unique_ptr<CoinWitnessData> uptr(new CoinWitnessData());
        mapStakeCache.insert(std::make_pair(hashStake, std::move(uptr)));
        return mapStakeCache.at(hashStake).get();
    }

    return mapStakeCache.at(hashStake).get();
}

bool CzPIVTracker::ClearSpendCache()
{
    AssertLockHeld(cs_spendcache);
    if (!mapStakeCache.empty()) {
        mapStakeCache.clear();
        return true;
    }

    return false;
}

std::vector<uint256> CzPIVTracker::GetSerialHashes()
{
    std::vector<uint256> vHashes;
    for (auto it : mapSerialHashes) {
        if (it.second.isArchived)
            continue;

        vHashes.emplace_back(it.first);
    }


    return vHashes;
}

CAmount CzPIVTracker::GetBalance(bool fConfirmedOnly, bool fUnconfirmedOnly) const
{
    CAmount nTotal = 0;
    //! zerocoin specific fields
    std::map<libzerocoin::CoinDenomination, unsigned int> myZerocoinSupply;
    for (auto& denom : libzerocoin::zerocoinDenomList) {
        myZerocoinSupply.insert(std::make_pair(denom, 0));
    }

    {
        //LOCK(cs_pivtracker);
        // Get Unused coins
        for (auto& it : mapSerialHashes) {
            CMintMeta meta = it.second;
            if (meta.isUsed || meta.isArchived)
                continue;
            bool fConfirmed = ((meta.nHeight < chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations()) && !(meta.nHeight == 0));
            if (fConfirmedOnly && !fConfirmed)
                continue;
            if (fUnconfirmedOnly && fConfirmed)
                continue;

            nTotal += libzerocoin::ZerocoinDenominationToAmount(meta.denom);
            myZerocoinSupply.at(meta.denom)++;
        }
    }

    if (nTotal < 0 ) nTotal = 0; // Sanity never hurts

    return nTotal;
}

CAmount CzPIVTracker::GetUnconfirmedBalance() const
{
    return GetBalance(false, true);
}

std::vector<CMintMeta> CzPIVTracker::GetMints(bool fConfirmedOnly) const
{
    std::vector<CMintMeta> vMints;
    for (auto& it : mapSerialHashes) {
        CMintMeta mint = it.second;
        if (mint.isArchived || mint.isUsed)
            continue;
        bool fConfirmed = (mint.nHeight < chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations());
        if (fConfirmedOnly && !fConfirmed)
            continue;
        vMints.emplace_back(mint);
    }
    return vMints;
}

//Does a mint in the tracker have this txid
bool CzPIVTracker::HasMintTx(const uint256& txid)
{
    for (auto it : mapSerialHashes) {
        if (it.second.txid == txid)
            return true;
    }

    return false;
}

bool CzPIVTracker::HasPubcoin(const CBigNum &bnValue) const
{
    // Check if this mint's pubcoin value belongs to our mapSerialHashes (which includes hashpubcoin values)
    uint256 hash = GetPubCoinHash(bnValue);
    return HasPubcoinHash(hash);
}

bool CzPIVTracker::HasPubcoinHash(const uint256& hashPubcoin) const
{
    for (auto it : mapSerialHashes) {
        CMintMeta meta = it.second;
        if (meta.hashPubcoin == hashPubcoin)
            return true;
    }
    return false;
}

bool CzPIVTracker::HasSerial(const CBigNum& bnSerial) const
{
    uint256 hash = GetSerialHash(bnSerial);
    return HasSerialHash(hash);
}

bool CzPIVTracker::HasSerialHash(const uint256& hashSerial) const
{
    auto it = mapSerialHashes.find(hashSerial);
    return it != mapSerialHashes.end();
}

bool CzPIVTracker::UpdateZerocoinMint(const CZerocoinMint& mint)
{
    if (!HasSerial(mint.GetSerialNumber()))
        return error("%s: mint %s is not known", __func__, mint.GetValue().GetHex());

    uint256 hashSerial = GetSerialHash(mint.GetSerialNumber());

    //Update the meta object
    CMintMeta meta = Get(hashSerial);
    meta.isUsed = mint.IsUsed();
    meta.denom = mint.GetDenomination();
    meta.nHeight = mint.GetHeight();
    mapSerialHashes.at(hashSerial) = meta;

    //Write to db
    return CWalletDB(strWalletFile).WriteZerocoinMint(mint);
}

bool CzPIVTracker::UpdateState(const CMintMeta& meta)
{
    CWalletDB walletdb(strWalletFile);

    if (meta.isDeterministic) {
        CDeterministicMint dMint;
        if (!walletdb.ReadDeterministicMint(meta.hashPubcoin, dMint)) {
            // Check archive just in case
            if (!meta.isArchived)
                return error("%s: failed to read deterministic mint from database", __func__);

            // Unarchive this mint since it is being requested and updated
            if (!walletdb.UnarchiveDeterministicMint(meta.hashPubcoin, dMint))
                return error("%s: failed to unarchive deterministic mint from database", __func__);
        }

        dMint.SetTxHash(meta.txid);
        dMint.SetHeight(meta.nHeight);
        dMint.SetUsed(meta.isUsed);
        dMint.SetDenomination(meta.denom);
        dMint.SetStakeHash(meta.hashStake);

        if (!walletdb.WriteDeterministicMint(dMint))
            return error("%s: failed to update deterministic mint when writing to db", __func__);
    } else {
        CZerocoinMint mint;
        if (!walletdb.ReadZerocoinMint(meta.hashPubcoin, mint))
            return error("%s: failed to read mint from database", __func__);

        mint.SetTxHash(meta.txid);
        mint.SetHeight(meta.nHeight);
        mint.SetUsed(meta.isUsed);
        mint.SetDenomination(meta.denom);

        if (!walletdb.WriteZerocoinMint(mint))
            return error("%s: failed to write mint to database", __func__);
    }

    mapSerialHashes[meta.hashSerial] = meta;

    return true;
}

void CzPIVTracker::Add(const CDeterministicMint& dMint, bool isNew, bool isArchived, CzPIVWallet* zPIVWallet)
{
    bool iszPIVWalletInitialized = (NULL != zPIVWallet);
    CMintMeta meta;
    meta.hashPubcoin = dMint.GetPubcoinHash();
    meta.nHeight = dMint.GetHeight();
    meta.nVersion = dMint.GetVersion();
    meta.txid = dMint.GetTxHash();
    meta.isUsed = dMint.IsUsed();
    meta.hashSerial = dMint.GetSerialHash();
    meta.hashStake = dMint.GetStakeHash();
    meta.denom = dMint.GetDenomination();
    meta.isArchived = isArchived;
    meta.isDeterministic = true;
    if (! iszPIVWalletInitialized)
        zPIVWallet = new CzPIVWallet(strWalletFile);
    meta.isSeedCorrect = zPIVWallet->CheckSeed(dMint);
    if (! iszPIVWalletInitialized)
        delete zPIVWallet;
    mapSerialHashes[meta.hashSerial] = meta;

    if (isNew)
        CWalletDB(strWalletFile).WriteDeterministicMint(dMint);
}

void CzPIVTracker::Add(const CZerocoinMint& mint, bool isNew, bool isArchived)
{
    CMintMeta meta;
    meta.hashPubcoin = GetPubCoinHash(mint.GetValue());
    meta.nHeight = mint.GetHeight();
    meta.nVersion = libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber());
    meta.txid = mint.GetTxHash();
    meta.isUsed = mint.IsUsed();
    meta.hashSerial = GetSerialHash(mint.GetSerialNumber());
    uint256 nSerial = mint.GetSerialNumber().getuint256();
    meta.hashStake = Hash(nSerial.begin(), nSerial.end());
    meta.denom = mint.GetDenomination();
    meta.isArchived = isArchived;
    meta.isDeterministic = false;
    meta.isSeedCorrect = true;
    mapSerialHashes[meta.hashSerial] = meta;

    if (isNew)
        CWalletDB(strWalletFile).WriteZerocoinMint(mint);
}

void CzPIVTracker::SetPubcoinUsed(const uint256& hashPubcoin, const uint256& txid)
{
    if (!HasPubcoinHash(hashPubcoin))
        return;
    CMintMeta meta = GetMetaFromPubcoin(hashPubcoin);
    meta.isUsed = true;
    mapPendingSpends.insert(std::make_pair(meta.hashSerial, txid));
    UpdateState(meta);
}

void CzPIVTracker::SetPubcoinNotUsed(const uint256& hashPubcoin)
{
    if (!HasPubcoinHash(hashPubcoin))
        return;
    CMintMeta meta = GetMetaFromPubcoin(hashPubcoin);
    meta.isUsed = false;

    if (mapPendingSpends.count(meta.hashSerial))
        mapPendingSpends.erase(meta.hashSerial);

    UpdateState(meta);
}

void CzPIVTracker::RemovePending(const uint256& txid)
{
    uint256 hashSerial;
    for (auto it : mapPendingSpends) {
        if (it.second == txid) {
            hashSerial = it.first;
            break;
        }
    }

    if (hashSerial > 0)
        mapPendingSpends.erase(hashSerial);
}

bool CzPIVTracker::UpdateStatusInternal(const std::set<uint256>& setMempool, CMintMeta& mint)
{
    //! Check whether this mint has been spent and is considered 'pending' or 'confirmed'
    // If there is not a record of the block height, then look it up and assign it
    uint256 txidMint;
    bool isMintInChain = zerocoinDB->ReadCoinMint(mint.hashPubcoin, txidMint);

    //See if there is internal record of spending this mint (note this is memory only, would reset on restart)
    bool isPendingSpend = static_cast<bool>(mapPendingSpends.count(mint.hashSerial));

    // See if there is a blockchain record of spending this mint
    uint256 txidSpend;
    bool isConfirmedSpend = zerocoinDB->ReadCoinSpend(mint.hashSerial, txidSpend);

    // Double check the mempool for pending spend
    if (isPendingSpend) {
        uint256 txidPendingSpend = mapPendingSpends.at(mint.hashSerial);
        if (!setMempool.count(txidPendingSpend) || isConfirmedSpend) {
            RemovePending(txidPendingSpend);
            isPendingSpend = false;
            LogPrintf("%s : Pending txid %s removed because not in mempool\n", __func__, txidPendingSpend.GetHex());
        }
    }

    bool isUsed = isPendingSpend || isConfirmedSpend;

    if (!mint.nHeight || !isMintInChain || isUsed != mint.isUsed) {
        CTransaction tx;
        uint256 hashBlock;

        // Txid will be marked 0 if there is no knowledge of the final tx hash yet
        if (mint.txid == 0) {
            if (!isMintInChain) {
                LogPrintf("%s : Failed to find mint in zerocoinDB %s\n", __func__, mint.hashPubcoin.GetHex().substr(0, 6));
                mint.isArchived = true;
                Archive(mint);
                return true;
            }
            mint.txid = txidMint;
        }

        if (setMempool.count(mint.txid))
            return true;

        // Check the transaction associated with this mint
        if (!IsInitialBlockDownload() && !GetTransaction(mint.txid, tx, hashBlock, true)) {
            LogPrintf("%s : Failed to find tx for mint txid=%s\n", __func__, mint.txid.GetHex());
            mint.isArchived = true;
            Archive(mint);
            return true;
        }

        // An orphan tx if hashblock is in mapBlockIndex but not in chain active
        if (mapBlockIndex.count(hashBlock) && !chainActive.Contains(mapBlockIndex.at(hashBlock))) {
            LogPrintf("%s : Found orphaned mint txid=%s\n", __func__, mint.txid.GetHex());
            mint.isUsed = false;
            mint.nHeight = 0;
            if (tx.IsCoinStake()) {
                mint.isArchived = true;
                Archive(mint);
            }

            return true;
        }

        // Check that the mint has correct used status
        if (mint.isUsed != isUsed) {
            LogPrintf("%s : Set mint %s isUsed to %d\n", __func__, mint.hashPubcoin.GetHex(), isUsed);
            mint.isUsed = isUsed;
            return true;
        }
    }

    return false;
}

std::set<CMintMeta> CzPIVTracker::ListMints(bool fUnusedOnly, bool fMatureOnly, bool fUpdateStatus, bool fWrongSeed, bool fExcludeV1)
{
    CWalletDB walletdb(strWalletFile);
    if (fUpdateStatus) {
        std::list<CZerocoinMint> listMintsDB = walletdb.ListMintedCoins();
        for (auto& mint : listMintsDB)
            Add(mint);
        LogPrint("zero", "%s: added %d zerocoinmints from DB\n", __func__, listMintsDB.size());

        std::list<CDeterministicMint> listDeterministicDB = walletdb.ListDeterministicMints();

        CzPIVWallet* zPIVWallet = new CzPIVWallet(strWalletFile);
        for (auto& dMint : listDeterministicDB) {
            if (fExcludeV1 && dMint.GetVersion() < 2)
                continue;
            Add(dMint, false, false, zPIVWallet);
        }
        delete zPIVWallet;
        LogPrint("zero", "%s: added %d dzpiv from DB\n", __func__, listDeterministicDB.size());
    }

    std::vector<CMintMeta> vOverWrite;
    std::set<CMintMeta> setMints;
    std::set<uint256> setMempool;
    {
        LOCK(mempool.cs);
        mempool.getTransactions(setMempool);
    }

    std::map<libzerocoin::CoinDenomination, int> mapMaturity = GetMintMaturityHeight();
    for (auto& it : mapSerialHashes) {
        CMintMeta mint = it.second;

        //This is only intended for unarchived coins
        if (mint.isArchived)
            continue;

        // Update the metadata of the mints if requested
        if (fUpdateStatus && UpdateStatusInternal(setMempool, mint)) {
            if (mint.isArchived)
                continue;

            // Mint was updated, queue for overwrite
            vOverWrite.emplace_back(mint);
        }

        if (fUnusedOnly && mint.isUsed)
            continue;

        if (fMatureOnly) {
            // Not confirmed
            if (!mint.nHeight || mint.nHeight > chainActive.Height() - Params().Zerocoin_MintRequiredConfirmations())
                continue;
            if (mint.nHeight >= mapMaturity.at(mint.denom))
                continue;
        }

        if (!fWrongSeed && !mint.isSeedCorrect)
            continue;

        setMints.insert(mint);
    }

    //overwrite any updates
    for (CMintMeta& meta : vOverWrite)
        UpdateState(meta);

    return setMints;
}

void CzPIVTracker::Clear()
{
    mapSerialHashes.clear();
}
