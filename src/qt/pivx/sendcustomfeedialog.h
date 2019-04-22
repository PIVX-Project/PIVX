#ifndef SENDCUSTOMFEEDIALOG_H
#define SENDCUSTOMFEEDIALOG_H

#include <QDialog>

namespace Ui {
class SendCustomFeeDialog;
}

class SendCustomFeeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCustomFeeDialog(QWidget *parent = nullptr);
    ~SendCustomFeeDialog();

public slots:
    void onRecommendedChecked();
    void onCustomChecked();
private:
    Ui::SendCustomFeeDialog *ui;
};

#endif // SENDCUSTOMFEEDIALOG_H
