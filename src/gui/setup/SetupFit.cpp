// no-port-check: NereusSDR-original layout helpers for the Setup pages.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/setup/SetupFit.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See SetupFit.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "SetupFit.h"

#include <algorithm>

#include <QFrame>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QWidget>

namespace NereusSDR::SetupFit {

namespace {

// The spacing Qt uses when a layout leaves it to the style and the style
// leaves it to the controls (QStyle::PM_LayoutHorizontalSpacing of -1).
constexpr int kFallbackSpacingPx = 6;

// A hidden widget takes no room and no spacing; a spacer takes its own
// size and no spacing, as QBoxLayout counts them.
bool takesNoRoom(QLayoutItem* item)
{
    return item->isEmpty() && item->widget() != nullptr;
}

bool isSpacer(QLayoutItem* item)
{
    return item->spacerItem() != nullptr;
}

} // namespace

FitRow::FitRow(QWidget* parent)
    : QBoxLayout(QBoxLayout::LeftToRight, parent)
{
    setProperty(kFitRowStackedProperty, false);
}

int FitRow::horizontalSpacing() const
{
    // Stacked, spacing() answers for the vertical direction; an explicit
    // setSpacing() is the same both ways.
    const int own = spacing();
    return own >= 0 ? own : kFallbackSpacingPx;
}

int FitRow::sideBySideMinimumWidth() const
{
    int total = 0;
    int shown = 0;
    for (int i = 0; i < count(); ++i) {
        QLayoutItem* item = itemAt(i);
        if (item == nullptr || takesNoRoom(item)) {
            continue;
        }
        total += item->minimumSize().width();
        if (!isSpacer(item)) {
            ++shown;
        }
    }
    const QMargins m = contentsMargins();
    return total + horizontalSpacing() * std::max(0, shown - 1) + m.left() + m.right();
}

QSize FitRow::minimumSize() const
{
    int widest = 0;
    for (int i = 0; i < count(); ++i) {
        QLayoutItem* item = itemAt(i);
        if (item == nullptr || takesNoRoom(item) || isSpacer(item)) {
            continue;
        }
        widest = std::max(widest, item->minimumSize().width());
    }
    const QMargins m = contentsMargins();
    return QSize(widest + m.left() + m.right(), QBoxLayout::minimumSize().height());
}

Qt::Orientations FitRow::expandingDirections() const
{
    // Stacked, the groups keep their own height: what is left of the page
    // goes to its bottom, not into the gaps between groups.
    const Qt::Orientations own = QBoxLayout::expandingDirections();
    return isStacked() ? (own & ~Qt::Vertical) : own;
}

void FitRow::setGeometry(const QRect& rect)
{
    const bool stacked = rect.width() < sideBySideMinimumWidth();
    if (stacked != isStacked()) {
        arrange(stacked);
    }
    QBoxLayout::setGeometry(rect);
}

void FitRow::arrange(bool stacked)
{
    if (stacked) {
        m_sideBySideStretch.clear();
        for (int i = 0; i < count(); ++i) {
            m_sideBySideStretch.append(stretch(i));
        }
        setDirection(QBoxLayout::TopToBottom);
        for (int i = 0; i < count(); ++i) {
            setStretch(i, 0);
        }
    } else {
        setDirection(QBoxLayout::LeftToRight);
        for (int i = 0; i < count() && i < m_sideBySideStretch.size(); ++i) {
            setStretch(i, m_sideBySideStretch.at(i));
        }
    }
    setProperty(kFitRowStackedProperty, stacked);
}

FitGrid::FitGrid(QWidget* parent)
    : QGridLayout(parent)
{
    setProperty(kFitRowStackedProperty, false);
}

void FitGrid::addFitWidget(QWidget* widget, int row, int column, int rowSpan, int columnSpan,
                           int stackedRow, Qt::Alignment alignment)
{
    Placement p;
    p.widget = widget;
    p.row = row;
    p.column = column;
    p.rowSpan = rowSpan;
    p.columnSpan = columnSpan;
    p.stackedRow = stackedRow;
    p.alignment = alignment;
    m_placements.append(p);
    if (m_stacked) {
        addWidget(widget, stackedRow, 0, 1, 1, alignment);
    } else {
        addWidget(widget, row, column, rowSpan, columnSpan, alignment);
    }
}

QLayoutItem* FitGrid::itemFor(QWidget* widget) const
{
    const int index = indexOf(widget);
    return index >= 0 ? itemAt(index) : nullptr;
}

int FitGrid::sideBySideMinimumWidth() const
{
    int columns = 0;
    for (const Placement& p : m_placements) {
        columns = std::max(columns, p.column + p.columnSpan);
    }
    int total = 0;
    int shown = 0;
    for (int c = 0; c < columns; ++c) {
        int widest = columnMinimumWidth(c);
        bool any = false;
        for (const Placement& p : m_placements) {
            QLayoutItem* item = itemFor(p.widget);
            if (p.column != c || p.columnSpan != 1 || item == nullptr || takesNoRoom(item)) {
                continue;
            }
            widest = std::max(widest, item->minimumSize().width());
            any = true;
        }
        total += widest;
        if (any) {
            ++shown;
        }
    }
    const int own = horizontalSpacing();
    const int gap = own >= 0 ? own : kFallbackSpacingPx;
    const QMargins m = contentsMargins();
    return total + gap * std::max(0, shown - 1) + m.left() + m.right();
}

QSize FitGrid::minimumSize() const
{
    int widest = 0;
    for (const Placement& p : m_placements) {
        QLayoutItem* item = itemFor(p.widget);
        if (item != nullptr && !takesNoRoom(item)) {
            widest = std::max(widest, item->minimumSize().width());
        }
    }
    const QMargins m = contentsMargins();
    return QSize(widest + m.left() + m.right(), QGridLayout::minimumSize().height());
}

void FitGrid::setGeometry(const QRect& rect)
{
    const bool stacked = rect.width() < sideBySideMinimumWidth();
    if (stacked != m_stacked) {
        arrange(stacked);
    }
    QGridLayout::setGeometry(rect);
}

void FitGrid::arrange(bool stacked)
{
    m_stacked = stacked;
    for (const Placement& p : m_placements) {
        const int index = indexOf(p.widget);
        if (index < 0) {
            continue;
        }
        QLayoutItem* item = takeAt(index);
        if (stacked) {
            addItem(item, p.stackedRow, 0, 1, 1, p.alignment);
        } else {
            addItem(item, p.row, p.column, p.rowSpan, p.columnSpan, p.alignment);
        }
    }
    setProperty(kFitRowStackedProperty, stacked);
}

void widthAtLeast(QLabel* label, int px)
{
    // The label's own style sheet sets its font; measure with it.
    label->ensurePolished();
    label->setFixedWidth(std::max(px, label->sizeHint().width()));
}

QScrollArea* scrollingTab(QWidget* content)
{
    auto* area = new QScrollArea;
    area->setObjectName(QStringLiteral("setupTabScroll"));
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidget(content);
    // QScrollArea::setWidget() turns the content's own fill on; the tab
    // keeps the look it had, drawn by the tab pane behind it.
    content->setAutoFillBackground(false);
    area->setAutoFillBackground(false);
    area->viewport()->setAutoFillBackground(false);
    return area;
}

void showWhole(QScrollArea* area)
{
    area->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    area->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

SidewaysFrame::SidewaysFrame(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setProperty(kScrollsSidewaysProperty, true);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
}

bool SidewaysFrame::needsBar() const
{
    return widget() != nullptr && widget()->minimumSizeHint().width() > viewport()->width();
}

int SidewaysFrame::contentHeight() const
{
    QWidget* content = widget();
    if (content == nullptr) {
        return 0;
    }
    const int width = std::max(viewport()->width(), content->minimumSizeHint().width());
    int height = content->hasHeightForWidth() ? content->heightForWidth(width)
                                              : content->sizeHint().height();
    height = std::max(height, content->minimumSizeHint().height());
    if (needsBar()) {
        height += horizontalScrollBar()->sizeHint().height();
    }
    return height;
}

QSize SidewaysFrame::sizeHint() const
{
    const int width = widget() != nullptr ? widget()->sizeHint().width() : 0;
    return QSize(width, contentHeight());
}

QSize SidewaysFrame::minimumSizeHint() const
{
    return QSize(QScrollArea::minimumSizeHint().width(), contentHeight());
}

void SidewaysFrame::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);
    // The height follows the width: the content wraps differently and the
    // scroll bar comes and goes.
    if (event->oldSize().width() != event->size().width() || needsBar() != m_hadBar) {
        m_hadBar = needsBar();
        updateGeometry();
    }
}

} // namespace NereusSDR::SetupFit
