// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSMULTISENDWIDGET_H
#define SETTINGSMULTISENDWIDGET_H

#include <QWidget>
#include <QAbstractTableModel>
#include "qt/pivx/pwidget.h"
#include "qt/pivx/furabstractlistitemdelegate.h"

class PIVXGUI;

namespace Ui {
class SettingsMultisendWidget;
}

class MultiSendModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MultiSendModel(QObject *parent = nullptr);
    ~MultiSendModel() override{}

    enum ColumnIndex {
        ADDRESS = 0,
        PERCENTAGE = 1
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 2; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    void updateList();
};

class SettingsMultisendWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsMultisendWidget(PWidget *parent);
    ~SettingsMultisendWidget();

    void showEvent(QShowEvent *event) override;
    void loadWalletModel() override;
    void changeTheme(bool isLightTheme, QString &theme) override;

private slots:
    void onAddRecipientClicked();
    void clearAll();
    void checkBoxChanged();
    void activate();
    void deactivate();

private:
    Ui::SettingsMultisendWidget *ui;
    MultiSendModel* multiSendModel = nullptr;
    FurAbstractListItemDelegate *delegate = nullptr;

    void addMultiSend(QString address, int percentage, QString addressLabel);
    void updateListState();
};

#endif // SETTINGSMULTISENDWIDGET_H
