// Copyright (c) 2020-2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "qt/pivx/balancebubble.h"
#include "qt/pivx/forms/ui_balancebubble.h"

#include "qt/pivx/qtutils.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <qt/bitcoinunits.h>

BalanceBubble::BalanceBubble(QWidget *parent) :
        QWidget(parent),
        ui(new Ui::BalanceBubble)
{
    ui->setupUi(this);

    ui->frame->setProperty("cssClass", "container-popup");
    setCssProperty({ui->textTransparent, ui->textShielded}, "amount-small-popup");

    std::initializer_list<QWidget*> lblTitles = {ui->lblFirst, ui->lblSecond};
    setCssProperty(lblTitles, "text-title-topbar");
    QFont font;
    font.setWeight(QFont::Light);
    for (QWidget* w : lblTitles) { w->setFont(font); }
}

void BalanceBubble::updateValues(int64_t nTransparentBalance, int64_t nShieldedBalance, int unit)
{
    QString valueTrans = BitcoinUnits::formatWithUnit(unit, nTransparentBalance, false, BitcoinUnits::separatorAlways);
    valueTrans = valueTrans.replace(QChar(THIN_SP_CP), QString(","));
    QString valueShield = BitcoinUnits::formatWithUnit(unit, nShieldedBalance, false, BitcoinUnits::separatorAlways);
    valueShield = valueShield.replace(QChar(THIN_SP_CP), QString(","));

    ui->textTransparent->setText(valueTrans);
    ui->textShielded->setText(valueShield);
    adjustSize();
}

void BalanceBubble::showEvent(QShowEvent *event)
{
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation *anim = new QPropertyAnimation(eff,"opacity");
    anim->setDuration(400);
    anim->setStartValue(0);
    anim->setEndValue(1);
    anim->setEasingCurve(QEasingCurve::Linear);
    anim->start(QPropertyAnimation::DeleteWhenStopped);

    if (!hideTimer) hideTimer = new QTimer(this);
    connect(hideTimer, &QTimer::timeout, this, &BalanceBubble::hideTimeout);
    hideTimer->start(7000);
}

void BalanceBubble::hideEvent(QHideEvent *event)
{
    if (hideTimer) hideTimer->stop();
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
    a->setDuration(800);
    a->setStartValue(1);
    a->setEndValue(0);
    a->setEasingCurve(QEasingCurve::OutBack);
    a->start(QPropertyAnimation::DeleteWhenStopped);
}

void BalanceBubble::hideTimeout()
{
    hide();
}

BalanceBubble::~BalanceBubble()
{
    delete ui;
}
