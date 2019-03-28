#ifndef COINCONTROLZPIVDIALOG_H
#define COINCONTROLZPIVDIALOG_H

#include <QDialog>

namespace Ui {
class CoinControlZpivDialog;
}

class CoinControlZpivDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlZpivDialog(QWidget *parent = nullptr);
    ~CoinControlZpivDialog();

private:
    Ui::CoinControlZpivDialog *ui;
};

#endif // COINCONTROLZPIVDIALOG_H
