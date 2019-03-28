#ifndef SENDMULTIROW_H
#define SENDMULTIROW_H

#include <QWidget>
#include <QPushButton>

namespace Ui {
class SendMultiRow;
class QPushButton;
}

class SendMultiRow : public QWidget
{
    Q_OBJECT

public:
    explicit SendMultiRow(QWidget *parent = nullptr);
    ~SendMultiRow();

protected:
    void resizeEvent(QResizeEvent *event);

private:
    Ui::SendMultiRow *ui;
    QPushButton *iconNumber;
    QPushButton *btnContact;
};

#endif // SENDMULTIROW_H
