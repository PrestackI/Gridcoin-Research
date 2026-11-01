// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_MODELHARNESS_H
#define BITCOIN_QT_QUICK_TEST_MODELHARNESS_H

#include "qmltestutil.h"

#include <interfaces/node.h>
#include <interfaces/wallet_coin_source.h>
#include <qt/clientmodel.h>
#include <qt/guifrontend.h>
#include <qt/mrcmodel.h>
#include <qt/optionsmodel.h>
#include <qt/researcher/researchermodel.h>
#include <qt/test/interfacefakes.h>
#include <qt/test/modelfakes.h>
#include <qt/voting/votingmodel.h>
#include <qt/walletmodel.h>
#include <util.h>

#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace qml_test {

//! The sidestake fake with its edit commands counted and its settings
//! notification kept, so a test can tell whether a command reached it.
class RecordingSideStakeManager : public qt_test::FakeSideStakeManager
{
public:
    int m_set_allocation_calls{0};
    int m_set_description_calls{0};
    //! The last callback handleRwSettingsUpdated was given.
    interfaces::RwSettingsUpdatedFn m_rw_settings_fn;

    interfaces::SideStakeEditResult setAllocation(const std::string& address, double allocation_percent) override
    {
        ++m_set_allocation_calls;
        return FakeSideStakeManager::setAllocation(address, allocation_percent);
    }
    interfaces::SideStakeEditResult setDescription(const std::string& address,
                                                   const std::string& description) override
    {
        ++m_set_description_calls;
        return FakeSideStakeManager::setDescription(address, description);
    }
    std::unique_ptr<interfaces::Handler> handleRwSettingsUpdated(interfaces::RwSettingsUpdatedFn fn) override
    {
        m_rw_settings_fn = std::move(fn);
        return interfaces::MakeCleanupHandler([] {});
    }
};

//! Fake interfaces::Node for the status adapters: settable values for every
//! read they make. getWarnings, getScraperConvergenceSnapshot and
//! checkForLatestUpdate can each be held at their start on a Latch, and record
//! their calling thread and count their calls; getSettingBool counts its
//! "disableupdatecheck" reads. The settings reads and writes take the lock,
//! because the settings adapter reads them on the serial lane; a test reads
//! the recorded batches through changeCalls(). handleRwSettingsUpdated keeps
//! its callback, which fireRwSettingsUpdated() calls.
class StatusFakeNode : public qt_test::FakeNode
{
public:
    std::atomic<int> m_node_count{0};
    std::atomic<int64_t> m_last_block_time{0};
    std::atomic<bool> m_testnet{false};
    std::atomic<bool> m_mainnet{false};
    std::atomic<double> m_difficulty{0.0};

    // Optional latches, waited on at the start of the call when set.
    Latch* m_warnings_latch{nullptr};
    Latch* m_scraper_latch{nullptr};
    Latch* m_update_check_latch{nullptr};

    // Recorded calls.
    std::atomic<int> m_warnings_calls{0};
    std::atomic<std::thread::id> m_warnings_thread{};
    std::atomic<int> m_scraper_calls{0};
    std::atomic<std::thread::id> m_scraper_thread{};
    std::atomic<int> m_update_check_calls{0};
    std::atomic<int> m_disable_update_check_reads{0};

    void setWarnings(std::string warnings)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_warnings = std::move(warnings);
    }
    void setClientVersion(std::string version)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_client_version = std::move(version);
    }
    void setScraperSnapshot(interfaces::ScraperConvergenceSnapshot snapshot)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scraper_snapshot = std::move(snapshot);
    }
    void setLatestVersion(interfaces::LatestVersionInfo info)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_latest_version = std::move(info);
    }
    //! The thread of the last checkForLatestUpdate call.
    std::thread::id updateCheckThread() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_update_check_thread;
    }

    int getNodeCount() override { return m_node_count; }
    int64_t getLastBlockTime() override { return m_last_block_time; }
    double getDifficulty() override { return m_difficulty; }
    bool isTestNet() override { return m_testnet; }
    bool isMainNet() override { return m_mainnet; }
    //! The core's rule: out of sync once the tip is 900 s old.
    bool isOutOfSyncByAge() override
    {
        const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
        return now - m_last_block_time >= 900;
    }
    std::string getClientVersion() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_client_version;
    }
    bool getSettingBool(const std::string& name, bool default_val) override
    {
        if (name == "disableupdatecheck") ++m_disable_update_check_reads;
        std::lock_guard<std::mutex> lock(m_mutex);
        return FakeNode::getSettingBool(name, default_val);
    }
    int64_t getSettingInt(const std::string& name, int64_t default_val) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return FakeNode::getSettingInt(name, default_val);
    }
    std::string getSettingStr(const std::string& name, const std::string& default_val) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return FakeNode::getSettingStr(name, default_val);
    }
    interfaces::SettingChangeResult changeSettings(
        const std::vector<std::pair<std::string, std::string>>& settings) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return FakeNode::changeSettings(settings);
    }
    //! Sets one stored setting, as another writer would.
    void setSetting(const std::string& name, const std::string& value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_settings[name] = value;
    }
    //! A copy of the recorded changeSettings batches.
    std::vector<std::vector<std::pair<std::string, std::string>>> changeCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_change_calls;
    }
    std::unique_ptr<interfaces::Handler> handleRwSettingsUpdated(RwSettingsUpdatedFn fn) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rw_settings_fn = std::move(fn);
        return interfaces::MakeCleanupHandler([] {});
    }
    //! Calls the stored settings callback, as the node does after a write;
    //! does nothing when none is stored.
    void fireRwSettingsUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_rw_settings_fn) m_rw_settings_fn();
    }
    std::string getWarnings() override
    {
        if (m_warnings_latch) m_warnings_latch->wait();
        m_warnings_thread = std::this_thread::get_id();
        ++m_warnings_calls;
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_warnings;
    }
    interfaces::ScraperConvergenceSnapshot getScraperConvergenceSnapshot() override
    {
        if (m_scraper_latch) m_scraper_latch->wait();
        m_scraper_thread = std::this_thread::get_id();
        ++m_scraper_calls;
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_scraper_snapshot;
    }
    interfaces::LatestVersionInfo checkForLatestUpdate() override
    {
        if (m_update_check_latch) m_update_check_latch->wait();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_update_check_thread = std::this_thread::get_id();
        ++m_update_check_calls;
        return m_latest_version;
    }

private:
    mutable std::mutex m_mutex;
    std::string m_warnings;
    std::string m_client_version;
    interfaces::ScraperConvergenceSnapshot m_scraper_snapshot;
    interfaces::LatestVersionInfo m_latest_version;
    std::thread::id m_update_check_thread{};
    RwSettingsUpdatedFn m_rw_settings_fn;
};

//! How many ModelHarnessIsolation objects are alive (at most one).
inline int g_live_harnesses = 0;

//! Points the settings and the datadir at temporary directories for the life
//! of a harness, so OptionsModel and GetDataDir never reach the user's own. It
//! is the harness's first member: it runs before OptionsModel::Init opens
//! QSettings, and is undone after every model is gone. At most one is alive:
//! QSettings::setPath has no getter, so an inner harness could not put back an
//! outer harness's path.
class ModelHarnessIsolation
{
public:
    ModelHarnessIsolation()
    {
        // Every check runs before any setting changes.
        if (g_live_harnesses > 0) {
            throw std::logic_error("ModelHarnessIsolation: another model harness is already alive");
        }
        if (QCoreApplication::organizationName() == QLatin1String("Gridcoin") ||
            QCoreApplication::applicationName().startsWith(QLatin1String("Gridcoin-Qt"))) {
            throw std::logic_error("ModelHarnessIsolation: the application names are Gridcoin's own; "
                                   "a harness would replace the user's settings path");
        }
        if (!m_settings_root.isValid() || !m_data_dir.isValid()) {
            // An empty -datadir would resolve to the default datadir.
            throw std::runtime_error("ModelHarnessIsolation: a temporary directory could not be created");
        }

        m_previous_organization = QCoreApplication::organizationName();
        m_previous_application = QCoreApplication::applicationName();
        m_previous_format = QSettings::defaultFormat();
        m_previous_datadir_set = gArgs.IsArgSet("-datadir");
        m_previous_datadir = gArgs.GetArg("-datadir", "");

        QCoreApplication::setOrganizationName(QStringLiteral("GridcoinTest"));
        QCoreApplication::setApplicationName(QStringLiteral("modelharness"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root.path());
        gArgs.ForceSetArg("-datadir", m_data_dir.path().toStdString());
        gArgs.ClearPathCache();
        ++g_live_harnesses;
    }

    ~ModelHarnessIsolation()
    {
        QSettings().clear();
        if (m_previous_datadir_set) {
            gArgs.ForceSetArg("-datadir", m_previous_datadir);
        } else {
            gArgs.ClearForcedArg("-datadir");
        }
        gArgs.ClearPathCache();
        QCoreApplication::setOrganizationName(m_previous_organization);
        QCoreApplication::setApplicationName(m_previous_application);
        QSettings::setDefaultFormat(m_previous_format);
        --g_live_harnesses;
    }

    ModelHarnessIsolation(const ModelHarnessIsolation&) = delete;
    ModelHarnessIsolation& operator=(const ModelHarnessIsolation&) = delete;

    //! The directory the settings file is written under.
    QString settingsRoot() const { return m_settings_root.path(); }
    //! The directory -datadir points at.
    QString dataDirRoot() const { return m_data_dir.path(); }

private:
    QTemporaryDir m_settings_root;
    QTemporaryDir m_data_dir;
    QString m_previous_organization;
    QString m_previous_application;
    QSettings::Format m_previous_format{QSettings::NativeFormat};
    bool m_previous_datadir_set{false};
    std::string m_previous_datadir;
};

//! The five GUI models GuiMain builds, over fakes, in GuiMain's order, with
//! the settings and datadir isolated. Members are destroyed in reverse order:
//! the models first, then the fakes, then the isolation.
class ModelHarness
{
public:
    //! `wallet` stands behind the real WalletModel, so a test can pass a
    //! derived fake.
    explicit ModelHarness(std::unique_ptr<qt_test::FakeWallet> wallet = std::make_unique<qt_test::FakeWallet>())
        : wallet_fake(std::move(wallet))
    {
    }

    ModelHarness(const ModelHarness&) = delete;
    ModelHarness& operator=(const ModelHarness&) = delete;

    qt_test::FakeWallet& wallet() { return *wallet_fake; }

    //! The models as QmlFrontEnd::attachModels takes them.
    ModelBundle bundle() { return ModelBundle{client, wallet_model, mrc, researcher, voting, nullptr}; }

    ModelHarnessIsolation isolation;
    StatusFakeNode node;
    RecordingSideStakeManager side_stakes;
    OptionsModel options{node, side_stakes};
    qt_test::FakeStakingStatus staking;
    std::unique_ptr<qt_test::FakeWallet> wallet_fake;
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source{interfaces::MakeSyntheticCoinSource(0, 1)};
    qt_test::FakeResearcherContext researcher_context;
    qt_test::FakeVotingManager voting_manager;
    qt_test::FakeMRC mrc_fake;
    ClientModel client{node, staking, &options};
    WalletModel wallet_model{*wallet_fake, tx_source, *coin_source, &options};
    ResearcherModel researcher{researcher_context};
    MRCModel mrc{mrc_fake, &wallet_model, &client, &researcher};
    VotingModel voting{voting_manager, researcher_context, client, options, wallet_model};
};

} // namespace qml_test

#endif // BITCOIN_QT_QUICK_TEST_MODELHARNESS_H
