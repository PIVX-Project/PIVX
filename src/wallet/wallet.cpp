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
bool bSpendZeroConfChange = true;
bool bdisableSystemnotifications = false; // Those bubbles can be annoying and slow down the UI when you get lots of trx
bool fSendFreeTransactions = false;
bool fPayAtLeastCustomFee = true;

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

bool CWallet::SetupSPKM()
{
    return m_spk_man->SetupGeneration();
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

    ret = DestinationFor(keyID, addrType);
    return PairResult(true);
}

int64_t CWallet::GetKeyCreationTime(CPubKey pubkey)
{
    return mapKeyMetadata[pubkey.GetID()].nCreateTime;
}

int64_t CWallet::GetKeyCreationTime(const CBitcoinAddress& address)
{
    CKeyID keyID;
    if (address.GetKeyID(keyID)) {
        CPubKey keyRet;
        if (GetPubKey(keyID, keyRet)) {
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
        if (zwalletMain) zwalletMain->Lock();
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
            LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.");
            throw std::runtime_error("Error unlocking wallet: some keys decrypt but not all. Your wallet file may be corrupt.");
        }
        if (keyFail || !keyPass)
            return false;
        vMasterKey = vMasterKeyIn;
        fDecryptionThoroughlyChecked = true;

        if (zwalletMain) {
            uint256 hashSeed;
            if (CWalletDB(strWalletFile).ReadCurrentSeedHash(hashSeed)) {
                uint256 nSeed;
                if (!GetDeterministicSeed(hashSeed, nSeed)) {
                    return error("Failed to read zPIV seed from DB. Wallet is probably corrupt.");
                }
                zwalletMain->SetMasterSeed(nSeed, false);
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

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteBestBlock(loc);
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
    return result;
}

void CWallet::SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = nullptr;
    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
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
    for (TxSpends::iterator it = range.first; it != range.second; ++it) {
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
        copyTo->strFromAccount = copyFrom->strFromAccount;
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
    bSpendZeroConfChange = GetBoolArg("-spendzeroconfchange", false);
    bdisableSystemnotifications = GetBoolArg("-disablesystemnotifications", false);
    fSendFreeTransactions = GetBoolArg("-sendfreetransactions", false);

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
    mapTxSpends.insert(std::make_pair(outpoint, wtxid));
    setLockedCoins.erase(outpoint);

    std::pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData(range);
}


void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    for (const CTxIn& txin : thisTx.vin)
        AddToSpends(txin.prevout, wtxid);
}

bool CWallet::GetMasternodeVinAndKeys(CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, std::string strTxHash, std::string strOutputIndex)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    // Find possible candidates (remove delegated)
    std::vector<COutput> vPossibleCoins;
    AvailableCoins(&vPossibleCoins,
            nullptr,        // coin control
            false,          // fIncludeDelegated
            false,          // fIncludeColdStaking
            ONLY_10000);    // coin type

    if (vPossibleCoins.empty()) {
        LogPrintf("CWallet::GetMasternodeVinAndKeys -- Could not locate any valid masternode vin\n");
        return false;
    }

    if (strTxHash.empty()) // No output specified, select the first one
        return GetVinAndKeysFromOutput(vPossibleCoins[0], txinRet, pubKeyRet, keyRet);

    // Find specific vin
    uint256 txHash = uint256S(strTxHash);

    int nOutputIndex;
    try {
        nOutputIndex = std::stoi(strOutputIndex.c_str());
    } catch (const std::exception& e) {
        LogPrintf("%s: %s on strOutputIndex\n", __func__, e.what());
        return false;
    }

    for (COutput& out : vPossibleCoins)
        if (out.tx->GetHash() == txHash && out.i == nOutputIndex) // found it!
            return GetVinAndKeysFromOutput(out, txinRet, pubKeyRet, keyRet);

    LogPrintf("CWallet::GetMasternodeVinAndKeys -- Could not locate specified masternode vin\n");
    return false;
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
    CBitcoinAddress address2(address1);

    CKeyID keyID;
    if (!address2.GetKeyID(keyID)) {
        LogPrintf("CWallet::GetVinAndKeysFromOutput -- Address does not refer to a key\n");
        return false;
    }

    if (!GetKey(keyID, keyRet)) {
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

        if (!EncryptKeys(vMasterKey)) {
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
            if (!m_spk_man->SetupGeneration(true)) {
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

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for (PAIRTYPE(const uint256, CWalletTx) & item : mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb)
{
    uint256 hash = wtxIn.GetHash();

    if (fFromLoadWallet) {
        mapWallet[hash] = wtxIn;
        CWalletTx& wtx = mapWallet[hash];
        wtx.BindWallet(this);
        wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));
        AddToSpends(hash);
    } else {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        std::pair<std::map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(std::make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew) {
            wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext(pwalletdb);
            wtxOrdered.insert(std::make_pair(wtx.nOrderPos, TxPair(&wtx, (CAccountingEntry*)0)));
            wtx.UpdateTimeSmart();
            AddToSpends(hash);
            for (const CTxIn& txin : wtx.vin) {
                if (mapWallet.count(txin.prevout.hash)) {
                    CWalletTx& prevtx = mapWallet[txin.prevout.hash];
                    if (prevtx.nIndex == -1 && !prevtx.hashUnset()) {
                        MarkConflicted(prevtx.hashBlock, wtx.GetHash());
                    }
                }
            }
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
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe) {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
        }

        //// debug print
        LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk(pwalletdb))
                return false;

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
    }
    return true;
}

/**
 * Add a transaction to the wallet, or update it.
 * pblock is optional, but should be provided if the transaction is known to be in a block.
 * If fUpdate is true, existing transactions will be updated.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate)
{
    {
        AssertLockHeld(cs_wallet);

        if (pblock && !tx.HasZerocoinSpendInputs() && !tx.IsCoinBase()) {
            for (const CTxIn& txin : tx.vin) {
                std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range = mapTxSpends.equal_range(txin.prevout);
                while (range.first != range.second) {
                    if (range.first->second != tx.GetHash()) {
                        LogPrintf("Transaction %s (in block %s) conflicts with wallet transaction %s (both spend %s:%i)\n", tx.GetHash().ToString(), pblock->GetHash().ToString(), range.first->second.ToString(), range.first->first.hash.ToString(), range.first->first.n);
                        MarkConflicted(pblock->GetHash(), range.first->second);
                    }
                    range.first++;
                }
            }
        }

        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;
        if (fExisted || IsMine(tx) || IsFromMe(tx)) {

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
            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(*pblock);
            // Do not flush the wallet here for performance reasons
            // this is safe, as in case of a crash, we rescan the necessary blocks on startup through our SetBestChain-mechanism
            CWalletDB walletdb(strWalletFile, "r+", false);

            return AddToWallet(wtx, false, &walletdb);
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
            wtx.WriteToDisk(&walletdb);
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

    CBlockIndex* pindex;
    assert(mapBlockIndex.count(hashBlock));
    pindex = mapBlockIndex[hashBlock];
    int conflictconfirms = 0;
    if (chainActive.Contains(pindex)) {
        conflictconfirms = -(chainActive.Height() - pindex->nHeight + 1);
    }
    assert(conflictconfirms < 0);

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
            wtx.WriteToDisk(&walletdb);
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

void CWallet::SyncTransaction(const CTransaction& tx, const CBlock* pblock)
{
    LOCK(cs_wallet);
    if (!AddToWalletIfInvolvingMe(tx, pblock, true))
        return; // Not one of ours

    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    for (const CTxIn& txin : tx.vin) {
        if (!txin.IsZerocoinSpend() && mapWallet.count(txin.prevout.hash))
            mapWallet[txin.prevout.hash].MarkDirty();
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

bool CWallet::IsUsed(const CBitcoinAddress address) const
{
    LOCK(cs_wallet);
    CScript scriptPubKey = GetScriptForDestination(address.Get());
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
        if (!mapAddressBook.count(address))
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

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase()) {
            // Generated block
            if (!hashUnset()) {
                std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        } else {
            // Did anyone request this transaction?
            std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end()) {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && !hashUnset()) {
                    std::map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

//! filter decides which addresses will count towards the debit
CAmount CWalletTx::GetDebit(const isminefilter& filter) const
{
    if (vin.empty())
        return 0;

    CAmount debit = 0;
    if (filter & ISMINE_SPENDABLE) {
        if (fDebitCached)
            debit += nDebitCached;
        else {
            nDebitCached = pwallet->GetDebit(*this, ISMINE_SPENDABLE);
            fDebitCached = true;
            debit += nDebitCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY) {
        if (fWatchDebitCached)
            debit += nWatchDebitCached;
        else {
            nWatchDebitCached = pwallet->GetDebit(*this, ISMINE_WATCH_ONLY);
            fWatchDebitCached = true;
            debit += nWatchDebitCached;
        }
    }
    if (filter & ISMINE_COLD) {
        if (fColdDebitCached)
            debit += nColdDebitCached;
        else {
            nColdDebitCached = pwallet->GetDebit(*this, ISMINE_COLD);
            fColdDebitCached = true;
            debit += nColdDebitCached;
        }
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        if (fDelegatedDebitCached)
            debit += nDelegatedDebitCached;
        else {
            nDelegatedDebitCached = pwallet->GetDebit(*this, ISMINE_SPENDABLE_DELEGATED);
            fDelegatedDebitCached = true;
            debit += nDelegatedDebitCached;
        }
    }
    return debit;
}

CAmount CWalletTx::GetColdStakingDebit(bool fUseCache) const
{
    return UpdateAmount(nColdDebitCached, fColdDebitCached, fUseCache, ISMINE_COLD, false);
}

CAmount CWalletTx::GetStakeDelegationDebit(bool fUseCache) const
{
    return UpdateAmount(nDelegatedDebitCached, fDelegatedDebitCached, fUseCache, ISMINE_SPENDABLE_DELEGATED, false);
}

CAmount CWalletTx::GetUnspentCredit(const isminefilter& filter) const
{
    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (GetBlocksToMaturity() > 0)
        return 0;

    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE) {
        credit += pwallet->GetCredit(*this, ISMINE_SPENDABLE, true);
    }
    if (filter & ISMINE_WATCH_ONLY) {
        credit += pwallet->GetCredit(*this, ISMINE_WATCH_ONLY, true);
    }
    if (filter & ISMINE_COLD) {
        credit += pwallet->GetCredit(*this, ISMINE_COLD, true);
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        credit += pwallet->GetCredit(*this, ISMINE_SPENDABLE_DELEGATED, true);
    }
    return credit;
}

CAmount CWalletTx::GetCredit(const isminefilter& filter) const
{
    CAmount credit = 0;
    if (filter & ISMINE_SPENDABLE) {
        // GetBalance can assume transactions in mapWallet won't change
        if (fCreditCached)
            credit += nCreditCached;
        else {
            nCreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE, false);
            fCreditCached = true;
            credit += nCreditCached;
        }
    }
    if (filter & ISMINE_WATCH_ONLY) {
        if (fWatchCreditCached)
            credit += nWatchCreditCached;
        else {
            nWatchCreditCached = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY, false);
            fWatchCreditCached = true;
            credit += nWatchCreditCached;
        }
    }
    if (filter & ISMINE_COLD) {
        if (fColdCreditCached)
            credit += nColdCreditCached;
        else {
            nColdCreditCached = pwallet->GetCredit(*this, ISMINE_COLD, false);
            fColdCreditCached = true;
            credit += nColdCreditCached;
        }
    }
    if (filter & ISMINE_SPENDABLE_DELEGATED) {
        if (fDelegatedCreditCached)
            credit += nDelegatedCreditCached;
        else {
            nDelegatedCreditCached = pwallet->GetCredit(*this, ISMINE_SPENDABLE_DELEGATED, false);
            fDelegatedCreditCached = true;
            credit += nDelegatedCreditCached;
        }
    }
    return credit;
}

CAmount CWalletTx::GetImmatureCredit(bool fUseCache, const isminefilter& filter) const
{
    LOCK(cs_main);
    if (IsInMainChainImmature()) {
        if (fUseCache && fImmatureCreditCached && filter == ISMINE_SPENDABLE_ALL)
            return nImmatureCreditCached;
        nImmatureCreditCached = pwallet->GetCredit(*this, filter);
        fImmatureCreditCached = true;
        return nImmatureCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetAvailableCredit(bool fUseCache) const
{
    return GetUnspentCredit(ISMINE_SPENDABLE_ALL);
}

CAmount CWalletTx::GetColdStakingCredit(bool fUseCache) const
{
    return GetUnspentCredit(ISMINE_COLD);
}

CAmount CWalletTx::GetStakeDelegationCredit(bool fUseCache) const
{
    return GetUnspentCredit(ISMINE_SPENDABLE_DELEGATED);
}

CAmount CWalletTx::UpdateAmount(CAmount& amountToUpdate, bool& cacheFlagToUpdate, bool fUseCache, isminetype mimeType, bool fCredit) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && cacheFlagToUpdate)
        return amountToUpdate;

    amountToUpdate = (fCredit) ? GetCredit(mimeType) : GetDebit(mimeType);
    cacheFlagToUpdate = true;
    return amountToUpdate;
}

// Return sum of unlocked coins
CAmount CWalletTx::GetUnlockedCredit() const
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

        if (pwallet->IsSpent(hashTx, i) || pwallet->IsLockedCoin(hashTx, i)) continue;
        if (fMasterNode && vout[i].nValue == 10000 * COIN) continue; // do not count MN-like outputs

        nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
        if (!Params().GetConsensus().MoneyRange(nCredit))
            throw std::runtime_error("CWalletTx::GetUnlockedCredit() : value out of range");
    }

    return nCredit;
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
        if (fUseCache && fImmatureWatchCreditCached)
            return nImmatureWatchCreditCached;
        nImmatureWatchCreditCached = pwallet->GetCredit(*this, ISMINE_WATCH_ONLY);
        fImmatureWatchCreditCached = true;
        return nImmatureWatchCreditCached;
    }

    return 0;
}

CAmount CWalletTx::GetAvailableWatchOnlyCredit(const bool& fUseCache) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && fAvailableWatchCreditCached)
        return nAvailableWatchCreditCached;

    CAmount nCredit = 0;
    for (unsigned int i = 0; i < vout.size(); i++) {
        if (!pwallet->IsSpent(GetHash(), i)) {
            const CTxOut& txout = vout[i];
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY);
            if (!Params().GetConsensus().MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
        }
    }

    nAvailableWatchCreditCached = nCredit;
    fAvailableWatchCreditCached = true;
    return nCredit;
}

CAmount CWalletTx::GetLockedWatchOnlyCredit() const
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
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY);
        }

        // Add masternode collaterals which are handled likc locked coins
        else if (fMasterNode && vout[i].nValue == 10000 * COIN) {
            nCredit += pwallet->GetCredit(txout, ISMINE_WATCH_ONLY);
        }

        if (!Params().GetConsensus().MoneyRange(nCredit))
            throw std::runtime_error("CWalletTx::GetLockedCredit() : value out of range");
    }

    return nCredit;
}

void CWalletTx::GetAmounts(std::list<COutputEntry>& listReceived,
    std::list<COutputEntry>& listSent,
    CAmount& nFee,
    std::string& strSentAccount,
    const isminefilter& filter) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Compute fee:
    CAmount nDebit = GetDebit(filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
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

void CWalletTx::GetAccountAmounts(const std::string& strAccount, CAmount& nReceived, CAmount& nSent, CAmount& nFee, const isminefilter& filter) const
{
    nReceived = nSent = nFee = 0;

    CAmount allFee;
    std::string strSentAccount;
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;
    GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);

    if (strAccount == strSentAccount) {
        for (const COutputEntry& s : listSent)
            nSent += s.amount;
        nFee = allFee;
    }
    {
        LOCK(pwallet->cs_wallet);
        for (const COutputEntry& r : listReceived) {
            if (pwallet->mapAddressBook.count(r.destination)) {
                std::map<CTxDestination, AddressBook::CAddressBookData>::const_iterator mi = pwallet->mapAddressBook.find(r.destination);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second.name == strAccount)
                    nReceived += r.amount;
            } else if (strAccount.empty()) {
                nReceived += r.amount;
            }
        }
    }
}


bool CWalletTx::WriteToDisk(CWalletDB *pwalletdb)
{
    if (pwalletdb)
        return pwalletdb->WriteTx(GetHash(), *this);
    return CWalletDB(pwallet->strWalletFile).WriteTx(GetHash(), *this);
}

bool CWallet::Upgrade(std::string& error, const int& prevVersion)
{
    LOCK(cs_wallet);
    // Do not upgrade versions if we are already in the last one
    if (prevVersion >= FEATURE_PRE_SPLIT_KEYPOOL) {
        error = strprintf(_("Cannot upgrade to HD wallet (already running HD support). Version: %d"), prevVersion);
        return false;
    }
    return m_spk_man->Upgrade(prevVersion, error);
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
            for (CTransaction& tx : block.vtx) {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                    ret++;
            }

            //If this is a zapwallettx, need to readd zpiv
            if (fCheckZPIV && consensus.NetworkUpgradeActive(pindex->nHeight, Consensus::UPGRADE_ZC)) {
                std::list<CZerocoinMint> listMints;
                BlockToZerocoinMintList(block, listMints, true);
                CWalletDB walletdb(strWalletFile);

                for (auto& m : listMints) {
                    if (IsMyMint(m.GetValue())) {
                        LogPrint(BCLog::LEGACYZC, "%s: found mint\n", __func__);
                        UpdateMint(m.GetValue(), pindex->nHeight, m.GetTxHash(), m.GetDenomination());

                        // Add the transaction to the wallet
                        for (auto& tx : block.vtx) {
                            uint256 txid = tx.GetHash();
                            if (setAddedToWallet.count(txid) || mapWallet.count(txid))
                                continue;
                            if (txid == m.GetTxHash()) {
                                CWalletTx wtx(this, tx);
                                wtx.nTimeReceived = block.GetBlockTime();
                                wtx.SetMerkleBranch(block);
                                AddToWallet(wtx, false, &walletdb);
                                setAddedToWallet.insert(txid);
                            }
                        }

                        //Check if the mint was ever spent
                        int nHeightSpend = 0;
                        uint256 txidSpend;
                        CTransaction txSpend;
                        if (IsSerialInBlockchain(GetSerialHash(m.GetSerialNumber()), nHeightSpend, txidSpend, txSpend)) {
                            if (setAddedToWallet.count(txidSpend) || mapWallet.count(txidSpend))
                                continue;

                            CWalletTx wtx(this, txSpend);
                            CBlockIndex* pindexSpend = chainActive[nHeightSpend];
                            CBlock blockSpend;
                            if (ReadBlockFromDisk(blockSpend, pindexSpend))
                                wtx.SetMerkleBranch(blockSpend);

                            wtx.nTimeReceived = pindexSpend->nTime;
                            AddToWallet(wtx, false, &walletdb);
                            setAddedToWallet.emplace(txidSpend);
                        }
                    }
                }
            }

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
    for (PAIRTYPE(const uint256, CWalletTx)& item: mapWallet) {
        const uint256& wtxid = item.first;
        CWalletTx& wtx = item.second;
        assert(wtx.GetHash() == wtxid);

        int nDepth = wtx.GetDepthInMainChain();
        if (!wtx.IsCoinBase() && !wtx.IsCoinStake() && nDepth == 0  && !wtx.isAbandoned()) {
            mapSorted.insert(std::make_pair(wtx.nOrderPos, &wtx));
        }
    }

    // Try to add wallet transactions to memory pool
    for (PAIRTYPE(const int64_t, CWalletTx*)& item: mapSorted)
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

void CWalletTx::RelayWalletTransaction(std::string strCommand)
{
    LOCK(cs_main);
    if (!IsCoinBase() && !IsCoinStake()) {
        if (GetDepthInMainChain() == 0 && !isAbandoned()) {
            uint256 hash = GetHash();
            LogPrintf("Relaying wtx %s\n", hash.ToString());

            if (strCommand == NetMsgType::IX) {
                mapTxLockReq.insert(std::make_pair(hash, (CTransaction) * this));
                CreateNewLock(((CTransaction) * this));
                RelayTransactionLockReq((CTransaction) * this, true);
            } else {
                RelayTransaction((CTransaction) * this);
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

void CWallet::ResendWalletTransactions()
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
        for (PAIRTYPE(const uint256, CWalletTx) & item : mapWallet) {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60)
                mapSorted.insert(std::make_pair(wtx.nTimeReceived, &wtx));
        }
        for (PAIRTYPE(const unsigned int, CWalletTx*) & item : mapSorted) {
            CWalletTx& wtx = *item.second;
            wtx.RelayWalletTransaction();
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

CAmount CWallet::GetBalance(bool fIncludeDelegated) const
{
    return loopTxsBalance([fIncludeDelegated](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal){
        if (pcoin.IsTrusted()) {
            nTotal += pcoin.GetAvailableCredit();
            if (!fIncludeDelegated) nTotal -= pcoin.GetStakeDelegationCredit();
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

CAmount CWallet::GetUnlockedCoins() const
{
    if (fLiteMode) return 0;

    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (pcoin.IsTrusted() && pcoin.GetDepthInMainChain() > 0)
                nTotal += pcoin.GetUnlockedCredit();
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

CAmount CWallet::GetLockedWatchOnlyBalance() const
{
    return loopTxsBalance([](const uint256& id, const CWalletTx& pcoin, CAmount& nTotal) {
            if (pcoin.IsTrusted() && pcoin.GetDepthInMainChain() > 0)
                nTotal += pcoin.GetLockedWatchOnlyCredit();
    });
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
                            // Depth is not used, no need waste resources and set it for now.
                            vCoins.emplace_back(COutput(pcoin, i, 0, isMineSpendable));
                    }
                }
            }
        }
    }

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
                             bool fIncludeZeroValue,            // Default: false
                             bool fUseIX,                       // Default: false
                             int nWatchonlyConfig               // Default: 1
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

            if (!CheckFinalTx(*pcoin)) continue;
            if (fOnlyConfirmed && !pcoin->IsTrusted()) continue;
            if (pcoin->GetBlocksToMaturity() > 0) continue;

            int nDepth = pcoin->GetDepthInMainChain(false);
            // do not use IX for inputs that have less then 6 blockchain confirmations
            if (fUseIX && nDepth < 6) continue;

            // We should not consider coins which aren't at least in our mempool
            // It's possible for these to be conflicted via ancestors which we may never be able to detect
            if (nDepth == 0 && !pcoin->InMempool()) continue;

            // Check min depth requirement for stake inputs
            if (nCoinType == STAKEABLE_COINS && nDepth < Params().GetConsensus().nStakeMinDepth) continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                bool found = false;
                if (nCoinType == ONLY_10000) {
                    found = pcoin->vout[i].nValue == 10000 * COIN;
                } else {
                    found = true;
                }
                if (!found) continue;

                if (nCoinType == STAKEABLE_COINS && pcoin->vout[i].IsZerocoinMint()) continue;
                if (IsSpent(wtxid, i)) continue;

                isminetype mine = IsMine(pcoin->vout[i]);
                if (  (mine == ISMINE_NO) ||
                      (mine == ISMINE_SPENDABLE && nWatchonlyConfig == 2) ||
                      (mine == ISMINE_WATCH_ONLY && nWatchonlyConfig == 1) ||
                      (IsLockedCoin((*it).first, i) && nCoinType != ONLY_10000) ||
                      (pcoin->vout[i].nValue <= 0 && !fIncludeZeroValue) ||
                      (fCoinsSelected && !coinControl->fAllowOtherInputs && !coinControl->IsSelected(COutPoint((*it).first, i)))
                   ) continue;

                // --Skip P2CS outputs
                // skip cold coins
                if (mine == ISMINE_COLD && (!fIncludeColdStaking || !HasDelegator(pcoin->vout[i]))) continue;
                // skip delegated coins
                if (mine == ISMINE_SPENDABLE_DELEGATED && !fIncludeDelegated) continue;
                // skip auto-delegated coins
                if (mine == ISMINE_SPENDABLE_STAKEABLE && !fIncludeColdStaking && !fIncludeDelegated) continue;

                bool fIsValid = (
                        ((mine & ISMINE_SPENDABLE) != ISMINE_NO) ||
                        (coinControl && coinControl->fAllowWatchOnly && (mine & ISMINE_WATCH_SOLVABLE) != ISMINE_NO) ||
                        ((mine & ((fIncludeColdStaking ? ISMINE_COLD : ISMINE_NO) |
                                (fIncludeDelegated ? ISMINE_SPENDABLE_DELEGATED : ISMINE_NO) )) != ISMINE_NO));

                // found valid coin
                if (!pCoins) return true;
                pCoins->emplace_back(COutput(pcoin, i, nDepth, fIsValid));
            }
        }
        return (pCoins && pCoins->size() > 0);
    }
}

std::map<CBitcoinAddress, std::vector<COutput> > CWallet::AvailableCoinsByAddress(bool fConfirmed, CAmount maxCoinValue)
{
    std::vector<COutput> vCoins;
    // include cold
    AvailableCoins(&vCoins,
            nullptr,            // coin control
            true,               // fIncludeDelegated
            true,               // fIncludeColdStaking
            ALL_COINS,          // coin type
            fConfirmed);        // only confirmed

    std::map<CBitcoinAddress, std::vector<COutput> > mapCoins;
    for (COutput out : vCoins) {
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

        mapCoins[CBitcoinAddress(address, fColdStakeAddr ? CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS)].push_back(out);
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


bool CWallet::StakeableCoins(std::vector<COutput>* pCoins)
{
    const bool fIncludeCold = (sporkManager.IsSporkActive(SPORK_17_COLDSTAKING_ENFORCEMENT) &&
                               GetBoolArg("-coldstaking", true));

    return AvailableCoins(pCoins,
            nullptr,            // coin control
            false,              // fIncludeDelegated
            fIncludeCold,       // fIncludeColdStaking
            STAKEABLE_COINS);  // coin type
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

bool CWallet::SelectCoinsToSpend(const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl, AvailableCoinsType coin_type, bool useIX, bool fIncludeColdStaking, bool fIncludeDelegated) const
{
    // Note: this function should never be used for "always free" tx types like dstx
    std::vector<COutput> vCoins;
    AvailableCoins(&vCoins,
            coinControl,
            fIncludeDelegated,
            fIncludeColdStaking,
            coin_type,
            true,                   // fOnlyConfirmed
            false,                  // fIncludeZeroValue
            useIX);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected() && !coinControl->fAllowOtherInputs) {
        for (const COutput& out : vCoins) {
            if (!out.fSpendable)
                continue;

            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(std::make_pair(out.tx, out.i));
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
            setPresetCoins.insert(std::make_pair(pcoin, outpoint.n));
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

bool CWallet::GetBudgetSystemCollateralTX(CWalletTx& tx, uint256 hash, bool useIX)
{
    // make our change address
    CReserveKey reservekey(this);

    CScript scriptChange;
    scriptChange << OP_RETURN << ToByteVector(hash);

    CAmount nFeeRet = 0;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;
    vecSend.push_back(CRecipient{scriptChange, BUDGET_FEE_TX_OLD, false}); // Old 50 PIV collateral

    CCoinControl* coinControl = NULL;
    int nChangePosInOut = -1;
    bool success = CreateTransaction(vecSend, tx, reservekey, nFeeRet, nChangePosInOut, strFail, coinControl, ALL_COINS, true, useIX, (CAmount)0);
    if (!success) {
        LogPrintf("GetBudgetSystemCollateralTX: Error - %s\n", strFail);
        return false;
    }

    return true;
}

bool CWallet::GetBudgetFinalizationCollateralTX(CWalletTx& tx, uint256 hash, bool useIX)
{
    // make our change address
    CReserveKey reservekey(this);

    CScript scriptChange;
    scriptChange << OP_RETURN << ToByteVector(hash);

    CAmount nFeeRet = 0;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;
    vecSend.push_back(CRecipient{scriptChange, BUDGET_FEE_TX, false}); // New 5 PIV collateral

    CCoinControl* coinControl = NULL;
    int nChangePosInOut = -1;
    bool success = CreateTransaction(vecSend, tx, reservekey, nFeeRet, nChangePosInOut, strFail, coinControl, ALL_COINS, true, useIX, (CAmount)0);
    if (!success) {
        LogPrintf("GetBudgetSystemCollateralTX: Error - %s\n", strFail);
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
                                txNew.vout.push_back(CTxOut((rec.nAmount / nSplitBlock) + nRemainder, rec.scriptPubKey));
                            } else
                                txNew.vout.push_back(CTxOut(rec.nAmount / nSplitBlock, rec.scriptPubKey));
                        }
                    }
                }

                // Choose coins to use
                std::set<std::pair<const CWalletTx*, unsigned int> > setCoins;
                CAmount nValueIn = 0;

                if (!SelectCoinsToSpend(nTotalValue, setCoins, nValueIn, coinControl, coin_type, useIX, false, fIncludeDelegated)) {
                    if (coin_type == ALL_COINS) {
                        strFailReason = _("Insufficient funds.");
                    }

                    if (useIX) {
                        strFailReason += " " + _("SwiftX requires inputs with at least 6 confirmations, you might need to wait a few minutes and try again.");
                    }

                    return false;
                }


                for (PAIRTYPE(const CWalletTx*, unsigned int) pcoin : setCoins) {
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
                    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange)) {
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
                for (const PAIRTYPE(const CWalletTx*, unsigned int) & coin : setCoins)
                    txNew.vin.push_back(CTxIn(coin.first->GetHash(), coin.second));

                // Sign
                int nIn = 0;
                CTransaction txNewConst(txNew);
                for (const PAIRTYPE(const CWalletTx*, unsigned int) & coin : setCoins) {
                    bool signSuccess;
                    const CScript& scriptPubKey = coin.first->vout[coin.second].scriptPubKey;
                    CScript& scriptSigRes = txNew.vin[nIn].scriptSig;
                    if (sign)
                        signSuccess = ProduceSignature(TransactionSignatureCreator(this, &txNewConst, nIn, SIGHASH_ALL), scriptPubKey, scriptSigRes, false);
                    else
                        signSuccess = ProduceSignature(DummySignatureCreator(this), scriptPubKey, scriptSigRes, false);

                    if (!signSuccess) {
                        strFailReason = _("Signing transaction failed");
                        return false;
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
                    double dPriorityNeeded = mempool.estimatePriority(nTxConfirmTarget);
                    // Not enough mempool history to estimate: use hard-coded AllowFree.
                    if (dPriorityNeeded <= 0 && AllowFree(dPriority))
                        break;

                    // Small enough, and priority high enough, to send for free
                    if (dPriorityNeeded > 0 && dPriority >= dPriorityNeeded)
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
    vecSend.push_back(CRecipient{scriptPubKey, nValue, false});
    int nChangePosInOut = -1;
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePosInOut, strFailReason, coinControl, coin_type, true, useIX, nFeePay, fIncludeDelegated);
}

bool CWallet::CreateCoinStake(
        const CKeyStore& keystore,
        const CBlockIndex* pindexPrev,
        unsigned int nBits,
        CMutableTransaction& txNew,
        int64_t& nTxNewTime
        )
{
    // Get the list of stakable utxos
    std::vector<COutput> vCoins;
    if (!StakeableCoins(&vCoins)) {
        LogPrintf("%s: No coin available to stake.\n", __func__);
        return false;
    }

    // Parse utxos into CPivStakes
    std::list<std::unique_ptr<CStakeInput> > listInputs;
    for (const COutput &out : vCoins) {
        std::unique_ptr<CPivStake> input(new CPivStake());
        input->SetPrevout((CTransaction) *out.tx, out.i);
        listInputs.emplace_back(std::move(input));
    }

    const Consensus::Params& consensus = Params().GetConsensus();

    // Mark coin stake transaction
    txNew.vin.clear();
    txNew.vout.clear();
    txNew.vout.push_back(CTxOut(0, CScript()));

    // update staker status (hash)
    pStakerStatus->SetLastTip(pindexPrev);
    pStakerStatus->SetLastCoins(listInputs.size());

    // Kernel Search
    CAmount nCredit;
    CScript scriptPubKeyKernel;
    bool fKernelFound = false;
    int nAttempts = 0;
    for (std::unique_ptr<CStakeInput>& stakeInput : listInputs) {
        //new block came in, move on
        if (chainActive.Height() != pindexPrev->nHeight) return false;

        // Make sure the wallet is unlocked and shutdown hasn't been requested
        if (IsLocked() || ShutdownRequested()) return false;

        // This should never happen
        if (stakeInput->IsZPIV()) {
            LogPrintf("%s: ERROR - zPOS is disabled\n", __func__);
            continue;
        }

        nCredit = 0;

        nAttempts++;
        fKernelFound = Stake(pindexPrev, stakeInput.get(), nBits, nTxNewTime);

        // update staker status (time, attempts)
        pStakerStatus->SetLastTime(nTxNewTime);
        pStakerStatus->SetLastTries(nAttempts);

        if (!fKernelFound) continue;

        // Found a kernel
        LogPrintf("CreateCoinStake : kernel found\n");
        nCredit += stakeInput->GetValue();

        // Calculate reward
        CAmount nReward;
        nReward = GetBlockValue(chainActive.Height() + 1);
        nCredit += nReward;

        // Create the output transaction(s)
        std::vector<CTxOut> vout;
        if (!stakeInput->CreateTxOuts(this, vout, nCredit)) {
            LogPrintf("%s : failed to create output\n", __func__);
            continue;
        }
        txNew.vout.insert(txNew.vout.end(), vout.begin(), vout.end());

        CAmount nMinFee = 0;
        // Set output amount
        int outputs = txNew.vout.size() - 1;
        CAmount nRemaining = nCredit - nMinFee;
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
        FillBlockPayee(txNew, nMinFee, true, false);

        uint256 hashTxOut = txNew.GetHash();
        CTxIn in;
        if (!stakeInput->CreateTxIn(this, in, hashTxOut)) {
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

    // Sign for PIV
    int nIn = 0;
    if (!txNew.vin[0].scriptSig.IsZerocoinSpend()) {
        for (CTxIn txIn : txNew.vin) {
            const CWalletTx *wtx = GetWalletTx(txIn.prevout.hash);
            if (!SignSignature(*this, *wtx, txNew, nIn++, SIGHASH_ALL, true))
                return error("CreateCoinStake : failed to sign coinstake");
        }
    } else {
        //Update the mint database with tx hash and height
        for (const CTxOut& out : txNew.vout) {
            if (!out.IsZerocoinMint())
                continue;

            libzerocoin::PublicCoin pubcoin(consensus.Zerocoin_Params(false));
            CValidationState state;
            if (!TxOutToPublicCoin(out, pubcoin, state))
                return error("%s: extracting pubcoin from txout failed", __func__);

            uint256 hashPubcoin = GetPubCoinHash(pubcoin.getValue());
            if (!zpivTracker->HasPubcoinHash(hashPubcoin))
                return error("%s: could not find pubcoinhash %s in tracker", __func__, hashPubcoin.GetHex());

            CMintMeta meta = zpivTracker->GetMetaFromPubcoin(hashPubcoin);
            meta.txid = txNew.GetHash();
            meta.nHeight = chainActive.Height() + 1;
            if (!zpivTracker->UpdateState(meta))
                return error("%s: failed to update metadata in tracker", __func__);
        }
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
CWallet::CommitResult CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, std::string strCommand)
{
    CommitResult res;
    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("%s:\n%s", __func__, wtxNew.ToString());
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile, "r+") : NULL;

            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew, false, pwalletdb);

            // Notify that old coins are spent
            if (!wtxNew.HasZerocoinSpendInputs()) {
                std::set<uint256> updated_hahes;
                for (const CTxIn& txin : wtxNew.vin) {
                    // notify only once
                    if (updated_hahes.find(txin.prevout.hash) != updated_hahes.end()) continue;

                    CWalletTx& coin = mapWallet[txin.prevout.hash];
                    coin.BindWallet(this);
                    NotifyTransactionChanged(this, txin.prevout.hash, CT_UPDATED);
                    updated_hahes.insert(txin.prevout.hash);
                }
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        res.hashTx = wtxNew.GetHash();

        // Try ATMP. This must not fail. The transaction has already been signed and recorded.
        CValidationState state;
        if (!AcceptToMemoryPool(mempool, state, wtxNew, false, nullptr, false, true, false)) {
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

        // Track how many getdata requests our transaction gets
        mapRequestCount[res.hashTx] = 0;

        // Broadcast
        wtxNew.RelayWalletTransaction(strCommand);
    }
    return res;
}

bool CWallet::AddAccountingEntry(const CAccountingEntry& acentry, CWalletDB & pwalletdb)
{
    if (!pwalletdb.WriteAccountingEntry_Backend(acentry))
        return false;

    laccentries.push_back(acentry);
    CAccountingEntry & entry = laccentries.back();
    wtxOrdered.insert(std::make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));

    return true;
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
    if (nFeeNeeded == 0)
        nFeeNeeded = pool.estimateFee(nConfirmTarget).GetFee(nTxBytes);
    // ... unless we don't have enough mempool data, in which case fall
    // back to the required fee
    if (nFeeNeeded == 0)
        nFeeNeeded = GetRequiredFee(nTxBytes);
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
    if (!fFileBacked)
        return DB_LOAD_OK;

    DBErrors nLoadWalletRet = CWalletDB(strWalletFile, "cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            LOCK(cs_wallet);
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

CBitcoinAddress CWallet::ParseIntoAddress(const CTxDestination& dest, const std::string& purpose) {
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
    std::string addressStr = ParseIntoAddress(address, mapAddressBook.at(address).purpose).ToString();
    if (!strPurpose.empty() && !CWalletDB(strWalletFile).WritePurpose(addressStr, strPurpose))
        return false;
    return CWalletDB(strWalletFile).WriteName(addressStr, strName);
}

bool CWallet::DelAddressBook(const CTxDestination& address, const CChainParams::Base58Type addrType)
{
    std::string strAddress =  EncodeDestination(address, addrType);
    std::string purpose = purposeForAddress(address);
    {
        LOCK(cs_wallet); // mapAddressBook

        if (fFileBacked) {
            // Delete destdata tuples associated with address
            for (const PAIRTYPE(std::string, std::string) & item : mapAddressBook[address].destdata) {
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

std::string CWallet::purposeForAddress(const CTxDestination& address) const
{
    {
        LOCK(cs_wallet);
        auto mi = mapAddressBook.find(address);
        if (mi != mapAddressBook.end()) {
            return mi->second.purpose;
        }
    }
    return "";
}

bool CWallet::HasAddressBook(const CTxDestination& address) const
{
    LOCK(cs_wallet); // mapAddressBook
    std::map<CTxDestination, AddressBook::CAddressBookData>::const_iterator mi = mapAddressBook.find(address);
    return mi != mapAddressBook.end();
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
        for (PAIRTYPE(uint256, CWalletTx) walletEntry : mapWallet) {
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

    for (PAIRTYPE(uint256, CWalletTx) walletEntry : mapWallet) {
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

std::set<CTxDestination> CWallet::GetAccountAddresses(std::string strAccount) const
{
    LOCK(cs_wallet);
    std::set<CTxDestination> result;
    for (const PAIRTYPE(CTxDestination, AddressBook::CAddressBookData) & item : mapAddressBook) {
        const CTxDestination& address = item.first;
        const std::string& strName = item.second.name;
        if (strName == strAccount)
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
    if (boost::get<CNoDestination>(&dest))
        return false;

    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteDestData(CBitcoinAddress(dest).ToString(), key, value);
}

bool CWallet::EraseDestData(const CTxDestination& dest, const std::string& key)
{
    if (!mapAddressBook[dest].destdata.erase(key))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).EraseDestData(CBitcoinAddress(dest).ToString(), key);
}

bool CWallet::LoadDestData(const CTxDestination& dest, const std::string& key, const std::string& value)
{
    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    return true;
}

void CWallet::AutoCombineDust()
{
    LOCK2(cs_main, cs_wallet);
    const CBlockIndex* tip = chainActive.Tip();
    if (tip->nTime < (GetAdjustedTime() - 300) || IsLocked()) {
        return;
    }

    std::map<CBitcoinAddress, std::vector<COutput> > mapCoinsByAddress = AvailableCoinsByAddress(true, nAutoCombineThreshold * COIN);

    //coins are sectioned by address. This combination code only wants to combine inputs that belong to the same address
    for (std::map<CBitcoinAddress, std::vector<COutput> >::iterator it = mapCoinsByAddress.begin(); it != mapCoinsByAddress.end(); it++) {
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
        CScript scriptPubKey = GetScriptForDestination(it->first.Get());
        vecSend.push_back(CRecipient{scriptPubKey, nTotalRewardsValue, false});

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

        const CWallet::CommitResult& res = CommitTransaction(wtx, keyChange);
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
            vecSend.push_back(CRecipient{scriptPubKey, nAmount, false});
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

void CMerkleTx::SetMerkleBranch(const CBlock& block)
{
    CBlock blockTmp;

    // Update the tx's hashBlock
    hashBlock = block.GetHash();

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)block.vtx.size(); nIndex++)
        if (block.vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)block.vtx.size()) {
        nIndex = -1;
        LogPrintf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
        return;
    }
}

int CMerkleTx::GetDepthInMainChain(bool enableIX) const
{
    const CBlockIndex* pindexRet;
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
    }
    else {
        CBlockIndex* pindex = (*mi).second;
        if (!pindex || !chainActive.Contains(pindex)) {
            nResult = 0;
        }
        else {
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
    bool fAccepted = ::AcceptToMemoryPool(mempool, state, *this, fLimitFree, nullptr, false, fRejectInsaneFee, ignoreFees);
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
    return strprintf("wallet.dat%s", DateTimeStrFormat(".%Y-%m-%d-%H-%M", GetTime()));
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
    return (GetDebit(tx, ISMINE_ALL) > 0);
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

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter, const bool fUnspent) const
{
    CAmount nCredit = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        if (fUnspent && IsSpent(tx.GetHash(), i)) continue;
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

void CWallet::Inventory(const uint256& hash)
{
    {
        LOCK(cs_wallet);
        std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
        if (mi != mapRequestCount.end())
            (*mi).second++;
    }
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
    vOrderForm.clear();
    fTimeReceivedIsTxTime = false;
    nTimeReceived = 0;
    nTimeSmart = 0;
    fFromMe = false;
    strFromAccount.clear();
    fDebitCached = false;
    fCreditCached = false;
    fImmatureCreditCached = false;
    fAvailableCreditCached = false;
    fWatchDebitCached = false;
    fWatchCreditCached = false;
    fImmatureWatchCreditCached = false;
    fAvailableWatchCreditCached = false;
    fChangeCached = false;
    fColdDebitCached = false;
    fColdCreditCached = false;
    fDelegatedDebitCached = false;
    fDelegatedCreditCached = false;
    fStakeDelegationVoided = false;
    nDebitCached = 0;
    nCreditCached = 0;
    nImmatureCreditCached = 0;
    nAvailableCreditCached = 0;
    nWatchDebitCached = 0;
    nWatchCreditCached = 0;
    nAvailableWatchCreditCached = 0;
    nImmatureWatchCreditCached = 0;
    nColdDebitCached = 0;
    nColdCreditCached = 0;
    nDelegatedDebitCached = 0;
    nDelegatedCreditCached = 0;
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
    fCreditCached = false;
    fAvailableCreditCached = false;
    fWatchDebitCached = false;
    fWatchCreditCached = false;
    fAvailableWatchCreditCached = false;
    fImmatureWatchCreditCached = false;
    fDebitCached = false;
    fChangeCached = false;
    fColdDebitCached = false;
    fColdCreditCached = false;
    fDelegatedDebitCached = false;
    fDelegatedCreditCached = false;
}

void CWalletTx::BindWallet(CWallet* pwalletIn)
{
    pwallet = pwalletIn;
    MarkDirty();
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
