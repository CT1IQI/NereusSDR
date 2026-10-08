// =================================================================
// src/gui/widgets/ProfileRailDock.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original collapsing wrapper around the ported
// ProfileRail. It ports no code. See ProfileRailDock.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/widgets/ProfileRailDock.h"

#include "gui/containers/ContainerWidget.h"
#include "gui/containers/WorkspaceProfiles.h"
#include "gui/styles/WorkspaceStyle.h"
#include "gui/widgets/PillStyle.h"
#include "gui/widgets/ProfileRail.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFocusEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>

namespace NereusSDR {

namespace {

// The Reveal timings of the container headers (Task 3), so the rail and
// the headers answer the pointer alike.
constexpr int kShowDelayMs = ContainerWidget::kRevealDelayMs;
constexpr int kHideDelayMs = ContainerWidget::kRevealHideMs;
constexpr int kShadowPx = ContainerWidget::kRevealShadowPx;
constexpr int kShadowAlpha = 150;
// Width for a badge's letter: the letter plus the pill's 11 px side
// padding and 1 px border on each side, rounded up as the rail does.
constexpr int kBadgeTextPad = 20;

bool isInside(const QWidget* w, const QWidget* container)
{
    for (const QWidget* p = w; p; p = p->parentWidget()) {
        if (p == container) {
            return true;
        }
    }
    return false;
}

} // namespace

ProfileRailDock::ProfileRailDock(WorkspaceProfiles* profiles, QWidget* anchor,
                                 QWidget* overlayParent, QWidget* parent)
    : QWidget(parent)
    , m_profiles(profiles)
    , m_anchor(anchor)
    , m_overlayParent(overlayParent)
{
    setObjectName(QStringLiteral("profileRailDock"));
    setAttribute(Qt::WA_TranslucentBackground);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    m_badge = new QPushButton(this);
    m_badge->setObjectName(QStringLiteral("profileRailCollapsed"));
    m_badge->setCheckable(true);
    m_badge->setChecked(true);
    m_badge->setCursor(Qt::PointingHandCursor);
    m_badge->setFocusPolicy(Qt::StrongFocus);
    m_badge->setContextMenuPolicy(Qt::CustomContextMenu);
    m_badge->setStyleSheet(PillStyle::pillStyle());
    m_badge->installEventFilter(this);
    row->addWidget(m_badge);

    // The popover: palette panel and border roles, radius 8, soft shadow.
    m_popover = new QFrame(overlayParent);
    m_popover->setObjectName(QStringLiteral("profileRailPopover"));
    m_popover->setAttribute(Qt::WA_StyledBackground, true);
    m_popover->setStyleSheet(
        QStringLiteral("#profileRailPopover { background: %1; border: 1px solid %2;"
                       " border-radius: %3px; }")
            .arg(Workspace::role("panel", "#0c0c0e"), Workspace::role("border", "#2c2c31"))
            .arg(kPopoverRadius));
    auto* shadow = new QGraphicsDropShadowEffect(m_popover);
    shadow->setBlurRadius(2 * kShadowPx);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, kShadowAlpha));
    m_popover->setGraphicsEffect(shadow);
    auto* inner = new QHBoxLayout(m_popover);
    inner->setContentsMargins(kPopoverPadH, kPopoverPadV, kPopoverPadH, kPopoverPadV);
    inner->setSpacing(0);
    m_rail = new ProfileRail(profiles, Qt::Horizontal, m_popover);
    m_rail->setObjectName(QStringLiteral("profileRail"));
    m_rail->setPlacementToggleOffered(false);
    inner->addWidget(m_rail);
    m_popover->installEventFilter(this);
    m_popover->hide();

    m_showTimer.setSingleShot(true);
    m_showTimer.setInterval(kShowDelayMs);
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(kHideDelayMs);
    connect(&m_showTimer, &QTimer::timeout, this, [this]() { open(false); });
    connect(&m_hideTimer, &QTimer::timeout, this, &ProfileRailDock::hideSoon);

    // A click opens it too (touch has no hover).
    connect(m_badge, &QPushButton::clicked, this, [this]() {
        m_badge->setChecked(true);
        m_showTimer.stop();
        open(false);
    });
    // Right-click on the collapsed badge: the rail's menu for the active
    // profile, from the fanned-out active badge.
    connect(m_badge, &QWidget::customContextMenuRequested, this, [this](const QPoint&) {
        if (!m_profiles) {
            return;
        }
        open(false);
        if (QPushButton* active = m_rail->badgeButton(m_profiles->activeId())) {
            emit active->customContextMenuRequested(QPoint(0, active->height()));
        }
    });

    if (m_profiles) {
        connect(m_profiles, &WorkspaceProfiles::profilesChanged, this, [this]() {
            refreshBadge();
            hookRailBadges();
        });
        connect(m_profiles, &WorkspaceProfiles::activeChanged, this, [this](const QString&) {
            refreshBadge();
            hookRailBadges();
            // Picked: close right away.
            close();
        });
    }
    if (m_anchor) {
        m_anchor->installEventFilter(this);
    }
    if (parent) {
        parent->installEventFilter(this);
    }
    if (m_overlayParent) {
        m_overlayParent->installEventFilter(this);
    }
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
        if (!isOpen() || !now || QApplication::activePopupWidget() || QApplication::activeModalWidget()) {
            return;
        }
        if (!isInside(now, m_popover) && now != m_badge) {
            close();
        }
    });
    refreshBadge();
    hookRailBadges();
}

ProfileRailDock::~ProfileRailDock()
{
    if (m_appFilter) {
        qApp->removeEventFilter(this);
    }
    if (m_popover) {
        m_popover->deleteLater();
    }
}

bool ProfileRailDock::isOpen() const
{
    return m_popover && m_popover->isVisible();
}

void ProfileRailDock::open(bool focusRail)
{
    if (!m_popover) {
        return;
    }
    m_hideTimer.stop();
    m_showTimer.stop();
    const bool was = isOpen();
    placePopover();
    m_popover->show();
    m_popover->raise();
    if (!m_appFilter) {
        qApp->installEventFilter(this);
        m_appFilter = true;
    }
    if (focusRail && m_profiles) {
        if (QPushButton* active = m_rail->badgeButton(m_profiles->activeId())) {
            active->setFocus(Qt::TabFocusReason);
        }
    }
    if (!was) {
        emit openChanged(true);
    }
}

void ProfileRailDock::close()
{
    m_showTimer.stop();
    m_hideTimer.stop();
    if (m_appFilter) {
        qApp->removeEventFilter(this);
        m_appFilter = false;
    }
    if (!isOpen()) {
        return;
    }
    const bool focusWasInside = isInside(QApplication::focusWidget(), m_popover);
    m_popover->hide();
    if (focusWasInside) {
        m_badge->setFocus(Qt::OtherFocusReason);
    }
    emit openChanged(false);
}

void ProfileRailDock::hideSoon()
{
    if (!isOpen()) {
        return;
    }
    // A menu or dialog from the rail (context menu, New menu, a name
    // prompt) keeps it open until it is done.
    if (QApplication::activePopupWidget() || QApplication::activeModalWidget()
        || m_popover->underMouse() || m_badge->underMouse()) {
        m_hideTimer.start();
        return;
    }
    close();
}

void ProfileRailDock::refreshBadge()
{
    const QString name = m_profiles ? m_profiles->name(m_profiles->activeId()) : QString();
    m_badge->setText(ProfileRail::initialFor(name));
    m_badge->setToolTip(name);
    m_badge->setAccessibleName(QStringLiteral("Workspace profile %1").arg(name));
    m_badge->setMinimumWidth(m_badge->fontMetrics().horizontalAdvance(m_badge->text()) + kBadgeTextPad);
    adjustSize();
    reposition();
}

void ProfileRailDock::hookRailBadges()
{
    if (!m_profiles) {
        return;
    }
    for (const QString& id : m_rail->badges()) {
        QPushButton* b = m_rail->badgeButton(id);
        if (!b || b->property("profileRailDockHooked").toBool()) {
            continue;
        }
        b->setProperty("profileRailDockHooked", true);
        // A pick of the profile that is already active changes nothing,
        // so activeChanged does not close the popover; this does.
        connect(b, &QPushButton::clicked, this, [this]() { close(); }, Qt::QueuedConnection);
    }
    if (isOpen()) {
        placePopover();
    }
}

void ProfileRailDock::reposition()
{
    QWidget* bar = parentWidget();
    if (!bar || !m_anchor) {
        return;
    }
    // Against the title TEXT, not the label box: the label may be wider
    // than its text.
    int textLeft = m_anchor->x();
    if (auto* label = qobject_cast<QLabel*>(m_anchor.data())) {
        const int textWidth = label->fontMetrics().horizontalAdvance(label->text());
        textLeft = m_anchor->x() + qMax(0, (m_anchor->width() - textWidth) / 2);
    }
    const QSize s = sizeHint();
    resize(s);
    move(textLeft - kTitleGap - s.width(), (bar->height() - s.height()) / 2);
    raise();
    if (isOpen()) {
        placePopover();
    }
}

void ProfileRailDock::placePopover()
{
    if (!m_popover || !m_overlayParent) {
        return;
    }
    m_popover->adjustSize();
    const QSize s = m_popover->sizeHint();
    const QPoint badgeTopLeft = m_badge->mapTo(m_overlayParent, QPoint(0, 0));
    const int right = badgeTopLeft.x() + m_badge->width();
    const int x = qMax(0, right - s.width());
    const int y = qMax(0, badgeTopLeft.y() - kPopoverPadV - 1);
    m_popover->setGeometry(x, y, s.width(), s.height());
}

bool ProfileRailDock::eventFilter(QObject* watched, QEvent* event)
{
    const QEvent::Type type = event->type();
    // While open this also filters the application, for Escape wherever
    // the focus is. The handlers below are idempotent, so seeing an
    // event twice (application, then object) is harmless.
    if (m_appFilter && type == QEvent::KeyPress && isOpen()
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape
        && !QApplication::activePopupWidget() && !QApplication::activeModalWidget()) {
        close();
        return true;
    }
    if (watched == m_badge) {
        if (type == QEvent::Enter) {
            m_hideTimer.stop();
            if (!isOpen()) {
                m_showTimer.start();
            }
        } else if (type == QEvent::Leave) {
            m_showTimer.stop();
            if (isOpen()) {
                m_hideTimer.start();
            }
        } else if (type == QEvent::FocusIn) {
            const auto reason = static_cast<QFocusEvent*>(event)->reason();
            if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason) {
                open(true);
            }
        }
    } else if (watched == m_popover) {
        if (type == QEvent::Enter) {
            m_hideTimer.stop();
        } else if (type == QEvent::Leave) {
            m_hideTimer.start();
        }
    } else if (watched == m_anchor || watched == parentWidget()) {
        if (type == QEvent::Move || type == QEvent::Resize || type == QEvent::Show
            || type == QEvent::LayoutRequest) {
            QTimer::singleShot(0, this, &ProfileRailDock::reposition);
        }
    } else if (watched == m_overlayParent) {
        if (type == QEvent::Resize && isOpen()) {
            QTimer::singleShot(0, this, &ProfileRailDock::placePopover);
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace NereusSDR
