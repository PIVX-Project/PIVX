#ifndef SENDCHANGEADDRESSDIALOG_H
#define SENDCHANGEADDRESSDIALOG_H

#include <QDialog>

namespace Ui {
class SendChangeAddressDialog;
}

class SendChangeAddressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendChangeAddressDialog(QWidget *parent = nullptr);
    ~SendChangeAddressDialog();

private:
    Ui::SendChangeAddressDialog *ui;
};

#endif // SENDCHANGEADDRESSDIALOG_H
