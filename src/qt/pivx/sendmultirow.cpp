// Copyright (c) 2019-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/sendmultirow.h"
#include "qt/pivx/forms/ui_sendmultirow.h"

#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "bitcoinunits.h"
#include "qt/pivx/sendmemodialog.h"
#include "qt/pivx/qtutils.h"

SendMultiRow::SendMultiRow(PIVXGUI* _window, PWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SendMultiRow),
    iconNumber(new QPushButton())
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

    setCssProperty(ui->lineEditAddress, "edit-primary-multi-book");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->stackedAddress);

    initCssEditLine(ui->lineEditAmount);
    GUIUtil::setupAmountWidget(ui->lineEditAmount, this);

    /* Description */
    setCssProperty(ui->labelSubtitleDescription, "text-title");
    initCssEditLine(ui->lineEditDescription);

    // future: when we get a designer, this should have another icon. A "memo" icon instead of a "+"
    setCssProperty(ui->btnAddMemo, "btn-secundary-add");

    setCssProperty(ui->checkboxSubtractFeeFromAmount, "combo-light");

    // Button menu
    setCssProperty(ui->btnMenu, "btn-menu");
    ui->btnMenu->setVisible(false);

    // Button Contact
    btnContact = ui->lineEditAddress->addAction(QIcon("://ic-contact-arrow-down"), QLineEdit::TrailingPosition);
    // Icon Number
    ui->stackedAddress->addWidget(iconNumber);
    iconNumber->show();
    iconNumber->raise();

    setCssProperty(iconNumber, "ic-multi-number");
    iconNumber->setText("1");
    iconNumber->setVisible(false);
    QSize size = QSize(24, 24);
    iconNumber->setMinimumSize(size);
    iconNumber->setMaximumSize(size);

    int posIconX = 0;
    int posIconY = 14;
    iconNumber->move(posIconX, posIconY);

    connect(ui->lineEditAmount, &QLineEdit::textChanged, this, &SendMultiRow::amountChanged);
    connect(ui->lineEditAddress, &QLineEdit::textChanged, [this](){addressChanged(ui->lineEditAddress->text());});
    connect(btnContact, &QAction::triggered, [this](){Q_EMIT onContactsClicked(this);});
    connect(ui->btnMenu, &QPushButton::clicked, [this](){Q_EMIT onMenuClicked(this);});
    connect(ui->btnAddMemo, &QPushButton::clicked, this, &SendMultiRow::onMemoClicked);
}

void SendMultiRow::amountChanged(const QString& amountStr)
{
    if (!amountStr.isEmpty()) {
        auto value = GUIUtil::parseValue(amountStr, displayUnit);
        if (value > 0) {
            GUIUtil::updateWidgetTextAndCursorPosition(ui->lineEditAmount, amountStr);
            setCssEditLine(ui->lineEditAmount, true, true);
        }
    }
    Q_EMIT onValueChanged();
}

void SendMultiRow::onMemoClicked()
{
    launchMemoDialog();
}

bool SendMultiRow::launchMemoDialog()
{
    window->showHide(true);
    SendMemoDialog* dialog = new SendMemoDialog(window, walletModel);
    dialog->setMemo(recipient.message);
    bool ret = false;
    if (openDialogWithOpaqueBackgroundY(dialog, window, 3, 5)) {
        recipient.message = dialog->getMemo();
        ui->btnAddMemo->setText(tr("Update memo"));
        setCssProperty(ui->btnAddMemo, "btn-secondary-update", true);
    } else if (dialog->getOperationResult()) {
        bool isMemoEmpty = recipient.message.isEmpty();
        // reset..
        recipient.message.clear();
        ui->btnAddMemo->setText(tr("Add encrypted memo"));
        setCssProperty(ui->btnAddMemo, "btn-secundary-add", true);
        if (!isMemoEmpty) inform(tr("Memo field reset"));
        ret = false;
    }
    dialog->deleteLater();
    return ret;
}

bool SendMultiRow::addressChanged(const QString& str, bool fOnlyValidate)
{
    if (!str.isEmpty()) {
        QString trimmedStr = str.trimmed();
        bool isShielded = false;
        const bool valid = walletModel->validateAddress(trimmedStr, this->onlyStakingAddressAccepted, isShielded);
        if (!valid) {
            // check URI
            SendCoinsRecipient rcp;
            if (GUIUtil::parseBitcoinURI(trimmedStr, &rcp)) {
                ui->lineEditAddress->setText(rcp.address);
                ui->lineEditAmount->setText(BitcoinUnits::format(displayUnit, rcp.amount, false));

                QString label = walletModel->getAddressTableModel()->labelForAddress(rcp.address);
                if (!label.isNull() && !label.isEmpty()){
                    ui->lineEditDescription->setText(label);
                } else if (!rcp.message.isEmpty())
                    ui->lineEditDescription->setText(rcp.message);

                Q_EMIT onUriParsed(rcp);
            } else {
                setCssProperty(ui->lineEditAddress, "edit-primary-multi-book-error");
            }
        } else {
            setCssProperty(ui->lineEditAddress, "edit-primary-multi-book");
            if (!fOnlyValidate) {
                QString label = walletModel->getAddressTableModel()->labelForAddress(trimmedStr);
                if (!label.isEmpty()) {
                    ui->lineEditDescription->setText(label);
                }
            }
        }
        updateStyle(ui->lineEditAddress);
        return valid;
    }

    setCssProperty(ui->lineEditAddress, "edit-primary-multi-book");
    updateStyle(ui->lineEditAddress);

    return false;
}


void SendMultiRow::loadWalletModel()
{
    if (walletModel && walletModel->getOptionsModel()) {
        displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &SendMultiRow::updateDisplayUnit);
    }
    clear();
}

void SendMultiRow::updateDisplayUnit()
{
    // Update edit text..
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
}

void SendMultiRow::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

void SendMultiRow::clear()
{
    ui->lineEditAddress->clear();
    ui->lineEditAmount->clear();
    ui->lineEditDescription->clear();
    setCssProperty(ui->lineEditAddress, "edit-primary-multi-book", true);
}

bool SendMultiRow::validate()
{
    if (!walletModel)
        return false;

    // Check input validity
    bool retval = true;

    // Check address validity, returns false if it's invalid
    QString address = ui->lineEditAddress->text();
    if (address.isEmpty()){
        retval = false;
        setCssProperty(ui->lineEditAddress, "edit-primary-multi-book-error", true);
    } else
        retval = addressChanged(address, true);

    CAmount value = getAmountValue();

    // Sending a zero amount is invalid
    if (value <= 0) {
        setCssEditLine(ui->lineEditAmount, false, true);
        retval = false;
    }

    // Reject dust outputs:
    if (retval && GUIUtil::isDust(address, value)) {
        setCssEditLine(ui->lineEditAmount, false, true);
        retval = false;
    }

    return retval;
}

SendCoinsRecipient SendMultiRow::getValue()
{
    recipient.address = getAddress();
    recipient.label = ui->lineEditDescription->text();
    recipient.amount = getAmountValue();
    auto dest = Standard::DecodeDestination(recipient.address.toStdString());
    recipient.isShieldedAddr = boost::get<libzcash::SaplingPaymentAddress>(&dest);
    recipient.fSubtractFee = getSubtractFeeFromAmount();
    return recipient;
}

QString SendMultiRow::getAddress()
{
    return ui->lineEditAddress->text().trimmed();
}

CAmount SendMultiRow::getAmountValue()
{
    return GUIUtil::parseValue(ui->lineEditAmount->text(), displayUnit);
}

QString SendMultiRow::getMemo()
{
    return recipient.message;
}

QRect SendMultiRow::getEditLineRect()
{
    return ui->lineEditAddress->rect();
}

int SendMultiRow::getEditHeight()
{
    return ui->stackedAddress->height();
}

int SendMultiRow::getEditWidth()
{
    return ui->lineEditAddress->width();
}

int SendMultiRow::getNumber()
{
    return number;
}

bool SendMultiRow::getSubtractFeeFromAmount() const
{
    return ui->checkboxSubtractFeeFromAmount->isChecked();
}

void SendMultiRow::setAddress(const QString& address)
{
    ui->lineEditAddress->setText(address);
    ui->lineEditAmount->setFocus();
}

void SendMultiRow::setAmount(const QString& amount)
{
    ui->lineEditAmount->setText(amount);
}

void SendMultiRow::toggleSubtractFeeFromAmount()
{
    bool old = ui->checkboxSubtractFeeFromAmount->isChecked();
    ui->checkboxSubtractFeeFromAmount->setChecked(!old);
}

void SendMultiRow::setAddressAndLabelOrDescription(const QString& address, const QString& message)
{
    QString label = walletModel->getAddressTableModel()->labelForAddress(address);
    if (!label.isNull() && !label.isEmpty()){
        ui->lineEditDescription->setText(label);
    } else if(!message.isEmpty())
        ui->lineEditDescription->setText(message);
    setAddress(address);
}

void SendMultiRow::setLabel(const QString& label)
{
    ui->lineEditDescription->setText(label);
}

bool SendMultiRow::isClear()
{
    return ui->lineEditAddress->text().isEmpty();
}

void SendMultiRow::setFocus()
{
    ui->lineEditAddress->setFocus();
}

void SendMultiRow::setOnlyStakingAddressAccepted(bool onlyStakingAddress)
{
    this->onlyStakingAddressAccepted = onlyStakingAddress;
    ui->containerMemo->setVisible(!onlyStakingAddress);
}


void SendMultiRow::setNumber(int _number)
{
    number = _number;
    iconNumber->setText(QString::number(_number));
}

void SendMultiRow::hideLabels()
{
    ui->layoutLabel->setVisible(false);
    iconNumber->setVisible(true);
}

void SendMultiRow::showLabels()
{
    ui->layoutLabel->setVisible(true);
    iconNumber->setVisible(false);
}

void SendMultiRow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void SendMultiRow::enterEvent(QEvent *)
{
    if (!this->isExpanded && iconNumber->isVisible()) {
        isExpanded = true;
        ui->btnMenu->setVisible(isExpanded);
    }
}

void SendMultiRow::leaveEvent(QEvent *)
{
    if (isExpanded) {
        isExpanded = false;
        ui->btnMenu->setVisible(isExpanded);
    }
}

int SendMultiRow::getMenuBtnWidth()
{
    return ui->btnMenu->width();
}

SendMultiRow::~SendMultiRow()
{
    delete ui;
}
