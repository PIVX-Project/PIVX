// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"

#include "coincontrol.h"
#include "init.h"
#include "guiinterfaceutil.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "policy/policy.h"
#include "script/sign.h"
#include "spork.h"
#include "swifttx.h"    // mapTxLockReq
#include "util.h"
#include "utilmoneystr.h"
#include "zpivchain.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>

CWallet* pwalletMain = nullptr;
/**
 * Settings
 */
CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;
unsigned int nTxConfirmTarget = 1;
bool bdisableSystemnotifications = false; // Those bubbles can be annoying and slow down the UI when you get lots of trx
bool fSendFreeTransactions = false;
bool fPayAtLeastCustomFee = true;
bool bSpendZeroConfChange = DEFAULT_SPEND_ZEROCONF_CHANGE;

const char * DEFAULT_WALLET_DAT = "wallet.dat";

/**
 * Fees smaller than this (in upiv) are considered zero fee (for transaction creation)
 * We are ~100 times smaller then bitcoin now (2015-06-23), set minTxFee 10 times higher
 * so it's still 10 times lower comparing to bitcoin.
 * Override with -mintxfee
 */
CFeeRate CWallet::minTxFee = CFeeRate(10000);

/**
 * minimum accpeted value for stake split threshold
 */
CAmount CWallet::minStakeSplitThreshold = DEFAULT_MIN_STAKE_SPLIT_THRESHOLD;

const uint256 CMerkleTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

/** @defgroup mapWallet
 *
 * @{
 */

struct CompareValueOnly {
    bool operator()(const std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> >& t1,
        const std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->vout[i].nValue));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

bool CWallet::SetupSPKM(bool newKeypool)
{
    if (m_spk_man->SetupGeneration(newKeypool, true)) {
        LogPrintf("%s : spkm setup completed\n", __func__);
        return ActivateSaplingWallet();
    }
    return false;
}

bool CWallet::ActivateSaplingWallet()
{
    if (m_sspk_man->SetupGeneration(m_spk_man->GetHDChain().GetID())) {
        LogPrintf("%s : sapling spkm setup completed\n", __func__);
        return true;
    }
    return false;
}

bool CWallet::IsHDEnabled() const
{
    return m_spk_man->IsHDEnabled();
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return NULL;
    return &(it->second);
}

std::vector<CWalletTx> CWallet::getWalletTxs()
{
    LOCK(cs_wallet);
    std::vector<CWalletTx> result;
    result.reserve(mapWallet.size());
    for (const auto& entry : mapWallet) {
        result.emplace_back(entry.second);
    }
    return result;
}

PairResult CWallet::getNewAddress(CTxDestination& ret, std::string label){
    return getNewAddress(ret, label, AddressBook::AddressBookPurpose::RECEIVE);
}

PairResult CWallet::getNewStakingAddress(CTxDestination& ret, std::string label){
    return getNewAddress(ret, label, AddressBook::AddressBookPurpose::COLD_STAKING, CChainParams::Base58Type::STAKING_ADDRESS);
}

PairResult CWallet::getNewAddress(CTxDestination& ret, const std::string addressLabel, const std::string purpose,
                                         const CChainParams::Base58Type addrType)
{
    LOCK(cs_wallet);

    // Refill keypool if wallet is unlocked
    if (!IsLocked())
        TopUpKeyPool();

    uint8_t type = (addrType == CChainParams::Base58Type::STAKING_ADDRESS ? HDChain::ChangeType::STAKING : HDChain::ChangeType::EXTERNAL);
    CPubKey newKey;
    // Get a key
    if (!GetKeyFromPool(newKey, type)) {
        // inform the user to top-up the keypool or unlock the wallet
        return PairResult(false, new std::string(
                        _("Keypool ran out, please call keypoolrefill first, or unlock the wallet.")));
    }
    CKeyID keyID = newKey.GetID();

    if (!SetAddressBook(keyID, addressLabel, purpose))
        throw std::runtime_error("CWallet::getNewAddress() : SetAddressBook failed");

    ret = CTxDestination(keyID);
    return PairResult(true);
}

int64_t CWallet::GetKeyCreationTime(CPubKey pubkey)
{
    return mapKeyMetadata[pubkey.GetID()].nCreateTime;
}

int64_t CWallet::GetKeyCreationTime(const CTxDestination& address)
{
    const CKeyID* keyID = boost::get<CKeyID>(&address);
    if (keyID) {
        CPubKey keyRet;
        if (GetPubKey(*keyID, keyRet)) {
            return GetKeyCreationTime(keyRet);
        }
    }
    return 0;
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey& pubkey)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey))
        return false;

    // TODO: Move the follow block entirely inside the spkm (including WriteKey to AddKeyPubKeyWithDB)
    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);

    script = GetScriptForRawPubKey(pubkey);
    if (HaveWatchOnly(script)) {
        RemoveWatchOnly(script);
    }

    if (!fFileBacked)
        return true;
    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteKey(
                pubkey,
                secret.GetPrivKey(),
                mapKeyMetadata[pubkey.GetID()]);
    }
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey& vchPubKey,
    const std::vector<unsigned char>& vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey,
                vchCryptedSecret,
                mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}

bool CWallet::LoadKeyMetadata(const CPubKey& pubkey, const CKeyMetadata& meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        std::string strAddr = EncodeDestination(CScriptID(redeemScript));
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CScript& dest)
{
    if (!CCryptoKeyStore::AddWatchOnly(dest))
        return false;
    nTimeFirstKey = 1; // No birthday information for watch-only keys.
    NotifyWatchonlyChanged(true);
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript& dest)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveWatchOnly(dest))
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (fFileBacked)
        if (!CWalletDB(strWalletFile).EraseWatchOnly(dest))
            return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript& dest)
{
    return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase, bool stakingOnly)
{
    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        for (const MasterKeyMap::value_type& pMasterKey : mapMasterKeys) {
            if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                continue; // try another master key
            if (Unlock(vMasterKey)) {
                fWalletUnlockStaking = stakingOnly;
                return true;
            }
        }
    }
    return false;
}

bool CWallet::Lock()
{
    if (!SetCrypted())
        return false;

    {
        LOCK(cs_KeyStore);
        vMasterKey.clear();
        if (zwallet) zwallet->Lock();
    }

    NotifyStatusChanged(this);
    return true;
}

bool CWallet::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        bool keyPass = false;
        bool keyFail = false;
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi) {
            const CPubKey& vchPubKey = (*mi).second.first;
            const std::vector<unsigned char>& vchCryptedSecret = (*mi).second.second;
            CKeyingMaterial vchSecret;
            if (!DecryptSecret(vMasterKeyIn, vchCryptedSecret, vchPubKey.GetHash(), vchSecret)) {
                keyFail = true;
                break;
            }
            if (vchSecret.size() != 32) {
                keyFail = true;
                break;
            }
            CKey key;
            key.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
            if (key.GetPubKey() != vchPubKey) {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }

        if (keyPass && keyFail) {
            LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.\n");
            throw std::runtime_error("Error unlocking wallet: some keys decrypt but not all. Your wallet file may be corrupt.");
        }

        if (keyFail || !keyPass)
            return false;

        // Sapling
        if (!UnlockSaplingKeys(vMasterKeyIn, fDecryptionThoroughlyChecked)) {
            // If Sapling key encryption fail, let's unencrypt the rest of the keys
            LogPrintf("Sapling wallet unlock keys failed\n");
            throw std::runtime_error("Error unlocking wallet: some Sapling keys decrypt but not all. Your wallet file may be corrupt.");
        }

        vMasterKey = vMasterKeyIn;
        fDecryptionThoroughlyChecked = true;

        if (zwallet) {
            uint256 hashSeed;
            if (CWalletDB(strWalletFile).ReadCurrentSeedHash(hashSeed)) {
                uint256 nSeed;
                if (!GetDeterministicSeed(hashSeed, nSeed)) {
                    return error("Failed to read zPIV seed from DB. Wallet is probably corrupt.");
                }
                zwallet->SetMasterSeed(nSeed, false);
            }
        }
    }

    NotifyStatusChanged(this);
    return true;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();
    SecureString strOldWalletPassphraseFinal = strOldWalletPassphrase;

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        for (MasterKeyMap::value_type& pMasterKey : mapMasterKeys) {
            if (!crypter.SetKeyFromPassphrase(strOldWalletPassphraseFinal, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (Unlock(vMasterKey)) {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();

                return true;
            }
        }
    }

    return false;
}

void CWallet::ChainTipAdded(const CBlockIndex *pindex,
                            const CBlock *pblock,
                            SaplingMerkleTree saplingTree)
{
    IncrementNoteWitnesses(pindex, pblock, saplingTree);
    m_sspk_man->UpdateSaplingNullifierNoteMapForBlock(pblock);
}

void CWallet::ChainTip(const CBlockIndex *pindex,
                       const CBlock *pblock,
                       Optional<SaplingMerkleTree> added)
{
    if (added) {
        ChainTipAdded(pindex, pblock, added.get());
    } else {
        DecrementNoteWitnesses(pindex);
        m_sspk_man->UpdateSaplingNullifierNoteMapForBlock(pblock);
    }
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    SetBestChainInternal(walletdb, loc);
}

void CWallet::SetBestChainInternal(CWalletDB& walletdb, const CBlockLocator& loc)
{
    if (!walletdb.TxnBegin()) {
        // This needs to be done atomically, so don't do it at all
        LogPrintf("%s: Couldn't start atomic write\n", __func__);
        return;
    }

    // Store the best block
    if (!walletdb.WriteBestBlock(loc)) {
        LogPrintf("SetBestChain(): Failed to write best block, aborting atomic write\n");
        walletdb.TxnAbort();
        return;
    }

    // Store sapling witness cache size
    if (m_sspk_man->nWitnessCacheNeedsUpdate) {
        if (!walletdb.WriteWitnessCacheSize(m_sspk_man->nWitnessCacheSize)) {
            LogPrintf("%s: Failed to write nWitnessCacheSize\n", __func__);
            walletdb.TxnAbort();
            return;
        }
    }

    if (!walletdb.TxnCommit()) {
        // Couldn't commit all to db, but in-memory state is fine
        LogPrintf("%s: Couldn't commit atomic write\n", __func__);
        return;
    }

    // Reset cache if the commit succeed and is needed.
    if (m_sspk_man->nWitnessCacheNeedsUpdate) {
        m_sspk_man->nWitnessCacheNeedsUpdate = false;
    }
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
        nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked) {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

std::set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    std::set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    for (const CTxIn& txin : wtx.vin) {
        if (mapTxSpends.count(txin.prevout) <= 1 || wtx.HasZerocoinSpendInputs())
            continue; // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
            result.insert(it->second);
    }

    // Sapling
    if (HasSaplingSPKM()) {
        m_sspk_man->GetConflicts(wtx, result);
    }

    return result;
}

void CWallet::SyncMetaDataN(std::pair<TxSpendMap<uint256>::iterator, TxSpendMap<uint256>::iterator> range)
{
    SyncMetaData<uint256>(range);
}

template <class T>
void CWallet::SyncMetaData(std::pair<typename TxSpendMap<T>::iterator, typename TxSpendMap<T>::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = nullptr;
    for (typename TxSpendMap<T>::iterator it = range.first; it != range.second; ++it) {
        const CWalletTx* wtx = &mapWallet.at(it->second);
        int n = wtx->nOrderPos;
        if (n < nMinOrderPos) {
            nMinOrderPos = n;
            copyFrom = wtx;
        }
    }

    if (!copyFrom) {
        return;
    }

    // Now copy data from copyFrom to rest:
    for (auto it = range.first; it != range.second; ++it) {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet[hash];
        if (copyFrom == copyTo) continue;
        assert(copyFrom && "Oldest wallet transaction in range assumed to have been found.");
        //if (!copyFrom->IsEquivalentTo(*copyTo)) continue;
        copyTo->mapValue = copyFrom->mapValue;
        copyTo->vOrderForm = copyFrom->vOrderForm;
        // fTimeReceivedIsTxTime not copied on purpose
        // nTimeReceived not copied on purpose
        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
        // nOrderPos not copied on purpose
        // cached members not copied on purpose
    }
}

///////// Init ////////////////

bool CWallet::ParameterInteraction()
{
    if (mapArgs.count("-mintxfee")) {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-mintxfee"], n) && n > 0)
            CWallet::minTxFee = CFeeRate(n);
        else
            return UIError(AmountErrMsg("mintxfee", mapArgs["-mintxfee"]));
    }
    if (mapArgs.count("-paytxfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(mapArgs["-paytxfee"], nFeePerK))
            return UIError(AmountErrMsg("paytxfee", mapArgs["-paytxfee"]));
        if (nFeePerK > nHighTransactionFeeWarning)
            UIWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee) {
            return UIError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                       mapArgs["-paytxfee"], ::minRelayTxFee.ToString()));
        }
    }
    if (mapArgs.count("-maxtxfee")) {
        CAmount nMaxFee = 0;
        if (!ParseMoney(mapArgs["-maxtxfee"], nMaxFee))
            return UIError(AmountErrMsg("maxtxfee", mapArgs["-maxtxfee"]));
        if (nMaxFee > nHighTransactionMaxFeeWarning)
            UIWarning(_("Warning: -maxtxfee is set very high! Fees this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee) {
            return UIError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                       mapArgs["-maxtxfee"], ::minRelayTxFee.ToString()));
        }
    }
    if (mapArgs.count("-minstakesplit")) {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-minstakesplit"], n) && n > 0)
            CWallet::minStakeSplitThreshold = n;
        else
            return UIError(AmountErrMsg("minstakesplit", mapArgs["-minstakesplit"]));
    }
    nTxConfirmTarget = GetArg("-txconfirmtarget", 1);
    bSpendZeroConfChange = GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);
    bdisableSystemnotifications = GetBoolArg("-disablesystemnotifications", false);
    fSendFreeTransactions = GetBoolArg("-sendfreetransactions", DEFAULT_SEND_FREE_TRANSACTIONS);

    return true;
}

//////// End Init ////////////

const CKeyingMaterial& CWallet::GetEncryptionKey() const
{
    return vMasterKey;
}

bool CWallet::HasEncryptionKeys() const
{
    return !mapMasterKeys.empty();
}

ScriptPubKeyMan* CWallet::GetScriptPubKeyMan() const
{
    return m_spk_man.get();
}

bool CWallet::HasSaplingSPKM() const
{
    return GetSaplingScriptPubKeyMan()->IsEnabled();
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
    const COutPoint outpoint(hash, n);
    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    for (TxSpends::const_iterator it = range.first; it != range.second; ++it) {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end()) {
            bool fConflicted;
            const int nDepth = mit->second.GetDepthAndMempool(fConflicted);
            // not in mempool txes can spend coins only if not coinstakes
            const bool fConflictedCoinstake = fConflicted && mit->second.IsCoinStake();
            if (nDepth > 0  || (nDepth == 0 && !mit->second.isAbandoned() && !fConflictedCoinstake) )
                return true; // Spent
        }
    }
    return false;
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.emplace(outpoint, wtxid);
    setLockedCoins.erase(outpoint);

    std::pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData<COutPoint>(range);
}

void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    for (const CTxIn& txin : thisTx.vin)
        AddToSpends(txin.prevout, wtxid);

    if (CanSupportFeature(FEATURE_SAPLING) && thisTx.sapData) {
        for (const SpendDescription &spend : thisTx.sapData->vShieldedSpend) {
            GetSaplingScriptPubKeyMan()->AddToSaplingSpends(spend.nullifier, wtxid);
        }
    }
}

bool CWallet::GetVinAndKeysFromOutput(COutput out, CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, bool fColdStake)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    CScript pubScript;

    txinRet = CTxIn(out.tx->GetHash(), out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

    CTxDestination address1;
    ExtractDestination(pubScript, address1, fColdStake);

    CKeyID* keyID = boost::get<CKeyID>(&address1);
    if (!keyID) {
        LogPrintf("CWallet::GetVinAndKeysFromOutput -- Address does not refer to a key\n");
        return false;
    }

    if (!GetKey(*keyID, keyRet)) {
        LogPrintf("CWallet::GetVinAndKeysFromOutput -- Private key for address is not known\n");
        return false;
    }

    pubKeyRet = keyRet.GetPubKey();
    return true;
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetStrongRandBytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked) {
            assert(!pwalletdbEncryption);
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin()) {
                delete pwalletdbEncryption;
                pwalletdbEncryption = NULL;
                return false;
            }
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey) || (m_sspk_man->IsEnabled() && !m_sspk_man->EncryptSaplingKeys(vMasterKey))) {
            if (fFileBacked) {
                pwalletdbEncryption->TxnAbort();
                delete pwalletdbEncryption;
            }
            // We now probably have half of our keys encrypted in memory, and half not...
            // die and let the user reload their unencrypted wallet.
            assert(false);
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked) {
            if (!pwalletdbEncryption->TxnCommit()) {
                delete pwalletdbEncryption;
                // We now have keys encrypted in memory, but not on disk...
                // die to avoid confusion and let the user reload their unencrypted wallet.
                assert(false);
            }

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }
        Lock();
        Unlock(strWalletPassphrase);
        // if we are using HD, replace the HD seed with a new one
        if (m_spk_man->IsHDEnabled()) {
            if (!m_spk_man->SetupGeneration(true, true)) {
                return false;
            }
        }
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);
    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB* pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

bool CWallet::IsKeyUsed(const CPubKey& vchPubKey) {
    if (vchPubKey.IsValid()) {
        CScript scriptPubKey = GetScriptForDestination(vchPubKey.GetID());
        for (std::map<uint256, CWalletTx>::iterator it = mapWallet.begin();
             it != mapWallet.end() && vchPubKey.IsValid();
             ++it) {
            const CWalletTx& wtx = (*it).second;
            for (const CTxOut& txout : wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    return true;
        }
    }
    return false;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for (std::pair<const uint256, CWalletTx> & item : mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose)
{
    LOCK(cs_wallet);
    CWalletDB walletdb(strWalletFile, "r+", fFlushOnClose);
    uint256 hash = wtxIn.GetHash();

    // Inserts only if not already there, returns tx inserted or tx found
    std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.emplace(hash, wtxIn);
    CWalletTx& wtx = (*ret.first).second;
    wtx.BindWallet(this);
    // Sapling
    m_sspk_man->UpdateNullifierNoteMapWithTx(wtx);
    bool fInsertedNew = ret.second;
    if (fInsertedNew) {
        wtx.nTimeReceived = GetAdjustedTime();
        wtx.nOrderPos = IncOrderPosNext(&walletdb);
        wtxOrdered.emplace(wtx.nOrderPos, &wtx);
        wtx.UpdateTimeSmart();
        AddToSpends(hash);
    }

    bool fUpdated = false;
    if (!fInsertedNew) {
        // Merge
        if (!wtxIn.hashUnset() && wtxIn.hashBlock != wtx.hashBlock) {
            wtx.hashBlock = wtxIn.hashBlock;
            wtx.UpdateTimeSmart();
            fUpdated = true;
        }
        // If no longer abandoned, update
        if (wtxIn.hashBlock.IsNull() && wtx.isAbandoned()) {
            wtx.hashBlock = wtxIn.hashBlock;
            if (!fUpdated) wtx.UpdateTimeSmart();
            fUpdated = true;
        }
        if (wtxIn.nIndex != -1 && wtxIn.nIndex != wtx.nIndex) {
            wtx.nIndex = wtxIn.nIndex;
            fUpdated = true;
        }
        if (HasSaplingSPKM() && m_sspk_man->UpdatedNoteData(wtxIn, wtx)) {
            fUpdated = true;
        }
        if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe) {
            wtx.fFromMe = wtxIn.fFromMe;
            fUpdated = true;
        }
    }

    //// debug print
    LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

    // Write to disk
    if (fInsertedNew || fUpdated) {
        if (!walletdb.WriteTx(wtx))
            return false;
    }

    // Break debit/credit balance caches:
    wtx.MarkDirty();

    // Notify UI of new or updated transaction
    NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

    // notify an external script when a wallet transaction comes in or is updated
    std::string strCmd = GetArg("-walletnotify", "");

    if (!strCmd.empty()) {
        boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }
    return true;
}

bool CWallet::LoadToWallet(const CWalletTx& wtxIn)
{
    const uint256& hash = wtxIn.GetHash();
    mapWallet[hash] = wtxIn;
    CWalletTx& wtx = mapWallet[hash];
    wtx.BindWallet(this);
    // Sapling
    m_sspk_man->UpdateNullifierNoteMapWithTx(mapWallet[hash]);
    wtxOrdered.emplace(wtx.nOrderPos, &wtx);
    AddToSpends(hash);
    for (const CTxIn& txin : wtx.vin) {
        if (mapWallet.count(txin.prevout.hash)) {
            CWalletTx& prevtx = mapWallet[txin.prevout.hash];
            if (prevtx.nIndex == -1 && !prevtx.hashUnset()) {
                MarkConflicted(prevtx.hashBlock, wtx.GetHash());
            }
        }
    }
    return true;
}

/**
 * Add a transaction to the wallet, or update it.
 * pblock is optional, but should be provided if the transaction is known to be in a block.
 * If fUpdate is true, existing transactions will be updated.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlockIndex* pIndex, int posInBlock, bool fUpdate)
{
    {
        AssertLockHeld(cs_wallet);

        if (posInBlock != -1 && !tx.HasZerocoinSpendInputs() && !tx.IsCoinBase()) {
            for (const CTxIn& txin : tx.vin) {
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range = mapTxSpends.equal_range(txin.prevout);
                while (range.first != range.second) {
                    if (range.first->second != tx.GetHash()) {
                        LogPrintf("Transaction %s (in block %s) conflicts with wallet transaction %s (both spend %s:%i)\n", tx.GetHash().ToString(), pIndex->GetBlockHash().ToString(), range.first->second.ToString(), range.first->first.hash.ToString(), range.first->first.n);
                        MarkConflicted(pIndex->GetBlockHash(), range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;

        // Check tx for Sapling notes
        mapSaplingNoteData_t saplingNoteData;
        if (HasSaplingSPKM()) {
            auto saplingNoteDataAndAddressesToAdd = m_sspk_man->FindMySaplingNotes(tx);
            saplingNoteData = saplingNoteDataAndAddressesToAdd.first;
            auto addressesToAdd = saplingNoteDataAndAddressesToAdd.second;
            // Add my addresses
            for (const auto &addressToAdd : addressesToAdd) {
                if (!m_sspk_man->AddSaplingIncomingViewingKey(addressToAdd.second, addressToAdd.first)) {
                    return false;
                }
            }
        }

        if (fExisted || IsMine(tx) || IsFromMe(tx) || saplingNoteData.size() > 0) {

            /* Check if any keys in the wallet keypool that were supposed to be unused
             * have appeared in a new transaction. If so, remove those keys from the keypool.
             * This can happen when restoring an old wallet backup that does not contain
             * the mostly recently created transactions from newer versions of the wallet.
             */

            // loop though all outputs
            for (const CTxOut& txout: tx.vout) {
                m_spk_man->MarkUnusedAddresses(txout.scriptPubKey);
            }

            CWalletTx wtx(this, tx);

            if (saplingNoteData.size() > 0) {
                wtx.SetSaplingNoteData(saplingNoteData);
            }

            // Get merkle branch if transaction was found in a block
            if (posInBlock != -1)
                wtx.SetMerkleBranch(pIndex, posInBlock);

            return AddToWallet(wtx, false);
        }
    }
    return false;
}

bool CWallet::AbandonTransaction(const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    CWalletDB walletdb(strWalletFile, "r+");

    std::set<uint256> todo;
    std::set<uint256> done;

    // Can't mark abandoned if confirmed or in mempool
    assert(mapWallet.count(hashTx));
    CWalletTx& origtx = mapWallet[hashTx];
    if (origtx.GetDepthInMainChain() > 0 || origtx.InMempool()) {
        return false;
    }

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        assert(mapWallet.count(now));
        CWalletTx& wtx = mapWallet[now];
        int currentconfirm = wtx.GetDepthInMainChain();
        // If the orig tx was not in block, none of its spends can be
        assert(currentconfirm <= 0);
        // if (currentconfirm < 0) {Tx and spends are already conflicted, no need to abandon}
        if (currentconfirm == 0 && !wtx.isAbandoned()) {
            // If the orig tx was not in block/mempool, none of its spends can be in mempool
            assert(!wtx.InMempool());
            wtx.nIndex = -1;
            wtx.setAbandoned();
            wtx.MarkDirty();
            walletdb.WriteTx(wtx);
            NotifyTransactionChanged(this, wtx.GetHash(), CT_UPDATED);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them abandoned too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                if (!done.count(iter->second)) {
                    todo.insert(iter->second);
                }
                iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin: wtx.vin)
            {
                if (mapWallet.count(txin.prevout.hash))
                    mapWallet[txin.prevout.hash].MarkDirty();
            }
        }
    }

    return true;
}

void CWallet::MarkConflicted(const uint256& hashBlock, const uint256& hashTx)
{
    LOCK2(cs_main, cs_wallet);

    int conflictconfirms = 0;
    if (mapBlockIndex.count(hashBlock)) {
        CBlockIndex* pindex = mapBlockIndex[hashBlock];
        if (chainActive.Contains(pindex)) {
            conflictconfirms = -(chainActive.Height() - pindex->nHeight + 1);
        }
    }

    // If number of conflict confirms cannot be determined, this means
    // that the block is still unknown or not yet part of the main chain,
    // for example when loading the wallet during a reindex. Do nothing in that
    // case.
    if (conflictconfirms >= 0)
        return;

    // Do not flush the wallet here for performance reasons
    CWalletDB walletdb(strWalletFile, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        assert(mapWallet.count(now));
        CWalletTx& wtx = mapWallet[now];
        int currentconfirm = wtx.GetDepthInMainChain();
        if (conflictconfirms < currentconfirm) {
            // Block is 'more conflicted' than current confirm; update.
            // Mark transaction as conflicted with this block.
            wtx.nIndex = -1;
            wtx.hashBlock = hashBlock;
            wtx.MarkDirty();
            walletdb.WriteTx(wtx);
            // Iterate over all its outputs, and mark transactions in the wallet that spend them conflicted too
            TxSpends::const_iterator iter = mapTxSpends.lower_bound(COutPoint(now, 0));
            while (iter != mapTxSpends.end() && iter->first.hash == now) {
                 if (!done.count(iter->second)) {
                     todo.insert(iter->second);
                 }
                 iter++;
            }
            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be recomputed
            for (const CTxIn& txin: wtx.vin)
            {
                if (mapWallet.count(txin.prevout.hash))
                    mapWallet[txin.prevout.hash].MarkDirty();
            }
        }
    }
}

void CWallet::SyncTransaction(const CTransaction& tx, const CBlockIndex *pindex, int posInBlock)
{
    LOCK(cs_wallet);
    if (!AddToWalletIfInvolvingMe(tx, pindex, posInBlock, true))
        return; // Not one of ours

    MarkAffectedTransactionsDirty(tx);
}

void CWallet::MarkAffectedTransactionsDirty(const CTransaction& tx)
{
    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    for (const CTxIn& txin : tx.vin) {
        if (!txin.IsZerocoinSpend() && mapWallet.count(txin.prevout.hash))
            mapWallet[txin.prevout.hash].MarkDirty();
    }

    // Sapling
    if (HasSaplingSPKM() && tx.isSapling() && tx.hasSaplingData()) {
        for (const SpendDescription &spend : tx.sapData->vShieldedSpend) {
            uint256 nullifier = spend.nullifier;
            if (m_sspk_man->mapSaplingNullifiersToNotes.count(nullifier) &&
                mapWallet.count(m_sspk_man->mapSaplingNullifiersToNotes[nullifier].hash)) {
                mapWallet[m_sspk_man->mapSaplingNullifiersToNotes[nullifier].hash].MarkDirty();
            }
        }
    }
}

void CWallet::EraseFromWallet(const uint256& hash)
{
    if (!fFileBacked)
        return;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
        LogPrintf("%s: Erased wtx %s from wallet\n", __func__, hash.GetHex());
    }
    return;
}

isminetype CWallet::IsMine(const CTxIn& txin) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return IsMine(prev.vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

bool CWallet::IsUsed(const CTxDestination address) const
{
    LOCK(cs_wallet);
    CScript scriptPubKey = GetScriptForDestination(address);
    if (!::IsMine(*this, scriptPubKey)) {
        return false;
    }

    for (const auto& it : mapWallet) {
        const CWalletTx& wtx = it.second;
        if (wtx.IsCoinBase()) {
            continue;
        }
        for (const CTxOut& txout : wtx.vout) {
            if (txout.scriptPubKey == scriptPubKey)
                return true;
        }
    }
    return false;
}

CAmount CWallet::GetDebit(const CTxIn& txin, const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]) & filter)
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (::IsMine(*this, txout.scriptPubKey)) {
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
            return true;

        LOCK(cs_wallet);
        if (!HasAddressBook(address))
            return true;
    }
    return false;
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

/**
 * Update smart timestamp for a transaction being added to the wallet.
 *
 * Logic:
 * - If the transaction is not yet part of a block, assign its timestamp to the current time.
 * - Else assign its timestamp to the block time
 */
void CWalletTx::UpdateTimeSmart()
{
    nTimeSmart = nTimeReceived;
    if (!hashBlock.IsNull()) {
        if (mapBlockIndex.count(hashBlock)) {
            nTimeSmart = mapBlockIndex.at(hashBlock)->GetBlockTime();
        } else
            LogPrintf("%s : found %s in block %s not in index\n", __func__, GetHash().ToString(), hashBlock.ToString());
    }
}

CAmount CWalletTx::GetCachableAmount(AmountType type, const isminefilter& filter, bool recalculate) const
{
    auto& amount = m_amounts[type];
    if (recalculate || !amount.m_cached[filter]) {
        amount.Set(filter, type == DEBIT ? pwallet->GetDebit(*this, filter) : pwallet->GetCredit(*this, filter));
    }
    return amount.m_value[filter];
}

bool CWalletTx::IsAmountCached(AmountType type, const isminefilter& filter) const
{
    return m_amounts[type].m_cached[filter];
}

//! filter decides which addresses will count towards the debit
CAmount CWalletTx::GetDebit(const isminefilter& filter) const
{
    if (vin.empty())
        return 0;

    CAmount debit = 0;
    if (filter & ISMINE_SPENDABLE) {
        debit += GetCachableAmount(DEBIT, ISMINE_SPENDABLE);
    }
    if (filter & ISMINE_WATCH_ONLY) {
        debit += GetCachableAmount(DEBIT, ISMINE_WATCH_ONLY);
    }
    if (filter & ISMINE_COLD) {
        debit += GetCachableAmount(DEBIT, ISMINE_COLD);
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        debit += GetCachableAmount(DEBIT, ISMINE_SPENDABLE_DELEGATED);
    }
    return debit;
}

CAmount CWalletTx::GetColdStakingDebit(bool fUseCache) const
{
    return GetCachableAmount(DEBIT, ISMINE_COLD, !fUseCache);
}

CAmount CWalletTx::GetStakeDelegationDebit(bool fUseCache) const
{
    return GetCachableAmount(DEBIT, ISMINE_SPENDABLE_DELEGATED, !fUseCache);
}

CAmount CWalletTx::GetCredit(const isminefilter& filter, bool recalculate) const
{
    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE) {
        // GetBalance can assume transactions in mapWallet won't change
        credit += GetCachableAmount(CREDIT, ISMINE_SPENDABLE, recalculate);
    }
    if (filter & ISMINE_WATCH_ONLY) {
        credit += GetCachableAmount(CREDIT, ISMINE_WATCH_ONLY, recalculate);
    }
    if (filter & ISMINE_COLD) {
        credit += GetCachableAmount(CREDIT, ISMINE_COLD, recalculate);
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        credit += GetCachableAmount(CREDIT, ISMINE_SPENDABLE_DELEGATED, recalculate);
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache, const isminefilter& filter) const
{
    LOCK(cs_main);
    if (IsInMainChainImmature()) {
        return GetCachableAmount(IMMATURE_CREDIT, filter, !fUseCache);
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(bool fUseCache, const isminefilter& filter) const
{
    if (!pwallet)
        return 0;

    // Avoid caching ismine for NO or ALL cases (could remove this check and simplify in the future).
    bool allow_cache = filter == ISMINE_SPENDABLE || filter == ISMINE_WATCH_ONLY;

    // Must wait until coinbase/coinstake is safely deep enough in the chain before valuing it
    if (GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && allow_cache && m_amounts[AVAILABLE_CREDIT].m_cached[filter]) {
        return m_amounts[AVAILABLE_CREDIT].m_value[filter];
    }

    CAmount nCredit = 0;
    const uint256& hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++) {
        if (!pwallet->IsSpent(hashTx, i)) {
            const CTxOut &txout = vout[i];
            nCredit += pwallet->GetCredit(txout, filter);
            if (!Params().GetConsensus().MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + " : value out of range");
        }
    }

    if (allow_cache) {
        m_amounts[AVAILABLE_CREDIT].Set(filter, nCredit);
    }

    return nCredit;
}

CAmount CWalletTx::GetColdStakingCredit(bool fUseCache) const
{
    return GetAvailableCredit(fUseCache, ISMINE_COLD);
}

CAmount CWalletTx::GetStakeDelegationCredit(bool fUseCache) const
{
    return GetAvailableCredit(fUseCache, ISMINE_SPENDABLE_DELEGATED);
}

// Return sum of locked coins
CAmount CWalletTx::GetLockedCredit() const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];

        // Skip spent coins
        if (pwallet->IsSpent(hashTx, i)) continue;

        // Add locked coins
        if (pwallet->IsLockedCoin(hashTx, i)) {
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE_ALL);
        }

        // Add masternode collaterals which are handled like locked coins
        else if (fMasterNode && vout[i].nValue == 10000 * COIN) {
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
        }

        if (!Params().GetConsensus().MoneyRange(nCredit))
            throw std::runtime_error("CWalletTx::GetLockedCredit() : value out of range");
    }

    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(const bool& fUseCache) const
{
    LOCK(cs_main);
    if (IsInMainChainImmature()) {
        return GetCachableAmount(IMMATURE_CREDIT, ISMINE_WATCH_ONLY, !fUseCache);
    }

    return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(const bool& fUseCache) const
{
    return GetAvailableCredit(fUseCache, ISMINE_WATCH_ONLY);
}

void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
    std::list<COutputEntry>& listSent,
    CAmount& nFee,
    const isminefilter& filter) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();

    // Compute fee:
    CAmount nDebit = GetDebit(filter);
    bool isFromMyTaddr = nDebit > 0; // debit>0 means we signed/sent this transaction

    if (isFromMyTaddr) {// debit>0 means we signed/sent this transaction
        CAmount nValueOut = GetValueOut(); // transasparent outputs plus the negative Sapling valueBalance
        CAmount nValueIn = GetShieldedValueIn();
        nFee = nDebit - nValueOut + nValueIn;

        // If we sent utxos from this transaction, create output for value taken from (negative valueBalance)
        // or added (positive valueBalance) to the transparent value pool by Sapling shielding and unshielding.
        if (sapData) {
            if (sapData->valueBalance < 0) {
                COutputEntry output = {CNoDestination(), -sapData->valueBalance, (int) vout.size()};
                listSent.push_back(output);
            } else if (sapData->valueBalance > 0) {
                COutputEntry output = {CNoDestination(), sapData->valueBalance, (int) vout.size()};
                listReceived.push_back(output);
            }
        }
    }

    // Sent/received.
    bool hasZerocoinSpends = HasZerocoinSpendInputs();
    for (unsigned int i = 0; i < vout.size(); ++i) {
        const CTxOut& txout = vout[i];
        isminetype fIsMine = pwallet->IsMine(txout);
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (nDebit > 0) {
            // Don't report 'change' txouts
            if (pwallet->IsChange(txout))
                continue;
        } else if (!(fIsMine & filter) && !hasZerocoinSpends)
            continue;

        // In either case, we need to get the destination address
        const bool fColdStake = (filter & ISMINE_COLD);
        CTxDestination address;
        if (txout.IsZerocoinMint()) {
            address = CNoDestination();
        } else if (!ExtractDestination(txout.scriptPubKey, address, fColdStake)) {
            if (!IsCoinStake() && !IsCoinBase()) {
                LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n", this->GetHash().ToString());
            }
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

        // If we are debited by the transaction, add the output as a "sent" entry
        if (nDebit > 0)
            listSent.push_back(output);

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine & filter)
            listReceived.push_back(output);
    }
}

bool CWallet::Upgrade(std::string& error, const int& prevVersion)
{
    LOCK2(cs_wallet, cs_KeyStore);

    // Do not upgrade versions if we are already in the last one
    if (prevVersion >= FEATURE_SAPLING) {
        error = strprintf(_("Cannot upgrade to Sapling wallet (already running Sapling support). Version: %d"), prevVersion);
        return false;
    }

    // Check if we need to upgrade to HD
    if (prevVersion < FEATURE_PRE_SPLIT_KEYPOOL) {
        if (!m_spk_man->Upgrade(prevVersion, error)) {
            return false;
        }
    }

    // Now upgrade to Sapling manager
    if (prevVersion < FEATURE_SAPLING) {
        if (!ActivateSaplingWallet()) {
            return false;
        }
    }

    return true;
}

/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 * @returns -1 if process was cancelled or the number of tx added to the wallet.
 */
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate, bool fromStartup)
{
    int ret = 0;
    int64_t nNow = GetTime();
    bool fCheckZPIV = GetBoolArg("-zapwallettxes", false);
    if (fCheckZPIV)
        zpivTracker->Init();

    const Consensus::Params& consensus = Params().GetConsensus();

    CBlockIndex* pindex = pindexStart;
    {
        LOCK2(cs_main, cs_wallet);

        // no need to read and scan block, if block was created before
        // our wallet birthday (as adjusted for block time variability)
        while (pindex && nTimeFirstKey && (pindex->GetBlockTime() < (nTimeFirstKey - 7200)) &&
                (pindex->nHeight < 1 || !consensus.NetworkUpgradeActive(pindex->nHeight - 1, Consensus::UPGRADE_ZC)))
            pindex = chainActive.Next(pindex);

        ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        double dProgressStart = Checkpoints::GuessVerificationProgress(pindex, false);
        double dProgressTip = Checkpoints::GuessVerificationProgress(chainActive.Tip(), false);
        std::set<uint256> setAddedToWallet;
        while (pindex) {
            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0)
                ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((Checkpoints::GuessVerificationProgress(pindex, false) - dProgressStart) / (dProgressTip - dProgressStart) * 100))));

            if (fromStartup && ShutdownRequested()) {
                return -1;
            }

            CBlock block;
            ReadBlockFromDisk(block, pindex);
            int posInBlock;
            for (posInBlock = 0; posInBlock < (int)block.vtx.size(); posInBlock++) {
                if (AddToWalletIfInvolvingMe(*block.vtx[posInBlock], pindex, posInBlock, fUpdate))
                    ret++;
            }

            // Will try to rescan it if zPIV upgrade is active.
            doZPivRescan(pindex, block, setAddedToWallet, consensus, fCheckZPIV);

            pindex = chainActive.Next(pindex);
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, Checkpoints::GuessVerificationProgress(pindex));
            }
        }
        ShowProgress(_("Rescanning..."), 100); // hide progress dialog in GUI
    }
    return ret;
}

void CWallet::ReacceptWalletTransactions(bool fFirstLoad)
{
    LOCK2(cs_main, cs_wallet);
    std::map<int64_t, CWalletTx*> mapSorted;

    // Sort pending wallet transactions based on their initial wallet insertion order
    for (std::pair<const uint256, CWalletTx>& item: mapWallet) {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain();
        if (!wtx.IsCoinBase() && !wtx.IsCoinStake() && nDepth == 0  && !wtx.isAbandoned()) {
            mapSorted.emplace(wtx.nOrderPos, &wtx);
        }
    }

    // Try to add wallet transactions to memory pool
    for (std::pair<const int64_t, CWalletTx*>& item: mapSorted)
    {
        CWalletTx& wtx = *(item.second);

        LOCK(mempool.cs);
        bool fSuccess = wtx.AcceptToMemoryPool(false);
        if (!fSuccess && fFirstLoad && GetTime() - wtx.GetTxTime() > 12*60*60) {
            //First load of wallet, failed to accept to mempool, and older than 12 hours... not likely to ever
            //make it in to mempool
            AbandonTransaction(wtx.GetHash());
        }
    }
}

bool CWalletTx::InMempool() const
{
    LOCK(mempool.cs);
    if (mempool.exists(GetHash())) {
        return true;
    }
    return false;
}

void CWalletTx::RelayWalletTransaction(CConnman* connman, std::string strCommand)
{
    LOCK(cs_main);
    if (!IsCoinBase() && !IsCoinStake()) {
        if (GetDepthInMainChain() == 0 && !isAbandoned()) {
            uint256 hash = GetHash();
            LogPrintf("Relaying wtx %s\n", hash.ToString());

            int invType = MSG_TX;
            if (strCommand == NetMsgType::IX) {
                mapTxLockReq.emplace(hash, (CTransaction) * this);
                CreateNewLock(((CTransaction) * this));
                invType = MSG_TXLOCK_REQUEST;
            }

            if (connman) {
                CInv inv(invType, hash);
                connman->ForEachNode([&inv](CNode* pnode) {
                  pnode->PushInventory(inv);
                });
            }
        }
    }
}

std::set<uint256> CWalletTx::GetConflicts() const
{
    std::set<uint256> result;
    if (pwallet != NULL) {
        uint256 myHash = GetHash();
        result = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

bool CWallet::Verify()
{
    std::string walletFile = GetArg("-wallet", DEFAULT_WALLET_DAT);
    std::string strDataDir = GetDataDir().string();

    // Wallet file must be a plain filename without a directory
    fs::path wallet_file_path(walletFile);
    if (walletFile != wallet_file_path.filename().string())
        return UIError(strprintf(_("Wallet %s resides outside data directory %s"), walletFile, strDataDir));

    LogPrintf("Using wallet %s\n", walletFile);
    uiInterface.InitMessage(_("Verifying wallet..."));

    if (!bitdb.Open(GetDataDir())) {
        // try moving the database env out of the way
        fs::path pathDatabase = GetDataDir() / "database";
        fs::path pathDatabaseBak = GetDataDir() / strprintf("database.%d.bak", GetTime());
        try {
            fs::rename(pathDatabase, pathDatabaseBak);
            LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string(), pathDatabaseBak.string());
        } catch (const fs::filesystem_error& error) {
            // failure is ok (well, not really, but it's not worse than what we started with)
        }

        // try again
        if (!bitdb.Open(GetDataDir())) {
            // if it still fails, it probably means we can't even create the database env
            return UIError(strprintf(_("Error initializing wallet database environment %s!"), strDataDir));
        }
    }

    if (GetBoolArg("-salvagewallet", false)) {
        // Recover readable keypairs:
        if (!CWalletDB::Recover(bitdb, walletFile, true))
            return false;
    }

    if (fs::exists(GetDataDir() / walletFile)) {
        CDBEnv::VerifyResult r = bitdb.Verify(walletFile, CWalletDB::Recover);
        if (r == CDBEnv::RECOVER_OK) {
            UIWarning(strprintf(_("Warning: wallet file corrupt, data salvaged!"
                                     " Original %s saved as %s in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."),
                    walletFile, "wallet.{timestamp}.bak", strDataDir));
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return UIError(strprintf(_("%s corrupt, salvage failed"), walletFile));
    }

    return true;
}



void CWallet::ResendWalletTransactions(CConnman* connman)
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (GetTime() < nNextResend)
        return;
    bool fFirst = (nNextResend == 0);
    nNextResend = GetTime() + GetRand(30 * 60);
    if (fFirst)
        return;

    // Only do it if there's been a new block since last time
    if (nTimeBestReceived < nLastResend)
        return;
    nLastResend = GetTime();

    // Rebroadcast any of our txes that aren't in a block yet
    LogPrintf("ResendWalletTransactions()\n");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        std::multimap<unsigned int, CWalletTx*> mapSorted;
        for (std::pair<const uint256, CWalletTx> & item : mapWallet) {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60)
                mapSorted.emplace(wtx.nTimeReceived, &wtx);
        }
        for (std::pair<const unsigned int, CWalletTx*> & item : mapSorted) {
            CWalletTx& wtx = *item.second;
            wtx.RelayWalletTransaction(connman);
        }
    }
}

/** @} */ // end of mapWallet


/** @defgroup Actions
 *
 * @{
 */

CAmount CWallet::loopTxsBalance(std::function<void(const uint256&, const CWalletTx&, CAmount&)> method) const
{
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& it : mapWallet) {
            method(it.first, it.second, nTotal);
        }
    }
    return nTotal;
}

CAmount CWallet::GetAvailableBalance(bool fIncludeDelegated) const
{
    isminefilter filter = fIncludeDelegated ? ISMINE_SPENDABLE_ALL : ISMINE_SPENDABLE;
    return GetAvailableBalance(filter, true, 0);
}

CAmount CWallet::GetAvailableBalance(isminefilter& filter, bool useCache, int minDepth) const
{
    return loopTxsBalance([filter, useCache, minDepth](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal){
        bool fConflicted;
        int depth;
        if (pcoin.IsTrusted(depth, fConflicted) && depth >= minDepth) {
            nTotal += pcoin.GetAvailableCredit(useCache, filter);
        }
    });
}

CAmount CWallet::GetColdStakingBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
        if (pcoin.HasP2CSOutputs() && pcoin.IsTrusted())
            nTotal += pcoin.GetColdStakingCredit();
    });
}

CAmount CWallet::GetStakingBalance(const bool fIncludeColdStaking) const
{
    return std::max(CAmount(0), loopTxsBalance(
            [fIncludeColdStaking](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
        if (pcoin.IsTrusted() && pcoin.GetDepthInMainChain() >= Params().GetConsensus().nStakeMinDepth) {
            nTotal += pcoin.GetAvailableCredit();       // available coins
            nTotal -= pcoin.GetStakeDelegationCredit(); // minus delegated coins, if any
            nTotal -= pcoin.GetLockedCredit();          // minus locked coins, if any
            if (fIncludeColdStaking)
                nTotal += pcoin.GetColdStakingCredit(); // plus cold coins, if any and if requested
        }
    }));
}

CAmount CWallet::GetDelegatedBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (pcoin.HasP2CSOutputs() && pcoin.IsTrusted())
                nTotal += pcoin.GetStakeDelegationCredit();
    });
}

CAmount CWallet::GetLockedCoins() const
{
    if (fLiteMode) return 0;

    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (pcoin.IsTrusted() && pcoin.GetDepthInMainChain() > 0)
                nTotal += pcoin.GetLockedCredit();
    });
}

CAmount CWallet::GetUnconfirmedBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (!pcoin.IsTrusted() && pcoin.GetDepthInMainChain() == 0 && pcoin.InMempool())
                nTotal += pcoin.GetAvailableCredit();
    });
}

CAmount CWallet::GetImmatureBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            nTotal += pcoin.GetImmatureCredit(false);
    });
}

CAmount CWallet::GetImmatureColdStakingBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            nTotal += pcoin.GetImmatureCredit(false, ISMINE_COLD);
    });
}

CAmount CWallet::GetImmatureDelegatedBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            nTotal += pcoin.GetImmatureCredit(false, ISMINE_SPENDABLE_DELEGATED);
    });
}

CAmount CWallet::GetWatchOnlyBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (pcoin.IsTrusted())
                nTotal += pcoin.GetAvailableWatchOnlyCredit();
    });
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (!pcoin.IsTrusted() && pcoin.GetDepthInMainChain() == 0 && pcoin.InMempool())
                nTotal += pcoin.GetAvailableWatchOnlyCredit();
    });
}

CAmount CWallet::GetImmatureWatchOnlyBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            nTotal += pcoin.GetImmatureWatchOnlyCredit();
    });
}

// Calculate total balance in a different way from GetBalance. The biggest
// difference is that GetBalance sums up all unspent TxOuts paying to the
// wallet, while this sums up both spent and unspent TxOuts paying to the
// wallet, and then subtracts the values of TxIns spending from the wallet. This
// also has fewer restrictions on which unconfirmed transactions are considered
// trusted.
CAmount CWallet::GetLegacyBalance(const isminefilter& filter, int minDepth) const
{
    LOCK2(cs_main, cs_wallet);

    CAmount balance = 0;
    for (const auto& entry : mapWallet) {
        const CWalletTx& wtx = entry.second;
        bool fConflicted;
        const int depth = wtx.GetDepthAndMempool(fConflicted);
        if (!IsFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || depth < 0 || fConflicted) {
            continue;
        }

        // Loop through tx outputs and add incoming payments. For outgoing txs,
        // treat change outputs specially, as part of the amount debited.
        CAmount debit = wtx.GetDebit(filter);
        const bool outgoing = debit > 0;
        for (const CTxOut& out : wtx.vout) {
            if (outgoing && IsChange(out)) {
                debit -= out.nValue;
            } else if (IsMine(out) & filter && depth >= minDepth) {
                balance += out.nValue;
            }
        }

        // For outgoing txs, subtract amount debited.
        if (outgoing) {
            balance -= debit;
        }
    }

    return balance;
}

void CWallet::GetAvailableP2CSCoins(std::vector<COutput>& vCoins) const {
    vCoins.clear();
    {
        LOCK2(cs_main, cs_wallet);
        for (const auto& it : mapWallet) {
            const uint256& wtxid = it.first;
            const CWalletTx* pcoin = &it.second;

            bool fConflicted;
            int nDepth = pcoin->GetDepthAndMempool(fConflicted);

            if (fConflicted || nDepth < 0)
                continue;

            if (pcoin->HasP2CSOutputs()) {
                for (int i = 0; i < (int) pcoin->vout.size(); i++) {
                    const auto &utxo = pcoin->vout[i];

                    if (IsSpent(wtxid, i))
                        continue;

                    if (utxo.scriptPubKey.IsPayToColdStaking()) {
                        isminetype mine = IsMine(utxo);
                        bool isMineSpendable = mine & ISMINE_SPENDABLE_DELEGATED;
                        if (mine & ISMINE_COLD || isMineSpendable)
                            // Depth and solvability members are not used, no need waste resources and set them for now.
                            vCoins.emplace_back(pcoin, i, 0, isMineSpendable, true);
                    }
                }
            }
        }
    }

}

/**
 * Test if the transaction is spendable.
 */
bool CheckTXAvailability(const CWalletTx* pcoin, bool fOnlyConfirmed, bool fUseIX, int& nDepth, const CBlockIndex*& pindexRet)
{
    if (!CheckFinalTx(*pcoin)) return false;
    if (fOnlyConfirmed && !pcoin->IsTrusted()) return false;
    if (pcoin->GetBlocksToMaturity() > 0) return false;

    nDepth = pcoin->GetDepthInMainChain(pindexRet, false);
    // do not use IX for inputs that have less then 6 blockchain confirmations
    if (fUseIX && nDepth < 6) return false;

    // We should not consider coins which aren't at least in our mempool
    // It's possible for these to be conflicted via ancestors which we may never be able to detect
    if (nDepth == 0 && !pcoin->InMempool()) return false;

    return true;
}

bool CheckTXAvailability(const CWalletTx* pcoin, bool fOnlyConfirmed, bool fUseIX, int& nDepth)
{
    const CBlockIndex* pindexRet = nullptr;
    return CheckTXAvailability(pcoin, fOnlyConfirmed, fUseIX, nDepth, pindexRet);
}

bool CWallet::GetMasternodeVinAndKeys(CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, std::string strTxHash, std::string strOutputIndex, std::string& strError)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    if (strTxHash.empty() || strOutputIndex.empty()) {
        strError = "Invalid masternode collateral hash or output index";
        return error("%s: %s", __func__, strError);
    }

    int nOutputIndex;
    try {
        nOutputIndex = std::stoi(strOutputIndex.c_str());
    } catch (const std::exception& e) {
        strError = "Invalid masternode output index";
        return error("%s: %s on strOutputIndex", __func__, e.what());
    }

    // Find specific vin
    const uint256 txHash = uint256S(strTxHash);
    const CWalletTx* wtx = GetWalletTx(txHash);
    if (!wtx) {
        strError = "collateral tx not found in the wallet";
        return error("%s: %s", __func__, strError);
    }

    // Verify index limits
    if (nOutputIndex < 0 || nOutputIndex >= (int) wtx->vout.size()) {
        strError = "Invalid masternode output index";
        return error("%s: output index %d not found in %s", __func__, nOutputIndex, strTxHash);
    }

    CTxOut txOut = wtx->vout[nOutputIndex];

    // Masternode collateral value
    if (txOut.nValue != 10000 * COIN) {
        strError = "Invalid collateral tx value, must be 10,000 PIV";
        return error("%s: tx %s, index %d not a masternode collateral", __func__, strTxHash, nOutputIndex);
    }

    // Check availability
    int nDepth = 0;
    if (!CheckTXAvailability(wtx, true, false, nDepth)) {
        strError = "Not available collateral transaction";
        return error("%s: tx %s not available", __func__, strTxHash);
    }
    // Skip spent coins
    if (IsSpent(txHash, nOutputIndex)) {
        strError = "Error: collateral already spent";
        return error("%s: tx %s already spent", __func__, strTxHash);
    }

    // Depth must be at least MASTERNODE_MIN_CONFIRMATIONS
    if (nDepth < MasternodeCollateralMinConf()) {
        strError = strprintf("Collateral tx must have at least %d confirmations, has %d", MasternodeCollateralMinConf(), nDepth);
        return error("%s: %s", __func__, strError);
    }

    // utxo need to be mine.
    isminetype mine = IsMine(txOut);
    if (mine != ISMINE_SPENDABLE) {
        strError = "Invalid collateral transaction. Not from this wallet";
        return error("%s: tx %s not mine", __func__, strTxHash);
    }

    return GetVinAndKeysFromOutput(
            COutput(wtx, nOutputIndex, nDepth, true, true),
            txinRet,
            pubKeyRet,
            keyRet);
}

CWallet::OutputAvailabilityResult CWallet::CheckOutputAvailability(
        const CTxOut& output,
        const unsigned int outIndex,
        const uint256& wtxid,
        AvailableCoinsType nCoinType,
        const CCoinControl* coinControl,
        const bool fCoinsSelected,
        const bool fIncludeColdStaking,
        const bool fIncludeDelegated) const
{
    OutputAvailabilityResult res;

    // Check for only 10k utxo
    if (nCoinType == ONLY_10000 && output.nValue != 10000 * COIN) return res;

    // Check for stakeable utxo
    if (nCoinType == STAKEABLE_COINS && output.IsZerocoinMint()) return res;

    // Check if the utxo was spent.
    if (IsSpent(wtxid, outIndex)) return res;

    isminetype mine = IsMine(output);

    // Check If not mine
    if (mine == ISMINE_NO) return res;

    // Check if watch only utxo are allowed
    if (mine == ISMINE_WATCH_ONLY && coinControl && !coinControl->fAllowWatchOnly) return res;

    // Skip locked utxo
    if (IsLockedCoin(wtxid, outIndex) && nCoinType != ONLY_10000) return res;

    // Check if we should include zero value utxo
    if (output.nValue <= 0) return res;

    if (fCoinsSelected && !coinControl->fAllowOtherInputs && !coinControl->IsSelected(COutPoint(wtxid, outIndex)))
        return res;

    // --Skip P2CS outputs
    // skip cold coins
    if (mine == ISMINE_COLD && (!fIncludeColdStaking || !HasDelegator(output))) return res;
    // skip delegated coins
    if (mine == ISMINE_SPENDABLE_DELEGATED && !fIncludeDelegated) return res;

    res.solvable = IsSolvable(*this, output.scriptPubKey, mine == ISMINE_COLD);

    res.spendable = ((mine & ISMINE_SPENDABLE) != ISMINE_NO) ||
                     (((mine & ISMINE_WATCH_ONLY) != ISMINE_NO) && (coinControl && coinControl->fAllowWatchOnly && res.solvable)) ||
                     ((mine & ((fIncludeColdStaking ? ISMINE_COLD : ISMINE_NO) |
                               (fIncludeDelegated ? ISMINE_SPENDABLE_DELEGATED : ISMINE_NO) )) != ISMINE_NO);
    res.available = true;
    return res;
}

/**
 * populate vCoins with vector of available COutputs.
 */
bool CWallet::AvailableCoins(std::vector<COutput>* pCoins,      // --> populates when != nullptr
                             const CCoinControl* coinControl,   // Default: nullptr
                             bool fIncludeDelegated,            // Default: true
                             bool fIncludeColdStaking,          // Default: false
                             AvailableCoinsType nCoinType,      // Default: ALL_COINS
                             bool fOnlyConfirmed,               // Default: true
                             bool fUseIX                       // Default: false
                             ) const
{
    if (pCoins) pCoins->clear();
    const bool fCoinsSelected = (coinControl != nullptr) && coinControl->HasSelected();
    // include delegated coins when coinControl is active
    if (!fIncludeDelegated && fCoinsSelected)
        fIncludeDelegated = true;

    {
        LOCK2(cs_main, cs_wallet);
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it) {
            const uint256& wtxid = it->first;
            const CWalletTx* pcoin = &(*it).second;

            // Check if the tx is selectable
            int nDepth;
            if (!CheckTXAvailability(pcoin, fOnlyConfirmed, fUseIX, nDepth))
                continue;

            // Check min depth requirement for stake inputs
            if (nCoinType == STAKEABLE_COINS && nDepth < Params().GetConsensus().nStakeMinDepth) continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                const auto& output = pcoin->vout[i];

                auto res = CheckOutputAvailability(
                        output,
                        i,
                        wtxid,
                        nCoinType,
                        coinControl,
                        fCoinsSelected,
                        fIncludeColdStaking,
                        fIncludeDelegated);

                if (!res.available) continue;

                // found valid coin
                if (!pCoins) return true;
                pCoins->emplace_back(pcoin, (int) i, nDepth, res.spendable, res.solvable);
            }
        }
        return (pCoins && !pCoins->empty());
    }
}

std::map<CTxDestination , std::vector<COutput> > CWallet::AvailableCoinsByAddress(bool fConfirmed, CAmount maxCoinValue)
{
    std::vector<COutput> vCoins;
    // include cold
    AvailableCoins(&vCoins,
            nullptr,            // coin control
            true,               // fIncludeDelegated
            true,               // fIncludeColdStaking
            ALL_COINS,          // coin type
            fConfirmed);        // only confirmed

    std::map<CTxDestination, std::vector<COutput> > mapCoins;
    for (COutput& out : vCoins) {
        if (maxCoinValue > 0 && out.tx->vout[out.i].nValue > maxCoinValue)
            continue;

        CTxDestination address;
        bool fColdStakeAddr = false;
        if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address, fColdStakeAddr)) {
            // if this is a P2CS we don't have the owner key - check if we have the staking key
            fColdStakeAddr = true;
            if ( !out.tx->vout[out.i].scriptPubKey.IsPayToColdStaking() ||
                    !ExtractDestination(out.tx->vout[out.i].scriptPubKey, address, fColdStakeAddr) )
                continue;
        }

        mapCoins[address].push_back(out);
    }

    return mapCoins;
}

static void ApproximateBestSubset(std::vector<std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> > > vValue, const CAmount& nTotalLower, const CAmount& nTargetValue, std::vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
{
    std::vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    FastRandomContext insecure_rand;

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++) {
        vfIncluded.assign(vValue.size(), false);
        CAmount nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++) {
            for (unsigned int i = 0; i < vValue.size(); i++) {
                //The solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng is fast. We do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (nPass == 0 ? insecure_rand.randbool() : !vfIncluded[i]) {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue) {
                        fReachedTarget = true;
                        if (nTotal < nBest) {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

bool CWallet::StakeableCoins(std::vector<CStakeableOutput>* pCoins)
{
    const bool fIncludeColdStaking = (sporkManager.IsSporkActive(SPORK_17_COLDSTAKING_ENFORCEMENT) &&
                               GetBoolArg("-coldstaking", DEFAULT_COLDSTAKING));

    LOCK2(cs_main, cs_wallet);
    for (const auto& it : mapWallet) {
        const uint256& wtxid = it.first;
        const CWalletTx* pcoin = &(it).second;

        // Check if the tx is selectable
        int nDepth;
        const CBlockIndex* pindex = nullptr;
        if (!CheckTXAvailability(pcoin, true, false, nDepth, pindex))
            continue;

        // Check min depth requirement for stake inputs
        if (nDepth < Params().GetConsensus().nStakeMinDepth) continue;

        for (unsigned int index = 0; index < pcoin->vout.size(); index++) {

            auto res = CheckOutputAvailability(
                    pcoin->vout[index],
                    index,
                    wtxid,
                    STAKEABLE_COINS,
                    nullptr, // coin control
                    false,   // fIncludeDelegated
                    fIncludeColdStaking,
                    false);

            if (!res.available) continue;

            // found valid coin
            if (!pCoins) return true;
            pCoins->emplace_back(CStakeableOutput(pcoin, (int) index, nDepth, res.spendable, res.solvable, pindex));
        }
    }
    return (pCoins && !pCoins->empty());
}

bool CWallet::SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<CAmount>::max();
    coinLowestLarger.second.first = NULL;
    std::vector<std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> > > vValue;
    CAmount nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    for (const COutput& output : vCoins) {
        if (!output.fSpendable)
            continue;

        const CWalletTx* pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;
        CAmount n = pcoin->vout[i].nValue;

        std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> > coin = std::make_pair(n, std::make_pair(pcoin, i));

        if (n == nTargetValue) {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        } else if (n < nTargetValue + CENT) {
            vValue.push_back(coin);
            nTotalLower += n;
        } else if (n < coinLowestLarger.first) {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue) {
        for (unsigned int i = 0; i < vValue.size(); ++i) {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue) {
        if (coinLowestLarger.second.first == NULL)
                return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    std::sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    std::vector<char> vfBest;
    CAmount nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + CENT)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + CENT, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + CENT) || coinLowestLarger.first <= nBest)) {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    } else {
        std::string s = "CWallet::SelectCoinsMinConf best subset: ";
        for (unsigned int i = 0; i < vValue.size(); i++) {
            if (vfBest[i]) {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
                s += FormatMoney(vValue[i].first) + " ";
            }
        }
        LogPrintf("%s - total %s\n", s, FormatMoney(nBest));
    }

    return true;
}

bool CWallet::SelectCoinsToSpend(const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl) const
{
    // Note: this function should never be used for "always free" tx types like dstx
    std::vector<COutput> vCoins(vAvailableCoins);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs) {
        for (const COutput& out : vCoins) {
            if (!out.fSpendable)
                continue;

            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.emplace(out.tx, out.i);
        }
        return (nValueRet >= nTargetValue);
    }

    // calculate value from preset inputs and store them
    std::set<std::pair<const CWalletTx*, uint32_t> > setPresetCoins;
    CAmount nValueFromPresetInputs = 0;

    std::vector<COutPoint> vPresetInputs;
    if (coinControl)
        coinControl->ListSelected(vPresetInputs);
    for (const COutPoint& outpoint : vPresetInputs) {
        std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(outpoint.hash);
        if (it != mapWallet.end()) {
            const CWalletTx* pcoin = &it->second;
            // Clearly invalid input, fail
            if (pcoin->vout.size() <= outpoint.n)
                return false;
            nValueFromPresetInputs += pcoin->vout[outpoint.n].nValue;
            setPresetCoins.emplace(pcoin, outpoint.n);
        } else
            return false; // TODO: Allow non-wallet inputs
    }

    // remove preset inputs from vCoins
    for (std::vector<COutput>::iterator it = vCoins.begin(); it != vCoins.end() && coinControl && coinControl->HasSelected();) {
        if (setPresetCoins.count(std::make_pair(it->tx, it->i)))
            it = vCoins.erase(it);
        else
            ++it;
    }

    bool res = nTargetValue <= nValueFromPresetInputs ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 6, vCoins, setCoinsRet, nValueRet) ||
        SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 1, vCoins, setCoinsRet, nValueRet) ||
        (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, vCoins, setCoinsRet, nValueRet));

    // because SelectCoinsMinConf clears the setCoinsRet, we now add the possible inputs to the coinset
    setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

    // add preset inputs to the total value selected
    nValueRet += nValueFromPresetInputs;

    return res;
}

bool CWallet::CreateBudgetFeeTX(CWalletTx& tx, const uint256& hash, CReserveKey& keyChange, bool fFinalization)
{
    CScript scriptChange;
    scriptChange << OP_RETURN << ToByteVector(hash);

    CAmount nFeeRet = 0;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;
    vecSend.emplace_back(scriptChange, (fFinalization ? BUDGET_FEE_TX : BUDGET_FEE_TX_OLD), false);

    CCoinControl* coinControl = nullptr;
    int nChangePosInOut = -1;
    bool success = CreateTransaction(vecSend, tx, keyChange, nFeeRet, nChangePosInOut, strFail, coinControl, ALL_COINS, true, false /*useIX*/, (CAmount)0);
    if (!success) {
        LogPrintf("%s: Error - %s\n", __func__, strFail);
        return false;
    }

    return true;
}

bool CWallet::FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, bool overrideEstimatedFeeRate, const CFeeRate& specificFeeRate, int& nChangePosInOut, std::string& strFailReason, bool includeWatching, bool lockUnspents, const CTxDestination& destChange)
{
    std::vector<CRecipient> vecSend;

    // Turn the txout set into a CRecipient vector
    for (const CTxOut& txOut : tx.vout) {
        CRecipient recipient = {txOut.scriptPubKey, txOut.nValue, false};
        vecSend.push_back(recipient);
    }

    CCoinControl coinControl;
    coinControl.destChange = destChange;
    coinControl.fAllowOtherInputs = true;
    coinControl.fAllowWatchOnly = includeWatching;
    coinControl.fOverrideFeeRate = overrideEstimatedFeeRate;
    coinControl.nFeeRate = specificFeeRate;

    for (const CTxIn& txin : tx.vin)
        coinControl.Select(txin.prevout);

    CReserveKey reservekey(this);
    CWalletTx wtx;
    if (!CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePosInOut, strFailReason, &coinControl, ALL_COINS, false))
        return false;

    if (nChangePosInOut != -1)
        tx.vout.insert(tx.vout.begin() + nChangePosInOut, wtx.vout[nChangePosInOut]);

    // Add new txins (keeping original txin scriptSig/order)
    for (const CTxIn& txin : wtx.vin) {
        if (!coinControl.IsSelected(txin.prevout)) {
            tx.vin.push_back(txin);

            if (lockUnspents) {
              LOCK2(cs_main, cs_wallet);
              LockCoin(txin.prevout);
            }
        }
    }

    return true;
}

bool CWallet::CreateTransaction(const std::vector<CRecipient>& vecSend,
    CWalletTx& wtxNew,
    CReserveKey& reservekey,
    CAmount& nFeeRet,
    int& nChangePosInOut,
    std::string& strFailReason,
    const CCoinControl* coinControl,
    AvailableCoinsType coin_type,
    bool sign,
    bool useIX,
    CAmount nFeePay,
    bool fIncludeDelegated)
{
    if (useIX && nFeePay < CENT) nFeePay = CENT;

    CAmount nValue = 0;
    int nChangePosRequest = nChangePosInOut;

    for (const CRecipient& rec : vecSend) {
        if (rec.nAmount < 0) {
            strFailReason = _("Transaction amounts must be positive");
            return false;
        }
        nValue += rec.nAmount;
    }
    if (vecSend.empty() || nValue < 0) {
        strFailReason = _("Transaction amounts must be positive");
        return false;
    }

    wtxNew.fTimeReceivedIsTxTime = true;
    wtxNew.BindWallet(this);
    CMutableTransaction txNew;
    CScript scriptChange;

    {
        LOCK2(cs_main, cs_wallet);
        {
            std::vector<COutput> vAvailableCoins;
            AvailableCoins(&vAvailableCoins,
                coinControl,
                fIncludeDelegated,
                false,                  // fIncludeColdStaking
                coin_type,
                true,                   // fOnlyConfirmed
                useIX);

            nFeeRet = 0;
            if (nFeePay > 0) nFeeRet = nFeePay;
            while (true) {
                nChangePosInOut = nChangePosRequest;
                txNew.vin.clear();
                txNew.vout.clear();
                wtxNew.fFromMe = true;

                CAmount nTotalValue = nValue + nFeeRet;
                double dPriority = 0;

                // vouts to the payees
                if (coinControl && !coinControl->fSplitBlock) {
                    for (const CRecipient& rec : vecSend) {
                        CTxOut txout(rec.nAmount, rec.scriptPubKey);
                        if (txout.IsDust(::minRelayTxFee)) {
                            strFailReason = _("Transaction amount too small");
                            return false;
                        }
                        txNew.vout.push_back(txout);
                    }
                } else //UTXO Splitter Transaction
                {
                    int nSplitBlock;

                    if (coinControl)
                        nSplitBlock = coinControl->nSplitBlock;
                    else
                        nSplitBlock = 1;

                    for (const CRecipient& rec : vecSend) {
                        for (int i = 0; i < nSplitBlock; i++) {
                            if (i == nSplitBlock - 1) {
                                uint64_t nRemainder = rec.nAmount % nSplitBlock;
                                txNew.vout.emplace_back((rec.nAmount / nSplitBlock) + nRemainder, rec.scriptPubKey);
                            } else
                                txNew.vout.emplace_back(rec.nAmount / nSplitBlock, rec.scriptPubKey);
                        }
                    }
                }

                // Choose coins to use
                std::set<std::pair<const CWalletTx*, unsigned int> > setCoins;
                CAmount nValueIn = 0;

                if (!SelectCoinsToSpend(vAvailableCoins, nTotalValue, setCoins, nValueIn, coinControl)) {
                    if (coin_type == ALL_COINS) {
                        strFailReason = _("Insufficient funds.");
                    }

                    if (useIX) {
                        strFailReason += " " + _("SwiftX requires inputs with at least 6 confirmations, you might need to wait a few minutes and try again.");
                    }

                    return false;
                }


                for (std::pair<const CWalletTx*, unsigned int> pcoin : setCoins) {
                    if(pcoin.first->vout[pcoin.second].scriptPubKey.IsPayToColdStaking())
                        wtxNew.fStakeDelegationVoided = true;
                    CAmount nCredit = pcoin.first->vout[pcoin.second].nValue;
                    //The coin age after the next block (depth+1) is used instead of the current,
                    //reflecting an assumption the user would accept a bit more delay for
                    //a chance at a free transaction.
                    //But mempool inputs might still be in the mempool, so their age stays 0
                    int age = pcoin.first->GetDepthInMainChain();
                    assert(age >= 0);
                    if (age != 0)
                        age += 1;
                    dPriority += (double)nCredit * age;
                }

                CAmount nChange = nValueIn - nValue - nFeeRet;

                if (nChange > 0) {
                    // Fill a vout to ourself
                    // TODO: pass in scriptChange instead of reservekey so
                    // change transaction isn't always pay-to-pivx-address
                    bool combineChange = false;

                    // coin control: send change to custom address
                    if (coinControl && IsValidDestination(coinControl->destChange)) {
                        scriptChange = GetScriptForDestination(coinControl->destChange);

                        std::vector<CTxOut>::iterator it = txNew.vout.begin();
                        while (it != txNew.vout.end()) {
                            if (scriptChange == it->scriptPubKey) {
                                it->nValue += nChange;
                                nChange = 0;
                                reservekey.ReturnKey();
                                combineChange = true;
                                break;
                            }
                            ++it;
                        }
                    }

                    // no coin control: send change to newly generated address
                    else {
                        // Note: We use a new key here to keep it from being obvious which side is the change.
                        //  The drawback is that by not reusing a previous key, the change may be lost if a
                        //  backup is restored, if the backup doesn't have the new private key for the change.
                        //  If we reused the old key, it would be possible to add code to look for and
                        //  rediscover unknown transactions that were written with keys of ours to recover
                        //  post-backup change.

                        // Reserve a new key pair from key pool. If it fails, provide a dummy
                        CPubKey vchPubKey;
                        if (!reservekey.GetReservedKey(vchPubKey, true)) {
                            strFailReason = _("Can't generate a change-address key. Please call keypoolrefill first.");
                            scriptChange = CScript();
                        } else {
                            scriptChange = GetScriptForDestination(vchPubKey.GetID());
                        }
                    }

                    if (!combineChange) {
                        CTxOut newTxOut(nChange, scriptChange);

                        // Never create dust outputs; if we would, just
                        // add the dust to the fee.
                        if (newTxOut.IsDust(::minRelayTxFee)) {
                            nFeeRet += nChange;
                            nChange = 0;
                            reservekey.ReturnKey();
                            nChangePosInOut = -1;
                        } else {
                            if (nChangePosInOut == -1) {
                                // Insert change txn at random position:
                                nChangePosInOut = GetRandInt(txNew.vout.size()+1);
                            } else if (nChangePosInOut < 0 || (unsigned int) nChangePosInOut > txNew.vout.size()) {
                                strFailReason = _("Change index out of range");
                                return false;
                            }
                            std::vector<CTxOut>::iterator position = txNew.vout.begin() + nChangePosInOut;
                            txNew.vout.insert(position, newTxOut);
                        }
                    }
                } else
                    reservekey.ReturnKey();

                // Fill vin
                for (const std::pair<const CWalletTx*, unsigned int> & coin : setCoins)
                    txNew.vin.emplace_back(coin.first->GetHash(), coin.second);

                // Sign
                int nIn = 0;
                CTransaction txNewConst(txNew);
                for (const std::pair<const CWalletTx*, unsigned int> & coin : setCoins) {
                    bool signSuccess;
                    const CScript& scriptPubKey = coin.first->vout[coin.second].scriptPubKey;
                    SignatureData sigdata;
                    bool haveKey = coin.first->GetStakeDelegationCredit() > 0;
                    if (sign) {
                        signSuccess = ProduceSignature(
                                TransactionSignatureCreator(this, &txNewConst, nIn, coin.first->vout[coin.second].nValue, SIGHASH_ALL),
                                scriptPubKey,
                                sigdata,
                                !haveKey // fColdStake = false
                        );
                    } else {
                        signSuccess = ProduceSignature(
                                DummySignatureCreator(this), scriptPubKey, sigdata, false);
                    }

                    if (!signSuccess) {
                        strFailReason = _("Signing transaction failed");
                        return false;
                    } else {
                        UpdateTransaction(txNew, nIn, sigdata);
                    }
                    nIn++;
                }

                unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);

                // Remove scriptSigs if we used dummy signatures for fee calculation
                if (!sign) {
                    for (CTxIn& vin : txNew.vin)
                        vin.scriptSig = CScript();
                }

                // Embed the constructed transaction data in wtxNew.
                *static_cast<CTransaction*>(&wtxNew) = CTransaction(txNew);

                // Limit size
                if (nBytes >= MAX_STANDARD_TX_SIZE) {
                    strFailReason = _("Transaction too large");
                    return false;
                }

                dPriority = wtxNew.ComputePriority(dPriority, nBytes);

                // Can we complete this as a free transaction?
                if (fSendFreeTransactions && nBytes <= MAX_FREE_TRANSACTION_CREATE_SIZE) {
                    // Not enough fee: enough priority?
                    double dPriorityNeeded = mempool.estimateSmartPriority(nTxConfirmTarget);
                    // Require at least hard-coded AllowFree.
                    if (dPriority >= dPriorityNeeded && AllowFree(dPriority))
                        break;
                }

                CAmount nFeeNeeded = std::max(nFeePay, GetMinimumFee(nBytes, nTxConfirmTarget, mempool));

                if (coinControl && nFeeNeeded > 0 && coinControl->nMinimumTotalFee > nFeeNeeded) {
                    nFeeNeeded = coinControl->nMinimumTotalFee;
                }
                if (coinControl && coinControl->fOverrideFeeRate)
                    nFeeNeeded = coinControl->nFeeRate.GetFee(nBytes);

                // If we made it here and we aren't even able to meet the relay fee on the next pass, give up
                // because we must be at the maximum allowed fee.
                if (nFeeNeeded < ::minRelayTxFee.GetFee(nBytes)) {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }

                if (nFeeRet >= nFeeNeeded) // Done, enough fee included
                    break;

                // Include more fee and try again.
                nFeeRet = nFeeNeeded;
                continue;
            }

            // Give up if change keypool ran out and we failed to find a solution without change:
            if (scriptChange.empty() && nChangePosInOut != -1) {
                return false;
            }
        }
    }
    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey, const CAmount& nValue, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl, AvailableCoinsType coin_type, bool useIX, CAmount nFeePay, bool fIncludeDelegated)
{
    std::vector<CRecipient> vecSend;
    vecSend.emplace_back(scriptPubKey, nValue, false);
    int nChangePosInOut = -1;
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePosInOut, strFailReason, coinControl, coin_type, true, useIX, nFeePay, fIncludeDelegated);
}

bool CWallet::CreateCoinStake(
        const CKeyStore& keystore,
        const CBlockIndex* pindexPrev,
        unsigned int nBits,
        CMutableTransaction& txNew,
        int64_t& nTxNewTime,
        std::vector<CStakeableOutput>* availableCoins)
{

    const Consensus::Params& consensus = Params().GetConsensus();

    // Mark coin stake transaction
    txNew.vin.clear();
    txNew.vout.clear();
    txNew.vout.emplace_back(0, CScript());

    // update staker status (hash)
    pStakerStatus->SetLastTip(pindexPrev);
    pStakerStatus->SetLastCoins((int) availableCoins->size());

    // P2PKH block signatures were not accepted before v5 update.
    bool onlyP2PK = !consensus.NetworkUpgradeActive(pindexPrev->nHeight + 1, Consensus::UPGRADE_V5_DUMMY);

    // Kernel Search
    CAmount nCredit;
    CScript scriptPubKeyKernel;
    bool fKernelFound = false;
    int nAttempts = 0;
    for (const auto& out : *availableCoins) {
        CPivStake stakeInput(out.tx->vout[out.i],
                             COutPoint(out.tx->GetHash(), out.i),
                             out.pindex);

        //new block came in, move on
        if (WITH_LOCK(cs_main, return chainActive.Height()) != pindexPrev->nHeight) return false;

        // Make sure the wallet is unlocked and shutdown hasn't been requested
        if (IsLocked() || ShutdownRequested()) return false;

        // This should never happen
        if (stakeInput.IsZPIV()) {
            LogPrintf("%s: ERROR - zPOS is disabled\n", __func__);
            continue;
        }

        nCredit = 0;

        nAttempts++;
        fKernelFound = Stake(pindexPrev, &stakeInput, nBits, nTxNewTime);

        // update staker status (time, attempts)
        pStakerStatus->SetLastTime(nTxNewTime);
        pStakerStatus->SetLastTries(nAttempts);

        if (!fKernelFound) continue;

        // Found a kernel
        LogPrintf("CreateCoinStake : kernel found\n");
        nCredit += stakeInput.GetValue();

        // Add block reward to the credit
        nCredit += GetBlockValue(pindexPrev->nHeight + 1);

        // Create the output transaction(s)
        std::vector<CTxOut> vout;
        if (!stakeInput.CreateTxOuts(this, vout, nCredit, onlyP2PK)) {
            LogPrintf("%s : failed to create output\n", __func__);
            continue;
        }
        txNew.vout.insert(txNew.vout.end(), vout.begin(), vout.end());

        // Set output amount
        int outputs = (int) txNew.vout.size() - 1;
        CAmount nRemaining = nCredit;
        if (outputs > 1) {
            // Split the stake across the outputs
            CAmount nShare = nRemaining / outputs;
            for (int i = 1; i < outputs; i++) {
                // loop through all but the last one.
                txNew.vout[i].nValue = nShare;
                nRemaining -= nShare;
            }
        }
        // put the remaining on the last output (which all into the first if only one output)
        txNew.vout[outputs].nValue += nRemaining;

        // Limit size
        unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
        if (nBytes >= DEFAULT_BLOCK_MAX_SIZE / 5)
            return error("%s : exceeded coinstake size limit", __func__);

        // Masternode payment
        FillBlockPayee(txNew, pindexPrev, true);

        const uint256& hashTxOut = txNew.GetHash();
        CTxIn in;
        if (!stakeInput.CreateTxIn(this, in, hashTxOut)) {
            LogPrintf("%s : failed to create TxIn\n", __func__);
            txNew.vin.clear();
            txNew.vout.clear();
            continue;
        }
        txNew.vin.emplace_back(in);

        break;
    }
    LogPrint(BCLog::STAKING, "%s: attempted staking %d times\n", __func__, nAttempts);

    if (!fKernelFound)
        return false;

    // Sign it
    int nIn = 0;
    for (const CTxIn& txIn : txNew.vin) {
        const CWalletTx *wtx = GetWalletTx(txIn.prevout.hash);
        if (!SignSignature(*this, *wtx, txNew, nIn++, SIGHASH_ALL, true))
            return error("%s : failed to sign coinstake", __func__);
    }

    // Successfully generated coinstake
    return true;
}

std::string CWallet::CommitResult::ToString() const
{
    std::string strErrRet = strprintf(_("Failed to accept tx in the memory pool (reason: %s)\n"), FormatStateMessage(state));

    switch (status) {
        case CWallet::CommitStatus::OK:
            return _("No error");
        case CWallet::CommitStatus::Abandoned:
            strErrRet += _("Transaction canceled.");
            break;
        case CWallet::CommitStatus::NotAccepted:
            strErrRet += strprintf(_("WARNING: The transaction has been signed and recorded, so the wallet will try to re-send it. "
                    "Use 'abandontransaction' to cancel it. (txid: %s)"), hashTx.ToString());
            break;
        default:
            return _("Invalid status error.");
    }

    return strErrRet;
}

/**
 * Call after CreateTransaction unless you want to abort
 */
CWallet::CommitResult CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, CConnman* connman, std::string strCommand)
{
    CommitResult res;
    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("%s:\n%s", __func__, wtxNew.ToString());
        {
            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Notify that old coins are spent
            if (!wtxNew.HasZerocoinSpendInputs()) {
                std::set<uint256> updated_hashes;
                for (const CTxIn& txin : wtxNew.vin) {
                    // notify only once
                    if (updated_hashes.find(txin.prevout.hash) != updated_hashes.end()) continue;

                    CWalletTx& coin = mapWallet[txin.prevout.hash];
                    coin.BindWallet(this);
                    NotifyTransactionChanged(this, txin.prevout.hash, CT_UPDATED);
                    updated_hashes.insert(txin.prevout.hash);
                }
            }
        }

        res.hashTx = wtxNew.GetHash();

        // Try ATMP. This must not fail. The transaction has already been signed and recorded.
        CValidationState state;
        if (!AcceptToMemoryPool(mempool, state, MakeTransactionRef(std::move(wtxNew)), false, nullptr, false, true, false)) {
            res.state = state;
            // Abandon the transaction
            if (AbandonTransaction(res.hashTx)) {
                res.status = CWallet::CommitStatus::Abandoned;
                // Return the change key
                reservekey.ReturnKey();
            }

            LogPrintf("%s: ERROR: %s\n", __func__, res.ToString());
            return res;
        }

        res.status = CWallet::CommitStatus::OK;

        // Broadcast
        wtxNew.RelayWalletTransaction(connman, strCommand);
    }
    return res;
}

CAmount CWallet::GetRequiredFee(unsigned int nTxBytes)
{
    return std::max(minTxFee.GetFee(nTxBytes), ::minRelayTxFee.GetFee(nTxBytes));
}

CAmount CWallet::GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool)
{
    // payTxFee is user-set "I want to pay this much"
    CAmount nFeeNeeded = payTxFee.GetFee(nTxBytes);
    // User didn't set: use -txconfirmtarget to estimate...
    if (nFeeNeeded == 0) {
        int estimateFoundTarget = (int) nConfirmTarget;
        nFeeNeeded = pool.estimateSmartFee((int) nConfirmTarget, &estimateFoundTarget).GetFee(nTxBytes);
        // ... unless we don't have enough mempool data for our desired target
        // so we make sure we're paying at least minTxFee
        if (nFeeNeeded == 0 || (unsigned int) estimateFoundTarget > nConfirmTarget)
            nFeeNeeded = std::max(nFeeNeeded, GetRequiredFee(nTxBytes));
    }
    // prevent user from paying a non-sense fee (like 1 satoshi): 0 < fee < minRelayFee
    if (nFeeNeeded < ::minRelayTxFee.GetFee(nTxBytes))
        nFeeNeeded = ::minRelayTxFee.GetFee(nTxBytes);
    // But always obey the maximum
    if (nFeeNeeded > maxTxFee)
        nFeeNeeded = maxTxFee;
    return nFeeNeeded;
}

DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    LOCK2(cs_main, cs_wallet);

    if (!fFileBacked)
        return DB_LOAD_OK;

    DBErrors nLoadWalletRet = CWalletDB(strWalletFile, "cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            // TODO: Implement spk_man->RewriteDB().
            m_spk_man->set_pre_split_keypool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    // This wallet is in its first run if all of these are empty
    fFirstRunRet = mapKeys.empty() && mapCryptedKeys.empty() && mapMasterKeys.empty() && setWatchOnly.empty() && mapScripts.empty();

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;

    uiInterface.LoadWallet(this);

    return DB_LOAD_OK;
}


DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    DBErrors nZapWalletTxRet = CWalletDB(strWalletFile, "cr+").ZapWalletTx(this, vWtx);
    if (nZapWalletTxRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            LOCK(cs_wallet);
            m_spk_man->set_pre_split_keypool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapWalletTxRet != DB_LOAD_OK)
        return nZapWalletTxRet;

    return DB_LOAD_OK;
}

std::string CWallet::ParseIntoAddress(const CTxDestination& dest, const std::string& purpose) {
    const CChainParams::Base58Type addrType =
            AddressBook::IsColdStakingPurpose(purpose) ?
            CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS;
    return EncodeDestination(dest, addrType);
}

bool CWallet::SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& strPurpose)
{
    bool fUpdated = HasAddressBook(address);
    {
        LOCK(cs_wallet); // mapAddressBook
        mapAddressBook[address].name = strName;
        if (!strPurpose.empty()) /* update purpose only if requested */
            mapAddressBook[address].purpose = strPurpose;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address) != ISMINE_NO,
            mapAddressBook.at(address).purpose, (fUpdated ? CT_UPDATED : CT_NEW));
    if (!fFileBacked)
        return false;
    std::string addressStr = ParseIntoAddress(address, mapAddressBook.at(address).purpose);
    if (!strPurpose.empty() && !CWalletDB(strWalletFile).WritePurpose(addressStr, strPurpose))
        return false;
    return CWalletDB(strWalletFile).WriteName(addressStr, strName);
}

bool CWallet::DelAddressBook(const CTxDestination& address, const CChainParams::Base58Type addrType)
{
    std::string strAddress =  EncodeDestination(address, addrType);
    std::string purpose = GetPurposeForAddressBookEntry(address);
    {
        LOCK(cs_wallet); // mapAddressBook

        if (fFileBacked) {
            // Delete destdata tuples associated with address
            for (const std::pair<std::string, std::string> &item : mapAddressBook[address].destdata) {
                CWalletDB(strWalletFile).EraseDestData(strAddress, item.first);
            }
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address) != ISMINE_NO, purpose, CT_DELETED);

    if (!fFileBacked)
        return false;
    CWalletDB(strWalletFile).ErasePurpose(strAddress);
    return CWalletDB(strWalletFile).EraseName(strAddress);
}

std::string CWallet::GetPurposeForAddressBookEntry(const CTxDestination& address) const
{
    LOCK(cs_wallet);
    auto it = mapAddressBook.find(address);
    return it != mapAddressBook.end() ? it->second.purpose : "";
}

std::string CWallet::GetNameForAddressBookEntry(const CTxDestination& address) const
{
    LOCK(cs_wallet);
    auto it = mapAddressBook.find(address);
    return it != mapAddressBook.end() ? it->second.name : "";
}

Optional<AddressBook::CAddressBookData> CWallet::GetAddressBookEntry(const CTxDestination& dest) const
{
    LOCK(cs_wallet);
    auto it = mapAddressBook.find(dest);
    return it != mapAddressBook.end() ? Optional<AddressBook::CAddressBookData>(it->second) : nullopt;
}

void CWallet::LoadAddressBookName(const CTxDestination& dest, const std::string& strName)
{
    mapAddressBook[dest].name = strName;
}

void CWallet::LoadAddressBookPurpose(const CTxDestination& dest, const std::string& strPurpose)
{
    mapAddressBook[dest].purpose = strPurpose;
}

bool CWallet::HasAddressBook(const CTxDestination& address) const
{
    return WITH_LOCK(cs_wallet, return mapAddressBook.count(address));
}

bool CWallet::HasDelegator(const CTxOut& out) const
{
    CTxDestination delegator;
    if (!ExtractDestination(out.scriptPubKey, delegator, false))
        return false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<CTxDestination, AddressBook::CAddressBookData>::const_iterator mi = mapAddressBook.find(delegator);
        if (mi == mapAddressBook.end())
            return false;
        return (*mi).second.purpose == AddressBook::AddressBookPurpose::DELEGATOR;
    }
}

size_t CWallet::KeypoolCountExternalKeys()
{
    return m_spk_man->KeypoolCountExternalKeys();
}

bool CWallet::TopUpKeyPool(unsigned int kpSize)
{
    return m_spk_man->TopUp(kpSize);
}

void CWallet::KeepKey(int64_t nIndex)
{
    m_spk_man->KeepDestination(nIndex);
}

void CWallet::ReturnKey(int64_t nIndex, const bool& internal, const bool& staking)
{
    // Return to key pool
    CTxDestination address; // This is not needed for now.
    uint8_t changeType = staking ? HDChain::ChangeType::STAKING : (internal ? HDChain::ChangeType::INTERNAL : HDChain::ChangeType::EXTERNAL);
    m_spk_man->ReturnDestination(nIndex, changeType, address);
}

bool CWallet::GetKeyFromPool(CPubKey& result, const uint8_t& type)
{
    return m_spk_man->GetKeyFromPool(result, type);
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    return WITH_LOCK(cs_wallet, return std::min(std::numeric_limits<int64_t>::max(), m_spk_man->GetOldestKeyPoolTime()));
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances()
{
    std::map<CTxDestination, CAmount> balances;

    {
        LOCK(cs_wallet);
        for (std::pair<uint256, CWalletTx> walletEntry : mapWallet) {
            CWalletTx* pcoin = &walletEntry.second;

            if (!IsFinalTx(*pcoin) || !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            bool fConflicted;
            int nDepth = pcoin->GetDepthAndMempool(fConflicted);
            if (fConflicted)
                continue;
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                CTxDestination addr;
                if (!IsMine(pcoin->vout[i]))
                    continue;
                if ( !ExtractDestination(pcoin->vout[i].scriptPubKey, addr) &&
                        !ExtractDestination(pcoin->vout[i].scriptPubKey, addr, true) )
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

std::set<std::set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    std::set<std::set<CTxDestination> > groupings;
    std::set<CTxDestination> grouping;

    for (std::pair<uint256, CWalletTx> walletEntry : mapWallet) {
        CWalletTx* pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0) {
            bool any_mine = false;
            // group all input addresses with each other
            for (CTxIn txin : pcoin->vin) {
                CTxDestination address;
                if (!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                if (!ExtractDestination(mapWallet[txin.prevout.hash].vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine) {
                for (CTxOut txout : pcoin->vout)
                    if (IsChange(txout)) {
                        CTxDestination txoutAddr;
                        if (!ExtractDestination(txout.scriptPubKey, txoutAddr))
                            continue;
                        grouping.insert(txoutAddr);
                    }
            }
            if (grouping.size() > 0) {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i])) {
                CTxDestination address;
                if (!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    std::set<std::set<CTxDestination>*> uniqueGroupings;        // a set of pointers to groups of addresses
    std::map<CTxDestination, std::set<CTxDestination>*> setmap; // map addresses to the unique group containing it
    for (std::set<CTxDestination> grouping : groupings) {
        // make a set of all the groups hit by this new group
        std::set<std::set<CTxDestination>*> hits;
        std::map<CTxDestination, std::set<CTxDestination>*>::iterator it;
        for (CTxDestination address : grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        std::set<CTxDestination>* merged = new std::set<CTxDestination>(grouping);
        for (std::set<CTxDestination>* hit : hits) {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for (CTxDestination element : *merged)
            setmap[element] = merged;
    }

    std::set<std::set<CTxDestination> > ret;
    for (std::set<CTxDestination>* uniqueGrouping : uniqueGroupings) {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

std::set<CTxDestination> CWallet::GetLabelAddresses(const std::string& label) const
{
    LOCK(cs_wallet);
    std::set<CTxDestination> result;
    for (const std::pair<CTxDestination, AddressBook::CAddressBookData>& item : mapAddressBook) {
        const CTxDestination& address = item.first;
        const std::string& strName = item.second.name;
        if (strName == label)
            result.insert(address);
    }
    return result;
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey, bool _internal)
{

    ScriptPubKeyMan* m_spk_man = pwallet->GetScriptPubKeyMan();
    if (!m_spk_man) {
        return false;
    }

    if (nIndex == -1) {

        // Fill the pool if needed
        m_spk_man->TopUp();
        internal = _internal;

        // Modify this for Staking addresses support if needed.
        uint8_t changeType = internal ? HDChain::ChangeType::INTERNAL : HDChain::ChangeType::EXTERNAL;
        CKeyPool keypool;
        if (!m_spk_man->GetReservedKey(changeType, nIndex, keypool))
            return false;

        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex, internal);
    nIndex = -1;
    vchPubKey = CPubKey();
}

bool CWallet::UpdatedTransaction(const uint256& hashTx)
{
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end()) {
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
            return true;
        }
    }
    return false;
}

void CWallet::LockCoin(const COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.insert(output);
}

void CWallet::UnlockCoin(const COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.erase(output);
}

void CWallet::UnlockAllCoins()
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.clear();
}

bool CWallet::IsLockedCoin(const uint256& hash, unsigned int n) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    const COutPoint outpt(hash, n);

    return (setLockedCoins.count(outpt) > 0);
}

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
         it != setLockedCoins.end(); it++) {
        COutPoint outpt = (*it);
        vOutpts.push_back(outpt);
    }
}

/** @} */ // end of Actions

class CAffectedKeysVisitor : public boost::static_visitor<void>
{
private:
    const CKeyStore& keystore;
    std::vector<CKeyID>& vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore& keystoreIn, std::vector<CKeyID>& vKeysIn) : keystore(keystoreIn), vKeys(vKeysIn) {}

    void Process(const CScript& script)
    {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(script, type, vDest, nRequired)) {
            for (const CTxDestination& dest : vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID& keyId)
    {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID& scriptId)
    {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
    }

    void operator()(const CNoDestination& none) {}
};

std::vector<CKeyID> CWallet::GetAffectedKeys(const CScript& spk)
{
    std::vector<CKeyID> ret;
    std::vector<CKeyID> vAffected;
    CAffectedKeysVisitor(*this, vAffected).Process(spk);
    for (const CKeyID& keyid : vAffected) {
        ret.push_back(keyid);
    }
    return ret;
}


void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndex* pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    GetKeys(setKeys);
    for (const CKeyID& keyid : setKeys) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }
    setKeys.clear();

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {
        // iterate over all wallet transactions...
        const CWalletTx& wtx = (*it).second;
        BlockMap::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && chainActive.Contains(blit->second)) {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            for (const CTxOut& txout : wtx.vout) {
                // iterate over all their outputs
                CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
                for (const CKeyID& keyid : vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = blit->second;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
        mapKeyBirth[it->first] = it->second->GetBlockTime() - 7200; // block times can be 2h off
}

bool CWallet::AddDestData(const CTxDestination& dest, const std::string& key, const std::string& value)
{
    if (!IsValidDestination(dest))
        return false;

    mapAddressBook[dest].destdata.emplace(key, value);
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteDestData(EncodeDestination(dest), key, value);
}

bool CWallet::EraseDestData(const CTxDestination& dest, const std::string& key)
{
    if (!mapAddressBook[dest].destdata.erase(key))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).EraseDestData(EncodeDestination(dest), key);
}

bool CWallet::LoadDestData(const CTxDestination& dest, const std::string& key, const std::string& value)
{
    mapAddressBook[dest].destdata.emplace(key, value);
    return true;
}

void CWallet::AutoCombineDust(CConnman* connman)
{
    LOCK2(cs_main, cs_wallet);
    const CBlockIndex* tip = chainActive.Tip();
    if (tip->nTime < (GetAdjustedTime() - 300) || IsLocked()) {
        return;
    }

    std::map<CTxDestination, std::vector<COutput> > mapCoinsByAddress = AvailableCoinsByAddress(true, nAutoCombineThreshold * COIN);

    //coins are sectioned by address. This combination code only wants to combine inputs that belong to the same address
    for (std::map<CTxDestination, std::vector<COutput> >::iterator it = mapCoinsByAddress.begin(); it != mapCoinsByAddress.end(); it++) {
        std::vector<COutput> vCoins, vRewardCoins;
        bool maxSize = false;
        vCoins = it->second;

        // We don't want the tx to be refused for being too large
        // we use 50 bytes as a base tx size (2 output: 2*34 + overhead: 10 -> 90 to be certain)
        unsigned int txSizeEstimate = 90;

        //find masternode rewards that need to be combined
        CCoinControl* coinControl = new CCoinControl();
        CAmount nTotalRewardsValue = 0;
        for (const COutput& out : vCoins) {
            if (!out.fSpendable)
                continue;
            //no coins should get this far if they dont have proper maturity, this is double checking
            if (out.tx->IsCoinStake() && out.tx->GetDepthInMainChain() < Params().GetConsensus().nCoinbaseMaturity + 1)
                continue;

            // no p2cs accepted, those coins are "locked"
            if (out.tx->vout[out.i].scriptPubKey.IsPayToColdStaking())
                continue;

            COutPoint outpt(out.tx->GetHash(), out.i);
            coinControl->Select(outpt);
            vRewardCoins.push_back(out);
            nTotalRewardsValue += out.Value();

            // Combine to the threshold and not way above
            if (nTotalRewardsValue > nAutoCombineThreshold * COIN)
                break;

            // Around 180 bytes per input. We use 190 to be certain
            txSizeEstimate += 190;
            if (txSizeEstimate >= MAX_STANDARD_TX_SIZE - 200) {
                maxSize = true;
                break;
            }
        }

        //if no inputs found then return
        if (!coinControl->HasSelected())
            continue;

        //we cannot combine one coin with itself
        if (vRewardCoins.size() <= 1)
            continue;

        std::vector<CRecipient> vecSend;
        CScript scriptPubKey = GetScriptForDestination(it->first);
        vecSend.emplace_back(scriptPubKey, nTotalRewardsValue, false);

        //Send change to same address
        CTxDestination destMyAddress;
        if (!ExtractDestination(scriptPubKey, destMyAddress)) {
            LogPrintf("AutoCombineDust: failed to extract destination\n");
            continue;
        }
        coinControl->destChange = destMyAddress;

        // Create the transaction and commit it to the network
        CWalletTx wtx;
        CReserveKey keyChange(this); // this change address does not end up being used, because change is returned with coin control switch
        std::string strErr;
        CAmount nFeeRet = 0;
        int nChangePosInOut = -1;

        // 10% safety margin to avoid "Insufficient funds" errors
        vecSend[0].nAmount = nTotalRewardsValue - (nTotalRewardsValue / 10);

        if (!CreateTransaction(vecSend, wtx, keyChange, nFeeRet, nChangePosInOut, strErr, coinControl, ALL_COINS, true, false, CAmount(0))) {
            LogPrintf("AutoCombineDust createtransaction failed, reason: %s\n", strErr);
            continue;
        }

        //we don't combine below the threshold unless the fees are 0 to avoid paying fees over fees over fees
        if (!maxSize && nTotalRewardsValue < nAutoCombineThreshold * COIN && nFeeRet > 0)
            continue;

        const CWallet::CommitResult& res = CommitTransaction(wtx, keyChange, connman);
        if (res.status != CWallet::CommitStatus::OK) {
            LogPrintf("AutoCombineDust transaction commit failed\n");
            continue;
        }

        LogPrintf("AutoCombineDust sent transaction\n");

        delete coinControl;
    }
}

bool CWallet::MultiSend()
{
    return false;
    /* disable multisend
    LOCK2(cs_main, cs_wallet);
    // Stop the old blocks from sending multisends
    const CBlockIndex* tip = chainActive.Tip();
    int chainTipHeight = tip->nHeight;
    if (tip->nTime < (GetAdjustedTime() - 300) || IsLocked()) {
        return false;
    }

    if (chainTipHeight <= nLastMultiSendHeight) {
        LogPrintf("Multisend: lastmultisendheight is higher than current best height\n");
        return false;
    }

    std::vector<COutput> vCoins;
    AvailableCoins(&vCoins);
    bool stakeSent = false;
    bool mnSent = false;
    for (const COutput& out : vCoins) {

        //need output with precise confirm count - this is how we identify which is the output to send
        if (out.tx->GetDepthInMainChain() != Params().GetConsensus().nCoinbaseMaturity + 1)
            continue;

        bool isCoinStake = out.tx->IsCoinStake();
        bool isMNOutPoint = isCoinStake && (out.i == ((int)out.tx->vout.size()) - 1) &&
                (out.tx->vout[1].scriptPubKey != out.tx->vout[out.i].scriptPubKey);
        bool sendMSonMNReward = fMultiSendMasternodeReward && isMNOutPoint;
        bool sendMSOnStake = fMultiSendStake && isCoinStake && !sendMSonMNReward; //output is either mnreward or stake reward, not both

        if (!(sendMSOnStake || sendMSonMNReward))
            continue;

        CTxDestination destMyAddress;
        if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, destMyAddress)) {
            LogPrintf("Multisend: failed to extract destination\n");
            continue;
        }

        //Disabled Addresses won't send MultiSend transactions
        if (vDisabledAddresses.size() > 0) {
            for (unsigned int i = 0; i < vDisabledAddresses.size(); i++) {
                if (vDisabledAddresses[i] == CBitcoinAddress(destMyAddress).ToString()) {
                    LogPrintf("Multisend: disabled address preventing multisend\n");
                    return false;
                }
            }
        }

        // create new coin control, populate it with the selected utxo, create sending vector
        CCoinControl cControl;
        COutPoint outpt(out.tx->GetHash(), out.i);
        cControl.Select(outpt);
        cControl.destChange = destMyAddress;

        CWalletTx wtx;
        CReserveKey keyChange(this); // this change address does not end up being used, because change is returned with coin control switch
        CAmount nFeeRet = 0;
        std::vector<CRecipient> vecSend;

        // loop through multisend vector and add amounts and addresses to the sending vector
        const isminefilter filter = ISMINE_SPENDABLE;
        CAmount nAmount = 0;
        for (unsigned int i = 0; i < vMultiSend.size(); i++) {
            // MultiSend vector is a pair of 1)Address as a std::string 2) Percent of stake to send as an int
            nAmount = ((out.tx->GetCredit(filter) - out.tx->GetDebit(filter)) * vMultiSend[i].second) / 100;
            CBitcoinAddress strAddSend(vMultiSend[i].first);
            CScript scriptPubKey;
            scriptPubKey = GetScriptForDestination(strAddSend.Get());
            vecSend.emplace_back(scriptPubKey, nAmount, false);
        }

        //get the fee amount
        CWalletTx wtxdummy;
        std::string strErr;
        int nChangePosInOut = -1;
        CreateTransaction(vecSend, wtxdummy, keyChange, nFeeRet, nChangePosInOut, strErr, &cControl, ALL_COINS, true, false, CAmount(0));
        CAmount nLastSendAmount = vecSend[vecSend.size() - 1].nAmount;
        if (nLastSendAmount < nFeeRet + 500) {
            LogPrintf("%s: fee of %d is too large to insert into last output\n", __func__, nFeeRet + 500);
            return false;
        }
        vecSend[vecSend.size() - 1].nAmount = nLastSendAmount - nFeeRet - 500;

        // Create the transaction and commit it to the network
        if (!CreateTransaction(vecSend, wtx, keyChange, nFeeRet, int nChangePosInOut, strErr, &cControl, ALL_COINS, true, false, CAmount(0))) {
            LogPrintf("MultiSend createtransaction failed\n");
            return false;
        }

        const CWallet::CommitResult& res = CommitTransaction(wtx, keyChange);
        if (res.status != CWallet::CommitStatus::OK) {
            LogPrintf("MultiSend transaction commit failed\n");
            return false;
        } else
            fMultiSendNotify = true;

        //write nLastMultiSendHeight to DB
        CWalletDB walletdb(strWalletFile);
        nLastMultiSendHeight = chainActive.Tip()->nHeight;
        if (!walletdb.WriteMSettings(fMultiSendStake, fMultiSendMasternodeReward, nLastMultiSendHeight))
            LogPrintf("Failed to write MultiSend setting to DB\n");

        LogPrintf("MultiSend successfully sent\n");

        //set which MultiSend triggered
        if (sendMSOnStake)
            stakeSent = true;
        else
            mnSent = true;

        //stop iterating if we have sent out all the MultiSend(s)
        if ((stakeSent && mnSent) || (stakeSent && !fMultiSendMasternodeReward) || (mnSent && !fMultiSendStake))
            return true;
    }

    return true;
    */
}

std::string CWallet::GetWalletHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-backuppath=<dir|file>", _("Specify custom backup path to add a copy of any wallet backup. If set as dir, every backup generates a timestamped file. If set as file, will rewrite to that file every backup."));
    strUsage += HelpMessageOpt("-createwalletbackups=<n>", strprintf(_("Number of automatic wallet backups (default: %d)"), DEFAULT_CREATEWALLETBACKUPS));
    strUsage += HelpMessageOpt("-custombackupthreshold=<n>", strprintf(_("Number of custom location backups to retain (default: %d)"), DEFAULT_CUSTOMBACKUPTHRESHOLD));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), DEFAULT_KEYPOOL_SIZE));
    strUsage += HelpMessageOpt("-legacywallet", _("On first run, create a legacy wallet instead of a HD wallet"));
    strUsage += HelpMessageOpt("-maxtxfee=<amt>", strprintf(_("Maximum total fees to use in a single wallet transaction, setting too low may abort large transactions (default: %s)"), FormatMoney(maxTxFee)));
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf(_("Fees (in %s/Kb) smaller than this are considered zero fee for transaction creation (default: %s)"), CURRENCY_UNIT, FormatMoney(CWallet::minTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("Fee (in %s/kB) to add to transactions you send (default: %s)"), CURRENCY_UNIT, FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet file") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-sendfreetransactions", strprintf(_("Send transactions as zero-fee transactions if possible (default: %u)"), DEFAULT_SEND_FREE_TRANSACTIONS));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), DEFAULT_SPEND_ZEROCONF_CHANGE));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), 1));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), DEFAULT_WALLET_DAT));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") +
        " " + _("(1 = keep tx meta data e.g. payment request information, 2 = drop tx meta data)"));
    strUsage += HelpMessageGroup(_("Mining/Staking options:"));
    strUsage += HelpMessageOpt("-coldstaking=<n>", strprintf(_("Enable cold staking functionality (0-1, default: %u). Disabled if staking=0"), DEFAULT_COLDSTAKING));
    strUsage += HelpMessageOpt("-gen", strprintf(_("Generate coins (default: %u)"), DEFAULT_GENERATE));
    strUsage += HelpMessageOpt("-genproclimit=<n>", strprintf(_("Set the number of threads for coin generation if enabled (-1 = all cores, default: %d)"), DEFAULT_GENERATE_PROCLIMIT));
    strUsage += HelpMessageOpt("-minstakesplit=<amt>", strprintf(_("Minimum positive amount (in PIV) allowed by GUI and RPC for the stake split threshold (default: %s)"), FormatMoney(DEFAULT_MIN_STAKE_SPLIT_THRESHOLD)));
    strUsage += HelpMessageOpt("-staking=<n>", strprintf(_("Enable staking functionality (0-1, default: %u)"), DEFAULT_STAKING));
    if (showDebug) {
        strUsage += HelpMessageGroup(_("Wallet debugging/testing options:"));
        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf(_("Flush database activity from memory pool to disk log every <n> megabytes (default: %u)"), DEFAULT_WALLET_DBLOGSIZE));
        strUsage += HelpMessageOpt("-flushwallet", strprintf(_("Run a thread to flush wallet periodically (default: %u)"), DEFAULT_FLUSHWALLET));
        strUsage += HelpMessageOpt("-printcoinstake", _("Display verbose coin stake messages in the debug.log file."));
        strUsage += HelpMessageOpt("-printstakemodifier", _("Display the stake modifier calculations in the debug.log file."));
        strUsage += HelpMessageOpt("-privdb", strprintf(_("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)"), DEFAULT_WALLET_PRIVDB));
    }

    return strUsage;
}

CWallet* CWallet::CreateWalletFromFile(const std::string walletFile)
{
    // needed to restore wallet transaction meta data after -zapwallettxes
    std::vector<CWalletTx> vWtx;

    if (GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        CWallet *tempWallet = new CWallet(walletFile);
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DB_LOAD_OK) {
            UIError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        }

        delete tempWallet;
        tempWallet = nullptr;
    }

    uiInterface.InitMessage(_("Loading wallet..."));
    fVerifyingBlocks = true;

    int64_t nStart = GetTimeMillis();
    bool fFirstRun = true;
    CWallet *walletInstance = new CWallet(walletFile);
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK) {
        if (nLoadWalletRet == DB_CORRUPT) {
            UIError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        } else if (nLoadWalletRet == DB_NONCRITICAL_ERROR) {
            UIWarning(strprintf(_("Warning: error reading %s! All keys read correctly, but transaction data"
                         " or address book entries might be missing or incorrect."), walletFile));
        } else if (nLoadWalletRet == DB_TOO_NEW) {
            UIError(strprintf(_("Error loading %s: Wallet requires newer version of PIVX Core"), walletFile));
            return nullptr;
        } else if (nLoadWalletRet == DB_NEED_REWRITE) {
            UIError(_("Wallet needed to be rewritten: restart PIVX Core to complete"));
            return nullptr;
        } else {
            UIError(strprintf(_("Error loading %s\n"), walletFile));
            return nullptr;
        }
    }

    // check minimum stake split threshold
    if (walletInstance->nStakeSplitThreshold && walletInstance->nStakeSplitThreshold < CWallet::minStakeSplitThreshold) {
        LogPrintf("WARNING: stake split threshold value %s too low. Restoring to minimum value %s.\n",
                FormatMoney(walletInstance->nStakeSplitThreshold), FormatMoney(CWallet::minStakeSplitThreshold));
        walletInstance->nStakeSplitThreshold = CWallet::minStakeSplitThreshold;
    }

    int prev_version = walletInstance->GetVersion();

    // Forced upgrade
    const bool fLegacyWallet = GetBoolArg("-legacywallet", false);
    if (GetBoolArg("-upgradewallet", fFirstRun && !fLegacyWallet)) {
        if (prev_version <= FEATURE_PRE_PIVX && walletInstance->IsLocked()) {
            // Cannot upgrade a locked wallet
            UIError("Cannot upgrade a locked wallet.");
            return nullptr;
        }

        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = FEATURE_LATEST;
            walletInstance->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        } else
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < walletInstance->GetVersion()) {
            UIError("Cannot downgrade wallet\n");
            return nullptr;
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

    // Upgrade to HD only if explicit upgrade was requested
    if (GetBoolArg("-upgradewallet", false)) {
        std::string upgradeError;
        if (!walletInstance->Upgrade(upgradeError, prev_version)) {
            UIError(upgradeError);
            return nullptr;
        }
    }

    if (fFirstRun) {
        if (!fLegacyWallet) {
            // Create new HD Wallet
            LogPrintf("Creating HD Wallet\n");
            // Ensure this wallet can only be opened by clients supporting HD.
            walletInstance->SetMinVersion(FEATURE_LATEST);
            walletInstance->SetupSPKM();
        } else {
            if (!Params().IsRegTestNet()) {
                UIError("Legacy wallets can only be created on RegTest.");
                return nullptr;
            }
            // Create legacy wallet
            LogPrintf("Creating Pre-HD Wallet\n");
            walletInstance->SetMaxVersion(FEATURE_PRE_PIVX);
        }

        // Top up the keypool
        if (!walletInstance->TopUpKeyPool()) {
            // Error generating keys
            UIError("Unable to generate initial key!");
            return nullptr;
        }

        walletInstance->SetBestChain(chainActive.GetLocator());
    }

    LogPrintf("Wallet completed loading in %15dms\n", GetTimeMillis() - nStart);

    CzPIVWallet* zwalletInstance = new CzPIVWallet(walletInstance);
    walletInstance->setZWallet(zwalletInstance);

    RegisterValidationInterface(walletInstance);

    CBlockIndex* pindexRescan = chainActive.Tip();
    if (GetBoolArg("-rescan", false))
        pindexRescan = chainActive.Genesis();
    else {
        CWalletDB walletdb(walletFile);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
        else
            pindexRescan = chainActive.Genesis();
    }
    if (chainActive.Tip() && chainActive.Tip() != pindexRescan) {
        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
        const int64_t nWalletRescanTime = GetTimeMillis();
        if (walletInstance->ScanForWalletTransactions(pindexRescan, true, true) == -1) {
            UIError(_("Shutdown requested over the txs scan. Exiting."));
            return nullptr;
        }
        LogPrintf("Rescan completed in %15dms\n", GetTimeMillis() - nWalletRescanTime);
        walletInstance->SetBestChain(chainActive.GetLocator());
        CWalletDB::IncrementUpdateCounter();

        // Restore wallet transaction metadata after -zapwallettxes=1
        if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2") {
            CWalletDB walletdb(walletFile);
            for (const CWalletTx& wtxOld : vWtx) {
                uint256 hash = wtxOld.GetHash();
                std::map<uint256, CWalletTx>::iterator mi = walletInstance->mapWallet.find(hash);
                if (mi != walletInstance->mapWallet.end()) {
                    const CWalletTx* copyFrom = &wtxOld;
                    CWalletTx* copyTo = &mi->second;
                    copyTo->mapValue = copyFrom->mapValue;
                    copyTo->vOrderForm = copyFrom->vOrderForm;
                    copyTo->nTimeReceived = copyFrom->nTimeReceived;
                    copyTo->nTimeSmart = copyFrom->nTimeSmart;
                    copyTo->fFromMe = copyFrom->fFromMe;
                    copyTo->nOrderPos = copyFrom->nOrderPos;
                    walletdb.WriteTx(*copyTo);
                }
            }
        }
    }
    fVerifyingBlocks = false;

    if (!zwalletInstance->GetMasterSeed().IsNull()) {
        //Inititalize zPIVWallet
        uiInterface.InitMessage(_("Syncing zPIV wallet..."));

        //Load zerocoin mint hashes to memory
        walletInstance->zpivTracker->Init();
        zwalletInstance->LoadMintPoolFromDB();
        zwalletInstance->SyncWithChain();
    }

    return walletInstance;
}

bool CWallet::InitLoadWallet()
{
    if (GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        pwalletMain = nullptr;
        LogPrintf("Wallet disabled!\n");
        return true;
    }

    std::string walletFile = GetArg("-wallet", DEFAULT_WALLET_DAT);

    CWallet * const pwallet = CreateWalletFromFile(walletFile);
    if (!pwallet) {
        return false;
    }
    pwalletMain = pwallet;

    return true;
}

std::atomic<bool> CWallet::fFlushThreadRunning(false);

void CWallet::postInitProcess(boost::thread_group& threadGroup)
{
    // Add wallet transactions that aren't already in a block to mapTransactions
    ReacceptWalletTransactions(/*fFirstLoad*/true);

    // Run a thread to flush wallet periodically
    if (!CWallet::fFlushThreadRunning.exchange(true)) {
        threadGroup.create_thread(ThreadFlushWalletDB);
    }
}

CKeyPool::CKeyPool()
{
    nTime = GetTime();
    type = HDChain::ChangeType::EXTERNAL;
    m_pre_split = false;
}

CKeyPool::CKeyPool(const CPubKey& vchPubKeyIn, const uint8_t& _type)
{
    nTime = GetTime();
    vchPubKey = vchPubKeyIn;
    type = _type;
    m_pre_split = false;
}

CWalletKey::CWalletKey(int64_t nExpires)
{
    nTimeCreated = (nExpires ? GetTime() : 0);
    nTimeExpires = nExpires;
}

void CMerkleTx::SetMerkleBranch(const CBlockIndex* pindex, int posInBlock)
{
    // Update the tx's hashBlock
    hashBlock = pindex->GetBlockHash();

    // set the position of the transaction in the block
    nIndex = posInBlock;
}

int CMerkleTx::GetDepthInMainChain(bool enableIX) const
{
    const CBlockIndex* pindexRet = nullptr;
    return GetDepthInMainChain(pindexRet, enableIX);
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex*& pindexRet, bool enableIX) const
{
    if (hashUnset())
        return 0;
    AssertLockHeld(cs_main);
    int nResult;

    // Find the block it claims to be in
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end()) {
        nResult = 0;
    } else {
        CBlockIndex* pindex = (*mi).second;
        if (!pindex || !chainActive.Contains(pindex)) {
            nResult = 0;
        } else {
            pindexRet = pindex;
            nResult = ((nIndex == -1) ? (-1) : 1) * (chainActive.Height() - pindex->nHeight + 1);
        }
    }

    if (enableIX) {
        if (nResult < 6) {
            int signatures = GetTransactionLockSignatures();
            if (signatures >= SWIFTTX_SIGNATURES_REQUIRED) {
                return nSwiftTXDepth + nResult;
            }
        }
    }

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    LOCK(cs_main);
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    return std::max(0, (Params().GetConsensus().nCoinbaseMaturity + 1) - GetDepthInMainChain());
}

bool CMerkleTx::IsInMainChain() const
{
    return GetDepthInMainChain(false) > 0;
}

bool CMerkleTx::IsInMainChainImmature() const
{
    if (!IsCoinBase() && !IsCoinStake()) return false;
    const int depth = GetDepthInMainChain(false);
    return (depth > 0 && depth <= Params().GetConsensus().nCoinbaseMaturity);
}


bool CMerkleTx::AcceptToMemoryPool(bool fLimitFree, bool fRejectInsaneFee, bool ignoreFees)
{
    CValidationState state;
    bool fAccepted = ::AcceptToMemoryPool(mempool, state, MakeTransactionRef(*this), fLimitFree, nullptr, false, fRejectInsaneFee, ignoreFees);
    if (!fAccepted)
        LogPrintf("%s : %s\n", __func__, state.GetRejectReason());
    return fAccepted;
}

int CMerkleTx::GetTransactionLockSignatures() const
{
    if (fLargeWorkForkFound || fLargeWorkInvalidChainFound) return -2;
    if (!sporkManager.IsSporkActive(SPORK_2_SWIFTTX)) return -3;
    if (!fEnableSwiftTX) return -1;

    //compile consessus vote
    std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(GetHash());
    if (i != mapTxLocks.end()) {
        return (*i).second.CountSignatures();
    }

    return -1;
}

bool CMerkleTx::IsTransactionLockTimedOut() const
{
    if (!fEnableSwiftTX) return 0;

    //compile consessus vote
    std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(GetHash());
    if (i != mapTxLocks.end()) {
        return GetTime() > (*i).second.nTimeout;
    }

    return false;
}

std::string CWallet::GetUniqueWalletBackupName() const
{
    return strprintf("%s%s", strWalletFile, DateTimeStrFormat(".%Y-%m-%d-%H-%M", GetTime()));
}

CWallet::CWallet()
{
    SetNull();
}

CWallet::CWallet(std::string strWalletFileIn)
{
    SetNull();

    strWalletFile = strWalletFileIn;
    fFileBacked = true;
}

CWallet::~CWallet()
{
    delete zwallet;
    delete pwalletdbEncryption;
    delete pStakerStatus;
}

void CWallet::SetNull()
{
    nWalletVersion = FEATURE_BASE;
    nWalletMaxVersion = FEATURE_BASE;
    fFileBacked = false;
    nMasterKeyMaxID = 0;
    pwalletdbEncryption = NULL;
    nOrderPosNext = 0;
    nNextResend = 0;
    nLastResend = 0;
    nTimeFirstKey = 0;
    fWalletUnlockStaking = false;

    // Staker status (last hashed block and time)
    if (pStakerStatus) {
        pStakerStatus->SetNull();
    } else {
        pStakerStatus = new CStakerStatus();
    }
    // Stake split threshold
    nStakeSplitThreshold = DEFAULT_STAKE_SPLIT_THRESHOLD;

    // User-defined fee PIV/kb
    fUseCustomFee = false;
    nCustomFee = CWallet::minTxFee.GetFeePerK();

    //MultiSend
    vMultiSend.clear();
    fMultiSendStake = false;
    fMultiSendMasternodeReward = false;
    fMultiSendNotify = false;
    strMultiSendChangeAddress = "";
    nLastMultiSendHeight = 0;
    vDisabledAddresses.clear();

    //Auto Combine Dust
    fCombineDust = false;
    nAutoCombineThreshold = 0;

    // Sapling.
    m_sspk_man->nWitnessCacheSize = 0;
    m_sspk_man->nWitnessCacheNeedsUpdate = true;
}

bool CWallet::isMultiSendEnabled()
{
    return fMultiSendMasternodeReward || fMultiSendStake;
}

void CWallet::setMultiSendDisabled()
{
    fMultiSendMasternodeReward = false;
    fMultiSendStake = false;
}

bool CWallet::CanSupportFeature(enum WalletFeature wf)
{
    AssertLockHeld(cs_wallet);
    return nWalletMaxVersion >= wf;
}

bool CWallet::LoadMinVersion(int nVersion)
{
    AssertLockHeld(cs_wallet);
    nWalletVersion = nVersion;
    nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion);
    return true;
}

isminetype CWallet::IsMine(const CTxOut& txout) const
{
    return ::IsMine(*this, txout.scriptPubKey);
}

CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter) const
{
    if (!Params().GetConsensus().MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetCredit() : value out of range");
    return ((IsMine(txout) & filter) ? txout.nValue : 0);
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!Params().GetConsensus().MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetChange() : value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction& tx) const
{
    for (const CTxOut& txout : tx.vout)
        if (IsMine(txout))
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransaction& tx) const
{
    if (GetDebit(tx, ISMINE_ALL) > 0) {
        return true;
    }

    if (tx.hasSaplingData()) {
        for (const SpendDescription& spend : tx.sapData->vShieldedSpend) {
            if (m_sspk_man->IsSaplingNullifierFromMe(spend.nullifier)) {
                return true;
            }
        }
    }

    return false;
}

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nDebit = 0;
    for (const CTxIn& txin : tx.vin) {
        nDebit += GetDebit(txin, filter);
        if (!Params().GetConsensus().MoneyRange(nDebit))
            throw std::runtime_error("CWallet::GetDebit() : value out of range");
    }
    return nDebit;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        nCredit += GetCredit(tx.vout[i], filter);
    }
    if (!Params().GetConsensus().MoneyRange(nCredit))
        throw std::runtime_error("CWallet::GetCredit() : value out of range");
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout) {
        nChange += GetChange(txout);
        if (!Params().GetConsensus().MoneyRange(nChange))
            throw std::runtime_error("CWallet::GetChange() : value out of range");
    }
    return nChange;
}

unsigned int CWallet::GetKeyPoolSize()
{
    return m_spk_man->GetKeyPoolSize();
}

unsigned int CWallet::GetStakingKeyPoolSize()
{
    return m_spk_man->GetStakingKeyPoolSize();
}

int CWallet::GetVersion()
{
    LOCK(cs_wallet);
    return nWalletVersion;
}

///////////////// Sapling Methods //////////////////////////
////////////////////////////////////////////////////////////

libzcash::SaplingPaymentAddress CWallet::GenerateNewSaplingZKey() { return m_sspk_man->GenerateNewSaplingZKey(); }

void CWallet::IncrementNoteWitnesses(const CBlockIndex* pindex,
                            const CBlock* pblock,
                            SaplingMerkleTree& saplingTree) { m_sspk_man->IncrementNoteWitnesses(pindex, pblock, saplingTree); }

void CWallet::DecrementNoteWitnesses(const CBlockIndex* pindex) { m_sspk_man->DecrementNoteWitnesses(pindex); }

bool CWallet::AddSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key) { return m_sspk_man->AddSaplingZKey(key); }

bool CWallet::AddSaplingIncomingViewingKeyW(
        const libzcash::SaplingIncomingViewingKey &ivk,
        const libzcash::SaplingPaymentAddress &addr) { return m_sspk_man->AddSaplingIncomingViewingKey(ivk, addr); }

bool CWallet::AddCryptedSaplingSpendingKeyW(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char> &vchCryptedSecret) { return m_sspk_man->AddCryptedSaplingSpendingKeyDB(extfvk, vchCryptedSecret); }

bool CWallet::HaveSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &zaddr) const { return m_sspk_man->HaveSpendingKeyForPaymentAddress(zaddr); }
bool CWallet::LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key) { return m_sspk_man->LoadSaplingZKey(key); }
bool CWallet::LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta) { return m_sspk_man->LoadSaplingZKeyMetadata(ivk, meta); }
bool CWallet::LoadCryptedSaplingZKey(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                            const std::vector<unsigned char> &vchCryptedSecret) { return m_sspk_man->LoadCryptedSaplingZKey(extfvk, vchCryptedSecret); }

bool CWallet::LoadSaplingPaymentAddress(
        const libzcash::SaplingPaymentAddress &addr,
        const libzcash::SaplingIncomingViewingKey &ivk) { return m_sspk_man->LoadSaplingPaymentAddress(addr, ivk); }

///////////////// End Sapling Methods //////////////////////
////////////////////////////////////////////////////////////

CWalletTx::CWalletTx()
{
    Init(NULL);
}

CWalletTx::CWalletTx(const CWallet* pwalletIn)
{
    Init(pwalletIn);
}

CWalletTx::CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn)
{
    Init(pwalletIn);
}

CWalletTx::CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn)
{
    Init(pwalletIn);
}

void CWalletTx::Init(const CWallet* pwalletIn)
{
    pwallet = pwalletIn;
    mapValue.clear();
    mapSaplingNoteData.clear();
    vOrderForm.clear();
    fTimeReceivedIsTxTime = false;
    nTimeReceived = 0;
    nTimeSmart = 0;
    fFromMe = false;
    fChangeCached = false;
    nChangeCached = 0;
    nOrderPos = -1;
}

bool CWalletTx::IsTrusted() const
{
    bool fConflicted = false;
    int nDepth = 0;
    return IsTrusted(nDepth, fConflicted);
}

bool CWalletTx::IsTrusted(int& nDepth, bool& fConflicted) const
{
    // Quick answer in most cases
    if (!IsFinalTx(*this))
        return false;

    nDepth = GetDepthAndMempool(fConflicted);

    if (fConflicted) // Don't trust unconfirmed transactions from us unless they are in the mempool.
        return false;
    if (nDepth >= 1)
        return true;
    if (nDepth < 0)
        return false;
    if (!bSpendZeroConfChange || !IsFromMe(ISMINE_ALL)) // using wtx's cached debit
        return false;

    // Trusted if all inputs are from us and are in the mempool:
    for (const CTxIn& txin : vin) {
        // Transactions not sent by us: not trusted
        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == nullptr)
            return false;
        const CTxOut& parentOut = parent->vout[txin.prevout.n];
        if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
            return false;
    }
    return true;
}

int CWalletTx::GetDepthAndMempool(bool& fConflicted, bool enableIX) const
{
    int ret = GetDepthInMainChain(enableIX);
    fConflicted = (ret == 0 && !InMempool());  // not in chain nor in mempool
    return ret;
}

bool CWalletTx::IsEquivalentTo(const CWalletTx& _tx) const
{
    CMutableTransaction tx1 {*this};
    CMutableTransaction tx2 {_tx};
    for (auto& txin : tx1.vin) txin.scriptSig = CScript();
    for (auto& txin : tx2.vin) txin.scriptSig = CScript();
    return CTransaction(tx1) == CTransaction(tx2);
}

void CWalletTx::MarkDirty()
{
    m_amounts[DEBIT].Reset();
    m_amounts[CREDIT].Reset();
    m_amounts[IMMATURE_CREDIT].Reset();
    m_amounts[AVAILABLE_CREDIT].Reset();
    nChangeCached = 0;
    fChangeCached = false;
    fStakeDelegationVoided = false;
}

void CWalletTx::BindWallet(CWallet* pwalletIn)
{
    pwallet = pwalletIn;
    MarkDirty();
}

void CWalletTx::SetSaplingNoteData(mapSaplingNoteData_t &noteData)
{
    mapSaplingNoteData.clear();
    for (const std::pair<SaplingOutPoint, SaplingNoteData> nd : noteData) {
        if (nd.first.n < sapData->vShieldedOutput.size()) {
            mapSaplingNoteData[nd.first] = nd.second;
        } else {
            throw std::logic_error("CWalletTx::SetSaplingNoteData(): Invalid note");
        }
    }
}

bool CWalletTx::HasP2CSInputs() const
{
    return GetStakeDelegationDebit(true) > 0 || GetColdStakingDebit(true) > 0;
}

CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(*this);
    fChangeCached = true;
    return nChangeCached;
}

bool CWalletTx::IsFromMe(const isminefilter& filter) const
{
    return (GetDebit(filter) > 0);
}

CStakeableOutput::CStakeableOutput(const CWalletTx* txIn, int iIn, int nDepthIn, bool fSpendableIn, bool fSolvableIn,
                                   const CBlockIndex*& _pindex) : COutput(txIn, iIn, nDepthIn, fSpendableIn, fSolvableIn),
                                                                pindex(_pindex) {}

