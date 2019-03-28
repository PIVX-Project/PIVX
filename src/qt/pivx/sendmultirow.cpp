#include "qt/pivx/sendmultirow.h"
#include "qt/pivx/forms/ui_sendmultirow.h"
#include <QGraphicsDropShadowEffect>

SendMultiRow::SendMultiRow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SendMultiRow),
    btnContact(new QPushButton()),
    iconNumber(new QPushButton())
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setColor(QColor(0, 0, 0, 22));
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(3);
    shadowEffect->setBlurRadius(6);

    QGraphicsDropShadowEffect* shadowEffect2 = new QGraphicsDropShadowEffect();
    shadowEffect2->setColor(QColor(0, 0, 0, 22));
    shadowEffect2->setXOffset(0);
    shadowEffect2->setYOffset(3);
    shadowEffect2->setBlurRadius(6);

    ui->lineEditAddress->setPlaceholderText("Add address");
    ui->lineEditAddress->setProperty("cssClass", "edit-primary-multi-book");
    ui->lineEditAddress->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->stackedAddress->setGraphicsEffect(shadowEffect);

    ui->lineEditAmount->setPlaceholderText("0.00 zPIV ");
    ui->lineEditAmount->setProperty("cssClass", "edit-primary");
    ui->lineEditAmount->setAttribute(Qt::WA_MacShowFocusRect, 0);
    ui->lineEditAmount->setGraphicsEffect(shadowEffect2);

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
    iconNumber->setText("9");

    QSize BUTTON_SIZE = QSize(24, 24);
    iconNumber->setMinimumSize(BUTTON_SIZE);
    iconNumber->setMaximumSize(BUTTON_SIZE);

    int posIconX = 0;
    int posIconY = 14;
    iconNumber->move(posIconX, posIconY);

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
