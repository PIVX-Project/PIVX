// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

void ExpandableButton::setIcon(QString path){
    ui->pushButton->setIcon(QIcon(path));
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

void ExpandableButton::setExpanded(){
    this->setMaximumWidth(100);
    ui->pushButton->setText(text);
    this->isExpanded = true;
}

void ExpandableButton::enterEvent(QEvent *) {
    if(!this->isAnimating){
        setExpanded();
        emit Mouse_Hover();
    }
    update();
}

void ExpandableButton::leaveEvent(QEvent *) {
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
