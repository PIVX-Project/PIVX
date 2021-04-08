//Copyright (c) 2019-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Sombe/guitransactionsutils.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"

namespace GuiTransactionsUtils {

    QString ProcessSendCoinsReturn(PWidget::Translator *parent, const WalletModel::SendCoinsReturn &sendCoinsReturn,
                                WalletModel *walletModel, CClientUIInterface::MessageBoxFlags& informType, const QString &msgArg,
                                bool fPrepare)
    {
        QString retStr;
        informType = CClientUIInterface::MSG_WARNING;
        // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
        // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
        // all others are used only in WalletModel::prepareTransaction()
        switch (sendCoinsReturn.status) {
            case WalletModel::InvalidAddress:
                retStr = parent->translate("The recipient address is not valid, please recheck.");
                break;
            case WalletModel::InvalidAmount:
                retStr = parent->translate("The amount to pay must be larger than 0.");
                break;
            case WalletModel::AmountExceedsBalance:
                retStr = parent->translate("The amount exceeds your balance.");
                break;
            case WalletModel::AmountWithFeeExceedsBalance:
                retStr = parent->translate(
                        "The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
                break;
            case WalletModel::DuplicateAddress:
                retStr = parent->translate(
                        "Duplicate address found, can only send to each address once per send operation.");
                break;
            case WalletModel::TransactionCreationFailed:
                informType = CClientUIInterface::MSG_ERROR;
                break;
            case WalletModel::TransactionCommitFailed:
                retStr = parent->translate(
                        "The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
                informType = CClientUIInterface::MSG_ERROR;
                break;
            case WalletModel::StakingOnlyUnlocked:
                // Unlock is only need when the coins are send
                if (!fPrepare) {
                    // Unlock wallet if it wasn't fully unlocked already
                    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
                    if (!ctx.isValid()) {
                        retStr = parent->translate(
                                "Error: The wallet was unlocked for staking only. Unlock canceled.");
                    }
                } else
                    retStr = parent->translate("Error: The wallet is unlocked for staking only. Fully unlock the wallet to send the transaction.");
                break;
            case WalletModel::InsaneFee:
                retStr = parent->translate(
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
        QString informMsg = ProcessSendCoinsReturn(parent, sendCoinsReturn, walletModel, informType, msgArg, fPrepare);
        if (!informMsg.isEmpty()) parent->emitMessage(parent->translate("Send Coins"), informMsg, informType, 0);
    }

}
