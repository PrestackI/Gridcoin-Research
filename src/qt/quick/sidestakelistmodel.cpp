// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/sidestakelistmodel.h>

#include <qt/sidestaketablemodel.h>

#include <QCoreApplication>

SideStakeListModel::SideStakeListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void SideStakeListModel::attach(SideStakeTableModel* table)
{
    detach();
    if (!table) return;
    // Every change of the table's rows (a refresh, a sort, a command's
    // refetch) is a layout change there; a reset here is the simplest exact
    // copy of it.
    m_about_to_change = connect(table, &QAbstractItemModel::layoutAboutToBeChanged, this,
                                [this] { beginResetModel(); });
    m_changed = connect(table, &QAbstractItemModel::layoutChanged, this, [this] { endResetModel(); });
    beginResetModel();
    m_table = table;
    endResetModel();
    m_table->refresh();
}

void SideStakeListModel::detach()
{
    if (!m_table) return;
    disconnect(m_about_to_change);
    disconnect(m_changed);
    beginResetModel();
    m_table = nullptr;
    endResetModel();
}

int SideStakeListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_table) return 0;
    return m_table->rowCount(QModelIndex());
}

QVariant SideStakeListModel::data(const QModelIndex& index, int role) const
{
    if (!m_table || !index.isValid() || index.row() < 0 || index.row() >= rowCount()) return QVariant();
    const int row = index.row();
    const auto cell = [this, row](SideStakeTableModel::ColumnIndex column, int cell_role) {
        return m_table->data(m_table->index(row, column, QModelIndex()), cell_role);
    };
    switch (role) {
    case AddressRole:
        return cell(SideStakeTableModel::Address, Qt::DisplayRole);
    case AllocationTextRole:
        return cell(SideStakeTableModel::Allocation, Qt::DisplayRole);
    case AllocationRole:
        return cell(SideStakeTableModel::Allocation, Qt::EditRole).toString().toDouble();
    case DescriptionRole:
        return cell(SideStakeTableModel::Description, Qt::DisplayRole);
    case StatusRole:
        return cell(SideStakeTableModel::Status, Qt::DisplayRole);
    case MandatoryRole:
        return m_table->isMandatory(row);
    case EditableRole:
        return !m_table->isMandatory(row);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SideStakeListModel::roleNames() const
{
    return {
        {AddressRole, "address"},
        {AllocationTextRole, "allocationText"},
        {AllocationRole, "allocation"},
        {DescriptionRole, "description"},
        {StatusRole, "status"},
        {MandatoryRole, "mandatory"},
        {EditableRole, "editable"},
    };
}

bool SideStakeListModel::add(const QString& address, const QString& allocation, const QString& description)
{
    if (!m_table) return false;
    if (m_table->addRow(address, allocation, description).isEmpty()) {
        setLastError(editStatusText(address));
        return false;
    }
    setLastError(QString());
    return true;
}

bool SideStakeListModel::edit(int row, const QString& allocation, const QString& description)
{
    if (!m_table || row < 0 || row >= rowCount()) return false;
    if (m_table->isMandatory(row)) {
        refuseMandatory();
        return false;
    }
    const QString address =
        m_table->data(m_table->index(row, SideStakeTableModel::Address, QModelIndex()), Qt::EditRole).toString();
    // EditSideStakeDialog::saveCurrentRow's order: the description is set only
    // when the allocation step answered OK or no change.
    const auto accepted = [this] {
        const SideStakeTableModel::EditStatus status = m_table->getEditStatus();
        return status == SideStakeTableModel::OK || status == SideStakeTableModel::NO_CHANGES;
    };
    m_table->setData(m_table->index(row, SideStakeTableModel::Allocation, QModelIndex()), allocation, Qt::EditRole);
    if (accepted()) {
        m_table->setData(m_table->index(row, SideStakeTableModel::Description, QModelIndex()), description,
                         Qt::EditRole);
        if (accepted()) {
            setLastError(QString());
            return true;
        }
    }
    setLastError(editStatusText(address));
    return false;
}

bool SideStakeListModel::remove(int row)
{
    if (!m_table || row < 0 || row >= rowCount()) return false;
    if (m_table->isMandatory(row)) {
        refuseMandatory();
        return false;
    }
    if (!m_table->removeRows(row, 1)) {
        // The table keeps no status for a delete, and the Options dialog
        // shows nothing when one is refused.
        setLastError(QString());
        return false;
    }
    setLastError(QString());
    return true;
}

void SideStakeListModel::refresh()
{
    if (m_table) m_table->refresh();
}

void SideStakeListModel::setLastError(const QString& text)
{
    if (m_last_error == text) return;
    m_last_error = text;
    emit lastErrorChanged();
}

QString SideStakeListModel::editStatusText(const QString& address) const
{
    // EditSideStakeDialog::accept's texts.
    switch (m_table->getEditStatus()) {
    case SideStakeTableModel::INVALID_ADDRESS:
        return QCoreApplication::translate("EditSideStakeDialog", "The entered address \"%1\" is not "
                                                                  "a valid Gridcoin address.")
            .arg(address);
    case SideStakeTableModel::DUPLICATE_ADDRESS:
        return QCoreApplication::translate("EditSideStakeDialog", "The entered address \"%1\" already "
                                                                  "has a local sidestake entry.")
            .arg(address);
    case SideStakeTableModel::INVALID_ALLOCATION:
        return QCoreApplication::translate("EditSideStakeDialog",
                                           "The entered allocation is not valid. Check to make sure that the "
                                           "allocation is greater than zero and when added to the other "
                                           "allocations totals less than 100.");
    case SideStakeTableModel::INVALID_DESCRIPTION:
        return QCoreApplication::translate("EditSideStakeDialog",
                                           "The entered description is not valid. Check to make sure that the "
                                           "description only contains letters, numbers, spaces, periods, or "
                                           "underscores.");
    case SideStakeTableModel::OK:
    case SideStakeTableModel::NO_CHANGES:
        break;
    }
    return QString();
}

void SideStakeListModel::refuseMandatory()
{
    setLastError(QCoreApplication::translate("OptionsDialog", "A mandatory sidestake cannot be edited or deleted."));
}
