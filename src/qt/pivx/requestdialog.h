#ifndef REQUESTDIALOG_H
#define REQUESTDIALOG_H

#include <QDialog>

namespace Ui {
class RequestDialog;
}

class RequestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RequestDialog(QWidget *parent = nullptr);
    ~RequestDialog();

private slots:
    void onNextClicked();

private:
    Ui::RequestDialog *ui;
    int pos = 0;
};

#endif // REQUESTDIALOG_H
