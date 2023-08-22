// Copyright (c) 2019-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GMROW_H
#define GMROW_H

#include <QWidget>

namespace Ui {
class GMRow;
}

class GMRow : public QWidget
{
    Q_OBJECT

public:
    explicit GMRow(QWidget *parent = nullptr);
    ~GMRow();

    void updateView(QString address, const QString& label, QString status, bool wasCollateralAccepted);

Q_SIGNALS:
    void onMenuClicked();
private:
    Ui::GMRow *ui;
};

#endif // GMROW_H
