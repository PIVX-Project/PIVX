// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/masternodewizarddialog.h"
#include "qt/pivx/forms/ui_masternodewizarddialog.h"

#include "key_io.h"
#include "interfaces/tiertwo.h"
#include "qt/pivx/clickablelabel.h"
#include "qt/pivx/mnmodel.h"
#include "qt/pivx/qtutils.h"
#include "qt/walletmodel.h"

#include <QDoubleValidator>
#include <QRegularExpression>
#include <QGraphicsDropShadowEffect>

static inline QString formatParagraph(const QString& str) {
    return "<p align=\"justify\" style=\"text-align:center;\">" + str + "</p>";
}

static inline QString formatHtmlContent(const QString& str) {
    return "<html><body>" + str + "</body></html>";
}

static void initTopBtns(const QList<QPushButton*>& icons, const QList<QPushButton*>& numbers, const QList<QPushButton*>& names)
{
    assert(icons.size() == names.size() && names.size() == numbers.size());
    QSize BUTTON_SIZE = QSize(22, 22);
    for (int i=0; i < icons.size(); i++) {
        auto pushNumber = numbers[i];
        auto pushIcon = icons[i];
        auto pushName = names[i];

        setCssProperty(pushNumber, "btn-number-check");
        pushNumber->setEnabled(false);

        setCssProperty(pushName, "btn-name-check");
        pushName->setEnabled(false);

        setCssProperty(pushIcon, "ic-step-confirm");
        pushIcon->setMinimumSize(BUTTON_SIZE);
        pushIcon->setMaximumSize(BUTTON_SIZE);
        pushIcon->move(0, 0);
        pushIcon->show();
        pushIcon->raise();
        pushIcon->setVisible(false);
    }
}

static void setCardShadow(QWidget* edit)
{
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(77, 77, 77, 30));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(4);
    shadowEffect->setBlurRadius(6);
    edit->setGraphicsEffect(shadowEffect);
}

MasterNodeWizardDialog::MasterNodeWizardDialog(WalletModel* model, MNModel* _mnModel, ClientModel* _clientModel, QWidget *parent) :
    FocusedDialog(parent),
    ui(new Ui::MasterNodeWizardDialog),
    walletModel(model),
    mnModel(_mnModel),
    clientModel(_clientModel)
{
    ui->setupUi(this);
    setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");
    ui->frame->setContentsMargins(10,10,10,10);

    isDeterministic = walletModel->isV6Enforced();

    for (int i = 0; i < 6; i++) list_icConfirm.push_back(new QPushButton(this));
    list_pushNumber = {ui->pushNumber1, ui->pushNumber2, ui->pushNumber3, ui->pushNumber4, ui->pushNumber5, ui->pushNumber6};
    list_pushName = {ui->pushName1, ui->pushName2, ui->pushName3, ui->pushName4, ui->pushName5, ui->pushName6};
    setCssProperty({ui->labelLine1, ui->labelLine2, ui->labelLine3, ui->labelLine4, ui->labelLine5}, "line-purple");
    setCssProperty({ui->groupBoxName, ui->groupContainer}, "container-border");

    QString collateralAmountStr(GUIUtil::formatBalance(mnModel->getMNCollateralRequiredAmount()));
    initIntroPage(collateralAmountStr);
    initCollateralPage(collateralAmountStr);
    initServicePage();
    initOwnerPage();
    initOperatorPage();
    initVoterPage();
    initSummaryPage();

    // Confirm icons
    ui->stackedIcon1->addWidget(list_icConfirm[0]);
    ui->stackedIcon2->addWidget(list_icConfirm[1]);
    ui->stackedIcon3->addWidget(list_icConfirm[2]);
    ui->stackedIcon4->addWidget(list_icConfirm[3]);
    ui->stackedIcon5->addWidget(list_icConfirm[4]);
    ui->stackedIcon6->addWidget(list_icConfirm[5]);
    initTopBtns(list_icConfirm, list_pushNumber, list_pushName);

    // Connect btns
    setCssBtnPrimary(ui->btnNext);
    setCssProperty(ui->btnBack , "btn-dialog-cancel");
    ui->btnBack->setVisible(false);
    setCssProperty(ui->pushButtonSkip, "ic-close");

    connect(ui->pushButtonSkip, &QPushButton::clicked, this, &MasterNodeWizardDialog::close);
    connect(ui->btnNext, &QPushButton::clicked, this, &MasterNodeWizardDialog::accept);
    connect(ui->btnBack, &QPushButton::clicked, this, &MasterNodeWizardDialog::onBackClicked);
}

void MasterNodeWizardDialog::initIntroPage(const QString& collateralAmountStr)
{
    setCssProperty(ui->labelTitle1, "text-title-dialog");
    setCssProperty(ui->labelMessage1a, "text-main-grey");
    setCssProperty(ui->labelMessage1b, "text-main-purple");

    ui->labelMessage1a->setText(formatHtmlContent(
            formatParagraph(tr("To create a PIVX Masternode you must dedicate %1 (the unit of PIVX) "
                               "to the network (however, these coins are still yours and will never leave your possession).").arg(collateralAmountStr)) +
            formatParagraph(tr("You can deactivate the node and unlock the coins at any time."))));
}

void MasterNodeWizardDialog::initCollateralPage(const QString& collateralAmountStr)
{
    setCssProperty(ui->labelTitle3, "text-title-dialog");
    setCssProperty(ui->labelMessage3, "text-main-grey");
    setCssSubtitleScreen(ui->labelSubtitleName);

    ui->labelMessage3->setText(formatHtmlContent(
            formatParagraph(tr("A transaction of %1 will be made").arg(collateralAmountStr)) +
            formatParagraph(tr("to a new empty address in your wallet.")) +
            formatParagraph(tr("The Address is labeled under the master node's name."))));

    initCssEditLine(ui->lineEditName);
    // MN alias must not contain spaces or "#" character
    QRegularExpression rx("^(?:(?![\\#\\s]).)*");
    ui->lineEditName->setValidator(new QRegularExpressionValidator(rx, ui->lineEditName));
}

void MasterNodeWizardDialog::initServicePage()
{
    setCssProperty(ui->labelTitle4, "text-title-dialog");
    setCssProperty({ui->labelSubtitleIp, ui->labelSubtitlePort}, "text-title");
    setCssSubtitleScreen(ui->labelSubtitleAddressIp);

    initCssEditLine(ui->lineEditIpAddress);
    initCssEditLine(ui->lineEditPort);
    ui->stackedWidget->setCurrentIndex(pos);
    // Fixed to default port number for mainnet and testnet.
    ui->lineEditPort->setEnabled(walletModel->isRegTestNetwork());
    ui->lineEditPort->setText(QString::number(clientModel->getNetworkPort()));
}

void MasterNodeWizardDialog::initOwnerPage()
{
    setCssProperty(ui->labelTitle5, "text-title-dialog");
    setCssSubtitleScreen(ui->labelSubtitleOwner);
    setCssProperty({ui->labelSubtitleOwnerAddress, ui->labelSubtitlePayoutAddress}, "text-title");
    initCssEditLine(ui->lineEditOwnerAddress);
    initCssEditLine(ui->lineEditPayoutAddress);
    setDropdownList(ui->lineEditOwnerAddress, actOwnerAddrList, {AddressTableModel::Receive});
}

void MasterNodeWizardDialog::initOperatorPage()
{
    setCssProperty(ui->labelTitle6, "text-title-dialog");
    setCssSubtitleScreen(ui->labelSubtitleOperator);
    setCssProperty({ui->labelSubtitleOperatorKey, ui->labelSubtitleOperatorReward}, "text-title");
    initCssEditLine(ui->lineEditOperatorKey);
    initCssEditLine(ui->lineEditOperatorPayoutAddress);
    initCssEditLine(ui->lineEditPercentage);
    ui->lineEditPercentage->setValidator(new QDoubleValidator(0.00, 100.00, 2, ui->lineEditPercentage));
}

void MasterNodeWizardDialog::initVoterPage()
{
    setCssProperty(ui->labelTitleVoter, "text-title-dialog");
    setCssSubtitleScreen(ui->labelSubtitleVoter);
    setCssProperty({ui->labelSubtitleVoterKey}, "text-title");
    initCssEditLine(ui->lineEditVoterKey);
}

void MasterNodeWizardDialog::initSummaryPage()
{
    setCssProperty({ui->scrollAreaSummary, ui->containerSummary}, "container");
    setCssProperty(ui->labelSummary, "text-title-dialog");
    setCssSubtitleScreen(ui->labelSubtitleSummary);
    setCssProperty({ui->containerOwner, ui->containerOperator, ui->containerVoter}, "card-governance");
    setCssProperty({ui->labelOwnerSection, ui->labelOperatorSection, ui->labelVoterSection}, "text-section-title");
    setCssProperty({ui->labelTitleMainAddr, ui->labelTitlePayoutAddr, ui->labelTitleCollateral, ui->labelTitleCollateralIndex,
                    ui->labelTitleOperatorKey, ui->labelTitleOperatorService, ui->labelTitleOperatorPayout, ui->labelTitleOperatorPercentage,
                    ui->labelTitleOperatorService, ui->labelTitleVoterAddress}, "text-title-right");
    setCssProperty({ui->labelMainAddr, ui->labelPayoutAddr, ui->labelCollateralIndex, ui->labelCollateralHash,
                    ui->labelOperatorKey, ui->labelOperatorPayout, ui->labelOperatorPercentage, ui->labelOperatorService,
                    ui->labelVoterAddress}, "text-body2-dialog");
    setCardShadow(ui->containerOwner);
    setCardShadow(ui->containerOperator);
    setCardShadow(ui->containerVoter);

    connect(ui->labelMainAddr, &ClickableLabel::clicked, [this](){
        GUIUtil::setClipboard(QString::fromStdString(mnSummary->ownerAddr));
        inform(tr("Owner address copied to clipboard"));
    });
    connect(ui->labelPayoutAddr, &ClickableLabel::clicked, [this](){
        GUIUtil::setClipboard(QString::fromStdString(mnSummary->ownerPayoutAddr));
        inform(tr("Payout address copied to clipboard"));
    });
    connect(ui->labelCollateralHash, &ClickableLabel::clicked, [this](){
        GUIUtil::setClipboard(QString::fromStdString(mnSummary->collateralOut.hash.GetHex()));
        inform(tr("Collateral tx hash copied to clipboard"));
    });
    connect(ui->labelOperatorService, &ClickableLabel::clicked, [this](){
        GUIUtil::setClipboard(QString::fromStdString(mnSummary->service));
        inform(tr("Service copied to clipboard"));
    });
    connect(ui->labelOperatorKey, &ClickableLabel::clicked, [this](){
        GUIUtil::setClipboard(QString::fromStdString(mnSummary->operatorKey));
        inform(tr("Operator key copied to clipboard"));
    });
    connect(ui->labelOperatorPayout, &ClickableLabel::clicked, [this](){
        if (!mnSummary->operatorPayoutAddr) return;
        GUIUtil::setClipboard(QString::fromStdString(*mnSummary->operatorPayoutAddr));
        inform(tr("Operator payout address copied to clipboard"));
    });

    connect(ui->labelVoterAddress, &ClickableLabel::clicked, [this](){
        GUIUtil::setClipboard(QString::fromStdString(mnSummary->votingKey));
        inform(tr("Voting address copied to clipboard"));
    });
}

void MasterNodeWizardDialog::showEvent(QShowEvent *event)
{
    if (ui->btnNext) ui->btnNext->setFocus();
}

void setBtnsChecked(QList<QPushButton*> btns, int start, int end)
{
    for (int i=start; i < btns.size(); i++) {
        btns[i]->setChecked(i <= end);
    }
}

void MasterNodeWizardDialog::moveToNextPage(int currentPos, int nextPos)
{
    ui->stackedWidget->setCurrentIndex(nextPos);
    list_icConfirm[currentPos]->setVisible(true);
    if (list_pushNumber.size() != nextPos) list_pushNumber[nextPos]->setChecked(true);
    setBtnsChecked(list_pushName, pos, nextPos);
}

void MasterNodeWizardDialog::accept()
{
    int nextPos = pos + 1;
    switch (pos) {
        case Pages::INTRO: {
            moveToNextPage(pos, nextPos);
            ui->btnBack->setVisible(true);
            ui->lineEditName->setFocus();
            break;
        }
        case Pages::ALIAS: {
            // No empty names accepted.
            if (ui->lineEditName->text().isEmpty()) {
                setCssEditLine(ui->lineEditName, false, true);
                return;
            }
            setCssEditLine(ui->lineEditName, true, true);
            moveToNextPage(pos, nextPos);
            ui->lineEditIpAddress->setFocus();
            break;
        }
        case Pages::SERVICE: {
            // No empty address accepted
            if (ui->lineEditIpAddress->text().isEmpty()) {
                return;
            }
            if (!isDeterministic) {
                isOk = createMN();
                QDialog::accept();
            } else {
                if (!validateService()) return; // invalid state informed internally
                // Ask if the user want to customize the owner, operator and voter addresses and keys
                // if not, the process will generate all the values for them and present them in the summary page.
                isWaitingForAsk = true;
                hide();
            }
            break;
        }
        case Pages::OWNER: {
            if (!validateOwner()) return; // invalid state informed internally
            moveToNextPage(pos, nextPos);
            break;
        }
        case Pages::OPERATOR: {
            if (!validateOperator()) return; // invalid state informed internally
            moveToNextPage(pos, nextPos);
            break;
        }
        case Pages::VOTER: {
            if (!validateVoter()) return; // invalid state informed internally
            completeTask();
            return;
        }
        case Pages::SUMMARY: {
            QDialog::accept();
            break;
        }
    }
    pos++;
}

void MasterNodeWizardDialog::completeTask()
{
    for (auto btn : list_icConfirm) { btn->setVisible(true); }
    setBtnsChecked(list_pushNumber, 0, list_pushNumber.size());
    setBtnsChecked(list_pushName, 0, list_pushNumber.size());
    ui->btnBack->setVisible(false);
    ui->btnNext->setText("CLOSE");
    pos = Pages::SUMMARY;
    ui->stackedWidget->setCurrentIndex(pos);
    isOk = createMN();
    if (isOk) setSummary();
    else QDialog::accept();
}

void MasterNodeWizardDialog::setSummary()
{
    assert(mnSummary);
    setShortText(ui->labelMainAddr, QString::fromStdString(mnSummary->ownerAddr), 14);
    setShortText(ui->labelPayoutAddr, QString::fromStdString(mnSummary->ownerPayoutAddr), 14);
    setShortText(ui->labelCollateralHash, QString::fromStdString(mnSummary->collateralOut.hash.GetHex()), 20);
    ui->labelCollateralIndex->setText(QString::number(mnSummary->collateralOut.n));
    ui->labelOperatorService->setText(QString::fromStdString(mnSummary->service));
    setShortText(ui->labelOperatorKey, QString::fromStdString(mnSummary->operatorKey), 20);
    if (mnSummary->operatorPayoutAddr) {
        setShortText(ui->labelOperatorPayout, QString::fromStdString(*mnSummary->operatorPayoutAddr), 14);
        ui->labelOperatorPercentage->setText(QString::number(mnSummary->operatorPercentage)+ " %");
    } else {
        ui->labelOperatorPayout->setText(tr("No address"));
    }
    setShortText(ui->labelVoterAddress, QString::fromStdString(mnSummary->votingKey), 14);
}

CallResult<std::pair<std::string, CKeyID>> getOrCreateAddress(const QString& input,
                                                              const std::string& alias,
                                                              const std::string& label_prefix,
                                                              WalletModel* walletModel,
                                                              bool checkIsMine)
{
    if (input.isEmpty()) {
        const auto addr = walletModel->getNewAddress("dmn_"+label_prefix+"_"+alias);
        if (!addr) return {addr.getError()};
        const CKeyID* ownerKeyId = addr.getObjResult()->getKeyID();
        return {{addr.getObjResult()->ToString(), *ownerKeyId}};
    } else {
        std::string addrStr = input.toStdString();
        auto opKeyId = walletModel->getKeyIDFromAddr(addrStr);
        if (!opKeyId) return {"Invalid "+label_prefix+" address id"};
        if (checkIsMine && !walletModel->isMine(*opKeyId)) {
            return {"Invalid "+label_prefix+" address, must be owned by this wallet"};
        }
        return {{addrStr, *opKeyId}};
    }
}

CallResult<std::pair<std::string, CKeyID>> MasterNodeWizardDialog::getOrCreateOwnerAddress(const std::string& alias)
{
    return getOrCreateAddress(ui->lineEditOwnerAddress->text(), alias, "owner", walletModel, true);
}

CallResult<std::pair<std::string, CKeyID>> MasterNodeWizardDialog::getOrCreatePayoutAddress(const std::string& alias)
{
    return getOrCreateAddress(ui->lineEditPayoutAddress->text(), alias, "payout", walletModel, false);
}

CallResult<std::pair<std::string, CKeyID>> MasterNodeWizardDialog::getOrCreateVotingAddress(const std::string& alias)
{
    return getOrCreateAddress(ui->lineEditVoterKey->text(), alias, "voting", walletModel, false);
}

bool MasterNodeWizardDialog::createMN()
{
    if (!walletModel) {
        returnStr = tr("walletModel not set");
        return false;
    }

    // validate IP address
    QString addressLabel = ui->lineEditName->text();
    if (addressLabel.isEmpty()) {
        returnStr = tr("address label cannot be empty");
        return false;
    }

    QString addressStr = ui->lineEditIpAddress->text();
    QString portStr = ui->lineEditPort->text();
    if (addressStr.isEmpty() || portStr.isEmpty()) {
        returnStr = tr("IP or port cannot be empty");
        return false;
    }
    if (!MNModel::validateMNIP(addressStr)) {
        returnStr = tr("Invalid IP address");
        return false;
    }

    std::string alias = addressLabel.toStdString();
    std::string ipAddress = addressStr.toStdString();
    std::string port = portStr.toStdString();

    // Look for a valid collateral utxo
    COutPoint collateralOut;

    // If not found create a new collateral tx
    if (!walletModel->getMNCollateralCandidate(collateralOut)) {
        // New receive address
        auto r = walletModel->getNewAddress(alias);
        if (!r) return errorOut(tr(r.getError().c_str()));
        if (!mnModel->createMNCollateral(addressLabel,
                                         QString::fromStdString(r.getObjResult()->ToString()),
                                         collateralOut,
                                         returnStr)) {
            // error str set internally
            return false;
        }
    }

    if (isDeterministic) {
        // 1) Get or create the owner addr
        auto opOwnerAddrAndKeyId = getOrCreateOwnerAddress(alias);
        if (!opOwnerAddrAndKeyId.getRes()) {
            return errorOut(tr(opOwnerAddrAndKeyId.getError().c_str()));
        }
        auto ownerAddrAndKeyId = opOwnerAddrAndKeyId.getObjResult();
        std::string ownerAddrStr = ownerAddrAndKeyId->first;
        CKeyID ownerKeyId = ownerAddrAndKeyId->second;

        // 2) Get or create the payout addr
        auto opPayoutAddrAndKeyId = getOrCreatePayoutAddress(alias);
        if (!opPayoutAddrAndKeyId.getRes()) {
            return errorOut(tr(opPayoutAddrAndKeyId.getError().c_str()));
        }
        auto payoutAddrAndKeyId = opPayoutAddrAndKeyId.getObjResult();
        std::string payoutAddrStr = payoutAddrAndKeyId->first;
        CKeyID payoutKeyId = payoutAddrAndKeyId->second;

        // 3) Get operator data
        QString operatorKey = ui->lineEditOperatorKey->text();
        Optional<CKeyID> operatorPayoutKeyId = walletModel->getKeyIDFromAddr(ui->lineEditOperatorPayoutAddress->text().toStdString());
        double operatorPercentage = ui->lineEditPercentage->text().isEmpty() ? 0 : (double) ui->lineEditPercentage->text().toDouble();

        // 4) Get voter data
        Optional<CKeyID> votingAddr;
        if (!ui->lineEditVoterKey->text().isEmpty()) {
            auto opVotingAddrAndKeyId = getOrCreateVotingAddress(alias);
            if (!opVotingAddrAndKeyId.getRes()) {
                return errorOut(tr(opVotingAddrAndKeyId.getError().c_str()));
            }
            auto votingAddrAndKeyId = opVotingAddrAndKeyId.getObjResult();
            votingAddr = votingAddrAndKeyId->second;
        } else {
            // Empty voting address means "use the owner key for voting"
            votingAddr = ownerKeyId;
        }

        // For now, collateral key is always inside the wallet
        std::string error_str;
        auto res = mnModel->createDMN(alias,
                                      collateralOut,
                                      ipAddress,
                                      port,
                                      ownerKeyId,
                                      operatorKey.isEmpty() ? nullopt : Optional<std::string>(operatorKey.toStdString()),
                                      votingAddr,
                                      payoutKeyId,
                                      error_str,
                                      (uint16_t) operatorPercentage * 100, // operator percentage
                                      operatorPayoutKeyId); // operator payout script
        if (!res) {
            return errorOut(tr(error_str.c_str()));
        }

        // If the operator key was created locally, let's get it for the summary
        // future: move "operatorSk" to a constant field
        std::string operatorSk = walletModel->getStrFromTxExtraData(*res.getObjResult(), "operatorSk");
        mnSummary = std::make_unique<MNSummary>(alias,
                                                ipAddress+":"+port,
                                                collateralOut,
                                                ownerAddrStr,
                                                payoutAddrStr,
                                                operatorSk.empty() ? operatorKey.toStdString() : operatorSk,
                                                ownerAddrStr, // voting key, for now fixed to owner addr
                                                0, // operator percentage
                                                nullopt); // operator payout

        returnStr = tr("Deterministic Masternode created! It will appear on your list on the next mined block!");
    } else {
        // Legacy
        CKey secret;
        secret.MakeNewKey(false);
        std::string mnKeyString = KeyIO::EncodeSecret(secret);
        std::string mnPubKeyStr = secret.GetPubKey().GetHash().GetHex();

        mnEntry = mnModel->createLegacyMN(collateralOut, alias, ipAddress, port, mnKeyString, mnPubKeyStr, returnStr);
        if (!mnEntry) return false;
        // Update list
        mnModel->addMn(mnEntry);
        returnStr = tr("Masternode created! Wait %1 confirmations before starting it.").arg(mnModel->getMasternodeCollateralMinConf());
    }
    return true;
}

bool MasterNodeWizardDialog::validateService()
{
    auto opRes = interfaces::g_tiertwo->isServiceValid(ui->lineEditIpAddress->text().toStdString());
    return opRes || errorOut(tr(opRes.getError().c_str()));
}

bool MasterNodeWizardDialog::validateOwner()
{
    QString ownerAddress(ui->lineEditOwnerAddress->text());
    if (!ownerAddress.isEmpty() && !walletModel->isMine(ownerAddress)) {
        setCssEditLine(ui->lineEditOwnerAddress, false, true);
        inform(tr("Invalid main address, must be an address from this wallet"));
        return false;
    }

    QString payoutAddress(ui->lineEditPayoutAddress->text());
    if (!payoutAddress.isEmpty() && !walletModel->validateAddress(payoutAddress)) {
        setCssEditLine(ui->lineEditPayoutAddress, false, true);
        inform(tr("Invalid payout address"));
        return false;
    }

    return true;
}

bool MasterNodeWizardDialog::validateVoter()
{
    QString voterAddress(ui->lineEditVoterKey->text());
    if (!voterAddress.isEmpty() && !walletModel->validateAddress(voterAddress)) {
        setCssEditLine(ui->lineEditVoterKey, false, true);
        inform(tr("Invalid voting address"));
        return false;
    }

    return true;
}

bool MasterNodeWizardDialog::validateOperator()
{
    QString operatorKey(ui->lineEditOperatorKey->text());
    if (!operatorKey.isEmpty() && !interfaces::g_tiertwo->isBlsPubKeyValid(operatorKey.toStdString())) {
        setCssEditLine(ui->lineEditOperatorKey, false, true);
        inform(tr("Invalid operator public key"));
        return false;
    }

    QString payoutAddress(ui->lineEditOperatorPayoutAddress->text());
    if (!payoutAddress.isEmpty() && !walletModel->validateAddress(payoutAddress)) {
        setCssEditLine(ui->lineEditOperatorPayoutAddress, false, true);
        inform(tr("Invalid payout address"));
        return false;
    }

    return true;
}

void MasterNodeWizardDialog::moveBack(int backPos)
{
    ui->stackedWidget->setCurrentIndex(backPos);
    list_icConfirm[pos]->setVisible(false);
    setBtnsChecked(list_pushName, backPos, backPos);
    setBtnsChecked(list_pushNumber, backPos, backPos);
}

void MasterNodeWizardDialog::onBackClicked()
{
    if (pos == 0) return;
    pos--;
    moveBack(pos);
    switch (pos) {
        case 0:{
            ui->btnBack->setVisible(false);
            ui->btnNext->setFocus();
            break;
        }
        case 1: {
            ui->lineEditName->setFocus();
            break;
        }
    }
}

void MasterNodeWizardDialog::setDropdownList(QLineEdit* edit, QAction* action, const QStringList& types)
{
    action = edit->addAction(QIcon("://ic-contact-arrow-down"), QLineEdit::TrailingPosition);
    connect(action, &QAction::triggered, [this, types, edit](){ onAddrListClicked(types, edit); });
}

// TODO: Connect it to every address editable box
void MasterNodeWizardDialog::onAddrListClicked(const QStringList& types, QLineEdit* edit)
{
    const auto& addrModel = walletModel->getAddressTableModel();
    int addrSize = 0;
    for (const auto& type : types) {
        if (type == AddressTableModel::Send) {
            addrSize += addrModel->sizeSend();
        } else if (type == AddressTableModel::Receive) {
            addrSize += addrModel->sizeRecv();
        }
    }
    if (addrSize == 0) {
        inform(tr("No addresses available"));
        return;
    }

    int height = 70 * 2 + 1; // 2 rows (70 each row).
    int width = edit->width();
    if (!dropdownMenu) {
        // TODO: add different row icon for contacts and own addresses.
        // TODO: add filter/search option.
        dropdownMenu = new ContactsDropdown(
                width,
                height,
                dynamic_cast<PIVXGUI*>(parent()),
                this
        );
        // TODO: Update connection every time that a new 'edit' is provided
        connect(dropdownMenu, &ContactsDropdown::contactSelected, [edit](const QString& address, const QString& label) {
            edit->setText(address);
        });

    }
    if (dropdownMenu->isVisible()) {
        dropdownMenu->hide();
        return;
    }

    dropdownMenu->setWalletModel(walletModel, types);
    dropdownMenu->resizeList(width, height);
    dropdownMenu->setStyleSheet(styleSheet());
    dropdownMenu->adjustSize();

    QPoint position = edit->parentWidget()->mapToGlobal(edit->rect().bottomLeft());
    position.setY(position.y() - 8); // Minus spacing
    position.setX(position.x() - width / 2 - 8);
    dropdownMenu->move(position);
    dropdownMenu->show();
}

void MasterNodeWizardDialog::inform(const QString& text)
{
    if (!snackBar)
        snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

bool MasterNodeWizardDialog::errorOut(const QString& err)
{
    returnStr = err;
    return false;
}

MasterNodeWizardDialog::~MasterNodeWizardDialog()
{
    delete snackBar;
    delete ui;
}
