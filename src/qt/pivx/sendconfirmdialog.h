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

    bool isConfirm() { return confirm;}

public slots:
    void accept() override;

private:
    Ui::SendConfirmDialog *ui;
    bool confirm = false;
};

#endif // SENDCONFIRMDIALOG_H
