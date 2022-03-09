// Copyright (c) 2019-2022 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOOLTIPMENU_H
#define TOOLTIPMENU_H

#include "qt/pivx/pwidget.h"
#include <QObject>
#include <QPushButton>
#include <QMap>
#include <QModelIndex>
#include <QMetaMethod>
#include <QVBoxLayout>
#include <QWidget>

class PIVXGUI;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class TooltipMenu : public PWidget
{
    Q_OBJECT

public:
    explicit TooltipMenu(PIVXGUI* _window, QWidget *parent = nullptr);

    void showEvent(QShowEvent *event) override;

    template <typename Func>
    void addBtn(uint8_t id, const QString& label, Func func) {
        QPushButton* btn = createBtn(label);
        connect(btn, &QPushButton::clicked, [this, func](){func(); hideTooltip();});
        verticalLayoutBtns->addWidget(btn, 0, Qt::AlignLeft);
        mapBtns.insert(id, btn);
    }
    void showHideBtn(uint8_t id, bool show);
    void updateLabelForBtn(uint8_t id, const QString& newLabel);
    void setBtnCheckable(uint8_t id, bool checkable, bool isChecked);

Q_SIGNALS:
    void onBtnClicked(const QString& btnLabel);

public Q_SLOTS:
    void hideTooltip();

private:
    QModelIndex index;
    QWidget* container{nullptr};
    QVBoxLayout* containerLayout{nullptr};
    QVBoxLayout* verticalLayoutBtns{nullptr};
    QMap<uint8_t, QPushButton*> mapBtns{};

    void setupUi();

    QPushButton* createBtn(const QString& label);
};

#endif // TOOLTIPMENU_H
