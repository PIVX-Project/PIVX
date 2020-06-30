// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_H
#define BITCOIN_WALLET_H

#include "addressbook.h"
#include "amount.h"
#include "base58.h"
#include "consensus/tx_verify.h"
#include "crypter.h"
#include "kernel.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "pairresult.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "zpiv/zerocoin.h"
#include "guiinterface.h"
#include "util.h"
#include "util/memory.h"
#include "validationinterface.h"
#include "wallet/wallet_ismine.h"
#include "wallet/scriptpubkeyman.h"
#include "wallet/walletdb.h"
#include "zpiv/zpivmodule.h"
#include "zpiv/zpivwallet.h"
#include "zpiv/zpivtracker.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

extern CWallet* pwalletMain;

/**
 * Settings
 */
extern CFeeRate payTxFee;
extern CAmount maxTxFee;
extern unsigned int nTxConfirmTarget;
extern bool bSpendZeroConfChange;
extern bool bdisableSystemnotifications;
extern bool fSendFreeTransactions;
extern bool fPayAtLeastCustomFee;

//! -paytxfee default
static const CAmount DEFAULT_TRANSACTION_FEE = 0;
//! -paytxfee will warn if called with a higher fee than this amount (in satoshis) per KB
static const CAmount nHighTransactionFeeWarning = 0.1 * COIN;
//! -maxtxfee default
static const CAmount DEFAULT_TRANSACTION_MAXFEE = 1 * COIN;
//! -maxtxfee will warn if called with a higher fee than this amount (in satoshis)
static const CAmount nHighTransactionMaxFeeWarning = 100 * nHighTransactionFeeWarning;
//! Largest (in bytes) free transaction we're willing to create
static const unsigned int MAX_FREE_TRANSACTION_CREATE_SIZE = 1000;
//! -custombackupthreshold default
static const int DEFAULT_CUSTOMBACKUPTHRESHOLD = 1;
//! -minstakesplit default
static const CAmount DEFAULT_MIN_STAKE_SPLIT_THRESHOLD = 100 * COIN;

class CAccountingEntry;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CWalletTx;
class ScriptPubKeyMan;

/** (client) version numbers for particular wallet features */
enum WalletFeature {
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_PRE_PIVX = 61000, // inherited version..

    // The following features were implemented in BTC but not in our wallet, we can simply skip them.
    // FEATURE_HD = 130000,  Hierarchical key derivation after BIP32 (HD Wallet)
    // FEATURE_HD_SPLIT = 139900, // Wallet with HD chain split (change outputs will use m/0'/1'/k)

    FEATURE_PRE_SPLIT_KEYPOOL = 169900, // Upgraded to HD SPLIT and can have a pre-split keypool

    FEATURE_LATEST = FEATURE_PRE_SPLIT_KEYPOOL
};

enum AvailableCoinsType {
    ALL_COINS = 1,
    ONLY_10000 = 5,                                 // find masternode outputs including locked ones (use with caution)
    STAKEABLE_COINS = 6                             // UTXO's that are valid for staking
};

// Possible states for zPIV send
enum ZerocoinSpendStatus {
    ZPIV_SPEND_OKAY = 0,                            // No error
    ZPIV_SPEND_ERROR = 1,                           // Unspecified class of errors, more details are (hopefully) in the returning text
    ZPIV_WALLET_LOCKED = 2,                         // Wallet was locked
    ZPIV_COMMIT_FAILED = 3,                         // Commit failed, reset status
    ZPIV_ERASE_SPENDS_FAILED = 4,                   // Erasing spends during reset failed
    ZPIV_ERASE_NEW_MINTS_FAILED = 5,                // Erasing new mints during reset failed
    ZPIV_TRX_FUNDS_PROBLEMS = 6,                    // Everything related to available funds
    ZPIV_TRX_CREATE = 7,                            // Everything related to create the transaction
    ZPIV_TRX_CHANGE = 8,                            // Everything related to transaction change
    ZPIV_TXMINT_GENERAL = 9,                        // General errors in MintsToInputVectorPublicSpend
    ZPIV_INVALID_COIN = 10,                         // Selected mint coin is not valid
    ZPIV_FAILED_ACCUMULATOR_INITIALIZATION = 11,    // Failed to initialize witness
    ZPIV_INVALID_WITNESS = 12,                      // Spend coin transaction did not verify
    ZPIV_BAD_SERIALIZATION = 13,                    // Transaction verification failed
    ZPIV_SPENT_USED_ZPIV = 14,                      // Coin has already been spend
    ZPIV_TX_TOO_LARGE = 15,                         // The transaction is larger than the max tx size
    ZPIV_SPEND_V1_SEC_LEVEL                         // Spend is V1 and security level is not set to 100
};

/** A key pool entry */
class CKeyPool
{
public:
    //! The time at which the key was generated. Set in AddKeypoolPubKeyWithDB
    int64_t nTime;
    //! The public key
    CPubKey vchPubKey;
    //! Whether this keypool entry is in the internal, external or staking keypool.
    uint8_t type;
    //! Whether this key was generated for a keypool before the wallet was upgraded to HD-split
    bool m_pre_split;

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn, const uint8_t& type);

    bool IsInternal() const { return type == HDChain::ChangeType::INTERNAL; }
    bool IsExternal() const { return type == HDChain::ChangeType::EXTERNAL; }
    bool IsStaking() const { return type == HDChain::ChangeType::STAKING; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (ser_action.ForRead()) {
            try {
                READWRITE(FLATDATA(type));
                READWRITE(m_pre_split);
            } catch (std::ios_base::failure&) {
                /* Set as external address if we can't read the type boolean
                   (this will be the case for any wallet before the HD chain) */
                type = HDChain::ChangeType::EXTERNAL;
                m_pre_split = true;
            }
        } else {
            READWRITE(FLATDATA(type));
            READWRITE(m_pre_split);
        }
    }
};

/** Record info about last stake attempt:
 *  - tipBlock       index of the block on top of which last stake attempt was made
 *  - nTime          time slot of last attempt
 *  - nTries         number of UTXOs hashed during last attempt
 *  - nCoins         number of stakeable utxos during last attempt
**/
class CStakerStatus
{
private:
    const CBlockIndex* tipBlock{nullptr};
    int64_t nTime{0};
    int nTries{0};
    int nCoins{0};

public:
    // Get
    const CBlockIndex* GetLastTip() const { return tipBlock; }
    uint256 GetLastHash() const { return (GetLastTip() == nullptr ? UINT256_ZERO : GetLastTip()->GetBlockHash()); }
    int GetLastHeight() const { return (GetLastTip() == nullptr ? 0 : GetLastTip()->nHeight); }
    int GetLastCoins() const { return nCoins; }
    int GetLastTries() const { return nTries; }
    int64_t GetLastTime() const { return nTime; }
    // Set
    void SetLastCoins(const int coins) { nCoins = coins; }
    void SetLastTries(const int tries) { nTries = tries; }
    void SetLastTip(const CBlockIndex* lastTip) { tipBlock = lastTip; }
    void SetLastTime(const uint64_t lastTime) { nTime = lastTime; }
    void SetNull()
    {
        SetLastCoins(0);
        SetLastTries(0);
        SetLastTip(nullptr);
        SetLastTime(0);
    }
    // Check whether staking status is active (last attempt earlier than 30 seconds ago)
    bool IsActive() const { return (nTime + 30) >= GetTime(); }
};

struct CRecipient
{
    CScript scriptPubKey;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;
};


/**
 * A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore, public CValidationInterface
{
private:
    CWalletDB* pwalletdbEncryption;
    //! keeps track of whether Unlock has run a thorough check before
    bool fDecryptionThoroughlyChecked{false};

    //! Key manager //
    std::unique_ptr<ScriptPubKeyMan> m_spk_man = MakeUnique<ScriptPubKeyMan>(this);

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

    int64_t nNextResend;
    int64_t nLastResend;

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef std::multimap<COutPoint, uint256> TxSpends;
    TxSpends mapTxSpends;
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

    /* Mark a transaction (and its in-wallet descendants) as conflicting with a particular block. */
    void MarkConflicted(const uint256& hashBlock, const uint256& hashTx);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

public:

    static const CAmount DEFAULT_STAKE_SPLIT_THRESHOLD = 500 * COIN;

    //! Generates hd wallet //
    bool SetupSPKM();
    //! Whether the wallet is hd or not //
    bool IsHDEnabled() const;

    /* SPKM Helpers */
    const CKeyingMaterial& GetEncryptionKey() const;
    bool HasEncryptionKeys() const;

    //! Get spkm
    ScriptPubKeyMan* GetScriptPubKeyMan() const;

    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable RecursiveMutex cs_wallet;

    bool fFileBacked;
    bool fWalletUnlockStaking;
    std::string strWalletFile;

    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    // Stake split threshold
    CAmount nStakeSplitThreshold;
    // minimum value allowed for nStakeSplitThreshold (customizable with -minstakesplit flag)
    static CAmount minStakeSplitThreshold;
    // Staker status (last hashed block and time)
    CStakerStatus* pStakerStatus = nullptr;

    // User-defined fee PIV/kb
    bool fUseCustomFee;
    CAmount nCustomFee;

    //MultiSend
    std::vector<std::pair<std::string, int> > vMultiSend;
    bool fMultiSendStake;
    bool fMultiSendMasternodeReward;
    bool fMultiSendNotify;
    std::string strMultiSendChangeAddress;
    int nLastMultiSendHeight;
    std::vector<std::string> vDisabledAddresses;

    //Auto Combine Inputs
    bool fCombineDust;
    CAmount nAutoCombineThreshold;

    CWallet();
    CWallet(std::string strWalletFileIn);
    ~CWallet();
    void SetNull();
    bool isMultiSendEnabled();
    void setMultiSendDisabled();

    std::map<uint256, CWalletTx> mapWallet;
    std::list<CAccountingEntry> laccentries;

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems wtxOrdered;

    int64_t nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<CTxDestination, AddressBook::CAddressBookData> mapAddressBook;

    std::set<COutPoint> setLockedCoins;

    int64_t nTimeFirstKey;

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    std::vector<CWalletTx> getWalletTxs();
    std::string GetUniqueWalletBackupName() const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf);

    //! >> Available coins (generic)
    bool AvailableCoins(std::vector<COutput>* pCoins,   // --> populates when != nullptr
                        const CCoinControl* coinControl = nullptr,
                        bool fIncludeDelegated          = true,
                        bool fIncludeColdStaking        = false,
                        AvailableCoinsType nCoinType    = ALL_COINS,
                        bool fOnlyConfirmed             = true,
                        bool fIncludeZeroValue          = false,
                        bool fUseIX                     = false,
                        int nWatchonlyConfig            = 1
                        ) const;
    //! >> Available coins (spending)
    bool SelectCoinsToSpend(const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl = nullptr, AvailableCoinsType coin_type = ALL_COINS, bool useIX = true, bool fIncludeColdStaking = false, bool fIncludeDelegated = true) const;
    bool SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet) const;
    //! >> Available coins (staking)
    bool StakeableCoins(std::vector<COutput>* pCoins = nullptr);
    //! >> Available coins (P2CS)
    void GetAvailableP2CSCoins(std::vector<COutput>& vCoins) const;

    std::map<CBitcoinAddress, std::vector<COutput> > AvailableCoinsByAddress(bool fConfirmed = true, CAmount maxCoinValue = 0);

    /// Get 10000 PIV output and keys which can be used for the Masternode
    bool GetMasternodeVinAndKeys(CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, std::string strTxHash = "", std::string strOutputIndex = "");
    /// Extract txin information and keys from output
    bool GetVinAndKeysFromOutput(COutput out, CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, bool fColdStake = false);

    bool IsSpent(const uint256& hash, unsigned int n) const;

    bool IsLockedCoin(const uint256& hash, unsigned int n) const;
    void LockCoin(const COutPoint& output);
    void UnlockCoin(const COutPoint& output);
    void UnlockAllCoins();
    void ListLockedCoins(std::vector<COutPoint>& vOutpts);

    //  keystore implementation
    PairResult getNewAddress(CTxDestination& ret, const std::string addressLabel, const std::string purpose,
                                           const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS);
    PairResult getNewAddress(CTxDestination& ret, std::string label);
    PairResult getNewStakingAddress(CTxDestination& ret, std::string label);
    int64_t GetKeyCreationTime(CPubKey pubkey);
    int64_t GetKeyCreationTime(const CBitcoinAddress& address);

    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey& pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
    //! Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey& pubkey, const CKeyMetadata& metadata);

    bool LoadMinVersion(int nVersion);

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);

    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination& dest, const std::string& key, const std::string& value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination& dest, const std::string& key);
    //! Adds a destination data tuple to the store, without saving it to disk
    bool LoadDestData(const CTxDestination& dest, const std::string& key, const std::string& value);

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript& dest);
    bool RemoveWatchOnly(const CScript& dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript& dest);

    //! Lock Wallet
    bool Lock();
    bool Unlock(const SecureString& strWalletPassphrase, bool anonimizeOnly = false);
    bool Unlock(const CKeyingMaterial& vMasterKeyIn);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    std::vector<CKeyID> GetAffectedKeys(const CScript& spk);
    void GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const;

    /**
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB* pwalletdb = NULL);

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb);
    void SyncTransaction(const CTransaction& tx, const CBlock* pblock);
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate);
    void EraseFromWallet(const uint256& hash);

    /**
     * Upgrade wallet to HD if needed. Does nothing if not.
     */
    bool Upgrade(std::string& error, const int& prevVersion);

    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false, bool fromStartup = false);
    void ReacceptWalletTransactions(bool fFirstLoad = false);
    void ResendWalletTransactions();

    CAmount loopTxsBalance(std::function<void(const uint256&, const CWalletTx&, CAmount&)>method) const;
    CAmount GetBalance(bool fIncludeDelegated = true) const;
    CAmount GetColdStakingBalance() const;  // delegated coins for which we have the staking key
    CAmount GetImmatureColdStakingBalance() const;
    CAmount GetStakingBalance(const bool fIncludeColdStaking = true) const;
    CAmount GetDelegatedBalance() const;    // delegated coins for which we have the spending key
    CAmount GetImmatureDelegatedBalance() const;
    CAmount GetLockedCoins() const;
    CAmount GetUnlockedCoins() const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    CAmount GetWatchOnlyBalance() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;
    CAmount GetLockedWatchOnlyBalance() const;
    bool FundTransaction(CMutableTransaction& tx, CAmount &nFeeRet, bool overrideEstimatedFeeRate, const CFeeRate& specificFeeRate, int& nChangePosInOut, std::string& strFailReason, bool includeWatching, bool lockUnspents, const CTxDestination& destChange = CNoDestination());
    /**
     * Create a new transaction paying the recipients with a set of coins
     * selected by SelectCoins(); Also create the change output, when needed
     * @note passing nChangePosInOut as -1 will result in setting a random position
     */
    bool CreateTransaction(const std::vector<CRecipient>& vecSend,
        CWalletTx& wtxNew,
        CReserveKey& reservekey,
        CAmount& nFeeRet,
        int& nChangePosInOut,
        std::string& strFailReason,
        const CCoinControl* coinControl = NULL,
        AvailableCoinsType coin_type = ALL_COINS,
        bool sign = true,
        bool useIX = false,
        CAmount nFeePay = 0,
        bool fIncludeDelegated = false);
    bool CreateTransaction(CScript scriptPubKey, const CAmount& nValue, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl = NULL, AvailableCoinsType coin_type = ALL_COINS, bool useIX = false, CAmount nFeePay = 0, bool fIncludeDelegated = false);

    // enumeration for CommitResult (return status of CommitTransaction)
    enum CommitStatus
    {
        OK,
        Abandoned,              // Failed to accept to memory pool. Successfully removed from the wallet.
        NotAccepted,            // Failed to accept to memory pool. Unable to abandon.
    };
    struct CommitResult
    {
        CommitResult(): status(CommitStatus::NotAccepted) {}
        CWallet::CommitStatus status;
        CValidationState state;
        uint256 hashTx = UINT256_ZERO;
        // converts CommitResult in human-readable format
        std::string ToString() const;
    };
    CWallet::CommitResult CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, std::string strCommand = NetMsgType::TX);
    bool AddAccountingEntry(const CAccountingEntry&, CWalletDB & pwalletdb);
    bool CreateCoinStake(const CKeyStore& keystore, const CBlockIndex* pindexPrev, unsigned int nBits, CMutableTransaction& txNew, int64_t& nTxNewTime);
    bool MultiSend();
    void AutoCombineDust();

    static CFeeRate minTxFee;
    /**
     * Estimate the minimum fee considering user set parameters
     * and the required fee
     */
    static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool);
    /**
     * Return the minimum required fee taking into account the
     * floating relay fee and user set minimum transaction fee
     */
    static CAmount GetRequiredFee(unsigned int nTxBytes);

    size_t KeypoolCountExternalKeys();
    bool TopUpKeyPool(unsigned int kpSize = 0);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex, const bool& internal = false, const bool& staking = false);
    bool GetKeyFromPool(CPubKey& key, const uint8_t& type = HDChain::ChangeType::EXTERNAL);
    int64_t GetOldestKeyPoolTime();

    std::set<std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, CAmount> GetAddressBalances();

    std::set<CTxDestination> GetAccountAddresses(std::string strAccount) const;

    bool GetBudgetSystemCollateralTX(CWalletTx& tx, uint256 hash, bool useIX);
    bool GetBudgetFinalizationCollateralTX(CWalletTx& tx, uint256 hash, bool useIX); // Only used for budget finalization

    bool IsUsed(const CBitcoinAddress address) const;

    isminetype IsMine(const CTxIn& txin) const;
    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const;
    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool IsChange(const CTxOut& txout) const;
    CAmount GetChange(const CTxOut& txout) const;
    bool IsMine(const CTransaction& tx) const;
    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction& tx) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter, const bool fUnspent = false) const;
    CAmount GetChange(const CTransaction& tx) const;
    void SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);

    static CBitcoinAddress ParseIntoAddress(const CTxDestination& dest, const std::string& purpose);

    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose);
    bool DelAddressBook(const CTxDestination& address, const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS);
    bool HasAddressBook(const CTxDestination& address) const;
    bool HasDelegator(const CTxOut& out) const;

    std::string purposeForAddress(const CTxDestination& address) const;

    bool UpdatedTransaction(const uint256& hashTx);

    void Inventory(const uint256& hash);

    unsigned int GetKeyPoolSize();
    unsigned int GetStakingKeyPoolSize();

    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion();

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    /* Mark a transaction (and it in-wallet descendants) as abandoned so its inputs may be respent. */
    bool AbandonTransaction(const uint256& hashTx);

    /**
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void(CWallet* wallet, const CTxDestination& address, const std::string& label, bool isMine, const std::string& purpose, ChangeType status)> NotifyAddressBookChanged;

    /**
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void(CWallet* wallet, const uint256& hashTx, ChangeType status)> NotifyTransactionChanged;

    /** Show progress e.g. for rescan */
    boost::signals2::signal<void(const std::string& title, int nProgress)> ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void(bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /** notify wallet file backed up */
    boost::signals2::signal<void (const bool& fSuccess, const std::string& filename)> NotifyWalletBacked;


    /* Legacy ZC - implementations in wallet_zerocoin.cpp */

    bool GetDeterministicSeed(const uint256& hashSeed, uint256& seed);
    bool AddDeterministicSeed(const uint256& seed);

    //- ZC Mints (Only for regtest)
    std::string MintZerocoin(CAmount nValue, CWalletTx& wtxNew, std::vector<CDeterministicMint>& vDMints, const CCoinControl* coinControl = NULL);
    std::string MintZerocoinFromOutPoint(CAmount nValue, CWalletTx& wtxNew, std::vector<CDeterministicMint>& vDMints, const std::vector<COutPoint> vOutpts);
    bool CreateZPIVOutPut(libzerocoin::CoinDenomination denomination, CTxOut& outMint, CDeterministicMint& dMint);
    bool CreateZerocoinMintTransaction(const CAmount nValue,
            CMutableTransaction& txNew,
            std::vector<CDeterministicMint>& vDMints,
            CReserveKey* reservekey,
            std::string& strFailReason,
            const CCoinControl* coinControl = NULL);

    // - ZC PublicSpends
    bool SpendZerocoin(CAmount nAmount, CWalletTx& wtxNew, CZerocoinSpendReceipt& receipt, std::vector<CZerocoinMint>& vMintsSelected, std::list<std::pair<CTxDestination,CAmount>> addressesTo, CBitcoinAddress* changeAddress = nullptr);
    bool MintsToInputVectorPublicSpend(std::map<CBigNum, CZerocoinMint>& mapMintsSelected, const uint256& hashTxOut, std::vector<CTxIn>& vin, CZerocoinSpendReceipt& receipt, libzerocoin::SpendType spendType, CBlockIndex* pindexCheckpoint = nullptr);
    bool CreateZCPublicSpendTransaction(
            CAmount nValue,
            CWalletTx& wtxNew,
            CReserveKey& reserveKey,
            CZerocoinSpendReceipt& receipt,
            std::vector<CZerocoinMint>& vSelectedMints,
            std::vector<CDeterministicMint>& vNewMints,
            std::list<std::pair<CTxDestination,CAmount>> addressesTo,
            CBitcoinAddress* changeAddress = nullptr);

    // - ZC Balances
    CAmount GetZerocoinBalance(bool fMatureOnly) const;
    CAmount GetUnconfirmedZerocoinBalance() const;
    CAmount GetImmatureZerocoinBalance() const;
    std::map<libzerocoin::CoinDenomination, CAmount> GetMyZerocoinDistribution() const;

    // zPIV wallet
    CzPIVWallet* zwalletMain{nullptr};
    std::unique_ptr<CzPIVTracker> zpivTracker{nullptr};
    void setZWallet(CzPIVWallet* zwallet);
    CzPIVWallet* getZWallet();
    bool IsMyZerocoinSpend(const CBigNum& bnSerial) const;
    bool IsMyMint(const CBigNum& bnValue) const;
    std::string ResetMintZerocoin();
    std::string ResetSpentZerocoin();
    void ReconsiderZerocoins(std::list<CZerocoinMint>& listMintsRestored, std::list<CDeterministicMint>& listDMintsRestored);
    bool GetZerocoinKey(const CBigNum& bnSerial, CKey& key);
    bool GetMint(const uint256& hashSerial, CZerocoinMint& mint);
    bool GetMintFromStakeHash(const uint256& hashStake, CZerocoinMint& mint);
    bool DatabaseMint(CDeterministicMint& dMint);
    bool SetMintUnspent(const CBigNum& bnSerial);
    bool UpdateMint(const CBigNum& bnValue, const int& nHeight, const uint256& txid, const libzerocoin::CoinDenomination& denom);
    // Zerocoin entry changed. (called with lock cs_wallet held)
    boost::signals2::signal<void(CWallet* wallet, const std::string& pubCoin, const std::string& isUsed, ChangeType status)> NotifyZerocoinChanged;
    // zPIV reset
    boost::signals2::signal<void()> NotifyzPIVReset;

    /* Wallets parameter interaction */
    static bool ParameterInteraction();
};

/** A key allocated from the key pool. */
class CReserveKey
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    bool internal{false};
    CPubKey vchPubKey;

public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey& pubkey, bool internal = false);
    void KeepKey();
};


typedef std::map<std::string, std::string> mapValue_t;


static inline void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n")) {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static inline void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry {
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
    /** Constant used in hashBlock to indicate tx has been abandoned */
    static const uint256 ABANDON_HASH;

public:
    uint256 hashBlock;
    /* An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */
    int nIndex;

    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock = UINT256_ZERO;
        nIndex = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        std::vector<uint256> vMerkleBranch; // For compatibility with older versions.
        READWRITE(*(CTransaction*)this);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    void SetMerkleBranch(const CBlock& block);


    /**
     * Return depth of transaction in blockchain:
     * <0  : conflicts with a transaction this deep in the blockchain
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(const CBlockIndex*& pindexRet, bool enableIX = true) const;
    int GetDepthInMainChain(bool enableIX = true) const;
    bool IsInMainChain() const;
    bool IsInMainChainImmature() const;
    int GetBlocksToMaturity() const;
    bool AcceptToMemoryPool(bool fLimitFree = true, bool fRejectInsaneFee = true, bool ignoreFees = false);
    int GetTransactionLockSignatures() const;
    bool IsTransactionLockTimedOut() const;
    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }
};

/**
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //! time received by this node
    /**
     * Stable timestamp representing the block time, for a transaction included in a block,
     * or else the time when the transaction was received if it isn't yet part of a block.
     */
    unsigned int nTimeSmart;
    char fFromMe;
    std::string strFromAccount;
    int64_t nOrderPos; //! position in ordered transaction list

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable bool fColdDebitCached;
    mutable bool fColdCreditCached;
    mutable bool fDelegatedDebitCached;
    mutable bool fDelegatedCreditCached;
    mutable bool fStakeDelegationVoided;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;
    mutable CAmount nColdDebitCached;
    mutable CAmount nColdCreditCached;
    mutable CAmount nDelegatedDebitCached;
    mutable CAmount nDelegatedCreditCached;

    CWalletTx();
    CWalletTx(const CWallet* pwalletIn);
    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn);
    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn);
    void Init(const CWallet* pwalletIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead())
            Init(NULL);
        char fSpent = false;

        if (!ser_action.ForRead()) {
            mapValue["fromaccount"] = strFromAccount;

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //! Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead()) {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("version");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty();

    void BindWallet(CWallet* pwalletIn);
    //! checks whether a tx has P2CS inputs or not
    bool HasP2CSInputs() const;

    int GetDepthAndMempool(bool& fConflicted, bool enableIX = true) const;

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetUnspentCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(bool fUseCache = true, const isminefilter& filter = ISMINE_SPENDABLE_ALL) const;
    CAmount GetAvailableCredit(bool fUseCache = true) const;
    // Return sum of unlocked coins
    CAmount GetUnlockedCredit() const;
    // Return sum of unlocked coins
    CAmount GetLockedCredit() const;
    CAmount GetImmatureWatchOnlyCredit(const bool& fUseCache = true) const;
    CAmount GetAvailableWatchOnlyCredit(const bool& fUseCache = true) const;
    CAmount GetLockedWatchOnlyCredit() const;
    CAmount GetChange() const;

    // Cold staking contracts credit/debit
    CAmount GetColdStakingCredit(bool fUseCache = true) const;
    CAmount GetColdStakingDebit(bool fUseCache = true) const;
    CAmount GetStakeDelegationCredit(bool fUseCache = true) const;
    CAmount GetStakeDelegationDebit(bool fUseCache = true) const;

    // Helper method to update the amount and cacheFlag.
    CAmount UpdateAmount(CAmount& amountToUpdate, bool& cacheFlagToUpdate, bool fUseCache, isminetype mimeType, bool fCredit = true) const;

    void GetAmounts(std::list<COutputEntry>& listReceived,
        std::list<COutputEntry>& listSent,
        CAmount& nFee,
        std::string& strSentAccount,
        const isminefilter& filter) const;

    void GetAccountAmounts(const std::string& strAccount, CAmount& nReceived, CAmount& nSent, CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const;

    bool InMempool() const;

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool IsTrusted() const;
    bool IsTrusted(int& nDepth, bool& fConflicted) const;

    bool WriteToDisk(CWalletDB *pwalletdb);

    int64_t GetTxTime() const;
    void UpdateTimeSmart();
    int GetRequestCount() const;
    void RelayWalletTransaction(std::string strCommand = NetMsgType::TX);
    std::set<uint256> GetConflicts() const;
};


class COutput
{
public:
    const CWalletTx* tx;
    int i;
    int nDepth;
    bool fSpendable;

    COutput(const CWalletTx* txIn, int iIn, int nDepthIn, bool fSpendableIn)
    {
        tx = txIn;
        i = iIn;
        nDepth = nDepthIn;
        fSpendable = fSpendableIn;
    }

    CAmount Value() const
    {
        return tx->vout[i].nValue;
    }

    std::string ToString() const;
};


/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires = 0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};


/**
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};


/**
 * Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    CAmount nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64_t nOrderPos; //! position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
        nEntryNo = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead()) {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty())) {
                CDataStream ss(s.GetType(), s.GetVersion());
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead()) {
            mapValue.clear();
            if (std::string::npos != nSepPos) {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), s.GetType(), s.GetVersion());
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

#endif // BITCOIN_WALLET_H
