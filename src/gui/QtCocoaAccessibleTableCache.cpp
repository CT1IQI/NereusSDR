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
//   2026-10-15  J.J. Boyd (KG4VCF)  resetViewKeepingPlace and
//                                    resetViewAfterRowChanges; the
//                                    cache-only repair is removed.
//                                    AI-assisted via Anthropic Claude Code.

#include "QtCocoaAccessibleTableCache.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAccessible>
#include <QComboBox>
#include <QHash>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollBar>
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

namespace {

// The row at the top of the view's viewport, by key, with how far its top
// sits above the viewport's top edge, and the horizontal scroll.
struct TopPlace {
    QString key;
    int column{0};
    int offset{0};
    int horizontal{0};
};

TopPlace topPlaceOf(const QAbstractItemView* view, const ItemViewRowKey& rowKey)
{
    TopPlace place;
    place.horizontal = view->horizontalScrollBar()->value();
    const QModelIndex top = view->indexAt(QPoint(0, 0));
    if (top.isValid()) {
        place.key = rowKey(top.siblingAtColumn(0));
        place.column = top.column();
        place.offset = view->visualRect(top).top();
    }
    return place;
}

bool resetKeepingPlace(QAbstractItemView* view, const std::function<void()>& resetModel,
                       const ItemViewRowKey& rowKey, const TopPlace& topPlace)
{
    QAbstractItemModel* const model = view->model();
    const QModelIndex root = view->rootIndex();
    const auto keyOf = [&](int row) { return rowKey(model->index(row, 0, root)); };

    // The selection and current item as they are now: the model's own row
    // signals have already moved them with their rows.
    QItemSelectionModel* const selection = view->selectionModel();
    QList<std::pair<QString, int>> selected;
    if (selection != nullptr) {
        const QModelIndexList indexes = selection->selectedIndexes();
        for (const QModelIndex& index : indexes) {
            selected.append({keyOf(index.row()), index.column()});
        }
    }
    const QModelIndex current = view->currentIndex();
    const QString currentKey = current.isValid() ? keyOf(current.row()) : QString();
    const int currentColumn = current.column();

    QHash<QString, int> rowOf;
    {
        const QSignalBlocker viewBlock(view);
        const QSignalBlocker selectionBlock(selection);
        resetModel();

        const int rows = model->rowCount(root);
        rowOf.reserve(rows);
        for (int row = rows - 1; row >= 0; --row) {
            // The first row wins where two rows share a key.
            rowOf.insert(keyOf(row), row);
        }
        if (selection != nullptr) {
            QItemSelection restored;
            for (const auto& [key, column] : std::as_const(selected)) {
                const auto it = rowOf.constFind(key);
                if (it != rowOf.constEnd()) {
                    const QModelIndex index = model->index(it.value(), column, root);
                    restored.select(index, index);
                }
            }
            if (!restored.isEmpty()) {
                selection->select(restored, QItemSelectionModel::Select);
            }
            const auto it = currentKey.isNull() ? rowOf.constEnd() : rowOf.constFind(currentKey);
            if (it != rowOf.constEnd()) {
                selection->setCurrentIndex(model->index(it.value(), currentColumn, root),
                                           QItemSelectionModel::NoUpdate);
            }
        }
    }

    // The scroll range follows the new row count, then the same row goes
    // back to the top at the same offset. When that row is gone, the
    // scroll value stays as it is.
    view->doItemsLayout();
    const auto top = topPlace.key.isNull() ? rowOf.constEnd() : rowOf.constFind(topPlace.key);
    if (top != rowOf.constEnd()) {
        // The column that was at the left edge: shown, so scrollTo acts.
        const QModelIndex index = model->index(top.value(), topPlace.column, root);
        view->scrollTo(index, QAbstractItemView::PositionAtTop);
        if (view->verticalScrollMode() == QAbstractItemView::ScrollPerPixel) {
            QScrollBar* const vbar = view->verticalScrollBar();
            vbar->setValue(vbar->value() + view->visualRect(index).top() - topPlace.offset);
        }
    }
    view->horizontalScrollBar()->setValue(topPlace.horizontal);
    view->viewport()->update();
    return true;
}

} // namespace

bool resetViewKeepingPlace(QAbstractItemView* view, const std::function<void()>& resetModel,
                           const ItemViewRowKey& rowKey)
{
    if (view == nullptr || !resetModel || !rowKey || !QAccessible::isActive()
        || view->model() == nullptr) {
        return false;
    }
    return resetKeepingPlace(view, resetModel, rowKey, topPlaceOf(view, rowKey));
}

namespace {

// Holds a view's queued reset: one per event-loop pass. The top row is
// noted before the first change of the pass, so the reset can put back the
// row the user was looking at.
class RowChangeReset : public QObject {
public:
    RowChangeReset(QAbstractItemView* view, std::function<void()> resetModel, ItemViewRowKey rowKey)
        : QObject(view), m_view(view), m_resetModel(std::move(resetModel)),
          m_rowKey(std::move(rowKey))
    {
    }

    // Before rows move: note the top row once per pass.
    void noteTop()
    {
        if (m_topNoted || !QAccessible::isActive() || !m_resetModel || !m_rowKey) {
            return;
        }
        m_top = topPlaceOf(m_view, m_rowKey);
        m_topNoted = true;
    }

    // After a change: queue the one reset of this pass.
    void schedule()
    {
        if (m_pending || !QAccessible::isActive() || !m_resetModel || !m_rowKey) {
            return;
        }
        noteTop();
        m_pending = true;
        QMetaObject::invokeMethod(this, [this] { run(); }, Qt::QueuedConnection);
    }

private:
    void run()
    {
        m_pending = false;
        m_topNoted = false;
        if (QAccessible::isActive() && m_view->model() != nullptr) {
            resetKeepingPlace(m_view, m_resetModel, m_rowKey, m_top);
        }
    }

    QAbstractItemView* m_view;
    std::function<void()> m_resetModel;
    ItemViewRowKey m_rowKey;
    TopPlace m_top;
    bool m_topNoted{false};
    bool m_pending{false};
};

} // namespace

void resetViewAfterRowChanges(QAbstractItemView* view, std::function<void()> resetModel,
                              ItemViewRowKey rowKey)
{
    if (view == nullptr || view->model() == nullptr) {
        return;
    }
    QAbstractItemModel* const model = view->model();
    auto* reset = new RowChangeReset(view, std::move(resetModel), std::move(rowKey));
    const auto noteTop = [reset] { reset->noteTop(); };
    const auto schedule = [reset] { reset->schedule(); };
    // The reset object is the context: the connections end with the view.
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeInserted, reset, noteTop);
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, reset, noteTop);
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeMoved, reset, noteTop);
    QObject::connect(model, &QAbstractItemModel::columnsAboutToBeInserted, reset, noteTop);
    QObject::connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, reset, noteTop);
    QObject::connect(model, &QAbstractItemModel::layoutAboutToBeChanged, reset, noteTop);
    QObject::connect(model, &QAbstractItemModel::rowsInserted, reset, schedule);
    QObject::connect(model, &QAbstractItemModel::rowsRemoved, reset, schedule);
    QObject::connect(model, &QAbstractItemModel::rowsMoved, reset, schedule);
    QObject::connect(model, &QAbstractItemModel::columnsInserted, reset, schedule);
    QObject::connect(model, &QAbstractItemModel::columnsRemoved, reset, schedule);
    QObject::connect(model, &QAbstractItemModel::dataChanged, reset, schedule);
    QObject::connect(model, &QAbstractItemModel::layoutChanged, reset, schedule);
}

} // namespace NereusSDR
