// Copyright (c) 2019-2022 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/guitransactionsutils.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"

namespace GuiTransactionsUtils {

    QString ProcessSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn, WalletModel *walletModel,
                                   CClientUIInterface::MessageBoxFlags& informType, const QString &msgArg,
                                   bool fPrepare)
    {
        QString retStr;
        informType = CClientUIInterface::MSG_WARNING;
        // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
        // WalletModel::TransactionCheckFailed and WalletModel::TransactionCommitFailed
        // are used only in WalletModel::sendCoins(). All others are used only in WalletModel::prepareTransaction()
        switch (sendCoinsReturn.status) {
            case WalletModel::InvalidAddress:
                retStr = QObject::tr("The recipient address is not valid, please recheck.");
                break;
            case WalletModel::InvalidAmount:
                retStr = QObject::tr("The amount to pay must be larger than 0.");
                break;
            case WalletModel::AmountExceedsBalance:
                retStr = QObject::tr("The amount to pay exceeds the available balance.");
                break;
            case WalletModel::AmountWithFeeExceedsBalance:
                retStr = QObject::tr(
                        "The total amount to pay exceeds the available balance when the %1 transaction fee is included.").arg(msgArg);
                break;
            case WalletModel::DuplicateAddress:
                retStr = QObject::tr(
                        "Duplicate address found, can only send to each address once per send operation.");
                break;
            case WalletModel::TransactionCreationFailed:
                informType = CClientUIInterface::MSG_ERROR;
                break;
            case WalletModel::TransactionCheckFailed:
                retStr = QObject::tr("The transaction is not valid!");
                informType = CClientUIInterface::MSG_ERROR;
                break;
            case WalletModel::TransactionCommitFailed:
                retStr = QString::fromStdString(sendCoinsReturn.commitRes.ToString());
                informType = CClientUIInterface::MSG_ERROR;
                break;
            case WalletModel::StakingOnlyUnlocked:
                // Unlock is only need when the coins are send
                if (!fPrepare) {
                    // Unlock wallet if it wasn't fully unlocked already
                    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
                    if (!ctx.isValid()) {
                        retStr = QObject::tr(
                                "Error: The wallet was unlocked for staking only. Unlock canceled.");
                    }
                } else
                    retStr = QObject::tr("Error: The wallet is unlocked for staking only. Fully unlock the wallet to send the transaction.");
                break;
            case WalletModel::InsaneFee:
                retStr = QObject::tr(
                        "A fee %1 times higher than %2 per kB is considered an insanely high fee.").arg(10000).arg(
                        BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                     ::minRelayTxFee.GetFeePerK()));
                break;
            // included to prevent a compiler warning.
            case WalletModel::OK:
            default:
                return retStr; // No issue
        }

        return retStr;
    }

    void ProcessSendCoinsReturnAndInform(PWidget* parent, const WalletModel::SendCoinsReturn& sendCoinsReturn, WalletModel* walletModel, const QString& msgArg, bool fPrepare)
    {
        CClientUIInterface::MessageBoxFlags informType;
        QString informMsg = ProcessSendCoinsReturn(sendCoinsReturn, walletModel, informType, msgArg, fPrepare);
        if (!informMsg.isEmpty()) parent->emitMessage(QObject::tr("Send Coins"), informMsg, informType, nullptr);
    }

}
