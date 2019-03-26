#ifndef TXDETAILDIALOG_H
#define TXDETAILDIALOG_H

#include <QDialog>

namespace Ui {
class TxDetailDialog;
}

class TxDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TxDetailDialog(QWidget *parent = nullptr);
    ~TxDetailDialog();

private:
    Ui::TxDetailDialog *ui;
};

#endif // TXDETAILDIALOG_H
