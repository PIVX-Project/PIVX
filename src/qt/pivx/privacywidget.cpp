#include "qt/pivx/privacywidget.h"
#include "qt/pivx/forms/ui_privacywidget.h"
#include <QFile>
#include "qt/pivx/qtutils.h"
#include "guiutil.h"
#include "qt/pivx/coincontrolzpivdialog.h"
#include "qt/pivx/denomgenerationdialog.h"
#include <QGraphicsDropShadowEffect>
#include "qt/pivx/defaultdialog.h"
#include "qt/pivx/furlistrow.h"
#include "qt/pivx/txviewholder.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "coincontroldialog.h"

#define DECORATION_SIZE 70
#define NUM_ITEMS 3

PrivacyWidget::PrivacyWidget(PIVXGUI* _window, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PrivacyWidget),
    window(_window)
{
    ui->setupUi(this);

    this->setStyleSheet(_window->styleSheet());

    /* Containers */
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,20,0,20);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(20,10,20,20);

    /* Light Font */
    QFont fontLight;
    fontLight.setWeight(QFont::Light);

    /* Title */
    ui->labelTitle->setText("Privacy");
    ui->labelTitle->setProperty("cssClass", "text-title-screen");
    ui->labelTitle->setFont(fontLight);

    /* Button Group */
    ui->pushLeft->setText("Convert");
    ui->pushLeft->setProperty("cssClass", "btn-check-left");
    ui->pushRight->setText("Mint");
    ui->pushRight->setProperty("cssClass", "btn-check-right");

    /* Subtitle */

    ui->labelSubtitle1->setText("Minting zPIV anonymizes your PIV by removing\ntransaction history, making transactions untraceable ");
    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle");

    ui->labelSubtitle2->setText("Mint new zPIV or convert back to PIV");
    ui->labelSubtitle2->setProperty("cssClass", "text-subtitle");
    ui->labelSubtitle2->setContentsMargins(0,2,0,0);
    /* Amount */

    ui->labelSubtitleAmount->setText("Enter amount of PIV to mint into zPIV ");
    ui->labelSubtitleAmount->setProperty("cssClass", "text-title");

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditAmount->setPlaceholderText("0.00 PIV ");
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


    ui->labelListHistory->setText("Last Zerocoin Movements");
    ui->labelListHistory->setProperty("cssClass", "text-title");

    //ui->emptyContainer->setVisible(false);
    ui->pushImgEmpty->setProperty("cssClass", "img-empty-privacy");

    ui->labelEmpty->setText("No transactions yet");
    ui->labelEmpty->setProperty("cssClass", "text-empty");

    // Buttons

    ui->pushButtonSave->setProperty("cssClass", "btn-primary");
    onMintSelected(true);

    ui->btnTotalzPIV->setTitleClassAndText("btn-title-grey", "Total zPIV 1000");
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
    // Style
    connect(window, SIGNAL(themeChanged(bool, QString&)), this, SLOT(changeTheme(bool, QString&)));


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

void PrivacyWidget::setWalletModel(WalletModel* _model){
    walletModel = _model;
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

        if (txModel->size() == 0) {
            ui->emptyContainer->setVisible(true);
            ui->listView->setVisible(false);
            // TODO: Connect waiting for tx updates..
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
    // TODO: Complete me..
    QString btnText;
    if(isMint){
        btnText = tr("Mint zPIV");
    }else{
        btnText = tr("Convert back to PIV");
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
        emit message(tr("Mint Zerocoin"), tr("zPIV is currently undergoing maintenance"), CClientUIInterface::MSG_ERROR);
        return;
    }

    bool isConvert = ui->pushLeft->isChecked();

    if(!GUIUtil::requestUnlock(walletModel, AskPassphraseDialog::Context::Mint_zPIV, true)){
        emit message("",
                tr("You need to unlock the wallet to be able to %1 zPIV").arg(isConvert ? tr("convert") : tr("mint")), CClientUIInterface::MSG_INFORMATION_SNACK);
        return;
    }

    bool isValid = true;
    CAmount value = GUIUtil::parseValue(
            ui->lineEditAmount->text(),
            walletModel->getOptionsModel()->getDisplayUnit(),
            &isValid
    );

    if (value <= 0 || !isValid) {
        setCssEditLine(ui->lineEditAmount, false, true);
        emit message("", tr("Invalid value"), CClientUIInterface::MSG_INFORMATION_SNACK);
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
        emit message("", tr(strError.data()), CClientUIInterface::MSG_INFORMATION_SNACK);
    }else{
        // Mint succeed
        emit message("", tr("zPIV minted successfully"), CClientUIInterface::MSG_INFORMATION_SNACK);

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
        emit message("", receipt.GetStatusMessage().data(), CClientUIInterface::MSG_INFORMATION_SNACK);
    }else{
        // Spend succeed
        emit message("", tr("zPIV converted back to PIV"), CClientUIInterface::MSG_INFORMATION_SNACK);

        // clear
        ui->lineEditAmount->clear();
    }
}


void PrivacyWidget::onCoinControlClicked(){
    window->showHide(true);
    CoinControlZpivDialog* dialog = new CoinControlZpivDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 4.5, 5);
}

void PrivacyWidget::onDenomClicked(){
    window->showHide(true);
    DenomGenerationDialog* dialog = new DenomGenerationDialog(window);
    openDialogWithOpaqueBackgroundY(dialog, window, 4.5, 5);
}

void PrivacyWidget::onRescanMintsClicked(){
    bool ret = false;
    window->message(
            tr("Rescan Mints"),
            tr("Your zerocoin mints are going to be scanned from the blockchain from scratch"),
            CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL,
            &ret);
    if (ret){
        string strResetMintResult = walletModel->resetMintZerocoin();
        emit message("", QString::fromStdString(strResetMintResult), CClientUIInterface::MSG_INFORMATION_SNACK);
    }
}

void PrivacyWidget::onResetZeroClicked(){
    bool ret = false;
    window->message(
            tr("Reset Spent Zerocoins"),
            tr("Your zerocoin spends are going to be scanned from the blockchain from scratch"),
            CClientUIInterface::MSG_INFORMATION | CClientUIInterface::BTN_MASK | CClientUIInterface::MODAL,
            &ret);
    if (ret){
        string strResetMintResult = walletModel->resetSpentZerocoin();
        emit message("", QString::fromStdString(strResetMintResult), CClientUIInterface::MSG_INFORMATION_SNACK);
    }
}

void PrivacyWidget::changeTheme(bool isLightTheme, QString& theme){
    // Change theme in all of the childs here..
    this->setStyleSheet(theme);
    static_cast<TxViewHolder*>(this->delegate->getRowFactory())->isLightTheme = isLightTheme;
    ui->listView->update();
    updateStyle(this);
}

PrivacyWidget::~PrivacyWidget()
{
    delete ui;
}
