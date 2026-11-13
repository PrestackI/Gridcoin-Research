// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "settingswiringtests.h"

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

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>
#include <QVariant>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <exception>
#include <string>

namespace {

using namespace std::chrono_literals;

//! The carried root, as gridcoinresearch-qml loads it.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

//! An address the sidestake fake accepts unchecked.
const QString ADDRESS = QStringLiteral("mfiy9sc2QEZZCK3WMUMZjNfrdRA6gXzRhr");

//! Every message Qt's message handler receives while CollectMessage is
//! installed; the previous handler still gets each message.
QStringList g_messages;
QtMessageHandler g_previous_handler = nullptr;

void CollectMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    g_messages << message;
    if (g_previous_handler) g_previous_handler(type, context, message);
}

//! Keeps a closed QML window from quitting the application for the life of a
//! test.
class NoQuitOnLastWindow
{
public:
    NoQuitOnLastWindow() : m_old(qApp->quitOnLastWindowClosed()) { qApp->setQuitOnLastWindowClosed(false); }
    ~NoQuitOnLastWindow() { qApp->setQuitOnLastWindowClosed(m_old); }
    NoQuitOnLastWindow(const NoQuitOnLastWindow&) = delete;
    NoQuitOnLastWindow& operator=(const NoQuitOnLastWindow&) = delete;

private:
    bool m_old;
};

//! Joins the global pool, so no pooled read outlives the harness's fakes.
void ReapPool()
{
    QThreadPool::globalInstance()->waitForDone();
}

//! What GuiMain does before its event loop, with the models attached: the
//! splash, the root, the models, then the main window. False when the root
//! did not load.
bool Preamble(QmlFrontEnd& fe, qml_test::ModelHarness& harness)
{
    fe.createSplash();
    try {
        fe.construct();
    } catch (const std::exception& e) {
        qWarning().noquote() << "construct() threw:" << e.what();
        return false;
    }
    if (!fe.rootObject()) return false;
    fe.attachModels(harness.bundle());
    fe.showMain(false);
    return true;
}

//! An object the root holds in the QML property `name`.
QObject* RootProperty(QmlFrontEnd& fe, const char* name)
{
    QObject* root = fe.rootObject();
    return root ? root->property(name).value<QObject*>() : nullptr;
}

//! The object named `name` under the main window.
QObject* MainItem(QmlFrontEnd& fe, const char* name)
{
    QObject* main = RootProperty(fe, "mainWindow");
    return main ? main->findChild<QObject*>(QString::fromLatin1(name)) : nullptr;
}

//! Evaluates `expression` in the root's own context, where the root's ids and
//! imports resolve. `error` holds the engine's message when it fails.
QVariant Evaluate(QmlFrontEnd& fe, const QString& expression, QString& error)
{
    QObject* root = fe.rootObject();
    QQmlExpression expr(QQmlEngine::contextForObject(root), root, expression);
    QVariant value = expr.evaluate();
    error = expr.hasError() ? expr.error().toString() : QString();
    return value;
}

//! Pumps until a seed newer than `before` has been delivered.
bool SeedAfter(SettingsAdapter& settings, int before)
{
    return qml_test::PumpUntil([&settings, before] { return settings.seedGeneration() > before; }, 2s);
}

//! Opens the settings screen through the tab menu, as a click on its entry
//! does, and returns the view once it is loaded and has read Settings again.
QObject* OpenSettings(QmlFrontEnd& fe)
{
    QObject* menu = MainItem(fe, "tabMenu");
    QObject* loader = MainItem(fe, "viewLoader");
    if (!menu || !loader) return nullptr;
    SettingsAdapter& settings = fe.provider().settings();
    const int before = settings.seedGeneration();
    if (!QMetaObject::invokeMethod(menu, "select", Q_ARG(QVariant, QVariant(QStringLiteral("SettingsView.qml"))))) {
        return nullptr;
    }
    QObject* view = nullptr;
    const bool loaded = qml_test::PumpUntil(
        [&] {
            view = loader->property("item").value<QObject*>();
            return view && view->findChild<QObject*>(QStringLiteral("settingsTabRow"));
        },
        2s);
    if (!loaded || !SeedAfter(settings, before)) return nullptr;
    return view;
}

//! The object named `name` under `view`, once it exists.
QObject* Find(QObject* view, const char* name)
{
    QObject* found = nullptr;
    qml_test::PumpUntil(
        [&] {
            found = view->findChild<QObject*>(QString::fromLatin1(name));
            return found != nullptr;
        },
        2s);
    return found;
}

//! Shows the tab whose button is named `tab`, as a click on it does.
bool SelectTab(QObject* view, const char* tab)
{
    QObject* row = view->findChild<QObject*>(QStringLiteral("settingsTabRow"));
    QObject* button = view->findChild<QObject*>(QString::fromLatin1(tab));
    if (!row || !button) return false;
    return row->setProperty("currentItem", QVariant::fromValue(qobject_cast<QQuickItem*>(button)));
}

//! A user's click on a check box: the toggle, then its signal.
bool Toggle(QObject* box)
{
    return QMetaObject::invokeMethod(box, "toggle") && QMetaObject::invokeMethod(box, "toggled");
}

//! A user's edit of a text field.
bool Type(QObject* field, const QString& text)
{
    return field->setProperty("text", text) && QMetaObject::invokeMethod(field, "textEdited");
}

//! A button press.
bool Press(QObject* button)
{
    return QMetaObject::invokeMethod(button, "clicked");
}

//! The delegate the list shows for `row`.
QQuickItem* Row(QObject* list, int row)
{
    QQuickItem* item = nullptr;
    QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, row));
    return item;
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

//! The number of recorded batches that write `name`.
int BatchesNaming(const qml_test::StatusFakeNode& node, const std::string& name)
{
    int count = 0;
    for (const auto& batch : node.changeCalls()) {
        for (const auto& setting : batch) {
            if (setting.first == name) ++count;
        }
    }
    return count;
}

QString AllocationText()
{
    return QCoreApplication::translate("EditSideStakeDialog",
                                       "The entered allocation is not valid. Check to make sure that the "
                                       "allocation is greater than zero and when added to the other allocations "
                                       "totals less than 100.");
}

} // namespace

void QmlSettingsWiringTests::screenSaveAppliesDiscardReverts()
{
    // The first harness: the four tabs, with the message collector installed
    // before its front end is built.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.options.setData(harness.options.index(OptionsModel::WalletStylesheet), QStringLiteral("light"),
                                Qt::EditRole);
        harness.side_stakes.m_snapshot.entries = {Entry("mandatory", 5.0, true), Entry(ADDRESS.toStdString(), 10.0, false)};
        g_messages.clear();
        g_previous_handler = qInstallMessageHandler(CollectMessage);
        qml_test::ScopeExit restore_handler([] { qInstallMessageHandler(g_previous_handler); });
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        SettingsAdapter& settings = fe.provider().settings();
        QObject* view = OpenSettings(fe);
        QVERIFY2(view, "the settings screen did not open");

        // (c) The update-check box is hidden off mainnet, and the login item
        // is never shown.
        QObject* update_box = Find(view, "disableUpdateCheckBox");
        QVERIFY(update_box);
        QVERIFY(!update_box->property("visible").toBool());
        QObject* login_box = Find(view, "startAtLoginCheckbox");
        QVERIFY(login_box);
        QVERIFY(!login_box->property("visible").toBool());

        // (d) The mandatory row cannot be removed or edited; the local row can,
        // and no address is editable.
        QObject* list = Find(view, "sideStakeListView");
        QObject* remove = Find(view, "removeSSButton");
        QObject* edit = Find(view, "editSSButton");
        QVERIFY(list);
        QVERIFY(remove);
        QVERIFY(edit);
        QVERIFY(qml_test::PumpUntil([&] { return list->property("count").toInt() == 2; }, 2s));
        QVERIFY(list->setProperty("currentIndex", 0));
        QVERIFY(!remove->property("enabled").toBool());
        QVERIFY(!edit->property("enabled").toBool());
        QQuickItem* row0 = Row(list, 0);
        QVERIFY(row0);
        QObject* allocation0 = row0->findChild<QObject*>(QStringLiteral("allocationField"));
        QVERIFY(allocation0);
        QVERIFY(allocation0->property("readOnly").toBool());
        QVERIFY(list->setProperty("currentIndex", 1));
        QVERIFY(remove->property("enabled").toBool());
        QVERIFY(edit->property("enabled").toBool());
        QQuickItem* row1 = Row(list, 1);
        QVERIFY(row1);
        QObject* address1 = row1->findChild<QObject*>(QStringLiteral("addressField"));
        QVERIFY(address1);
        QVERIFY(address1->property("readOnly").toBool());

        // (l) Add and edit show only once 'Enable Sidestaking' is stored: the
        // node lists local rows only while the stored setting is on. Ticking
        // the box stages it and shows neither; Save shows both; unticking it
        // again, unsaved, keeps them; saving it unticked hides them. Delete
        // stays. The node stores no enablesidestaking here, so the box opens
        // unticked.
        QObject* side_staking = Find(view, "sideStakingCheckbox");
        QObject* add = Find(view, "addSSButton");
        QObject* save = Find(view, "settingsSaveButton");
        QObject* discard = Find(view, "settingsDiscardButton");
        QVERIFY(side_staking);
        QVERIFY(add);
        QVERIFY(save);
        QVERIFY(discard);
        QVERIFY(!side_staking->property("checked").toBool());
        QVERIFY(remove->property("visible").toBool());
        QVERIFY(!add->property("visible").toBool());
        QVERIFY(!edit->property("visible").toBool());
        QVERIFY(Toggle(side_staking));
        QVERIFY(settings.enableSideStaking());
        QVERIFY(!settings.sideStakingStored());
        QVERIFY(!add->property("visible").toBool());
        QVERIFY(!edit->property("visible").toBool());
        int before = settings.seedGeneration();
        QVERIFY(Press(save));
        QVERIFY2(SeedAfter(settings, before), "Save posted no seed");
        QCOMPARE(BatchesNaming(harness.node, "enablesidestaking"), 1);
        QVERIFY(settings.sideStakingStored());
        QVERIFY(add->property("visible").toBool());
        QVERIFY(edit->property("visible").toBool());
        QVERIFY(Toggle(side_staking));
        QVERIFY(!settings.enableSideStaking());
        QVERIFY(add->property("visible").toBool());
        QVERIFY(edit->property("visible").toBool());
        QVERIFY(Press(discard));
        QVERIFY(settings.enableSideStaking());

        // (h) The add row reaches the sidestake table, through the + shown
        // while the stored setting is on.
        QObject* new_address = Find(view, "newSideStakeAddressField");
        QObject* new_allocation = Find(view, "newSideStakeAllocationField");
        QObject* new_description = Find(view, "newSideStakeDescriptionField");
        QVERIFY(new_address);
        QVERIFY(new_allocation);
        QVERIFY(new_description);
        QVERIFY(Type(new_address, ADDRESS));
        QVERIFY(Type(new_allocation, QStringLiteral("5")));
        QVERIFY(Type(new_description, QStringLiteral("new")));
        QVERIFY(add->property("visible").toBool());
        QVERIFY(Press(add));
        QCOMPARE(QString::fromStdString(harness.side_stakes.m_last_add_address), ADDRESS);

        // (l, continued) Saving the box unticked hides add and edit again.
        QVERIFY(Toggle(side_staking));
        before = settings.seedGeneration();
        QVERIFY(Press(save));
        QVERIFY2(SeedAfter(settings, before), "Save posted no seed");
        QCOMPARE(BatchesNaming(harness.node, "enablesidestaking"), 2);
        QVERIFY(!settings.sideStakingStored());
        QVERIFY(!add->property("visible").toBool());
        QVERIFY(!edit->property("visible").toBool());
        QVERIFY(remove->property("visible").toBool());

        // (k) The contract-change box is saved to the node.
        QObject* contract = Find(view, "contractChangeCheckbox");
        QVERIFY(contract);
        QVERIFY(!contract->property("checked").toBool());
        QVERIFY(Toggle(contract));
        const int contract_before = settings.seedGeneration();
        QVERIFY(Press(save));
        QVERIFY2(SeedAfter(settings, contract_before), "Save posted no seed");
        QCOMPARE(BatchesNaming(harness.node, "contractchangetoinputaddress"), 1);

        // (a) Save submits the toggled box; Discard drops a second toggle.
        // Without UPnP compiled in, the box is disabled and not driven.
        QVERIFY(SelectTab(view, "networkSettingsTabButton"));
        QObject* upnp = Find(view, "upnpCheckbox");
        QVERIFY(upnp);
        before = settings.seedGeneration();
#ifdef USE_UPNP
        QVERIFY(upnp->property("enabled").toBool());
        QVERIFY(Toggle(upnp));
        QVERIFY(Press(save));
        QVERIFY2(SeedAfter(settings, before), "Save posted no seed");
        QCOMPARE(BatchesNaming(harness.node, "upnp"), 1);
        const bool saved = upnp->property("checked").toBool();
        QVERIFY(Toggle(upnp));
        QCOMPARE(upnp->property("checked").toBool(), !saved);
        QVERIFY(Press(discard));
        QCOMPARE(BatchesNaming(harness.node, "upnp"), 1);
        QCOMPARE(upnp->property("checked").toBool(), saved);
#else
        QVERIFY(!upnp->property("enabled").toBool());
#endif

        // (e) The minimise-to-tray option is saved.
        QVERIFY(SelectTab(view, "windowSettingsTabButton"));
        QObject* tray = Find(view, "minimiseToTrayCheckbox");
        QVERIFY(tray);
        QVERIFY(!harness.options.getMinimizeToTray());
        QVERIFY(Toggle(tray));
        before = settings.seedGeneration();
        QVERIFY(Press(save));
        QVERIFY2(SeedAfter(settings, before), "Save posted no seed");
        QVERIFY(harness.options.getMinimizeToTray());

        // (b) The theme box writes the stored theme.
        QVERIFY(SelectTab(view, "displaySettingsTabButton"));
        QObject* theme = Find(view, "themeCombobox");
        QVERIFY(theme);
        QCOMPARE(theme->property("currentIndex").toInt(), 0);
        QVERIFY(theme->setProperty("currentIndex", 1));
        QVERIFY(QMetaObject::invokeMethod(theme, "activated", Q_ARG(int, 1)));
        before = settings.seedGeneration();
        QVERIFY(Press(save));
        QVERIFY2(SeedAfter(settings, before), "Save posted no seed");
        QCOMPARE(harness.options.getCurrentStyle(), QStringLiteral("dark"));

        // (g) Page Down stops at the Display tab.
        QObject* row = view->findChild<QObject*>(QStringLiteral("settingsTabRow"));
        QObject* next_page = Find(view, "nextSettingsPageShortcut");
        QObject* display_tab = Find(view, "displaySettingsTabButton");
        QVERIFY(row);
        QVERIFY(next_page);
        QVERIFY(display_tab);
        QVERIFY(QMetaObject::invokeMethod(next_page, "activated"));
        QCOMPARE(row->property("currentItem").value<QObject*>(), display_tab);

        QStringList problems;
        for (const QString& message : std::as_const(g_messages)) {
            if (message.contains(QStringLiteral("qrc:/qt/qml/Gridcoin/App/SettingsView.qml"))) problems << message;
        }
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    }
    if (QTest::currentTestFailed()) return;

    // (c) On mainnet the update-check box shows.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.m_mainnet = true;
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        QObject* view = OpenSettings(fe);
        QVERIFY2(view, "the settings screen did not open");
        QObject* update_box = Find(view, "disableUpdateCheckBox");
        QVERIFY(update_box);
        QVERIFY(update_box->property("visible").toBool());
    }
    if (QTest::currentTestFailed()) return;

    // (f) A stored reserve is shown in full in the display unit, and the field
    // accepts it.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.setSetting("reservebalance", "1000.00000000");
        harness.options.setData(harness.options.index(OptionsModel::DisplayUnit), int{BitcoinUnits::BTC},
                                Qt::EditRole);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        QObject* view = OpenSettings(fe);
        QVERIFY2(view, "the settings screen did not open");
        QObject* reserve = Find(view, "reserveField");
        QVERIFY(reserve);
        QCOMPARE(reserve->property("text").toString(), BitcoinUnits::format(BitcoinUnits::BTC, 1000 * COIN));
        QVERIFY(reserve->property("acceptableInput").toBool());
    }
    if (QTest::currentTestFailed()) return;

    // (j) The Display tab's two lists open on the stored values: a stored
    // unit other than the first, and the default language.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.options.setData(harness.options.index(OptionsModel::DisplayUnit), int{BitcoinUnits::mBTC},
                                Qt::EditRole);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        SettingsAdapter& settings = fe.provider().settings();
        QObject* view = OpenSettings(fe);
        QVERIFY2(view, "the settings screen did not open");
        QCOMPARE(settings.displayUnit(), int{BitcoinUnits::mBTC});
        QVERIFY(settings.language().isEmpty());
        QVERIFY(SelectTab(view, "displaySettingsTabButton"));
        QObject* unit = Find(view, "unitCombobox");
        QObject* language = Find(view, "languageCombobox");
        QVERIFY(unit);
        QVERIFY(language);
        QCOMPARE(unit->property("currentText").toString(), BitcoinUnits::name(BitcoinUnits::mBTC));
        QCOMPARE(language->property("currentIndex").toInt(), 0);
    }
    if (QTest::currentTestFailed()) return;

    // (i) A refused row and a refused sidestake show their texts.
    {
        NoQuitOnLastWindow no_quit;
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        harness.node.m_fail_changes = true;
        // Stored on, so the + shows: the add below presses it as a user would.
        harness.node.setSetting("enablesidestaking", "1");
        harness.side_stakes.m_edit_result.status = interfaces::SideStakeEditStatus::INVALID_ALLOCATION;
        g_messages.clear();
        g_previous_handler = qInstallMessageHandler(CollectMessage);
        qml_test::ScopeExit restore_handler([] { qInstallMessageHandler(g_previous_handler); });
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        SettingsAdapter& settings = fe.provider().settings();
        QObject* view = OpenSettings(fe);
        QVERIFY2(view, "the settings screen did not open");

        // The General tab shows first: the reserve and the add row are
        // driven there. The reserve is a node setting, which the failing
        // node refuses.
        QObject* reserve = Find(view, "reserveField");
        QObject* new_address = Find(view, "newSideStakeAddressField");
        QObject* new_allocation = Find(view, "newSideStakeAllocationField");
        QObject* new_description = Find(view, "newSideStakeDescriptionField");
        QObject* add = Find(view, "addSSButton");
        QObject* side_status = Find(view, "sideStakeStatusLabel");
        QVERIFY(reserve);
        QVERIFY(new_address);
        QVERIFY(new_allocation);
        QVERIFY(new_description);
        QVERIFY(add);
        QVERIFY(side_status);
        QVERIFY(Type(reserve, QStringLiteral("5")));
        QVERIFY(Type(new_address, ADDRESS));
        QVERIFY(Type(new_allocation, QStringLiteral("5")));
        QVERIFY(Type(new_description, QStringLiteral("new")));
        QVERIFY(add->property("visible").toBool());
        QVERIFY(Press(add));
        QCOMPARE(side_status->property("text").toString(), AllocationText());

        // The refusal wraps over several lines. It sits under the table, not
        // in the table's one-line footer: the add row stays inside the table,
        // and the controls below start under the text, so nothing covers the
        // add row's fields.
        auto* table = qobject_cast<QQuickItem*>(Find(view, "sideStakeRect"));
        auto* below = qobject_cast<QQuickItem*>(Find(view, "secondContentGrid"));
        auto* address_item = qobject_cast<QQuickItem*>(new_address);
        auto* status_item = qobject_cast<QQuickItem*>(side_status);
        QVERIFY(table);
        QVERIFY(below);
        QVERIFY(address_item);
        QVERIFY(status_item);
        // The window gets the size of the live run's window. No frame is
        // rendered here, so the layouts are polished by hand, innermost
        // first, before anything is measured.
        QQuickWindow* window = table->window();
        QVERIFY(window);
        window->resize(720, 840);
        qml_test::PumpFor(100ms);
        for (QQuickItem* item : {status_item->parentItem(), address_item->parentItem(), below, table}) {
            if (item) item->ensurePolished();
        }
        const auto scene_box = [](QQuickItem* item) {
            return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
        };
        const auto describe = [](const char* name, const QRectF& box) {
            return QStringLiteral("%1 y %2..%3 x %4..%5")
                .arg(QLatin1String(name))
                .arg(box.top())
                .arg(box.bottom())
                .arg(box.left())
                .arg(box.right());
        };
        const QString boxes = QStringList{describe("table", scene_box(table)),
                                          describe("address field", scene_box(address_item)),
                                          describe("refusal text", scene_box(status_item)),
                                          describe("controls below", scene_box(below))}
                                  .join(QStringLiteral("; "));
        QVERIFY2(scene_box(address_item).bottom() <= scene_box(table).bottom(),
                 qPrintable(QStringLiteral("the address field reaches below the table: ") + boxes));
        QVERIFY2(scene_box(status_item).top() >= scene_box(table).bottom(),
                 qPrintable(QStringLiteral("the refusal text overlaps the table: ") + boxes));
        QVERIFY2(scene_box(below).top() >= scene_box(status_item).bottom(),
                 qPrintable(QStringLiteral("the controls below overlap the refusal text: ") + boxes));
        // The checks above measured a wrapped text: more than one line.
        QVERIFY2(scene_box(status_item).height() > 20, qPrintable(QStringLiteral("one line only: ") + boxes));
        QStringList problems;
        for (const QString& message : std::as_const(g_messages)) {
            if (message.contains(QStringLiteral("qrc:/qt/qml/Gridcoin/App/SettingsView.qml"))) problems << message;
        }
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));

        QObject* save = Find(view, "settingsSaveButton");
        QObject* status = Find(view, "settingsStatusLabel");
        QVERIFY(save);
        QVERIFY(status);
        const int before = settings.seedGeneration();
        QVERIFY(Press(save));
        QVERIFY2(SeedAfter(settings, before), "Save posted no seed");
        // The failing node records the batch it refuses; apply()'s own check
        // of the reserve would record none.
        QCOMPARE(BatchesNaming(harness.node, "reservebalance"), 1);
        QCOMPARE(status->property("text").toString(), QStringLiteral("A setting could not be saved."));
    }
}

void QmlSettingsWiringTests::themeSurvivesProviderRestart()
{
    NoQuitOnLastWindow no_quit;
    QString error;
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);

        // (1) Front end 1 stores "light".
        {
            std::atomic<int> disconnects{0};
            qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
            qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
            QVERIFY2(Preamble(fe, harness), "the root did not load");
            SettingsAdapter& settings = fe.provider().settings();
            QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
            settings.setTheme(QStringLiteral("light"));
            const int before = settings.seedGeneration();
            QVERIFY(settings.apply());
            QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
        }

        // Front end 2 shows the stored theme, and follows each Apply after a
        // JS write of the theme.
        {
            std::atomic<int> disconnects{0};
            qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
            qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
            QVERIFY2(Preamble(fe, harness), "the root did not load");
            SettingsAdapter& settings = fe.provider().settings();
            QVERIFY2(SeedAfter(settings, 0), "the first seed was not delivered");
            const int light = Evaluate(fe, QStringLiteral("MMPTheme.lightTheme"), error).toInt();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            const int dark = Evaluate(fe, QStringLiteral("MMPTheme.darkTheme"), error).toInt();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            int theme = Evaluate(fe, QStringLiteral("MMPTheme.theme"), error).toInt();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(theme, light);

            // (2)
            Evaluate(fe, QStringLiteral("MMPTheme.theme = MMPTheme.lightTheme"), error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            settings.setTheme(QStringLiteral("dark"));
            int before = settings.seedGeneration();
            QVERIFY(settings.apply());
            QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
            theme = Evaluate(fe, QStringLiteral("MMPTheme.theme"), error).toInt();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(theme, dark);

            // (3)
            Evaluate(fe, QStringLiteral("MMPTheme.theme = MMPTheme.darkTheme"), error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            settings.setTheme(QStringLiteral("light"));
            before = settings.seedGeneration();
            QVERIFY(settings.apply());
            QVERIFY2(SeedAfter(settings, before), "apply() posted no seed");
            theme = Evaluate(fe, QStringLiteral("MMPTheme.theme"), error).toInt();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(theme, light);
        }

        // (4) Front end 3 on the same harness shows the stored theme.
        {
            std::atomic<int> disconnects{0};
            qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
            qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
            QVERIFY2(Preamble(fe, harness), "the root did not load");
            QVERIFY2(SeedAfter(fe.provider().settings(), 0), "the first seed was not delivered");
            const int light = Evaluate(fe, QStringLiteral("MMPTheme.lightTheme"), error).toInt();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            const int theme = Evaluate(fe, QStringLiteral("MMPTheme.theme"), error).toInt();
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(theme, light);
        }
    }

    // A new harness stores no theme: the default shows.
    {
        qml_test::ModelHarness harness;
        qml_test::ScopeExit reap(ReapPool);
        std::atomic<int> disconnects{0};
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        qml_test::ScopeExit detach([&fe] { fe.detachModels(); });
        QVERIFY2(Preamble(fe, harness), "the root did not load");
        QVERIFY2(SeedAfter(fe.provider().settings(), 0), "the first seed was not delivered");
        const int dark = Evaluate(fe, QStringLiteral("MMPTheme.darkTheme"), error).toInt();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const int theme = Evaluate(fe, QStringLiteral("MMPTheme.theme"), error).toInt();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(theme, dark);
    }
}
