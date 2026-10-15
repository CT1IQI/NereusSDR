// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - accessibility-safe row changes for item views whose rows
// change while the app runs. NereusSDR-native; studies qtbase v6.11.0 (see
// docs/attribution/QT-PROVENANCE.md), no Qt code copied.
//
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Shared from the Spot List crash fix so
//                                    every live table, list, tree and
//                                    combo popup takes the same repair.
//                                    AI tooling: Anthropic Claude Code.
//   2026-10-15  J.J. Boyd (KG4VCF)  resetItemsBeforeRowChange: a real model
//                                    reset, kept items and all, before a
//                                    view inserts or removes rows, so
//                                    accessibility cannot read a deleted
//                                    cell. Promoted from DeviceCard's
//                                    popup repair; every platform and Qt.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-15  J.J. Boyd (KG4VCF)  resetViewKeepingPlace and
//                                    resetViewAfterRowChanges for the Spot
//                                    List and FreeDV Reporter tables; the
//                                    cache-only repair is removed.
//                                    AI-assisted via Anthropic Claude Code.

#pragma once

#include "gui/NereusGuiExport.h"

#include <QString>

#include <functional>

class QModelIndex;

class QAbstractItemView;
class QComboBox;
class QListWidget;
class QTableWidget;
class QTreeWidget;

namespace NereusSDR {

// With accessibility on (VoiceOver, or any accessibility query), Qt's
// QAccessibleTable caches the cell interfaces of a view's rows, and the
// macOS bridge deletes those interfaces when its own native rows expire.
// The next row insertion or removal walks the cached ids and reads a
// deleted cell (Qt 6.11 QAccessibleTable::modelChange, itemviews.cpp:645-741).
// Clearing only that cache is not safe either: deleting still-valid cells
// lets the bridge delete an interface again from inside the deletion
// (qaccessiblecache.cpp:193-208; DeviceCard native report, 2026-10-04).
// A real model reset is safe: the model invalidates every cell's index
// first, then the view drops its whole accessibility cache, and rows
// inserted or removed right after it find the cache empty.
//
// resetItemsBeforeRowChange gives the view that reset with the same
// content, so call it right before the code that inserts or removes rows,
// and leave that code as it is. Kept through the reset: every row, every
// item with all its roles and flags, the current index and the selection
// (and a combo's edit text; a tree's expanded, hidden and spanned rows).
// The widget and its selection model are blocked while it runs, so no
// currentIndexChanged, activated, itemChanged or selection signal comes
// from it; the model itself announces the reset and the restored rows.
// Returns true when it reset the model: only while accessibility is
// active, on every platform and Qt version. Otherwise it does nothing.
//
// A combo: its model must be the default QStandardItemModel at the top
// level, as every combo in the app has; otherwise nothing is done.
NEREUS_GUI_EXPORT bool resetItemsBeforeRowChange(QComboBox* combo);
// A table keeps its QTableWidgetItem objects. Cell widgets are not kept:
// Qt's reset releases them, so a caller that has any rebuilds them when
// this returns true.
NEREUS_GUI_EXPORT bool resetItemsBeforeRowChange(QTableWidget* table);
// A list or tree keeps copies (QListWidgetItem::clone,
// QTreeWidgetItem::clone): the old item pointers are gone.
NEREUS_GUI_EXPORT bool resetItemsBeforeRowChange(QListWidget* list);
NEREUS_GUI_EXPORT bool resetItemsBeforeRowChange(QTreeWidget* tree);

// A view over the app's own model (a proxy over a live table: the Spot List
// and FreeDV Reporter stations). Rows there change from the model's own
// signals, not from code that can call a reset first, so the reset follows
// the change instead. The macOS bridge deletes the cells it holds only
// when control returns to the event loop after a table change (it releases
// its old native rows then: qcocoaaccessibilityelement.mm:257-267,
// updateTableModel), and Qt posts this reset in that same pass, so the
// next row change finds an empty accessibility cache.
//
// The row's identity, from the view's model at column 0 of the row: a
// stable id where the model has one. The place is kept by this key, never
// by row number.
using ItemViewRowKey = std::function<QString(const QModelIndex& row)>;

// A real reset of the view's model through resetModel (which must reset
// that model, keeping its rows and their order, for example
// OrderKeepingProxyModel::resetKeepingOrder), then
// the selection, the current item and the scroll position (the same top
// row, at the same offset) put back by rowKey. The view and its selection
// model are blocked meanwhile, so no selection or current signal comes
// from it. Returns true when it reset: only while accessibility is active.
NEREUS_GUI_EXPORT bool resetViewKeepingPlace(QAbstractItemView* view,
                                             const std::function<void()>& resetModel,
                                             const ItemViewRowKey& rowKey);

// Call once after the view has its model. With accessibility active, every
// row insert or removal, data change or layout change the model announces
// queues one resetViewKeepingPlace; changes in the same event-loop pass
// (a burst of spots) share it. The top row it puts back is the one shown
// before the first change of the pass, so a spot landing above does not
// push the user's place down. The model's own signals go out as before,
// and with accessibility off nothing changes.
NEREUS_GUI_EXPORT void resetViewAfterRowChanges(QAbstractItemView* view,
                                                std::function<void()> resetModel,
                                                ItemViewRowKey rowKey);

} // namespace NereusSDR
