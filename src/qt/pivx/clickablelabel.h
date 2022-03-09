// Copyright (c) 2022 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_CLICKABLELABEL_H
#define PIVX_CLICKABLELABEL_H

#include <QLabel>
#include <QWidget>

class ClickableLabel : public QLabel {
Q_OBJECT

public:
    explicit ClickableLabel(QWidget* parent = nullptr,
                            Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(parent) {};
    ~ClickableLabel() override = default;

Q_SIGNALS:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override { Q_EMIT clicked(); }
};


#endif //PIVX_CLICKABLELABEL_H
