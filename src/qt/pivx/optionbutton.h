// Copyright (c) 2019 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPTIONBUTTON_H
#define OPTIONBUTTON_H

#include <QWidget>

namespace Ui {
class OptionButton;
}

class OptionButton : public QWidget
{
    Q_OBJECT

public:
    explicit OptionButton(QWidget *parent = nullptr);
    ~OptionButton();

    void setSubTitleClassAndText(QString className, QString text);
    void setTitleClassAndText(QString className, QString text);
    void setTitleText(QString text);
    void setRightIconClass(QString className, bool forceUpdate = false);
    void setRightIcon(QPixmap icon);
    void setActive(bool);
    void setChecked(bool checked);
Q_SIGNALS:
    void clicked();

protected:
    virtual void mousePressEvent(QMouseEvent *qevent);

private:
    Ui::OptionButton *ui;
};

#endif // OPTIONBUTTON_H
