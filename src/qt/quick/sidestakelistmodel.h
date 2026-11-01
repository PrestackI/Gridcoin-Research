// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_SIDESTAKELISTMODEL_H
#define BITCOIN_QT_QUICK_SIDESTAKELISTMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QMetaObject>
#include <QModelIndex>
#include <QString>
#include <QVariant>

class SideStakeTableModel;

//! The sidestake table as a list model with one role per column, for the
//! settings screen. It follows SideStakeTableModel, which holds the rows, and
//! forwards its commands to it on the GUI thread, as the Widgets dialogs do.
//! Owned by SettingsAdapter.
class SideStakeListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Role {
        AddressRole = Qt::UserRole + 1,
        AllocationTextRole,
        AllocationRole,
        DescriptionRole,
        StatusRole,
        MandatoryRole,
        EditableRole,
    };

    explicit SideStakeListModel(QObject* parent = nullptr);

    //! Follows `table` from now on: its layout signals reset this model. The
    //! signals are connected before the table is refreshed, so the refresh
    //! reaches this model. The table must outlive the matching detach().
    void attach(SideStakeTableModel* table);
    //! Stops following the table and resets to no rows. A detach() before
    //! attach() does nothing.
    void detach();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    //! Adds a local sidestake; false, with lastError set, when the table
    //! refuses it. False while detached.
    Q_INVOKABLE bool add(const QString& address, const QString& allocation, const QString& description);
    //! Sets a local row's allocation, then its description when the first
    //! step answered OK or no change. A mandatory row is refused before any
    //! edit reaches the table. False while detached.
    Q_INVOKABLE bool edit(int row, const QString& allocation, const QString& description);
    //! Deletes a local row. A mandatory row is refused before any delete
    //! reaches the table.
    //! A delete the table refuses returns false with lastError empty, as the
    //! Options dialog shows nothing then. False while detached.
    Q_INVOKABLE bool remove(int row);
    //! Re-reads the table's rows; does nothing while detached.
    Q_INVOKABLE void refresh();

    QString lastError() const { return m_last_error; }

signals:
    void lastErrorChanged();

private:
    void setLastError(const QString& text);
    //! The text for the table's last edit status; empty for OK and no change.
    QString editStatusText(const QString& address) const;
    //! The refusal text for a mandatory row.
    void refuseMandatory();

    SideStakeTableModel* m_table{nullptr};
    QMetaObject::Connection m_about_to_change;
    QMetaObject::Connection m_changed;
    QString m_last_error;
};

#endif // BITCOIN_QT_QUICK_SIDESTAKELISTMODEL_H
