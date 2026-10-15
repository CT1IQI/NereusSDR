// no-port-check: NereusSDR-original test helper.
//
// ExpiredAccessibleCells: the state that crashes an item view's next row
// change with accessibility on. Qt's macOS bridge deletes the cell
// interfaces of native rows it expires
// (qcocoaaccessibilityelement.mm:219-226,257-267,342-362) while the view's
// QAccessibleTable keeps their IDs; RowsInserted and RowsRemoved then read
// them (itemviews.cpp:645-741). Offscreen there is no bridge, so a test
// walks the cells the way an assistive client does and deletes them itself
// through the public QAccessible::deleteAccessibleInterface.
#pragma once

#include <QAccessible>
#include <QAbstractItemView>
#include <QList>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatformintegration.h>

namespace NereusSDR::ExpiredAccessibleCells {

// Turns the platform's accessibility on or off. Qt 6.11's
// QAccessible::setActive does that; Qt 6.8's (Linux CI) only tells the
// activation observers, so there the platform's own switch is set.
inline void setActive(bool active)
{
    QAccessible::setActive(active);
    if (QAccessible::isActive() == active) {
        return;
    }
    if (QPlatformAccessibility* platform = QGuiApplicationPrivate::platformIntegration()->accessibility()) {
        platform->setActive(active);
    }
}

// Walks the view's cells, then deletes them as the bridge does. Returns
// how many were deleted.
inline int expire(QAbstractItemView* view)
{
    QList<QAccessible::Id> ids;
    QAccessibleInterface* table = QAccessible::queryAccessibleInterface(view);
    if (table == nullptr) {
        return 0;
    }
    for (int i = 0; i < table->childCount(); ++i) {
        if (QAccessibleInterface* child = table->child(i)) {
            ids.append(QAccessible::uniqueId(child));
        }
    }
    for (QAccessible::Id id : std::as_const(ids)) {
        QAccessible::deleteAccessibleInterface(id);
    }
    return int(ids.size());
}

// Accessibility on for one test function, off again after.
class ActiveScope {
public:
    ActiveScope() { setActive(true); }
    ~ActiveScope() { setActive(false); }
    ActiveScope(const ActiveScope&) = delete;
    ActiveScope& operator=(const ActiveScope&) = delete;
};

} // namespace NereusSDR::ExpiredAccessibleCells
