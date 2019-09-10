// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EXPANDABLEBUTTON2_H
#define EXPANDABLEBUTTON2_H

#include <QWidget>
#include <QEvent>
#include <QString>
#include <iostream>

#include <QPropertyAnimation>
#include <atomic>

namespace Ui {
class ExpandableButton;
}

class ExpandableButton : public QWidget
{
    Q_OBJECT

public:
    explicit ExpandableButton(QWidget *parent = nullptr);
    ~ExpandableButton();

    void setButtonClassStyle(const char *name, const QVariant &value, bool forceUpdate = false);
    void setButtonText(const QString _text);
    void setIcon(QString path);

    bool isChecked();
    void setChecked(bool check);
    void setKeepExpanded(bool _keepExpended){
        this->keepExpanded = _keepExpended;
    }
    void setSmall();
    void setExpanded();
signals:
    void Mouse_Pressed();
    void Mouse_Hover();
    void Mouse_HoverLeave();

public slots:
    void setText2(QString text2);

    QString getText(){
        return this->text;
    }

protected:
    virtual void enterEvent(QEvent *);
    virtual void leaveEvent(QEvent *);

    //virtual void mouseMoveEvent(QMouseEvent *ev);
    virtual void mousePressEvent(QMouseEvent *ev);

private slots:

    void on_pushButton_clicked(bool checked);

    void mousePressEvent();
private:
    Ui::ExpandableButton *ui;
    QString text;
    std::atomic<bool> isAnimating;
    QPropertyAnimation *animation = nullptr;
    bool isExpanded = false;

    bool keepExpanded = false;
};

#endif // EXPANDABLEBUTTON2_H
