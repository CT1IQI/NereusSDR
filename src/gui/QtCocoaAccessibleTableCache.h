// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - repair for the Qt 6.11.0 Cocoa accessibility cache of item
// views whose rows change while the app runs. NereusSDR-native; studies
// qtbase v6.11.0 (see docs/attribution/QT-PROVENANCE.md), no Qt code copied.
//
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Shared from the Spot List crash fix so
//                                    every live table, list, tree and
//                                    combo popup takes the same repair.
//                                    AI tooling: Anthropic Claude Code.

#pragma once

class QAbstractItemView;

namespace NereusSDR {

// Qt 6.11.0 Cocoa deletes the real cell interfaces of expired native rows,
// but QAccessibleTable keeps their IDs and dereferences them on the next
// RowsInserted or RowsRemoved. Call once after the view has its model (for a
// QComboBox, pass combo->view()). Before every row or column insert or
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
