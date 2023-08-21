// Copyright (c) 2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/createproposaldialog.h"
#include "qt/pivx/forms/ui_createproposaldialog.h"

#include "qt/pivx/contactsdropdown.h"
#include "qt/pivx/governancemodel.h"
#include "qt/pivx/pwidget.h"
#include "qt/pivx/qtutils.h"
#include "qt/pivx/snackbar.h"

void initPageIndexBtn(QPushButton* btn)
{
    QSize BUTTON_SIZE = QSize(22, 22);
    setCssProperty(btn, "ic-step-confirm");
    btn->setMinimumSize(BUTTON_SIZE);
    btn->setMaximumSize(BUTTON_SIZE);
    btn->move(0, 0);
    btn->show();
    btn->raise();
    btn->setVisible(false);
}

CreateProposalDialog::CreateProposalDialog(PIVXGUI* parent, GovernanceModel* _govModel, WalletModel* _walletModel) :
    QDialog(parent),
    ui(new Ui::CreateProposalDialog),
    govModel(_govModel),
    walletModel(_walletModel),
    icConfirm1(new QPushButton(this)),
    icConfirm2(new QPushButton(this)),
    icConfirm3(new QPushButton(this))
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

    setCssProperty(ui->frame, "container-dialog");
    ui->frame->setContentsMargins(10,10,10,10);
    setCssProperty({ui->labelLine1, ui->labelLine2}, "line-purple");
    setCssProperty({ui->groupBoxName, ui->groupContainer}, "container-border");
    setCssProperty({ui->pushNumber1, ui->pushNumber2, ui->pushNumber3}, "btn-number-check");
    setCssProperty({ui->pushName1, ui->pushName2, ui->pushName3}, "btn-name-check");

    // Pages setup
    setupPageOne();
    setupPageTwo();
    setupPageThree();

    // Confirm icons
    ui->stackedIcon1->addWidget(icConfirm1);
    ui->stackedIcon2->addWidget(icConfirm2);
    ui->stackedIcon3->addWidget(icConfirm3);
    initPageIndexBtn(icConfirm1);
    initPageIndexBtn(icConfirm2);
    initPageIndexBtn(icConfirm3);

    // Connect btns
    setCssProperty({ui->btnNext, ui->btnGenAddr}, "btn-primary");
    ui->btnGenAddr->setVisible(false);
    ui->btnGenAddr->setText(tr("GENERATE ADDRESS"));
    ui->btnNext->setText(tr("NEXT"));
    setCssProperty(ui->btnBack, "btn-dialog-cancel");
    ui->btnBack->setVisible(false);
    ui->btnBack->setText(tr("BACK"));
    setCssProperty(ui->pushButtonSkip, "ic-close");

    connect(ui->pushButtonSkip, &QPushButton::clicked, this, &CreateProposalDialog::close);
    connect(ui->btnNext, &QPushButton::clicked, this, &CreateProposalDialog::onNextClicked);
    connect(ui->btnBack, &QPushButton::clicked, this, &CreateProposalDialog::onBackClicked);
    connect(ui->btnGenAddr, &QPushButton::clicked, this, &CreateProposalDialog::onGenAddressClicked);
}

void setEditBoxStyle(QLabel* label, QLineEdit* lineEdit, const QString& placeholderText)
{
    setCssProperty(label, "text-title");
    lineEdit->setPlaceholderText(placeholderText);
    setCssProperty(lineEdit, "edit-primary");
    lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    setShadow(lineEdit);
}

void CreateProposalDialog::setupPageOne()
{
    setCssProperty(ui->labelTitle1, "text-title-dialog");
    setCssProperty(ui->labelMessage1b, "dialog-proposal-message");
    setEditBoxStyle(ui->labelName, ui->lineEditPropName, "e.g Best proposal ever!");
    setEditBoxStyle(ui->labelURL, ui->lineEditURL, "e.g https://forum.pivx/proposals/best_proposal_ever");

    connect(ui->lineEditPropName, &QLineEdit::textChanged, this, &CreateProposalDialog::propNameChanged);
    connect(ui->lineEditURL, &QLineEdit::textChanged, this, &CreateProposalDialog::propUrlChanged);
}

void CreateProposalDialog::setupPageTwo()
{
    setCssProperty(ui->labelTitleDest, "text-title-dialog");
    setCssProperty(ui->labelMessageDest, "dialog-proposal-message");
    setEditBoxStyle(ui->labelAmount, ui->lineEditAmount, "e.g 500 HMS");
    setCssProperty(ui->labelMonths, "text-title");
    setEditBoxStyle(ui->labelAddress, ui->lineEditAddress, "e.g D...something..");
    setCssProperty(ui->lineEditAddress, "edit-primary-multi-book");
    actAddrList = ui->lineEditAddress->addAction(QIcon("://ic-contact-arrow-down"), QLineEdit::TrailingPosition);
    GUIUtil::setupAmountWidget(ui->lineEditAmount, this);
    setCssProperty(ui->lineEditMonths, "btn-spin-box");
    setShadow(ui->lineEditMonths);
    ui->lineEditMonths->setAttribute(Qt::WA_MacShowFocusRect, false);
    connect(ui->lineEditMonths, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            &CreateProposalDialog::monthsEditDeselect, Qt::QueuedConnection);
    connect(ui->lineEditMonths->findChild<QLineEdit*>(), &QLineEdit::cursorPositionChanged,
            this,  &CreateProposalDialog::monthsEditDeselect, Qt::QueuedConnection);

    connect(ui->lineEditAmount, &QLineEdit::textChanged, this, &CreateProposalDialog::propAmountChanged);
    connect(ui->lineEditAddress, &QLineEdit::textChanged, this, &CreateProposalDialog::propaddressChanged);
    connect(actAddrList, &QAction::triggered, this, &CreateProposalDialog::onAddrListClicked);
}

void CreateProposalDialog::monthsEditDeselect(int i)
{
    ui->lineEditMonths->findChild<QLineEdit*>()->deselect();
    ui->lineEditMonths->clearFocus();
}

void CreateProposalDialog::setupPageThree()
{
    setCssProperty(ui->labelTitle3, "text-title-dialog");
    ui->stackedWidget->setCurrentIndex(pos);
    setCssProperty({ui->labelResultNameTitle,
                    ui->labelResultAmountTitle,
                    ui->labelResultAddressTitle,
                    ui->labelResultMonthsTitle,
                    ui->labelResultUrlTitle},
                   "text-title");
    setCssProperty({ui->labelResultName,
                    ui->labelResultName,
                    ui->labelResultAmount,
                    ui->labelResultAddress,
                    ui->labelResultMonths,
                    ui->labelResultUrl}, "text-body1-dialog");
}

void CreateProposalDialog::propNameChanged(const QString& newText)
{
    bool isValid = !newText.isEmpty() && IsValidUTF8(newText.toStdString()) && govModel->validatePropName(newText).getRes();
    setCssEditLine(ui->lineEditPropName, isValid, true);
}

void CreateProposalDialog::propUrlChanged(const QString& newText)
{
    setCssEditLine(ui->lineEditURL, govModel->validatePropURL(newText).getRes(), true);
}

void CreateProposalDialog::propAmountChanged(const QString& newText)
{
    if (newText.isEmpty()) {
        setCssEditLine(ui->lineEditAmount, true, true);
        return;
    }
    auto amount = GUIUtil::parseValue(newText);
    if (amount > govModel->getMaxAvailableBudgetAmount()) {
        ui->lineEditAmount->setText(newText.left(newText.size() - 1));
        return;
    }
    setCssEditLine(ui->lineEditAmount, govModel->validatePropAmount(amount).getRes(), true);
}

bool CreateProposalDialog::propaddressChanged(const QString& str)
{
    if (!str.isEmpty()) {
        QString trimmedStr = str.trimmed();
        bool isShielded = false;
        const bool valid = walletModel->validateAddress(trimmedStr, false, isShielded) && !isShielded;
        setCssEditLine(ui->lineEditAddress,  valid, true);
        return valid;
    }
    setCssEditLine(ui->lineEditAddress, true, true);
    return false;
}

bool CreateProposalDialog::validatePageOne()
{
    QString propName = ui->lineEditPropName->text();
    if (propName.isEmpty()) {
        setCssEditLine(ui->lineEditPropName, false, true);
        inform(tr("Proposal name cannot be empty"));
        return false;
    }

    auto res = govModel->validatePropName(propName);
    if (!res) {
        inform(QString::fromStdString(res.getError()));
        return false;
    }

    // For now, only accept UTF8 valid strings.
    if (!IsValidUTF8(propName.toStdString())) {
        setCssEditLine(ui->lineEditPropName, false, true);
        inform(tr("Proposal name cannot contain non UTF-8 characters"));
        return false;
    }

    res = govModel->validatePropURL(ui->lineEditURL->text());
    if (!res) inform(QString::fromStdString(res.getError()));
    return res.getRes();
}

bool CreateProposalDialog::validatePageTwo()
{
    // Amount validation
    auto amount = GUIUtil::parseValue(ui->lineEditAmount->text());
    if (amount <= 0) {
        inform(tr("Invalid amount"));
        return false;
    }
    auto opRes = govModel->validatePropAmount(amount);
    if (!opRes) {
        inform(QString::fromStdString(opRes.getError()));
        return false;
    }

    // Payments count validation
    opRes = govModel->validatePropPaymentCount(ui->lineEditMonths->value());
    if (!opRes) {
        inform(QString::fromStdString(opRes.getError()));
        return false;
    }

    if (!propaddressChanged(ui->lineEditAddress->text())) {
        inform(tr("Invalid payment address"));
        return false;
    }

    return true;
}

void CreateProposalDialog::loadSummary()
{
    ui->labelResultName->setText(ui->lineEditPropName->text());
    ui->labelResultUrl->setText(ui->lineEditURL->text());
    ui->labelResultAmount->setText(GUIUtil::formatBalance(GUIUtil::parseValue(ui->lineEditAmount->text())));
    ui->labelResultMonths->setText(QString::number(ui->lineEditMonths->value()));
    ui->labelResultAddress->setText(ui->lineEditAddress->text());
    ui->labelResultUrl->setText(ui->lineEditURL->text());
}

void CreateProposalDialog::sendProposal()
{
    int months = ui->lineEditMonths->value();
    CAmount amount = GUIUtil::parseValue(ui->lineEditAmount->text());
    auto opRes = govModel->createProposal(
            ui->lineEditPropName->text().toStdString(),
            ui->lineEditURL->text().toStdString(),
            months,
            amount,
            ui->lineEditAddress->text().toStdString()
            );
    if (!opRes) {
        inform(QString::fromStdString(opRes.getError()));
        return;
    }
    accept();
}

void CreateProposalDialog::onNextClicked()
{
    int nextPos = pos + 1;
    switch (pos) {
        case 0: {
            if (!validatePageOne()) return;
            ui->stackedWidget->setCurrentIndex(nextPos);
            ui->pushNumber2->setChecked(true);
            ui->pushName3->setChecked(false);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(true);
            ui->btnBack->setVisible(true);
            ui->btnGenAddr->setVisible(true);
            break;
        }
        case 1: {
            if (!validatePageTwo()) return;
            loadSummary();
            ui->stackedWidget->setCurrentIndex(nextPos);
            ui->pushNumber3->setChecked(true);
            ui->pushName3->setChecked(true);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm2->setVisible(true);
            ui->btnNext->setText(tr("Send"));
            break;
        }
        case 2: {
            sendProposal();
        }
    }
    pos = nextPos;
}

void CreateProposalDialog::onBackClicked()
{
    if (pos == 0) return;
    pos--;
    switch(pos) {
        case 0: {
            ui->stackedWidget->setCurrentIndex(pos);
            ui->pushNumber1->setChecked(true);
            ui->pushNumber3->setChecked(false);
            ui->pushNumber2->setChecked(false);
            ui->pushName3->setChecked(false);
            ui->pushName2->setChecked(false);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(false);
            ui->btnBack->setVisible(false);
            ui->btnGenAddr->setVisible(false);
            break;
        }
        case 1: {
            ui->stackedWidget->setCurrentIndex(pos);
            ui->pushNumber2->setChecked(true);
            ui->pushNumber3->setChecked(false);
            ui->pushName3->setChecked(false);
            ui->pushName2->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm2->setVisible(false);
            ui->btnNext->setText(tr("Next"));
            break;
        }
    }
}

void CreateProposalDialog::onAddrListClicked()
{
    int addrSize = walletModel->getAddressTableModel()->sizeSend() +
                       walletModel->getAddressTableModel()->sizeRecv();
    if (addrSize == 0) {
        inform(tr("No contacts available, you can go to the contacts screen and add some there!"));
        return;
    }

    int rowHeight = ui->lineEditAddress->height();
    int height = 70 * 2 + 1; // 2 rows (70 each row).
    int width = ui->lineEditAddress->width();

    if (!menuContacts) {
        // TODO: add different row icon for contacts and own addresses.
        // TODO: add filter/search option.
        // TODO: fix bug that the last presented address isn't being showed.
        menuContacts = new ContactsDropdown(
                width,
                height,
                dynamic_cast<PIVXGUI*>(parent()),
                this
        );
        menuContacts->setWalletModel(walletModel, {AddressTableModel::Send, AddressTableModel::Receive});
        connect(menuContacts, &ContactsDropdown::contactSelected, [this](const QString& address, const QString& label) {
            ui->lineEditAddress->setText(address);
        });

    }

    if (menuContacts->isVisible()) {
        menuContacts->hide();
        return;
    }

    menuContacts->resizeList(width, height);
    menuContacts->setStyleSheet(this->styleSheet());
    menuContacts->adjustSize();

    QPoint position = ui->containerPage2->rect().bottomLeft();
    position.setY(position.y() + rowHeight * 2 - 20);
    position.setX(position.x() + 74); // Add widget's fixed padding manually
    menuContacts->move(position);
    menuContacts->show();
}

void CreateProposalDialog::onGenAddressClicked()
{
    std::string addrLabel = ui->lineEditPropName->text().toStdString();
    CallResult<Destination> addr = !addrLabel.empty() ? walletModel->getNewAddress(addrLabel) : walletModel->getNewAddress("");
    QString newAddr = QString::fromStdString(addr.getObjResult()->ToString());
    ui->lineEditAddress->setText(newAddr);
}

void CreateProposalDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) onNextClicked();
        if (ke->key() == Qt::Key_Escape) reject();
    }
}

void CreateProposalDialog::showEvent(QShowEvent *event)
{
    switch (pos) {
        case 0:
            if (ui->lineEditPropName) ui->lineEditPropName->setFocus();
            break;
        case 1:
            if (ui->lineEditAddress) ui->lineEditAddress->setFocus();
            break;
        case 2:
            if (ui->btnNext) ui->btnNext->setFocus();
            break;
        default:
            return;
    }
}

void CreateProposalDialog::inform(const QString& text)
{
    if (!snackBar) snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

CreateProposalDialog::~CreateProposalDialog()
{
    delete ui;
}
