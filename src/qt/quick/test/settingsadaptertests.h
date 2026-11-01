// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_SETTINGSADAPTERTESTS_H
#define BITCOIN_QT_QUICK_TEST_SETTINGSADAPTERTESTS_H

#include <QObject>
#include <QTest>

//! Settings and its sidestake list over the real OptionsModel and
//! SideStakeTableModel, built on fakes by qml_test::ModelHarness. No case
//! loads a root or runs the application's event loop.
class QmlSettingsAdapterTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void discardCallsNothing();
    void applySubmitsOnlyTouchedInOrder();
    void efficiencyWholeNumberInRange();
    void hiddenSplitFieldsNotSubmitted();
    void proxyFieldsIgnoredWhileUnticked();
    void minSplitAtLeastFloor();
    void failedRowDoesNotStopApply();
    void overHundredSidestakeRefused();
    void mandatoryRowNotEditable();
    void failedRemoveShowsNoStaleStatus();
    void rwSettingsUpdateRefreshesUntouched();
    void windowOptionsPushedOnApply();
    void sideStakesRefreshOnAttach();
    void sideStakesForwardsLayoutChange();
    void languageListFromResource();
    void reserveAppliedInSatoshis();
    void availabilityFlags();
    void cleanupTestCase();
};

#endif // BITCOIN_QT_QUICK_TEST_SETTINGSADAPTERTESTS_H
