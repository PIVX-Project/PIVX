// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNMODEL_H
#define MNMODEL_H

#include <QAbstractTableModel>
#include "amount.h"
#include "masternodeconfig.h"
#include "operationresult.h"
#include "primitives/transaction.h"

class CMasternode;
class DMNView;
class WalletModel;

enum MNViewType : uint8_t
{
    LEGACY = 0,
    DMN_OWNER = (1 << 0),
    DMN_OPERATOR = (1 << 1),
    DMN_VOTER = (1 << 2)
};

class MasternodeWrapper
{
public:
    explicit MasternodeWrapper(
            const QString& _label,
            const QString& _ipPortStr,
            CMasternode* _masternode,
            COutPoint& _collateralId,
            const Optional<QString>& _mnPubKey,
            const std::shared_ptr<DMNView>& _dmnView) :
            label(_label), ipPort(_ipPortStr), masternode(_masternode),
            collateralId(_collateralId), mnPubKey(_mnPubKey), dmnView(_dmnView) { };

    QString label;
    QString ipPort;
    CMasternode* masternode{nullptr};
    // Cache collateral id and MN pk to be used if 'masternode' is null.
    // (Denoting MNs that were not initialized on the conf file or removed from the network list)
    // when masternode is not null, the collateralId is directly pointing to masternode.vin.prevout.
    Optional<COutPoint> collateralId{nullopt};
    Optional<QString> mnPubKey{nullopt};

    // DMN data
    std::shared_ptr<DMNView> dmnView{nullptr};

    uint16_t getType() const;
};

class MNModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MNModel(QObject *parent);
    ~MNModel() override {
        nodes.clear();
        collateralTxAccepted.clear();
    }
    void init();
    void setWalletModel(WalletModel* _model) { walletModel = _model; };

    enum ColumnIndex {
        ALIAS = 0,  /**< User specified MN alias */
        ADDRESS = 1, /**< Node address */
        PROTO_VERSION = 2, /**< Node protocol version */
        STATUS = 3, /**< Node status */
        ACTIVE_TIMESTAMP = 4, /**<  */
        PUB_KEY = 5,
        COLLATERAL_ID = 6,
        COLLATERAL_OUT_INDEX = 7,
        PRIV_KEY = 8,
        WAS_COLLATERAL_ACCEPTED = 9,
        TYPE = 10, /**< Whether is from a Legacy or Deterministic MN */
        IS_POSE_ENABLED = 11, /**< Whether the DMN is enabled or not*/
        PRO_TX_HASH = 12, /**< The DMN pro reg hash */
        COLUMN_COUNT
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool removeMn(const QModelIndex& index);
    bool addMn(CMasternodeConfig::CMasternodeEntry* entry);
    void updateMNList();

    // Whether the MN legacy system is active or not
    bool isLegacySystemObsolete();
    // Whether the tier two synchronization completed or not
    bool isMNsNetworkSynced();
    // Returns the MN activeState field.
    int getMNState(const QString& mnAlias);
    // Checks if the masternode is inactive
    bool isMNInactive(const QString& mnAlias);
    // Masternode is active if it's in PRE_ENABLED OR ENABLED state
    bool isMNActive(const QString& mnAlias);
    // Masternode collateral has enough confirmations
    bool isMNCollateralMature(const QString& mnAlias);
    // Validate string representing a masternode IP address
    static bool validateMNIP(const QString& addrStr);

    // Return the specific chain amount value for the MN collateral output.
    CAmount getMNCollateralRequiredAmount();
    // Return the specific chain min conf for the collateral tx
    int getMasternodeCollateralMinConf();

    // Creates the DMN and return the hash of the proregtx
    CallResult<uint256> createDMN(const std::string& alias,
                                  const COutPoint& collateral,
                                  std::string& serviceAddr,
                                  const std::string& servicePort,
                                  const CKeyID& ownerAddr,
                                  const Optional<std::string>& operatorPubKey,
                                  const Optional<CKeyID>& votingAddr,
                                  const CKeyID& payoutAddr,
                                  std::string& strError,
                                  const Optional<uint16_t>& operatorPercentage = nullopt,
                                  const Optional<CKeyID>& operatorPayoutAddr = nullopt);

    // Completely stops the Masternode spending the collateral
    OperationResult killDMN(const uint256& collateralHash, unsigned int outIndex);

    // Generates the collateral transaction
    bool createMNCollateral(const QString& alias, const QString& addr, COutPoint& ret_outpoint, QString& ret_error);
    // Creates the mnb and broadcast it to the network
    bool startLegacyMN(const CMasternodeConfig::CMasternodeEntry& mne, int chainHeight, std::string& strError);
    void startAllLegacyMNs(bool onlyMissing, int& amountOfMnFailed, int& amountOfMnStarted,
                           std::string* aliasFilter = nullptr, std::string* error_ret = nullptr);

    CMasternodeConfig::CMasternodeEntry* createLegacyMN(COutPoint& collateralOut,
                                                        const std::string& alias,
                                                        std::string& serviceAddr,
                                                        const std::string& port,
                                                        const std::string& mnKeyString,
                                                        const std::string& mnPubKeyStr,
                                                        QString& ret_error);

    bool removeLegacyMN(const std::string& alias_to_remove, const std::string& tx_id, unsigned int out_index, QString& ret_error);

private:
    WalletModel* walletModel{nullptr};
    // alias mn node ---> <ip, master node>
    QList<MasternodeWrapper> nodes;
    QMap<std::string, bool> collateralTxAccepted;

    const MasternodeWrapper* getMNWrapper(const QString& mnAlias);
};

#endif // MNMODEL_H
