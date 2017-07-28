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
#include "../script/interpreter.h"

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


//using namespace boost;


MultisigAddressDialog::MultisigAddressDialog(QWidget* parent) :  QDialog(parent),
                                                                 ui(new Ui::MultisigAddressDialog),
                                                                 model(0)
{
    ui->setupUi(this);
    ui->keyScrollArea->hide();

    isFirstPK = true;


    //populate lists with a single object
    on_addInputButton_clicked();
    on_addDestinationButton_clicked();
    on_addAddressButton_clicked();
}

MultisigAddressDialog::~MultisigAddressDialog()
{
    delete ui;
}

void MultisigAddressDialog::setModel(WalletModel* model)
{
    this->model = model;
}

void MultisigAddressDialog::showDialog()
{
    ui->multisigTabWidget->setCurrentIndex(0);
    this->show();
}

//slot for deleting QFrames with the delete buttons
void MultisigAddressDialog::deleteFrame()
{
   QWidget *buttonWidget = qobject_cast<QWidget*>(sender());
   if(!buttonWidget){
       return;
   }
   QFrame* frame = qobject_cast<QFrame*>(buttonWidget->parentWidget());
   delete frame;
}


void MultisigAddressDialog::addressBookButtonReceiving()
{
    QWidget* addressButton = qobject_cast<QWidget*>(sender());
    QFrame* addressFrame = qobject_cast<QFrame*>(addressButton->parentWidget());
    QValidatedLineEdit* vle = addressFrame->findChild<QValidatedLineEdit*>("address");

    if (model && model->getAddressTableModel()) {
        AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec()) {
             vle->setText(dlg.getReturnValue());
        }
    }

}

void MultisigAddressDialog::on_pasteButton_clicked()
{
    //setAddress(QApplication::clipboard()->text());
}
/**
 * Begin create, spend, sign button click handlers
 */
//create
void MultisigAddressDialog::on_addMultisigButton_clicked()
{
    if(!model)
        return;

    int m = ui->enterMSpinbox->value();
    string label = ui->multisigAddressLabel->text().toStdString();

    vector<string> keys;

    for (int i = 0; i < ui->addressList->count(); i++) {
        QWidget* address = qobject_cast<QWidget*>(ui->addressList->itemAt(i)->widget());
        QValidatedLineEdit* vle = address->findChild<QValidatedLineEdit*>("address");

        if(!vle->text().isEmpty()){
            keys.push_back(vle->text().toStdString());
        }
    }

    try{
        CScript redeem = createRedeemScript(m, keys);
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

//spend
void MultisigAddressDialog::on_createButton_clicked()
{
    if(!model)
        return;

    //first collect user data into vectors
    vector<CTxIn> userVin;
    vector<CTxOut> userVout;
    try{
        for(int i = 0; i < ui->inputsList->count(); i++){
            QWidget* input = qobject_cast<QWidget*>(ui->inputsList->itemAt(i)->widget());
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
            userVin.push_back(in);
        }

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

    //        if(setAddress.count(address)){
    //            ui->createButtonStatus->setStyleSheet("QLabel { color: red; }");
    //            ui->createButtonStatus->setText("Invalid amount.");
    //            validDest = false;
    //        }

            if(!validDest){
                validInput = false;
                continue;
            }

           // setAddress.insert(address);
            CScript scriptPubKey = GetScriptForDestination(address.Get());
            CTxOut out(amt->value(), scriptPubKey);
            userVout.push_back(out);
        }


        if(validInput){

            //calculate total output val
            //

            CCoinsViewCache view = accessInputCoins(userVin);

            //retrieve total input val and change dest
            CAmount totalIn = 0;
            vector<CAmount> vInputVals;
            CScript changePubKey;
            bool fFirst = true;

            for(CTxIn in : userVin){
                const CCoins* coins = view.AccessCoins(in.prevout.hash);
                if (coins == NULL) {
                    qWarning() << "input not found";
                    continue;
                }

                if(!coins->IsAvailable(in.prevout.n)){
                    qWarning() << "input unconfirmed/spent";
                    continue;
                }

                CTxOut prevout = coins->vout[in.prevout.n];
                CScript pk = prevout.scriptPubKey;

                vInputVals.push_back(prevout.nValue);
                totalIn += prevout.nValue;

                if(!fFirst){
                    if(pk != changePubKey){
                        throw runtime_error("Address mismatch! Inputs must originate from the same multisignature address.");
                    }
                }else{
                    fFirst = false;
                    changePubKey = pk;
                }
            }

            CAmount totalOut = 0;

            //retrieve total output val
            for(CTxOut out : userVout){
                totalOut += out.nValue;
            }

            if(totalIn <= totalOut){
                runtime_error("Not enough PIV provided as input to complete transaction (including fee).");
            }


            //calculate change amount
            CAmount changeAmount = totalIn - totalOut;
            CTxOut change(changeAmount, changePubKey);

            //generate random position for change
            unsigned int changeIndex = rand() % (userVout.size() + 1);
            unsigned int count = 0;

            //insert change into vout[rand]
            if(changeIndex < userVout.size()){
                for(vector<CTxOut>::iterator it = userVout.begin(); it != userVout.end(); it++){
                    qWarning() << "inside";
                    qWarning() << count << " " << changeIndex;
                        if(count++ == changeIndex){
                            userVout.insert(it, change);
                            break;
                        }
                }
            }else{
                userVout.insert(userVout.end(),change);
            }


            //populate tx
            CMutableTransaction tx;
            tx.vin = userVin;
            tx.vout = userVout;

            //put junk data in scriptsigs for better fee estimate
            int64_t junk = INT64_MAX;
            for(CTxIn& in : tx.vin){
                in.scriptSig.clear();
                for(int i = 0; i < 50; i++){
                    in.scriptSig  << junk;
                }
            }

            //calculate fee
            unsigned int nBytes = tx.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);
            CAmount fee = ::minRelayTxFee.GetFee(nBytes);
            qWarning() << nBytes << ::minRelayTxFee.GetFee(nBytes) << "   " << fee;
            //display fee warning
            tx.vout.at(changeIndex).nValue -= fee;
            qWarning() << "" << tx.ToString().c_str();

            //return;

            for(CTxIn& in : tx.vin){
                in.scriptSig.clear();
            }


            string errorOut;

            bool c = signTxFromLocalWallet(tx, errorOut);

            qWarning() << "size after sign: " << tx.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);


            if(!errorOut.empty()){
                throw runtime_error(errorOut.data());
            }

            if(c){
                ui->createButtonStatus->setText(QString::fromStdString(strprintf("Complete: true \n%s\n%s", tx.GetHash().GetHex(), EncodeHexTx(tx))));
            }else{
                ui->createButtonStatus->setText(QString::fromStdString(strprintf("Complete: false \n%s", EncodeHexTx(tx))));
            }
        }

    }catch(const runtime_error& e){
        ui->createButtonStatus->setStyleSheet("QTextEdit{ color: red }");
        ui->createButtonStatus->setText(e.what());
    }
}

void MultisigAddressDialog::on_pushButton_clicked()
{
    vector<unsigned char> txData(ParseHex(ui->lineEdit->text().toStdString()));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);

    vector<CMutableTransaction> txVariants;

    //retrieve each tx
    while (!ssData.empty()) {
        try {
            CMutableTransaction tx;
            ssData >> tx;
            txVariants.push_back(tx);
        } catch (const std::exception&) {
            throw runtime_error("TX decode failed");
        }
    }

    if (txVariants.empty()){
        throw runtime_error("Missing transaction");
    }

    if(txVariants.size() > 1){
        throw runtime_error("Invalid Tx Hex (contains more than 1 transaction)");
    }


    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx
    CMutableTransaction mergedTx(txVariants[0]);
    ui->lolStatus->setText(QString::fromStdString((string("txs: ") + strprintf("%i :::",txVariants.size()) + mergedTx.ToString())));
}

//sign
void MultisigAddressDialog::on_signButton_clicked()
{
    if(!model)
        return;
   try{
        //parse tx hex
        CTransaction tx;
        if(!DecodeHexTx(tx, ui->transactionHex->text().toStdString())){
            throw runtime_error("Failed to decode transaction hex!");
        }

        CMutableTransaction mergedTx(tx);
        string errorOut = string();

        bool fComplete = signTxFromLocalWallet(mergedTx, errorOut, ui->keyList);
        qWarning() << "size after sign: " << mergedTx.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);
        if(!errorOut.empty()){
            throw runtime_error(errorOut.data());
        }

        if(fComplete){
            ui->signButtonStatus->setText(QString::fromStdString(strprintf("Complete: true\n\nTransaction ID: %s\n\nTransaction Hex:%s", mergedTx.GetHash().GetHex(), EncodeHexTx(mergedTx))));
        }else{
            ui->signButtonStatus->setText(QString::fromStdString(strprintf("Complete: False\n\nTransaction Hex: %s", EncodeHexTx(mergedTx))));
        }

    }catch(const runtime_error& e){
        ui->signButtonStatus->setStyleSheet("QTextEdit{ color: red }");
        ui->signButtonStatus->setText(e.what());
    }
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


/***
 *private helper functions
 */

CCoinsViewCache MultisigAddressDialog::accessInputCoins(vector<CTxIn>& vin)
{
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache& viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for(const CTxIn& txin : vin) {
            const uint256& prevHash = txin.prevout.hash;
            view.AccessCoins(prevHash); // this is certainly allowed to fail
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

    return view;
}


bool MultisigAddressDialog::signTxFromLocalWallet(CMutableTransaction& tx, string& errorOut, QVBoxLayout* keyList)
{
    //will be set false if all inputs are not fully signed(valid)
    bool fComplete = true;

    //if keyslist is not default value AND has items in list then true
    bool fGivenKeys = (keyList != nullptr) && (keyList->count() > 0);

    try{
        //copy of vin for reference before vin is mutated
        vector<CTxIn> oldVin(tx.vin);

        //pull coins into cache
        CCoinsViewCache view = accessInputCoins(oldVin);
        CBasicKeyStore privKeystore;

        //if keys were given, attempt to collect redeem and scriptpubkey
        if(fGivenKeys){
            for(int i = 0; i < keyList->count(); i++){
                QWidget* keyFrame = qobject_cast<QWidget*>(keyList->itemAt(i)->widget());
                QLineEdit* key = keyFrame->findChild<QLineEdit*>("key");
                CBitcoinSecret vchSecret;
                if (!vchSecret.SetString(key->text().toStdString()))
                    throw runtime_error("Invalid private key");
                CKey cKey = vchSecret.GetKey();
                if (!cKey.IsValid())
                    throw runtime_error("Private key outside allowed range");
                privKeystore.AddKey(cKey);
            }

            for(CTxIn& txin : tx.vin){
                //get coins
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                if(coins == NULL || !coins->IsAvailable(txin.prevout.n)){
                    throw runtime_error("Coins unavailable (unconfirmed/spent)");
                }
                //get pubkey from coins
                CScript prevPubKey = coins->vout[txin.prevout.n].scriptPubKey;

                //get payment destination
                CTxDestination address;
                if(!ExtractDestination(prevPubKey, address)){
                    throw runtime_error("Could not find address for destination.");
                }

                //get redeem script related to destination
                CScriptID hash = boost::get<CScriptID>(address);
                CScript redeemScript;

                if (!pwalletMain->GetCScript(hash, redeemScript)){
                    errorOut = "could not redeem";
                }
                privKeystore.AddCScript(redeemScript);
            }
        }

        //choose between local wallet and provided
        const CKeyStore& keystore = fGivenKeys ? privKeystore : *pwalletMain;

        //attempt to sign each input from local wallet
        int nIn = 0;
        for(CTxIn& txin : tx.vin){
            const CCoins* coins = view.AccessCoins(txin.prevout.hash);
            //only access input if wallet has access
            if (coins == NULL || !coins->IsAvailable(txin.prevout.n)) {
                throw runtime_error("input skipped (no access)");
            }
            txin.scriptSig.clear();
            CScript prevPubKey = coins->vout[txin.prevout.n].scriptPubKey;

            //sign what we can
            SignSignature(keystore, prevPubKey, tx, nIn);

            //merge in any previous signatures
            txin.scriptSig = CombineSignatures(prevPubKey, tx, nIn, txin.scriptSig, oldVin[nIn].scriptSig);

            if (!VerifyScript(txin.scriptSig, prevPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&tx, nIn))){
                fComplete = false;
            }
            nIn++;
        }

        //if all inputs have been signed commit tx
        if(fComplete){
            uint256 hashTx = tx.GetHash();
            CCoinsViewCache& view = *pcoinsTip;
            const CCoins* existingCoins = view.AccessCoins(hashTx);
            bool fOverrideFees = false;
            bool fHaveMempool = mempool.exists(hashTx);
            bool fHaveChain = existingCoins && existingCoins->nHeight < 1000000000;

            if (!fHaveMempool && !fHaveChain) {
                // push to local node and sync with wallets
                CValidationState state;
                if (!AcceptToMemoryPool(mempool, state, tx, false, NULL, !fOverrideFees)) {
                    if (state.IsInvalid())
                        throw runtime_error(strprintf("Transaction rejected - %i: %s", state.GetRejectCode(), state.GetRejectReason()));
                    else
                        throw runtime_error(string("Transaction rejected - ") + state.GetRejectReason());
                }
            } else if (fHaveChain) {
                throw runtime_error("transaction already in block chain");
            }
            RelayTransaction(tx);
        }
    }catch(const runtime_error& e){
        errorOut = string(e.what());
    }
    return fComplete;
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

/***
 * Begin QFrame object creation methods
 */
//creates an address object on the create tab
void MultisigAddressDialog::on_addAddressButton_clicked()
{
    //max addresses 16
    if(ui->addressList->count() > 15){
        ui->addMultisigStatus->setStyleSheet("QLabel { color: red; }");
        ui->addMultisigStatus->setText("Maximum possible addresses reached. (16)");
        return;
    }

    //Create base size policy
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);

    //Create
    QFrame* addressFrame = new QFrame();
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
    addressLabel->setText(QApplication::translate("MultisigAddressDialog", strprintf("Address %i:", ui->addressList->count()+1).c_str() , 0));
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
    connect(addressBookButton, SIGNAL(clicked()), this, SLOT(addressBookButtonReceiving()));

    addressLayout->addWidget(addressBookButton);

    QPushButton* addressPasteButton = new QPushButton(addressFrame);
    addressPasteButton->setObjectName(QStringLiteral("addressPasteButton"));
    QIcon icon4;
    icon4.addFile(QStringLiteral(":/icons/editpaste"), QSize(), QIcon::Normal, QIcon::Off);
    addressPasteButton->setIcon(icon4);
    addressPasteButton->setAutoDefault(false);

    addressLayout->addWidget(addressPasteButton);

    QPushButton* addressDeleteButton = new QPushButton(addressFrame);
    addressDeleteButton->setObjectName(QStringLiteral("addressDeleteButton"));
    QIcon icon5;
    icon5.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    addressDeleteButton->setIcon(icon5);
    addressDeleteButton->setAutoDefault(false);
    connect(addressDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));

    addressLayout->addWidget(addressDeleteButton);
    frameLayout->addLayout(addressLayout);

    ui->addressList->addWidget(addressFrame);
}

void MultisigAddressDialog::on_addInputButton_clicked()
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
    txInputIdLabel->setText(QApplication::translate("MultisigAddressDialog", strprintf("%i. Tx Hash: ", ui->inputsList->count()+1).c_str(), 0));
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
    txInputLayout->addWidget(txInputVout);

    QPushButton* inputDeleteButton = new QPushButton(txInputFrame);
    inputDeleteButton->setObjectName(QStringLiteral("inputDeleteButton"));
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    inputDeleteButton->setIcon(icon);
    inputDeleteButton->setAutoDefault(false);
    connect(inputDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));
    txInputLayout->addWidget(inputDeleteButton);

    frameLayout->addLayout(txInputLayout);

    ui->inputsList->addWidget(txInputFrame);
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

    QPushButton* destinationDeleteButton = new QPushButton(destinationFrame);
    destinationDeleteButton->setObjectName(QStringLiteral("destinationDeleteButton"));
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    destinationDeleteButton->setIcon(icon);
    destinationDeleteButton->setAutoDefault(false);
    connect(destinationDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));
    destinationLayout->addWidget(destinationDeleteButton);

    frameLayout->addLayout(destinationLayout);


    ui->destinationsList->addWidget(destinationFrame);
}

void MultisigAddressDialog::on_addPKButton_clicked()
{
    if(isFirstPK){//on first click the scroll area must show
        isFirstPK = false;
        ui->keyScrollArea->show();
    }

    QFrame* keyFrame = new QFrame(ui->keyScrollAreaContents);

    keyFrame->setObjectName(QStringLiteral("keyFrame"));
    keyFrame->setFrameShape(QFrame::StyledPanel);
    keyFrame->setFrameShadow(QFrame::Raised);

    QHBoxLayout* keyLayout = new QHBoxLayout(keyFrame);
    keyLayout->setObjectName(QStringLiteral("keyLayout"));

    QLabel* keyLabel = new QLabel(keyFrame);
    keyLabel->setObjectName(QStringLiteral("keyLabel"));
    keyLabel->setText(QApplication::translate("MultisigAddressDialog", strprintf("Key %i: ", (ui->keyList->count()+1)).c_str(), 0));
    keyLayout->addWidget(keyLabel);

    QLineEdit* key = new QLineEdit(keyFrame);
    key->setObjectName(QStringLiteral("key"));
    key->setEchoMode(QLineEdit::Password);
    keyLayout->addWidget(key);

    QPushButton* keyDeleteButton = new QPushButton(keyFrame);
    keyDeleteButton->setObjectName(QStringLiteral("keyDeleteButton"));
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
    keyDeleteButton->setIcon(icon);
    keyDeleteButton->setAutoDefault(false);
    connect(keyDeleteButton, SIGNAL(clicked()), this, SLOT(deleteFrame()));
    keyLayout->addWidget(keyDeleteButton);

    ui->keyList->addWidget(keyFrame);
}

