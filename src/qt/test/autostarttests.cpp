// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/autostarttests.h"

#include "chainparams.h"
#include "qt/guifrontend.h"
#include "qt/guiutil.h"
#include "qt/optionsmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/widgetsfrontend.h"
#include "util.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>

#include <string>

//!
//! \file autostarttests.cpp
//! \brief The OS login item: a regtest GUI neither creates nor rewrites it,
//! and at start-up only the front end that owns it
//! (GuiFrontEnd::ownsLoginItem) rewrites it.
//!
//! The login item is named and argued for mainnet or testnet only, and
//! everything that is not testnet falls through to the mainnet form. On
//! regtest, GUIUtil::SetStartOnSystemStartup/GetStartOnSystemStartup must
//! therefore do nothing at all: no entry of their own, and no change to the
//! mainnet entry that a regtest launch would otherwise rewrite at startup.
//!
//! The cases drive those public entry points, and RegenerateLoginItem, end to
//! end, so they run on Linux only, where the entry is a .desktop file under
//! $XDG_CONFIG_HOME/autostart and init() points that at a temporary
//! directory. The Windows .lnk path is not exercised here.
//!
//! The Qt test main selects no chain, so each case selects its own with
//! SelectParams(). A selection cannot be undone; that is harmless because no
//! other class in this binary reads Params().
//!

#ifdef Q_OS_LINUX
namespace {
//! Point the datadir at \p datadir and select \p chain, as the GUI's own
//! start-up does before it regenerates the login item.
void UseChain(const std::string& chain, const QString& datadir)
{
    gArgs.ForceSetArg("-datadir", datadir.toStdString());
    gArgs.ClearPathCache();
    SelectParams(chain);
}

QString MakeDataDir(const QTemporaryDir& root, const QString& name)
{
    const QString path = root.filePath(name);
    QDir().mkpath(path);
    return path;
}

QStringList AutostartEntries(const QTemporaryDir& config_home)
{
    return QDir(config_home.filePath("autostart")).entryList(QDir::Files, QDir::Name);
}

QByteArray ReadEntry(const QTemporaryDir& config_home, const QString& name)
{
    QFile file(config_home.filePath("autostart/" + name));
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

bool WriteAutostartEntry(const QTemporaryDir& config_home, const QString& name, const QByteArray& content)
{
    QDir().mkpath(config_home.filePath("autostart"));
    QFile file(config_home.filePath("autostart/" + name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(content) == content.size();
}

//! The entry's Exec= line without its line break, or empty if it has none.
QByteArray ExecLine(const QByteArray& entry)
{
    for (const QByteArray& line : entry.split('\n')) {
        if (line.startsWith("Exec=")) return line;
    }
    return QByteArray();
}

//! Stores the two flat start-up keys in the default QSettings store directly
//! (OptionsModel::setData would also write the item), reads them into a fresh
//! OptionsModel as start-up does, and runs the start-up regeneration for
//! \p frontend.
void RegenerateWith(const GuiFrontEnd& frontend, bool start_at_startup, bool start_min)
{
    {
        QSettings settings;
        settings.setValue("fStartAtStartup", start_at_startup);
        settings.setValue("fStartMin", start_min);
    }
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    OptionsModel model(node, sidestakes);
    model.readNodeSettings();
    RegenerateLoginItem(frontend, model);
}

//! A front end that keeps every default of the seam: it does not override
//! ownsLoginItem. Its other overrides are trivial.
class NonOwningFrontEnd : public GuiFrontEnd
{
public:
    QObject* construct() override { return nullptr; }
    void destroyMain() override {}
    QObject* createSplash() override { return nullptr; }
    void finishSplash() override {}
    void destroySplash() override {}
    void showBuildMismatchWarning(const QString&, const QString&) override {}
    void setIpcConnectionInfo(const GuiIpcInfo&) override {}
    void attachModels(const ModelBundle&) override {}
    void showMain(bool) override {}
    WId nativeWindowId() override { return 0; }
    void requestQuit() override {}

protected:
    void hideMain() override {}
    void detachClient() override {}
    void detachWallet() override {}
    void detachMRC() override {}
    void detachResearcher() override {}
    void detachVoting() override {}
    void detachPSGT() override {}
};
} // anonymous namespace
#endif // Q_OS_LINUX

AutoStartTests::AutoStartTests() = default;
AutoStartTests::~AutoStartTests() = default;

void AutoStartTests::initTestCase()
{
    // The start-up cases read the two flat start-up keys through OptionsModel,
    // which opens a default-constructed QSettings. That store is isolated here,
    // as OptionsModelTests isolates it: the organization and application names,
    // the IniFormat default and the IniFormat user path under a temporary
    // directory.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("autostarttests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());
}

void AutoStartTests::cleanupTestCase()
{
    // The IniFormat user path keeps pointing at the removed temporary directory:
    // Qt has no getter to restore the previous one. A later class that opens an
    // IniFormat QSettings must set its own path first, as
    // OptionsModelTests::initTestCase does.
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void AutoStartTests::init()
{
    m_config_home = std::make_unique<QTemporaryDir>();
    m_data_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_config_home->isValid());
    QVERIFY(m_data_root->isValid());

    m_had_config_home = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
    m_saved_config_home = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_config_home->path()));
}

void AutoStartTests::cleanup()
{
    QSettings().clear();
    gArgs.ForceSetArg("-datadir", "");
    gArgs.ClearPathCache();

    if (m_had_config_home) {
        qputenv("XDG_CONFIG_HOME", m_saved_config_home);
    } else {
        qunsetenv("XDG_CONFIG_HOME");
    }

    m_config_home.reset();
    m_data_root.reset();
}

void AutoStartTests::regtestCreatesNoLoginItem()
{
#ifndef Q_OS_LINUX
    QSKIP("The login item is checked end to end on Linux only.");
#else
    UseChain(CBaseChainParams::REGTEST, MakeDataDir(*m_data_root, "regtest"));

    // Enabling autostart on regtest must write nothing, and must not report
    // itself as enabled. The result is checked after the listing, so a failure
    // shows what was written.
    const bool set_on = GUIUtil::SetStartOnSystemStartup(true, true);

    const QStringList written = AutostartEntries(*m_config_home);
    QVERIFY2(written.isEmpty(), qPrintable("regtest wrote a login item: " + written.join(", ")));
    QVERIFY(!set_on);
    QVERIFY(!GUIUtil::GetStartOnSystemStartup());

    // Control: the same call on mainnet writes into the redirected directory,
    // so the empty listing above is not a redirect that never took effect.
    UseChain(CBaseChainParams::MAIN, MakeDataDir(*m_data_root, "main"));
    QVERIFY(GUIUtil::SetStartOnSystemStartup(true, true));
    QCOMPARE(AutostartEntries(*m_config_home), QStringList{"gridcoin-mainnet.desktop"});
    QVERIFY(GUIUtil::GetStartOnSystemStartup());
#endif
}

void AutoStartTests::regtestLeavesTheMainnetLoginItemAlone()
{
#ifndef Q_OS_LINUX
    QSKIP("The login item is checked end to end on Linux only.");
#else
    // A mainnet user with autostart on. The entry names the mainnet datadir.
    const QString main_datadir = MakeDataDir(*m_data_root, "main");
    UseChain(CBaseChainParams::MAIN, main_datadir);
    QVERIFY(GUIUtil::SetStartOnSystemStartup(true, true));

    const QByteArray mainnet_entry = ReadEntry(*m_config_home, "gridcoin-mainnet.desktop");
    QVERIFY(mainnet_entry.contains(QFile::encodeName(main_datadir)));

    // The same user starts a regtest GUI on another datadir. The autostart
    // setting is shared with mainnet, so its start-up calls this with true.
    UseChain(CBaseChainParams::REGTEST, MakeDataDir(*m_data_root, "regtest"));
    const bool set_on = GUIUtil::SetStartOnSystemStartup(true, true);
    QCOMPARE(ReadEntry(*m_config_home, "gridcoin-mainnet.desktop"), mainnet_entry);
    QVERIFY(!set_on);

    // Turning autostart off from the regtest GUI must not remove it either.
    const bool set_off = GUIUtil::SetStartOnSystemStartup(false, true);
    QCOMPARE(ReadEntry(*m_config_home, "gridcoin-mainnet.desktop"), mainnet_entry);
    QVERIFY(!set_off);

    QCOMPARE(AutostartEntries(*m_config_home), QStringList{"gridcoin-mainnet.desktop"});
    QVERIFY(!GUIUtil::GetStartOnSystemStartup());
#endif
}

void AutoStartTests::widgetsFrontEndRegeneratesTheLoginItem()
{
#ifndef Q_OS_LINUX
    QSKIP("The login item is checked end to end on Linux only.");
#else
    const QString datadir = MakeDataDir(*m_data_root, "main");
    UseChain(CBaseChainParams::MAIN, datadir);
    const std::unique_ptr<GuiFrontEnd> widgets = MakeWidgetsFrontEnd();
    const QStringList mainnet_only{"gridcoin-mainnet.desktop"};

    // On and minimized: the item starts this binary on this datadir, with -min.
    RegenerateWith(*widgets, true, true);
    QVERIFY2(AutostartEntries(*m_config_home) == mainnet_only, "the Widgets front end wrote no login item");
    const QByteArray minimized = ReadEntry(*m_config_home, "gridcoin-mainnet.desktop");
    QVERIFY2(minimized.contains(QFile::encodeName(datadir)), "the login item lost its datadir");
    QVERIFY2(minimized.contains(" -min"), "the login item lacks -min");

    // On, not minimized: the item stays and loses -min.
    RegenerateWith(*widgets, true, false);
    QVERIFY2(AutostartEntries(*m_config_home) == mainnet_only,
             "the Widgets front end removed the login item when only -min changed");
    const QByteArray exec_line = ExecLine(ReadEntry(*m_config_home, "gridcoin-mainnet.desktop"));
    QVERIFY2(exec_line.startsWith("Exec=") && !exec_line.trimmed().endsWith(" -min"), "the login item kept -min");

    // Off: the item is removed.
    RegenerateWith(*widgets, false, true);
    QVERIFY2(AutostartEntries(*m_config_home).isEmpty(),
             "the Widgets front end left the login item when the setting is off");
#endif
}

void AutoStartTests::nonOwningFrontEndLeavesTheLoginItemAlone()
{
#ifndef Q_OS_LINUX
    QSKIP("The login item is checked end to end on Linux only.");
#else
    const QString datadir = MakeDataDir(*m_data_root, "main");
    UseChain(CBaseChainParams::MAIN, datadir);

    // The Widgets wallet's item and a legacy entry, written by hand.
    const QByteArray widgets_entry = "[Desktop Entry]\n"
                                     "Type=Application\n"
                                     "Name=Gridcoin-mainnet\n"
                                     "Exec=/opt/widgets/gridcoinresearch -min\n"
                                     "Terminal=false\n"
                                     "Hidden=false\n";
    const QByteArray legacy_entry = "[Desktop Entry]\n"
                                    "Type=Application\n"
                                    "Name=Gridcoin\n"
                                    "Exec=/opt/widgets/gridcoinresearch\n";
    QVERIFY2(WriteAutostartEntry(*m_config_home, "gridcoin-mainnet.desktop", widgets_entry),
             "the hand-written login item was not written");
    QVERIFY2(WriteAutostartEntry(*m_config_home, "gridcoin.desktop", legacy_entry),
             "the hand-written legacy entry was not written");
    const QStringList mainnet_only{"gridcoin-mainnet.desktop"};
    NonOwningFrontEnd non_owning;

    // On and minimized: nothing is rewritten, and the legacy entry stays.
    RegenerateWith(non_owning, true, true);
    QVERIFY2(ReadEntry(*m_config_home, "gridcoin-mainnet.desktop") == widgets_entry,
             "a non-owning front end rewrote the login item");
    QVERIFY2(ReadEntry(*m_config_home, "gridcoin.desktop") == legacy_entry,
             "a non-owning front end removed the legacy entry");

    // Off: nothing is removed.
    RegenerateWith(non_owning, false, true);
    QVERIFY2(AutostartEntries(*m_config_home).size() == 2
                 && ReadEntry(*m_config_home, "gridcoin-mainnet.desktop") == widgets_entry
                 && ReadEntry(*m_config_home, "gridcoin.desktop") == legacy_entry,
             "a non-owning front end removed the login item");

    // Control: the owner, seeded on and minimized in the same redirected
    // directories, rewrites the item and removes the legacy entry, so the
    // compares above are not vacuous.
    const std::unique_ptr<GuiFrontEnd> widgets = MakeWidgetsFrontEnd();
    RegenerateWith(*widgets, true, true);
    QVERIFY2(!QFile::exists(m_config_home->filePath("autostart/gridcoin.desktop")),
             "the control rewrite did not reach the redirected directory");
    QVERIFY2(AutostartEntries(*m_config_home) == mainnet_only, "the control did not read its seed");
    const QByteArray rewritten = ReadEntry(*m_config_home, "gridcoin-mainnet.desktop");
    QVERIFY2(!rewritten.contains("/opt/widgets/gridcoinresearch") && rewritten.contains(QFile::encodeName(datadir)),
             "the control rewrite did not reach the redirected directory");
    QVERIFY2(rewritten.contains(" -min"), "the control rewrite lacks -min");
#endif
}
