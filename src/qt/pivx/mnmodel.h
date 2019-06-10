#ifndef MNMODEL_H
#define MNMODEL_H

#include <QAbstractTableModel>
#include "masternode.h"

class MNModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MNModel(QObject *parent = nullptr);
    ~MNModel() override{}

    enum ColumnIndex {
        ALIAS = 0,  /**< User specified MN alias */
        ADDRESS = 1, /**< Node address */
        PROTO_VERSION = 2, /**< Node protocol version */
        STATUS = 3, /**< Node status */
        ACTIVE_TIMESTAMP = 4 /**<  */
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;

private:
    // alias mn node ---> pair <ip, master node>
    QMap<QString, std::pair<QString,CMasternode*>> nodes;
};

#endif // MNMODEL_H
