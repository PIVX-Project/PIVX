#include "qt/pivx/privacywidget.h"
#include "qt/pivx/forms/ui_privacywidget.h"
#include <QFile>
#include "qt/pivx/qtutils.h"
#include "guiutil.h"
#include "qt/pivx/coincontrolzpivdialog.h"
#include "qt/pivx/denomgenerationdialog.h"
#include "qt/pivx/defaultdialog.h"
#include "qt/pivx/furlistrow.h"
#include "qt/pivx/txviewholder.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "coincontroldialog.h"
#include "coincontrol.h"
#include "zpiv/accumulators.h"

#define DECORATION_SIZE 65
#define NUM_ITEMS 3

PrivacyWidget::PrivacyWidget(PIVXGUI* parent) :
    PWidget(parent),
    ui(new Ui::PrivacyWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,0,0);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    /* Title */
    ui->labelTitle->setText(tr("Privacy"));
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    /* Button Group */
    ui->pushLeft->setText(tr("Convert"));
    ui->pushLeft->setProperty("cssClass", "btn-check-left");
    ui->pushRight->setText(tr("Mint"));
    ui->pushRight->setProperty("cssClass", "btn-check-right");

    /* Subtitle */
    ui->labelSubtitle1->setText(tr("Minting zPIV anonymizes your PIV by removing\ntransaction history, making transactions untraceable "));
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelSubtitle2->setText(tr("Mint new zPIV or convert back to PIV"));
    ui->labelSubtitle2->setProperty("cssClass", "text-subtitle");
    ui->labelSubtitle2->setContentsMargins(0,2,0,0);
    /* Amount */
    ui->labelSubtitleAmount->setText(tr("Enter amount of PIV to mint into zPIV "));
    ui->labelSubtitleAmount->setProperty("cssClass", "text-title");

    ui->lineEditAmount->setPlaceholderText("0.00 PIV ");
    ui->lineEditAmount->setValidator(new QRegExpValidator(QRegExp("[0-9]+")));
    initCssEditLine(ui->lineEditAmount);

    /* Denom */
    ui->labelTitleDenom1->setText("Denom. with value 1:");
    ui->labelTitleDenom1->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom1->setText("0x1 = 0 zPIV");
    ui->labelValueDenom1->setProperty("cssClass", "text-body2");

    ui->labelTitleDenom5->setText("Denom. with value 5:");
    ui->labelTitleDenom5->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom5->setText("0x5 = 0 zPIV");
    ui->labelValueDenom5->setProperty("cssClass", "text-body2");

    ui->labelTitleDenom10->setText("Denom. with value 10:");
    ui->labelTitleDenom10->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom10->setText("0x10 = 0 zPIV");
    ui->labelValueDenom10->setProperty("cssClass", "text-body2");

    ui->labelTitleDenom50->setText("Denom. with value 50:");
    ui->labelTitleDenom50->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom50->setText("0x50 = 0 zPIV");
    ui->labelValueDenom50->setProperty("cssClass", "text-body2");

    ui->labelTitleDenom100->setText("Denom. with value 100:");
    ui->labelTitleDenom100->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom100->setText("0x100 = 0 zPIV");
    ui->labelValueDenom100->setProperty("cssClass", "text-body2");

    ui->labelTitleDenom500->setText("Denom. with value 500:");
    ui->labelTitleDenom500->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom500->setText("0x500 = 0 zPIV");
    ui->labelValueDenom500->setProperty("cssClass", "text-body2");

    ui->labelTitleDenom1000->setText("Denom. with value 1000:");
    ui->labelTitleDenom1000->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom1000->setText("0x1000 = 0 zPIV");
    ui->labelValueDenom1000->setProperty("cssClass", "text-body2");

    ui->labelTitleDenom5000->setText("Denom. with value 5000:");
    ui->labelTitleDenom5000->setProperty("cssClass", "text-subtitle");
    ui->labelValueDenom5000->setText("0x5000 = 0 zPIV");
    ui->labelValueDenom5000->setProperty("cssClass", "text-body2");

    ui->layoutDenom->setVisible(false);

    // List
    ui->labelListHistory->setText(tr("Last Zerocoin Movements"));
    ui->labelListHistory->setProperty("cssClass", "text-title");

    //ui->emptyContainer->setVisible(false);
    ui->pushImgEmpty->setProperty("cssClass", "img-empty-privacy");
    ui->labelEmpty->setText(tr("No transactions yet"));
    ui->labelEmpty->setProperty("cssClass", "text-empty");

    // Buttons
    setCssBtnPrimary(ui->pushButtonSave);
    onMintSelected(true);

    ui->btnTotalzPIV->setTitleClassAndText("btn-title-grey", "Total 0 zPIV");
    ui->btnTotalzPIV->setSubTitleClassAndText("text-subtitle", "Show own coins denominations.");
    ui->btnTotalzPIV->setRightIconClass("btn-dropdown");

    ui->btnCoinControl->setTitleClassAndText("btn-title-grey", "Coin Control");
    ui->btnCoinControl->setSubTitleClassAndText("text-subtitle", "Select PIV outputs to mint into zPIV.");

    ui->btnDenomGeneration->setTitleClassAndText("btn-title-grey", "Denom generation");
    ui->btnDenomGeneration->setSubTitleClassAndText("text-subtitle", "Select the denomination of the coins.");
    ui->btnDenomGeneration->setVisible(false);

    ui->btnRescanMints->setTitleClassAndText("btn-title-grey", "Rescan mints");
    ui->btnRescanMints->setSubTitleClassAndText("text-subtitle", "Find mints in the blockchain.");

    ui->btnResetZerocoin->setTitleClassAndText("btn-title-grey", "Reset Zerocoin");
    ui->btnResetZerocoin->setSubTitleClassAndText("text-subtitle", "Reset zerocoin database.");

    connect(ui->btnTotalzPIV, SIGNAL(clicked()), this, SLOT(onTotalZpivClicked()));
    connect(ui->btnCoinControl, SIGNAL(clicked()), this, SLOT(onCoinControlClicked()));
    connect(ui->btnDenomGeneration, SIGNAL(clicked()), this, SLOT(onDenomClicked()));
    connect(ui->btnRescanMints, SIGNAL(clicked()), this, SLOT(onRescanMintsClicked()));
    connect(ui->btnResetZerocoin, SIGNAL(clicked()), this, SLOT(onResetZeroClicked()));

    connect(ui->pushLeft, &QPushButton::clicked, [this](){onMintSelected(false);});
    connect(ui->pushRight,  &QPushButton::clicked, [this](){onMintSelected(true);});

    // List
    ui->listView->setProperty("cssClass", "container");
    txHolder = new TxViewHolder(isLightTheme());
    delegate = new FurAbstractListItemDelegate(
                DECORATION_SIZE,
                txHolder,
                this
    );

    ui->listView->setItemDelegate(delegate);
    ui->listView->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listView->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listView->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listView->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void PrivacyWidget::loadWalletModel(){
    if(walletModel) {
        txModel = walletModel->getTransactionTableModel();
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(txModel);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);
        filter->setShowZcTxes(true);
        txHolder->setDisplayUnit(walletModel->getOptionsModel()->getDisplayUnit());
        txHolder->setFilter(filter);
        ui->listView->setModel(filter);

        updateDisplayUnit();
        updateDenomsSupply();

        if (!txModel->hasZcTxes()) {
            ui->emptyContainer->setVisible(true);
            ui->listView->setVisible(false);
        }else{
            // TODO: Use show list method..
            ui->emptyContainer->setVisible(false);
            ui->listView->setVisible(true);
        }

        connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(onSendClicked()));
        // TODO: Connect update display unit..
    }

}

void PrivacyWidget::onMintSelected(bool isMint){
    QString btnText;
    if(isMint){
        btnText = tr("Mint zPIV");
        ui->btnCoinControl->setVisible(true);
    }else{
        btnText = tr("Convert back to PIV");
        ui->btnCoinControl->setVisible(false);
    }
    ui->pushButtonSave->setText(btnText);
}

void PrivacyWidget::updateDisplayUnit() {
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

        txHolder->setDisplayUnit(nDisplayUnit);
        ui->listView->update();
    }
}

void PrivacyWidget::showList(){
    // TODO: here if there is a zc mint/spend here
    ui->emptyContainer->setVisible(false);
    ui->listView->setVisible(true);
}

void PrivacyWidget::onTotalZpivClicked(){

    bool isVisible = ui->layoutDenom->isVisible();

    if(!isVisible){
        ui->layoutDenom->setVisible(true);
    }else{
        ui->layoutDenom->setVisible(false);
    }

}

void PrivacyWidget::onSendClicked(){
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    if(GetAdjustedTime() > GetSporkValue(SPORK_16_ZEROCOIN_MAINTENANCE_MODE)) {
        warn(tr("Mint Zerocoin"), tr("zPIV is currently undergoing maintenance"));
        return;
    }

    bool isConvert = ui->pushLeft->isChecked();

    if(!GUIUtil::requestUnlock(walletModel, AskPassphraseDialog::Context::Mint_zPIV, true)){
        inform(tr("You need to unlock the wallet to be able to %1 zPIV").arg(isConvert ? tr("convert") : tr("mint")));
        return;
    }

    bool isValid = true;
    CAmount value = GUIUtil::parseValue(
            ui->lineEditAmount->text(),
            walletModel->getOptionsModel()->getDisplayUnit(),
            &isValid
    );

    if (!isValid || value <= 0) {
        setCssEditLine(ui->lineEditAmount, false, true);
        inform(tr("Invalid value"));
        return;
    }

    setCssEditLine(ui->lineEditAmount, true, true);
    // TODO: Launch confirmation dialog here..
    if(isConvert){
        spend(value);
    }else{
        mint(value);
    }
}

void PrivacyWidget::mint(CAmount value){
    std::string strError;
    if(!walletModel->mintCoins(value, CoinControlDialog::coinControl, strError)){
        inform(tr(strError.data()));
    }else{
        // Mint succeed
        inform(tr("zPIV minted successfully"));
        // clear
        ui->lineEditAmount->clear();
    }
}

void PrivacyWidget::spend(CAmount value){
    CZerocoinSpendReceipt receipt;
    std::vector<CZerocoinMint> selectedMints;
    bool mintChange = false;
    bool minimizeChange = false;
    // todo: add custom address to
    if(!walletModel->convertBackZpiv(
            value,
            selectedMints,
            mintChange,
            minimizeChange,
            receipt,
            walletModel->getNewAddress()
    )){
        inform(receipt.GetStatusMessage().data());
    }else{
        // Spend succeed
        inform(tr("zPIV converted back to PIV"));
        // clear
        ui->lineEditAmount->clear();
    }
}


void PrivacyWidget::onCoinControlClicked(){
    if(ui->pushRight->isChecked()) {
        if (walletModel->getBalance() > 0) {
            if (!coinControlDialog) {
                coinControlDialog = new CoinControlDialog();
                coinControlDialog->setModel(walletModel);
            }
            coinControlDialog->exec();
            ui->btnCoinControl->setActive(CoinControlDialog::coinControl->HasSelected());
        } else {
            inform(tr("You don't have any PIV to select."));
        }
    }
}

void PrivacyWidget::onDenomClicked(){
    showHideOp(true);
    DenomGenerationDialog* dialog = new DenomGenerationDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 4.5, 5);
}

void PrivacyWidget::onRescanMintsClicked(){
    if (ask(tr("Rescan Mints"),
        tr("Your zerocoin mints are going to be scanned from the blockchain from scratch"))
    ){
        string strResetMintResult = walletModel->resetMintZerocoin();
        inform(QString::fromStdString(strResetMintResult));
    }
}

void PrivacyWidget::onResetZeroClicked(){
    if (ask(tr("Reset Spent Zerocoins"),
        tr("Your zerocoin spends are going to be scanned from the blockchain from scratch"))
    ){
        string strResetMintResult = walletModel->resetSpentZerocoin();
        inform(QString::fromStdString(strResetMintResult));
    }
}

void PrivacyWidget::updateDenomsSupply(){
    std::map<libzerocoin::CoinDenomination, CAmount> mapDenomBalances;
    std::map<libzerocoin::CoinDenomination, int> mapUnconfirmed;
    std::map<libzerocoin::CoinDenomination, int> mapImmature;
    for (const auto& denom : libzerocoin::zerocoinDenomList){
        mapDenomBalances.insert(make_pair(denom, 0));
        mapUnconfirmed.insert(make_pair(denom, 0));
        mapImmature.insert(make_pair(denom, 0));
    }

    std::set<CMintMeta> vMints;
    walletModel->listZerocoinMints(vMints, true, false, true, true);

    map<libzerocoin::CoinDenomination, int> mapMaturityHeights = GetMintMaturityHeight();
    for (auto& meta : vMints){
        // All denominations
        mapDenomBalances.at(meta.denom)++;

        if (!meta.nHeight || chainActive.Height() - meta.nHeight <= Params().Zerocoin_MintRequiredConfirmations()) {
            // All unconfirmed denominations
            mapUnconfirmed.at(meta.denom)++;
        } else {
            if (meta.denom == libzerocoin::CoinDenomination::ZQ_ERROR) {
                mapImmature.at(meta.denom)++;
            } else if (meta.nHeight >= mapMaturityHeights.at(meta.denom)) {
                mapImmature.at(meta.denom)++;
            }
        }
    }

    int64_t nCoins = 0;
    int64_t nSumPerCoin = 0;
    int64_t nUnconfirmed = 0;
    int64_t nImmature = 0;
    QString strDenomStats, strUnconfirmed = "";

    for (const auto& denom : libzerocoin::zerocoinDenomList) {
        nCoins = libzerocoin::ZerocoinDenominationToInt(denom);
        nSumPerCoin = nCoins * mapDenomBalances.at(denom);
        nUnconfirmed = mapUnconfirmed.at(denom);
        nImmature = mapImmature.at(denom);

        strUnconfirmed = "";
        if (nUnconfirmed) {
            strUnconfirmed += QString::number(nUnconfirmed) + QString(" unconf. ");
        }
        if(nImmature) {
            strUnconfirmed += QString::number(nImmature) + QString(" immature ");
        }
        if(nImmature || nUnconfirmed) {
            strUnconfirmed = QString("( ") + strUnconfirmed + QString(") ");
        }

        strDenomStats = strUnconfirmed + QString::number(mapDenomBalances.at(denom)) + " x " +
                        QString::number(nCoins) + " = <b>" +
                        QString::number(nSumPerCoin) + " zPIV </b>";

        switch (nCoins) {
            case libzerocoin::CoinDenomination::ZQ_ONE:
                ui->labelValueDenom1->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE:
                ui->labelValueDenom5->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_TEN:
                ui->labelValueDenom10->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIFTY:
                ui->labelValueDenom50->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_HUNDRED:
                ui->labelValueDenom100->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_HUNDRED:
                ui->labelValueDenom500->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_THOUSAND:
                ui->labelValueDenom1000->setText(strDenomStats);
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_THOUSAND:
                ui->labelValueDenom5000->setText(strDenomStats);
                break;
            default:
                // Error Case: don't update display
                break;
        }
    }

    CAmount matureZerocoinBalance = walletModel->getZerocoinBalance() - walletModel->getUnconfirmedZerocoinBalance() - walletModel->getImmatureZerocoinBalance();
    ui->btnTotalzPIV->setTitleText(tr("Total %1").arg(GUIUtil::formatBalance(matureZerocoinBalance, nDisplayUnit, true)));
}

void PrivacyWidget::changeTheme(bool isLightTheme, QString& theme){
    static_cast<TxViewHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
    ui->listView->update();
}

PrivacyWidget::~PrivacyWidget()
{
    delete ui;
}
