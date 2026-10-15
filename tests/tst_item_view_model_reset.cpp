// SPDX-License-Identifier: GPL-3.0-or-later
//
// resetItemsBeforeRowChange (gui/QtCocoaAccessibleTableCache.h): with
// accessibility on, an item view's rows change only after a real model
// reset, so QAccessibleTable::modelChange never reads a cell interface the
// macOS bridge has already deleted (itemviews.cpp:645-741).
//
// Each case checks that the reset keeps everything the user sees and the
// code reads: the items, their order, data in every role, flags, the
// current row and the selection, and that the view itself sends no signal
// (no currentIndexChanged, activated, itemSelectionChanged). The model
// sends one modelReset. A last case per view deletes the walked cells the
// way the Cocoa bridge does, then resets and removes a row: no crash. The
// helpers act on macOS only; ExpiredAccessibleCells::setActive lets them
// act on every platform so Linux CI runs these cases too. One case checks
// that OrderKeepingProxyModel does no extra filter work while its gate
// (itemViewResetsActive in the app) is false.

#include <QtTest>
#include <QAccessible>
#include <QComboBox>
#include <QHeaderView>
#include <QListWidget>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTreeWidget>

#include "ExpiredAccessibleCells.h"
#include "gui/QtCocoaAccessibleTableCache.h"
#include "models/OrderKeepingProxyModel.h"

using namespace NereusSDR;

namespace {

constexpr int kExtraRole = Qt::UserRole + 7;

void fillCombo(QComboBox& combo)
{
    combo.addItem(QStringLiteral("Alpha"), 11);
    combo.addItem(QStringLiteral("Bravo"), 22);
    combo.addItem(QStringLiteral("Charlie"), 33);
    auto* model = qobject_cast<QStandardItemModel*>(combo.model());
    model->item(1)->setToolTip(QStringLiteral("second"));
    model->item(1)->setData(QStringLiteral("extra"), kExtraRole);
    model->item(2)->setFlags(model->item(2)->flags() & ~Qt::ItemIsEnabled);
}

// Counts its filter calls, to show what a data change costs.
class CountingProxy : public OrderKeepingProxyModel {
public:
    using OrderKeepingProxyModel::OrderKeepingProxyModel;
    mutable int filterCalls{0};

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        ++filterCalls;
        return OrderKeepingProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }
};

} // namespace

class TestItemViewModelReset : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        ExpiredAccessibleCells::setActive(true);
        QVERIFY2(QAccessible::isActive(),
                 "accessibility must be active so item views send table model changes");
    }

    void cleanupTestCase() { ExpiredAccessibleCells::setActive(false); }

    void comboKeepsItemsCurrentAndSendsNoComboSignal()
    {
        QComboBox combo;
        fillCombo(combo);
        combo.setCurrentIndex(1);
        auto* model = qobject_cast<QStandardItemModel*>(combo.model());

        QSignalSpy indexSpy(&combo, &QComboBox::currentIndexChanged);
        QSignalSpy textSpy(&combo, &QComboBox::currentTextChanged);
        QSignalSpy activatedSpy(&combo, &QComboBox::activated);
        QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);
        QSignalSpy removedSpy(model, &QAbstractItemModel::rowsRemoved);

        QVERIFY(resetItemsBeforeRowChange(&combo));

        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(removedSpy.count(), 0);
        QCOMPARE(indexSpy.count(), 0);
        QCOMPARE(textSpy.count(), 0);
        QCOMPARE(activatedSpy.count(), 0);
        QCOMPARE(combo.count(), 3);
        QCOMPARE(combo.currentIndex(), 1);
        QCOMPARE(combo.currentText(), QStringLiteral("Bravo"));
        QCOMPARE(combo.itemText(0), QStringLiteral("Alpha"));
        QCOMPARE(combo.itemText(2), QStringLiteral("Charlie"));
        QCOMPARE(combo.itemData(0).toInt(), 11);
        QCOMPARE(combo.itemData(2).toInt(), 33);
        QCOMPARE(combo.itemData(1, Qt::ToolTipRole).toString(), QStringLiteral("second"));
        QCOMPARE(combo.itemData(1, kExtraRole).toString(), QStringLiteral("extra"));
        QVERIFY(!(model->item(2)->flags() & Qt::ItemIsEnabled));
        QVERIFY(model->item(0)->flags() & Qt::ItemIsEnabled);
    }

    void editableComboKeepsTypedText()
    {
        QComboBox combo;
        combo.setEditable(true);
        fillCombo(combo);
        combo.setCurrentIndex(0);
        combo.setEditText(QStringLiteral("typed"));
        QSignalSpy editSpy(&combo, &QComboBox::editTextChanged);

        QVERIFY(resetItemsBeforeRowChange(&combo));

        QCOMPARE(combo.currentText(), QStringLiteral("typed"));
        QCOMPARE(combo.currentIndex(), 0);
        QCOMPARE(editSpy.count(), 0);
    }

    void comboWithNoCurrentStaysWithout()
    {
        QComboBox combo;
        fillCombo(combo);
        combo.setCurrentIndex(-1);
        QVERIFY(resetItemsBeforeRowChange(&combo));
        QCOMPARE(combo.currentIndex(), -1);
        QCOMPARE(combo.count(), 3);
    }

    void comboSurvivesExpiredCellsThenClear()
    {
        QComboBox combo;
        fillCombo(combo);
        QVERIFY(ExpiredAccessibleCells::expire(combo.view()) > 0);
        QVERIFY(resetItemsBeforeRowChange(&combo));
        combo.clear();
        combo.addItem(QStringLiteral("Delta"));
        QCOMPARE(combo.count(), 1);
    }

    void tableKeepsCellsSelectionCurrentAndHeaders()
    {
        QTableWidget table(3, 2);
        table.setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Value")});
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 2; ++column) {
                auto* item = new QTableWidgetItem(QStringLiteral("r%1c%2").arg(row).arg(column));
                item->setData(kExtraRole, row * 10 + column);
                table.setItem(row, column, item);
            }
        }
        table.item(1, 1)->setFlags(Qt::ItemIsEnabled);
        table.item(2, 0)->setToolTip(QStringLiteral("tip"));
        table.setSelectionBehavior(QAbstractItemView::SelectRows);
        table.selectRow(2);
        table.setCurrentCell(2, 1, QItemSelectionModel::NoUpdate);

        QSignalSpy selSpy(&table, &QTableWidget::itemSelectionChanged);
        QSignalSpy curSpy(&table, &QTableWidget::currentCellChanged);
        QSignalSpy resetSpy(table.model(), &QAbstractItemModel::modelReset);
        QSignalSpy removedSpy(table.model(), &QAbstractItemModel::rowsRemoved);

        QVERIFY(resetItemsBeforeRowChange(&table));

        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(removedSpy.count(), 0);
        QCOMPARE(selSpy.count(), 0);
        QCOMPARE(curSpy.count(), 0);
        QCOMPARE(table.rowCount(), 3);
        QCOMPARE(table.columnCount(), 2);
        QCOMPARE(table.horizontalHeaderItem(1)->text(), QStringLiteral("Value"));
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 2; ++column) {
                QCOMPARE(table.item(row, column)->text(), QStringLiteral("r%1c%2").arg(row).arg(column));
                QCOMPARE(table.item(row, column)->data(kExtraRole).toInt(), row * 10 + column);
            }
        }
        QCOMPARE(table.item(1, 1)->flags(), Qt::ItemFlags(Qt::ItemIsEnabled));
        QCOMPARE(table.item(2, 0)->toolTip(), QStringLiteral("tip"));
        QCOMPARE(table.currentRow(), 2);
        QCOMPARE(table.currentColumn(), 1);
        QVERIFY(table.item(2, 0)->isSelected());
        QVERIFY(table.item(2, 1)->isSelected());
        QVERIFY(!table.item(0, 0)->isSelected());
    }

    void sortedTableKeepsRowOrder()
    {
        QTableWidget table(3, 1);
        table.setItem(0, 0, new QTableWidgetItem(QStringLiteral("b")));
        table.setItem(1, 0, new QTableWidgetItem(QStringLiteral("a")));
        table.setItem(2, 0, new QTableWidgetItem(QStringLiteral("c")));
        table.setSortingEnabled(true);
        table.sortByColumn(0, Qt::DescendingOrder);
        QCOMPARE(table.item(0, 0)->text(), QStringLiteral("c"));

        QVERIFY(resetItemsBeforeRowChange(&table));

        QVERIFY(table.isSortingEnabled());
        QCOMPARE(table.item(0, 0)->text(), QStringLiteral("c"));
        QCOMPARE(table.item(1, 0)->text(), QStringLiteral("b"));
        QCOMPARE(table.item(2, 0)->text(), QStringLiteral("a"));
    }

    void tableSurvivesExpiredCellsThenRowChanges()
    {
        QTableWidget table(3, 2);
        QVERIFY(ExpiredAccessibleCells::expire(&table) > 0);
        QVERIFY(resetItemsBeforeRowChange(&table));
        table.removeRow(0);
        QVERIFY(ExpiredAccessibleCells::expire(&table) > 0);
        QVERIFY(resetItemsBeforeRowChange(&table));
        table.insertRow(0);
        QCOMPARE(table.rowCount(), 3);
    }

    void listKeepsItemsHiddenSelectionAndCurrent()
    {
        QListWidget list;
        list.setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (int i = 0; i < 4; ++i) {
            auto* item = new QListWidgetItem(QStringLiteral("line %1").arg(i), &list);
            item->setData(kExtraRole, i);
        }
        list.item(3)->setHidden(true);
        list.item(0)->setSelected(true);
        list.item(2)->setSelected(true);
        list.setCurrentRow(2, QItemSelectionModel::NoUpdate);

        QSignalSpy selSpy(&list, &QListWidget::itemSelectionChanged);
        QSignalSpy curSpy(&list, &QListWidget::currentRowChanged);
        QSignalSpy resetSpy(list.model(), &QAbstractItemModel::modelReset);

        QVERIFY(resetItemsBeforeRowChange(&list));

        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(selSpy.count(), 0);
        QCOMPARE(curSpy.count(), 0);
        QCOMPARE(list.count(), 4);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(list.item(i)->text(), QStringLiteral("line %1").arg(i));
            QCOMPARE(list.item(i)->data(kExtraRole).toInt(), i);
        }
        QVERIFY(list.item(3)->isHidden());
        QVERIFY(list.item(0)->isSelected());
        QVERIFY(!list.item(1)->isSelected());
        QVERIFY(list.item(2)->isSelected());
        QCOMPARE(list.currentRow(), 2);
    }

    void listSurvivesExpiredCellsThenTrim()
    {
        QListWidget list;
        list.addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
        QVERIFY(ExpiredAccessibleCells::expire(&list) > 0);
        QVERIFY(resetItemsBeforeRowChange(&list));
        delete list.takeItem(0);
        list.addItem(QStringLiteral("d"));
        QCOMPARE(list.count(), 3);
    }

    // A log of plain text lines (the HL2 I2C log) comes back in one
    // insert, one table change for the bridge instead of one per line; a
    // line with any other role or flag takes the clone path.
    void plainTextListComesBackInOneInsert()
    {
        QListWidget list;
        for (int i = 0; i < 200; ++i) {
            list.addItem(QStringLiteral("line %1").arg(i));
        }
        list.item(7)->setHidden(true);
        list.setCurrentRow(150);
        QSignalSpy inserts(list.model(), &QAbstractItemModel::rowsInserted);

        QVERIFY(resetItemsBeforeRowChange(&list));

        QCOMPARE(inserts.count(), 1);
        QCOMPARE(list.count(), 200);
        QCOMPARE(list.item(199)->text(), QStringLiteral("line 199"));
        QVERIFY(list.item(7)->isHidden());
        QCOMPARE(list.currentRow(), 150);

        list.item(3)->setData(kExtraRole, 3);
        inserts.clear();
        QVERIFY(resetItemsBeforeRowChange(&list));
        QCOMPARE(inserts.count(), 200);
        QCOMPARE(list.item(3)->data(kExtraRole).toInt(), 3);
        QCOMPARE(list.item(4)->text(), QStringLiteral("line 4"));
    }

    // The resets act on macOS only; elsewhere (unless a test forces them)
    // every helper is a pass-through even with accessibility on.
    void actsOnMacOnlyUnlessForced()
    {
        QListWidget list;
        list.addItem(QStringLiteral("a"));
        setItemViewResetsOnEveryPlatformForTest(false);
#if defined(Q_OS_MAC)
        const bool expected = true;
#else
        const bool expected = false;
#endif
        QCOMPARE(itemViewResetsActive(), expected);
        QCOMPARE(resetItemsBeforeRowChange(&list), expected);
        setItemViewResetsOnEveryPlatformForTest(true);
        QVERIFY(itemViewResetsActive());
        QVERIFY(resetItemsBeforeRowChange(&list));
    }

    void treeKeepsChildrenExpansionSelectionAndCurrent()
    {
        QTreeWidget tree;
        tree.setColumnCount(2);
        auto* groupA = new QTreeWidgetItem(&tree, {QStringLiteral("Group A")});
        groupA->setFirstColumnSpanned(true);
        auto* a1 = new QTreeWidgetItem(groupA, {QStringLiteral("a1"), QStringLiteral("x")});
        a1->setData(0, kExtraRole, QStringLiteral("key-a1"));
        new QTreeWidgetItem(groupA, {QStringLiteral("a2"), QStringLiteral("y")});
        auto* groupB = new QTreeWidgetItem(&tree, {QStringLiteral("Group B")});
        auto* b1 = new QTreeWidgetItem(groupB, {QStringLiteral("b1"), QStringLiteral("z")});
        b1->setFlags(Qt::ItemIsEnabled);
        groupA->setExpanded(true);
        groupB->setExpanded(false);
        groupA->child(1)->setHidden(true);
        tree.setCurrentItem(a1, 1);

        QSignalSpy curSpy(&tree, &QTreeWidget::currentItemChanged);
        QSignalSpy selSpy(&tree, &QTreeWidget::itemSelectionChanged);
        QSignalSpy resetSpy(tree.model(), &QAbstractItemModel::modelReset);

        QVERIFY(resetItemsBeforeRowChange(&tree));

        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(curSpy.count(), 0);
        QCOMPARE(selSpy.count(), 0);
        QCOMPARE(tree.topLevelItemCount(), 2);
        QTreeWidgetItem* newA = tree.topLevelItem(0);
        QTreeWidgetItem* newB = tree.topLevelItem(1);
        QCOMPARE(newA->text(0), QStringLiteral("Group A"));
        QVERIFY(newA->isFirstColumnSpanned());
        QVERIFY(newA->isExpanded());
        QVERIFY(!newB->isExpanded());
        QCOMPARE(newA->childCount(), 2);
        QVERIFY(newA->child(1)->isHidden());
        QCOMPARE(newA->child(0)->data(0, kExtraRole).toString(), QStringLiteral("key-a1"));
        QCOMPARE(newA->child(0)->text(1), QStringLiteral("x"));
        QCOMPARE(newB->child(0)->flags(), Qt::ItemFlags(Qt::ItemIsEnabled));
        QCOMPARE(tree.currentItem(), newA->child(0));
        QCOMPARE(tree.currentColumn(), 1);
        QVERIFY(newA->child(0)->isSelected());
        QVERIFY(!newA->isSelected());
    }

    void treeSurvivesExpiredCellsThenRowChanges()
    {
        QTreeWidget tree;
        new QTreeWidgetItem(&tree, {QStringLiteral("one")});
        new QTreeWidgetItem(&tree, {QStringLiteral("two")});
        QVERIFY(ExpiredAccessibleCells::expire(&tree) > 0);
        QVERIFY(resetItemsBeforeRowChange(&tree));
        delete tree.takeTopLevelItem(0);
        new QTreeWidgetItem(&tree, {QStringLiteral("three")});
        QCOMPARE(tree.topLevelItemCount(), 2);
    }

    // OrderKeepingProxyModel with its gate false (accessibility off): a
    // data change costs the filter calls a plain proxy makes and nothing
    // more, and the flush is never called. With the gate true it checks
    // each changed row once more to flush before a row it will hide.
    void orderKeepingProxyGateSkipsExtraWork()
    {
        QStandardItemModel source(20, 1);
        for (int row = 0; row < 20; ++row) {
            source.setItem(row, new QStandardItem(QString::number(row)));
        }
        const auto filterCallsForDataChange = [&source](bool gate, int* flushes) {
            CountingProxy proxy;
            proxy.setSourceModel(&source);
            proxy.setBeforeRowChange([flushes] { ++*flushes; }, [gate] { return gate; });
            static_cast<void>(proxy.rowCount());
            proxy.filterCalls = 0;
            emit source.dataChanged(source.index(0, 0), source.index(19, 0));
            proxy.prepareForRowChange();
            return proxy.filterCalls;
        };
        int plainCalls = 0;
        {
            CountingProxy plain;
            plain.setSourceModel(&source);
            static_cast<void>(plain.rowCount());
            plain.filterCalls = 0;
            emit source.dataChanged(source.index(0, 0), source.index(19, 0));
            plainCalls = plain.filterCalls;
        }
        int offFlushes = 0;
        int onFlushes = 0;
        QCOMPARE(filterCallsForDataChange(false, &offFlushes), plainCalls);
        QCOMPARE(offFlushes, 0);
        QCOMPARE(filterCallsForDataChange(true, &onFlushes), plainCalls + 20);
        QCOMPARE(onFlushes, 1);
    }

    void doesNothingWithAccessibilityOff()
    {
        ExpiredAccessibleCells::setActive(false);
        QComboBox combo;
        fillCombo(combo);
        QTableWidget table(2, 2);
        QListWidget list;
        list.addItem(QStringLiteral("a"));
        QTreeWidget tree;
        new QTreeWidgetItem(&tree, {QStringLiteral("a")});
        QSignalSpy resetSpy(combo.model(), &QAbstractItemModel::modelReset);
        const bool combos = resetItemsBeforeRowChange(&combo);
        const bool tables = resetItemsBeforeRowChange(&table);
        const bool lists = resetItemsBeforeRowChange(&list);
        const bool trees = resetItemsBeforeRowChange(&tree);
        ExpiredAccessibleCells::setActive(true);
        QVERIFY(!combos);
        QVERIFY(!tables);
        QVERIFY(!lists);
        QVERIFY(!trees);
        QCOMPARE(resetSpy.count(), 0);
    }
};

QTEST_MAIN(TestItemViewModelReset)
#include "tst_item_view_model_reset.moc"
