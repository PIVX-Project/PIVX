#ifndef ADDNEWADDRESSDIALOG_H
#define ADDNEWADDRESSDIALOG_H

#include <QWidget>

namespace Ui {
class AddNewAddressDialog;
}

class AddNewAddressDialog : public QWidget
{
    Q_OBJECT

public:
    explicit AddNewAddressDialog(QWidget *parent = nullptr);
    ~AddNewAddressDialog();

private:
    Ui::AddNewAddressDialog *ui;
};

#endif // ADDNEWADDRESSDIALOG_H
