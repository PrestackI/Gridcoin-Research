// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/sendfeenegotiatortests.h"

#include "amount.h"
#include "chainparams.h"
#include "interfaces/wallet_coin_source.h"
#include "key_io.h"
#include "pubkey.h"
#include "qt/bitcoinunits.h"
#include "qt/optionsmodel.h"
#include "qt/sendcoinsdialog.h"
#include "qt/sendcoinsentry.h"
#include "qt/test/interfacefakes.h"
#include "qt/test/modelfakes.h"
#include "qt/walletmodel.h"
#include "uint256.h"
#include "util.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

//! The models a test drives, in the order GuiMain builds them: the scripted
//! wallet behind a real WalletModel.
struct Models {
    qt_test::ScriptedWallet wallet;
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager side_stakes;
    OptionsModel options{node, side_stakes};
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source{interfaces::MakeSyntheticCoinSource(0, 1)};
    WalletModel model{wallet, tx_source, *coin_source, &options};
};

//! A valid address of the selected chain, distinct for each `b`.
QString Address(unsigned char b)
{
    return QString::fromStdString(EncodeDestination(CKeyID(uint160(std::vector<unsigned char>(20, b)))));
}

interfaces::SendCoinsResult Result(interfaces::SendCoinsStatus status, int64_t fee = 0, std::string txid = {})
{
    interfaces::SendCoinsResult result;
    result.status = status;
    result.fee = fee;
    result.txid_hex = std::move(txid);
    return result;
}

interfaces::SendCoinsResult FeeRequest(int64_t fee)
{
    return Result(interfaces::SendCoinsStatus::FeeConfirmationRequired, fee);
}

//! Answers the message boxes the dialog opens, in order, from a script: a
//! timer finds the visible message box, records it, runs the hook set for
//! that box (by its index) and clicks the next scripted button. A box the
//! script cannot answer (the script is used up, or the box has no such
//! button) is recorded as unanswered and rejected, so its exec() returns.
//! It records rather than failing from its timer; each test checks the
//! records after its own compares.
class ScriptedMessageBoxes
{
public:
    struct Box {
        QString title;
        QString text;
        QMessageBox::StandardButtons buttons;
        QMessageBox::StandardButton default_button{QMessageBox::NoButton};
        bool answered{false};
    };

    explicit ScriptedMessageBoxes(std::vector<QMessageBox::StandardButton> script) : m_script(std::move(script))
    {
        QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] { Poll(); });
        m_timer.start(5);
    }
    ~ScriptedMessageBoxes() { stop(); }
    ScriptedMessageBoxes(const ScriptedMessageBoxes&) = delete;
    ScriptedMessageBoxes& operator=(const ScriptedMessageBoxes&) = delete;

    //! Runs `hook` while box number `index` (zero-based) is open, before it is
    //! answered.
    void setHook(int index, std::function<void()> hook) { m_hooks[index] = std::move(hook); }
    void stop() { m_timer.stop(); }

    const std::vector<Box>& boxes() const { return m_boxes; }
    int count(const QString& title) const
    {
        int n = 0;
        for (const Box& box : m_boxes) {
            if (box.title == title) ++n;
        }
        return n;
    }
    int unanswered() const
    {
        int n = 0;
        for (const Box& box : m_boxes) {
            if (!box.answered) ++n;
        }
        return n;
    }
    //! Scripted answers no box used.
    int leftover() const { return static_cast<int>(m_script.size() - m_next); }

private:
    void Poll()
    {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* box = qobject_cast<QMessageBox*>(widget);
            if (!box || !box->isVisible()) continue;
            bool seen = false;
            for (const QPointer<QMessageBox>& handled : m_handled) {
                if (handled == box) seen = true;
            }
            if (seen) continue;
            m_handled.push_back(box);

            Box record;
            record.title = box->windowTitle();
            record.text = box->text();
            record.buttons = box->standardButtons();
            record.default_button = box->defaultButton() ? box->standardButton(box->defaultButton())
                                                         : QMessageBox::NoButton;
            const int index = static_cast<int>(m_boxes.size());
            const auto hook = m_hooks.find(index);
            if (hook != m_hooks.end()) hook->second();

            if (m_next < m_script.size() && (record.buttons & m_script[m_next])) {
                QAbstractButton* button = box->button(m_script[m_next]);
                ++m_next;
                record.answered = true;
                m_boxes.push_back(record);
                button->click();
            } else {
                m_boxes.push_back(record);
                box->reject();
            }
            return;
        }
    }

    QTimer m_timer;
    std::vector<QMessageBox::StandardButton> m_script;
    std::size_t m_next{0};
    std::map<int, std::function<void()>> m_hooks;
    std::vector<Box> m_boxes;
    std::vector<QPointer<QMessageBox>> m_handled;
};

//! The dialog's entries, in layout order.
QList<SendCoinsEntry*> Entries(SendCoinsDialog& dlg)
{
    return dlg.findChildren<SendCoinsEntry*>();
}

int EntryCount(SendCoinsDialog& dlg)
{
    return static_cast<int>(Entries(dlg).size());
}

//! One recipient of COIN to `address`.
SendCoinsRecipient Recipient(const QString& address)
{
    SendCoinsRecipient rv;
    rv.address = address;
    rv.amount = COIN;
    return rv;
}

//! Fills the dialog's one entry and presses Send, as a user does.
void PressSend(SendCoinsDialog& dlg, const QString& address)
{
    QList<SendCoinsEntry*> entries = Entries(dlg);
    if (entries.size() == 1) entries.first()->setValue(Recipient(address));
    QMetaObject::invokeMethod(&dlg, "on_sendButton_clicked");
}

//! The accepted fee of each sendCoins call, in order.
std::vector<int64_t> AcceptedFees(const qt_test::ScriptedWallet& wallet)
{
    std::vector<int64_t> fees;
    for (const qt_test::ScriptedWallet::Call& call : wallet.log()) {
        if (call.method == qt_test::ScriptedWallet::Method::SendCoins) fees.push_back(call.accepted_fee);
    }
    return fees;
}

const QString CONFIRM_SEND_TITLE = QStringLiteral("Confirm send coins");
const QString CONFIRM_FEE_TITLE = QStringLiteral("Confirm transaction fee");

QString FeeText(int64_t fee)
{
    return QCoreApplication::translate("BitcoinGUI",
               "This transaction is over the size limit.  You can still send it for a fee of %1, "
               "which goes to the nodes that process your transaction and helps to support the network.  "
               "Do you want to pay the fee?").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, fee));
}

constexpr int64_t FEE = 123456789;

} // namespace

void SendFeeNegotiatorTests::initTestCase()
{
    // OptionsModel opens QSettings; keep its writes out of the user's store,
    // as OptionsModelTests does.
    m_prev_organization = QCoreApplication::organizationName();
    m_prev_application = QCoreApplication::applicationName();
    m_prev_format = QSettings::defaultFormat();

    m_settings_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings_root->isValid());

    QCoreApplication::setOrganizationName("GridcoinTest");
    QCoreApplication::setApplicationName("sendfeenegotiatortests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings_root->path());

    // The dialog validates addresses against the selected chain.
    SelectParams(CBaseChainParams::REGTEST);
}

void SendFeeNegotiatorTests::cleanupTestCase()
{
    QSettings::setDefaultFormat(m_prev_format);
    QCoreApplication::setOrganizationName(m_prev_organization);
    QCoreApplication::setApplicationName(m_prev_application);
    m_settings_root.reset();
}

void SendFeeNegotiatorTests::init()
{
    // OptionsModel::Init reaches GetDataDir(false), which creates a datadir:
    // point -datadir at a temporary directory.
    m_prev_datadir_set = gArgs.IsArgSet("-datadir");
    m_prev_datadir = gArgs.GetArg("-datadir", "");
    m_data_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_data_dir->isValid());
    gArgs.ForceSetArg("-datadir", m_data_dir->path().toStdString());
    gArgs.ClearPathCache();
}

void SendFeeNegotiatorTests::cleanup()
{
    QSettings().clear();
    if (m_prev_datadir_set) {
        gArgs.ForceSetArg("-datadir", m_prev_datadir);
    } else {
        gArgs.ClearForcedArg("-datadir");
    }
    gArgs.ClearPathCache();
    m_data_dir.reset();
}

void SendFeeNegotiatorTests::sendDialogFeeLoopCharacterization()
{
    const QString paste_address = Address(9);

    // A fee is asked for once and accepted: the second attempt carries it, and
    // the sent dialog is cleared. A URI pasted while the fee box is open is
    // ignored.
    {
        Models m;
        m.wallet.scriptSend({FeeRequest(FEE), Result(interfaces::SendCoinsStatus::OK, 0, "aa")});
        SendCoinsDialog dlg;
        dlg.setModel(&m.model);
        ScriptedMessageBoxes boxes({QMessageBox::Yes, QMessageBox::Yes});
        int entries_while_open = -1;
        boxes.setHook(1, [&] {
            dlg.pasteEntry(Recipient(Address(8)));
            entries_while_open = EntryCount(dlg);
        });
        PressSend(dlg, Address(1));
        boxes.stop();

        QCOMPARE(AcceptedFees(m.wallet), (std::vector<int64_t>{0, FEE}));
        QCOMPARE(boxes.boxes().size(), size_t{2});
        QCOMPARE(boxes.boxes()[0].title, CONFIRM_SEND_TITLE);
        QVERIFY(boxes.boxes()[0].buttons == QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::Cancel));
        QCOMPARE(boxes.boxes()[0].default_button, QMessageBox::Cancel);
        QCOMPARE(boxes.boxes()[1].title, CONFIRM_FEE_TITLE);
        QVERIFY(boxes.boxes()[1].buttons == QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::Cancel));
        QCOMPARE(boxes.boxes()[1].default_button, QMessageBox::Yes);
        QCOMPARE(boxes.boxes()[1].text, FeeText(FEE));
        QCOMPARE(entries_while_open, 1);
        QCOMPARE(EntryCount(dlg), 1);
        QVERIFY(Entries(dlg).first()->isClear());

        dlg.pasteEntry(Recipient(paste_address));
        QCOMPARE(EntryCount(dlg), 1);
        QCOMPARE(Entries(dlg).first()->getValue().address, paste_address);
        QCOMPARE(boxes.unanswered(), 0);
        QCOMPARE(boxes.leftover(), 0);
    }

    // The fee is declined: one attempt, no warning, the entry kept.
    {
        Models m;
        m.wallet.scriptSend({FeeRequest(FEE)});
        SendCoinsDialog dlg;
        dlg.setModel(&m.model);
        ScriptedMessageBoxes boxes({QMessageBox::Yes, QMessageBox::Cancel});
        PressSend(dlg, Address(1));
        boxes.stop();

        QCOMPARE(m.wallet.sendCalls(), 1);
        QCOMPARE(boxes.boxes().size(), size_t{2});
        QCOMPARE(boxes.boxes()[1].title, CONFIRM_FEE_TITLE);

        dlg.pasteEntry(Recipient(paste_address));
        QCOMPARE(EntryCount(dlg), 2);
        QCOMPARE(Entries(dlg).last()->getValue().address, paste_address);
        QCOMPARE(boxes.unanswered(), 0);
        QCOMPARE(boxes.leftover(), 0);
    }

    // A fee that keeps rising: ten fee boxes are accepted, the eleventh
    // request ends the send as a creation failure.
    {
        Models m;
        std::vector<interfaces::SendCoinsResult> results;
        for (int i = 0; i <= 10; ++i) results.push_back(FeeRequest(FEE + i));
        m.wallet.scriptSend(results);
        SendCoinsDialog dlg;
        dlg.setModel(&m.model);
        std::vector<QMessageBox::StandardButton> script{QMessageBox::Yes};
        for (int i = 0; i < 10; ++i) script.push_back(QMessageBox::Yes);
        script.push_back(QMessageBox::Ok);
        ScriptedMessageBoxes boxes(script);
        PressSend(dlg, Address(1));
        boxes.stop();

        QCOMPARE(boxes.count(CONFIRM_FEE_TITLE), 10);
        QCOMPARE(m.wallet.sendCalls(), 11);
        QVERIFY(!boxes.boxes().empty());
        const ScriptedMessageBoxes::Box& warning = boxes.boxes().back();
        QCOMPARE(warning.title, QStringLiteral("Send Coins"));
        QCOMPARE(warning.text, QStringLiteral("Error: Transaction creation failed."));
        QVERIFY(warning.buttons == QMessageBox::StandardButtons(QMessageBox::Ok));
        QCOMPARE(warning.default_button, QMessageBox::Ok);

        dlg.pasteEntry(Recipient(paste_address));
        QCOMPARE(EntryCount(dlg), 2);
        QCOMPARE(Entries(dlg).last()->getValue().address, paste_address);
        QCOMPARE(boxes.unanswered(), 0);
        QCOMPARE(boxes.leftover(), 0);
    }

    // A wallet unlocked for staking only: its own warning.
    {
        Models m;
        m.wallet.scriptSend({Result(interfaces::SendCoinsStatus::WalletUnlockedForStakingOnly)});
        SendCoinsDialog dlg;
        dlg.setModel(&m.model);
        ScriptedMessageBoxes boxes({QMessageBox::Yes, QMessageBox::Ok});
        PressSend(dlg, Address(1));
        boxes.stop();

        QCOMPARE(boxes.boxes().size(), size_t{2});
        QCOMPARE(boxes.boxes()[1].title, QStringLiteral("Send Coins"));
        QCOMPARE(boxes.boxes()[1].text,
                 QStringLiteral("Error: The wallet is unlocked for staking only, so it will not send. "
                                "Lock the wallet and unlock it again without the staking-only option to send."));

        dlg.pasteEntry(Recipient(paste_address));
        QCOMPARE(EntryCount(dlg), 2);
        QCOMPARE(Entries(dlg).last()->getValue().address, paste_address);
        QCOMPARE(boxes.unanswered(), 0);
        QCOMPARE(boxes.leftover(), 0);
    }
}

void SendFeeNegotiatorTests::sendDialogFeeInBalanceText()
{
    // A fee accepted, then a balance shortfall: the warning names the
    // shortfall's own fee.
    constexpr int64_t SHORT_FEE = 987654321;
    Models m;
    m.wallet.scriptSend({FeeRequest(FEE), Result(interfaces::SendCoinsStatus::AmountWithFeeExceedsBalance, SHORT_FEE)});
    SendCoinsDialog dlg;
    dlg.setModel(&m.model);
    ScriptedMessageBoxes boxes({QMessageBox::Yes, QMessageBox::Yes, QMessageBox::Ok});
    PressSend(dlg, Address(1));
    boxes.stop();

    QCOMPARE(boxes.boxes().size(), size_t{3});
    QCOMPARE(boxes.boxes()[2].title, QStringLiteral("Send Coins"));
    QCOMPARE(boxes.boxes()[2].text,
             QCoreApplication::translate("SendCoinsDialog",
                                         "The total exceeds your balance when the %1 transaction fee is included.")
                 .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, SHORT_FEE)));
    QCOMPARE(boxes.unanswered(), 0);
    QCOMPARE(boxes.leftover(), 0);
}

void SendFeeNegotiatorTests::sendDialogCoinControlEveryAttempt()
{
    const QString change = Address(7);

    // Coin control on, with a custom change address: every attempt carries
    // it.
    {
        Models m;
        m.wallet.scriptSend({FeeRequest(FEE), Result(interfaces::SendCoinsStatus::OK, 0, "aa")});
        SendCoinsDialog dlg;
        dlg.setModel(&m.model);
        QVERIFY(m.options.setData(m.options.index(OptionsModel::CoinControlFeatures), true, Qt::EditRole));
        dlg.show();
        auto* box = dlg.findChild<QCheckBox*>(QStringLiteral("coinControlChangeCheckBox"));
        auto* edit = dlg.findChild<QLineEdit*>(QStringLiteral("coinControlChangeEdit"));
        QVERIFY(box);
        QVERIFY(edit);
        box->setChecked(true);
        QVERIFY(edit->isEnabled());
        QTest::keyClicks(edit, change);
        QCOMPARE(edit->text(), change);

        ScriptedMessageBoxes boxes({QMessageBox::Yes, QMessageBox::Yes});
        PressSend(dlg, Address(1));
        boxes.stop();

        const std::vector<qt_test::ScriptedWallet::Call> log = m.wallet.log();
        int sends = 0;
        for (const qt_test::ScriptedWallet::Call& call : log) {
            if (call.method != qt_test::ScriptedWallet::Method::SendCoins) continue;
            ++sends;
            QVERIFY(call.coin_control.has_value());
            QCOMPARE(QString::fromStdString(call.coin_control->dest_change), change);
        }
        QCOMPARE(sends, 2);
        QCOMPARE(boxes.unanswered(), 0);
        QCOMPARE(boxes.leftover(), 0);
    }

    // Coin control off: no attempt carries a coin control.
    {
        Models m;
        m.wallet.scriptSend({FeeRequest(FEE), Result(interfaces::SendCoinsStatus::OK, 0, "aa")});
        SendCoinsDialog dlg;
        dlg.setModel(&m.model);
        QVERIFY(m.options.setData(m.options.index(OptionsModel::CoinControlFeatures), false, Qt::EditRole));
        ScriptedMessageBoxes boxes({QMessageBox::Yes, QMessageBox::Yes});
        PressSend(dlg, Address(1));
        boxes.stop();

        int sends = 0;
        for (const qt_test::ScriptedWallet::Call& call : m.wallet.log()) {
            if (call.method != qt_test::ScriptedWallet::Method::SendCoins) continue;
            ++sends;
            QVERIFY(!call.coin_control.has_value());
        }
        QCOMPARE(sends, 2);
        QCOMPARE(boxes.unanswered(), 0);
        QCOMPARE(boxes.leftover(), 0);
    }
}
