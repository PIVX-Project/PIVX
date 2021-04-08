// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
//Copyright (c) 2015-2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/Sombe-config.h"
#endif

#include "optionsmodel.h"

#include "bitcoinunits.h"
#include "guiutil.h"

#include "amount.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "txdb.h" // for -dbcache defaults
#include "util.h"

#ifdef ENABLE_WALLET
#include "masternodeconfig.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <QNetworkProxy>
#include <QStringList>

OptionsModel::OptionsModel(QObject* parent) : QAbstractListModel(parent)
{
    Init();
}

void OptionsModel::addOverriddenOption(const std::string& option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(mapArgs[option]) + " ";
}

// Writes all missing QSettings with their default values
void OptionsModel::Init()
{
    resetSettings = false;
    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);
    setSSTChanged(false);

    // These are Qt-only settings:

    // Window
    setWindowDefaultOptions(settings);

    // Display
    if (!settings.contains("fHideZeroBalances"))
        settings.setValue("fHideZeroBalances", true);
    fHideZeroBalances = settings.value("fHideZeroBalances").toBool();

    if (!settings.contains("fHideOrphans"))
        settings.setValue("fHideOrphans", true);
    fHideOrphans = settings.value("fHideOrphans").toBool();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    if (!settings.contains("fShowColdStakingScreen"))
        settings.setValue("fShowColdStakingScreen", false);
    showColdStakingScreen = settings.value("fShowColdStakingScreen", false).toBool();

    if (!settings.contains("fShowMasternodesTab"))
        settings.setValue("fShowMasternodesTab", masternodeConfig.getCount());

    // Main
    setMainDefaultOptions(settings);

    // Wallet
#ifdef ENABLE_WALLET
    setWalletDefaultOptions(settings);
#endif

    // Network
    setNetworkDefaultOptions(settings);
    // Display
    setDisplayDefaultOptions(settings);

    language = settings.value("language").toString();
}

void OptionsModel::refreshDataView()
{
    Q_EMIT dataChanged(index(0), index(rowCount(QModelIndex()) - 1));
}

void OptionsModel::setMainDefaultOptions(QSettings& settings, bool reset)
{
    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    //
    // If setting doesn't exist create it with defaults.
    //
    // If SoftSetArg() or SoftSetBoolArg() return false we were overridden
    // by command-line and show this in the UI.
    // Main
    if (!settings.contains("nDatabaseCache") || reset)
        settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);
    if (!SoftSetArg("-dbcache", settings.value("nDatabaseCache").toString().toStdString()))
        addOverriddenOption("-dbcache");

    if (!settings.contains("nThreadsScriptVerif") || reset)
        settings.setValue("nThreadsScriptVerif", DEFAULT_SCRIPTCHECK_THREADS);
    if (!SoftSetArg("-par", settings.value("nThreadsScriptVerif").toString().toStdString()))
        addOverriddenOption("-par");

    if (reset) {
        refreshDataView();
    }
}

void OptionsModel::setWalletDefaultOptions(QSettings& settings, bool reset)
{
    if (!settings.contains("bSpendZeroConfChange") || reset)
        settings.setValue("bSpendZeroConfChange", false);
    if (!SoftSetBoolArg("-spendzeroconfchange", settings.value("bSpendZeroConfChange").toBool()))
        addOverriddenOption("-spendzeroconfchange");
    if (reset) {
        setStakeSplitThreshold(CWallet::DEFAULT_STAKE_SPLIT_THRESHOLD);
        setUseCustomFee(false);
        refreshDataView();
    }
}

void OptionsModel::setNetworkDefaultOptions(QSettings& settings, bool reset)
{
    if (!settings.contains("fUseUPnP") || reset)
        settings.setValue("fUseUPnP", DEFAULT_UPNP);
    if (!SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool()))
        addOverriddenOption("-upnp");

    if (!settings.contains("fListen") || reset)
        settings.setValue("fListen", DEFAULT_LISTEN);
    if (!SoftSetBoolArg("-listen", settings.value("fListen").toBool()))
        addOverriddenOption("-listen");

    if (!settings.contains("fUseProxy") || reset)
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy") || reset)
        settings.setValue("addrProxy", "127.0.0.1:9050");
    // Only try to set -proxy, if user has enabled fUseProxy
    if (settings.value("fUseProxy").toBool() && !SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString()))
        addOverriddenOption("-proxy");
    else if (!settings.value("fUseProxy").toBool() && !GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    if (reset) {
        refreshDataView();
    }
}

void OptionsModel::setWindowDefaultOptions(QSettings& settings, bool reset)
{
    if (!settings.contains("fMinimizeToTray") || reset)
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool();

    if (!settings.contains("fMinimizeOnClose") || reset)
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    if (reset) {
        refreshDataView();
    }
}

void OptionsModel::setDisplayDefaultOptions(QSettings& settings, bool reset)
{
    if (!settings.contains("nDisplayUnit") || reset)
        settings.setValue("nDisplayUnit", BitcoinUnits::SBE);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();
    if (!settings.contains("digits") || reset)
        settings.setValue("digits", "2");
    if (!settings.contains("theme") || reset)
        settings.setValue("theme", "");
    if (!settings.contains("fCSSexternal") || reset)
        settings.setValue("fCSSexternal", false);
    if (!settings.contains("language") || reset)
        settings.setValue("language", "");
    if (!SoftSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    if (settings.contains("nAnonymizeSombeAmount") || reset)
        SoftSetArg("-anonymizeSombeamount", settings.value("nAnonymizeSombeAmount").toString().toStdString());

    if (!settings.contains("strThirdPartyTxUrls") || reset)
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    fHideCharts = GetBoolArg("-hidecharts", false);

    if (reset) {
        refreshDataView();
    }
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Remove all entries from our QSettings object
    settings.clear();
    resetSettings = true; // Needed in Sombe.cpp during shotdown to also remove the window positions

    // default setting for OptionsModel::StartAtStartup - disabled
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex& parent) const
{
    return OptionIDRowCount;
}

// read QSettings values and return them
QVariant OptionsModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::EditRole) {
        QSettings settings;
        switch (index.row()) {
        case StartAtStartup:
            return GUIUtil::GetStartOnSystemStartup();
        case MinimizeToTray:
            return fMinimizeToTray;
        case MapPortUPnP:
#ifdef USE_UPNP
            return settings.value("fUseUPnP");
#else
            return false;
#endif
        case MinimizeOnClose:
            return fMinimizeOnClose;

        // default proxy
        case ProxyUse:
            return settings.value("fUseProxy", false);
        case ProxyIP: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(0);
        }
        case ProxyPort: {
            // contains IP at index 0 and port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            return strlIpPort.at(1);
        }

#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            return settings.value("bSpendZeroConfChange");
        case ShowMasternodesTab:
            return settings.value("fShowMasternodesTab");
        case StakeSplitThreshold:
        {
            // Return CAmount/qlonglong as double
            const CAmount nStakeSplitThreshold = (pwalletMain) ? pwalletMain->nStakeSplitThreshold : CWallet::DEFAULT_STAKE_SPLIT_THRESHOLD;
            return QVariant(static_cast<double>(nStakeSplitThreshold / static_cast<double>(COIN)));
        }
        case fUseCustomFee:
            return QVariant((pwalletMain) ? pwalletMain->fUseCustomFee : false);
        case nCustomFee:
            return QVariant(static_cast<qlonglong>((pwalletMain) ? pwalletMain->nCustomFee : CWallet::GetRequiredFee(1000)));
#endif
        case DisplayUnit:
            return nDisplayUnit;
        case ThirdPartyTxUrls:
            return strThirdPartyTxUrls;
        case Digits:
            return settings.value("digits");
        case Theme:
            return settings.value("theme");
        case Language:
            return settings.value("language");
        case CoinControlFeatures:
            return fCoinControlFeatures;
        case ShowColdStakingScreen:
            return showColdStakingScreen;
        case DatabaseCache:
            return settings.value("nDatabaseCache");
        case ThreadsScriptVerif:
            return settings.value("nThreadsScriptVerif");
        case HideCharts:
            return fHideCharts;
        case HideZeroBalances:
            return settings.value("fHideZeroBalances");
        case HideOrphans:
            return settings.value("fHideOrphans");
        case Listen:
            return settings.value("fListen");
        default:
            return QVariant();
        }
    }
    return QVariant();
}

// write QSettings values
bool OptionsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    bool successful = true; /* set to false on parse error */
    if (role == Qt::EditRole) {
        QSettings settings;
        switch (index.row()) {
        case StartAtStartup:
            successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
            break;
        case MinimizeToTray:
            fMinimizeToTray = value.toBool();
            settings.setValue("fMinimizeToTray", fMinimizeToTray);
            break;
        case MapPortUPnP: // core option - can be changed on-the-fly
            settings.setValue("fUseUPnP", value.toBool());
            MapPort(value.toBool());
            break;
        case MinimizeOnClose:
            fMinimizeOnClose = value.toBool();
            settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
            break;

        // default proxy
        case ProxyUse:
            if (settings.value("fUseProxy") != value) {
                settings.setValue("fUseProxy", value.toBool());
                setRestartRequired(true);
            }
            break;
        case ProxyIP: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed IP
            if (!settings.contains("addrProxy") || strlIpPort.at(0) != value.toString()) {
                // construct new value from new IP and current port
                QString strNewValue = value.toString() + ":" + strlIpPort.at(1);
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        } break;
        case ProxyPort: {
            // contains current IP at index 0 and current port at index 1
            QStringList strlIpPort = settings.value("addrProxy").toString().split(":", QString::SkipEmptyParts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrProxy") || strlIpPort.at(1) != value.toString()) {
                // construct new value from current IP and new port
                QString strNewValue = strlIpPort.at(0) + ":" + value.toString();
                settings.setValue("addrProxy", strNewValue);
                setRestartRequired(true);
            }
        } break;
#ifdef ENABLE_WALLET
        case SpendZeroConfChange:
            if (settings.value("bSpendZeroConfChange") != value) {
                settings.setValue("bSpendZeroConfChange", value);
                setRestartRequired(true);
            }
            break;
        case ShowMasternodesTab:
            if (settings.value("fShowMasternodesTab") != value) {
                settings.setValue("fShowMasternodesTab", value);
                setRestartRequired(true);
            }
            break;
        case fUseCustomFee:
            setUseCustomFee(value.toBool());
            break;
        case nCustomFee:
            setCustomFeeValue(value.toLongLong());
            break;
#endif
        case StakeSplitThreshold:
            // Write double as qlonglong/CAmount
            setStakeSplitThreshold(static_cast<CAmount>(value.toDouble() * COIN));
            setSSTChanged(true);
            break;
        case DisplayUnit:
            setDisplayUnit(value);
            break;
        case ThirdPartyTxUrls:
            if (strThirdPartyTxUrls != value.toString()) {
                strThirdPartyTxUrls = value.toString();
                settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
                setRestartRequired(true);
            }
            break;
        case Digits:
            if (settings.value("digits") != value) {
                settings.setValue("digits", value);
                setRestartRequired(true);
            }
            break;
        case Theme:
            if (settings.value("theme") != value) {
                settings.setValue("theme", value);
                setRestartRequired(true);
            }
            break;
        case Language:
            if (settings.value("language") != value) {
                settings.setValue("language", value);
                setRestartRequired(true);
            }
            break;
        case HideCharts:
            fHideCharts = value.toBool();   // memory only
            Q_EMIT hideChartsChanged(fHideCharts);
            break;
        case HideZeroBalances:
            fHideZeroBalances = value.toBool();
            settings.setValue("fHideZeroBalances", fHideZeroBalances);
            Q_EMIT hideZeroBalancesChanged(fHideZeroBalances);
            break;
        case HideOrphans:
            fHideOrphans = value.toBool();
            settings.setValue("fHideOrphans", fHideOrphans);
            Q_EMIT hideOrphansChanged(fHideOrphans);
            break;
        case CoinControlFeatures:
            fCoinControlFeatures = value.toBool();
            settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
            Q_EMIT coinControlFeaturesChanged(fCoinControlFeatures);
            break;
        case ShowColdStakingScreen:
            this->showColdStakingScreen = value.toBool();
            settings.setValue("fShowColdStakingScreen", this->showColdStakingScreen);
            Q_EMIT showHideColdStakingScreen(this->showColdStakingScreen);
            break;
        case DatabaseCache:
            if (settings.value("nDatabaseCache") != value) {
                settings.setValue("nDatabaseCache", value);
                setRestartRequired(true);
            }
            break;
        case ThreadsScriptVerif:
            if (settings.value("nThreadsScriptVerif") != value) {
                settings.setValue("nThreadsScriptVerif", value);
                setRestartRequired(true);
            }
            break;
        case Listen:
            if (settings.value("fListen") != value) {
                settings.setValue("fListen", value);
                setRestartRequired(true);
            }
            break;
        default:
            break;
        }
    }

    Q_EMIT dataChanged(index, index);

    return successful;
}

/** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
void OptionsModel::setDisplayUnit(const QVariant& value)
{
    if (!value.isNull()) {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        Q_EMIT displayUnitChanged(nDisplayUnit);
    }
}

/* Update StakeSplitThreshold's value in wallet */
void OptionsModel::setStakeSplitThreshold(const CAmount nStakeSplitThreshold)
{
    if (pwalletMain && pwalletMain->nStakeSplitThreshold != nStakeSplitThreshold) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        LOCK(pwalletMain->cs_wallet);
        {
            pwalletMain->nStakeSplitThreshold = nStakeSplitThreshold;
            if (pwalletMain->fFileBacked)
                walletdb.WriteStakeSplitThreshold(nStakeSplitThreshold);
        }
    }
}

/* returns default minimum value for stake split threshold as doulbe */
double OptionsModel::getSSTMinimum() const
{
    return static_cast<double>(CWallet::minStakeSplitThreshold / COIN);
}

/* Verify that StakeSplitThreshold's value is either 0 or above the min. Else reset */
bool OptionsModel::isSSTValid()
{
    if (pwalletMain && pwalletMain->nStakeSplitThreshold &&
            pwalletMain->nStakeSplitThreshold < CWallet::minStakeSplitThreshold) {
        setStakeSplitThreshold(CWallet::minStakeSplitThreshold);
        return false;
    }
    return true;
}

/* Update Custom Fee value in wallet */
void OptionsModel::setUseCustomFee(bool fUse)
{
    if (pwalletMain && pwalletMain->fUseCustomFee != fUse) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->fUseCustomFee = fUse;
            if (pwalletMain->fFileBacked)
                walletdb.WriteUseCustomFee(fUse);
        }
    }
}

void OptionsModel::setCustomFeeValue(const CAmount& value)
{
    if (pwalletMain && pwalletMain->nCustomFee != value) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->nCustomFee = value;
            if (pwalletMain->fFileBacked)
                walletdb.WriteCustomFeeValue(value);
        }
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
    // Directly query current base proxy, because
    // GUI settings can be overridden with -proxy.
    proxyType curProxy;
    if (GetProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.proxy.ToStringIP()));
        proxy.setPort(curProxy.proxy.GetPort());

        return true;
    } else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired()
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}

void OptionsModel::setSSTChanged(bool fChanged)
{
    QSettings settings;
    return settings.setValue("fSSTChanged", fChanged);
}

bool OptionsModel::isSSTChanged()
{
    QSettings settings;
    return settings.value("fSSTChanged", false).toBool();
}
