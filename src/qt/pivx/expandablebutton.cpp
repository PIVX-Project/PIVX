#include "qt/pivx/expandablebutton.h"
#include "qt/pivx/forms/ui_expandablebutton.h"
#include "qt/pivx/qtutils.h"
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QStyle>
#include <iostream>

ExpandableButton::ExpandableButton(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ExpandableButton),
    isAnimating(false)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());
    ui->pushButton->setCheckable(true);
    this->layout()->setSizeConstraint(QLayout::SetFixedSize);

    connect(ui->pushButton, SIGNAL(clicked()), this, SLOT(mousePressEvent()));
}

void ExpandableButton::setButtonClassStyle(const char *name, const QVariant &value, bool forceUpdate){
    ui->pushButton->setProperty(name, value);
    if(forceUpdate){
        updateStyle(ui->pushButton);
    }
}

void ExpandableButton::setButtonText(const QString _text){
    this->text = _text;
    if(this->isExpanded){
        ui->pushButton->setText(_text);
    }
}

void ExpandableButton::setText2(QString text2)
{
    this->text = text2;
    ui->pushButton->setText(text2);
}

ExpandableButton::~ExpandableButton()
{
    delete ui;
}

bool ExpandableButton::isChecked(){
    return ui->pushButton->isChecked();
}

void ExpandableButton::setChecked(bool check){
    ui->pushButton->setChecked(check);
}

void ExpandableButton::setSmall()
{
    ui->pushButton->setText("");
    this->setMaximumWidth(36);
    this->isExpanded = false;
    update();
}

void ExpandableButton::enterEvent(QEvent *) {
    if(!this->isAnimating){
        this->isAnimating = true;
        this->setMaximumWidth(100);
        this->isExpanded = true;

//        animation = new QPropertyAnimation(this, "maximumWidth");
//        animation->setDuration(3000);
//        animation->setStartValue(36);
//        animation->setEndValue(100);

//        QPropertyAnimation *animation2 = new QPropertyAnimation(this, "text2");
//        animation2->setDuration(2000);
//        animation2->setStartValue("Wallet Unlocked");
//        animation2->setEndValue("Wallet Unlocked");

//        QParallelAnimationGroup *group = new QParallelAnimationGroup;
//        group->addAnimation(animation);
//        group->addAnimation(animation2);

//        group->start(QAbstractAnimation::DeleteWhenStopped);

        ui->pushButton->setText(text);
        //animation->start(QAbstractAnimation::DeleteWhenStopped);
        this->isAnimating = false;
        emit Mouse_Hover();
    }
    update();
}

void ExpandableButton::leaveEvent(QEvent *) {
//    if(animation){
//        animation->stop();
//        animation = nullptr;
//    }

    if(!keepExpanded){
        this->setSmall();
    }
    emit Mouse_HoverLeave();
}

void ExpandableButton::mousePressEvent(){
    emit Mouse_Pressed();
}

void ExpandableButton::mousePressEvent(QMouseEvent *ev)
{
    emit Mouse_Pressed();
}

void ExpandableButton::on_pushButton_clicked(bool checked)
{
    // TODO: Add callback event
}
