// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - see QtCocoaAccessibleTableCache.h.
//
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Shared from the Spot List crash fix.
//                                    AI tooling: Anthropic Claude Code.
//   2026-10-15  J.J. Boyd (KG4VCF)  resetItemsBeforeRowChange for combos,
//                                    tables, lists and trees.
//                                    AI-assisted via Anthropic Claude Code.

#include "QtCocoaAccessibleTableCache.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAccessible>
#include <QComboBox>
#include <QGuiApplication>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTreeWidget>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

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

// The (row, column) of each selected index, to select again after a reset.
QList<std::pair<int, int>> selectedCells(const QItemSelectionModel* selection)
{
    QList<std::pair<int, int>> cells;
    if (selection == nullptr) {
        return cells;
    }
    const QModelIndexList indexes = selection->selectedIndexes();
    for (const QModelIndex& index : indexes) {
        cells.append({index.row(), index.column()});
    }
    return cells;
}

// Selects the cells again and makes (currentRow, currentColumn) current,
// without the selection model announcing either (its signals are blocked by
// the caller).
void restoreSelection(QAbstractItemView* view, const QList<std::pair<int, int>>& cells,
                      int currentRow, int currentColumn)
{
    QItemSelectionModel* selection = view->selectionModel();
    QAbstractItemModel* model = view->model();
    if (selection == nullptr || model == nullptr) {
        return;
    }
    QItemSelection restored;
    for (const auto& [row, column] : cells) {
        const QModelIndex index = model->index(row, column, view->rootIndex());
        if (index.isValid()) {
            restored.select(index, index);
        }
    }
    if (!restored.isEmpty()) {
        selection->select(restored, QItemSelectionModel::Select);
    }
    const QModelIndex current = model->index(currentRow, currentColumn, view->rootIndex());
    if (current.isValid()) {
        selection->setCurrentIndex(current, QItemSelectionModel::NoUpdate);
    }
    view->viewport()->update();
}

// A tree item's view state, recorded in pre-order so a deep clone can take
// it back in the same order.
struct TreeRowState {
    bool expanded{false};
    bool hidden{false};
    bool spanned{false};
    bool selected{false};
    bool current{false};
};

void recordTreeRows(const QTreeWidgetItem* item, const QTreeWidgetItem* current,
                    std::vector<TreeRowState>& out)
{
    out.push_back({item->isExpanded(), item->isHidden(), item->isFirstColumnSpanned(),
                   item->isSelected(), item == current});
    for (int i = 0; i < item->childCount(); ++i) {
        recordTreeRows(item->child(i), current, out);
    }
}

void restoreTreeRows(QTreeWidgetItem* item, const std::vector<TreeRowState>& states,
                     std::size_t& at, QTreeWidgetItem*& current)
{
    if (at >= states.size()) {
        return;
    }
    const TreeRowState& state = states.at(at++);
    if (item->isFirstColumnSpanned() != state.spanned) {
        item->setFirstColumnSpanned(state.spanned);
    }
    if (item->isHidden() != state.hidden) {
        item->setHidden(state.hidden);
    }
    if (item->isExpanded() != state.expanded) {
        item->setExpanded(state.expanded);
    }
    if (state.selected) {
        item->setSelected(true);
    }
    if (state.current) {
        current = item;
    }
    for (int i = 0; i < item->childCount(); ++i) {
        restoreTreeRows(item->child(i), states, at, current);
    }
}

} // namespace

bool resetItemsBeforeRowChange(QComboBox* combo)
{
    if (combo == nullptr || !QAccessible::isActive()) {
        return false;
    }
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    if (model == nullptr || combo->rootModelIndex().isValid()) {
        return false;
    }
    const QSignalBlocker comboBlock(combo);
    QLineEdit* const edit = combo->lineEdit();
    const QSignalBlocker editBlock(edit);
    const QString editText = edit != nullptr ? edit->text() : QString();
    const int current = combo->currentIndex();
    const int rows = model->rowCount();
    const int columns = model->columnCount();

    std::vector<std::vector<std::unique_ptr<QStandardItem>>> items(static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        auto& cells = items[static_cast<std::size_t>(row)];
        for (int column = 0; column < columns; ++column) {
            const QStandardItem* item = model->item(row, column);
            cells.emplace_back(item != nullptr ? item->clone() : nullptr);
        }
    }

    // The reset (qstandarditemmodel.cpp:2264-2275): the cells' indexes are
    // invalid before the popup list drops its accessibility cache.
    model->clear();
    model->setColumnCount(columns);
    for (auto& cells : items) {
        QList<QStandardItem*> row;
        for (auto& cell : cells) {
            row.append(cell.release());
        }
        model->appendRow(row);
    }
    combo->setCurrentIndex(current);
    if (edit != nullptr && edit->text() != editText) {
        edit->setText(editText);
    }
    return true;
}

bool resetItemsBeforeRowChange(QTableWidget* table)
{
    if (table == nullptr || !QAccessible::isActive()) {
        return false;
    }
    const QSignalBlocker tableBlock(table);
    QItemSelectionModel* const selection = table->selectionModel();
    const QSignalBlocker selectionBlock(selection);
    const QModelIndex current = table->currentIndex();
    const QList<std::pair<int, int>> selected = selectedCells(selection);
    // Items set back while sorting is on would move to their sorted rows.
    const bool sorting = table->isSortingEnabled();
    if (sorting) {
        table->setSortingEnabled(false);
    }

    const int rows = table->rowCount();
    const int columns = table->columnCount();
    std::vector<std::unique_ptr<QTableWidgetItem>> items;
    items.reserve(static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            items.emplace_back(table->takeItem(row, column));
        }
    }
    // QTableWidget::clearContents is the reset (qtablewidget.cpp:768-779);
    // the row and column counts and the header items stay.
    table->clearContents();
    std::size_t at = 0;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            if (std::unique_ptr<QTableWidgetItem>& item = items[at++]) {
                table->setItem(row, column, item.release());
            }
        }
    }

    if (sorting) {
        // Sorts again by the same column, which leaves the sorted rows as
        // they were.
        table->setSortingEnabled(true);
    }
    restoreSelection(table, selected, current.row(), current.column());
    return true;
}

bool resetItemsBeforeRowChange(QListWidget* list)
{
    if (list == nullptr || !QAccessible::isActive()) {
        return false;
    }
    const QSignalBlocker listBlock(list);
    QItemSelectionModel* const selection = list->selectionModel();
    const QSignalBlocker selectionBlock(selection);
    const QModelIndex current = list->currentIndex();
    const QList<std::pair<int, int>> selected = selectedCells(selection);
    const bool sorting = list->isSortingEnabled();
    list->setSortingEnabled(false);

    const int rows = list->count();
    std::vector<std::unique_ptr<QListWidgetItem>> items;
    std::vector<bool> hidden;
    items.reserve(static_cast<std::size_t>(rows));
    hidden.reserve(static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        items.emplace_back(list->item(row)->clone());
        hidden.push_back(list->item(row)->isHidden());
    }
    // QListWidget::clear is the reset (qlistwidget.cpp:36-48).
    list->clear();
    for (std::unique_ptr<QListWidgetItem>& item : items) {
        list->addItem(item.release());
    }
    for (int row = 0; row < rows; ++row) {
        if (hidden[static_cast<std::size_t>(row)]) {
            list->item(row)->setHidden(true);
        }
    }

    list->setSortingEnabled(sorting);
    restoreSelection(list, selected, current.row(), current.column());
    return true;
}

bool resetItemsBeforeRowChange(QTreeWidget* tree)
{
    if (tree == nullptr || !QAccessible::isActive()) {
        return false;
    }
    const QSignalBlocker treeBlock(tree);
    QItemSelectionModel* const selection = tree->selectionModel();
    const QSignalBlocker selectionBlock(selection);
    const bool sorting = tree->isSortingEnabled();
    if (sorting) {
        tree->setSortingEnabled(false);
    }

    const QTreeWidgetItem* const currentItem = tree->currentItem();
    const int currentColumn = tree->currentColumn();
    std::vector<TreeRowState> states;
    std::vector<std::unique_ptr<QTreeWidgetItem>> items;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = tree->topLevelItem(i);
        recordTreeRows(item, currentItem, states);
        items.emplace_back(item->clone());
    }
    // QTreeWidget::clear is the reset (qtreewidget.cpp:120-133).
    tree->clear();
    QList<QTreeWidgetItem*> topLevel;
    for (std::unique_ptr<QTreeWidgetItem>& item : items) {
        topLevel.append(item.release());
    }
    tree->addTopLevelItems(topLevel);

    std::size_t at = 0;
    QTreeWidgetItem* current = nullptr;
    for (QTreeWidgetItem* item : std::as_const(topLevel)) {
        restoreTreeRows(item, states, at, current);
    }
    if (current != nullptr) {
        tree->setCurrentItem(current, std::max(currentColumn, 0), QItemSelectionModel::NoUpdate);
    }
    if (sorting) {
        tree->setSortingEnabled(true);
    }
    tree->viewport()->update();
    return true;
}

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
