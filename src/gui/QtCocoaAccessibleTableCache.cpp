// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - see QtCocoaAccessibleTableCache.h.
//
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Shared from the Spot List crash fix.
//                                    AI tooling: Anthropic Claude Code.

#include "QtCocoaAccessibleTableCache.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAccessible>
#include <QGuiApplication>

namespace NereusSDR {

namespace {

bool g_repairForced = false;

// Qt 6.11.0 Cocoa expires the real cell interfaces of old native rows
// (qcocoaaccessibilityelement.mm:219-226,257-267,342-362: removeElementsFromCache:
// calls QAccessibleCache::deleteInterface), but QAccessibleTable keeps their
// IDs in its child cache and calls role() on the null interface during the
// next RowsInserted or RowsRemoved (itemviews.cpp:645-741). Same gate as
// ConnectionSelector, RxApplet and QtCocoaAccessibilityOwnershipGuard.
bool repairNeeded()
{
    if (g_repairForced) {
        return true;
    }
#if defined(Q_OS_MAC)
    return QGuiApplication::platformName() == QStringLiteral("cocoa")
        && qVersion() == QStringLiteral("6.11.0");
#else
    return false;
#endif
}

// Clears only the view's accessible child cache, through the public
// table-interface ModelReset handler.
void clearAccessibleChildCache(QAbstractItemView* view)
{
    if (!QAccessible::isActive() || !repairNeeded()) {
        return;
    }
    QAccessibleInterface* accessible = QAccessible::queryAccessibleInterface(view);
    if (accessible != nullptr && accessible->tableInterface() != nullptr) {
        QAccessibleTableModelChangeEvent reset(
            view, QAccessibleTableModelChangeEvent::ModelReset);
        accessible->tableInterface()->modelChange(&reset);
    }
}

} // namespace

void installQtCocoaAccessibleTableCacheRepair(QAbstractItemView* view)
{
    if (view == nullptr || view->model() == nullptr) {
        return;
    }
    QAbstractItemModel* model = view->model();
    const auto clear = [view] { clearAccessibleChildCache(view); };
    // The view is the context: the connections end with it.
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeInserted, view, clear);
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, view, clear);
    QObject::connect(model, &QAbstractItemModel::columnsAboutToBeInserted, view, clear);
    QObject::connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, view, clear);
}

void setQtCocoaAccessibleTableCacheRepairForcedForTest(bool forced)
{
    g_repairForced = forced;
}

} // namespace NereusSDR
