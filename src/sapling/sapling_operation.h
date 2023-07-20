// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_SAPLING_OPERATION_H
#define hemis_SAPLING_OPERATION_H

#include "amount.h"
#include "sapling/transaction_builder.h"
#include "operationresult.h"
#include "primitives/transaction.h"
#include "wallet/wallet.h"

class CCoinControl;
struct TxValues;

class ShieldedRecipient final : public CRecipientBase
{
public:
    const libzcash::SaplingPaymentAddress address;
    const std::string memo;
    ShieldedRecipient(const libzcash::SaplingPaymentAddress& _address, const CAmount& _amount, const std::string& _memo, bool _fSubtractFeeFromAmount) :
        CRecipientBase(_amount, _fSubtractFeeFromAmount), address(_address), memo(_memo) {}
    bool isTransparent() const override { return false; }
    Optional<libzcash::SaplingPaymentAddress> getSapPaymentAddr() const override { return {address}; };
    std::string getMemo() const override { return memo; }
};

struct SendManyRecipient
{
    const std::shared_ptr<CRecipientBase> recipient;

    bool IsTransparent() const { return recipient->isTransparent(); }
    bool IsSubtractFee() const { return recipient->fSubtractFeeFromAmount; }
    CAmount getAmount() const { return recipient->nAmount; };
    CScript getScript() const { assert(IsTransparent()); return *recipient->getScript(); }
    libzcash::SaplingPaymentAddress getSapPaymentAddr() const { assert(!IsTransparent()); return *recipient->getSapPaymentAddr(); }
    std::string getMemo() const { return recipient->getMemo(); }

    // Prevent default empty initialization
    SendManyRecipient() = delete;

    // Shielded recipient
    SendManyRecipient(const libzcash::SaplingPaymentAddress& address, const CAmount& amount, const std::string& memo, bool fSubtractFeeFromAmount):
            recipient(new ShieldedRecipient(address, amount, memo, fSubtractFeeFromAmount))
    {}

    // Transparent recipient: P2PKH
    SendManyRecipient(const CTxDestination& dest, const CAmount& amount, bool fSubtractFeeFromAmount):
            recipient(new CRecipient(GetScriptForDestination(dest), amount, fSubtractFeeFromAmount))
    {}

    // Transparent recipient: P2CS
    SendManyRecipient(const CKeyID& ownerKey, const CKeyID& stakerKey, const CAmount& amount, bool fV6Enforced):
            recipient(new CRecipient(fV6Enforced ? GetScriptForStakeDelegation(stakerKey, ownerKey)
                        : GetScriptForStakeDelegationLOF(stakerKey, ownerKey), amount, false))
    {}

    // Transparent recipient: multisig
    SendManyRecipient(int nRequired, const std::vector<CPubKey>& keys, const CAmount& amount):
            recipient(new CRecipient(GetScriptForMultisig(nRequired, keys), amount, false))
    {}

    // Transparent recipient: OP_RETURN
    SendManyRecipient(const uint256& message):
            recipient(new CRecipient(GetScriptForOpReturn(message), 0, false))
    {}
};

class FromAddress {
public:
    explicit FromAddress() {};
    explicit FromAddress(const CTxDestination& _fromTaddr) : fromTaddr(_fromTaddr) {};
    explicit FromAddress(const libzcash::SaplingPaymentAddress& _fromSapaddr) : fromSapAddr(_fromSapaddr) {};

    bool isFromTAddress() const { return IsValidDestination(fromTaddr); }
    bool isFromSapAddress() const { return fromSapAddr.is_initialized(); }

    CTxDestination fromTaddr{CNoDestination()};
    Optional<libzcash::SaplingPaymentAddress> fromSapAddr{nullopt};
};

class SaplingOperation {
public:
    explicit SaplingOperation(const Consensus::Params& consensusParams, CWallet* _wallet);
    ~SaplingOperation();

    OperationResult build();
    OperationResult send(std::string& retTxHash);
    OperationResult buildAndSend(std::string& retTxHash);

    void setFromAddress(const CTxDestination&);
    void setFromAddress(const libzcash::SaplingPaymentAddress&);
    void clearTx() { txBuilder.Clear(); }
    // In case of no addressFrom filter selected, it will accept any utxo in the wallet as input.
    SaplingOperation* setSelectTransparentCoins(const bool select, const bool _fIncludeDelegated = false);
    SaplingOperation* setSelectShieldedCoins(const bool select) { selectFromShield = select; return this; };
    SaplingOperation* setRecipients(std::vector<SendManyRecipient>& vec) { recipients = std::move(vec); return this; };
    SaplingOperation* setFee(CAmount _fee) { fee = _fee; return this; }
    SaplingOperation* setMinDepth(int _mindepth) { assert(_mindepth >= 0); mindepth = _mindepth; return this; }
    SaplingOperation* setTransparentKeyChange(CReserveKey* reserveKey) { tkeyChange = reserveKey; return this; }
    SaplingOperation* setCoinControl(const CCoinControl* _coinControl) { coinControl = _coinControl; return this; }

    CAmount getFee() { return fee; }
    CTransaction getFinalTx() { return *finalTx; }
    CTransactionRef getFinalTxRef() { return finalTx; }

private:
    /*
     * Cannot be nullptr. A pointer to the wallet, used to retrieve the inputs to spend, the keys to create the outputs,
     * sapling notes and nullifiers, as well as to commit transactions.
     * The same keystore is passed to the transaction builder in order to produce the required signatures.
     */
    CWallet* wallet{nullptr};

    FromAddress fromAddress;
    // In case of no addressFrom filter selected, it will accept any utxo in the wallet as input.
    bool selectFromtaddrs{false};
    bool selectFromShield{false};
    bool fIncludeDelegated{false};
    const CCoinControl* coinControl{nullptr};
    std::vector<SendManyRecipient> recipients;
    std::vector<COutput> transInputs;
    std::vector<SaplingNoteEntry> shieldedInputs;
    int mindepth{5}; // Min default depth 5.
    CAmount fee{0};  // User selected fee.

    // transparent change
    CReserveKey* tkeyChange{nullptr};

    // Builder
    TransactionBuilder txBuilder;
    CTransactionRef finalTx;

    OperationResult loadUtxos(TxValues& values);
    OperationResult loadUtxos(TxValues& txValues, const std::vector<COutput>& selectedUTXO, const CAmount selectedUTXOAmount);
    OperationResult loadUnspentNotes(TxValues& txValues, uint256& ovk);
    OperationResult checkTxValues(TxValues& txValues, bool isFromtAddress, bool isFromShielded);
};

OperationResult GetMemoFromString(const std::string& s, std::array<unsigned char, ZC_MEMO_SIZE>& memoRet);

OperationResult CheckTransactionSize(std::vector<SendManyRecipient>& recipients, bool fromTaddr);

#endif //hemis_SAPLING_OPERATION_H
