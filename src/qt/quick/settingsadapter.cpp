// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/settingsadapter.h>

#include <amount.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <netbase.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/notificationlifetime.h>
#include <qt/optionsmodel.h>
#include <qt/quick/hostlocalpaths.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/shelladapter.h>
#include <qt/quick/sidestakelistmodel.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QJSEngine>
#include <QLocale>
#include <QMetaObject>
#include <QProcess>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QVariantMap>

#include <string>

namespace {

//! The options model rows this adapter stages, in OptionsModel::OptionID
//! order. The reserve is staged as satoshis and held apart.
const int STAGED_ROWS[] = {
    OptionsModel::StartAtStartup,
    OptionsModel::MinimizeToTray,
    OptionsModel::ConfirmOnClose,
    OptionsModel::StartMin,
    OptionsModel::DisableTrxNotifications,
    OptionsModel::DisablePollNotifications,
    OptionsModel::MapPortUPnP,
    OptionsModel::MinimizeOnClose,
    OptionsModel::ProxyUse,
    OptionsModel::ProxyIP,
    OptionsModel::ProxyPort,
    OptionsModel::DisplayUnit,
    OptionsModel::DisplayAddresses,
    OptionsModel::Language,
    OptionsModel::WalletStylesheet,
    OptionsModel::CoinControlFeatures,
    OptionsModel::DisableUpdateCheck,
    OptionsModel::EnableStaking,
    OptionsModel::EnableStakeSplit,
    OptionsModel::EnableSideStaking,
    OptionsModel::StakingEfficiency,
    OptionsModel::MinStakeSplitValue,
    OptionsModel::ContractChangeToInput,
};

bool IsProxyRow(int row)
{
    return row == OptionsModel::ProxyUse || row == OptionsModel::ProxyIP || row == OptionsModel::ProxyPort;
}

//! The rows whose change needs a restart, as OptionsDialog warns.
bool NeedsRestart(int row)
{
    return IsProxyRow(row) || row == OptionsModel::Language;
}

QString ProxyInvalidText()
{
    return QCoreApplication::translate("OptionsDialog", "The supplied proxy address is invalid.");
}

QString NotSavedText()
{
    return QCoreApplication::translate("OptionsDialog", "A setting could not be saved.");
}

//! OptionsDialog's staking efficiency rule, whole numbers only.
bool ParseEfficiency(const QString& text, int& value)
{
    bool ok = false;
    value = text.trimmed().toInt(&ok, 10);
    return ok && value >= 75 && value <= 98;
}

//! OptionsDialog's minimum stake-split rule.
bool ParseMinStakeSplit(const QString& text, qint64& value)
{
    bool ok = false;
    value = text.trimmed().toLongLong(&ok, 10);
    return ok && value >= MIN_STAKE_SPLIT_VALUE_GRC && value <= MAX_MONEY / COIN;
}

} // namespace

//! What the seed reads on the serial lane: the core settings, with the keys
//! and defaults OptionsModel::data uses. A plain value.
struct SettingsAdapter::SeedResult {
    bool disable_update_check{false};
    qint64 reserve_sat{0};
    bool staking{true};
    bool stake_split{false};
    bool side_staking{false};
    bool contract_change_to_input{false};
    int64_t staking_efficiency{90};
    int64_t min_stake_split{MIN_STAKE_SPLIT_VALUE_GRC};
};

SettingsAdapter::SettingsAdapter(int marker, ShellAdapter& shell)
    : QObject(nullptr),
      m_marker(marker),
      m_shell(shell),
      m_side_stakes(std::make_unique<SideStakeListModel>()),
      m_staged(OptionsModel::OptionIDRowCount),
      m_seed(OptionsModel::OptionIDRowCount)
{
    // OptionsModel's defaults, shown until the first seed.
    for (const int row : STAGED_ROWS) m_seed[row] = false;
    m_seed[OptionsModel::StartMin] = true;
    m_seed[OptionsModel::EnableStaking] = true;
    m_seed[OptionsModel::ProxyIP] = QStringLiteral("127.0.0.1");
    m_seed[OptionsModel::ProxyPort] = 9050;
    m_seed[OptionsModel::DisplayUnit] = static_cast<int>(BitcoinUnits::BTC);
    m_seed[OptionsModel::Language] = QString();
    m_seed[OptionsModel::WalletStylesheet] = QStringLiteral("dark");
    m_seed[OptionsModel::StakingEfficiency] = QStringLiteral("90");
    m_seed[OptionsModel::MinStakeSplitValue] = QString::number(MIN_STAKE_SPLIT_VALUE_GRC);
    m_staged = m_seed;
    m_reserve_text = BitcoinUnits::format(BitcoinUnits::BTC, 0);
}

SettingsAdapter::~SettingsAdapter()
{
    if (m_lifetime) m_lifetime->retire();
    m_rw_settings_handler.reset();
}

SettingsAdapter* SettingsAdapter::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("SettingsAdapter::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->settings(), QJSEngine::CppOwnership);
    return &provider->settings();
}

void SettingsAdapter::setHostLocalPaths(const HostLocalPaths* paths)
{
    m_host_paths = paths;
}

void SettingsAdapter::attach(ClientModel& client, OptionsModel* options, QmlCallRunner& runner)
{
    if (m_node || m_options || m_runner) detach();
    m_node = &client.node();
    m_options = options;
    m_runner = &runner;
    ++m_epoch;
    m_lifetime = std::make_unique<NotificationLifetime>();

    if (m_options) {
        // A changed setting, from any writer, is re-read on the serial lane;
        // the callback only posts to this thread.
        m_rw_settings_handler = client.node().handleRwSettingsUpdated(m_lifetime->guard([this] {
            QMetaObject::invokeMethod(this, [this] { onRwSettingsUpdated(); }, Qt::QueuedConnection);
        }));
        connect(m_options, &OptionsModel::displayUnitChanged, this, [this](int unit) {
            // Another writer changed the unit: an untouched unit follows it,
            // and the reserve is shown in it.
            if (m_touched.contains(OptionsModel::DisplayUnit)) return;
            m_seed[OptionsModel::DisplayUnit] = unit;
            if (m_staged[OptionsModel::DisplayUnit] == QVariant(unit)) return;
            m_staged[OptionsModel::DisplayUnit] = unit;
            formatReserve();
            emit changed();
        });
        connect(m_options, &OptionsModel::walletStylesheetChanged, this,
                [this](const QString&) { setAppliedTheme(m_options->getCurrentStyle()); });
        m_side_stakes->attach(m_options->getSideStakeTableModel());
    }

    pushWindowOptions();
    if (m_options) setAppliedTheme(m_options->getCurrentStyle());
    m_seed_in_flight = false;
    m_seed_pending = false;
    postSeed();
}

void SettingsAdapter::detach()
{
    if (!m_node && !m_options && !m_runner) return;
    m_rw_settings_handler.reset();
    if (m_lifetime) m_lifetime->retire();
    ++m_epoch;
    OptionsModel* options = m_options;
    m_node = nullptr;
    m_options = nullptr;
    m_runner = nullptr;
    m_seed_in_flight = false;
    m_seed_pending = false;
    if (options) disconnect(options, nullptr, this, nullptr);
    m_side_stakes->detach();
    setLoaded(false);
}

bool SettingsAdapter::apply()
{
    if (!m_options || !m_runner) return false;
    setLastError(QString());

    // Hidden fields are never submitted: with stake splitting off, its two
    // values go back to what was read.
    if (!enableStakeSplit()) {
        bool reset = false;
        for (const int row : {static_cast<int>(OptionsModel::StakingEfficiency),
                              static_cast<int>(OptionsModel::MinStakeSplitValue)}) {
            if (m_touched.remove(row) || m_staged[row] != m_seed[row]) reset = true;
            m_staged[row] = m_seed[row];
        }
        if (reset) emit changed();
    }

    // While the proxy is off its address and port fields are disabled: a
    // value staged there is neither checked nor submitted, and goes back to
    // what was read.
    if (!proxyUse()) {
        bool reset = false;
        for (const int row : {static_cast<int>(OptionsModel::ProxyIP),
                              static_cast<int>(OptionsModel::ProxyPort)}) {
            if (m_touched.remove(row) || m_staged[row] != m_seed[row]) reset = true;
            m_staged[row] = m_seed[row];
        }
        if (reset) emit changed();
    }

    for (const int row : STAGED_ROWS) {
        if (!m_touched.contains(row)) continue;
        const QString refusal = validate(row);
        if (!refusal.isEmpty()) {
            setLastError(refusal);
            return false;
        }
    }
    if (m_touched.contains(OptionsModel::ReserveBalance) && (!m_reserve_valid || m_reserve_sat < 0)) {
        // BitcoinUnits::parse accepts a leading minus, and the options model
        // would store a negative reserve as zero.
        setLastError(NotSavedText());
        return false;
    }

    bool ok = true;
    bool restart = false;
    QString first_error;
    for (int row = 0; row < OptionsModel::OptionIDRowCount; ++row) {
        if (!m_touched.contains(row)) continue;
        // The login item is never written from this front end.
        if ((row == OptionsModel::StartAtStartup || row == OptionsModel::StartMin) && !startupOptionsAvailable()) {
            continue;
        }
        if (!differsFromSeed(row)) continue;
        if (m_options->setData(m_options->index(row), submitted(row), Qt::EditRole)) {
            if (NeedsRestart(row)) restart = true;
        } else {
            ok = false;
            if (first_error.isEmpty()) first_error = IsProxyRow(row) ? ProxyInvalidText() : NotSavedText();
        }
    }
    if (!ok) setLastError(first_error);
    if (restart && !m_restart_required) {
        m_restart_required = true;
        emit restartRequiredChanged();
    }

    m_touched.clear();
    pushWindowOptions();
    postSeed();
    emit changed();
    return ok;
}

void SettingsAdapter::discard()
{
    if (!m_options || !m_runner) return;
    setLastError(QString());
    m_touched.clear();
    copySeed();
    emit changed();
}

void SettingsAdapter::reload()
{
    if (!m_options || !m_runner) return;
    postSeed();
}

bool SettingsAdapter::openConfigFile()
{
    if (!m_options || !m_runner || !hostPathsAvailable()) return false;
    const QString path = m_host_paths->configFile();
    // BitcoinGUI::openConfigClicked's handlers, each tried only when the one
    // before it failed.
    bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#ifdef Q_OS_WIN
    if (!opened) {
        opened = QProcess::startDetached(QStringLiteral("C:\\Windows\\system32\\notepad.exe"), QStringList{path});
    }
#endif
#ifdef Q_OS_MAC
    if (!opened) {
        opened = QProcess::startDetached(QStringLiteral("/usr/bin/open"), QStringList{QStringLiteral("-t"), path});
    }
#endif
    if (!opened) {
        setLastError(QCoreApplication::translate("BitcoinGUI", "Unable to open the config file. Please check your "
                                                               "operating system file associations."));
        return false;
    }
    return true;
}

bool SettingsAdapter::startAtStartup() const { return staged(OptionsModel::StartAtStartup).toBool(); }
bool SettingsAdapter::startAtStartupMinimized() const { return staged(OptionsModel::StartMin).toBool(); }
bool SettingsAdapter::disableUpdateCheck() const { return staged(OptionsModel::DisableUpdateCheck).toBool(); }
bool SettingsAdapter::enableStaking() const { return staged(OptionsModel::EnableStaking).toBool(); }
bool SettingsAdapter::enableStakeSplit() const { return staged(OptionsModel::EnableStakeSplit).toBool(); }
bool SettingsAdapter::enableSideStaking() const { return staged(OptionsModel::EnableSideStaking).toBool(); }
bool SettingsAdapter::sideStakingStored() const { return m_seed[OptionsModel::EnableSideStaking].toBool(); }
bool SettingsAdapter::contractChangeToInput() const { return staged(OptionsModel::ContractChangeToInput).toBool(); }
QString SettingsAdapter::stakingEfficiency() const { return staged(OptionsModel::StakingEfficiency).toString(); }
QString SettingsAdapter::minStakeSplitValue() const { return staged(OptionsModel::MinStakeSplitValue).toString(); }
bool SettingsAdapter::mapPortUpnp() const { return staged(OptionsModel::MapPortUPnP).toBool(); }
bool SettingsAdapter::proxyUse() const { return staged(OptionsModel::ProxyUse).toBool(); }
QString SettingsAdapter::proxyIp() const { return staged(OptionsModel::ProxyIP).toString(); }
int SettingsAdapter::proxyPort() const { return staged(OptionsModel::ProxyPort).toInt(); }
bool SettingsAdapter::minimizeToTray() const { return staged(OptionsModel::MinimizeToTray).toBool(); }
bool SettingsAdapter::minimizeOnClose() const { return staged(OptionsModel::MinimizeOnClose).toBool(); }
bool SettingsAdapter::confirmOnClose() const { return staged(OptionsModel::ConfirmOnClose).toBool(); }
bool SettingsAdapter::disableTrxNotifications() const { return staged(OptionsModel::DisableTrxNotifications).toBool(); }
bool SettingsAdapter::disablePollNotifications() const { return staged(OptionsModel::DisablePollNotifications).toBool(); }
int SettingsAdapter::displayUnit() const { return staged(OptionsModel::DisplayUnit).toInt(); }
QString SettingsAdapter::language() const { return staged(OptionsModel::Language).toString(); }
QString SettingsAdapter::theme() const { return staged(OptionsModel::WalletStylesheet).toString(); }
bool SettingsAdapter::displayAddresses() const { return staged(OptionsModel::DisplayAddresses).toBool(); }
bool SettingsAdapter::coinControlFeatures() const { return staged(OptionsModel::CoinControlFeatures).toBool(); }

void SettingsAdapter::setStartAtStartup(bool value) { stage(OptionsModel::StartAtStartup, value); }
void SettingsAdapter::setStartAtStartupMinimized(bool value) { stage(OptionsModel::StartMin, value); }
void SettingsAdapter::setDisableUpdateCheck(bool value) { stage(OptionsModel::DisableUpdateCheck, value); }
void SettingsAdapter::setEnableStaking(bool value) { stage(OptionsModel::EnableStaking, value); }
void SettingsAdapter::setEnableStakeSplit(bool value) { stage(OptionsModel::EnableStakeSplit, value); }
void SettingsAdapter::setEnableSideStaking(bool value) { stage(OptionsModel::EnableSideStaking, value); }
void SettingsAdapter::setContractChangeToInput(bool value) { stage(OptionsModel::ContractChangeToInput, value); }
void SettingsAdapter::setStakingEfficiency(const QString& value) { stage(OptionsModel::StakingEfficiency, value); }
void SettingsAdapter::setMinStakeSplitValue(const QString& value) { stage(OptionsModel::MinStakeSplitValue, value); }
void SettingsAdapter::setMapPortUpnp(bool value) { stage(OptionsModel::MapPortUPnP, value); }
void SettingsAdapter::setProxyUse(bool value) { stage(OptionsModel::ProxyUse, value); }
void SettingsAdapter::setProxyIp(const QString& value) { stage(OptionsModel::ProxyIP, value); }
void SettingsAdapter::setProxyPort(int value) { stage(OptionsModel::ProxyPort, value); }
void SettingsAdapter::setMinimizeToTray(bool value) { stage(OptionsModel::MinimizeToTray, value); }
void SettingsAdapter::setMinimizeOnClose(bool value) { stage(OptionsModel::MinimizeOnClose, value); }
void SettingsAdapter::setConfirmOnClose(bool value) { stage(OptionsModel::ConfirmOnClose, value); }
void SettingsAdapter::setDisableTrxNotifications(bool value) { stage(OptionsModel::DisableTrxNotifications, value); }
void SettingsAdapter::setDisablePollNotifications(bool value) { stage(OptionsModel::DisablePollNotifications, value); }
void SettingsAdapter::setLanguage(const QString& value) { stage(OptionsModel::Language, value); }
void SettingsAdapter::setTheme(const QString& value) { stage(OptionsModel::WalletStylesheet, value); }
void SettingsAdapter::setDisplayAddresses(bool value) { stage(OptionsModel::DisplayAddresses, value); }
void SettingsAdapter::setCoinControlFeatures(bool value) { stage(OptionsModel::CoinControlFeatures, value); }

void SettingsAdapter::setDisplayUnit(int value)
{
    if (staged(OptionsModel::DisplayUnit) == QVariant(value)) return;
    m_staged[OptionsModel::DisplayUnit] = value;
    m_touched.insert(OptionsModel::DisplayUnit);
    // The reserve is shown in the staged unit.
    formatReserve();
    emit changed();
}

void SettingsAdapter::setReserveBalance(const QString& text)
{
    if (text == m_reserve_text) return;
    m_reserve_text = text;
    // The text is kept as typed; a text that does not parse is refused by
    // apply().
    qint64 sat = 0;
    m_reserve_valid = BitcoinUnits::parse(displayUnit(), text.trimmed(), &sat);
    if (m_reserve_valid) m_reserve_sat = sat;
    m_touched.insert(OptionsModel::ReserveBalance);
    emit changed();
}

bool SettingsAdapter::dirty() const
{
    for (const int row : m_touched) {
        if (row == OptionsModel::ReserveBalance) {
            if (!m_reserve_valid || m_reserve_sat != m_seed_reserve_sat) return true;
        } else if (m_staged[row] != m_seed[row]) {
            return true;
        }
    }
    return false;
}

bool SettingsAdapter::trayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool SettingsAdapter::updateCheckOptionAvailable() const
{
    // OptionsDialog shows the box on mainnet only.
    return m_options && m_options->isMainNet();
}

bool SettingsAdapter::upnpAvailable() const
{
    // OptionsModel::data's test: whether UPnP was compiled in, whatever its
    // default.
#ifdef USE_UPNP
    return true;
#else
    return false;
#endif
}

bool SettingsAdapter::confirmOnCloseAvailable() const
{
#ifdef Q_OS_MAC
    return false;
#else
    return true;
#endif
}

bool SettingsAdapter::hostPathsAvailable() const
{
    return m_host_paths && m_host_paths->available();
}

QVariantList SettingsAdapter::languages() const
{
    // OptionsDialog's list: the default first, then each translation the
    // resources hold.
    QVariantList list;
    list << QVariantMap{
        {QStringLiteral("value"), QString()},
        {QStringLiteral("text"),
         QStringLiteral("(") + QCoreApplication::translate("OptionsDialog", "default") + QStringLiteral(")")}};
    for (const QString& name : QDir(QStringLiteral(":translations")).entryList()) {
        const QLocale locale(name);
        QString text;
        if (name.contains(QLatin1Char('_'))) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
            const QString country = locale.nativeTerritoryName();
#else
            const QString country = locale.nativeCountryName();
#endif
            text = locale.nativeLanguageName() + QStringLiteral(" - ") + country + QStringLiteral(" (") + name +
                   QStringLiteral(")");
        } else {
            text = locale.nativeLanguageName() + QStringLiteral(" (") + name + QStringLiteral(")");
        }
        list << QVariantMap{{QStringLiteral("value"), name}, {QStringLiteral("text"), text}};
    }
    return list;
}

QVariantList SettingsAdapter::displayUnits() const
{
    QVariantList list;
    for (const BitcoinUnits::Unit unit : BitcoinUnits::availableUnits()) {
        list << QVariantMap{{QStringLiteral("value"), static_cast<int>(unit)},
                            {QStringLiteral("text"), BitcoinUnits::name(unit)}};
    }
    return list;
}

QObject* SettingsAdapter::sideStakes() const
{
    return m_side_stakes.get();
}

void SettingsAdapter::stage(int row, const QVariant& value)
{
    if (staged(row) == value) return;
    m_staged[row] = value;
    m_touched.insert(row);
    emit changed();
}

QVariant SettingsAdapter::staged(int row) const
{
    return m_staged[row];
}

QVariant SettingsAdapter::submitted(int row) const
{
    switch (row) {
    case OptionsModel::ReserveBalance:
        return QVariant(qint64{m_reserve_sat});
    case OptionsModel::StakingEfficiency: {
        int value = 0;
        ParseEfficiency(m_staged[row].toString(), value);
        return QString::number(value);
    }
    case OptionsModel::MinStakeSplitValue: {
        qint64 value = 0;
        ParseMinStakeSplit(m_staged[row].toString(), value);
        return QString::number(value);
    }
    default:
        return m_staged[row];
    }
}

bool SettingsAdapter::differsFromSeed(int row) const
{
    switch (row) {
    case OptionsModel::ReserveBalance:
        return m_reserve_sat != m_seed_reserve_sat;
    case OptionsModel::StakingEfficiency:
    case OptionsModel::MinStakeSplitValue:
        return submitted(row) != m_seed[row];
    default:
        return m_staged[row] != m_seed[row];
    }
}

QString SettingsAdapter::validate(int row) const
{
    switch (row) {
    case OptionsModel::StakingEfficiency: {
        int value = 0;
        if (ParseEfficiency(m_staged[row].toString(), value)) return QString();
        return QCoreApplication::translate("OptionsDialog", "The supplied target staking efficiency is invalid.");
    }
    case OptionsModel::MinStakeSplitValue: {
        qint64 value = 0;
        if (ParseMinStakeSplit(m_staged[row].toString(), value)) return QString();
        return QCoreApplication::translate("OptionsDialog",
                                           "The supplied minimum post stake-split UTXO size is invalid.");
    }
    case OptionsModel::ProxyUse:
    case OptionsModel::ProxyIP: {
        if (!proxyUse()) return QString();
        // OptionsDialog's check of the proxy address.
        const std::string ip = proxyIp().toStdString();
        if (proxyType(LookupNumeric(ip.c_str(), 9050), true).IsValid()) return QString();
        return ProxyInvalidText();
    }
    case OptionsModel::ProxyPort: {
        // Checked only while the proxy is used, as the address is.
        if (!proxyUse()) return QString();
        const int port = proxyPort();
        if (port >= 1 && port <= 65535) return QString();
        return ProxyInvalidText();
    }
    default:
        return QString();
    }
}

void SettingsAdapter::copySeed()
{
    for (const int row : STAGED_ROWS) m_staged[row] = m_seed[row];
    m_reserve_sat = m_seed_reserve_sat;
    formatReserve();
}

void SettingsAdapter::postSeed()
{
    if (!m_node || !m_options || !m_runner) return;
    if (m_seed_in_flight) {
        m_seed_pending = true;
        return;
    }
    interfaces::Node& node = *m_node;
    auto seed = std::make_shared<SeedResult>();
    const uint64_t epoch = m_epoch;
    // OptionsModel::data's keys and defaults for the core settings. A closed
    // runner refuses the post; the values then stay as they are.
    if (m_runner->postSerial(
            [&node, seed] {
                seed->disable_update_check = node.getSettingBool("disableupdatecheck", false);
                qint64 sat = 0;
                BitcoinUnits::parse(BitcoinUnits::BTC, QString::fromStdString(node.getSettingStr("reservebalance", "")),
                                    &sat);
                seed->reserve_sat = sat;
                seed->staking = node.getSettingBool("staking", true);
                seed->stake_split = node.getSettingBool("enablestakesplit", false);
                seed->side_staking = node.getSettingBool("enablesidestaking", false);
                seed->contract_change_to_input = node.getSettingBool("contractchangetoinputaddress", false);
                seed->staking_efficiency = node.getSettingInt("stakingefficiency", 90);
                seed->min_stake_split = node.getSettingInt("minstakesplitvalue", MIN_STAKE_SPLIT_VALUE_GRC);
            },
            [this, seed, epoch] { deliverSeed(epoch, *seed); })) {
        m_seed_in_flight = true;
        m_seed_pending = false;
    }
}

void SettingsAdapter::deliverSeed(uint64_t epoch, const SeedResult& result)
{
    if (epoch != m_epoch || !m_options) return;
    m_seed_in_flight = false;
    if (m_seed_pending) {
        // A newer read was asked for while this one ran: it carries the
        // values, so this one is dropped.
        postSeed();
        if (m_seed_in_flight) return;
    }

    OptionsModel& options = *m_options;
    std::vector<QVariant> next = m_seed;
    const auto edit_value = [&options](int row) { return options.data(options.index(row), Qt::EditRole); };
    next[OptionsModel::StartAtStartup] = options.getStartAtStartup();
    next[OptionsModel::MinimizeToTray] = options.getMinimizeToTray();
    next[OptionsModel::ConfirmOnClose] = options.getConfirmOnClose();
    next[OptionsModel::StartMin] = options.getStartMin();
    next[OptionsModel::DisableTrxNotifications] = options.getDisableTrxNotifications();
    next[OptionsModel::DisablePollNotifications] = options.getDisablePollNotifications();
    next[OptionsModel::MapPortUPnP] = edit_value(OptionsModel::MapPortUPnP).toBool();
    next[OptionsModel::MinimizeOnClose] = options.getMinimizeOnClose();
    next[OptionsModel::ProxyUse] = edit_value(OptionsModel::ProxyUse).toBool();
    next[OptionsModel::ProxyIP] = edit_value(OptionsModel::ProxyIP).toString();
    next[OptionsModel::ProxyPort] = edit_value(OptionsModel::ProxyPort).toInt();
    next[OptionsModel::DisplayUnit] = options.getDisplayUnit();
    next[OptionsModel::DisplayAddresses] = options.getDisplayAddresses();
    next[OptionsModel::Language] = edit_value(OptionsModel::Language).toString();
    next[OptionsModel::WalletStylesheet] = options.getCurrentStyle();
    next[OptionsModel::CoinControlFeatures] = options.getCoinControlFeatures();
    next[OptionsModel::DisableUpdateCheck] = result.disable_update_check;
    next[OptionsModel::EnableStaking] = result.staking;
    next[OptionsModel::EnableStakeSplit] = result.stake_split;
    next[OptionsModel::EnableSideStaking] = result.side_staking;
    next[OptionsModel::StakingEfficiency] = QString::number(result.staking_efficiency);
    next[OptionsModel::MinStakeSplitValue] = QString::number(result.min_stake_split);
    next[OptionsModel::ContractChangeToInput] = result.contract_change_to_input;

    // A property follows the new value when it is untouched or still shows
    // the value read before.
    for (const int row : STAGED_ROWS) {
        if (!m_touched.contains(row) || m_staged[row] == m_seed[row]) {
            m_staged[row] = next[row];
            m_touched.remove(row);
        }
    }
    m_seed = std::move(next);
    if (!m_touched.contains(OptionsModel::ReserveBalance) ||
        (m_reserve_valid && m_reserve_sat == m_seed_reserve_sat)) {
        m_reserve_sat = result.reserve_sat;
        m_touched.remove(OptionsModel::ReserveBalance);
        formatReserve();
    }
    m_seed_reserve_sat = result.reserve_sat;

    ++m_seed_generation;
    setLoaded(true);
    emit changed();
    emit seedGenerationChanged();
}

void SettingsAdapter::onRwSettingsUpdated()
{
    if (!m_options || !m_runner) return;
    reload();
}

void SettingsAdapter::pushWindowOptions()
{
    if (m_options) {
        m_shell.setWindowOptions(m_options->getMinimizeToTray(), m_options->getMinimizeOnClose(),
                                 m_options->getConfirmOnClose());
    } else {
        m_shell.setWindowOptions(false, false, false);
    }
}

void SettingsAdapter::setAppliedTheme(const QString& theme)
{
    if (m_applied_theme == theme) return;
    m_applied_theme = theme;
    emit appliedThemeChanged();
}

void SettingsAdapter::setLastError(const QString& text)
{
    if (m_last_error == text) return;
    m_last_error = text;
    emit lastErrorChanged();
}

void SettingsAdapter::setLoaded(bool loaded)
{
    if (m_loaded == loaded) return;
    m_loaded = loaded;
    emit loadedChanged();
}

void SettingsAdapter::formatReserve()
{
    m_reserve_text = BitcoinUnits::format(displayUnit(), m_reserve_sat);
    m_reserve_valid = true;
}
