// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "settingsadaptertests.h"

#include "interfacefakes_qml.h"
#include "modelharness.h"
#include "qmltestutil.h"

#include <amount.h>
#include <interfaces/sidestake.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/settingsadapter.h>
#include <qt/quick/shelladapter.h>
#include <qt/quick/sidestakelistmodel.h>
#include <qt/sidestaketablemodel.h>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

//! The language list reads the translations in the bitcoin_locale resource,
//! which GuiMain registers before it builds a front end; this binary does not
//! run GuiMain. Outside any namespace: Q_INIT_RESOURCE declares the resource's
//! initialiser where it is used.
static void InitLocaleResources()
{
    Q_INIT_RESOURCE(bitcoin_locale);
}

namespace {

using namespace std::chrono_literals;

//! The root the front ends are built with; no case here constructs one.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

//! An address the sidestake fake accepts unchecked.
const QString ADDRESS = QStringLiteral("mfiy9sc2QEZZCK3WMUMZjNfrdRA6gXzRhr");

using Batches = std::vector<std::vector<std::pair<std::string, std::string>>>;

//! The recorded changeSettings batches as text, for a readable comparison.
QString Text(const Batches& batches)
{
    QStringList out;
    for (const auto& batch : batches) {
        QStringList pairs;
        for (const auto& [name, value] : batch) {
            pairs << QStringLiteral("(\"%1\",\"%2\")").arg(QString::fromStdString(name), QString::fromStdString(value));
        }
        out << QStringLiteral("[") + pairs.join(QLatin1Char(',')) + QStringLiteral("]");
    }
    return QStringLiteral("[") + out.join(QStringLiteral(", ")) + QStringLiteral("]");
}

//! Pumps until a seed newer than `before` has been delivered.
bool SeedAfter(SettingsAdapter& settings, int before)
{
    return qml_test::PumpUntil([&settings, before] { return settings.seedGeneration() > before; }, 2s);
}

//! A sidestake row as the node hands it over.
interfaces::SideStakeEntry Entry(const std::string& address, double allocation, bool mandatory)
{
    interfaces::SideStakeEntry entry;
    entry.address = address;
    entry.allocation_percent = allocation;
    entry.description = "row";
    entry.status = mandatory ? "Mandatory" : "Active";
    entry.is_mandatory = mandatory;
    return entry;
}

QString EfficiencyText()
{
    return QCoreApplication::translate("OptionsDialog", "The supplied target staking efficiency is invalid.");
}

QString MinSplitText()
{
    return QCoreApplication::translate("OptionsDialog", "The supplied minimum post stake-split UTXO size is invalid.");
}

QString NotSavedText()
{
    return QCoreApplication::translate("OptionsDialog", "A setting could not be saved.");
}

} // namespace

void QmlSettingsAdapterTests::initTestCase()
{
    QCOMPARE(qml_test::g_live_harnesses, 0);
}

void QmlSettingsAdapterTests::discardCallsNothing()
{
    qml_test::ModelHarness harness;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SettingsAdapter& settings = fe.provider().settings();
    QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

    const QString efficiency = settings.stakingEfficiency();
    const QString reserve = settings.reserveBalance();
    const bool upnp = settings.mapPortUpnp();
    const QString proxy_ip = settings.proxyIp();

    settings.setStakingEfficiency(QStringLiteral("80"));
    settings.setReserveBalance(QStringLiteral("5"));
    settings.setMapPortUpnp(!upnp);
    settings.setProxyIp(QStringLiteral("127.0.0.2"));
    QVERIFY(settings.dirty());
    settings.discard();
    const int before = settings.seedGeneration();
    const bool applied = settings.apply();
    QCOMPARE(harness.node.changeCalls().size(), size_t{0});
    QVERIFY(applied);
    QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");

    QCOMPARE(harness.node.changeCalls().size(), size_t{0});
    QCOMPARE(settings.stakingEfficiency(), efficiency);
    QCOMPARE(settings.reserveBalance(), reserve);
    QCOMPARE(settings.mapPortUpnp(), upnp);
    QCOMPARE(settings.proxyIp(), proxy_ip);
    QVERIFY(!settings.dirty());
}

void QmlSettingsAdapterTests::applySubmitsOnlyTouchedInOrder()
{
    qml_test::ModelHarness harness;
    harness.node.setSetting("enablestakesplit", "1");
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SettingsAdapter& settings = fe.provider().settings();
    QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

    QSignalSpy unit_spy(&harness.options, &OptionsModel::displayUnitChanged);
    QSignalSpy style_spy(&harness.options, &OptionsModel::walletStylesheetChanged);
    settings.setStakingEfficiency(QStringLiteral("80"));
    settings.setMinStakeSplitValue(QStringLiteral("900"));
    const int before = settings.seedGeneration();
    QVERIFY(settings.apply());
    QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");

    // Untouched rows are not submitted: neither GUI-side row signals.
    QCOMPARE(unit_spy.count(), 0);
    QCOMPARE(style_spy.count(), 0);
    QCOMPARE(Text(harness.node.changeCalls()),
             QStringLiteral("[[(\"stakingefficiency\",\"80\")], [(\"minstakesplitvalue\",\"900\")]]"));
    QVERIFY(!settings.dirty());
}

void QmlSettingsAdapterTests::efficiencyWholeNumberInRange()
{
    qml_test::ModelHarness harness;
    harness.node.setSetting("enablestakesplit", "1");
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SettingsAdapter& settings = fe.provider().settings();
    QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

    for (const QString& refused : {QStringLiteral("74"), QStringLiteral("99"), QStringLiteral("80.5"),
                                   QStringLiteral("abc"), QString()}) {
        settings.setStakingEfficiency(refused);
        const bool applied = settings.apply();
        QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
        QVERIFY2(!applied, qPrintable(refused));
        QCOMPARE(settings.lastError(), EfficiencyText());
    }

    // Surrounding spaces and a leading zero are read as the whole number.
    settings.setStakingEfficiency(QStringLiteral(" 080 "));
    int before = settings.seedGeneration();
    QVERIFY(settings.apply());
    QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
    QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[[(\"stakingefficiency\",\"80\")]]"));
    QCOMPARE(settings.stakingEfficiency(), QStringLiteral("80"));
    QVERIFY(!settings.dirty());

    // The bounds are accepted.
    for (const QString& accepted : {QStringLiteral("75"), QStringLiteral("98")}) {
        settings.setStakingEfficiency(accepted);
        before = settings.seedGeneration();
        QVERIFY2(settings.apply(), qPrintable(accepted));
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
    }
    QCOMPARE(Text(harness.node.changeCalls()),
             QStringLiteral("[[(\"stakingefficiency\",\"80\")], [(\"stakingefficiency\",\"75\")], "
                            "[(\"stakingefficiency\",\"98\")]]"));
}

void QmlSettingsAdapterTests::hiddenSplitFieldsNotSubmitted()
{
    qml_test::ModelHarness harness;
    harness.node.setSetting("enablestakesplit", "0");
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SettingsAdapter& settings = fe.provider().settings();
    QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
    const QString seed = settings.stakingEfficiency();

    // With stake splitting off the field is hidden: an invalid value there
    // neither blocks the apply nor reaches the node.
    settings.setStakingEfficiency(QStringLiteral("74"));
    const int before = settings.seedGeneration();
    QVERIFY(settings.apply());
    QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
    QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
    QCOMPARE(settings.stakingEfficiency(), seed);
}

void QmlSettingsAdapterTests::proxyFieldsIgnoredWhileUnticked()
{
    qml_test::ModelHarness harness;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SettingsAdapter& settings = fe.provider().settings();
    QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
    QVERIFY(!settings.proxyUse());
    QCOMPARE(settings.proxyIp(), QStringLiteral("127.0.0.1"));
    QCOMPARE(settings.proxyPort(), 9050);

    // With the proxy off its fields are disabled: values staged there
    // neither block the apply nor reach the node, and go back to the seed.
    settings.setProxyPort(0);
    settings.setProxyIp(QStringLiteral("x"));
    const int before = settings.seedGeneration();
    QVERIFY(settings.apply());
    QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
    QCOMPARE(settings.proxyIp(), QStringLiteral("127.0.0.1"));
    QCOMPARE(settings.proxyPort(), 9050);
    QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
    QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
    QCOMPARE(settings.proxyIp(), QStringLiteral("127.0.0.1"));
    QCOMPARE(settings.proxyPort(), 9050);
    QVERIFY(!settings.dirty());

    // A refused apply still puts the disabled fields back to the seed.
    settings.setProxyIp(QStringLiteral("x"));
    settings.setReserveBalance(QStringLiteral("-5"));
    QSignalSpy changed(&settings, &SettingsAdapter::changed);
    QVERIFY(!settings.apply());
    QCOMPARE(settings.proxyIp(), QStringLiteral("127.0.0.1"));
    QVERIFY(changed.count() >= 1);
}

void QmlSettingsAdapterTests::minSplitAtLeastFloor()
{
    qml_test::ModelHarness harness;
    harness.node.setSetting("enablestakesplit", "1");
    // Not the floor: OptionsModel::setData drops a value equal to the stored one.
    harness.node.setSetting("minstakesplitvalue", "1000");
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SettingsAdapter& settings = fe.provider().settings();
    QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
    QCOMPARE(settings.minStakeSplitValue(), QStringLiteral("1000"));

    settings.setMinStakeSplitValue(QStringLiteral("799"));
    bool applied = settings.apply();
    QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
    QVERIFY(!applied);
    QCOMPARE(settings.lastError(), MinSplitText());

    settings.setMinStakeSplitValue(QStringLiteral("800"));
    const int before = settings.seedGeneration();
    QVERIFY(settings.apply());
    QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
    QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[[(\"minstakesplitvalue\",\"800\")]]"));

    for (const QString& refused : {QStringLiteral("800.5"), QString::number(MAX_MONEY / COIN + 1)}) {
        settings.setMinStakeSplitValue(refused);
        applied = settings.apply();
        QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[[(\"minstakesplitvalue\",\"800\")]]"));
        QVERIFY2(!applied, qPrintable(refused));
        QCOMPARE(settings.lastError(), MinSplitText());
    }
}

void QmlSettingsAdapterTests::failedRowDoesNotStopApply()
{
    qml_test::ModelHarness harness;
    harness.node.setSetting("enablestakesplit", "1");
    harness.node.m_fail_changes = true;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SettingsAdapter& settings = fe.provider().settings();
    QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
    const bool upnp = settings.mapPortUpnp();
    const QString efficiency = settings.stakingEfficiency();

    settings.setMapPortUpnp(!upnp);
    settings.setStakingEfficiency(QStringLiteral("80"));
    const int before = settings.seedGeneration();
    const bool applied = settings.apply();
    // Both rows were submitted, although the first was refused.
    QCOMPARE(Text(harness.node.changeCalls()),
             QStringLiteral("[[(\"upnp\",\"%1\")], [(\"stakingefficiency\",\"80\")]]")
                 .arg(upnp ? QStringLiteral("0") : QStringLiteral("1")));
    QVERIFY(!applied);
    QCOMPARE(settings.lastError(), NotSavedText());
    QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
    // Nothing stays touched: the values are the ones read back.
    QVERIFY(!settings.dirty());
    QCOMPARE(settings.mapPortUpnp(), upnp);
    QCOMPARE(settings.stakingEfficiency(), efficiency);
}

void QmlSettingsAdapterTests::overHundredSidestakeRefused()
{
    qml_test::ModelHarness harness;
    harness.side_stakes.m_edit_result.status = interfaces::SideStakeEditStatus::INVALID_ALLOCATION;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SideStakeListModel& list = fe.provider().settings().sideStakeList();
    const int rows = list.rowCount();

    const bool added = list.add(ADDRESS, QStringLiteral("60"), QString());
    QVERIFY(!added);
    QCOMPARE(list.lastError(),
             QCoreApplication::translate("EditSideStakeDialog",
                                         "The entered allocation is not valid. Check to make sure that the "
                                         "allocation is greater than zero and when added to the other allocations "
                                         "totals less than 100."));
    QCOMPARE(list.rowCount(), rows);
}

void QmlSettingsAdapterTests::mandatoryRowNotEditable()
{
    qml_test::ModelHarness harness;
    harness.side_stakes.m_snapshot.entries = {Entry("mandatory", 5.0, true), Entry(ADDRESS.toStdString(), 10.0, false)};
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SideStakeListModel& list = fe.provider().settings().sideStakeList();
    QCOMPARE(list.rowCount(), 2);
    QVERIFY(!list.data(list.index(0), SideStakeListModel::EditableRole).toBool());
    QVERIFY(list.data(list.index(1), SideStakeListModel::EditableRole).toBool());

    const QString mandatory_text =
        QCoreApplication::translate("OptionsDialog", "A mandatory sidestake cannot be edited or deleted.");
    const bool edited = list.edit(0, QStringLiteral("5"), QStringLiteral("x"));
    const QString edit_error = list.lastError();
    const bool removed = list.remove(0);
    // Refused before any edit or delete reaches the table.
    QCOMPARE(harness.side_stakes.m_set_allocation_calls, 0);
    QCOMPARE(harness.side_stakes.m_set_description_calls, 0);
    QCOMPARE(QString::fromStdString(harness.side_stakes.m_last_deleted_address), QString());
    QVERIFY(!edited);
    QCOMPARE(edit_error, mandatory_text);
    QVERIFY(!removed);
    QCOMPARE(list.lastError(), mandatory_text);

    // A local row is edited: the allocation, then the description.
    QVERIFY(list.edit(1, QStringLiteral("5"), QStringLiteral("x")));
    QCOMPARE(harness.side_stakes.m_set_allocation_calls, 1);
    QCOMPARE(harness.side_stakes.m_set_description_calls, 1);
}

void QmlSettingsAdapterTests::failedRemoveShowsNoStaleStatus()
{
    qml_test::ModelHarness harness;
    harness.side_stakes.m_snapshot.entries = {Entry("mandatory", 5.0, true), Entry(ADDRESS.toStdString(), 10.0, false)};
    harness.side_stakes.m_edit_result.status = interfaces::SideStakeEditStatus::INVALID_ADDRESS;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SideStakeListModel& list = fe.provider().settings().sideStakeList();
    QCOMPARE(list.rowCount(), 2);

    // A refused add shows the table's text.
    QVERIFY(!list.add(ADDRESS, QStringLiteral("5"), QString()));
    QCOMPARE(list.lastError(),
             QCoreApplication::translate("EditSideStakeDialog",
                                         "The entered address \"%1\" is not a valid Gridcoin address.")
                 .arg(ADDRESS));

    // The same canned result refuses the delete of the local row. The
    // Options dialog shows nothing then, so the add's text is not shown
    // again.
    QVERIFY(!list.remove(1));
    QCOMPARE(QString::fromStdString(harness.side_stakes.m_last_deleted_address), ADDRESS);
    QCOMPARE(list.lastError(), QString());
    QCOMPARE(list.rowCount(), 2);
}

void QmlSettingsAdapterTests::rwSettingsUpdateRefreshesUntouched()
{
    // (1) A setting another writer changed is read again.
    {
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

        harness.node.setSetting("stakingefficiency", "85");
        const int before = settings.seedGeneration();
        harness.node.fireRwSettingsUpdated();
        QVERIFY2(SeedAfter(settings, before), "the settings notification posted no seed");
        QCOMPARE(settings.stakingEfficiency(), QStringLiteral("85"));
    }

    // (2) A touched value stays staged while an untouched one follows the
    // node: the split flag turns on, so the staged efficiency is submitted.
    {
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

        settings.setStakingEfficiency(QStringLiteral("80"));
        harness.node.setSetting("enablestakesplit", "1");
        int before = settings.seedGeneration();
        harness.node.fireRwSettingsUpdated();
        QVERIFY2(SeedAfter(settings, before), "the settings notification posted no seed");
        before = settings.seedGeneration();
        settings.apply();
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[[(\"stakingefficiency\",\"80\")]]"));
    }

    // (3) Writing the value already staged touches nothing, so the property
    // follows the node and is not submitted.
    {
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
        QCOMPARE(settings.stakingEfficiency(), QStringLiteral("90"));

        QSignalSpy changed(&settings, &SettingsAdapter::changed);
        settings.setStakingEfficiency(QStringLiteral("90"));
        QCOMPARE(changed.count(), 0);
        QVERIFY(!settings.dirty());
        harness.node.setSetting("stakingefficiency", "85");
        int before = settings.seedGeneration();
        harness.node.fireRwSettingsUpdated();
        QVERIFY2(SeedAfter(settings, before), "the settings notification posted no seed");
        QCOMPARE(settings.stakingEfficiency(), QStringLiteral("85"));
        const bool upnp = settings.mapPortUpnp();
        settings.setMapPortUpnp(!upnp);
        before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QCOMPARE(Text(harness.node.changeCalls()),
                 QStringLiteral("[[(\"upnp\",\"%1\")]]").arg(upnp ? QStringLiteral("0") : QStringLiteral("1")));
    }

    // (4) A value written away from its seed and back follows the node too.
    {
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
        QCOMPARE(settings.stakingEfficiency(), QStringLiteral("90"));

        settings.setStakingEfficiency(QStringLiteral("80"));
        settings.setStakingEfficiency(QStringLiteral("90"));
        harness.node.setSetting("stakingefficiency", "85");
        int before = settings.seedGeneration();
        harness.node.fireRwSettingsUpdated();
        QVERIFY2(SeedAfter(settings, before), "the settings notification posted no seed");
        const bool upnp = settings.mapPortUpnp();
        settings.setMapPortUpnp(!upnp);
        before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QCOMPARE(Text(harness.node.changeCalls()),
                 QStringLiteral("[[(\"upnp\",\"%1\")]]").arg(upnp ? QStringLiteral("0") : QStringLiteral("1")));
        QCOMPARE(settings.stakingEfficiency(), QStringLiteral("85"));
    }

    // (5) A unit staged and staged back is not submitted.
    {
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
        QCOMPARE(settings.displayUnit(), int{BitcoinUnits::BTC});

        QSignalSpy unit_spy(&harness.options, &OptionsModel::displayUnitChanged);
        settings.setDisplayUnit(BitcoinUnits::mBTC);
        settings.setDisplayUnit(BitcoinUnits::BTC);
        const int before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QCOMPARE(unit_spy.count(), 0);
        QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
    }
}

void QmlSettingsAdapterTests::windowOptionsPushedOnApply()
{
    // (1) The stored window options reach the shell at attach.
    {
        qml_test::ModelHarness harness;
        harness.options.setData(harness.options.index(OptionsModel::MinimizeOnClose), true, Qt::EditRole);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        fe.provider().shell().setTrayAvailableProbe([] { return true; });

        QVERIFY(!fe.provider().shell().requestClose());
        QCOMPARE(fe.quits, 0);
    }

    // (2) An applied change reaches the shell with no new attach.
    {
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        fe.provider().shell().setTrayAvailableProbe([] { return true; });
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
        QVERIFY(!harness.options.getMinimizeOnClose());

        settings.setMinimizeOnClose(true);
        const int before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QVERIFY(!fe.provider().shell().requestClose());
        QCOMPARE(fe.quits, 0);
    }
}

void QmlSettingsAdapterTests::sideStakesRefreshOnAttach()
{
    qml_test::ModelHarness harness;
    harness.side_stakes.m_snapshot.entries = {Entry(ADDRESS.toStdString(), 10.0, false), Entry("second", 20.0, false)};
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SideStakeListModel& list = fe.provider().settings().sideStakeList();

    QCOMPARE(list.rowCount(), 2);
    QVERIFY(harness.side_stakes.m_entries_calls >= 1);
    QCOMPARE(list.data(list.index(0), SideStakeListModel::AddressRole).toString(), ADDRESS);
    QCOMPARE(list.data(list.index(1), SideStakeListModel::AllocationRole).toDouble(), 20.0);
}

void QmlSettingsAdapterTests::sideStakesForwardsLayoutChange()
{
    qml_test::ModelHarness harness;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    SideStakeListModel& list = fe.provider().settings().sideStakeList();
    QCOMPARE(list.rowCount(), 0);

    QSignalSpy reset(&list, &QAbstractItemModel::modelReset);
    harness.side_stakes.m_snapshot.entries = {Entry("first", 1.0, false), Entry("second", 2.0, false),
                                              Entry("third", 3.0, false)};
    // The table's own refresh, as a node notification makes it.
    harness.options.getSideStakeTableModel()->refresh();
    QCOMPARE(reset.count(), 1);
    QCOMPARE(list.rowCount(), 3);
}

void QmlSettingsAdapterTests::languageListFromResource()
{
    InitLocaleResources();
    const QStringList names = QDir(QStringLiteral(":translations")).entryList();
    QVERIFY2(!names.isEmpty(), "the bitcoin_locale resource holds no translation");

    qml_test::ModelHarness harness;
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
    fe.attachModels(harness.bundle());
    const QVariantList languages = fe.provider().settings().languages();

    QVERIFY(languages.size() >= 2);
    const QVariantMap first = languages.at(0).toMap();
    QCOMPARE(first.value(QStringLiteral("value")).toString(), QString());
    QCOMPARE(first.value(QStringLiteral("text")).toString(), QStringLiteral("(default)"));
    QCOMPARE(languages.size(), names.size() + 1);
    for (int i = 0; i < names.size(); ++i) {
        const QVariantMap entry = languages.at(i + 1).toMap();
        QCOMPARE(entry.value(QStringLiteral("value")).toString(), names.at(i));
        QVERIFY2(entry.value(QStringLiteral("text")).toString().endsWith(QStringLiteral("(") + names.at(i) +
                                                                         QStringLiteral(")")),
                 qPrintable(entry.value(QStringLiteral("text")).toString()));
    }
}

void QmlSettingsAdapterTests::reserveAppliedInSatoshis()
{
    // (1) and (2): the reserve is submitted as satoshis, whatever unit it is
    // shown in.
    {
        qml_test::ModelHarness harness;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
        QCOMPARE(settings.displayUnit(), int{BitcoinUnits::BTC});

        settings.setReserveBalance(QStringLiteral("5"));
        int before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[[(\"reservebalance\",\"5.00000000\")]]"));

        settings.setReserveBalance(QStringLiteral("7"));
        settings.setDisplayUnit(BitcoinUnits::mBTC);
        before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QCOMPARE(Text(harness.node.changeCalls()),
                 QStringLiteral("[[(\"reservebalance\",\"5.00000000\")], [(\"reservebalance\",\"7.00000000\")]]"));
    }

    // (3) A negative reserve is refused, and the refusal keeps the text.
    {
        qml_test::ModelHarness harness;
        harness.node.setSetting("reservebalance", "5.00000000");
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

        settings.setReserveBalance(QStringLiteral("-5"));
        const bool applied = settings.apply();
        QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
        QVERIFY(!applied);
        QCOMPARE(settings.lastError(), NotSavedText());
        QCOMPARE(harness.options.getReserveBalance(), qint64{5 * COIN});
        QCOMPARE(settings.reserveBalance(), QStringLiteral("-5"));
    }

    // (4) A re-read reserve is shown in the staged unit and is not submitted.
    {
        qml_test::ModelHarness harness;
        harness.node.setSetting("reservebalance", "5.00000000");
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

        settings.setDisplayUnit(BitcoinUnits::mBTC);
        harness.node.setSetting("stakingefficiency", "85");
        int before = settings.seedGeneration();
        harness.node.fireRwSettingsUpdated();
        QVERIFY2(SeedAfter(settings, before), "the settings notification posted no seed");
        QCOMPARE(settings.reserveBalance(), BitcoinUnits::format(BitcoinUnits::mBTC, 5 * COIN));
        before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        for (const auto& batch : harness.node.changeCalls()) {
            for (const auto& setting : batch) QVERIFY(setting.first != "reservebalance");
        }
    }
}

void QmlSettingsAdapterTests::availabilityFlags()
{
    // On mainnet the update-check box shows, and the login item is never
    // written from this front end.
    {
        qml_test::ModelHarness harness;
        harness.node.m_mainnet = true;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

        QVERIFY(!settings.startupOptionsAvailable());
        QVERIFY(settings.updateCheckOptionAvailable());
        settings.setStartAtStartup(true);
        const int before = settings.seedGeneration();
        QVERIFY(settings.apply());
        QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        QCOMPARE(Text(harness.node.changeCalls()), QStringLiteral("[]"));
        QVERIFY(!QSettings().contains(QStringLiteral("fStartAtStartup")));
    }

    // Off mainnet neither shows.
    {
        qml_test::ModelHarness harness;
        harness.node.m_mainnet = false;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        fe.attachModels(harness.bundle());
        SettingsAdapter& settings = fe.provider().settings();
        QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");

        QVERIFY(!settings.startupOptionsAvailable());
        QVERIFY(!settings.updateCheckOptionAvailable());
#ifdef USE_UPNP
        const bool upnp_compiled = true;
#else
        const bool upnp_compiled = false;
#endif
        QCOMPARE(settings.upnpAvailable(), upnp_compiled);
    }
}

void QmlSettingsAdapterTests::cleanupTestCase()
{
    QCOMPARE(qml_test::g_live_harnesses, 0);
}
