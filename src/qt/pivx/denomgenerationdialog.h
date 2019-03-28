#ifndef DENOMGENERATIONDIALOG_H
#define DENOMGENERATIONDIALOG_H

#include <QDialog>

namespace Ui {
class DenomGenerationDialog;
}

class DenomGenerationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DenomGenerationDialog(QWidget *parent = nullptr);
    ~DenomGenerationDialog();

private:
    Ui::DenomGenerationDialog *ui;
};

#endif // DENOMGENERATIONDIALOG_H
