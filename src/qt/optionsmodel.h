// Copyright (c) 2011-2013 The Bitcoin developers
//Copyright (c) 2017-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OPTIONSMODEL_H
#define BITCOIN_QT_OPTIONSMODEL_H

#include "amount.h"

#include <QAbstractListModel>
#include <QSettings>

QT_BEGIN_NAMESPACE
class QNetworkProxy;
QT_END_NAMESPACE

/** Interface from Qt to configuration data structure for Sombe client.
   To Qt, the options are presented as a list with the different options
   laid out vertically.
   This can be changed to a tree once the settings become sufficiently
   complex.
 */
class OptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit OptionsModel(QObject* parent = 0);

    enum OptionID {
        StartAtStartup,      // bool
        MinimizeToTray,      // bool
        MapPortUPnP,         // bool
        MinimizeOnClose,     // bool
        ProxyUse,            // bool
        ProxyIP,             // QString
        ProxyPort,           // int
        DisplayUnit,         // BitcoinUnits::Unit
        ThirdPartyTxUrls,    // QString
        Digits,              // QString
        Theme,               // QString
        Language,            // QString
        CoinControlFeatures, // bool
        ThreadsScriptVerif,  // int
        DatabaseCache,       // int
        SpendZeroConfChange, // bool
        ZeromintEnable,      // bool
        ZeromintAddresses,   // bool
        ZeromintPercentage,  // int
        ZeromintPrefDenom,   // int
        HideCharts,          // bool
        HideZeroBalances,    // bool
        HideOrphans,    // bool
        AnonymizeSombeAmount, //int
        ShowMasternodesTab,  // bool
        Listen,              // bool
        StakeSplitThreshold,    // CAmount (LongLong)
        ShowColdStakingScreen,  // bool
        fUseCustomFee,          // bool
        nCustomFee,             // CAmount (LongLong)
        OptionIDRowCount,
    };

    void Init();
    void Reset();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    void refreshDataView();
    /** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
    void setDisplayUnit(const QVariant& value);
    /* Update StakeSplitThreshold's value in wallet */
    void setStakeSplitThreshold(const CAmount value);
    double getSSTMinimum() const;
    bool isSSTValid();
    /* Update Custom Fee value in wallet */
    void setUseCustomFee(bool fUse);
    void setCustomFeeValue(const CAmount& value);

    /* Explicit getters */
    bool isHideCharts() { return fHideCharts; }
    bool getMinimizeToTray() { return fMinimizeToTray; }
    bool getMinimizeOnClose() { return fMinimizeOnClose; }
    int getDisplayUnit() { return nDisplayUnit; }
    QString getThirdPartyTxUrls() { return strThirdPartyTxUrls; }
    bool getProxySettings(QNetworkProxy& proxy) const;
    bool getCoinControlFeatures() { return fCoinControlFeatures; }
    const QString& getOverriddenByCommandLine() { return strOverriddenByCommandLine; }

    /* Restart flag helper */
    void setRestartRequired(bool fRequired);
    bool isRestartRequired();
    void setSSTChanged(bool fChanged);
    bool isSSTChanged();
    bool resetSettings;

    bool isColdStakingScreenEnabled() { return showColdStakingScreen; }
    bool invertColdStakingScreenStatus() {
        setData(
                createIndex(ShowColdStakingScreen, 0),
                !isColdStakingScreenEnabled(),
                Qt::EditRole
        );
        return showColdStakingScreen;
    }

    // Reset
    void setMainDefaultOptions(QSettings& settings, bool reset = false);
    void setWalletDefaultOptions(QSettings& settings, bool reset = false);
    void setNetworkDefaultOptions(QSettings& settings, bool reset = false);
    void setWindowDefaultOptions(QSettings& settings, bool reset = false);
    void setDisplayDefaultOptions(QSettings& settings, bool reset = false);

private:
    /* Qt-only settings */
    bool fMinimizeToTray;
    bool fMinimizeOnClose;
    QString language;
    int nDisplayUnit;
    QString strThirdPartyTxUrls;
    bool fCoinControlFeatures;
    bool showColdStakingScreen;
    bool fHideCharts;
    bool fHideZeroBalances;
    bool fHideOrphans;
    /* settings that were overriden by command-line */
    QString strOverriddenByCommandLine;

    /// Add option to list of GUI options overridden through command line/config file
    void addOverriddenOption(const std::string& option);

Q_SIGNALS:
    void displayUnitChanged(int unit);
    void zeromintEnableChanged(bool);
    void zeromintAddressesChanged(bool);
    void zeromintPercentageChanged(int);
    void preferredDenomChanged(int);
    void anonymizeSombeAmountChanged(int);
    void coinControlFeaturesChanged(bool);
    void showHideColdStakingScreen(bool);
    void hideChartsChanged(bool);
    void hideZeroBalancesChanged(bool);
    void hideOrphansChanged(bool);
};

#endif // BITCOIN_QT_OPTIONSMODEL_H
