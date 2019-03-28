#ifndef SENDCONFIRMDIALOG_H
#define SENDCONFIRMDIALOG_H

#include <QDialog>

namespace Ui {
class SendConfirmDialog;
}

class SendConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendConfirmDialog(QWidget *parent = nullptr);
    ~SendConfirmDialog();

private:
    Ui::SendConfirmDialog *ui;
};

#endif // SENDCONFIRMDIALOG_H
