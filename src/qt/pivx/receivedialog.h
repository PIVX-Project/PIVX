#ifndef RECEIVEDIALOG_H
#define RECEIVEDIALOG_H

#include <QDialog>

namespace Ui {
class ReceiveDialog;
}

class ReceiveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReceiveDialog(QWidget *parent = nullptr);
    ~ReceiveDialog();

private slots:
    void onCopy();
private:
    Ui::ReceiveDialog *ui;
};

#endif // RECEIVEDIALOG_H
