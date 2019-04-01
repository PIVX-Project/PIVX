#ifndef ADDNEWCONTACTDIALOG_H
#define ADDNEWCONTACTDIALOG_H

#include <QDialog>

namespace Ui {
class AddNewContactDialog;
}

class AddNewContactDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddNewContactDialog(QWidget *parent = nullptr);
    ~AddNewContactDialog();

    QString getLabel();

private:
    Ui::AddNewContactDialog *ui;
};

#endif // ADDNEWCONTACTDIALOG_H
