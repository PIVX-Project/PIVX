// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_SAPLING_OPERATION_H
#define PIVX_SAPLING_OPERATION_H

#include "amount.h"
#include "sapling/transaction_builder.h"
#include "operationresult.h"
#include "primitives/transaction.h"
#include "wallet/wallet.h"

// transaction.h comment: spending taddr output requires CTxIn >= 148 bytes and typical taddr txout is 34 bytes
#define CTXIN_SPEND_DUST_SIZE   148
#define CTXOUT_REGULAR_SIZE     34

class CCoinControl;
struct TxValues;

struct ShieldRecipient
{
    const libzcash::SaplingPaymentAddress address;
    const CAmount amount;
    const std::string memo;
    ShieldRecipient(const libzcash::SaplingPaymentAddress& _address, const CAmount& _amount, const std::string& _memo) :
        address(_address),
        amount(_amount),
        memo(_memo)
    {}
};

struct SendManyRecipient
{
    const Optional<ShieldRecipient> shieldRecipient{nullopt};
    const Optional<CTxOut> transparentRecipient{nullopt};

    bool IsTransparent() const { return transparentRecipient != nullopt; }

    // Prevent default empty initialization
    SendManyRecipient() = delete;

    // Shield recipient
    SendManyRecipient(const libzcash::SaplingPaymentAddress& address, const CAmount& amount, const std::string& memo):
        shieldRecipient(ShieldRecipient(address, amount, memo))
    {}

    // Transparent recipient: P2PKH
    SendManyRecipient(const CTxDestination& dest, const CAmount& amount):
        transparentRecipient(CTxOut(amount, GetScriptForDestination(dest)))
    {}

    // Transparent recipient: P2CS
    SendManyRecipient(const CKeyID& ownerKey, const CKeyID& stakerKey, const CAmount& amount):
        transparentRecipient(CTxOut(amount, GetScriptForStakeDelegation(stakerKey, ownerKey)))
    {}

    // Transparent recipient: multisig
    SendManyRecipient(int nRequired, const std::vector<CPubKey>& keys, const CAmount& amount):
        transparentRecipient(CTxOut(amount, GetScriptForMultisig(nRequired, keys)))
    {}

    // Transparent recipient: OP_RETURN
    SendManyRecipient(const uint256& message):
        transparentRecipient(CTxOut(0, GetScriptForOpReturn(message)))
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
    explicit SaplingOperation(const Consensus::Params& consensusParams, int chainHeight) : txBuilder(consensusParams, chainHeight) {};
    explicit SaplingOperation(TransactionBuilder& _builder) : txBuilder(_builder) {};

    ~SaplingOperation() { delete tkeyChange; }

    OperationResult build();
    OperationResult send(std::string& retTxHash);
    OperationResult buildAndSend(std::string& retTxHash);

    void setFromAddress(const CTxDestination&);
    void setFromAddress(const libzcash::SaplingPaymentAddress&);
    void clearTx() { txBuilder.Clear(); }
    // In case of no addressFrom filter selected, it will accept any utxo in the wallet as input.
    SaplingOperation* setSelectTransparentCoins(const bool select, const bool _fIncludeDelegated = false);
    SaplingOperation* setSelectShieldCoins(const bool select) { selectFromShield = select; return this; };
    SaplingOperation* setRecipients(std::vector<SendManyRecipient>& vec) { recipients = std::move(vec); return this; };
    SaplingOperation* setFee(CAmount _fee) { fee = _fee; return this; }
    SaplingOperation* setMinDepth(int _mindepth) { assert(_mindepth >= 0); mindepth = _mindepth; return this; }
    SaplingOperation* setTxBuilder(TransactionBuilder& builder) { txBuilder = builder; return this; }
    SaplingOperation* setTransparentKeyChange(CReserveKey* reserveKey) { tkeyChange = reserveKey; return this; }
    SaplingOperation* setCoinControl(const CCoinControl* _coinControl) { coinControl = _coinControl; return this; }

    CAmount getFee() { return fee; }
    CTransaction getFinalTx() { return finalTx; }

private:
    FromAddress fromAddress;
    // In case of no addressFrom filter selected, it will accept any utxo in the wallet as input.
    bool selectFromtaddrs{false};
    bool selectFromShield{false};
    bool fIncludeDelegated{false};
    const CCoinControl* coinControl{nullptr};
    std::vector<SendManyRecipient> recipients;
    std::vector<COutput> transInputs;
    std::vector<SaplingNoteEntry> shieldInputs;
    int mindepth{5}; // Min default depth 5.
    CAmount fee{0};  // User selected fee.

    // transparent change
    CReserveKey* tkeyChange{nullptr};

    // Builder
    TransactionBuilder txBuilder;
    CTransaction finalTx;

    OperationResult loadUtxos(TxValues& values);
    OperationResult loadUtxos(TxValues& txValues, const std::vector<COutput>& selectedUTXO, const CAmount selectedUTXOAmount);
    OperationResult loadUnspentNotes(TxValues& txValues, uint256& ovk);
    OperationResult checkTxValues(TxValues& txValues, bool isFromtAddress, bool isFromShield);
};

OperationResult GetMemoFromString(const std::string& s, std::array<unsigned char, ZC_MEMO_SIZE>& memoRet);

OperationResult CheckTransactionSize(std::vector<SendManyRecipient>& recipients, bool fromTaddr);

#endif //PIVX_SAPLING_OPERATION_H
