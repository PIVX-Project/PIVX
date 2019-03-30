#ifndef RECEIVEDIALOG_H
#define RECEIVEDIALOG_H

#include <QDialog>
#include <QPixmap>

class SendCoinsRecipient;

namespace Ui {
class ReceiveDialog;
}

class ReceiveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReceiveDialog(QWidget *parent = nullptr);
    ~ReceiveDialog();

    void updateQr(QString address);

private slots:
    void onCopy();
private:
    Ui::ReceiveDialog *ui;
    QPixmap *qrImage;
    SendCoinsRecipient *info = nullptr;
};

#endif // RECEIVEDIALOG_H
