// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_SETTINGSADAPTER_H
#define BITCOIN_QT_QUICK_SETTINGSADAPTER_H

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QtGlobal>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

class ClientModel;
class HostLocalPaths;
class NotificationLifetime;
class OptionsModel;
class QmlCallRunner;
class QQmlEngine;
class QJSEngine;
class ShellAdapter;
class SideStakeListModel;

namespace interfaces {
class Handler;
class Node;
} // namespace interfaces

//! The QML singleton `Settings`: the settings screen's options, staged here
//! and written through OptionsModel::setData. Each property holds a staged
//! value; apply() submits only the touched properties whose staged value
//! differs from the last value read, in OptionsModel::OptionID order, on the
//! GUI thread. The core settings are read on the serial lane (the seed),
//! except UPnP, which is read with the GUI ones from the options model at
//! delivery. It is owned by QmlProvider,
//! which hands QML this instance through create().
class SettingsAdapter : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_SINGLETON
    Q_PROPERTY(int marker READ marker CONSTANT)

    Q_PROPERTY(bool startAtStartup READ startAtStartup WRITE setStartAtStartup NOTIFY changed)
    Q_PROPERTY(bool startAtStartupMinimized READ startAtStartupMinimized WRITE setStartAtStartupMinimized NOTIFY changed)
    Q_PROPERTY(bool disableUpdateCheck READ disableUpdateCheck WRITE setDisableUpdateCheck NOTIFY changed)
    Q_PROPERTY(QString reserveBalance READ reserveBalance WRITE setReserveBalance NOTIFY changed)
    Q_PROPERTY(bool enableStaking READ enableStaking WRITE setEnableStaking NOTIFY changed)
    Q_PROPERTY(bool enableStakeSplit READ enableStakeSplit WRITE setEnableStakeSplit NOTIFY changed)
    Q_PROPERTY(bool enableSideStaking READ enableSideStaking WRITE setEnableSideStaking NOTIFY changed)
    Q_PROPERTY(bool contractChangeToInput READ contractChangeToInput WRITE setContractChangeToInput NOTIFY changed)
    Q_PROPERTY(QString stakingEfficiency READ stakingEfficiency WRITE setStakingEfficiency NOTIFY changed)
    Q_PROPERTY(QString minStakeSplitValue READ minStakeSplitValue WRITE setMinStakeSplitValue NOTIFY changed)
    Q_PROPERTY(bool mapPortUpnp READ mapPortUpnp WRITE setMapPortUpnp NOTIFY changed)
    Q_PROPERTY(bool proxyUse READ proxyUse WRITE setProxyUse NOTIFY changed)
    Q_PROPERTY(QString proxyIp READ proxyIp WRITE setProxyIp NOTIFY changed)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY changed)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY changed)
    Q_PROPERTY(bool minimizeOnClose READ minimizeOnClose WRITE setMinimizeOnClose NOTIFY changed)
    Q_PROPERTY(bool confirmOnClose READ confirmOnClose WRITE setConfirmOnClose NOTIFY changed)
    Q_PROPERTY(bool disableTrxNotifications READ disableTrxNotifications WRITE setDisableTrxNotifications NOTIFY changed)
    Q_PROPERTY(bool disablePollNotifications READ disablePollNotifications WRITE setDisablePollNotifications NOTIFY changed)
    Q_PROPERTY(int displayUnit READ displayUnit WRITE setDisplayUnit NOTIFY changed)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
    Q_PROPERTY(bool displayAddresses READ displayAddresses WRITE setDisplayAddresses NOTIFY changed)
    Q_PROPERTY(bool coinControlFeatures READ coinControlFeatures WRITE setCoinControlFeatures NOTIFY changed)

    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(int seedGeneration READ seedGeneration NOTIFY seedGenerationChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString appliedTheme READ appliedTheme NOTIFY appliedThemeChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY loadedChanged)
    Q_PROPERTY(bool startupOptionsAvailable READ startupOptionsAvailable CONSTANT)
    Q_PROPERTY(bool updateCheckOptionAvailable READ updateCheckOptionAvailable NOTIFY loadedChanged)
    Q_PROPERTY(bool upnpAvailable READ upnpAvailable CONSTANT)
    Q_PROPERTY(bool confirmOnCloseAvailable READ confirmOnCloseAvailable CONSTANT)
    Q_PROPERTY(bool hostPathsAvailable READ hostPathsAvailable NOTIFY loadedChanged)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QVariantList displayUnits READ displayUnits CONSTANT)
    Q_PROPERTY(QObject* sideStakes READ sideStakes CONSTANT)

public:
    //! `marker` identifies the provider that owns this instance; `shell`
    //! receives the window options at attach and after each apply().
    SettingsAdapter(int marker, ShellAdapter& shell);
    ~SettingsAdapter() override;

    //! The singleton factory: hands QML the installed provider's instance with
    //! C++ ownership. Halts the process when no provider is installed.
    static SettingsAdapter* create(QQmlEngine* qml_engine, QJSEngine* js_engine);

    //! The provider's paths; null leaves hostPathsAvailable false.
    void setHostLocalPaths(const HostLocalPaths* paths);

    //! Follows `options` and the node behind `client`, pushes the window
    //! options to the shell, reads the stored theme and posts a seed. With a
    //! null `options` the adapter stays unloaded and every invokable answers
    //! false. The models and the runner must outlive the matching detach().
    void attach(ClientModel& client, OptionsModel* options, QmlCallRunner& runner);
    //! Stops following the models; a seed still in flight delivers nothing.
    //! appliedTheme keeps its value. A detach() before attach() does nothing.
    void detach();

    //! Validates the touched properties and submits the changed ones; see the
    //! class comment. False when a property is refused (nothing is submitted
    //! then) or when the options model refuses a row (the rest are still
    //! submitted). Unattached, false.
    Q_INVOKABLE bool apply();
    //! Puts every property back to the last value read; submits nothing.
    Q_INVOKABLE void discard();
    //! Posts a seed; one posted while another is in flight runs after it.
    Q_INVOKABLE void reload();
    //! Opens the configuration file with the platform's handler; false, with
    //! lastError set, when nothing could open it. False without a path probe.
    Q_INVOKABLE bool openConfigFile();

    int marker() const { return m_marker; }

    bool startAtStartup() const;
    bool startAtStartupMinimized() const;
    bool disableUpdateCheck() const;
    QString reserveBalance() const { return m_reserve_text; }
    bool enableStaking() const;
    bool enableStakeSplit() const;
    bool enableSideStaking() const;
    bool contractChangeToInput() const;
    QString stakingEfficiency() const;
    QString minStakeSplitValue() const;
    bool mapPortUpnp() const;
    bool proxyUse() const;
    QString proxyIp() const;
    int proxyPort() const;
    bool minimizeToTray() const;
    bool minimizeOnClose() const;
    bool confirmOnClose() const;
    bool disableTrxNotifications() const;
    bool disablePollNotifications() const;
    int displayUnit() const;
    QString language() const;
    QString theme() const;
    bool displayAddresses() const;
    bool coinControlFeatures() const;

    void setStartAtStartup(bool value);
    void setStartAtStartupMinimized(bool value);
    void setDisableUpdateCheck(bool value);
    void setReserveBalance(const QString& text);
    void setEnableStaking(bool value);
    void setEnableStakeSplit(bool value);
    void setEnableSideStaking(bool value);
    void setContractChangeToInput(bool value);
    void setStakingEfficiency(const QString& value);
    void setMinStakeSplitValue(const QString& value);
    void setMapPortUpnp(bool value);
    void setProxyUse(bool value);
    void setProxyIp(const QString& value);
    void setProxyPort(int value);
    void setMinimizeToTray(bool value);
    void setMinimizeOnClose(bool value);
    void setConfirmOnClose(bool value);
    void setDisableTrxNotifications(bool value);
    void setDisablePollNotifications(bool value);
    void setDisplayUnit(int value);
    void setLanguage(const QString& value);
    void setTheme(const QString& value);
    void setDisplayAddresses(bool value);
    void setCoinControlFeatures(bool value);

    bool loaded() const { return m_loaded; }
    int seedGeneration() const { return m_seed_generation; }
    bool dirty() const;
    bool restartRequired() const { return m_restart_required; }
    QString lastError() const { return m_last_error; }
    QString appliedTheme() const { return m_applied_theme; }
    bool trayAvailable() const;
    bool startupOptionsAvailable() const { return false; }
    bool updateCheckOptionAvailable() const;
    bool upnpAvailable() const;
    bool confirmOnCloseAvailable() const;
    bool hostPathsAvailable() const;
    QVariantList languages() const;
    QVariantList displayUnits() const;
    QObject* sideStakes() const;
    //! The sidestake list, typed.
    SideStakeListModel& sideStakeList() { return *m_side_stakes; }

signals:
    //! Any staged property, or dirty, changed.
    void changed();
    void loadedChanged();
    void seedGenerationChanged();
    void restartRequiredChanged();
    void lastErrorChanged();
    void appliedThemeChanged();

private:
    struct SeedResult;

    //! Stages `value` for the options model row `row`; equal to the staged
    //! value, it does nothing.
    void stage(int row, const QVariant& value);
    QVariant staged(int row) const;
    //! The value apply() submits for `row`, from its staged value.
    QVariant submitted(int row) const;
    //! Whether `row`'s staged value differs from its seed, as apply() submits.
    bool differsFromSeed(int row) const;
    //! The refusal text for `row`'s staged value, or empty when it is valid.
    QString validate(int row) const;
    //! Copies the seed into every staged property, the reserve included.
    void copySeed();
    void postSeed();
    void deliverSeed(uint64_t epoch, const SeedResult& result);
    void onRwSettingsUpdated();
    void pushWindowOptions();
    void setAppliedTheme(const QString& theme);
    void setLastError(const QString& text);
    void setLoaded(bool loaded);
    //! Re-formats the reserve text from the staged satoshis in the staged unit.
    void formatReserve();

    int m_marker;
    ShellAdapter& m_shell;
    const HostLocalPaths* m_host_paths{nullptr};
    std::unique_ptr<SideStakeListModel> m_side_stakes;

    interfaces::Node* m_node{nullptr};
    OptionsModel* m_options{nullptr};
    QmlCallRunner* m_runner{nullptr};
    std::unique_ptr<NotificationLifetime> m_lifetime;
    std::unique_ptr<interfaces::Handler> m_rw_settings_handler;
    //! Moved by every attach() and detach(); a delivery of an earlier epoch
    //! does nothing.
    uint64_t m_epoch{0};

    //! Indexed by OptionsModel::OptionID; the reserve is held apart.
    std::vector<QVariant> m_staged;
    std::vector<QVariant> m_seed;
    QSet<int> m_touched;
    qint64 m_reserve_sat{0};
    qint64 m_seed_reserve_sat{0};
    //! False while the reserve text does not parse.
    bool m_reserve_valid{true};
    QString m_reserve_text;

    bool m_seed_in_flight{false};
    bool m_seed_pending{false};
    bool m_loaded{false};
    int m_seed_generation{0};
    bool m_restart_required{false};
    QString m_last_error;
    QString m_applied_theme;
};

static_assert(!std::is_default_constructible_v<SettingsAdapter>,
              "Qt 6.4 prefers new T over create() for a default-constructible singleton");

#endif // BITCOIN_QT_QUICK_SETTINGSADAPTER_H
