// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_SAPLINGSCRIPTPUBKEYMAN_H
#define hemis_SAPLINGSCRIPTPUBKEYMAN_H

#include "consensus/consensus.h"
#include "sapling/note.h"
#include "wallet/hdchain.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "sapling/incrementalmerkletree.h"

//! Size of witness cache
//  Should be large enough that we can expect not to reorg beyond our cache
//  unless there is some exceptional network disruption.
static const unsigned int WITNESS_CACHE_SIZE = DEFAULT_MAX_REORG_DEPTH + 1;

class CBlock;
class CBlockIndex;

/** Sapling note, its location in a transaction, and number of confirmations. */
struct SaplingNoteEntry
{
    explicit SaplingNoteEntry(const SaplingOutPoint& _op,
                              const libzcash::SaplingPaymentAddress& _addr,
                              const libzcash::SaplingNote& _note,
                              const std::array<unsigned char, ZC_MEMO_SIZE>& _memo,
                              const int _conf) :
                              op(_op), address(_addr), note(_note), memo(_memo),
                              confirmations(_conf) { }
    SaplingOutPoint op;
    libzcash::SaplingPaymentAddress address;
    libzcash::SaplingNote note;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;
    int confirmations;
};

class SaplingNoteData
{
public:

    SaplingNoteData() : nullifier() { }
    SaplingNoteData(const libzcash::SaplingIncomingViewingKey& _ivk) : ivk {_ivk}, nullifier() { }
    SaplingNoteData(const libzcash::SaplingIncomingViewingKey& _ivk, const uint256& n) : ivk {_ivk}, nullifier(n) { }

    /* witnesses/ivk: only for own (received) outputs */
    std::list<SaplingWitness> witnesses;
    Optional<libzcash::SaplingIncomingViewingKey> ivk {nullopt};
    inline bool IsMyNote() const { return ivk != nullopt; }

    /**
     * Cached note amount.
     * It will be loaded the first time that the note is decrypted (when the tx is added to the wallet).
     */
    Optional<CAmount> amount{nullopt};

    /**
     * Cached shielded address
     * It will be loaded the first time that the note is decrypted (when the tx is added to the wallet)
     */
     Optional<libzcash::SaplingPaymentAddress> address{nullopt};

     /**
      * Cached note memo (only for non-empty memo)
      * It will be loaded the first time that the note is decrypted (when the tx is added to the wallet)
      */
     Optional<std::array<unsigned char, ZC_MEMO_SIZE>> memo{nullopt};

    /**
     * Block height corresponding to the most current witness.
     *
     * When we first create a SaplingNoteData in SaplingScriptPubKeyMan::FindMySaplingNotes, this is set to
     * -1 as a placeholder. The next time CWallet::BlockConnected/CWallet::BlockDisconnected is called, we can
     * determine what height the witness cache for this note is valid for (even
     * if no witnesses were cached), and so can set the correct value in
     * SaplingScriptPubKeyMan::IncrementNoteWitnesses and SaplingScriptPubKeyMan::DecrementNoteWitnesses.
     */
    int witnessHeight{-1};

    /**
     * Cached note nullifier. May not be set if the wallet was not unlocked when
     * this SaplingNoteData was created. If not set, we always assume that the
     * note has not been spent.
     *
     * It's okay to cache the nullifier in the wallet, because we are storing
     * the spending key there too, which could be used to derive this.
     * If the wallet is encrypted, this means that someone with access to the
     * locked wallet cannot spend notes, but can connect received notes to the
     * transactions they are spent in. This is the same security semantics as
     * for transparent addresses.
     */
    Optional<uint256> nullifier;

    SERIALIZE_METHODS(SaplingNoteData, obj)
    {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }
        READWRITE(obj.ivk);
        READWRITE(obj.nullifier);
        READWRITE(obj.witnesses);
        READWRITE(obj.witnessHeight);
        READWRITE(obj.amount);
        READWRITE(obj.address);
        READWRITE(obj.memo);
    }

    friend bool operator==(const SaplingNoteData& a, const SaplingNoteData& b) {
        return (a.ivk == b.ivk &&
                a.nullifier == b.nullifier &&
                a.witnessHeight == b.witnessHeight &&
                a.amount == b.amount &&
                a.address == b.address &&
                a.memo == b.memo);
    }

    friend bool operator!=(const SaplingNoteData& a, const SaplingNoteData& b) {
        return !(a == b);
    }
};

enum KeyAddResult {
    SpendingKeyExists,
    KeyAlreadyExists,
    KeyAdded,
    KeyNotAdded,
};

typedef std::map<SaplingOutPoint, SaplingNoteData> mapSaplingNoteData_t;

/*
 * Sapling keys manager
 * A class implementing SaplingScriptPubKeyMan manages all sapling keys and Notes used in a wallet.
 * It is responsible for the decryption of notes and caching metadata.
 * A SaplingScriptPubKeyMan will be able to give out notes to be used, as well as marking
 * when a note has been used. It also handles when and how to store a sapling OutputDescription
 * and its related keys, including encryption.
 * A ScriptPubKeyMan will also update notes witnesses, and keep track of nullifiers.
 */
class SaplingScriptPubKeyMan {

public:
    SaplingScriptPubKeyMan(CWallet *parent) : wallet(parent) {}

    ~SaplingScriptPubKeyMan() {};

    /**
     * Keep track of the used nullifier.
     */
    void AddToSaplingSpends(const uint256& nullifier, const uint256& wtxid);
    bool IsSaplingSpent(const SaplingOutPoint& op) const;
    bool IsSaplingSpent(const uint256& nullifier) const;

    /**
     * pindex is the new tip being connected.
     */
    void IncrementNoteWitnesses(const CBlockIndex* pindex,
                                const CBlock* pblock,
                                SaplingMerkleTree& saplingTree);
    /**
     * nChainHeight is the old tip height being disconnected.
     */
    void DecrementNoteWitnesses(int nChainHeight);

    /**
     * Update mapSaplingNullifiersToNotes
     * with the cached nullifiers in this tx.
     */
    void UpdateNullifierNoteMapWithTx(const CWalletTx& wtx);

    /**
     *  Update mapSaplingNullifiersToNotes, and NoteData of a specific outpoint,
     *  directly with nullifier provided by the caller.
     */
    void UpdateSaplingNullifierNoteMap(SaplingNoteData& nd, const SaplingOutPoint& op, const Optional<uint256>& nullifier);

    /**
     *  Update mapSaplingNullifiersToNotes, computing the nullifier
     *  from a cached witness if necessary.
     */
    void UpdateSaplingNullifierNoteMapWithTx(CWalletTx& wtx);

    /**
     * Iterate over transactions in a block and update the cached Sapling nullifiers
     * for transactions which belong to the wallet.
     */
    void UpdateSaplingNullifierNoteMapForBlock(const CBlock* pblock);

    /**
     * Set and initialize the Sapling HD chain.
     */
    bool SetupGeneration(const CKeyID& keyID, bool force = false, bool memonly = false);
    bool IsEnabled() const { return !hdChain.IsNull(); };

    /* Set the current HD seed (will reset the chain child index counters)
      Sets the seed's version based on the current wallet version (so the
      caller must ensure the current wallet version is correct before calling
      this function). */
    void SetHDSeed(const CPubKey& key, bool force = false, bool memonly = false);
    void SetHDSeed(const CKeyID& keyID, bool force = false, bool memonly = false);

    /* Set the HD chain model (chain child index counters) */
    void SetHDChain(CHDChain& chain, bool memonly);
    const CHDChain& GetHDChain() const { return hdChain; }

    /* Get cached sapling commonOVK
     * If nullopt, read it from the database, and save it.
     * If not found in the database, create a new one from the HD seed (throw
     * if the wallet is locked), write it to database, and save it.
     */
    uint256 getCommonOVK();
    void setCommonOVK(const uint256& ovk) { commonOVK = ovk; }

    /* Encrypt Sapling keys */
    bool EncryptSaplingKeys(CKeyingMaterial& vMasterKeyIn);

    void GetConflicts(const CWalletTx& wtx, std::set<uint256>& result) const;

    // Get the ivk creation time (we are only using the ivk's default address)
    int64_t GetKeyCreationTime(const libzcash::SaplingIncomingViewingKey& ivk);

    // Add full viewing key if it's not already in the wallet
    KeyAddResult AddViewingKeyToWallet(const libzcash::SaplingExtendedFullViewingKey &extfvk) const;

    // Add spending key if it's not already in the wallet
    KeyAddResult AddSpendingKeyToWallet(
            const Consensus::Params &params,
            const libzcash::SaplingExtendedSpendingKey &sk,
            int64_t nTime);

    //! Generates new Sapling key
    libzcash::SaplingPaymentAddress GenerateNewSaplingZKey();
    //! Adds Sapling spending key to the store, and saves it to disk
    bool AddSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key);
    bool AddSaplingIncomingViewingKey(
            const libzcash::SaplingIncomingViewingKey &ivk,
            const libzcash::SaplingPaymentAddress &addr);
    bool AddCryptedSaplingSpendingKeyDB(
            const libzcash::SaplingExtendedFullViewingKey &extfvk,
            const std::vector<unsigned char> &vchCryptedSecret);
    //! Returns true if the wallet contains the spending key
    bool HaveSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &zaddr) const;
    //! Returns true if the wallet contains the spending and viewing key for the shielded address
    bool PaymentAddressBelongsToWallet(const libzcash::SaplingPaymentAddress &zaddr) const;

    //! Adds spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key);
    //! Load spending key metadata (used by LoadWallet)
    bool LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta);
    //! Adds an encrypted spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedSaplingZKey(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds a Sapling payment address -> incoming viewing key map entry,
    //! without saving it to disk (used by LoadWallet)
    bool LoadSaplingPaymentAddress(
            const libzcash::SaplingPaymentAddress &addr,
            const libzcash::SaplingIncomingViewingKey &ivk);
    bool AddSaplingSpendingKey(const libzcash::SaplingExtendedSpendingKey &sk);

    //! Return the full viewing key for the shielded address
    Optional<libzcash::SaplingExtendedFullViewingKey> GetViewingKeyForPaymentAddress(
            const libzcash::SaplingPaymentAddress &addr) const;

    //! Return the spending key for the payment address (nullopt if the wallet has no spending key for such address)
    Optional<libzcash::SaplingExtendedSpendingKey> GetSpendingKeyForPaymentAddress(const libzcash::SaplingPaymentAddress &addr) const;

    //! Finds all output notes in the given tx that have been sent to a
    //! SaplingPaymentAddress in this wallet
    std::pair<mapSaplingNoteData_t, SaplingIncomingViewingKeyMap> FindMySaplingNotes(const CTransaction& tx) const;

    //! Find all of the addresses in the given tx that have been sent to a SaplingPaymentAddress in this wallet.
    std::vector<libzcash::SaplingPaymentAddress> FindMySaplingAddresses(const CTransaction& tx) const;

    //! Find notes for the outpoints
    void GetNotes(const std::vector<SaplingOutPoint>& saplingOutpoints,
                  std::vector<SaplingNoteEntry>& saplingEntriesRet) const;

    /* Find notes filtered by payment address, min depth, ability to spend and if they are locked */
    void GetFilteredNotes(std::vector<SaplingNoteEntry>& saplingEntries,
        Optional<libzcash::SaplingPaymentAddress>& address,
        int minDepth = 1,
        bool ignoreSpent = true,
        bool requireSpendingKey = true,
        bool ignoreLocked = true) const;

    /* Find notes filtered by payment addresses, min depth, max depth, if they are spent,
       if a spending key is required, and if they are locked */
    void GetFilteredNotes(std::vector<SaplingNoteEntry>& saplingEntries,
                          std::set<libzcash::PaymentAddress>& filterAddresses,
                          int minDepth=1,
                          int maxDepth=INT_MAX,
                          bool ignoreSpent=true,
                          bool requireSpendingKey=true,
                          bool ignoreLocked=true) const;

    /* Return list of available notes grouped by sapling address. */
    std::map<libzcash::SaplingPaymentAddress, std::vector<SaplingNoteEntry>> ListNotes() const;

    //! Return the address from where the shielded spend is taking the funds from (if possible)
    Optional<libzcash::SaplingPaymentAddress> GetAddressFromInputIfPossible(const CWalletTx* wtx, int index) const;
    Optional<libzcash::SaplingPaymentAddress> GetAddressFromInputIfPossible(const uint256& txHash, int index) const;

    //! Whether the nullifier is from this wallet
    bool IsSaplingNullifierFromMe(const uint256& nullifier) const;

    //! Return all of the witnesses for the input notes
    void GetSaplingNoteWitnesses(
            const std::vector<SaplingOutPoint>& notes,
            std::vector<Optional<SaplingWitness>>& witnesses,
            uint256& final_anchor) const;

    std::set<std::pair<libzcash::PaymentAddress, uint256>> GetNullifiersForAddresses(const std::set<libzcash::PaymentAddress> & addresses) const;
    bool IsNoteSaplingChange(const std::set<std::pair<libzcash::PaymentAddress, uint256>>& nullifierSet, const libzcash::PaymentAddress& address, const SaplingOutPoint& entry) const;

    //! Try to recover the note using the wallet's ovks (mostly used when the outpoint is a debit)
    Optional<std::pair<
            libzcash::SaplingNotePlaintext,
            libzcash::SaplingPaymentAddress>> TryToRecoverNote(const CWalletTx& tx, const SaplingOutPoint& op);

    //! Return true if the wallet can decrypt & spend the shielded output.
    isminetype IsMine(const CWalletTx& wtx, const SaplingOutPoint& op) const;
    //! Return the shielded address of a specific outpoint of wallet transaction
    Optional<libzcash::SaplingPaymentAddress> GetOutPointAddress(const CWalletTx& tx, const SaplingOutPoint& op) const;
    //! Return the shielded value of a specific outpoint of wallet transaction
    CAmount GetOutPointValue(const CWalletTx& tx, const SaplingOutPoint& op) const;
    //! Return the memo value of a specific outpoint of wallet transaction
    Optional<std::string> GetOutPointMemo(const CWalletTx& tx, const SaplingOutPoint& op) const;
    //! Return the shielded credit of the tx
    CAmount GetCredit(const CWalletTx& tx, const isminefilter& filter, const bool fUnspent = false) const;
    //! Return the shielded debit of the tx.
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    //! Return the shielded change of the tx
    CAmount GetShieldedChange(const CWalletTx& wtx) const;

    //! Check whether an specific output is change or not.
    bool IsNoteSaplingChange(const SaplingOutPoint& op, libzcash::SaplingPaymentAddress address) const;

    //! Update note data if is needed
    bool UpdatedNoteData(const CWalletTx& wtxIn, CWalletTx& wtx);

    //! Clear every notesData from every wallet tx and reset the witness cache size
    void ClearNoteWitnessCache();

    // Sapling metadata
    std::map<libzcash::SaplingIncomingViewingKey, CKeyMetadata> mapSaplingZKeyMetadata;

    /*
     * Size of the incremental witness cache for the notes in our wallet.
     * This will always be greater than or equal to the size of the largest
     * incremental witness cache in any transaction in mapWallet.
     */
    int64_t nWitnessCacheSize{0};
    bool nWitnessCacheNeedsUpdate{false};

    /**
     * The reverse mapping of nullifiers to notes.
     *
     * The mapping cannot be updated while an encrypted wallet is locked,
     * because we need the SpendingKey to create the nullifier (zcash#1502). This has
     * several implications for transactions added to the wallet while locked:
     *
     * - Parent transactions can't be marked dirty when a child transaction that
     *   spends their output notes is updated.
     *
     *   - We currently don't cache any note values, so this is not a problem,
     *     yet.
     *
     * - GetFilteredNotes can't filter out spent notes.
     *
     *   - Per the comment in SaplingNoteData, we assume that if we don't have a
     *     cached nullifier, the note is not spent.
     *
     * Another more problematic implication is that the wallet can fail to
     * detect transactions on the blockchain that spend our notes. There are two
     * possible cases in which this could happen:
     *
     * - We receive a note when the wallet is locked, and then spend it using a
     *   different wallet client.
     *
     * - We spend from a PaymentAddress we control, then we export the
     *   SpendingKey and import it into a new wallet, and reindex/rescan to find
     *   the old transactions.
     *
     * The wallet will only miss "pure" spends - transactions that are only
     * linked to us by the fact that they contain notes we spent. If it also
     * sends notes to us, or interacts with our transparent addresses, we will
     * detect the transaction and add it to the wallet (again without caching
     * nullifiers for new notes). As by default JoinSplits send change back to
     * the origin PaymentAddress, the wallet should rarely miss transactions.
     *
     * To work around these issues, whenever the wallet is unlocked, we scan all
     * cached notes, and cache any missing nullifiers. Since the wallet must be
     * unlocked in order to spend notes, this means that GetFilteredNotes will
     * always behave correctly within that context (and any other uses will give
     * correct responses afterwards), for the transactions that the wallet was
     * able to detect. Any missing transactions can be rediscovered by:
     *
     * - Unlocking the wallet (to fill all nullifier caches).
     *
     * - Restarting the node with -reindex (which operates on a locked wallet
     *   but with the now-cached nullifiers).
     */

    std::map<uint256, SaplingOutPoint> mapSaplingNullifiersToNotes;

private:
    /* Parent wallet */
    CWallet* wallet{nullptr};
    /* the HD chain data model (external/internal chain counters) */
    CHDChain hdChain;
    /* cached common OVK for sapling spends from t addresses */
    Optional<uint256> commonOVK;
    uint256 getCommonOVKFromSeed() const;


    /**
     * Used to keep track of spent Notes, and
     * detect and report conflicts (double-spends).
     */
    typedef std::multimap<uint256, uint256> TxNullifiers;
    TxNullifiers mapTxSaplingNullifiers;
};

#endif //hemis_SAPLINGSCRIPTPUBKEYMAN_H
