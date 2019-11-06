// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodel.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "recentrequeststablemodel.h"
#include "transactiontablemodel.h"

#include "base58.h"
#include "db.h"
#include "keystore.h"
#include "main.h"
#include "spork.h"
#include "sync.h"
#include "guiinterface.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h" // for BackupWallet
#include <stdint.h>
#include <iostream>
#include "zpiv/deterministicmint.h"

#include <QDebug>
#include <QSet>
#include <QTimer>


WalletModel::WalletModel(CWallet* wallet, OptionsModel* optionsModel, QObject* parent) : QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
                                                                                         transactionTableModel(0),
                                                                                         recentRequestsTableModel(0),
                                                                                         cachedBalance(0), cachedUnconfirmedBalance(0), cachedImmatureBalance(0),
                                                                                         cachedZerocoinBalance(0), cachedUnconfirmedZerocoinBalance(0), cachedImmatureZerocoinBalance(0),
                                                                                         cachedEncryptionStatus(Unencrypted),
                                                                                         cachedNumBlocks(0)
{
    fHaveWatchOnly = wallet->HaveWatchOnly();
    fHaveMultiSig = wallet->HaveMultiSig();
    fForceCheckBalanceChanged = false;

    addressTableModel = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);
    recentRequestsTableModel = new RecentRequestsTableModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

bool WalletModel::isTestnet() const {
    return Params().NetworkID() == CBaseChainParams::TESTNET;
}

CAmount WalletModel::getBalance(const CCoinControl* coinControl) const
{
    if (coinControl) {
        CAmount nBalance = 0;
        std::vector<COutput> vCoins;
        wallet->AvailableCoins(vCoins, true, coinControl);
        for (const COutput& out : vCoins)
            if (out.fSpendable)
                nBalance += out.tx->vout[out.i].nValue;

        return nBalance;
    }

    return wallet->GetBalance();
}

CAmount WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}

CAmount WalletModel::getImmatureBalance() const
{
    return wallet->GetImmatureBalance();
}

CAmount WalletModel::getLockedBalance() const
{
    return wallet->GetLockedCoins();
}

CAmount WalletModel::getZerocoinBalance() const
{
    return wallet->GetZerocoinBalance(false);
}

CAmount WalletModel::getUnconfirmedZerocoinBalance() const
{
    return wallet->GetUnconfirmedZerocoinBalance();
}

CAmount WalletModel::getImmatureZerocoinBalance() const
{
    return wallet->GetImmatureZerocoinBalance();
}


bool WalletModel::haveWatchOnly() const
{
    return fHaveWatchOnly;
}

CAmount WalletModel::getWatchBalance() const
{
    return wallet->GetWatchOnlyBalance();
}

CAmount WalletModel::getWatchUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedWatchOnlyBalance();
}

CAmount WalletModel::getWatchImmatureBalance() const
{
    return wallet->GetImmatureWatchOnlyBalance();
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if (cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

bool WalletModel::isWalletUnlocked() const {
    EncryptionStatus status = getEncryptionStatus();
    return status == Unencrypted || status == Unlocked;
}

void WalletModel::pollBalanceChanged()
{
    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if (!lockWallet)
        return;

    if (fForceCheckBalanceChanged || chainActive.Height() != cachedNumBlocks || nZeromintPercentage != cachedZeromintPercentage || cachedTxLocks != nCompleteTXLocks) {
        fForceCheckBalanceChanged = false;

        // Balance and number of transactions might have changed
        cachedNumBlocks = chainActive.Height();
        cachedZeromintPercentage = nZeromintPercentage;

        checkBalanceChanged();
        if (transactionTableModel) {
            transactionTableModel->updateConfirmations();
        }

        // Address in receive tab may have been used
        emit notifyReceiveAddressChanged();
    }
}

void WalletModel::emitBalanceChanged()
{
    // Force update of UI elements even when no values have changed
    emit balanceChanged(cachedBalance, cachedUnconfirmedBalance, cachedImmatureBalance,
                        cachedZerocoinBalance, cachedUnconfirmedZerocoinBalance, cachedImmatureZerocoinBalance,
                        cachedWatchOnlyBalance, cachedWatchUnconfBalance, cachedWatchImmatureBalance);
}

void WalletModel::checkBalanceChanged()
{
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain) return;

    CAmount newBalance = getBalance();
    CAmount newUnconfirmedBalance = getUnconfirmedBalance();
    CAmount newImmatureBalance = getImmatureBalance();
    CAmount newZerocoinBalance = getZerocoinBalance();
    CAmount newUnconfirmedZerocoinBalance = getUnconfirmedZerocoinBalance();
    CAmount newImmatureZerocoinBalance = getImmatureZerocoinBalance();
    CAmount newWatchOnlyBalance = 0;
    CAmount newWatchUnconfBalance = 0;
    CAmount newWatchImmatureBalance = 0;
    if (haveWatchOnly()) {
        newWatchOnlyBalance = getWatchBalance();
        newWatchUnconfBalance = getWatchUnconfirmedBalance();
        newWatchImmatureBalance = getWatchImmatureBalance();
    }

    if (cachedBalance != newBalance || cachedUnconfirmedBalance != newUnconfirmedBalance || cachedImmatureBalance != newImmatureBalance ||
        cachedZerocoinBalance != newZerocoinBalance || cachedUnconfirmedZerocoinBalance != newUnconfirmedZerocoinBalance || cachedImmatureZerocoinBalance != newImmatureZerocoinBalance ||
        cachedWatchOnlyBalance != newWatchOnlyBalance || cachedWatchUnconfBalance != newWatchUnconfBalance || cachedWatchImmatureBalance != newWatchImmatureBalance ||
        cachedTxLocks != nCompleteTXLocks ) {
        cachedBalance = newBalance;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance = newImmatureBalance;
        cachedZerocoinBalance = newZerocoinBalance;
        cachedUnconfirmedZerocoinBalance = newUnconfirmedZerocoinBalance;
        cachedImmatureZerocoinBalance = newImmatureZerocoinBalance;
        cachedTxLocks = nCompleteTXLocks;
        cachedWatchOnlyBalance = newWatchOnlyBalance;
        cachedWatchUnconfBalance = newWatchUnconfBalance;
        cachedWatchImmatureBalance = newWatchImmatureBalance;
        emit balanceChanged(newBalance, newUnconfirmedBalance, newImmatureBalance,
                            newZerocoinBalance, newUnconfirmedZerocoinBalance, newImmatureZerocoinBalance,
                            newWatchOnlyBalance, newWatchUnconfBalance, newWatchImmatureBalance);
    }
}

void WalletModel::setWalletDefaultFee(CAmount fee)
{
    payTxFee = CFeeRate(fee);
}

void WalletModel::updateTransaction()
{
    // Balance and number of transactions might have changed
    fForceCheckBalanceChanged = true;
}

void WalletModel::updateAddressBook(const QString& address, const QString& label, bool isMine, const QString& purpose, int status)
{
    try {
        if (addressTableModel)
            addressTableModel->updateEntry(address, label, isMine, purpose, status);
    }catch (...){
        std::cout << "Exception updateAddressBook" << std::endl;
    }
}
void WalletModel::updateAddressBook(const QString &pubCoin, const QString &isUsed, int status)
{
    try{
        if(addressTableModel)
            addressTableModel->updateEntry(pubCoin, isUsed, status);
    }catch (...){
        std::cout << "Exception updateAddressBook" << std::endl;
    }
}


void WalletModel::updateWatchOnlyFlag(bool fHaveWatchonly)
{
    fHaveWatchOnly = fHaveWatchonly;
    emit notifyWatchonlyChanged(fHaveWatchonly);
}

void WalletModel::updateMultiSigFlag(bool fHaveMultiSig)
{
    this->fHaveMultiSig = fHaveMultiSig;
    emit notifyMultiSigChanged(fHaveMultiSig);
}

bool WalletModel::getMint(const uint256& hashSerial, CZerocoinMint& mint){
    return wallet->GetMint(hashSerial, mint);
}

bool WalletModel::validateAddress(const QString& address)
{
    CBitcoinAddress addressParsed(address.toStdString());
    return addressParsed.IsValid();
}

bool WalletModel::updateAddressBookLabels(const CTxDestination& dest, const std::string& strName, const std::string& strPurpose)
{
    LOCK(wallet->cs_wallet);

    std::map<CTxDestination, CAddressBookData>::iterator mi = wallet->mapAddressBook.find(dest);

    // Check if we have a new address or an updated label
    if (mi == wallet->mapAddressBook.end()) {
        return wallet->SetAddressBook(dest, strName, strPurpose);
    } else if (mi->second.name != strName) {
        return wallet->SetAddressBook(dest, strName, ""); // "" means don't change purpose
    }
    return false;
}

WalletModel::SendCoinsReturn WalletModel::prepareTransaction(WalletModelTransaction& transaction, const CCoinControl* coinControl)
{
    CAmount total = 0;
    QList<SendCoinsRecipient> recipients = transaction.getRecipients();
    std::vector<std::pair<CScript, CAmount> > vecSend;

    if (recipients.empty()) {
        return OK;
    }

    if (isAnonymizeOnlyUnlocked()) {
        return AnonymizeOnlyUnlocked;
    }

    QSet<QString> setAddress; // Used to detect duplicates
    int nAddresses = 0;

    // Pre-check input data for validity
    foreach (const SendCoinsRecipient& rcp, recipients) {
        if (rcp.paymentRequest.IsInitialized()) { // PaymentRequest...
            CAmount subtotal = 0;
            const payments::PaymentDetails& details = rcp.paymentRequest.getDetails();
            for (int i = 0; i < details.outputs_size(); i++) {
                const payments::Output& out = details.outputs(i);
                if (out.amount() <= 0) continue;
                subtotal += out.amount();
                const unsigned char* scriptStr = (const unsigned char*)out.script().data();
                CScript scriptPubKey(scriptStr, scriptStr + out.script().size());
                vecSend.push_back(std::pair<CScript, CAmount>(scriptPubKey, out.amount()));
            }
            if (subtotal <= 0) {
                return InvalidAmount;
            }
            total += subtotal;
        } else { // User-entered pivx address / amount:
            if (!validateAddress(rcp.address)) {
                return InvalidAddress;
            }
            if (rcp.amount <= 0) {
                return InvalidAmount;
            }
            setAddress.insert(rcp.address);
            ++nAddresses;

            CScript scriptPubKey = GetScriptForDestination(CBitcoinAddress(rcp.address.toStdString()).Get());
            vecSend.push_back(std::pair<CScript, CAmount>(scriptPubKey, rcp.amount));

            total += rcp.amount;
        }
    }
    if (setAddress.size() != nAddresses) {
        return DuplicateAddress;
    }

    CAmount nBalance = getBalance(coinControl);

    if (total > nBalance) {
        return AmountExceedsBalance;
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        transaction.newPossibleKeyChange(wallet);
        CAmount nFeeRequired = 0;
        std::string strFailReason;

        CWalletTx* newTx = transaction.getTransaction();
        CReserveKey* keyChange = transaction.getPossibleKeyChange();


        if (recipients[0].useSwiftTX && total > sporkManager.GetSporkValue(SPORK_5_MAX_VALUE) * COIN) {
            emit message(tr("Send Coins"), tr("SwiftX doesn't support sending values that high yet. Transactions are currently limited to %1 PIV.").arg(sporkManager.GetSporkValue(SPORK_5_MAX_VALUE)),
                CClientUIInterface::MSG_ERROR);
            return TransactionCreationFailed;
        }

        bool fCreated = wallet->CreateTransaction(vecSend, *newTx, *keyChange, nFeeRequired, strFailReason, coinControl, recipients[0].inputType, recipients[0].useSwiftTX);
        transaction.setTransactionFee(nFeeRequired);

        if (recipients[0].useSwiftTX && newTx->GetValueOut() > sporkManager.GetSporkValue(SPORK_5_MAX_VALUE) * COIN) {
            emit message(tr("Send Coins"), tr("SwiftX doesn't support sending values that high yet. Transactions are currently limited to %1 PIV.").arg(sporkManager.GetSporkValue(SPORK_5_MAX_VALUE)),
                CClientUIInterface::MSG_ERROR);
            return TransactionCreationFailed;
        }

        if (!fCreated) {
            if ((total + nFeeRequired) > nBalance) {
                return SendCoinsReturn(AmountWithFeeExceedsBalance);
            }
            emit message(tr("Send Coins"), QString::fromStdString(strFailReason),
                CClientUIInterface::MSG_ERROR);
            return TransactionCreationFailed;
        }

        // reject insane fee
        if (nFeeRequired > ::minRelayTxFee.GetFee(transaction.getTransactionSize()) * 10000)
            return InsaneFee;
    }

    return SendCoinsReturn(OK);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(WalletModelTransaction& transaction)
{
    QByteArray transaction_array; /* store serialized transaction */

    if (isAnonymizeOnlyUnlocked()) {
        return AnonymizeOnlyUnlocked;
    }

    // Double check tx before do anything
    CValidationState state;
    if(!CheckTransaction(*transaction.getTransaction(), true, true, state, true)){
        return TransactionCommitFailed;
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);
        CWalletTx* newTx = transaction.getTransaction();
        QList<SendCoinsRecipient> recipients = transaction.getRecipients();

        // Store PaymentRequests in wtx.vOrderForm in wallet.
        foreach (const SendCoinsRecipient& rcp, recipients) {
            if (rcp.paymentRequest.IsInitialized()) {
                std::string key("PaymentRequest");
                std::string value;
                rcp.paymentRequest.SerializeToString(&value);
                newTx->vOrderForm.push_back(std::make_pair(key, value));
            } else if (!rcp.message.isEmpty()) // Message from normal pivx:URI (pivx:XyZ...?message=example)
            {
                newTx->vOrderForm.push_back(std::make_pair("Message", rcp.message.toStdString()));
            }
        }

        CReserveKey* keyChange = transaction.getPossibleKeyChange();
        if (!wallet->CommitTransaction(*newTx, *keyChange, (recipients[0].useSwiftTX) ? "ix" : "tx"))
            return TransactionCommitFailed;

        CTransaction* t = (CTransaction*)newTx;
        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << *t;
        transaction_array.append(&(ssTx[0]), ssTx.size());
    }

    // Add addresses / update labels that we've sent to to the address book,
    // and emit coinsSent signal for each recipient
    foreach (const SendCoinsRecipient& rcp, transaction.getRecipients()) {
        // Don't touch the address book when we have a payment request
        if (!rcp.paymentRequest.IsInitialized()) {

            std::string strAddress = rcp.address.toStdString();
            CTxDestination dest = CBitcoinAddress(strAddress).Get();
            std::string strLabel = rcp.label.toStdString();

            updateAddressBookLabels(dest, strLabel, "send");
        }
        emit coinsSent(wallet, rcp, transaction_array);
    }
    checkBalanceChanged(); // update balance immediately, otherwise there could be a short noticeable delay until pollBalanceChanged hits

    return SendCoinsReturn(OK);
}

const CWalletTx* WalletModel::getTx(uint256 id){
    return wallet->GetWalletTx(id);
}

bool WalletModel::mintCoins(CAmount value, CCoinControl* coinControl ,std::string &strError){
    CWalletTx wtx;
    std::vector<CDeterministicMint> vMints;
    strError = wallet->MintZerocoin(value, wtx, vMints, coinControl);
    return strError.empty();
}


bool WalletModel::createZpivSpend(
        CWalletTx &wtxNew,
        std::vector<CZerocoinMint> &vMintsSelected,
        bool fMintChange,
        bool fMinimizeChange,
        CZerocoinSpendReceipt &receipt,
        std::list<std::pair<CBitcoinAddress*, CAmount>> outputs,
        std::string changeAddress
        ){

    CBitcoinAddress *changeAdd = (!changeAddress.empty()) ? new CBitcoinAddress(changeAddress) : nullptr;
    CAmount value = 0;
    for(std::pair<CBitcoinAddress*, CAmount> pair : outputs){
        value += pair.second;
    }

    if (wallet->IsLocked()) {
        receipt.SetStatus("Error: Wallet locked, unable to create transaction!", ZPIV_WALLET_LOCKED);
        return false;
    }

    CReserveKey reserveKey(wallet);
    std::vector<CDeterministicMint> vNewMints;
    if (!wallet->CreateZerocoinSpendTransaction(
            value,
            wtxNew,
            reserveKey,
            receipt,
            vMintsSelected,
            vNewMints,
            false, // No more mints
            fMinimizeChange,
            outputs,
            changeAdd
    )) {
        return false;
    }

    // Double check tx before do anything
    CValidationState state;
    return CheckTransaction(wtxNew, true, true, state, true);
}

bool WalletModel::sendZpiv(
        std::vector<CZerocoinMint> &vMintsSelected,
        bool fMintChange,
        bool fMinimizeChange,
        CZerocoinSpendReceipt &receipt,
        std::list<std::pair<CBitcoinAddress*, CAmount>> outputs,
        std::string changeAddress
        ){

    CBitcoinAddress *changeAdd = (!changeAddress.empty()) ? new CBitcoinAddress(changeAddress) : nullptr;
    CAmount value = 0;
    for(std::pair<CBitcoinAddress*, CAmount> pair : outputs){
        value += pair.second;
    }

    CWalletTx wtxNew;
    return wallet->SpendZerocoin(
            value,
            wtxNew,
            receipt,
            vMintsSelected,
            false, // No more mints
            fMinimizeChange,
            outputs,
            changeAdd
    );

}

bool WalletModel::convertBackZpiv(
        CAmount value,
        std::vector<CZerocoinMint> &vMintsSelected,
        bool fMintChange,
        bool fMinimizeChange,
        CZerocoinSpendReceipt &receipt
){
    CWalletTx wtxNew;
    return wallet->SpendZerocoin(
            value,
            wtxNew,
            receipt,
            vMintsSelected,
            false, // No more mints
            fMinimizeChange,
            std::list<std::pair<CBitcoinAddress*, CAmount>>(),
            nullptr
    );
}

OptionsModel* WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel* WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel* WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

RecentRequestsTableModel* WalletModel::getRecentRequestsTableModel()
{
    return recentRequestsTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if (!wallet->IsCrypted()) {
        return Unencrypted;
    } else if (wallet->fWalletUnlockAnonymizeOnly) {
        return UnlockedForAnonymizationOnly;
    } else if (wallet->IsLocked()) {
        return Locked;
    } else {
        return Unlocked;
    }

}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString& passphrase)
{
    if (encrypted) {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    } else {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString& passPhrase, bool anonymizeOnly)
{
    if (locked) {
        // Lock
        wallet->fWalletUnlockAnonymizeOnly = false;
        return wallet->Lock();
    } else {
        // Unlock
        return wallet->Unlock(passPhrase, anonymizeOnly);
    }
}

bool WalletModel::isAnonymizeOnlyUnlocked()
{
    return wallet->fWalletUnlockAnonymizeOnly;
}

bool WalletModel::changePassphrase(const SecureString& oldPass, const SecureString& newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString& filename)
{
    //attempt regular backup
    if(!BackupWallet(*wallet, filename.toLocal8Bit().data())) {
        return error("ERROR: Failed to backup wallet!");
    }

    return true;
}


// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel* walletmodel, CCryptoKeyStore* wallet)
{
    qDebug() << "NotifyKeyStoreStatusChanged";
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel* walletmodel, CWallet* wallet, const CTxDestination& address, const std::string& label, bool isMine, const std::string& purpose, ChangeType status)
{
    QString strAddress = QString::fromStdString(CBitcoinAddress(address).ToString());
    QString strLabel = QString::fromStdString(label);
    QString strPurpose = QString::fromStdString(purpose);

    qDebug() << "NotifyAddressBookChanged : " + strAddress + " " + strLabel + " isMine=" + QString::number(isMine) + " purpose=" + strPurpose + " status=" + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
        Q_ARG(QString, strAddress),
        Q_ARG(QString, strLabel),
        Q_ARG(bool, isMine),
        Q_ARG(QString, strPurpose),
        Q_ARG(int, status));
}

// queue notifications to show a non freezing progress dialog e.g. for rescan
static bool fQueueNotifications = false;
static std::vector<std::pair<uint256, ChangeType> > vQueueNotifications;
static void NotifyTransactionChanged(WalletModel* walletmodel, CWallet* wallet, const uint256& hash, ChangeType status)
{
    if (fQueueNotifications) {
        vQueueNotifications.push_back(std::make_pair(hash, status));
        return;
    }

    QString strHash = QString::fromStdString(hash.GetHex());

    qDebug() << "NotifyTransactionChanged : " + strHash + " status= " + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection /*,
                              Q_ARG(QString, strHash),
                              Q_ARG(int, status)*/);
}

static void ShowProgress(WalletModel* walletmodel, const std::string& title, int nProgress)
{
    // emits signal "showProgress"
    QMetaObject::invokeMethod(walletmodel, "showProgress", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(title)),
        Q_ARG(int, nProgress));
}

static void NotifyWatchonlyChanged(WalletModel* walletmodel, bool fHaveWatchonly)
{
    QMetaObject::invokeMethod(walletmodel, "updateWatchOnlyFlag", Qt::QueuedConnection,
        Q_ARG(bool, fHaveWatchonly));
}

static void NotifyMultiSigChanged(WalletModel* walletmodel, bool fHaveMultiSig)
{
    QMetaObject::invokeMethod(walletmodel, "updateMultiSigFlag", Qt::QueuedConnection,
                              Q_ARG(bool, fHaveMultiSig));
}

static void NotifyZerocoinChanged(WalletModel* walletmodel, CWallet* wallet, const std::string& hexString,
                                  const std::string& isUsed, ChangeType status)
{
    QString HexStr = QString::fromStdString(hexString);
    QString isUsedStr = QString::fromStdString(isUsed);
    qDebug() << "NotifyZerocoinChanged : " + HexStr + " " + isUsedStr + " status= " + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, HexStr),
                              Q_ARG(QString, isUsedStr),
                              Q_ARG(int, status));
}

static void NotifyzPIVReset(WalletModel* walletmodel)
{
    qDebug() << "NotifyzPIVReset";
    QMetaObject::invokeMethod(walletmodel, "checkBalanceChanged", Qt::QueuedConnection);
}

static void NotifyWalletBacked(WalletModel* model, const bool& fSuccess, const std::string& filename)
{
    std::string message;
    std::string title = "Backup ";
    CClientUIInterface::MessageBoxFlags method;

    if (fSuccess) {
        message = "The wallet data was successfully saved to ";
        title += "Successful: ";
        method = CClientUIInterface::MessageBoxFlags::MSG_INFORMATION;
    } else {
        message = "There was an error trying to save the wallet data to ";
        title += "Failed: ";
        method = CClientUIInterface::MessageBoxFlags::MSG_ERROR;
    }

    message += _(filename.data());

    QMetaObject::invokeMethod(model, "message", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(title)),
                              Q_ARG(QString, QString::fromStdString(message)),
                              Q_ARG(unsigned int, (unsigned int)method));
}

void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    wallet->ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2));
    wallet->NotifyWatchonlyChanged.connect(boost::bind(NotifyWatchonlyChanged, this, _1));
    wallet->NotifyMultiSigChanged.connect(boost::bind(NotifyMultiSigChanged, this, _1));
    wallet->NotifyZerocoinChanged.connect(boost::bind(NotifyZerocoinChanged, this, _1, _2, _3, _4));
    wallet->NotifyzPIVReset.connect(boost::bind(NotifyzPIVReset, this));
    wallet->NotifyWalletBacked.connect(boost::bind(NotifyWalletBacked, this, _1, _2));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    wallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
    wallet->NotifyWatchonlyChanged.disconnect(boost::bind(NotifyWatchonlyChanged, this, _1));
    wallet->NotifyMultiSigChanged.disconnect(boost::bind(NotifyMultiSigChanged, this, _1));
    wallet->NotifyZerocoinChanged.disconnect(boost::bind(NotifyZerocoinChanged, this, _1, _2, _3, _4));
    wallet->NotifyzPIVReset.disconnect(boost::bind(NotifyzPIVReset, this));
    wallet->NotifyWalletBacked.disconnect(boost::bind(NotifyWalletBacked, this, _1, _2));
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock(AskPassphraseDialog::Context context, bool relock)
{
    bool was_locked = getEncryptionStatus() == Locked;

    if (!was_locked && isAnonymizeOnlyUnlocked()) {
        setWalletLocked(true);
        wallet->fWalletUnlockAnonymizeOnly = false;
        was_locked = getEncryptionStatus() == Locked;
    }

    if (was_locked) {
        // Request UI to unlock wallet
        emit requireUnlock(context);
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(valid, relock);
    //    return UnlockContext(this, valid, was_locked && !isAnonymizeOnlyUnlocked());
}

WalletModel::UnlockContext::UnlockContext(bool valid, bool relock) : valid(valid), relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
/*
    if (valid && relock) {
        wallet->setWalletLocked(true);
    }
*/
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const
{
    return wallet->GetPubKey(address, vchPubKeyOut);
}

int64_t WalletModel::getCreationTime() const {
    return wallet->nTimeFirstKey;
}

int64_t WalletModel::getKeyCreationTime(const CPubKey& key){
    return pwalletMain->GetKeyCreationTime(key);
}

int64_t WalletModel::getKeyCreationTime(const CBitcoinAddress& address){
    if(this->isMine(address)) {
        return pwalletMain->GetKeyCreationTime(address);
    }
    return 0;
}

CBitcoinAddress WalletModel::getNewAddress(std::string label) const{
    if (!wallet->IsLocked())
        wallet->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey = wallet->GenerateNewKey();
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, label, "receive");
    return CBitcoinAddress(keyID);
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    LOCK2(cs_main, wallet->cs_wallet);
    for (const COutPoint& outpoint : vOutpoints) {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        bool fConflicted;
        const int nDepth = wallet->mapWallet[outpoint.hash].GetDepthAndMempool(fConflicted);
        if (nDepth < 0 || fConflicted) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth, true);
        vOutputs.push_back(out);
    }
}

bool WalletModel::isSpent(const COutPoint& outpoint) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->IsSpent(outpoint.hash, outpoint.n);
}

// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address)
void WalletModel::listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins);

    LOCK2(cs_main, wallet->cs_wallet); // ListLockedCoins, mapWallet
    std::vector<COutPoint> vLockedCoins;
    wallet->ListLockedCoins(vLockedCoins);

    // add locked coins
    for (const COutPoint& outpoint : vLockedCoins) {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        bool fConflicted;
        int nDepth = wallet->mapWallet[outpoint.hash].GetDepthAndMempool(fConflicted);
        if (nDepth < 0 || fConflicted) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth, true);
        if (outpoint.n < out.tx->vout.size() && wallet->IsMine(out.tx->vout[outpoint.n]) == ISMINE_SPENDABLE)
            vCoins.push_back(out);
    }

    for (const COutput& out : vCoins) {
        COutput cout = out;

        while (wallet->IsChange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0 && wallet->IsMine(cout.tx->vin[0])) {
            if (!wallet->mapWallet.count(cout.tx->vin[0].prevout.hash)) break;
            cout = COutput(&wallet->mapWallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0, true);
        }

        CTxDestination address;
        if (!out.fSpendable || !ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address))
            continue;
        mapCoins[QString::fromStdString(CBitcoinAddress(address).ToString())].push_back(out);
    }
}

bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->IsLockedCoin(hash, n);
}

void WalletModel::lockCoin(COutPoint& output)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->LockCoin(output);
}

void WalletModel::unlockCoin(COutPoint& output)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->UnlockCoin(output);
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->ListLockedCoins(vOutpts);
}


void WalletModel::listZerocoinMints(std::set<CMintMeta>& setMints, bool fUnusedOnly, bool fMaturedOnly, bool fUpdateStatus, bool fWrongSeed)
{
    setMints.clear();
    setMints = pwalletMain->zpivTracker->ListMints(fUnusedOnly, fMaturedOnly, fUpdateStatus, fWrongSeed);
}

void WalletModel::loadReceiveRequests(std::vector<std::string>& vReceiveRequests)
{
    LOCK(wallet->cs_wallet);
    for (const PAIRTYPE(CTxDestination, CAddressBookData) & item : wallet->mapAddressBook)
        for (const PAIRTYPE(std::string, std::string) & item2 : item.second.destdata)
            if (item2.first.size() > 2 && item2.first.substr(0, 2) == "rr") // receive request
                vReceiveRequests.push_back(item2.second);
}

bool WalletModel::saveReceiveRequest(const std::string& sAddress, const int64_t nId, const std::string& sRequest)
{
    CTxDestination dest = CBitcoinAddress(sAddress).Get();

    std::stringstream ss;
    ss << nId;
    std::string key = "rr" + ss.str(); // "rr" prefix = "receive request" in destdata

    LOCK(wallet->cs_wallet);
    if (sRequest.empty())
        return wallet->EraseDestData(dest, key);
    else
        return wallet->AddDestData(dest, key, sRequest);
}

bool WalletModel::isMine(CBitcoinAddress address)
{
    return IsMine(*wallet, address.Get());
}

bool WalletModel::isUsed(CBitcoinAddress address)
{
    return wallet->IsUsed(address);
}

std::string WalletModel::resetMintZerocoin(){
    return wallet->ResetMintZerocoin();
}

std::string WalletModel::resetSpentZerocoin(){
    return wallet->ResetSpentZerocoin();
}
