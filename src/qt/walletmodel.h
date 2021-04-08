// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2016 The Dash developers
//Copyright (c) 2017-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Sombe_QT_WALLETMODEL_H
#define Sombe_QT_WALLETMODEL_H

#include "askpassphrasedialog.h"
#include "paymentrequestplus.h"
#include "walletmodeltransaction.h"

#include "allocators.h" /* for SecureString */
#include "swifttx.h"
#include "wallet/wallet.h"
#include "pairresult.h"

#include <map>
#include <vector>

#include <QObject>

class AddressTableModel;
class OptionsModel;
class RecentRequestsTableModel;
class TransactionTableModel;
class WalletModelTransaction;

class CCoinControl;
class CKeyID;
class COutPoint;
class COutput;
class CPubKey;
class CWallet;
class uint256;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SendCoinsRecipient
{
public:
    explicit SendCoinsRecipient() : amount(0), nVersion(SendCoinsRecipient::CURRENT_VERSION) {}
    explicit SendCoinsRecipient(const QString& addr, const QString& label, const CAmount& amount, const QString& message) : address(addr), label(label), amount(amount), message(message), nVersion(SendCoinsRecipient::CURRENT_VERSION) {}

    // If from an insecure payment request, this is used for storing
    // the addresses, e.g. address-A<br />address-B<br />address-C.
    // Info: As we don't need to process addresses in here when using
    // payment requests, we can abuse it for displaying an address list.
    // Todo: This is a hack, should be replaced with a cleaner solution!
    QString address;
    QString label;
    AvailableCoinsType inputType;
    bool useSwiftTX = false;

    // Cold staking.
    bool isP2CS = false;
    QString ownerAddress;

    // Amount
    CAmount amount;
    // If from a payment request, this is used for storing the memo
    QString message;

    // If from a payment request, paymentRequest.IsInitialized() will be true
    PaymentRequestPlus paymentRequest;
    // Empty if no authentication or invalid signature/cert/etc.
    QString authenticatedMerchant;

    static const int CURRENT_VERSION = 1;
    int nVersion;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        std::string sAddress = address.toStdString();
        std::string sLabel = label.toStdString();
        std::string sMessage = message.toStdString();
        std::string sPaymentRequest;
        if (!ser_action.ForRead() && paymentRequest.IsInitialized())
            paymentRequest.SerializeToString(&sPaymentRequest);
        std::string sAuthenticatedMerchant = authenticatedMerchant.toStdString();

        READWRITE(this->nVersion);
        READWRITE(sAddress);
        READWRITE(sLabel);
        READWRITE(amount);
        READWRITE(sMessage);
        READWRITE(sPaymentRequest);
        READWRITE(sAuthenticatedMerchant);

        if (ser_action.ForRead()) {
            address = QString::fromStdString(sAddress);
            label = QString::fromStdString(sLabel);
            message = QString::fromStdString(sMessage);
            if (!sPaymentRequest.empty())
                paymentRequest.parse(QByteArray::fromRawData(sPaymentRequest.data(), sPaymentRequest.size()));
            authenticatedMerchant = QString::fromStdString(sAuthenticatedMerchant);
        }
    }
};

/** Interface to Sombe wallet from Qt view code. */
class WalletModel : public QObject
{
    Q_OBJECT

public:
    explicit WalletModel(CWallet* wallet, OptionsModel* optionsModel, QObject* parent = 0);
    ~WalletModel();

    enum StatusCode // Returned by sendCoins
    {
        OK,
        InvalidAmount,
        InvalidAddress,
        AmountExceedsBalance,
        AmountWithFeeExceedsBalance,
        DuplicateAddress,
        TransactionCreationFailed,
        TransactionCommitFailed,
        StakingOnlyUnlocked,
        InsaneFee,
        CannotCreateInternalAddress
    };

    enum EncryptionStatus {
        Unencrypted,                 // !wallet->IsCrypted()
        Locked,                      // wallet->IsCrypted() && wallet->IsLocked()
        Unlocked,                    // wallet->IsCrypted() && !wallet->IsLocked()
        UnlockedForStaking          // wallet->IsCrypted() && !wallet->IsLocked() && wallet->fWalletUnlockStaking
    };

    OptionsModel* getOptionsModel();
    AddressTableModel* getAddressTableModel();
    TransactionTableModel* getTransactionTableModel();
    RecentRequestsTableModel* getRecentRequestsTableModel();

    bool isTestNetwork() const;
    bool isRegTestNetwork() const;
    /** Whether cold staking is enabled or disabled in the network **/
    bool isColdStakingNetworkelyEnabled() const;
    CAmount getMinColdStakingAmount() const;
    /* current staking status from the miner thread **/
    bool isStakingStatusActive() const;

    bool isHDEnabled() const;
    bool upgradeWallet(std::string& upgradeError);

    CAmount getBalance(const CCoinControl* coinControl = nullptr, bool fIncludeDelegated = true) const;
    CAmount getUnconfirmedBalance() const;
    CAmount getImmatureBalance() const;
    CAmount getLockedBalance() const;
    bool haveWatchOnly() const;
    CAmount getWatchBalance() const;
    CAmount getWatchUnconfirmedBalance() const;
    CAmount getWatchImmatureBalance() const;

    CAmount getDelegatedBalance() const;
    CAmount getColdStakedBalance() const;

    bool isColdStaking() const;

    EncryptionStatus getEncryptionStatus() const;
    bool isWalletUnlocked() const;
    bool isWalletLocked(bool fFullUnlocked = true) const;
    CKey generateNewKey() const; //for temporary paper wallet key generation
    bool setAddressBook(const CTxDestination& address, const std::string& strName, const std::string& strPurpose);
    void encryptKey(const CKey key, const std::string& pwd, const std::string& slt, std::vector<unsigned char>& crypted);
    void decryptKey(const std::vector<unsigned char>& crypted, const std::string& slt, const std::string& pwd, CKey& key);
    void emitBalanceChanged(); // Force update of UI-elements even when no values have changed

    // Check address for validity
    bool validateAddress(const QString& address);
    // Check address for validity and type (whether cold staking address or not)
    bool validateAddress(const QString& address, bool fStaking);

    // Return status record for SendCoins, contains error id + information
    struct SendCoinsReturn {
        SendCoinsReturn(StatusCode status = OK) : status(status) {}
        StatusCode status;
    };

    void setWalletDefaultFee(CAmount fee = DEFAULT_TRANSACTION_FEE);
    bool hasWalletCustomFee();
    bool getWalletCustomFee(CAmount& nFeeRet);
    void setWalletCustomFee(bool fUseCustomFee, const CAmount& nFee = DEFAULT_TRANSACTION_FEE);

    const CWalletTx* getTx(uint256 id);

    // prepare transaction for getting txfee before sending coins
    SendCoinsReturn prepareTransaction(WalletModelTransaction& transaction, const CCoinControl* coinControl = NULL, bool fIncludeDelegations = true);

    // Send coins to a list of recipients
    SendCoinsReturn sendCoins(WalletModelTransaction& transaction);

    // Wallet encryption
    bool setWalletEncrypted(bool encrypted, const SecureString& passphrase);
    // Passphrase only needed when unlocking
    bool setWalletLocked(bool locked, const SecureString& passPhrase = SecureString(), bool stakingOnly = false);

    // Method used to "lock" the wallet only for staking purposes. Just a flag that should prevent possible movements in the wallet.
    // Passphrase only needed when unlocking.
    bool lockForStakingOnly(const SecureString& passPhrase = SecureString());

    bool changePassphrase(const SecureString& oldPass, const SecureString& newPass);
    // Is wallet unlocked for staking only?
    bool isStakingOnlyUnlocked();
    // Wallet backup
    bool backupWallet(const QString& filename);

    // RAI object for unlocking wallet, returned by requestUnlock()
    class UnlockContext
    {
    public:
        UnlockContext(WalletModel *wallet, bool valid, const WalletModel::EncryptionStatus& status_before);
        ~UnlockContext();

        bool isValid() const { return valid; }

        // Copy constructor is disabled
        UnlockContext(const UnlockContext&) = delete;
        // Copy operator and constructor transfer the context
        UnlockContext(UnlockContext&& obj) { CopyFrom(std::move(obj)); }
        UnlockContext& operator=(UnlockContext&& rhs) { CopyFrom(std::move(rhs)); return *this; }

    private:
        WalletModel *wallet;
        bool valid;
        WalletModel::EncryptionStatus was_status;   // original status
        mutable bool relock; // mutable, as it can be set to false by copying

        UnlockContext& operator=(const UnlockContext&) = default;
        void CopyFrom(UnlockContext&& rhs);
    };

    UnlockContext requestUnlock();

    bool getPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const;
    int64_t getCreationTime() const;
    int64_t getKeyCreationTime(const CPubKey& key);
    int64_t getKeyCreationTime(const CTxDestination& address);
    PairResult getNewAddress(Destination& ret, std::string label = "") const;
    /**
     * Return a new address used to receive for delegated cold stake purpose.
     */
    PairResult getNewStakingAddress(Destination& ret, std::string label = "") const;

    bool whitelistAddressFromColdStaking(const QString &addressStr);
    bool blacklistAddressFromColdStaking(const QString &address);
    bool updateAddressBookPurpose(const QString &addressStr, const std::string& purpose);
    std::string getLabelForAddress(const CBitcoinAddress& address);
    bool getKeyId(const CBitcoinAddress& address, CKeyID& keyID);

    bool isMine(const CTxDestination& address);
    bool isMine(const QString& addressStr);
    bool isUsed(CBitcoinAddress address);
    void getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs);
    bool getMNCollateralCandidate(COutPoint& outPoint);
    bool isSpent(const COutPoint& outpoint) const;
    void listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const;

    bool isLockedCoin(uint256 hash, unsigned int n) const;
    void lockCoin(COutPoint& output);
    void unlockCoin(COutPoint& output);
    void listLockedCoins(std::vector<COutPoint>& vOutpts);

    std::string GetUniqueWalletBackupName();
    void loadReceiveRequests(std::vector<std::string>& vReceiveRequests);
    bool saveReceiveRequest(const std::string& sAddress, const int64_t nId, const std::string& sRequest);

private:
    CWallet* wallet;
    bool fHaveWatchOnly;
    bool fForceCheckBalanceChanged;

    // Wallet has an options model for wallet-specific options
    // (transaction fee, for example)
    OptionsModel* optionsModel;

    AddressTableModel* addressTableModel;
    TransactionTableModel* transactionTableModel;
    RecentRequestsTableModel* recentRequestsTableModel;

    // Cache some values to be able to detect changes
    CAmount cachedBalance;
    CAmount cachedUnconfirmedBalance;
    CAmount cachedImmatureBalance;
    CAmount cachedWatchOnlyBalance;
    CAmount cachedWatchUnconfBalance;
    CAmount cachedWatchImmatureBalance;
    CAmount cachedDelegatedBalance;
    CAmount cachedColdStakedBalance;

    EncryptionStatus cachedEncryptionStatus;
    int cachedNumBlocks;
    int cachedTxLocks;

    QTimer* pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    Q_INVOKABLE void checkBalanceChanged();

Q_SIGNALS:
    // Signal that balance in wallet changed
    void balanceChanged(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                        const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance,
                        const CAmount& delegatedBalance, const CAmount& coldStakingBalance);

    // Encryption status of wallet changed
    void encryptionStatusChanged(int status);

    // Signal emitted when wallet needs to be unlocked
    // It is valid behaviour for listeners to keep the wallet locked after this signal;
    // this means that the unlocking failed or was cancelled.
    void requireUnlock();

    // Fired when a message should be reported to the user
    void message(const QString& title, const QString& body, unsigned int style, bool* ret = nullptr);

    // Coins sent: from wallet, to recipient, in (serialized) transaction:
    void coinsSent(CWallet* wallet, SendCoinsRecipient recipient, QByteArray transaction);

    // Show progress dialog e.g. for rescan
    void showProgress(const QString& title, int nProgress);

    // Watch-only address added
    void notifyWatchonlyChanged(bool fHaveWatchonly);

    // Receive tab address may have changed
    void notifyReceiveAddressChanged();

public Q_SLOTS:
    /* Wallet status might have changed */
    void updateStatus();
    /* New transaction, or transaction changed status */
    void updateTransaction();
    /* New, updated or removed address book entry */
    void updateAddressBook(const QString& address, const QString& label, bool isMine, const QString& purpose, int status);
    /* Watch-only added */
    void updateWatchOnlyFlag(bool fHaveWatchonly);
    /* Current, immature or unconfirmed balance might have changed - emit 'balanceChanged' if so */
    void pollBalanceChanged();
    /* Update address book labels in the database */
    bool updateAddressBookLabels(const CTxDestination& address, const std::string& strName, const std::string& strPurpose);
};

#endif // Sombe_QT_WALLETMODEL_H
