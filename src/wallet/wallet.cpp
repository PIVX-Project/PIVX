// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/pivx-config.h"
#endif

#include "wallet/wallet.h"

#include "checkpoints.h"
#include "coincontrol.h"
#include "evo/providertx.h"
#include "guiinterfaceutil.h"
#include "policy/policy.h"
#include "sapling/key_io_sapling.h"
#include "script/sign.h"
#include "scheduler.h"
#include "shutdown.h"
#include "spork.h"
#include "util/validation.h"
#include "utilmoneystr.h"
#include "wallet/fees.h"

#include <future>
#include <boost/algorithm/string/replace.hpp>

std::vector<CWalletRef> vpwallets;
/**
 * Settings
 */
CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;
unsigned int nTxConfirmTarget = 1;
bool bdisableSystemnotifications = false; // Those bubbles can be annoying and slow down the UI when you get lots of trx
bool fPayAtLeastCustomFee = true;
bool bSpendZeroConfChange = DEFAULT_SPEND_ZEROCONF_CHANGE;

/**
 * Fees smaller than this (in upiv) are considered zero fee (for transaction creation)
 * We are ~100 times smaller then bitcoin now (2015-06-23), set minTxFee 10 times higher
 * so it's still 10 times lower comparing to bitcoin.
 * Override with -mintxfee
 */
CFeeRate CWallet::minTxFee = CFeeRate(DEFAULT_TRANSACTION_MINFEE);

/**
 * minimum accpeted value for stake split threshold
 */
CAmount CWallet::minStakeSplitThreshold = DEFAULT_MIN_STAKE_SPLIT_THRESHOLD;

const uint256 CWalletTx::ABANDON_HASH(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));

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
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->tx->vout[i].nValue));
}

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
        ret.emplace_back(keyid);
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

bool CWallet::SetupSPKM(bool newKeypool, bool memOnly)
{
    if (m_spk_man->SetupGeneration(newKeypool, true, memOnly)) {
        LogPrintf("%s : spkm setup completed\n", __func__);
        return ActivateSaplingWallet(memOnly);
    }
    return false;
}

bool CWallet::ActivateSaplingWallet(bool memOnly)
{
    if (m_sspk_man->SetupGeneration(m_spk_man->GetHDChain().GetID(), true, memOnly)) {
        LogPrintf("%s : sapling spkm setup completed\n", __func__);
        // Just to be triple sure, if the version isn't updated, set it.
        if (!SetMinVersion(WalletFeature::FEATURE_SAPLING)) {
            LogPrintf("%s : ERROR: wallet cannot upgrade to sapling features. Try to upgrade using the 'upgradewallet' RPC command\n", __func__);
            return false;
        }
        return true;
    }
    return false;
}

bool CWallet::IsHDEnabled() const
{
    return m_spk_man->IsHDEnabled();
}

bool CWallet::IsSaplingUpgradeEnabled() const
{
    return m_sspk_man->IsEnabled();
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return nullptr;
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

CallResult<CTxDestination> CWallet::getNewAddress(const std::string& label)
{
    return getNewAddress(label, AddressBook::AddressBookPurpose::RECEIVE);
}

CallResult<CTxDestination> CWallet::getNewStakingAddress(const std::string& label)
{
    return getNewAddress(label, AddressBook::AddressBookPurpose::COLD_STAKING, CChainParams::Base58Type::STAKING_ADDRESS);
}

CallResult<CTxDestination> CWallet::getNewAddress(const std::string& addressLabel, const std::string purpose,
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
        return CallResult<CTxDestination>(std::string(
                        _("Keypool ran out, please call keypoolrefill first, or unlock the wallet.")));
    }
    CKeyID keyID = newKey.GetID();

    if (!SetAddressBook(keyID, addressLabel, purpose))
        throw std::runtime_error("CWallet::getNewAddress() : SetAddressBook failed");

    return CallResult<CTxDestination>(CTxDestination(keyID));
}

int64_t CWallet::GetKeyCreationTime(const CWDestination& dest)
{
    auto shieldDest = Standard::GetShieldedDestination(dest);
    auto transpDest = Standard::GetTransparentDestination(dest);
    return shieldDest ? GetKeyCreationTime(*shieldDest) : transpDest ? GetKeyCreationTime(*transpDest) : 0;
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

int64_t CWallet::GetKeyCreationTime(const libzcash::SaplingPaymentAddress& address)
{
    libzcash::SaplingIncomingViewingKey ivk;
    return GetSaplingIncomingViewingKey(address, ivk) ?
            GetSaplingScriptPubKeyMan()->GetKeyCreationTime(ivk) : 0;
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

    if (!IsCrypted()) {
        return WalletBatch(*database).WriteKey(
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
    {
        LOCK(cs_wallet);
        if (encrypted_batch)
            return encrypted_batch->WriteCryptedKey(vchPubKey,
                vchCryptedSecret,
                mapKeyMetadata[vchPubKey.GetID()]);
        else
            return WalletBatch(*database).WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
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

/**
 * Update wallet first key creation time. This should be called whenever keys
 * are added to the wallet, with the oldest key creation time.
 */
void CWallet::UpdateTimeFirstKey(int64_t nCreateTime)
{
    AssertLockHeld(cs_wallet);
    if (nCreateTime <= 1) {
        // Cannot determine birthday information, so set the wallet birthday to
        // the beginning of time.
        nTimeFirstKey = 1;
    } else if (!nTimeFirstKey || nCreateTime < nTimeFirstKey) {
        nTimeFirstKey = nCreateTime;
    }
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    return WalletBatch(*database).WriteCScript(Hash160(redeemScript), redeemScript);
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
    return WalletBatch(*database).WriteWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript& dest)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveWatchOnly(dest))
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (!WalletBatch(*database).EraseWatchOnly(dest))
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
            CKey key;
            if (!DecryptKey(vMasterKeyIn, vchCryptedSecret, vchPubKey, key)) {
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
                WalletBatch(*database).WriteMasterKey(pMasterKey.first, pMasterKey.second);
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

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    WalletBatch batch(*database);
    SetBestChainInternal(batch, loc);
}

void CWallet::SetBestChainInternal(WalletBatch& batch, const CBlockLocator& loc)
{
    if (!batch.TxnBegin()) {
        // This needs to be done atomically, so don't do it at all
        LogPrintf("%s: Couldn't start atomic write\n", __func__);
        return;
    }

    // Store the best block
    if (!batch.WriteBestBlock(loc)) {
        LogPrintf("SetBestChain(): Failed to write best block, aborting atomic write\n");
        batch.TxnAbort();
        return;
    }

    // For performance reasons, we update the witnesses data here and not when each transaction arrives
    for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
        auto wtx = wtxItem.second;
        // We skip transactions for which mapSaplingNoteData is empty.
        // This covers transactions that have no Sapling data
        // (i.e. are purely transparent), as well as shielding and unshielding
        // transactions in which we only have transparent addresses involved.
        if (!wtx.mapSaplingNoteData.empty()) {
            // Sanity check
            if (!wtx.tx->isSaplingVersion()) {
                LogPrintf("SetBestChain(): ERROR, Invalid tx version found with sapling data\n");
                batch.TxnAbort();
                uiInterface.ThreadSafeMessageBox(
                        _("A fatal internal error occurred, see debug.log for details"),
                        "Error", CClientUIInterface::MSG_ERROR);
                StartShutdown();
                return;
            }

            if (!batch.WriteTx(wtx)) {
                LogPrintf("SetBestChain(): Failed to write CWalletTx, aborting atomic write\n");
                batch.TxnAbort();
                return;
            }
        }
    }

    // Store sapling witness cache size
    if (m_sspk_man->nWitnessCacheNeedsUpdate) {
        if (!batch.WriteWitnessCacheSize(m_sspk_man->nWitnessCacheSize)) {
            LogPrintf("%s: Failed to write nWitnessCacheSize\n", __func__);
            batch.TxnAbort();
            return;
        }
    }

    if (!batch.TxnCommit()) {
        // Couldn't commit all to db, but in-memory state is fine
        LogPrintf("%s: Couldn't commit atomic write\n", __func__);
        return;
    }

    // Reset cache if the commit succeed and is needed.
    if (m_sspk_man->nWitnessCacheNeedsUpdate) {
        m_sspk_man->nWitnessCacheNeedsUpdate = false;
    }
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, WalletBatch* batch_in, bool fExplicit)
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

    {
        WalletBatch* batch = batch_in ? batch_in : new WalletBatch(*database);
        if (nWalletVersion > 40000)
            batch->WriteMinVersion(nWalletVersion);
        if (!batch_in)
            delete batch;
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

    for (const CTxIn& txin : wtx.tx->vin) {
        if (mapTxSpends.count(txin.prevout) <= 1 || wtx.tx->HasZerocoinSpendInputs())
            continue; // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator _it = range.first; _it != range.second; ++_it)
            result.insert(_it->second);
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
        CWalletTx* copyTo = &mapWallet.at(hash);
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

std::string CWallet::GetStrFromTxExtraData(const uint256& txHash, const std::string& key)
{
    auto tx = GetWalletTx(txHash);
    if (!tx) return "";
    auto it = tx->mapValue.find(key);
    return it != tx->mapValue.end() ? it->second : "";
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const COutPoint& outpoint) const
{
    AssertLockHeld(cs_wallet);
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

bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
    return IsSpent(COutPoint(hash, n));
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
    auto it = mapWallet.find(wtxid);
    assert(it != mapWallet.end());
    CWalletTx& thisTx = it->second;
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    for (const CTxIn& txin : thisTx.tx->vin)
        AddToSpends(txin.prevout, wtxid);

    if (CanSupportFeature(FEATURE_SAPLING) && thisTx.tx->sapData) {
        for (const SpendDescription &spend : thisTx.tx->sapData->vShieldedSpend) {
            GetSaplingScriptPubKeyMan()->AddToSaplingSpends(spend.nullifier, wtxid);
        }
    }
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
        assert(!encrypted_batch);
        encrypted_batch = new WalletBatch(*database);
        if (!encrypted_batch->TxnBegin()) {
            delete encrypted_batch;
            encrypted_batch = NULL;
            return false;
        }
        encrypted_batch->WriteMasterKey(nMasterKeyMaxID, kMasterKey);


        if (!EncryptKeys(vMasterKey) || (m_sspk_man->IsEnabled() && !m_sspk_man->EncryptSaplingKeys(vMasterKey))) {
            encrypted_batch->TxnAbort();
            delete encrypted_batch;
            // We now probably have half of our keys encrypted in memory, and half not...
            // die and let the user reload their unencrypted wallet.
            assert(false);
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, encrypted_batch, true);

        if (!encrypted_batch->TxnCommit()) {
            delete encrypted_batch;
            // We now have keys encrypted in memory, but not on disk...
            // die to avoid confusion and let the user reload their unencrypted wallet.
            assert(false);
        }

        delete encrypted_batch;
        encrypted_batch = NULL;

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
        database->Rewrite();

        // BDB seems to have a bad habit of writing old data into
        // slack space in .dat files; that is bad if the old data is
        // unencrypted private keys. So:
        database->ReloadDbEnv();

    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(WalletBatch* batch)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (batch) {
        batch->WriteOrderPosNext(nOrderPosNext);
    } else {
        WalletBatch(*database).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

bool CWallet::IsKeyUsed(const CPubKey& vchPubKey) const
{
    if (vchPubKey.IsValid()) {
        const CScript& scriptPubKey = GetScriptForDestination(vchPubKey.GetID());
        for (const auto& entry : mapWallet) {
            const CWalletTx& wtx = entry.second;
            for (const CTxOut& txout : wtx.tx->vout)
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
    WalletBatch batch(*database, "r+", fFlushOnClose);
    const uint256& hash = wtxIn.GetHash();

    // Inserts only if not already there, returns tx inserted or tx found
    std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.emplace(hash, wtxIn);
    CWalletTx& wtx = (*ret.first).second;
    wtx.BindWallet(this);
    // Sapling
    m_sspk_man->UpdateNullifierNoteMapWithTx(wtx);
    bool fInsertedNew = ret.second;
    if (fInsertedNew) {
        wtx.nTimeReceived = GetAdjustedTime();
        wtx.nOrderPos = IncOrderPosNext(&batch);
        wtxOrdered.emplace(wtx.nOrderPos, &wtx);
        wtx.UpdateTimeSmart();
        AddToSpends(hash);
    }

    bool fUpdated = false;
    if (!fInsertedNew) {
        if (wtxIn.m_confirm.status != wtx.m_confirm.status) {
            wtx.m_confirm.status = wtxIn.m_confirm.status;
            wtx.m_confirm.nIndex = wtxIn.m_confirm.nIndex;
            wtx.m_confirm.hashBlock = wtxIn.m_confirm.hashBlock;
            wtx.m_confirm.block_height = wtxIn.m_confirm.block_height;
            wtx.UpdateTimeSmart();
            fUpdated = true;
        } else {
            assert(wtx.m_confirm.nIndex == wtxIn.m_confirm.nIndex);
            assert(wtx.m_confirm.hashBlock == wtxIn.m_confirm.hashBlock);
            assert(wtx.m_confirm.block_height == wtxIn.m_confirm.block_height);
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
        if (!batch.WriteTx(wtx))
            return false;
    }

    // Break debit/credit balance caches:
    wtx.MarkDirty();

    // Notify UI of new or updated transaction
    NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

    // notify an external script when a wallet transaction comes in or is updated
    std::string strCmd = gArgs.GetArg("-walletnotify", "");

    if (!strCmd.empty()) {
        boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
        std::thread t(runCommand, strCmd);
        t.detach(); // thread runs free
    }
    return true;
}

// Internal function for now, this will be part of a chain interface class in the future.
static Optional<int> getTipBlockHeight(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    CBlockIndex* pindex = LookupBlockIndex(hash);
    if (pindex && chainActive.Contains(pindex)) {
        return Optional<int>(pindex->nHeight);
    }
    return nullopt;
}

bool CWallet::LoadToWallet(CWalletTx& wtxIn)
{
    LOCK2(cs_main, cs_wallet);
    // If tx hasn't been reorged out of chain while wallet being shutdown
    // change tx status to UNCONFIRMED and reset hashBlock/nIndex.
    if (!wtxIn.m_confirm.hashBlock.IsNull()) {
        Optional<int> block_height = getTipBlockHeight(wtxIn.m_confirm.hashBlock);
        if (block_height) {
            // Update cached block height variable since it not stored in the
            // serialized transaction.
            wtxIn.m_confirm.block_height = *block_height;
        } else if (wtxIn.isConflicted() || wtxIn.isConfirmed()) {
            // If tx block (or conflicting block) was reorged out of chain
            // while the wallet was shutdown, change tx status to UNCONFIRMED
            // and reset block height, hash, and index. ABANDONED tx don't have
            // associated blocks and don't need to be updated. The case where a
            // transaction was reorged out while online and then reconfirmed
            // while offline is covered by the rescan logic.
            wtxIn.setUnconfirmed();
            wtxIn.m_confirm.hashBlock = UINT256_ZERO;
            wtxIn.m_confirm.block_height = 0;
            wtxIn.m_confirm.nIndex = 0;
        }
    }
    const uint256& hash = wtxIn.GetHash();
    CWalletTx& wtx = mapWallet.emplace(hash, wtxIn).first->second;
    wtx.BindWallet(this);
    // Sapling
    m_sspk_man->UpdateNullifierNoteMapWithTx(wtx);
    wtxOrdered.emplace(wtx.nOrderPos, &wtx);
    AddToSpends(hash);
    for (const CTxIn& txin : wtx.tx->vin) {
        auto it = mapWallet.find(txin.prevout.hash);
        if (it != mapWallet.end()) {
            CWalletTx& prevtx = it->second;
            if (prevtx.isConflicted()) {
                MarkConflicted(prevtx.m_confirm.hashBlock, prevtx.m_confirm.block_height, wtx.GetHash());
            }
        }
    }
    return true;
}

bool CWallet::FindNotesDataAndAddMissingIVKToKeystore(const CTransaction& tx, Optional<mapSaplingNoteData_t>& saplingNoteData)
{
    auto saplingNoteDataAndAddressesToAdd = m_sspk_man->FindMySaplingNotes(tx);
    saplingNoteData = saplingNoteDataAndAddressesToAdd.first;
    auto addressesToAdd = saplingNoteDataAndAddressesToAdd.second;
    // Add my addresses
    for (const auto& addressToAdd : addressesToAdd) {
        if (!m_sspk_man->AddSaplingIncomingViewingKey(addressToAdd.second, addressToAdd.first)) {
            return false;
        }
    }
    return true;
}

void CWallet::AddExternalNotesDataToTx(CWalletTx& wtx) const
{
    if (HasSaplingSPKM() && wtx.tx->IsShieldedTx()) {
        const uint256& txId = wtx.GetHash();
        // Add the external outputs.
        SaplingOutPoint op {txId, 0};
        for (unsigned int i = 0; i < wtx.tx->sapData->vShieldedOutput.size(); i++) {
            op.n = i;
            if (wtx.mapSaplingNoteData.count(op)) continue;     // internal output
            auto recovered = GetSaplingScriptPubKeyMan()->TryToRecoverNote(wtx, op);
            if (recovered) {
                // Always true for 'IsFromMe' transactions
                wtx.mapSaplingNoteData[op].address = recovered->second;
                wtx.mapSaplingNoteData[op].amount = recovered->first.value();
                const auto& memo = recovered->first.memo();
                // don't save empty memo (starting with 0xF6)
                if (memo[0] < 0xF6) {
                    wtx.mapSaplingNoteData[op].memo = memo;
                }
            }
        }
    }
}

/**
 * Add a transaction to the wallet, or update it. pIndex and posInBlock should
 * be set when the transaction was known to be included in a block.  When
 * pIndex == NULL, then wallet state is not updated in AddToWallet, but
 * notifications happen and cached balances are marked dirty.
 *
 * If fUpdate is true, existing transactions will be updated.
 * TODO: One exception to this is that the abandoned state is cleared under the
 * assumption that any further notification of a transaction that was considered
 * abandoned is an indication that it is not safe to be considered abandoned.
 * Abandoned state should probably be more carefully tracked via different
 * posInBlock signals or by checking mempool presence when necessary.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransactionRef& ptx, const CWalletTx::Confirmation& confirm, bool fUpdate)
{
    const CTransaction& tx = *ptx;
    {
        AssertLockHeld(cs_wallet);

        if (!confirm.hashBlock.IsNull() && !tx.HasZerocoinSpendInputs() && !tx.IsCoinBase()) {
            for (const CTxIn& txin : tx.vin) {
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range = mapTxSpends.equal_range(txin.prevout);
                while (range.first != range.second) {
                    if (range.first->second != tx.GetHash()) {
                        LogPrintf("Transaction %s (in block %s) conflicts with wallet transaction %s (both spend %s:%i)\n", tx.GetHash().ToString(), confirm.hashBlock.ToString(), range.first->second.ToString(), range.first->first.hash.ToString(), range.first->first.n);
                        MarkConflicted(confirm.hashBlock, confirm.block_height, range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;

        // Check tx for Sapling notes
        Optional<mapSaplingNoteData_t> saplingNoteData {nullopt};
        if (HasSaplingSPKM()) {
            if (!FindNotesDataAndAddMissingIVKToKeystore(tx, saplingNoteData)) {
                return false; // error adding incoming viewing key.
            }
        }

        // If this is a ProRegTx and the wallet owns the collateral, lock the corresponding coin
        LockIfMyCollateral(ptx);

        bool isFromMe = IsFromMe(ptx);
        if (fExisted || IsMine(ptx) || isFromMe || (saplingNoteData && !saplingNoteData->empty())) {

            /* Check if any keys in the wallet keypool that were supposed to be unused
             * have appeared in a new transaction. If so, remove those keys from the keypool.
             * This can happen when restoring an old wallet backup that does not contain
             * the mostly recently created transactions from newer versions of the wallet.
             */

            // loop though all outputs
            for (const CTxOut& txout: tx.vout) {
                m_spk_man->MarkUnusedAddresses(txout.scriptPubKey);
            }

            CWalletTx wtx(this, MakeTransactionRef(tx));
            if (wtx.tx->IsShieldedTx()) {
                if (saplingNoteData && !saplingNoteData->empty()) {
                    wtx.SetSaplingNoteData(*saplingNoteData);
                }

                // Add external notes info if we are sending
                if (isFromMe) AddExternalNotesDataToTx(wtx);
            }

            // Block disconnection override an abandoned tx as unconfirmed
            // which means user may have to call abandontransaction again
            wtx.m_confirm = confirm;

            return AddToWallet(wtx, false);
        }
    }
    return false;
}

bool CWallet::AbandonTransaction(const uint256& hashTx)
{
    LOCK(cs_wallet);

    WalletBatch batch(*database, "r+");

    std::set<uint256> todo;
    std::set<uint256> done;

    // Can't mark abandoned if confirmed or in mempool
    auto it = mapWallet.find(hashTx);
    assert(it != mapWallet.end());
    CWalletTx& origtx = it->second;
    if (origtx.GetDepthInMainChain() != 0 || origtx.InMempool()) {
        return false;
    }

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it2 = mapWallet.find(now);
        assert(it2 != mapWallet.end());
        CWalletTx& wtx = it2->second;
        int currentconfirm = wtx.GetDepthInMainChain();
        // If the orig tx was not in block, none of its spends can be
        assert(currentconfirm <= 0);
        // if (currentconfirm < 0) {Tx and spends are already conflicted, no need to abandon}
        if (currentconfirm == 0 && !wtx.isAbandoned()) {
            // If the orig tx was not in block/mempool, none of its spends can be in mempool
            assert(!wtx.InMempool());
            wtx.setAbandoned();
            wtx.MarkDirty();
            batch.WriteTx(wtx);
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
            for (const CTxIn& txin : wtx.tx->vin) {
                auto _it = mapWallet.find(txin.prevout.hash);
                if (_it != mapWallet.end()) {
                    _it->second.MarkDirty();
                }
            }
        }
    }

    return true;
}

void CWallet::MarkConflicted(const uint256& hashBlock, int conflicting_height, const uint256& hashTx)
{
    LOCK(cs_wallet);

    int conflictconfirms = (m_last_block_processed_height - conflicting_height + 1) * -1;
    // If number of conflict confirms cannot be determined, this means
    // that the block is still unknown or not yet part of the main chain,
    // for example when loading the wallet during a reindex. Do nothing in that
    // case.
    if (conflictconfirms >= 0)
        return;

    // Do not flush the wallet here for performance reasons
    WalletBatch batch(*database, "r+", false);

    std::set<uint256> todo;
    std::set<uint256> done;

    todo.insert(hashTx);

    while (!todo.empty()) {
        uint256 now = *todo.begin();
        todo.erase(now);
        done.insert(now);
        auto it = mapWallet.find(now);
        assert(it != mapWallet.end());
        CWalletTx& wtx = it->second;
        int currentconfirm = wtx.GetDepthInMainChain();
        if (conflictconfirms < currentconfirm) {
            // Block is 'more conflicted' than current confirm; update.
            // Mark transaction as conflicted with this block.
            wtx.m_confirm.nIndex = 0;
            wtx.m_confirm.hashBlock = hashBlock;
            wtx.m_confirm.block_height = conflicting_height;
            wtx.setConflicted();
            wtx.MarkDirty();
            batch.WriteTx(wtx);
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
            for (const CTxIn& txin : wtx.tx->vin) {
                auto _it = mapWallet.find(txin.prevout.hash);
                if (_it != mapWallet.end()) {
                    _it->second.MarkDirty();
                }
            }
        }
    }
}

void CWallet::SyncTransaction(const CTransactionRef& ptx, const CWalletTx::Confirmation& confirm)
{
    if (!AddToWalletIfInvolvingMe(ptx, confirm, true)) {
        return; // Not one of ours
    }

    MarkAffectedTransactionsDirty(*ptx);
}

void CWallet::TransactionAddedToMempool(const CTransactionRef& ptx)
{
    LOCK(cs_wallet);
    CWalletTx::Confirmation confirm(CWalletTx::Status::UNCONFIRMED, /* block_height */ 0, {}, /* nIndex */ 0);
    SyncTransaction(ptx, confirm);

    auto it = mapWallet.find(ptx->GetHash());
    if (it != mapWallet.end()) {
        it->second.fInMempool = true;
    }
}

void CWallet::TransactionRemovedFromMempool(const CTransactionRef &ptx, MemPoolRemovalReason reason) {
    LOCK(cs_wallet);
    auto it = mapWallet.find(ptx->GetHash());
    if (it != mapWallet.end()) {
        it->second.fInMempool = false;
    }
    // Handle transactions that were removed from the mempool because they
    // conflict with transactions in a newly connected block.
    if (reason == MemPoolRemovalReason::CONFLICT) {
        // Call SyncNotifications, so external -walletnotify notifications will
        // be triggered for these transactions. Set Status::UNCONFIRMED instead
        // of Status::CONFLICTED for a few reasons:
        //
        // 1. The transactionRemovedFromMempool callback does not currently
        //    provide the conflicting block's hash and height, and for backwards
        //    compatibility reasons it may not be not safe to store conflicted
        //    wallet transactions with a null block hash. See
        //    https://github.com/bitcoin/bitcoin/pull/18600#discussion_r420195993.
        // 2. For most of these transactions, the wallet's internal conflict
        //    detection in the blockConnected handler will subsequently call
        //    MarkConflicted and update them with CONFLICTED status anyway. This
        //    applies to any wallet transaction that has inputs spent in the
        //    block, or that has ancestors in the wallet with inputs spent by
        //    the block.
        // 3. Longstanding behavior since the sync implementation in
        //    https://github.com/bitcoin/bitcoin/pull/9371 and the prior sync
        //    implementation before that was to mark these transactions
        //    unconfirmed rather than conflicted.
        //
        // Nothing described above should be seen as an unchangeable requirement
        // when improving this code in the future. The wallet's heuristics for
        // distinguishing between conflicted and unconfirmed transactions are
        // imperfect, and could be improved in general, see
        // https://github.com/bitcoin-core/bitcoin-devwiki/wiki/Wallet-Transaction-Conflict-Tracking
        SyncTransaction(ptx, {CWalletTx::Status::UNCONFIRMED, /* block height  */ 0, /* block hash */ {}, /* index */ 0});
    }
}

void CWallet::BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex)
{
    {
        LOCK(cs_wallet);

        m_last_block_processed = pindex->GetBlockHash();
        m_last_block_processed_time = pindex->GetBlockTime();
        m_last_block_processed_height = pindex->nHeight;
        for (size_t index = 0; index < pblock->vtx.size(); index++) {
            CWalletTx::Confirmation confirm(CWalletTx::Status::CONFIRMED, m_last_block_processed_height,
                                            m_last_block_processed, index);
            SyncTransaction(pblock->vtx[index], confirm);
            TransactionRemovedFromMempool(pblock->vtx[index], MemPoolRemovalReason::BLOCK);
        }

        // Sapling: notify about the connected block
        // Get prev block tree anchor
        CBlockIndex* pprev = pindex->pprev;
        SaplingMerkleTree oldSaplingTree;
        bool isSaplingActive = (pprev) != nullptr &&
                               Params().GetConsensus().NetworkUpgradeActive(pprev->nHeight,
                                                                            Consensus::UPGRADE_V5_0);
        if (isSaplingActive) {
            assert(pcoinsTip->GetSaplingAnchorAt(pprev->hashFinalSaplingRoot, oldSaplingTree));
        } else {
            assert(pcoinsTip->GetSaplingAnchorAt(SaplingMerkleTree::empty_root(), oldSaplingTree));
        }

        // Sapling: Update cached incremental witnesses
        ChainTipAdded(pindex, pblock.get(), oldSaplingTree);
    } // cs_wallet lock end

    // Auto-combine functionality
    // If turned on Auto Combine will scan wallet for dust to combine
    // Outside of the cs_wallet lock because requires cs_main for now
    // due CreateTransaction/CommitTransaction dependency.
    if (fCombineDust) {
        AutoCombineDust(g_connman.get());
    }
}

void CWallet::BlockDisconnected(const std::shared_ptr<const CBlock>& pblock, const uint256& blockHash, int nBlockHeight, int64_t blockTime)
{
    LOCK(cs_wallet);

    // At block disconnection, this will change an abandoned transaction to
    // be unconfirmed, whether or not the transaction is added back to the mempool.
    // User may have to call abandontransaction again. It may be addressed in the
    // future with a stickier abandoned state or even removing abandontransaction call.
    m_last_block_processed_height = nBlockHeight - 1;
    m_last_block_processed_time = blockTime;
    m_last_block_processed = blockHash;
    for (const CTransactionRef& ptx : pblock->vtx) {
        CWalletTx::Confirmation confirm(CWalletTx::Status::UNCONFIRMED, /* block_height */ 0, {}, /* nIndex */ 0);
        SyncTransaction(ptx, confirm);
    }

    if (Params().GetConsensus().NetworkUpgradeActive(nBlockHeight, Consensus::UPGRADE_V5_0)) {
        // Update Sapling cached incremental witnesses
        m_sspk_man->DecrementNoteWitnesses(nBlockHeight);
        m_sspk_man->UpdateSaplingNullifierNoteMapForBlock(pblock.get());
    }
}

void CWallet::BlockUntilSyncedToCurrentChain() {
    AssertLockNotHeld(cs_main);
    AssertLockNotHeld(cs_wallet);

    {
        // Skip the queue-draining stuff if we know we're caught up with
        // chainActive.Tip()...
        // We could also take cs_wallet here, and call m_last_block_processed
        // protected by cs_wallet instead of cs_main, but as long as we need
        // cs_main here anyway, its easier to just call it cs_main-protected.
        uint256 last_block_hash = WITH_LOCK(cs_wallet, return m_last_block_processed);
        LOCK(cs_main);
        const CBlockIndex* initialChainTip = chainActive.Tip();
        if (!last_block_hash.IsNull() && initialChainTip &&
            last_block_hash == initialChainTip->GetBlockHash()) {
                return;
        }
    }

    // ...otherwise put a callback in the validation interface queue and wait
    // for the queue to drain enough to execute it (indicating we are caught up
    // at least with the time we entered this function).
    SyncWithValidationInterfaceQueue();
}

void CWallet::MarkAffectedTransactionsDirty(const CTransaction& tx)
{
    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    for (const CTxIn& txin : tx.vin) {
        if (!txin.IsZerocoinSpend()) {
            auto it = mapWallet.find(txin.prevout.hash);
            if (it != mapWallet.end()) {
                it->second.MarkDirty();
            }
        }
    }

    // Sapling
    if (HasSaplingSPKM() && tx.IsShieldedTx()) {
        for (const SpendDescription &spend : tx.sapData->vShieldedSpend) {
            const uint256& nullifier = spend.nullifier;
            auto nit = m_sspk_man->mapSaplingNullifiersToNotes.find(nullifier);
            if (nit != m_sspk_man->mapSaplingNullifiersToNotes.end()) {
                auto it = mapWallet.find(nit->second.hash);
                if (it != mapWallet.end()) {
                    it->second.MarkDirty();
                }
            }
        }
    }
}

void CWallet::EraseFromWallet(const uint256& hash)
{
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            WalletBatch(*database).EraseTx(hash);
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
            if (txin.prevout.n < prev.tx->vout.size())
                return IsMine(prev.tx->vout[txin.prevout.n]);
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
        for (const CTxOut& txout : wtx.tx->vout) {
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
            if (txin.prevout.n < prev.tx->vout.size())
                if (IsMine(prev.tx->vout[txin.prevout.n]) & filter)
                    return prev.tx->vout[txin.prevout.n].nValue;
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

        return IsChange(address);
    }
    return false;
}

bool CWallet::IsChange(const CTxDestination& address) const
{
    // Read the current assumptions in IsChange(const CTxOut&)
    // this can definitely be different in the short future.
    return WITH_LOCK(cs_wallet, return !HasAddressBook(address));
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
    if (!m_confirm.hashBlock.IsNull()) {
        if (mapBlockIndex.count(m_confirm.hashBlock)) {
            nTimeSmart = mapBlockIndex.at(m_confirm.hashBlock)->GetBlockTime();
        } else
            LogPrintf("%s : found %s in block %s not in index\n", __func__, GetHash().ToString(), m_confirm.hashBlock.ToString());
    }
}

CAmount CWalletTx::GetCachableAmount(AmountType type, const isminefilter& filter, bool recalculate) const
{
    auto& amount = m_amounts[type];
    if (recalculate || !amount.m_cached[filter]) {
        amount.Set(filter, type == DEBIT ? pwallet->GetDebit(tx, filter) : pwallet->GetCredit(*this, filter));
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
    if (tx->vin.empty() && (tx->sapData && tx->sapData->vShieldedSpend.empty())) {
        return 0;
    }

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
    if (filter & ISMINE_SPENDABLE_SHIELDED) {
        debit += GetCachableAmount(DEBIT, ISMINE_SPENDABLE_SHIELDED);
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
    if (filter & ISMINE_SPENDABLE_SHIELDED) {
        credit += GetCachableAmount(CREDIT, ISMINE_SPENDABLE_SHIELDED, recalculate);
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache, const isminefilter& filter) const
{
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
    bool allow_cache = filter == ISMINE_SPENDABLE || filter == ISMINE_WATCH_ONLY ||
            filter == ISMINE_SPENDABLE_SHIELDED || filter == ISMINE_WATCH_ONLY_SHIELDED;

    // Must wait until coinbase/coinstake is safely deep enough in the chain before valuing it
    if (GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && allow_cache && m_amounts[AVAILABLE_CREDIT].m_cached[filter]) {
        return m_amounts[AVAILABLE_CREDIT].m_value[filter];
    }

    CAmount nCredit = 0;
    // If the filter is only for shielded amounts, do not calculate the regular outputs
    if (filter != ISMINE_SPENDABLE_SHIELDED && filter != ISMINE_WATCH_ONLY_SHIELDED) {

        const uint256& hashTx = GetHash();
        for (unsigned int i = 0; i < tx->vout.size(); i++) {
            if (!pwallet->IsSpent(hashTx, i)) {
                const CTxOut &txout = tx->vout[i];
                nCredit += pwallet->GetCredit(txout, filter);
                if (!Params().GetConsensus().MoneyRange(nCredit))
                    throw std::runtime_error(std::string(__func__) + " : value out of range");
            }
        }

    }

    if (pwallet->HasSaplingSPKM()) {
        // Can calculate the shielded available balance.
        if (filter & ISMINE_SPENDABLE_SHIELDED || filter & ISMINE_WATCH_ONLY_SHIELDED) {
            nCredit += pwallet->GetSaplingScriptPubKeyMan()->GetCredit(*this, filter, true);
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
    const CAmount collAmt = Params().GetConsensus().nMNCollateralAmt;
    for (unsigned int i = 0; i < tx->vout.size(); i++) {
        const CTxOut& txout = tx->vout[i];

        // Skip spent coins
        if (pwallet->IsSpent(hashTx, i)) continue;

        // Add locked coins
        if (pwallet->IsLockedCoin(hashTx, i)) {
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE_ALL);
        }

        // Add masternode collaterals which are handled like locked coins
        else if (fMasterNode && tx->vout[i].nValue == collAmt) {
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
        }

        if (!Params().GetConsensus().MoneyRange(nCredit))
            throw std::runtime_error("CWalletTx::GetLockedCredit() : value out of range");
    }

    return nCredit;
}

CAmount CWalletTx::GetImmatureWatchOnlyCredit(const bool fUseCache) const
{
    if (IsInMainChainImmature()) {
        return GetCachableAmount(IMMATURE_CREDIT, ISMINE_WATCH_ONLY, !fUseCache);
    }

    return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(const bool fUseCache) const
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
        CAmount nValueOut = tx->GetValueOut(); // transasparent outputs plus the negative Sapling valueBalance
        CAmount nValueIn = tx->GetShieldedValueIn();
        nFee = nDebit - nValueOut + nValueIn;

        // If we sent utxos from this transaction, create output for value taken from (negative valueBalance)
        // or added (positive valueBalance) to the transparent value pool by Sapling shielding and unshielding.
        if (tx->sapData) {
            if (tx->sapData->valueBalance < 0) {
                COutputEntry output = {CNoDestination(), -tx->sapData->valueBalance, (int) tx->vout.size()};
                listSent.push_back(output);
            } else if (tx->sapData->valueBalance > 0) {
                COutputEntry output = {CNoDestination(), tx->sapData->valueBalance, (int) tx->vout.size()};
                listReceived.push_back(output);
            }
        }
    }

    // Sent/received.
    bool hasZerocoinSpends = tx->HasZerocoinSpendInputs();
    for (unsigned int i = 0; i < tx->vout.size(); ++i) {
        const CTxOut& txout = tx->vout[i];
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

bool CWallet::Upgrade(std::string& error, const int prevVersion)
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
 * Scan active chain for relevant transactions after importing keys. This should
 * be called whenever new keys are added to the wallet, with the oldest key
 * creation time.
 *
 * @return Earliest timestamp that could be successfully scanned from. Timestamp
 * returned will be higher than startTime if relevant blocks could not be read.
 */
int64_t CWallet::RescanFromTime(int64_t startTime, const WalletRescanReserver& reserver, bool update)
{
    // Find starting block. May be null if nCreateTime is greater than the
    // highest blockchain timestamp, in which case there is nothing that needs
    // to be scanned.
    CBlockIndex* startBlock = nullptr;
    {
        LOCK(cs_main);
        startBlock = chainActive.FindEarliestAtLeast(startTime - TIMESTAMP_WINDOW);
        LogPrintf("%s: Rescanning last %i blocks\n", __func__, startBlock ? chainActive.Height() - startBlock->nHeight + 1 : 0);
    }

    if (startBlock) {
        const CBlockIndex* const failedBlock = ScanForWalletTransactions(startBlock, nullptr, reserver, update);
        if (failedBlock) {
            return failedBlock->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1;
        }
    }
    return startTime;
}

/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 *
 * Returns null if scan was successful. Otherwise, if a complete rescan was not
 * possible (due to pruning or corruption), returns pointer to the most recent
 * block that could not be scanned.
 *
 * If pindexStop is not a nullptr, the scan will stop at the block-index
 * defined by pindexStop
 *
 * Caller needs to make sure pindexStop (and the optional pindexStart) are on
 * the main chain after to the addition of any new keys you want to detect
 * transactions for.
 */
CBlockIndex* CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, CBlockIndex* pindexStop, const WalletRescanReserver& reserver, bool fUpdate, bool fromStartup)
{
    int64_t nNow = GetTime();

    assert(reserver.isReserved());
    if (pindexStop) {
        assert(pindexStop->nHeight >= pindexStart->nHeight);
    }

    CBlockIndex* pindex = pindexStart;
    CBlockIndex* ret = nullptr;
    {
        ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        CBlockIndex* tip = nullptr;
        double dProgressStart;
        double dProgressTip;
        {
            LOCK(cs_main);
            tip = chainActive.Tip();
            dProgressStart = Checkpoints::GuessVerificationProgress(pindex, false);
            dProgressTip = Checkpoints::GuessVerificationProgress(tip, false);
        }

        std::vector<uint256> myTxHashes;
        while (pindex && !fAbortRescan) {
            double gvp = 0;
            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0) {
                gvp = WITH_LOCK(cs_main, return Checkpoints::GuessVerificationProgress(pindex, false); );
                ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((gvp - dProgressStart) / (dProgressTip - dProgressStart) * 100))));
            }
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, gvp);
            }
            if (fromStartup && ShutdownRequested()) {
                break;
            }

            CBlock block;
            if (ReadBlockFromDisk(block, pindex)) {
                LOCK2(cs_main, cs_wallet);
                if (pindex && !chainActive.Contains(pindex)) {
                     // Abort scan if current block is no longer active, to prevent
                     // marking transactions as coming from the wrong block.
                     ret = pindex;
                     break;
                 }
                for (int posInBlock = 0; posInBlock < (int) block.vtx.size(); posInBlock++) {
                    const auto& tx = block.vtx[posInBlock];
                    CWalletTx::Confirmation confirm(CWalletTx::Status::CONFIRMED, pindex->nHeight, pindex->GetBlockHash(), posInBlock);
                    if (AddToWalletIfInvolvingMe(tx, confirm, fUpdate)) {
                        myTxHashes.push_back(tx->GetHash());
                    }
                }

                // Sapling
                // This should never fail: we should always be able to get the tree
                // state on the path to the tip of our chain
                if (pindex->pprev) {
                    if (Params().GetConsensus().NetworkUpgradeActive(pindex->pprev->nHeight, Consensus::UPGRADE_V5_0)) {
                        SaplingMerkleTree saplingTree;
                        assert(pcoinsTip->GetSaplingAnchorAt(pindex->pprev->hashFinalSaplingRoot, saplingTree));
                        // Increment note witness caches
                        ChainTipAdded(pindex, &block, saplingTree);
                    }
                }
            } else {
                ret = pindex;
            }
            if (pindex == pindexStop) {
                break;
            }
            {
                LOCK(cs_main);
                pindex = chainActive.Next(pindex);
                if (tip != chainActive.Tip()) {
                    tip = chainActive.Tip();
                    // in case the tip has changed, update progress max
                    dProgressTip = Checkpoints::GuessVerificationProgress(tip, false);
                }
            }
        }

        // Sapling
        // After rescanning, persist Sapling note data that might have changed, e.g. nullifiers.
        // Do not flush the wallet here for performance reasons.
        WalletBatch batch(*database, "r+", false);
        for (const auto& hash : myTxHashes) {
            CWalletTx& wtx = mapWallet.at(hash);
            if (!wtx.mapSaplingNoteData.empty()) {
                if (!batch.WriteTx(wtx)) {
                    LogPrintf("Rescanning... WriteToDisk failed to update Sapling note data for: %s\n", hash.ToString());
                }
            }
        }

        if (pindex && fAbortRescan) {
            LogPrintf("Rescan aborted at block %d. Progress=%f\n", pindex->nHeight, Checkpoints::GuessVerificationProgress(pindex, false));
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
    for (std::pair<const int64_t, CWalletTx*>& item: mapSorted) {
        CWalletTx& wtx = *(item.second);

        CValidationState state;
        bool fSuccess = wtx.AcceptToMemoryPool(state);
        if (!fSuccess && fFirstLoad && GetTime() - wtx.GetTxTime() > 12*60*60) {
            //First load of wallet, failed to accept to mempool, and older than 12 hours... not likely to ever
            //make it in to mempool
            AbandonTransaction(wtx.GetHash());
        }
    }
}

bool CWalletTx::InMempool() const
{
    return fInMempool;
}

void CWalletTx::RelayWalletTransaction(CConnman* connman)
{
    if (!connman || IsCoinBase() || IsCoinStake()) {
        // Nothing to do. Return early
        return;
    }
    if (GetDepthInMainChain() == 0 && !isAbandoned()) {
        const uint256& hash = GetHash();
        LogPrintf("Relaying wtx %s\n", hash.ToString());
        CInv inv(MSG_TX, hash);
        connman->ForEachNode([&inv](CNode* pnode) {
          pnode->PushInventory(inv);
        });
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

void CWallet::Flush(bool shutdown)
{
    database->Flush(shutdown);
}

void CWallet::ResendWalletTransactions(CConnman* connman)
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (GetTime() < nNextResend) {
        return;
    }
    bool fFirst = (nNextResend == 0);
    nNextResend = GetTime() + GetRand(30 * 60);
    if (fFirst) {
        return;
    }

    // Only do it if there's been a new block since last time
    if (nTimeBestReceived < nLastResend) {
        return;
    }
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
            if (nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60) {
                mapSorted.emplace(wtx.nTimeReceived, &wtx);
            }
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

CWallet::Balance CWallet::GetBalance(const int min_depth) const
{
    Balance ret;
    {
        LOCK(cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& entry : mapWallet) {
            const CWalletTx& wtx = entry.second;
            const bool is_trusted{wtx.IsTrusted()};
            const int tx_depth{wtx.GetDepthInMainChain()};
            const CAmount tx_credit_mine{wtx.GetAvailableCredit(/* fUseCache */ true, ISMINE_SPENDABLE_TRANSPARENT)};
            const CAmount tx_credit_shield_mine{wtx.GetAvailableCredit(/* fUseCache */ true, ISMINE_SPENDABLE_SHIELDED)};
            if (is_trusted && tx_depth >= min_depth) {
                ret.m_mine_trusted += tx_credit_mine;
                ret.m_mine_trusted_shield += tx_credit_shield_mine;
                if (wtx.tx->HasP2CSOutputs()) {
                    ret.m_mine_cs_delegated_trusted += wtx.GetStakeDelegationCredit();
                }
            }
            if (!is_trusted && tx_depth == 0 && wtx.InMempool()) {
                ret.m_mine_untrusted_pending += tx_credit_mine;
                ret.m_mine_untrusted_shielded_balance += tx_credit_shield_mine;
            }
            ret.m_mine_immature += wtx.GetImmatureCredit();
        }
    }
    return ret;
}

CAmount CWallet::loopTxsBalance(const std::function<void(const uint256&, const CWalletTx&, CAmount&)>& method) const
{
    CAmount nTotal = 0;
    {
        LOCK(cs_wallet);
        for (const auto& it : mapWallet) {
            method(it.first, it.second, nTotal);
        }
    }
    return nTotal;
}

CAmount CWallet::GetAvailableBalance(bool fIncludeDelegated, bool fIncludeShielded) const
{
    isminefilter filter;
    if (fIncludeDelegated && fIncludeShielded) {
        filter = ISMINE_SPENDABLE_ALL;
    } else if (fIncludeDelegated) {
        filter = ISMINE_SPENDABLE_TRANSPARENT;
    } else if (fIncludeShielded) {
        filter = ISMINE_SPENDABLE_NO_DELEGATED;
    } else {
        filter = ISMINE_SPENDABLE;
    }
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
        if (pcoin.tx->HasP2CSOutputs() && pcoin.IsTrusted())
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
            if (pcoin.tx->HasP2CSOutputs() && pcoin.IsTrusted())
                nTotal += pcoin.GetStakeDelegationCredit();
    });
}

CAmount CWallet::GetLockedCoins() const
{
    LOCK(cs_wallet);
    if (setLockedCoins.empty()) return 0;

    CAmount ret = 0;
    for (const auto& coin : setLockedCoins) {
        auto it = mapWallet.find(coin.hash);
        if (it != mapWallet.end()) {
            const CWalletTx& pcoin = it->second;
            if (pcoin.IsTrusted() && pcoin.GetDepthInMainChain() > 0) {
                ret += it->second.tx->vout.at(coin.n).nValue;
            }
        }
    }
    return ret;
}

CAmount CWallet::GetUnconfirmedBalance(isminetype filter) const
{
    return loopTxsBalance([filter](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (!pcoin.IsTrusted() && pcoin.GetDepthInMainChain() == 0 && pcoin.InMempool())
                nTotal += pcoin.GetCredit(filter);
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
    LOCK(cs_wallet);

    CAmount balance = 0;
    for (const auto& entry : mapWallet) {
        const CWalletTx& wtx = entry.second;
        bool fConflicted;
        const int depth = wtx.GetDepthAndMempool(fConflicted);
        if (!IsFinalTx(wtx.tx, m_last_block_processed_height) || wtx.GetBlocksToMaturity() > 0 || depth < 0 || fConflicted) {
            continue;
        }

        // Loop through tx outputs and add incoming payments. For outgoing txs,
        // treat change outputs specially, as part of the amount debited.
        CAmount debit = wtx.GetDebit(filter);
        const bool outgoing = debit > 0;
        for (const CTxOut& out : wtx.tx->vout) {
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

// Sapling
CAmount CWallet::GetAvailableShieldedBalance(bool fUseCache) const
{
    isminefilter filter = ISMINE_SPENDABLE_SHIELDED;
    return GetAvailableBalance(filter, fUseCache);
};

CAmount CWallet::GetUnconfirmedShieldedBalance() const
{
    return GetUnconfirmedBalance(ISMINE_SPENDABLE_SHIELDED);
};

void CWallet::GetAvailableP2CSCoins(std::vector<COutput>& vCoins) const {
    vCoins.clear();
    {
        LOCK(cs_wallet);
        for (const auto& it : mapWallet) {
            const uint256& wtxid = it.first;
            const CWalletTx* pcoin = &it.second;

            bool fConflicted;
            int nDepth = pcoin->GetDepthAndMempool(fConflicted);

            if (fConflicted || nDepth < 0)
                continue;

            bool fSafe = pcoin->IsTrusted();

            if (pcoin->tx->HasP2CSOutputs()) {
                for (int i = 0; i < (int) pcoin->tx->vout.size(); i++) {
                    const auto &utxo = pcoin->tx->vout[i];

                    if (IsSpent(wtxid, i))
                        continue;

                    if (utxo.scriptPubKey.IsPayToColdStaking()) {
                        isminetype mine = IsMine(utxo);
                        bool isMineSpendable = mine & ISMINE_SPENDABLE_DELEGATED;
                        if (mine & ISMINE_COLD || isMineSpendable)
                            // Depth and solvability members are not used, no need waste resources and set them for now.
                            vCoins.emplace_back(pcoin, i, 0, isMineSpendable, true, fSafe);
                    }
                }
            }
        }
    }

}

/**
 * Test if the transaction is spendable.
 */
static bool CheckTXAvailabilityInternal(const CWalletTx* pcoin, bool fOnlySafe, int& nDepth, bool& safeTx)
{
    safeTx = pcoin->IsTrusted();
    if (fOnlySafe && !safeTx) return false;
    if (pcoin->GetBlocksToMaturity() > 0) return false;

    nDepth = pcoin->GetDepthInMainChain();

    // We should not consider coins which aren't at least in our mempool
    // It's possible for these to be conflicted via ancestors which we may never be able to detect
    if (nDepth == 0 && !pcoin->InMempool()) return false;

    return true;
}

// cs_main lock required
static bool CheckTXAvailability(const CWalletTx* pcoin, bool fOnlySafe, int& nDepth, bool& safeTx)
{
    AssertLockHeld(cs_main);
    if (!CheckFinalTx(pcoin->tx)) return false;
    return CheckTXAvailabilityInternal(pcoin, fOnlySafe, nDepth, safeTx);
}

// cs_main lock NOT required
static bool CheckTXAvailability(const CWalletTx* pcoin,
                         bool fOnlySafe,
                         int& nDepth,
                         bool& safeTx,
                         int nBlockHeight)
{
    // Mimic CheckFinalTx without cs_main lock
    if (!IsFinalTx(pcoin->tx, nBlockHeight + 1, GetAdjustedTime())) return false;
    return CheckTXAvailabilityInternal(pcoin, fOnlySafe, nDepth, safeTx);
}

bool CWallet::GetMasternodeVinAndKeys(CPubKey& pubKeyRet,
                                      CKey& keyRet,
                                      const COutPoint& collateralOut,
                                      bool fValidateCollateral,
                                      std::string& strError)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    // Find specific vin
    const CWalletTx* wtx = GetWalletTx(collateralOut.hash);
    if (!wtx) {
        strError = "collateral tx not found in the wallet";
        return error("%s: %s", __func__, strError);
    }

    // Verify index limits
    if (collateralOut.n < 0 || collateralOut.n >= (uint32_t) wtx->tx->vout.size()) {
        strError = "Invalid masternode output index";
        return error("%s: output index %d not found in %s", __func__, collateralOut.n, collateralOut.hash.GetHex());
    }

    CTxOut txOut = wtx->tx->vout[collateralOut.n];

    // Masternode collateral value
    const auto& consensus = Params().GetConsensus();
    if (txOut.nValue != consensus.nMNCollateralAmt) {
        strError = strprintf("Invalid collateral tx value, must be %s PIV", FormatMoney(Params().GetConsensus().nMNCollateralAmt));
        return error("%s: tx %s, index %d not a masternode collateral", __func__, collateralOut.hash.GetHex(), collateralOut.n);
    }

    if (fValidateCollateral) {
        int nDepth = 0;
        {
            LOCK(cs_wallet);
            // Check availability
            bool safeTx = false;
            if (!CheckTXAvailability(wtx, true, nDepth, safeTx, m_last_block_processed_height)) {
                strError = "Not available collateral transaction";
                return error("%s: tx %s not available", __func__, collateralOut.hash.GetHex());
            }

            // Skip spent coins
            if (IsSpent(collateralOut.hash, collateralOut.n)) {
                strError = "Error: collateral already spent";
                return error("%s: tx %s already spent", __func__, collateralOut.hash.GetHex());
            }
        }

        // Depth must be at least MASTERNODE_MIN_CONFIRMATIONS
        if (nDepth < consensus.MasternodeCollateralMinConf()) {
            strError = strprintf("Collateral tx must have at least %d confirmations, has %d",
                                 consensus.MasternodeCollateralMinConf(), nDepth);
            return error("%s: %s", __func__, strError);
        }
    }

    CTxDestination destCollateral;
    ExtractDestination(txOut.scriptPubKey, destCollateral, false);
    const CKeyID* keyID = boost::get<CKeyID>(&destCollateral);
    if (!keyID) {
        LogPrintf("%s: Address does not refer to a key\n", __func__);
        return false;
    }

    if (!GetKey(*keyID, keyRet)) {
        LogPrintf("%s: Private key for address is not known\n", __func__);
        return false;
    }

    pubKeyRet = keyRet.GetPubKey();
    return true;
}

CWallet::OutputAvailabilityResult CWallet::CheckOutputAvailability(
        const CTxOut& output,
        const unsigned int outIndex,
        const uint256& wtxid,
        const CCoinControl* coinControl,
        const bool fCoinsSelected,
        const bool fIncludeColdStaking,
        const bool fIncludeDelegated,
        const bool fIncludeLocked) const
{
    OutputAvailabilityResult res;

    // Check if the utxo was spent.
    if (IsSpent(wtxid, outIndex)) return res;

    isminetype mine = IsMine(output);

    // Check If not mine
    if (mine == ISMINE_NO) return res;

    // Skip locked utxo
    if (!fIncludeLocked && IsLockedCoin(wtxid, outIndex)) return res;

    // Check if we should include zero value utxo
    if (output.nValue <= 0) return res;
    if (fCoinsSelected && coinControl && !coinControl->fAllowOtherInputs && !coinControl->IsSelected(COutPoint(wtxid, outIndex)))
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

bool CWallet::AvailableCoins(std::vector<COutput>* pCoins,      // --> populates when != nullptr
                             const CCoinControl* coinControl,   // Default: nullptr
                             AvailableCoinsFilter coinsFilter) const
{
    if (pCoins) pCoins->clear();
    const bool fCoinsSelected = (coinControl != nullptr) && coinControl->HasSelected();
    // include delegated coins when coinControl is active
    if (!coinsFilter.fIncludeDelegated && fCoinsSelected)
        coinsFilter.fIncludeDelegated = true;

    {
        LOCK(cs_wallet);
        CAmount nTotal = 0;
        for (const auto& entry : mapWallet) {
            const uint256& wtxid = entry.first;
            const CWalletTx* pcoin = &entry.second;

            // Check if the tx is selectable
            int nDepth = 0;
            bool safeTx = false;
            if (!CheckTXAvailability(pcoin, coinsFilter.fOnlySafe, nDepth, safeTx, m_last_block_processed_height))
                continue;

            // Check min depth filtering requirements
            if (nDepth < coinsFilter.minDepth) continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
                const auto& output = pcoin->tx->vout[i];

                // Filter by value if needed
                if (coinsFilter.nMaxOutValue > 0 && output.nValue > coinsFilter.nMaxOutValue) {
                    continue;
                }
                if (coinsFilter.nMinOutValue > 0 && output.nValue < coinsFilter.nMinOutValue) {
                    continue;
                }

                // Filter by specific destinations if needed
                if (coinsFilter.onlyFilteredDest && !coinsFilter.onlyFilteredDest->empty()) {
                    CTxDestination address;
                    if (!ExtractDestination(output.scriptPubKey, address) || !coinsFilter.onlyFilteredDest->count(address)) {
                        continue;
                    }
                }

                // Now check for chain availability
                auto res = CheckOutputAvailability(
                        output,
                        i,
                        wtxid,
                        coinControl,
                        fCoinsSelected,
                        coinsFilter.fIncludeColdStaking,
                        coinsFilter.fIncludeDelegated,
                        coinsFilter.fIncludeLocked);

                if (!res.available) continue;
                if (coinsFilter.fOnlySpendable && !res.spendable) continue;

                // found valid coin
                if (!pCoins) return true;
                pCoins->emplace_back(pcoin, (int) i, nDepth, res.spendable, res.solvable, safeTx);

                // Checks the sum amount of all UTXO's.
                if (coinsFilter.nMinimumSumAmount != 0) {
                    nTotal += output.nValue;

                    if (nTotal >= coinsFilter.nMinimumSumAmount) {
                        return true;
                    }
                }

                // Checks the maximum number of UTXO's.
                if (coinsFilter.nMaximumCount > 0 && pCoins->size() >= coinsFilter.nMaximumCount) {
                    return true;
                }
            }
        }
        return (pCoins && !pCoins->empty());
    }
}

std::map<CTxDestination , std::vector<COutput> > CWallet::AvailableCoinsByAddress(bool fOnlySafe, CAmount maxCoinValue, bool fIncludeColdStaking)
{
    CWallet::AvailableCoinsFilter coinFilter;
    coinFilter.fIncludeColdStaking = true;
    coinFilter.fOnlySafe = fOnlySafe;
    coinFilter.fIncludeColdStaking = fIncludeColdStaking;
    coinFilter.nMaxOutValue = maxCoinValue;
    std::vector<COutput> vCoins;
    AvailableCoins(&vCoins, nullptr, coinFilter);

    std::map<CTxDestination, std::vector<COutput> > mapCoins;
    for (const COutput& out : vCoins) {
        CTxDestination address;
        bool fColdStakeAddr = false;
        if (!ExtractDestination(out.tx->tx->vout[out.i].scriptPubKey, address, fColdStakeAddr)) {
            bool isP2CS = out.tx->tx->vout[out.i].scriptPubKey.IsPayToColdStaking();
            if (isP2CS && !fIncludeColdStaking) {
                // It must never happen as the coin filtering process shouldn't had added the P2CS in the first place
                assert(false);
            }
            // if this is a P2CS we don't have the owner key - check if we have the staking key
            fColdStakeAddr = true;
            if (!isP2CS || !ExtractDestination(out.tx->tx->vout[out.i].scriptPubKey, address, fColdStakeAddr) )
                continue;
        }

        mapCoins[address].emplace_back(out);
    }

    return mapCoins;
}

static void ApproximateBestSubset(const std::vector<std::pair<CAmount, std::pair<const CWalletTx*,unsigned int> > >& vValue, const CAmount& nTotalLower, const CAmount& nTargetValue,
                                  std::vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
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
    const bool fIncludeColdStaking = !sporkManager.IsSporkActive(SPORK_19_COLDSTAKING_MAINTENANCE) &&
                                     gArgs.GetBoolArg("-coldstaking", DEFAULT_COLDSTAKING);

    if (pCoins) pCoins->clear();

    LOCK2(cs_main, cs_wallet);
    for (const auto& it : mapWallet) {
        const uint256& wtxid = it.first;
        const CWalletTx* pcoin = &(it).second;

        // Check if the tx is selectable
        int nDepth = 0;
        bool safeTx = false;
        if (!CheckTXAvailability(pcoin, true, nDepth, safeTx))
            continue;

        // Check min depth requirement for stake inputs
        if (nDepth < Params().GetConsensus().nStakeMinDepth) continue;

        const CBlockIndex* pindex = nullptr;
        for (unsigned int index = 0; index < pcoin->tx->vout.size(); index++) {

            auto res = CheckOutputAvailability(
                    pcoin->tx->vout[index],
                    index,
                    wtxid,
                    nullptr, // coin control
                    false,   // fIncludeDelegated
                    fIncludeColdStaking,
                    false,
                    false);   // fIncludeLocked

            if (!res.available || !res.spendable) continue;

            // found valid coin
            if (!pCoins) return true;
            if (!pindex) pindex = mapBlockIndex.at(pcoin->m_confirm.hashBlock);
            pCoins->emplace_back(pcoin, (int) index, nDepth, pindex);
        }
    }
    return (pCoins && !pCoins->empty());
}

bool CWallet::SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, uint64_t nMaxAncestors, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<CAmount>::max();
    coinLowestLarger.second.first = NULL;
    std::vector<std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> > > vValue;
    CAmount nTotalLower = 0;

    Shuffle(vCoins.begin(), vCoins.end(), FastRandomContext());

    for (const COutput& output : vCoins) {
        if (!output.fSpendable)
            continue;

        const CWalletTx* pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs)) {
            continue;
        }

        if (!mempool.TransactionWithinChainLimit(pcoin->GetHash(), nMaxAncestors)) {
            continue;
        }

        int i = output.i;
        CAmount n = pcoin->tx->vout[i].nValue;

        std::pair<CAmount, std::pair<const CWalletTx*, unsigned int> > coin = std::make_pair(n, std::make_pair(pcoin, i));

        if (n == nTargetValue) {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        } else if (n < nTargetValue + MIN_CHANGE) {
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

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + MIN_CHANGE)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + MIN_CHANGE, vfBest, nBest);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + MIN_CHANGE) || coinLowestLarger.first <= nBest)) {
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

            nValueRet += out.tx->tx->vout[out.i].nValue;
            setCoinsRet.emplace(out.tx, out.i);
        }
        return (nValueRet >= nTargetValue);
    }

    // calculate value from preset inputs and store them
    std::set<std::pair<const CWalletTx*, uint32_t> > setPresetCoins;
    CAmount nValueFromPresetInputs = 0;

    std::vector<OutPointWrapper> vPresetInputs;
    if (coinControl)
        coinControl->ListSelected(vPresetInputs);
    for (const auto& outpoint : vPresetInputs) {
        std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(outpoint.outPoint.hash);
        if (it != mapWallet.end()) {
            const CWalletTx* pcoin = &it->second;
            // Clearly invalid input, fail
            if (pcoin->tx->vout.size() <= outpoint.outPoint.n)
                return false;
            nValueFromPresetInputs += pcoin->tx->vout[outpoint.outPoint.n].nValue;
            setPresetCoins.emplace(pcoin, outpoint.outPoint.n);
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

    size_t nMaxChainLength = std::min(gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT), gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT));

    bool res = nTargetValue <= nValueFromPresetInputs ||
            SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 6, 0, vCoins, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 1, 1, 0, vCoins, setCoinsRet, nValueRet) ||
            (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, 2, vCoins, setCoinsRet, nValueRet)) ||
            (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, std::min((size_t)4, nMaxChainLength/3), vCoins, setCoinsRet, nValueRet)) ||
            (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, nMaxChainLength/2, vCoins, setCoinsRet, nValueRet)) ||
            (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue - nValueFromPresetInputs, 0, 1, nMaxChainLength, vCoins, setCoinsRet, nValueRet));

    // because SelectCoinsMinConf clears the setCoinsRet, we now add the possible inputs to the coinset
    setCoinsRet.insert(setPresetCoins.begin(), setPresetCoins.end());

    // add preset inputs to the total value selected
    nValueRet += nValueFromPresetInputs;

    return res;
}

std::map<std::pair<CTxDestination, Optional<CTxDestination>>, std::vector<COutput>> CWallet::ListCoins() const
{
    std::map<std::pair<CTxDestination, Optional<CTxDestination>>, std::vector<COutput>> result;

    CWallet::AvailableCoinsFilter filter;
    filter.fIncludeLocked = true;
    filter.fOnlySpendable = true;
    std::vector<COutput> availableCoins;
    AvailableCoins(&availableCoins, nullptr, filter);

    for (const COutput& coin : availableCoins) {
        const CScript& scriptPubKey = coin.tx->tx->vout[coin.i].scriptPubKey;
        txnouttype type; std::vector<CTxDestination> addresses; int nRequired;
        if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
            if (addresses.size() == 1) {
                // P2PK, P2PKH scripts
                const auto& addrpair = std::make_pair(addresses[0], nullopt);
                result[addrpair].emplace_back(std::move(coin));
            } else if (type == TX_COLDSTAKE) {
                // P2CS scripts
                assert(addresses.size() == 2);
                const auto& addrpair = std::make_pair(addresses[1], Optional<CTxDestination>(addresses[0]));
                result[addrpair].emplace_back(std::move(coin));
            }
        }
    }

    return result;
}

std::map<libzcash::SaplingPaymentAddress, std::vector<SaplingNoteEntry>> CWallet::ListNotes() const
{
    return m_sspk_man->ListNotes();
}

bool CWallet::CreateBudgetFeeTX(CTransactionRef& tx, const uint256& hash, CReserveKey& keyChange, CAmount fee)
{
    CScript scriptChange;
    scriptChange << OP_RETURN << ToByteVector(hash);

    CAmount nFeeRet = 0;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;
    vecSend.emplace_back(scriptChange, fee, false);

    CCoinControl* coinControl = nullptr;
    int nChangePosInOut = -1;
    bool success = CreateTransaction(vecSend, tx, keyChange, nFeeRet, nChangePosInOut, strFail, coinControl, true, (CAmount)0);
    if (!success) {
        LogPrintf("%s: Error - %s\n", __func__, strFail);
        return false;
    }

    return true;
}

bool CWallet::SignTransaction(CMutableTransaction& tx)
{
    LOCK(cs_wallet); // mapWallet

    // sign the new tx
    int nIn = 0;
    for (auto& input : tx.vin) {
        std::map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(input.prevout.hash);
        if (mi == mapWallet.end() || input.prevout.n >= mi->second.tx->vout.size()) {
            return false;
        }
        const CScript& scriptPubKey = mi->second.tx->vout[input.prevout.n].scriptPubKey;
        const CAmount& amount = mi->second.tx->vout[input.prevout.n].nValue;
        SignatureData sigdata;

        if (!ProduceSignature(MutableTransactionSignatureCreator(this, &tx, nIn, amount, SIGHASH_ALL),
                              scriptPubKey,
                              sigdata,
                              tx.GetRequiredSigVersion(),
                              false /* fColdStake */ )) {
            return false;
        }
        UpdateTransaction(tx, nIn, sigdata);
        nIn++;
    }
    return true;
}

bool CWallet::FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, bool overrideEstimatedFeeRate, const CFeeRate& specificFeeRate, int& nChangePosInOut, std::string& strFailReason, bool includeWatching, bool lockUnspents, const std::set<int>& setSubtractFeeFromOutputs, const CTxDestination& destChange)
{
    std::vector<CRecipient> vecSend;

    // Turn the txout set into a CRecipient vector.
    for (size_t idx = 0; idx < tx.vout.size(); idx++) {
        const CTxOut& txOut = tx.vout[idx];
        vecSend.emplace_back(txOut.scriptPubKey, txOut.nValue, setSubtractFeeFromOutputs.count(idx) == 1);
    }

    CCoinControl coinControl;
    coinControl.destChange = destChange;
    coinControl.fAllowOtherInputs = true;
    coinControl.fAllowWatchOnly = includeWatching;
    coinControl.fOverrideFeeRate = overrideEstimatedFeeRate;
    coinControl.nFeeRate = specificFeeRate;

    const int nExtraSize = tx.isSaplingVersion() ?
            (int)(GetSerializeSize(tx.sapData) + GetSerializeSize(tx.extraPayload)) : 0;

    for (const CTxIn& txin : tx.vin) {
        coinControl.Select(txin.prevout);
    }

    // Acquire the locks to prevent races to the new locked unspents between the
    // CreateTransaction call and LockCoin calls (when lockUnspents is true).
    LOCK2(cs_main, cs_wallet);

    CReserveKey reservekey(this);
    CTransactionRef wtx;
    if (!CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePosInOut, strFailReason, &coinControl, false, 0, false, nullptr, nExtraSize)) {
        return false;
    }

    if (nChangePosInOut != -1) {
        tx.vout.insert(tx.vout.begin() + nChangePosInOut, wtx->vout[nChangePosInOut]);
        // We don't have the normal Create/Commit cycle, and don't want to risk
        // reusing change, so just remove the key from the keypool here.
        reservekey.KeepKey();
    }

    // Copy output sizes from new transaction; they may have had the fee
    // subtracted from them.
    for (unsigned int idx = 0; idx < tx.vout.size(); idx++) {
        tx.vout[idx].nValue = wtx->vout[idx].nValue;
    }

    // Add new txins while keeping original txin scriptSig/order.
    for (const CTxIn& txin : wtx->vin) {
        if (!coinControl.IsSelected(txin.prevout)) {
            tx.vin.emplace_back(txin);
            if (lockUnspents) LockCoin(txin.prevout);
        }
    }

    return true;
}

std::vector<COutput> CWallet::GetOutputsFromCoinControl(const CCoinControl* coinControl)
{
    assert(coinControl);
    LOCK(cs_wallet);
    std::vector<COutput> vCoinsRet;
    std::vector<OutPointWrapper> vPresetInputs;
    coinControl->ListSelected(vPresetInputs);
    for (const auto& out : vPresetInputs) {
        auto it = mapWallet.find(out.outPoint.hash);
        if (it != mapWallet.end()) {
            assert(it->second.tx->vout.size() > out.outPoint.n);
            vCoinsRet.emplace_back(COutput(&(it->second), out.outPoint.n, 0, true, true, true));
        }
    }
    return vCoinsRet;
}

bool CWallet::CreateTransaction(const std::vector<CRecipient>& vecSend,
    CTransactionRef& txRet,
    CReserveKey& reservekey,
    CAmount& nFeeRet,
    int& nChangePosInOut,
    std::string& strFailReason,
    const CCoinControl* coinControl,
    bool sign,
    CAmount nFeePay,
    bool fIncludeDelegated,
    bool* fStakeDelegationVoided,
    int nExtraSize,
    int nMinDepth)
{
    CAmount nValue = 0;
    int nChangePosRequest = nChangePosInOut;
    unsigned int nSubtractFeeFromAmount = 0;
    for (const CRecipient& rec : vecSend) {
        if (nValue < 0 || rec.nAmount < 0) {
            strFailReason = _("Transaction amounts must be positive");
            return false;
        }
        nValue += rec.nAmount;
        if (rec.fSubtractFeeFromAmount)
            nSubtractFeeFromAmount++;
    }
    if (vecSend.empty()) {
        strFailReason = _("Transaction must have at least one recipient");
        return false;
    }

    CMutableTransaction txNew;
    CScript scriptChange;

    CWallet::AvailableCoinsFilter coinFilter;
    coinFilter.fOnlySpendable = true;
    coinFilter.fIncludeDelegated = fIncludeDelegated;
    coinFilter.minDepth = nMinDepth;

    {
        std::set<std::pair<const CWalletTx*,unsigned int> > setCoins;
        LOCK2(cs_main, cs_wallet);
        {
            std::vector<COutput> vAvailableCoins;
            if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs) {
                // Select only the outputs that the caller pre-selected.
                vAvailableCoins = GetOutputsFromCoinControl(coinControl);
            } else {
                // Regular selection
                AvailableCoins(&vAvailableCoins, coinControl, coinFilter);
            }

            nFeeRet = 0;
            if (nFeePay > 0) nFeeRet = nFeePay;
            while (true) {
                nChangePosInOut = nChangePosRequest;
                txNew.vin.clear();
                txNew.vout.clear();
                bool fFirst = true;

                CAmount nValueToSelect = nValue;
                if (nSubtractFeeFromAmount == 0)
                    nValueToSelect += nFeeRet;

                // Fill outputs
                for (const CRecipient& rec : vecSend) {
                    CTxOut txout(rec.nAmount, rec.scriptPubKey);
                    if (rec.fSubtractFeeFromAmount) {
                        assert(nSubtractFeeFromAmount != 0);
                        txout.nValue -= nFeeRet / nSubtractFeeFromAmount; // Subtract fee equally from each selected recipient

                        if (fFirst) {
                            // first receiver pays the remainder not divisible by output count
                            fFirst = false;
                            txout.nValue -= nFeeRet % nSubtractFeeFromAmount;
                        }
                    }
                    if (IsDust(txout, dustRelayFee)) {
                        strFailReason = _("Transaction amount too small");
                        return false;
                    }
                    txNew.vout.emplace_back(txout);
                }

                // Choose coins to use
                CAmount nValueIn = 0;
                setCoins.clear();

                if (!SelectCoinsToSpend(vAvailableCoins, nValueToSelect, setCoins, nValueIn, coinControl)) {
                    strFailReason = _("Insufficient funds.");
                    return false;
                }

                // Change
                CAmount nChange = nValueIn - nValueToSelect;
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
                        if (IsDust(newTxOut, dustRelayFee)) {
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
                } else {
                    reservekey.ReturnKey();
                    nChangePosInOut = -1;
                }

                // Fill vin
                for (const std::pair<const CWalletTx*, unsigned int>& coin : setCoins) {
                    if(fStakeDelegationVoided && coin.first->tx->vout[coin.second].scriptPubKey.IsPayToColdStaking()) {
                        *fStakeDelegationVoided = true;
                    }
                    txNew.vin.emplace_back(coin.first->GetHash(), coin.second);
                }

                // Fill in dummy signatures for fee calculation.
                int nIn = 0;
                for (const auto & coin : setCoins) {
                    const CScript& scriptPubKey = coin.first->tx->vout[coin.second].scriptPubKey;
                    SignatureData sigdata;
                    if (!ProduceSignature(DummySignatureCreator(this), scriptPubKey, sigdata, txNew.GetRequiredSigVersion(), false)) {
                        strFailReason = _("Signing transaction failed");
                        return false;
                    } else {
                        UpdateTransaction(txNew, nIn, sigdata);
                    }
                    nIn++;
                }

                // account for additional payloads in fee calculation
                const unsigned int nBytes = ::GetSerializeSize(txNew, PROTOCOL_VERSION) + nExtraSize;
                CAmount nFeeNeeded = std::max(nFeePay, GetMinimumFee(nBytes, nTxConfirmTarget, mempool));

                // Remove scriptSigs to eliminate the fee calculation dummy signatures
                for (CTxIn& vin : txNew.vin) {
                    vin.scriptSig = CScript();
                }

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

        if (sign) {
            int nIn = 0;
            for (const auto& coin : setCoins) {
                const CScript& scriptPubKey = coin.first->tx->vout[coin.second].scriptPubKey;
                SignatureData sigdata;
                bool haveKey = coin.first->GetStakeDelegationCredit() > 0;

                if (!ProduceSignature(
                        MutableTransactionSignatureCreator(this, &txNew, nIn, coin.first->tx->vout[coin.second].nValue, SIGHASH_ALL),
                        scriptPubKey,
                        sigdata,
                        txNew.GetRequiredSigVersion(),
                        !haveKey    // fColdStake
                        )) {
                    strFailReason = _("Signing transaction failed");
                    return false;
                } else {
                    UpdateTransaction(txNew, nIn, sigdata);
                }
                nIn++;
            }
        }

        // Limit size
        if (::GetSerializeSize(txNew, PROTOCOL_VERSION) >= MAX_STANDARD_TX_SIZE) {
            strFailReason = _("Transaction too large");
            return false;
        }

        // Embed the constructed transaction data in wtxNew.
        txRet = MakeTransactionRef(std::move(txNew));
    }

    // Lastly, ensure this tx will pass the mempool's chain limits
    CTxMemPoolEntry entry(txRet, 0, 0, 0, false, 0);
    CTxMemPool::setEntries setAncestors;
    size_t nLimitAncestors = gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
    size_t nLimitAncestorSize = gArgs.GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT)*1000;
    size_t nLimitDescendants = gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
    size_t nLimitDescendantSize = gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT)*1000;
    std::string errString;
    if (!mempool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize, nLimitDescendants, nLimitDescendantSize, errString)) {
        strFailReason = _("Transaction has too long of a mempool chain");
        return false;
    }

    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey, const CAmount& nValue, CTransactionRef& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl, CAmount nFeePay, bool fIncludeDelegated, bool* fStakeDelegationVoided, int nExtraSize, int nMinDepth)
{
    std::vector<CRecipient> vecSend;
    vecSend.emplace_back(scriptPubKey, nValue, false);
    int nChangePosInOut = -1;
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePosInOut, strFailReason, coinControl, true, nFeePay, fIncludeDelegated, fStakeDelegationVoided, nExtraSize, nMinDepth);
}

int CWallet::GetLastBlockHeightLockWallet() const
{
    return WITH_LOCK(cs_wallet, return m_last_block_processed_height;);
}

bool CWallet::CreateCoinstakeOuts(const CPivStake& stakeInput, std::vector<CTxOut>& vout, CAmount nTotal) const
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    CTxOut stakePrevout;
    if (!stakeInput.GetTxOutFrom(stakePrevout)) {
        return error("%s: failed to get stake input", __func__);
    }
    CScript scriptPubKeyKernel = stakePrevout.scriptPubKey;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
        return error("%s: failed to parse kernel", __func__);

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH && whichType != TX_COLDSTAKE)
        return error("%s: type=%d (%s) not supported for scriptPubKeyKernel", __func__, whichType, GetTxnOutputType(whichType));

    CKey key;
    if (whichType == TX_PUBKEYHASH || whichType == TX_COLDSTAKE) {
        // if P2PKH or P2CS check that we have the input private key
        if (!GetKey(CKeyID(uint160(vSolutions[0])), key))
            return error("%s: Unable to get staking private key", __func__);
    }

    vout.emplace_back(0, scriptPubKeyKernel);

    // Calculate if we need to split the output
    if (nStakeSplitThreshold > 0) {
        int nSplit = static_cast<int>(nTotal / nStakeSplitThreshold);
        if (nSplit > 1) {
            // if nTotal is twice or more of the threshold; create more outputs
            int txSizeMax = MAX_STANDARD_TX_SIZE >> 11; // limit splits to <10% of the max TX size (/2048)
            if (nSplit > txSizeMax)
                nSplit = txSizeMax;
            for (int i = nSplit; i > 1; i--) {
                LogPrintf("%s: StakeSplit: nTotal = %d; adding output %d of %d\n", __func__, nTotal, (nSplit-i)+2, nSplit);
                vout.emplace_back(0, scriptPubKeyKernel);
            }
        }
    }

    return true;
}

bool CWallet::CreateCoinStake(
        const CBlockIndex* pindexPrev,
        unsigned int nBits,
        CMutableTransaction& txNew,
        int64_t& nTxNewTime,
        std::vector<CStakeableOutput>* availableCoins,
        bool stopOnNewBlock) const
{
    // shuffle coins
    if (availableCoins && Params().IsRegTestNet()) {
        Shuffle(availableCoins->begin(), availableCoins->end(), FastRandomContext());
    }

    // Mark coin stake transaction
    txNew.vin.clear();
    txNew.vout.clear();
    txNew.vout.emplace_back(0, CScript());

    // update staker status (hash)
    pStakerStatus->SetLastTip(pindexPrev);
    pStakerStatus->SetLastCoins((int) availableCoins->size());

    // Kernel Search
    CAmount nCredit;
    CScript scriptPubKeyKernel;
    bool fKernelFound = false;
    int nAttempts = 0;
    for (auto it = availableCoins->begin(); it != availableCoins->end();) {
        COutPoint outPoint = COutPoint(it->tx->GetHash(), it->i);
        CPivStake stakeInput(it->tx->tx->vout[it->i],
                             outPoint,
                             it->pindex);

        // New block came in, move on
        if (stopOnNewBlock && GetLastBlockHeightLockWallet() != pindexPrev->nHeight) return false;

        // Make sure the wallet is unlocked and shutdown hasn't been requested
        if (IsLocked() || ShutdownRequested()) return false;

        // Make sure the stake input hasn't been spent since last check
        if (WITH_LOCK(cs_wallet, return IsSpent(outPoint))) {
            // remove it from the available coins
            it = availableCoins->erase(it);
            continue;
        }

        nCredit = 0;

        nAttempts++;
        fKernelFound = Stake(pindexPrev, &stakeInput, nBits, nTxNewTime);

        // update staker status (time, attempts)
        pStakerStatus->SetLastTime(nTxNewTime);
        pStakerStatus->SetLastTries(nAttempts);

        if (!fKernelFound) {
            it++;
            continue;
        }

        // Found a kernel
        LogPrintf("CreateCoinStake : kernel found\n");
        nCredit += stakeInput.GetValue();

        // Add block reward to the credit
        nCredit += GetBlockValue(pindexPrev->nHeight + 1);

        // Create the output transaction(s)
        std::vector<CTxOut> vout;
        if (!CreateCoinstakeOuts(stakeInput, vout, nCredit)) {
            LogPrintf("%s : failed to create output\n", __func__);
            it++;
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

        // Set coinstake input
        txNew.vin.emplace_back(stakeInput.GetTxIn());

        // Limit size
        unsigned int nBytes = ::GetSerializeSize(txNew, PROTOCOL_VERSION);
        if (nBytes >= DEFAULT_BLOCK_MAX_SIZE / 5)
            return error("%s : exceeded coinstake size limit", __func__);

        break;
    }
    LogPrint(BCLog::STAKING, "%s: attempted staking %d times\n", __func__, nAttempts);

    return fKernelFound;
}

bool CWallet::SignCoinStake(CMutableTransaction& txNew) const
{
    // Sign it
    int nIn = 0;
    for (const CTxIn& txIn : txNew.vin) {
        const CWalletTx* wtx = GetWalletTx(txIn.prevout.hash);
        if (!wtx || !SignSignature(*this, *(wtx->tx), txNew, nIn++, SIGHASH_ALL, true))
            return error("%s : failed to sign coinstake", __func__);
    }

    // Successfully signed coinstake
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

CWallet::CommitResult CWallet::CommitTransaction(CTransactionRef tx, CReserveKey& opReservekey, CConnman* connman)
{
    return CommitTransaction(std::move(tx), &opReservekey, connman);
}

/**
 * Call after CreateTransaction unless you want to abort
 */
CWallet::CommitResult CWallet::CommitTransaction(CTransactionRef tx, CReserveKey* opReservekey, CConnman* connman, mapValue_t* extras)
{
    CommitResult res;

    CWalletTx wtxNew(this, std::move(tx));
    wtxNew.fTimeReceivedIsTxTime = true;
    wtxNew.BindWallet(this);
    wtxNew.fFromMe = true;
    wtxNew.fStakeDelegationVoided = wtxNew.tx->HasP2CSOutputs();
    if (extras) wtxNew.mapValue.insert(extras->begin(), extras->end());

    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("%s:\n%s", __func__, wtxNew.tx->ToString());
        {
            // Take key pair from key pool so it won't be used again
            if (opReservekey) opReservekey->KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Notify that old coins are spent
            if (!wtxNew.tx->HasZerocoinSpendInputs()) {
                std::set<uint256> updated_hashes;
                for (const CTxIn& txin : wtxNew.tx->vin) {
                    // notify only once
                    if (updated_hashes.find(txin.prevout.hash) != updated_hashes.end()) continue;

                    CWalletTx& coin = mapWallet.at(txin.prevout.hash);
                    coin.BindWallet(this);
                    NotifyTransactionChanged(this, txin.prevout.hash, CT_UPDATED);
                    updated_hashes.insert(txin.prevout.hash);
                }
            }
        }

        res.hashTx = wtxNew.GetHash();

        // Get the inserted-CWalletTx from mapWallet so that the
        // fInMempool flag is cached properly
        CWalletTx& wtx = mapWallet.at(wtxNew.GetHash());

        // Try ATMP. This must not fail. The transaction has already been signed and recorded.
        CValidationState state;
        if (!wtx.AcceptToMemoryPool(state)) {
            res.state = state;
            // Abandon the transaction
            if (AbandonTransaction(res.hashTx)) {
                res.status = CWallet::CommitStatus::Abandoned;
                // Return the change key
                if (opReservekey) opReservekey->ReturnKey();
            }

            LogPrintf("%s: ERROR: %s\n", __func__, res.ToString());
            return res;
        }

        res.status = CWallet::CommitStatus::OK;

        // Broadcast
        wtx.RelayWalletTransaction(connman);
    }
    return res;
}

DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    LOCK2(cs_main, cs_wallet);

    DBErrors nLoadWalletRet = WalletBatch(*database, "cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE) {
        if (database->Rewrite( "\x04pool")) {
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
    DBErrors nZapWalletTxRet = WalletBatch(*database, "cr+").ZapWalletTx(this, vWtx);
    if (nZapWalletTxRet == DB_NEED_REWRITE) {
        if (database->Rewrite("\x04pool")) {
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

std::string CWallet::ParseIntoAddress(const CWDestination& dest, const std::string& purpose) {
    const CChainParams::Base58Type addrType =
            AddressBook::IsColdStakingPurpose(purpose) ?
            CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS;
    return Standard::EncodeDestination(dest, addrType);
}

bool CWallet::SetAddressBook(const CWDestination& address, const std::string& strName, const std::string& strPurpose)
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
    std::string addressStr = ParseIntoAddress(address, mapAddressBook.at(address).purpose);
    if (!strPurpose.empty() && !WalletBatch(*database).WritePurpose(addressStr, strPurpose))
        return false;
    return WalletBatch(*database).WriteName(addressStr, strName);
}

bool CWallet::DelAddressBook(const CWDestination& address, const CChainParams::Base58Type addrType)
{
    std::string strAddress = Standard::EncodeDestination(address, addrType);
    std::string purpose = GetPurposeForAddressBookEntry(address);
    {
        LOCK(cs_wallet); // mapAddressBook

        // Delete destdata tuples associated with address
        for (const std::pair<std::string, std::string> & item : mapAddressBook[address].destdata) {
            WalletBatch(*database).EraseDestData(strAddress, item.first);
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address) != ISMINE_NO, purpose, CT_DELETED);

    WalletBatch(*database).ErasePurpose(strAddress);
    return WalletBatch(*database).EraseName(strAddress);
}

std::string CWallet::GetPurposeForAddressBookEntry(const CWDestination& address) const
{
    LOCK(cs_wallet);
    auto it = mapAddressBook.find(address);
    return it != mapAddressBook.end() ? it->second.purpose : "";
}

std::string CWallet::GetNameForAddressBookEntry(const CWDestination& address) const
{
    LOCK(cs_wallet);
    auto it = mapAddressBook.find(address);
    return it != mapAddressBook.end() ? it->second.name : "";
}

Optional<AddressBook::CAddressBookData> CWallet::GetAddressBookEntry(const CWDestination& dest) const
{
    LOCK(cs_wallet);
    auto it = mapAddressBook.find(dest);
    return it != mapAddressBook.end() ? Optional<AddressBook::CAddressBookData>(it->second) : nullopt;
}

void CWallet::LoadAddressBookName(const CWDestination& dest, const std::string& strName)
{
    mapAddressBook[dest].name = strName;
}

void CWallet::LoadAddressBookPurpose(const CWDestination& dest, const std::string& strPurpose)
{
    mapAddressBook[dest].purpose = strPurpose;
}

bool CWallet::HasAddressBook(const CWDestination& address) const
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
        const auto mi = mapAddressBook.find(delegator);
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

void CWallet::ReturnKey(int64_t nIndex, const bool internal, const bool staking)
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
        for (const auto& walletEntry : mapWallet) {
            const CWalletTx* pcoin = &walletEntry.second;

            if (!IsFinalTx(pcoin->tx, m_last_block_processed_height) || !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            bool fConflicted;
            int nDepth = pcoin->GetDepthAndMempool(fConflicted);
            if (fConflicted)
                continue;
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++) {
                CTxDestination addr;
                if (!IsMine(pcoin->tx->vout[i]))
                    continue;
                if ( !ExtractDestination(pcoin->tx->vout[i].scriptPubKey, addr) &&
                        !ExtractDestination(pcoin->tx->vout[i].scriptPubKey, addr, true) )
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->tx->vout[i].nValue;

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

    for (const auto& walletEntry : mapWallet) {
        const CWalletTx* pcoin = &walletEntry.second;

        if (pcoin->tx->vin.size() > 0) {
            bool any_mine = false;
            // group all input addresses with each other
            for (CTxIn txin : pcoin->tx->vin) {
                CTxDestination address;
                if (!IsMine(txin)) /* If this input isn't mine, ignore it */
                    continue;
                if (!ExtractDestination(mapWallet.at(txin.prevout.hash).tx->vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine) {
                for (CTxOut txout : pcoin->tx->vout)
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
        for (unsigned int i = 0; i < pcoin->tx->vout.size(); i++)
            if (IsMine(pcoin->tx->vout[i])) {
                CTxDestination address;
                if (!ExtractDestination(pcoin->tx->vout[i].scriptPubKey, address))
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
    for (const auto& item : mapAddressBook) {
        if (item.second.isShielded()) continue;
        const auto& address = boost::get<CTxDestination>(item.first);
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

std::set<COutPoint> CWallet::ListLockedCoins()
{
    AssertLockHeld(cs_wallet);
    return setLockedCoins;
}

bool CWallet::SetStakeSplitThreshold(const CAmount sst)
{
    LOCK(cs_wallet);
    if (nStakeSplitThreshold != sst) {
        nStakeSplitThreshold = sst;
        if (!WalletBatch(*database).WriteStakeSplitThreshold(sst)) {
            return false;
        }
        NotifySSTChanged(sst);
    }
    return true;
}

/** @} */ // end of Actions


void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (const auto& entry : mapKeyMetadata) {
        if (entry.second.nCreateTime) mapKeyBirth[entry.first] = entry.second.nCreateTime;
    }

    // map in which we'll infer heights of other keys
    CBlockIndex* pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    for (const CKeyID& keyid : GetKeys()) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (const auto& entry : mapWallet) {
        // iterate over all wallet transactions...
        const CWalletTx& wtx = entry.second;
        CBlockIndex* pindex = LookupBlockIndex(wtx.m_confirm.hashBlock);
        if (pindex && chainActive.Contains(pindex)) {
            // ... which are already in a block
            int nHeight = pindex->nHeight;
            for (const CTxOut& txout : wtx.tx->vout) {
                // iterate over all their outputs
                CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
                for (const CKeyID& keyid : vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = pindex;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (const auto& entry : mapKeyFirstBlock)
        mapKeyBirth[entry.first] = entry.second->GetBlockTime() - TIMESTAMP_WINDOW; // block times can be 2h off
}

bool CWallet::AddDestData(const CTxDestination& dest, const std::string& key, const std::string& value)
{
    if (!IsValidDestination(dest))
        return false;

    mapAddressBook[dest].destdata.emplace(key, value);
    return WalletBatch(*database).WriteDestData(EncodeDestination(dest), key, value);
}

bool CWallet::EraseDestData(const CTxDestination& dest, const std::string& key)
{
    if (!mapAddressBook[dest].destdata.erase(key))
        return false;
    return WalletBatch(*database).EraseDestData(EncodeDestination(dest), key);
}

bool CWallet::LoadDestData(const CTxDestination& dest, const std::string& key, const std::string& value)
{
    mapAddressBook[dest].destdata.emplace(key, value);
    return true;
}

std::vector<std::string> CWallet::GetDestValues(const std::string& prefix) const
{
    LOCK(cs_wallet);
    std::vector<std::string> values;
    for (const auto& address : mapAddressBook) {
        for (const auto& data : address.second.destdata) {
            if (!data.first.compare(0, prefix.size(), prefix)) {
                values.emplace_back(data.second);
            }
        }
    }
    return values;
}

void CWallet::AutoCombineDust(CConnman* connman)
{
    {
        LOCK(cs_wallet);
        if (m_last_block_processed.IsNull() ||
            m_last_block_processed_time < (GetAdjustedTime() - 300) ||
            IsLocked()) {
            return;
        }
    }

    std::map<CTxDestination, std::vector<COutput> > mapCoinsByAddress =
            AvailableCoinsByAddress(true, nAutoCombineThreshold, false);

    //coins are sectioned by address. This combination code only wants to combine inputs that belong to the same address
    for (const auto& entry : mapCoinsByAddress) {
        std::vector<COutput> vCoins, vRewardCoins;
        bool maxSize = false;
        vCoins = entry.second;

        // We don't want the tx to be refused for being too large
        // we use 50 bytes as a base tx size (2 output: 2*34 + overhead: 10 -> 90 to be certain)
        unsigned int txSizeEstimate = 90;

        //find masternode rewards that need to be combined
        CCoinControl* coinControl = new CCoinControl();
        CAmount nTotalRewardsValue = 0;
        for (const COutput& out : vCoins) {
            if (!out.fSpendable)
                continue;

            COutPoint outpt(out.tx->GetHash(), out.i);
            coinControl->Select(outpt);
            vRewardCoins.push_back(out);
            nTotalRewardsValue += out.Value();

            // Combine to the threshold and not way above
            if (nTotalRewardsValue > nAutoCombineThreshold)
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
        const CScript& scriptPubKey = GetScriptForDestination(entry.first);
        vecSend.emplace_back(scriptPubKey, nTotalRewardsValue, false);

        //Send change to same address
        CTxDestination destMyAddress;
        if (!ExtractDestination(scriptPubKey, destMyAddress)) {
            LogPrintf("AutoCombineDust: failed to extract destination\n");
            continue;
        }
        coinControl->destChange = destMyAddress;

        // Create the transaction and commit it to the network
        CTransactionRef wtx;
        CReserveKey keyChange(this); // this change address does not end up being used, because change is returned with coin control switch
        std::string strErr;
        CAmount nFeeRet = 0;
        int nChangePosInOut = -1;

        // 10% safety margin to avoid "Insufficient funds" errors
        vecSend[0].nAmount = nTotalRewardsValue - (nTotalRewardsValue / 10);

        {
            // For now, CreateTransaction requires cs_main lock.
            LOCK2(cs_main, cs_wallet);
            if (!CreateTransaction(vecSend, wtx, keyChange, nFeeRet, nChangePosInOut, strErr, coinControl,
                                   true, false, CAmount(0))) {
                LogPrintf("AutoCombineDust createtransaction failed, reason: %s\n", strErr);
                continue;
            }
        }

        //we don't combine below the threshold unless the fees are 0 to avoid paying fees over fees over fees
        if (!maxSize && nTotalRewardsValue < nAutoCombineThreshold && nFeeRet > 0)
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

void CWallet::LockOutpointIfMineWithMutex(const CTransactionRef& ptx, const COutPoint& c)
{
    WITH_LOCK(cs_wallet, LockOutpointIfMine(ptx, c));
}

void CWallet::LockOutpointIfMine(const CTransactionRef& ptx, const COutPoint& c)
{
    AssertLockHeld(cs_wallet);
    CTxOut txout;
    if (ptx && c.hash == ptx->GetHash() && c.n < ptx->vout.size()) {
        // the collateral is an output of this tx
        txout = ptx->vout[c.n];
    } else {
        // the collateral is a reference to an utxo inside this wallet
        const auto& it = mapWallet.find(c.hash);
        if (it != mapWallet.end()) {
            txout = it->second.tx->vout[c.n];
        }
    }
    if (!txout.IsNull() && IsMine(txout) != ISMINE_NO && !IsSpent(c)) {
        LockCoin(c);
    }
}

// Called from AddToWalletIfInvolvingMe
void CWallet::LockIfMyCollateral(const CTransactionRef& ptx)
{
    AssertLockHeld(cs_wallet);

    COutPoint o;
    if (GetProRegCollateral(ptx, o)) {
        LockOutpointIfMine(ptx, o);
    }
}

CWallet* CWallet::CreateWalletFromFile(const std::string& name, const fs::path& path)
{
    const std::string& walletFile = name;

    // needed to restore wallet transaction meta data after -zapwallettxes
    std::vector<CWalletTx> vWtx;

    if (gArgs.GetBoolArg("-zapwallettxes", false)) {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

        std::unique_ptr<CWallet> tempWallet = std::make_unique<CWallet>(name, WalletDatabase::Create(path));
        DBErrors nZapWalletRet = tempWallet->ZapWalletTx(vWtx);
        if (nZapWalletRet != DB_LOAD_OK) {
            UIError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        }
    }

    uiInterface.InitMessage(_("Loading wallet..."));

    int64_t nStart = GetTimeMillis();
    bool fFirstRun = true;
    CWallet *walletInstance = new CWallet(name, WalletDatabase::Create(path));
    DBErrors nLoadWalletRet = walletInstance->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK) {
        if (nLoadWalletRet == DB_CORRUPT) {
            UIError(strprintf(_("Error loading %s: Wallet corrupted"), walletFile));
            return nullptr;
        } else if (nLoadWalletRet == DB_NONCRITICAL_ERROR) {
            UIWarning(strprintf(_("Warning: error reading %s! All keys read correctly, but transaction data"
                         " or address book entries might be missing or incorrect."), walletFile));
        } else if (nLoadWalletRet == DB_TOO_NEW) {
            UIError(strprintf(_("Error loading %s: Wallet requires newer version of %s"), walletFile, PACKAGE_NAME));
            return nullptr;
        } else if (nLoadWalletRet == DB_NEED_REWRITE) {
            UIError(strprintf(_("Wallet needed to be rewritten: restart %s to complete"), PACKAGE_NAME));
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
    const bool fLegacyWallet = gArgs.GetBoolArg("-legacywallet", false);
    if (gArgs.GetBoolArg("-upgradewallet", fFirstRun && !fLegacyWallet) ||
            (!walletInstance->IsLocked() && prev_version == FEATURE_PRE_SPLIT_KEYPOOL)) {
        if (prev_version <= FEATURE_PRE_PIVX && walletInstance->IsLocked()) {
            // Cannot upgrade a locked wallet
            UIError(_("Cannot upgrade a locked wallet."));
            return nullptr;
        }

        int nMaxVersion = gArgs.GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = FEATURE_LATEST;
            walletInstance->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        } else {
            LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        }
        if (nMaxVersion < walletInstance->GetVersion()) {
            UIError(_("Cannot downgrade wallet."));
            return nullptr;
        }
        walletInstance->SetMaxVersion(nMaxVersion);
    }

    // Upgrade to HD only if explicit upgrade was requested
    // or if we are running an HD wallet and need to upgrade to Sapling.
    if (gArgs.GetBoolArg("-upgradewallet", false) ||
        (!walletInstance->IsLocked() && prev_version == FEATURE_PRE_SPLIT_KEYPOOL)) {
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
            UIError(_("Unable to generate initial key!"));
            return nullptr;
        }

        walletInstance->SetBestChain(WITH_LOCK(cs_main, return chainActive.GetLocator()));
    }

    LogPrintf("Wallet completed loading in %15dms\n", GetTimeMillis() - nStart);

    LOCK(cs_main);
    CBlockIndex* pindexRescan = chainActive.Genesis();

    if (gArgs.GetBoolArg("-rescan", false)) {
        // clear note witness cache before a full rescan
        walletInstance->ClearNoteWitnessCache();
    } else {
        WalletBatch batch(*walletInstance->database);
        CBlockLocator locator;
        if (batch.ReadBestBlock(locator))
            pindexRescan = FindForkInGlobalIndex(chainActive, locator);
    }

    {
        LOCK(walletInstance->cs_wallet);
        const CBlockIndex* tip = chainActive.Tip();
        if (tip) {
            walletInstance->m_last_block_processed = tip->GetBlockHash();
            walletInstance->m_last_block_processed_height = tip->nHeight;
            walletInstance->m_last_block_processed_time = tip->GetBlockTime();
        }
    }
    RegisterValidationInterface(walletInstance);

    if (chainActive.Tip() && chainActive.Tip() != pindexRescan) {
        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);

        // no need to read and scan block, if block was created before
        // our wallet birthday (as adjusted for block time variability)
        while (pindexRescan && walletInstance->nTimeFirstKey &&
                pindexRescan->GetBlockTime() < (walletInstance->nTimeFirstKey - TIMESTAMP_WINDOW)) {
            pindexRescan = chainActive.Next(pindexRescan);
        }
        const int64_t nWalletRescanTime = GetTimeMillis();
        {
            WalletRescanReserver reserver(walletInstance);
            if (!reserver.reserve()) {
                UIError(_("Failed to rescan the wallet during initialization"));
                return nullptr;
            }
            if (walletInstance->ScanForWalletTransactions(pindexRescan, nullptr, reserver, true, true) != nullptr) {
                UIError(_("Shutdown requested over the txs scan. Exiting."));
                return nullptr;
            }
        }
        LogPrintf("Rescan completed in %15dms\n", GetTimeMillis() - nWalletRescanTime);
        walletInstance->SetBestChain(chainActive.GetLocator());
        walletInstance->database->IncrementUpdateCounter();

        // Restore wallet transaction metadata after -zapwallettxes=1
        if (gArgs.GetBoolArg("-zapwallettxes", false) && gArgs.GetArg("-zapwallettxes", "1") != "2") {
            WalletBatch batch(*walletInstance->database);
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
                    batch.WriteTx(*copyTo);
                }
            }
        }
    }

    return walletInstance;
}


std::atomic<bool> CWallet::fFlushScheduled(false);

void CWallet::postInitProcess(CScheduler& scheduler)
{
    // Add wallet transactions that aren't already in a block to mapTransactions
    ReacceptWalletTransactions(/*fFirstLoad*/true);

    // Run a thread to flush wallet periodically
    if (!CWallet::fFlushScheduled.exchange(true)) {
        scheduler.scheduleEvery(MaybeCompactWalletDB, 500);
    }
}

bool CWallet::BackupWallet(const std::string& strDest)
{
    return database->Backup(strDest);
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

int CWalletTx::GetDepthInMainChain() const
{
    assert(pwallet != nullptr);
    AssertLockHeld(pwallet->cs_wallet);
    if (isUnconfirmed() || isAbandoned()) return 0;

    return (pwallet->GetLastBlockHeight() - m_confirm.block_height + 1) * (isConflicted() ? -1 : 1);
}

int CWalletTx::GetBlocksToMaturity() const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    return std::max(0, (Params().GetConsensus().nCoinbaseMaturity + 1) - GetDepthInMainChain());
}

bool CWalletTx::IsInMainChainImmature() const
{
    if (!IsCoinBase() && !IsCoinStake()) return false;
    const int depth = GetDepthInMainChain();
    return (depth > 0 && depth <= Params().GetConsensus().nCoinbaseMaturity);
}


bool CWalletTx::AcceptToMemoryPool(CValidationState& state)
{
    AssertLockHeld(cs_main);

    // Quick check to avoid re-setting fInMempool to false
    if (mempool.exists(tx->GetHash())) {
        return false;
    }

    // We must set fInMempool here - while it will be re-set to true by the
    // entered-mempool callback, if we did not there would be a race where a
    // user could call sendmoney in a loop and hit spurious out of funds errors
    // because we think that the transaction they just generated's change is
    // unavailable as we're not yet aware its in mempool.
    bool fMissingInputs;
    bool fAccepted = ::AcceptToMemoryPool(mempool, state, tx, true, &fMissingInputs, false, true, false);
    fInMempool = fAccepted;
    if (!fAccepted) {
        if (fMissingInputs) {
            // For now, "missing inputs" error is not returning the proper state, so need to set it manually here.
            // TODO: clean this setting the proper invalid state inside AcceptToMemoryPool (btc#15921).
            state.Invalid(false, REJECT_INVALID, "Missing inputs");
        }
        LogPrintf("%s : %s\n", __func__, state.GetRejectReason());
    }
    return fAccepted;
}

std::string CWallet::GetUniqueWalletBackupName() const
{
    std::string name = !m_name.empty() ? SanitizeString(m_name, SAFE_CHARS_FILENAME) : "wallet_backup";
    return strprintf("%s%s", name, FormatISO8601DateTimeForBackup(GetTime()));
}

CWallet::CWallet(std::string name, std::unique_ptr<WalletDatabase> database) : m_name(std::move(name)), database(std::move(database))
{
    SetNull();
}

CWallet::~CWallet()
{
    delete encrypted_batch;
    delete pStakerStatus;
}

void CWallet::SetNull()
{
    nWalletVersion = FEATURE_BASE;
    nWalletMaxVersion = FEATURE_BASE;
    nMasterKeyMaxID = 0;
    encrypted_batch = NULL;
    nOrderPosNext = 0;
    nNextResend = 0;
    nLastResend = 0;
    nTimeFirstKey = 0;
    nRelockTime = 0;
    fAbortRescan = false;
    fScanningWallet = false;
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

    //Auto Combine Dust
    fCombineDust = false;
    nAutoCombineThreshold = 0;

    // Sapling.
    m_sspk_man->nWitnessCacheSize = 0;
    m_sspk_man->nWitnessCacheNeedsUpdate = true;
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

bool CWallet::IsMine(const CTransactionRef& tx) const
{
    for (const CTxOut& txout : tx->vout)
        if (IsMine(txout))
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransactionRef& tx) const
{
    if (GetDebit(tx, ISMINE_ALL) > 0) {
        return true;
    }

    if (tx->IsShieldedTx()) {
        for (const SpendDescription& spend : tx->sapData->vShieldedSpend) {
            if (m_sspk_man->IsSaplingNullifierFromMe(spend.nullifier)) {
                return true;
            }
        }
    }

    return false;
}

CAmount CWallet::GetDebit(const CTransactionRef& tx, const isminefilter& filter) const
{
    CAmount nDebit = 0;
    for (const CTxIn& txin : tx->vin) {
        nDebit += GetDebit(txin, filter);
        if (!Params().GetConsensus().MoneyRange(nDebit))
            throw std::runtime_error("CWallet::GetDebit() : value out of range");
    }

    // Shielded debit
    if (filter & ISMINE_SPENDABLE_SHIELDED || filter & ISMINE_WATCH_ONLY_SHIELDED) {
        if (tx->hasSaplingData()) {
            nDebit += m_sspk_man->GetDebit(*tx, filter);
        }
    }

    return nDebit;
}

CAmount CWallet::GetCredit(const CWalletTx& tx, const isminefilter& filter) const
{
    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx.tx->vout.size(); i++) {
        nCredit += GetCredit(tx.tx->vout[i], filter);
    }

    // Shielded credit
    if (filter & ISMINE_SPENDABLE_SHIELDED || filter & ISMINE_WATCH_ONLY_SHIELDED) {
        if (tx.tx->hasSaplingData()) {
            nCredit += m_sspk_man->GetCredit(tx, filter, false);
        }
    }

    if (!Params().GetConsensus().MoneyRange(nCredit))
        throw std::runtime_error("CWallet::GetCredit() : value out of range");
    return nCredit;
}

CAmount CWallet::GetChange(const CTransactionRef& tx) const
{
    CAmount nChange = 0;
    for (const CTxOut& txout : tx->vout) {
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

libzcash::SaplingPaymentAddress CWallet::GenerateNewSaplingZKey(std::string label) {
    if (!m_sspk_man->IsEnabled()) {
        throw std::runtime_error("Cannot generate shielded addresses. Start with -upgradewallet in order to upgrade a non-HD wallet to HD and Sapling features");
    }

    auto address = m_sspk_man->GenerateNewSaplingZKey();
    SetAddressBook(address, label, AddressBook::AddressBookPurpose::SHIELDED_RECEIVE);
    return address;
}

void CWallet::IncrementNoteWitnesses(const CBlockIndex* pindex,
                            const CBlock* pblock,
                            SaplingMerkleTree& saplingTree) { m_sspk_man->IncrementNoteWitnesses(pindex, pblock, saplingTree); }

void CWallet::DecrementNoteWitnesses(const CBlockIndex* pindex) { m_sspk_man->DecrementNoteWitnesses(pindex->nHeight); }

void CWallet::ClearNoteWitnessCache() { m_sspk_man->ClearNoteWitnessCache(); }

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

CWalletTx::CWalletTx(const CWallet* pwalletIn, CTransactionRef arg)
    : tx(std::move(arg))
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
    fInMempool = false;
    nChangeCached = 0;
    fStakeDelegationVoided = false;
    fShieldedChangeCached = false;
    nShieldedChangeCached = 0;
    nOrderPos = -1;
    m_confirm = Confirmation{};
}

bool CWalletTx::IsTrusted() const
{
    bool fConflicted = false;
    int nDepth = 0;
    return IsTrusted(nDepth, fConflicted);
}

bool CWalletTx::IsTrusted(int& nDepth, bool& fConflicted) const
{
    {
        LOCK(pwallet->cs_wallet); // future: receive block height instead of locking here.
        // Quick answer in most cases
        if (!IsFinalTx(tx, pwallet->GetLastBlockHeight()))
            return false;
    }

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
    for (const CTxIn& txin : tx->vin) {
        // Transactions not sent by us: not trusted
        const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
        if (parent == nullptr)
            return false;
        const CTxOut& parentOut = parent->tx->vout[txin.prevout.n];
        if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
            return false;
    }
    return true;
}

int CWalletTx::GetDepthAndMempool(bool& fConflicted) const
{
    int ret = GetDepthInMainChain();
    fConflicted = (ret == 0 && !InMempool());  // not in chain nor in mempool
    return ret;
}

bool CWalletTx::IsEquivalentTo(const CWalletTx& _tx) const
{
    CMutableTransaction tx1 {*tx};
    CMutableTransaction tx2 {*_tx.tx};
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
    nShieldedChangeCached = 0;
    fShieldedChangeCached = false;
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
        if (nd.first.n < tx->sapData->vShieldedOutput.size()) {
            mapSaplingNoteData[nd.first] = nd.second;
        } else {
            throw std::logic_error("CWalletTx::SetSaplingNoteData(): Invalid note");
        }
    }
}

Optional<std::pair<
        libzcash::SaplingNotePlaintext,
        libzcash::SaplingPaymentAddress>> CWalletTx::DecryptSaplingNote(const SaplingOutPoint& op) const
{
    // Check whether we can decrypt this SaplingOutPoint with the ivk
    auto it = this->mapSaplingNoteData.find(op);
    if (it == this->mapSaplingNoteData.end() || !it->second.IsMyNote()) {
        return nullopt;
    }

    auto output = this->tx->sapData->vShieldedOutput[op.n];
    auto nd = this->mapSaplingNoteData.at(op);

    auto maybe_pt = libzcash::SaplingNotePlaintext::decrypt(
            output.encCiphertext,
            *(nd.ivk),
            output.ephemeralKey,
            output.cmu);
    assert(static_cast<bool>(maybe_pt));
    auto notePt = maybe_pt.get();

    auto maybe_pa = nd.ivk->address(notePt.d);
    assert(static_cast<bool>(maybe_pa));
    auto pa = maybe_pa.get();

    return std::make_pair(notePt, pa);
}

Optional<std::pair<
        libzcash::SaplingNotePlaintext,
        libzcash::SaplingPaymentAddress>> CWalletTx::RecoverSaplingNote(const SaplingOutPoint& op, const std::set<uint256>& ovks) const
{
    auto output = this->tx->sapData->vShieldedOutput[op.n];

    for (const auto& ovk : ovks) {
        auto outPt = libzcash::SaplingOutgoingPlaintext::decrypt(
                output.outCiphertext,
                ovk,
                output.cv,
                output.cmu,
                output.ephemeralKey);
        if (!outPt) {
            continue;
        }

        auto maybe_pt = libzcash::SaplingNotePlaintext::decrypt(
                output.encCiphertext,
                output.ephemeralKey,
                outPt->esk,
                outPt->pk_d,
                output.cmu);
        assert(static_cast<bool>(maybe_pt));
        auto notePt = maybe_pt.get();

        return std::make_pair(notePt, libzcash::SaplingPaymentAddress(notePt.d, outPt->pk_d));
    }

    // Couldn't recover with any of the provided OutgoingViewingKeys
    return nullopt;
}

bool CWalletTx::HasP2CSInputs() const
{
    return GetStakeDelegationDebit(true) > 0 || GetColdStakingDebit(true) > 0;
}

CAmount CWalletTx::GetChange() const
{
    if (fChangeCached)
        return nChangeCached;
    nChangeCached = pwallet->GetChange(tx);
    fChangeCached = true;
    return nChangeCached;
}

CAmount CWalletTx::GetShieldedChange() const
{
    if (fShieldedChangeCached) {
        return nShieldedChangeCached;
    }
    nShieldedChangeCached = pwallet->GetSaplingScriptPubKeyMan()->GetShieldedChange(*this);
    fShieldedChangeCached = true;
    return nShieldedChangeCached;
}

bool CWalletTx::IsFromMe(const isminefilter& filter) const
{
    return (GetDebit(filter) > 0);
}

CAmount CWalletTx::GetShieldedAvailableCredit(bool fUseCache) const
{
    return GetAvailableCredit(fUseCache, ISMINE_SPENDABLE_SHIELDED);
}

const CTxDestination* CAddressBookIterator::GetCTxDestKey()
{
    return boost::get<CTxDestination>(&it->first);
}

const libzcash::SaplingPaymentAddress* CAddressBookIterator::GetShieldedDestKey()
{
    return boost::get<libzcash::SaplingPaymentAddress>(&it->first);
}

const CWDestination* CAddressBookIterator::GetDestKey()
{
    return &it->first;
}

CStakeableOutput::CStakeableOutput(const CWalletTx* txIn,
                                   int iIn,
                                   int nDepthIn,
                                   const CBlockIndex*& _pindex) :
                       COutput(txIn, iIn, nDepthIn, true /*fSpendable*/, true/*fSolvable*/, true/*fSafe*/),
                       pindex(_pindex)
{}
