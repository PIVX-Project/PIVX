#include "qt/pivx/sendmultirow.h"
#include "qt/pivx/forms/ui_sendmultirow.h"
#include <QGraphicsDropShadowEffect>

SendMultiRow::SendMultiRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SendMultiRow),
    iconNumber(new QPushButton()),
    btnContact(new QPushButton())
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    ui->lineEditAddress->setPlaceholderText("Add address");
    ui->lineEditAddress->setProperty("cssClass", "edit-primary-multi-book");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->stackedAddress->setGraphicsEffect(shadowEffect);

    ui->lineEditAmount->setPlaceholderText("0.00 zPIV ");
    ui->lineEditAmount->setProperty("cssClass", "edit-primary");
    ui->lineEditAmount->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditAmount->setGraphicsEffect(shadowEffect);

    /* Description */

    ui->labelSubtitleDescription->setText("Label address (optional)");
    ui->labelSubtitleDescription->setProperty("cssClass", "text-title");

    ui->lineEditDescription->setPlaceholderText("Add descripcion ");
    ui->lineEditDescription->setProperty("cssClass", "edit-primary");
    ui->lineEditDescription->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditDescription->setGraphicsEffect(shadowEffect);

    // Button Contact

    btnContact->setProperty("cssClass", "btn-dropdown");
    btnContact->setCheckable(true);

    QSize BUTTON_CONTACT_SIZE = QSize(24, 24);
    btnContact->setMinimumSize(BUTTON_CONTACT_SIZE);
    btnContact->setMaximumSize(BUTTON_CONTACT_SIZE);

    ui->stackedAddress->addWidget(btnContact);

    btnContact->show();
    btnContact->raise();

    int posBtnXX = ui->lineEditAddress->width() - 20;
    int posBtnYY = 12;
    btnContact->move(posBtnXX, posBtnYY);


    // Icon Number

    ui->stackedAddress->addWidget(iconNumber);
    iconNumber->show();
    iconNumber->raise();

    iconNumber->setProperty("cssClass", "ic-multi-number");
    iconNumber->setText("1");
    iconNumber->setVisible(false);

    QSize BUTTON_SIZE = QSize(24, 24);
    iconNumber->setMinimumSize(BUTTON_SIZE);
    iconNumber->setMaximumSize(BUTTON_SIZE);

    int posIconX = 0;
    int posIconY = 14;
    iconNumber->move(posIconX, posIconY);

    // TODO: add validator --> there is a class in the core QValidateLineEdit and some methods..
}

void SendMultiRow::setModel(WalletModel* model) {
    this->model = model;

    // TODO:Complete me..
    //if (model && model->getOptionsModel())
    //    connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    clear();
}

void SendMultiRow::deleteClicked() {
    emit removeEntry(this);
}

void SendMultiRow::clear() {
    ui->lineEditAddress->clear();
    ui->lineEditAmount->clear();
    ui->lineEditDescription->clear();
}

bool SendMultiRow::validate()
{
    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    /* TODO: Complete me..
    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    if (!model->validateAddress(ui->payTo->text())) {
        ui->payTo->setValid(false);
        retval = false;
    }

    if (!ui->payAmount->validate()) {
        retval = false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0) {
        ui->payAmount->setValid(false);
        retval = false;
    }

    // Reject dust outputs:
    if (retval && GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        retval = false;
    }
     */

    return retval;
}

SendCoinsRecipient SendMultiRow::getValue() {
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    // Normal payment
    recipient.address = ui->lineEditAddress->text();
    recipient.label = ui->lineEditDescription->text();

    // TODO: Convert this into a value..
    // ui->lineEditAmount->text();
    //recipient.amount =
    //recipient.message = ui->messageTextLabel->text();

    return recipient;
}

void SendMultiRow::setAddress(const QString& address) {
    ui->lineEditAddress->setText(address);
    ui->lineEditAmount->setFocus();
}

bool SendMultiRow::isClear()
{
    return ui->lineEditAddress->text().isEmpty();
}

void SendMultiRow::setFocus()
{
    ui->lineEditAddress->setFocus();
}


void SendMultiRow::setNumber(int _number){
    iconNumber->setText(QString::number(_number));
}

void SendMultiRow::hideLabels(){
    ui->layoutLabel->setVisible(false);
    iconNumber->setVisible(true);
}

void SendMultiRow::showLabels(){
    ui->layoutLabel->setVisible(true);
    iconNumber->setVisible(false);
}

void SendMultiRow::resizeEvent(QResizeEvent *event)
 {
    int posXX = ui->lineEditAddress->width() - 20;
    int posYY = 12;
    btnContact->move(posXX, posYY);
    QWidget::resizeEvent(event);
 }

SendMultiRow::~SendMultiRow()
{
    delete ui;
}
