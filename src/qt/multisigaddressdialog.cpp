// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "multisigaddressdialog.h"
#include "forms/ui_multisigaddressdialog.h"

#include "../primitives/transaction.h"
#include "addressbookpage.h"
#include "../utilstrencodings.h"
#include "../core_io.h"
#include "../script/script.h"
#include "../base58.h"
#include "../coins.h"
#include "../keystore.h"
#include "../init.h"
#include "../wallet.h"
#include "../script/sign.h"

#include "bitcoinunits.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "qvalidatedlineedit.h"
#include "bitcoinamountfield.h"

#include <string>
#include <vector>

#include <QtCore/QVariant>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QMap>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QClipboard>
#include <QDebug>


using namespace boost;


MultisigAddressDialog::MultisigAddressDialog(QWidget* parent) :  QDialog(parent),
                                                                 ui(new Ui::MultisigAddressDialog),
                                                                 model(0)
{
    ui->setupUi(this);
    ui->addMultisigButton->installEventFilter(this);


    //populate lists with a single object
    on_addAddressButton_SIG_clicked();
    on_addAddressButton_ADD_clicked();
    on_addInputButton_CRE_clicked();
    on_addInputButton_SIG_clicked();
    on_addDestinationButton_clicked();

}

MultisigAddressDialog::~MultisigAddressDialog()
{
    delete ui;
}

void MultisigAddressDialog::setAddress(const QString& address)
{

}

void MultisigAddressDialog::setModel(WalletModel* model)
{
    this->model = model;
}

void MultisigAddressDialog::showPage(int pageNumber)
{
    ui->multisigTabWidget->setCurrentIndex(pageNumber);
    this->show();
}

void MultisigAddressDialog::on_addAddressButton_ADD_clicked()
{
    if(ui->addressList_ADD->count() > 15){
        ui->addMultisigStatus->setStyleSheet("QLabel { color: red; }");
        ui->addMultisigStatus->setText("Maximum possible addresses reached. (16)");
        return;
    }

    QFrame* address = createAddress(ui->addressList_ADD->count()+1);

    ui->addressList_ADD->addWidget(address);
}

void MultisigAddressDialog::on_addAddressButton_SIG_clicked()
{
    if(ui->addressList_SIG->count() > 15){
        ui->signButtonStatus->setStyleSheet("QLabel { color: red; }");
        ui->signButtonStatus->setText("Maximum possible addresses reached. (16)");
        return;
    }

    QFrame* address = createAddress(ui->addressList_SIG->count()+1);

    ui->addressList_SIG->addWidget(address);
}

QFrame* MultisigAddressDialog::createAddress(int labelNumber)
{
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);

    QFrame* addressFrame = new QFrame(ui->addressScrollAreaContents);
    sizePolicy.setHeightForWidth(addressFrame->sizePolicy().hasHeightForWidth());
    addressFrame->setSizePolicy(sizePolicy);
    addressFrame->setFrameShape(QFrame::StyledPanel);
    addressFrame->setFrameShadow(QFrame::Raised);
    addressFrame->setObjectName(QStringLiteral("addressFrame"));

    QVBoxLayout* frameLayout = new QVBoxLayout(addressFrame);
    frameLayout->setSpacing(1);
    frameLayout->setObjectName(QStringLiteral("frameLayout"));
    frameLayout->setContentsMargins(6, 6, 6, 6);

    QHBoxLayout* addressLayout = new QHBoxLayout();
    addressLayout->setSpacing(0);
    addressLayout->setObjectName(QStringLiteral("addressLayout"));

    QLabel* addressLabel = new QLabel(addressFrame);
    addressLabel->setObjectName(QStringLiteral("addressLabel"));
    addressLabel->setText(QApplication::translate("MultisigAddressDialog", strprintf("Address %i:", labelNumber).c_str() , 0));
    addressLayout->addWidget(addressLabel);

    QValidatedLineEdit* address = new QValidatedLineEdit(addressFrame);
    address->setObjectName(QStringLiteral("address"));

    addressLayout->addWidget(address);

    QPushButton* addressBookButton = new QPushButton(addressFrame);
    addressBookButton->setObjectName(QStringLiteral("addressBookButton"));
    QIcon icon3;
    icon3.addFile(QStringLiteral(":/icons/address-book"), QSize(), QIcon::Normal, QIcon::Off);
    addressBookButton->setIcon(icon3);
    addressBookButton->setAutoDefault(false);

    addressLayout->addWidget(addressBookButton);

    QPushButton* addressPasteButton = new QPushButton(addressFrame);
    addressPasteButton->setObjectName(QStringLiteral("addressPasteButton"));
    QIcon icon4;
    icon4.addFile(QStringLiteral(":/icons/editpaste"), QSize(), QIcon::Normal, QIcon::Off);
    addressPasteButton->setIcon(icon4);
    addressPasteButton->setAutoDefault(false);

    addressLayout->addWidget(addressPasteButton);

    frameLayout->addLayout(addressLayout);

    return addressFrame;
}


void MultisigAddressDialog::on_addressBookButton_clicked()
{
    if (model && model->getAddressTableModel()) {
        AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec()) {
            setAddress(dlg.getReturnValue());
        }
    }
}

void MultisigAddressDialog::on_pasteButton_clicked()
{
    setAddress(QApplication::clipboard()->text());
}

void MultisigAddressDialog::on_addMultisigButton_clicked()
{
    if(!model)
        return;

    int m = ui->enterMSpinbox->value();
    string label = ui->multisigAddressLabel->text().toStdString();

    vector<string> keys;

    for (int i = 0; i < ui->addressList_ADD->count(); i++) {
        QWidget* address = qobject_cast<QWidget*>(ui->addressList_ADD->itemAt(i)->widget());
        QValidatedLineEdit* vle = address->findChild<QValidatedLineEdit*>("address");

        if(!vle->text().isEmpty()){
            keys.push_back(vle->text().toStdString());
        }
    }

    CScript redeem;
    try{
        redeem = createRedeemScript(m, keys);
        CScriptID innerID(redeem);
        pwalletMain->AddCScript(redeem);
        pwalletMain->SetAddressBook(innerID, label, "receive");
        ui->addMultisigStatus->setStyleSheet("QLabel { color: black; }");
        ui->addMultisigStatus->setText("Multisignature address " + QString::fromStdString(CBitcoinAddress(innerID).ToString()) + " has been added to the wallet.");
    }catch(const runtime_error& e) {
        ui->addMultisigStatus->setStyleSheet("QLabel { color: red; }");
        ui->addMultisigStatus->setText(QString::fromStdString(e.what()));
    }
}


void MultisigAddressDialog::on_addDestinationButton_clicked()
{
    QFrame* destinationFrame = new QFrame(ui->destinationsScrollAreaContents);
    destinationFrame->setObjectName(QStringLiteral("destinationFrame"));
    destinationFrame->setFrameShape(QFrame::StyledPanel);
    destinationFrame->setFrameShadow(QFrame::Raised);

    QVBoxLayout* frameLayout = new QVBoxLayout(destinationFrame);
    frameLayout->setObjectName(QStringLiteral("destinationFrameLayout"));
    QHBoxLayout* destinationLayout = new QHBoxLayout();
    destinationLayout->setSpacing(0);
    destinationLayout->setObjectName(QStringLiteral("destinationLayout"));
    QLabel* destinationAddressLabel = new QLabel(destinationFrame);
    destinationAddressLabel->setObjectName(QStringLiteral("destinationAddressLabel"));

    destinationLayout->addWidget(destinationAddressLabel);

    QValidatedLineEdit* destinationAddress = new QValidatedLineEdit(destinationFrame);
    destinationAddress->setObjectName(QStringLiteral("destinationAddress"));

    destinationLayout->addWidget(destinationAddress);

    QSpacerItem* horizontalSpacer = new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    destinationLayout->addItem(horizontalSpacer);

    QLabel* destinationAmountLabel = new QLabel(destinationFrame);
    destinationAmountLabel->setObjectName(QStringLiteral("destinationAmountLabel"));

    destinationLayout->addWidget(destinationAmountLabel);

    BitcoinAmountField* destinationAmount = new BitcoinAmountField(destinationFrame);
    destinationAmount->setObjectName(QStringLiteral("destinationAmount"));

    destinationAddressLabel->setText(QApplication::translate("MultisigAddressDialog", strprintf("%i. Address: ", ui->destinationsList->count()+1).c_str(), 0));
    destinationAmountLabel->setText(QApplication::translate("MultisigAddressDialog", "Amount: ", 0));

    destinationLayout->addWidget(destinationAmount);


    frameLayout->addLayout(destinationLayout);


    ui->destinationsList->addWidget(destinationFrame);
}

void MultisigAddressDialog::on_createButton_clicked()
{
    if(!model)
        return;
    CMutableTransaction rawTx;

    for(int i = 0; i < ui->inputsList_CRE->count(); i++){
        QWidget* input = qobject_cast<QWidget*>(ui->inputsList_CRE->itemAt(i)->widget());
        QLineEdit* txIdLine = input->findChild<QLineEdit*>("txInputId");
        QSpinBox* txVoutLine = input->findChild<QSpinBox*>("txInputVout");

        int nOutput = txVoutLine->value();
        if(nOutput < 0){
            ui->createButtonStatus->setStyleSheet("QLabel { color: red; }");
            ui->createButtonStatus->setText("Vout position must be positive.");
            return;
        }

        if(txIdLine->text().isEmpty()){
            ui->createButtonStatus->setStyleSheet("QLabel { color: red; }");
            ui->createButtonStatus->setText("Invalid Tx Hash.");
            return;
        }

        uint256 txid = uint256S(txIdLine->text().toStdString());
        CTxIn in(COutPoint(txid, nOutput));
        rawTx.vin.push_back(in);
    }

    set<CBitcoinAddress> setAddress;
    bool validInput = true;
    for(int i = 0; i < ui->destinationsList->count(); i++){
        QWidget* dest = qobject_cast<QWidget*>(ui->destinationsList->itemAt(i)->widget());
        QValidatedLineEdit* addr = dest->findChild<QValidatedLineEdit*>("destinationAddress");
        BitcoinAmountField* amt = dest->findChild<BitcoinAmountField*>("destinationAmount");
        CBitcoinAddress address;

        bool validDest = true;

        if(!model->validateAddress(addr->text())){
            addr->setValid(false);
            validDest = false;
        }else{
            address = CBitcoinAddress(addr->text().toStdString());
        }

        if(!amt->validate()){
            amt->setValid(false);
            validDest = false;
        }

        if(setAddress.count(address)){
            ui->createButtonStatus->setStyleSheet("QLabel { color: red; }");
            ui->createButtonStatus->setText("Invalid amount.");
            validDest = false;
        }

        if(!validDest){
            validInput = false;
            continue;
        }

        setAddress.insert(address);
        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CTxOut out(amt->value(), scriptPubKey);
        rawTx.vout.push_back(out);
    }



    if(validInput){
        ui->createButtonStatus->setText(QString::fromStdString(EncodeHexTx(rawTx)));
    }
}

void MultisigAddressDialog::on_addAddressButton_clicked()
{

}
void MultisigAddressDialog::on_addPKButton_clicked()
{
    QFrame* keyFrame = new QFrame(ui->keyScrollAreaContents);
    keyFrame->setObjectName(QStringLiteral("keyFrame"));
    keyFrame->setFrameShape(QFrame::StyledPanel);
    keyFrame->setFrameShadow(QFrame::Raised);
    QGridLayout* keyFrameGrid = new QGridLayout(keyFrame);
    keyFrameGrid->setObjectName(QStringLiteral("gridLayout_10"));
    QHBoxLayout* keyLayout = new QHBoxLayout();
    keyLayout->setObjectName(QStringLiteral("keyLayout"));
    QLabel* keyLabel = new QLabel(keyFrame);
    keyLabel->setObjectName(QStringLiteral("keyLabel"));
    keyLabel->setText(QApplication::translate("MultisigAddressDialog", strprintf("Key %i: ", (ui->keyList->count()+1)).c_str(), 0));

    keyLayout->addWidget(keyLabel);

    QLineEdit* key = new QLineEdit(keyFrame);
    key->setObjectName(QStringLiteral("key"));
    key->setEchoMode(QLineEdit::Password);

    keyLayout->addWidget(key);


    keyFrameGrid->addLayout(keyLayout, 0, 0, 1, 1);


    ui->keyList->addWidget(keyFrame);
}

void MultisigAddressDialog::on_signButton_clicked()
{
    if(!model)
        return;
   try{
        vector<unsigned char> txData(ParseHex(ui->transactionHex->text().toStdString()));

        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        vector<CMutableTransaction> txVariants;
        while (!ssData.empty()) {
            try {
                CMutableTransaction tx;
                ssData >> tx;
                txVariants.push_back(tx);
            } catch (const std::exception&) {
                throw runtime_error("TX decode failed");
            }
        }

        if (txVariants.empty())
            throw runtime_error("Missing transaction");
        // mergedTx will end up with all the signatures; it
        // starts as a clone of the rawtx:
        CMutableTransaction mergedTx(txVariants[0]);
        bool fComplete = true;

        // Fetch previous transactions (inputs):
        CCoinsView viewDummy;
        CCoinsViewCache view(&viewDummy);
        {
            LOCK(mempool.cs);
            CCoinsViewCache& viewChain = *pcoinsTip;
            CCoinsViewMemPool viewMempool(&viewChain, mempool);
            view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

            BOOST_FOREACH (const CTxIn& txin, mergedTx.vin) {
                const uint256& prevHash = txin.prevout.hash;
                CCoins coins;
                view.AccessCoins(prevHash); // this is certainly allowed to fail
            }

            view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
        }
        bool fGivenKeys = false;
        CBasicKeyStore tempKeystore;
        QVBoxLayout* keyList = ui->keyList;

        if (keyList->count() > 0) {
            fGivenKeys = true;

            for(int i = 0; i < keyList->count(); i++){
                QWidget* keyFrame = qobject_cast<QWidget*>(keyList->itemAt(i)->widget());
                QLineEdit* key = keyFrame->findChild<QLineEdit*>("key");
                CBitcoinSecret vchSecret;
                if (!vchSecret.SetString(key->text().toStdString()))
                    throw runtime_error("Invalid private key");
                CKey cKey = vchSecret.GetKey();
                if (!cKey.IsValid())
                    throw runtime_error("Private key outside allowed range");
                tempKeystore.AddKey(cKey);
            }
        }
    #ifdef ENABLE_WALLET
        else
            if (pwalletMain->IsLocked() || pwalletMain->fWalletUnlockAnonymizeOnly)
                throw runtime_error("Wallet is locked!");
    #endif
        // Add previous txouts given in the RPC call:
        if (ui->inputsList_SIG->count() > 0) {

            for(int i = 0; i < ui->inputsList_SIG->count(); i++){
                QWidget* input = qobject_cast<QWidget*>(ui->inputsList_SIG->itemAt(i)->widget());
                QLineEdit* txIdLine = input->findChild<QLineEdit*>("txInputId");
                QSpinBox* txVoutLine = input->findChild<QSpinBox*>("txInputVout");
                int nOutput = txVoutLine->value();
                if(nOutput < 0){
                    ui->signButtonStatus->setStyleSheet("QLabel { color: red; }");
                    ui->signButtonStatus->setText("Vout position must be positive.");
                    return;
                }

                uint256 txid = uint256S(txIdLine->text().toStdString());
                if(txIdLine->text().isEmpty()){
                    ui->signButtonStatus->setStyleSheet("QLabel { color: red; }");
                    ui->signButtonStatus->setText("Invalid Tx Hash.");
                    return;
                }

                CTransaction tx;
                uint256 hashBlock = 0;
                if (!GetTransaction(txid, tx, hashBlock, true))
                    throw runtime_error("No information available about transaction");
                CScript scriptPubKey;
                txnouttype type;
                vector<CTxDestination> addresses;
                int nRequired;

                if(tx.vout.size() >= nOutput){
                    scriptPubKey = tx.vout[nOutput].scriptPubKey;
                } else {
                    throw runtime_error("Output not found in transaction.");
                }

                if(!ExtractDestinations(scriptPubKey, type, addresses, nRequired)){
                    throw runtime_error("Could not find owned address associated with UTXO.");
                }

                if(!scriptPubKey.IsPayToScriptHash()){
                     throw runtime_error("UTXO was not paid to a script hash.");
                }

                CScriptID hash = get<CScriptID>(addresses[0]);
                CScript redeemScript;
                if (!pwalletMain->GetCScript(hash, redeemScript)){
                    throw runtime_error(strprintf("Could not extract redeem script from address: %s", CBitcoinAddress(addresses[0]).ToString()));
                }

                {
                    CCoinsModifier coins = view.ModifyCoins(txid);
                    if (coins->IsAvailable(nOutput) && coins->vout[nOutput].scriptPubKey != scriptPubKey) {
                        string err("Previous output scriptPubKey mismatch:\n");
                        err = err + coins->vout[nOutput].scriptPubKey.ToString() + "\nvs:\n" +
                              scriptPubKey.ToString();
                        throw runtime_error(err);
                    }
                    if ((unsigned int)nOutput >= coins->vout.size())
                        coins->vout.resize(nOutput + 1);
                    coins->vout[nOutput].scriptPubKey = scriptPubKey;
                    coins->vout[nOutput].nValue = 0; // we don't know the actual output value
                }
                tempKeystore.AddCScript(redeemScript);
            }
        }

    #ifdef ENABLE_WALLET
        const CKeyStore& keystore = ((fGivenKeys || !pwalletMain) ? tempKeystore : *pwalletMain);
    #else
        const CKeyStore& keystore = tempKeystore;
    #endif
        int nHashType = SIGHASH_ALL;
        // Sign what we can:

        for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
            CTxIn& txin = mergedTx.vin[i];
            const CCoins* coins = view.AccessCoins(txin.prevout.hash);
            if (coins == NULL || !coins->IsAvailable(txin.prevout.n)) {
                fComplete = false;
                continue;
            }
            const CScript& prevPubKey = coins->vout[txin.prevout.n].scriptPubKey;

            txin.scriptSig.clear();

            SignSignature(keystore, prevPubKey, mergedTx, i, nHashType);

            // ... and merge in other signatures:
            BOOST_FOREACH (const CMutableTransaction& txv, txVariants) {
                txin.scriptSig = CombineSignatures(prevPubKey, mergedTx, i, txin.scriptSig, txv.vin[i].scriptSig);
            }
            if (!VerifyScript(txin.scriptSig, prevPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&mergedTx, i)))
                fComplete = false;
        }


        if(fComplete){

            uint256 hashTx = mergedTx.GetHash();
            CCoinsViewCache& view = *pcoinsTip;
            const CCoins* existingCoins = view.AccessCoins(hashTx);
            bool fOverrideFees = false;
            bool fHaveMempool = mempool.exists(hashTx);
            bool fHaveChain = existingCoins && existingCoins->nHeight < 1000000000;

            if (!fHaveMempool && !fHaveChain) {
                // push to local node and sync with wallets
                CValidationState state;
                if (!AcceptToMemoryPool(mempool, state, mergedTx, false, NULL, !fOverrideFees)) {
                    if (state.IsInvalid())
                        throw runtime_error(strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
                    else
                        throw runtime_error(state.GetRejectReason());
                }
            } else if (fHaveChain) {
                throw runtime_error("transaction already in block chain");
            }
            RelayTransaction(mergedTx);
            ui->signButtonStatus->setText(QString::fromStdString(strprintf("Complete: True\n%s", hashTx.GetHex())));
        }else{
            ui->signButtonStatus->setText(QString::fromStdString(strprintf("Complete: False\n%s", EncodeHexTx(mergedTx))));
        }
    }catch(const runtime_error& e){
        ui->signButtonStatus->setText(e.what());
    }
}

void MultisigAddressDialog::on_addInputButton_CRE_clicked()
{

    QFrame* input = createInput(ui->inputsList_CRE->count()+1);

    ui->inputsList_CRE->addWidget(input);
//
//  THIS CRASHES THE WALLET ;)


//    CAmount txFee;
//    CAmount inputsAmount = 0;
//    CAmount outputsAmount = 0;

//    for(int i = 0; i < ui->inputsList_CRE->count(); i++){
//        QWidget* input = qobject_cast<QWidget*>(ui->inputsList_CRE->itemAt(i)->widget());
//        QLineEdit* txIdLine = input->findChild<QLineEdit*>("txInputId");
//        QSpinBox* txVout = input->findChild<QSpinBox*>("txInputVout");

//        uint256 txid = uint256S(txIdLine->text().toStdString());

//        CTransaction tx;
//        uint256 hashBlock = 0;
//        if (!GetTransaction(txid, tx, hashBlock, true))
//            throw runtime_error("No information available about transaction");

//        inputsAmount = inputsAmount + tx.vout[txVout->value()].nValue;
//    }

//    for(int i = 0; i < ui->destinationsList->count(); i++){
//        QWidget* dest = qobject_cast<QWidget*>(ui->destinationsList->itemAt(i)->widget());
//        BitcoinAmountField* amt = dest->findChild<BitcoinAmountField*>("destinationAmount");
//        outputsAmount = outputsAmount + amt->value();
//    }

//    txFee = inputsAmount - outputsAmount;

//    ui->fee->setText(BitcoinUnits::formatWithUnit(BitcoinUnits::PIV, txFee));
}


void MultisigAddressDialog::on_addInputButton_SIG_clicked()
{

    QFrame* input = createInput(ui->inputsList_SIG->count()+1);

    ui->inputsList_SIG->addWidget(input);
}

QFrame* MultisigAddressDialog::createInput(int labelNumber)
{
    QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);

    QFrame* txInputFrame = new QFrame(ui->txInputsWidget);
    sizePolicy.setHeightForWidth(txInputFrame->sizePolicy().hasHeightForWidth());
    txInputFrame->setFrameShape(QFrame::StyledPanel);
    txInputFrame->setFrameShadow(QFrame::Raised);
    txInputFrame->setObjectName(QStringLiteral("txInputFrame"));

    QVBoxLayout* frameLayout = new QVBoxLayout(txInputFrame);
    frameLayout->setSpacing(1);
    frameLayout->setObjectName(QStringLiteral("txInputFrameLayout"));
    frameLayout->setContentsMargins(6, 6, 6, 6);

    QHBoxLayout* txInputLayout = new QHBoxLayout();
    txInputLayout->setObjectName(QStringLiteral("txInputLayout"));

    QLabel* txInputIdLabel = new QLabel(txInputFrame);
    txInputIdLabel->setObjectName(QStringLiteral("txInputIdLabel"));
    txInputIdLabel->setText(QApplication::translate("MultisigAddressDialog", strprintf("%i. Tx Hash: ", labelNumber).c_str(), 0));
    txInputLayout->addWidget(txInputIdLabel);

    QLineEdit* txInputId = new QLineEdit(txInputFrame);
    txInputId->setObjectName(QStringLiteral("txInputId"));

    txInputLayout->addWidget(txInputId);

    QSpacerItem* horizontalSpacer = new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    txInputLayout->addItem(horizontalSpacer);

    QLabel* txInputVoutLabel = new QLabel(txInputFrame);
    txInputVoutLabel->setObjectName(QStringLiteral("txInputVoutLabel"));
    txInputVoutLabel->setText(QApplication::translate("MultisigAddressDialog", "Vout Position: ", 0));

    txInputLayout->addWidget(txInputVoutLabel);

    QSpinBox* txInputVout = new QSpinBox(txInputFrame);
    txInputVout->setObjectName("txInputVout");
    sizePolicy.setHeightForWidth(txInputVout->sizePolicy().hasHeightForWidth());
    txInputVout->setSizePolicy(sizePolicy);
//    QLineEdit* txInputVout = new QLineEdit(txInputFrame);
//    txInputVout->setObjectName(QStringLiteral("txInputVout"));
//    sizePolicy.setHeightForWidth(txInputVout->sizePolicy().hasHeightForWidth());
//    txInputVout->setSizePolicy(sizePolicy);

    txInputLayout->addWidget(txInputVout);

    frameLayout->addLayout(txInputLayout);

    return txInputFrame;
}

bool MultisigAddressDialog::eventFilter(QObject* object, QEvent* event)
{
//    size_t addrButton = (object->objectName()).toStdString().find("addressBook");
//    if(addrButton != string::npos){
//        if (model && model->getAddressTableModel()) {
//            AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
//            dlg.setModel(model->getAddressTableModel());
//            if (dlg.exec()) {
//                setAddress(dlg.getReturnValue());
//            }
//        }
//    }
//    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
//        if (ui->multisigTabWidget->currentIndex() == 0) {
//            /* Clear status message on focus change */
//            ui->addMultisigStatus->clear();
//        } else if (ui->multisigTabWidget->currentIndex() == 1) {
//            /* Clear status message on focus change */
//            ui->addMultisigStatus->clear();
//        }
//    }
    return QDialog::eventFilter(object, event);
}


CScript MultisigAddressDialog::createRedeemScript(int m, vector<string> keys)
{
    int n = keys.size();
    //gather pub keys
    if (n < 1)
        throw runtime_error("a Multisignature address must require at least one key to redeem");
    if (n < m)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %d keys, but need at least %d to redeem)",
                m, n));
    if (n > 16)
        throw runtime_error("Number of addresses involved in the Multisignature address creation > 16\nReduce the number");

    vector<CPubKey> pubkeys;
    pubkeys.resize(n);

    int i = 0;
    for(vector<string>::iterator it = keys.begin(); it != keys.end(); ++it) {
        string keyString = *it;
#ifdef ENABLE_WALLET
        // Case 1: PIVX address and we have full public key:
        CBitcoinAddress address(keyString);
        if (pwalletMain && address.IsValid()) {
            CKeyID keyID;
            if (!address.GetKeyID(keyID)) {
                throw runtime_error(
                    strprintf("%s does not refer to a key", keyString));
            }
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                throw runtime_error(
                    strprintf("no full public key for address %s", keyString));
            if (!vchPubKey.IsFullyValid()){
                string sKey = keyString.empty()?"(empty)":keyString;
                throw runtime_error(" Invalid public key: " + sKey );
            }
            pubkeys[i++] = vchPubKey;
        }

        //case 2: hex pub key
        else
#endif
            if (IsHex(keyString)) {
            CPubKey vchPubKey(ParseHex(keyString));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: " + keyString);
            pubkeys[i++] = vchPubKey;
        } else {
            throw runtime_error(" Invalid public key: " + keyString);
        }
    }

    return GetScriptForMultisig(m, pubkeys);
}

