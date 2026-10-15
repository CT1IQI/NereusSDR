// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/OrderKeepingProxyModel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// A QSortFilterProxyModel that can take a real model reset and come back
// with its rows in the order it showed before. A plain reset makes the
// proxy sort its rows again from scratch, and rows that tie on the sort key
// then come back in source order, while rows that arrived one by one were
// placed by arrival. The Spot List sorts on a key every spot ties on, so a
// plain reset would turn the list over. resetKeepingOrder() keeps the order
// the user sees; the filters and the sort column stay as they are.
//
// Used by the accessibility-safe refresh of the Spot List and the FreeDV
// Reporter station table (resetViewAfterRowChanges in
// gui/QtCocoaAccessibleTableCache.h).
//
// Modification history (NereusSDR):
//   2026-10-15  J.J. Boyd (KG4VCF)  Created. AI-assisted via Anthropic
//                                    Claude Code.

#pragma once

#include <QHash>
#include <QModelIndex>
#include <QSortFilterProxyModel>

namespace NereusSDR {

class OrderKeepingProxyModel : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    // A real reset (every index this proxy gave out becomes invalid) with
    // the same rows, in the same order.
    void resetKeepingOrder()
    {
        QHash<int, int> shownAt;
        const int rows = rowCount();
        shownAt.reserve(rows);
        for (int row = 0; row < rows; ++row) {
            shownAt.insert(mapToSource(index(row, 0)).row(), row);
        }
        // The views rebuild this proxy's rows from inside endResetModel, so
        // the order is held until it returns.
        m_shownAt = std::move(shownAt);
        m_keepingOrder = true;
        beginResetModel();
        endResetModel();
        static_cast<void>(rowCount());
        m_keepingOrder = false;
        m_shownAt.clear();
    }

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        if (!m_keepingOrder) {
            return QSortFilterProxyModel::lessThan(left, right);
        }
        // Qt sorts descending with lessThan(right, left), so the comparison
        // turns over with the order to give the shown order both ways.
        const int l = m_shownAt.value(left.row(), -1);
        const int r = m_shownAt.value(right.row(), -1);
        return sortOrder() == Qt::AscendingOrder ? l < r : l > r;
    }

private:
    QHash<int, int> m_shownAt;
    bool m_keepingOrder{false};
};

} // namespace NereusSDR
