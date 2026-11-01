// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_SETTINGSWIRINGTESTS_H
#define BITCOIN_QT_QUICK_TEST_SETTINGSWIRINGTESTS_H

#include <QObject>
#include <QTest>

//! The carried settings screen and theme bound to Settings, through the real
//! root and QmlFrontEnd over a qml_test::ModelHarness. None runs the
//! application's event loop.
class QmlSettingsWiringTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void screenSaveAppliesDiscardReverts();
    void themeSurvivesProviderRestart();
};

#endif // BITCOIN_QT_QUICK_TEST_SETTINGSWIRINGTESTS_H
