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

#pragma once

#include "gui/NereusGuiExport.h"

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

// The older repair, for views whose model is the app's own and must not be
// reset (the Spot List and FreeDV Reporter tables keep their selection and
// persistent indexes as rows come and go). Qt 6.11.0 Cocoa deletes the real
// cell interfaces of expired native rows, but QAccessibleTable keeps their
// IDs and dereferences them on the next RowsInserted or RowsRemoved. Call
// once after the view has its model. Before every row or column insert or
// removal that model announces, on Qt 6.11.0 Cocoa with accessibility
// active, the view's accessible child cache is cleared through the public
// table-interface ModelReset handler. The model, selection and persistent
// indexes are untouched. Other Qt versions and platforms are unaffected.
void installQtCocoaAccessibleTableCacheRepair(QAbstractItemView* view);

// Test seam: true applies the repair on any platform, so an offscreen test
// that deletes cell interfaces the way that Cocoa image does exercises it.
// Process-wide; tests set it back to false.
void setQtCocoaAccessibleTableCacheRepairForcedForTest(bool forced);

} // namespace NereusSDR
