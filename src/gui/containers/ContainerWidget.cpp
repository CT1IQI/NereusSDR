// =================================================================
// src/gui/containers/ContainerWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucMeter.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09 - Workspace preview only: the header options and source menus
//                 live on the heap under the container too, and no menu
//                 touches the container after an action removed it. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The right-click menu lives on the heap under the container
//                 (OwnedMenu.h): an action that removes the container while
//                 the menu is open no longer frees a menu on the stack (JJ's
//                 crash removing a popped-out container). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Workspace preview only: the content host learns how far the
//                 overlaid group header reaches into it, so an overlaid
//                 entry header sits below it. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The minimum width in every dock mode is the larger of
//                 260 px and what the content needs (setContentMinimumWidth),
//                 so a docked, overlaid or floating container never cuts its
//                 applets. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: the header's hide button hides the
//                 entry or container (Containers > Hidden brings it back).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: Arrange's outline and the Reveal
//                 shadow let clicks through as native views (macOS), so
//                 the header under them answers. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the source menu ticks the slice
//                 the chip shows, not the legacy rxSource key. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the header source chip opens the
//                 source menu (slices A to D, Active slice) and emits
//                 sourceRequested. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-02 — Effective contextual draft properties and portable settings by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-21: the receiver source names slices A to D (saved
//                 1..4, so a saved RX1 / RX2 reads as slice A / B), and an
//                 unavailable button's reason is relayed. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21 fix wave: a saved receiver value is
//                 clamped to slices A to D on load. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49: the title bar uses the native open / closed hand
//                 cursors instead of Qt's drawn four-way move cursor, which
//                 crashed Qt 6.11.0 on macOS. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-28 - The VFO display's filter right-click goes out as
//                 vfoFilterContextRequested. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-06 - Radio speaker plan Task 5 (R-SPK-19, D7): the pin-on-top
//                 button shows the app's own pin / pinned icons (AppIcon) in
//                 place of colour emoji text. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: the compact group header in place
//                 of the 22 px title bar slot (Always in the layout, Reveal
//                 as a zero-space overlay after 120 ms in the top band,
//                 Hidden), its options menu, Arrange presentation (header
//                 accent, dashed outline, inside corner and edge resize
//                 handles) and the glass plate. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: Move to Edge and Place Free,
//                 the panel-docked header drag toward an edge, Escape
//                 cancels a drag, the optional 8 px alignment at release
//                 (Alt bypasses it), the floating resize corner and the
//                 opaque panel surface behind a free or floating container.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The container carries the background the user chose as
//                 WorkspacePreview::kUserBackgroundProperty, so a meter
//                 inside it shows that colour in place of the panel role
//                 (workspace preview only). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
// =================================================================

/*  ucMeter.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "ContainerWidget.h"
#include "ContainerContentHost.h"
#include "gui/widgets/AppIcon.h"
#include <QMenu>
#include "gui/containers/OwnedMenu.h"
#include <QContextMenuEvent>
#include <QTimer>
#include <QKeyEvent>
#include "FloatingContainer.h"
#include "core/BoardCapabilities.h"
#include "core/LogCategories.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"
#include "WorkspaceHeader.h"
#include "EdgeDockStrip.h"

#include <QUuid>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QApplication>
#include <QMap>
#include <QStringList>
#include <QCursor>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <algorithm>

namespace NereusSDR {

namespace {

// Workspace preview: the shadow under a revealed overlay header,
// rgba(0,0,0,110) fading to transparent over kRevealShadowPx.
constexpr int kRevealShadowAlpha = 110;
// Workspace preview: Arrange's dashed outline, the accent at 60 %.
constexpr int kArrangeOutlineAlpha = 153;
// The legacy default content background; in the preview the glass plate
// shows through instead.
constexpr QRgb kLegacyContentBackground = 0xff0f0f1a;

class RevealShadow final : public QWidget {
public:
    explicit RevealShadow(QWidget* parent) : QWidget(parent)
    {
        // Native on macOS like every sibling here: it must pass clicks
        // through as a view, not only as a widget.
        WorkspacePreview::passInputThrough(this);
        hide();
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0.0, QColor(0, 0, 0, kRevealShadowAlpha));
        g.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), g);
    }
};

class ArrangeOutline final : public QWidget {
public:
    explicit ArrangeOutline(QWidget* parent) : QWidget(parent)
    {
        // Covers the whole container in Arrange and is raised over the
        // header: as a native view it took every click on macOS.
        WorkspacePreview::passInputThrough(this);
        hide();
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QColor c(Workspace::role("accent", Workspace::kAccent));
        c.setAlpha(kArrangeOutlineAlpha);
        p.setPen(QPen(c, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

// Logical size of the pin-on-top icon inside its 22 x 22 title-bar button.
constexpr int kPinIconPx = 16;

} // namespace

ContainerWidget::ContainerWidget(QWidget* parent)
    : QWidget(parent)
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setMinimumSize(kMinContainerWidth, kMinContainerHeight);
    // No user background yet: the content takes the container's role.
    setProperty(WorkspacePreview::kUserBackgroundProperty, QColor());
    setMouseTracking(true);
    buildUI();
    updateTitleBar();
    updateTitle();
    setupBorder();
    storeLocation();

    // Default placeholder content — replaced by setContent() in later phases
    auto* placeholder = new QLabel(QStringLiteral("Container"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral(
        "color: #405060; font-size: 14px; background: transparent;"));
    if (WorkspacePreview::enabled()) {
        placeholder->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 14px; background: transparent;")
            .arg(Workspace::role("text-inactive", "#405060")));
    }
    setContent(placeholder);

    qCDebug(lcContainer) << "Container created:" << m_id;
}

ContainerWidget::~ContainerWidget()
{
    if (m_structuredChrome) {
        qApp->removeEventFilter(this);
    }
    qCDebug(lcContainer) << "Container destroyed:" << m_id;
}

void ContainerWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar — hidden by default, shown on hover (Thetis ucMeter.cs:156)
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(22);
    m_titleBar->setVisible(false);
    m_titleBar->setStyleSheet(QStringLiteral(
        "background: #1a2a3a; border-bottom: 1px solid #203040;"));
    const bool workspaceLook = WorkspacePreview::enabled();
    if (workspaceLook) {
        // Workspace preview: Longpath's raised title bar, scoped to the bar
        // itself so its borders do not cascade onto the label.
        m_titleBar->setStyleSheet(QStringLiteral("QWidget#containerTitleBar { %1 }")
                                      .arg(Workspace::titleBarStyle()));
    }

    auto* barLayout = new QHBoxLayout(m_titleBar);
    barLayout->setContentsMargins(4, 0, 0, 0);
    barLayout->setSpacing(2);

    // Title label — "RX1" + notes (Thetis ucMeter.cs:625-639)
    m_titleLabel = new QLabel(QStringLiteral("RX1"), m_titleBar);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: #c8d8e8; font-size: 11px; font-weight: bold; background: transparent;"));
    if (workspaceLook) {
        m_titleLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: bold; background: transparent;")
            .arg(Workspace::role("title-text", "#c8d8e8")));
    }
    // Native hand cursors only. Qt 6.11.0 on macOS draws SizeAllCursor (and
    // WaitCursor / BusyCursor) from its own ICC-tagged PNGs, and
    // QImage::toCGImage() frees the colour space before CGImageCreate uses
    // it: EXC_BREAKPOINT under QCocoaCursor::createCursorData, reached from
    // this title bar on hover (operator's Mac, 2026-09-25 and 2026-09-26).
    // Native NSCursor shapes never take that path. Open hand on hover,
    // closed hand while dragging (beginDrag / endDrag).
    // Guarded by scripts/verify-no-image-cursors.py and tst_native_cursors.
    m_titleLabel->setCursor(Qt::OpenHandCursor);
    barLayout->addWidget(m_titleLabel, 1);

    const QString btnStyle = workspaceLook
        ? QStringLiteral(
              "QPushButton { background: transparent; border: none; color: %1;"
              "  font-size: 11px; padding: 2px 4px; }"
              "QPushButton:hover { background: %2; color: %3; }")
              .arg(Workspace::role("text-secondary", "#8090a0"),
                   Workspace::role("button-hover", "#2a3a4a"),
                   Workspace::role("text", "#c8d8e8"))
        : QStringLiteral(
              "QPushButton { background: transparent; border: none; color: #8090a0;"
              "  font-size: 11px; padding: 2px 4px; }"
              "QPushButton:hover { background: #2a3a4a; color: #c8d8e8; }");

    // Axis lock button (overlay-docked only)
    m_btnAxis = new QPushButton(QStringLiteral("\u2196"), m_titleBar);
    m_btnAxis->setFixedSize(22, 22);
    m_btnAxis->setToolTip(QStringLiteral("Axis lock (click to cycle, right-click reverse)"));
    m_btnAxis->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnAxis);

    // Pin-on-top button (floating only)
    m_btnPin = new QPushButton(m_titleBar);
    m_btnPin->setObjectName(QStringLiteral("ContainerPinButton"));
    AppIcon::apply(m_btnPin, QStringLiteral("pin"), kPinIconPx);
    m_btnPin->setFixedSize(22, 22);
    m_btnPin->setToolTip(QStringLiteral("Pin on top"));
    m_btnPin->setStyleSheet(btnStyle);
    m_btnPin->setVisible(false);
    barLayout->addWidget(m_btnPin);

    // Float/Dock toggle
    m_btnFloat = new QPushButton(QStringLiteral("\u2197"), m_titleBar);
    m_btnFloat->setFixedSize(22, 22);
    m_btnFloat->setToolTip(QStringLiteral("Float / Dock"));
    m_btnFloat->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnFloat);

    // Settings gear
    m_btnSettings = new QPushButton(QStringLiteral("\u2699"), m_titleBar);
    m_btnSettings->setFixedSize(22, 22);
    m_btnSettings->setToolTip(QStringLiteral("Container settings"));
    m_btnSettings->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnSettings);

    m_titleBar->setObjectName(QStringLiteral("containerTitleBar"));
    m_headerSlot = new QWidget(this);
    m_headerSlot->setObjectName(QStringLiteral("containerHeaderExtent"));
    m_headerSlot->setFixedHeight(kTitleBarHeight);
    m_headerSlot->hide();
    auto *headerLayout = new QVBoxLayout(m_headerSlot);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_headerSlot);
    mainLayout->addWidget(m_titleBar);

    // Content holder — layout slot for setContent()
    m_contentHolder = new QWidget(this);
    m_contentHolder->setMouseTracking(true);
    m_contentHolder->setStyleSheet(QStringLiteral("background: #0f0f1a;"));
    if (workspaceLook) {
        // Selector-less on purpose, like the original: the content's
        // descendants inherit this background.
        m_contentHolder->setStyleSheet(QStringLiteral("background: %1;")
                                           .arg(Workspace::role("panel", "#0f0f1a")));
    }
    new QVBoxLayout(m_contentHolder);
    m_contentHolder->layout()->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_contentHolder, 1);

    // Resize grip (bottom-right, hidden until hover)
    if (workspaceLook) {
        // Workspace preview: three diagonals of dots, inside the bounds,
        // shown while Arrange is on.
        m_resizeGrip = new WorkspaceCornerGrip(this);
        m_resizeGrip->setObjectName(QStringLiteral("containerResizeGrip"));
    } else {
        m_resizeGrip = new QWidget(this);
        m_resizeGrip->setObjectName(QStringLiteral("containerResizeGrip"));
        m_resizeGrip->setFixedSize(12, 12);
        m_resizeGrip->setCursor(Qt::SizeFDiagCursor);
        m_resizeGrip->setStyleSheet(QStringLiteral(
            "background: #405060; border-radius: 2px;"));
    }
    m_resizeGrip->setVisible(false);

    // Wire button signals
    connect(m_btnFloat, &QPushButton::clicked, this, [this]() {
        if (isFloating()) {
            if (m_popOutShell) {
                emit returnContainerRequested();
                return;
            }
            emit dockRequested();
        } else {
            emit floatRequested();
        }
    });

    connect(m_btnAxis, &QPushButton::clicked, this, [this]() {
        cycleAxisLock(QApplication::keyboardModifiers() & Qt::ShiftModifier);
    });

    connect(m_btnPin, &QPushButton::clicked, this, [this]() {
        setPinOnTop(!m_pinOnTop);
    });

    connect(m_btnSettings, &QPushButton::clicked, this, [this]() {
        emit settingsRequested();
    });

    // Event filters for title bar drag + resize grip + hover detection
    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);
    m_resizeGrip->installEventFilter(this);
    m_contentHolder->installEventFilter(this);

    if (workspaceLook) {
        buildWorkspaceHeader();
    }
}

void ContainerWidget::buildWorkspaceHeader()
{
    m_groupHeader = new WorkspaceGroupHeader(this);
    m_groupHeader->hide();
    m_groupHeader->installEventFilter(this);
    static_cast<QVBoxLayout*>(layout())->insertWidget(0, m_groupHeader);
    connect(m_groupHeader, &WorkspaceGroupHeader::popOutClicked, this, [this] {
        m_btnFloat->click();
    });
    connect(m_groupHeader, &WorkspaceGroupHeader::optionsClicked, this,
            &ContainerWidget::showWorkspaceOptions);
    connect(m_groupHeader, &WorkspaceGroupHeader::sourceClicked, this,
            &ContainerWidget::showSourceMenu);
    // The hide button: the same as the menu's "Hide container".
    connect(m_groupHeader, &WorkspaceGroupHeader::hideClicked, this,
            &ContainerWidget::hideContainerRequested);

    m_headerOpacity = new QGraphicsOpacityEffect(m_groupHeader);
    m_headerOpacity->setOpacity(kRevealOpacity);
    m_headerOpacity->setEnabled(false);
    m_groupHeader->setGraphicsEffect(m_headerOpacity);
    m_revealFade = new QPropertyAnimation(m_headerOpacity, "opacity", this);
    m_revealFade->setDuration(kRevealFadeMs);

    m_revealShadow = new RevealShadow(this);
    m_arrangeFrame = new ArrangeOutline(this);
    m_edgeRight = new QWidget(this);
    m_edgeRight->setObjectName(QStringLiteral("containerResizeEdgeRight"));
    m_edgeRight->setCursor(Qt::SizeHorCursor);
    m_edgeBottom = new QWidget(this);
    m_edgeBottom->setObjectName(QStringLiteral("containerResizeEdgeBottom"));
    m_edgeBottom->setCursor(Qt::SizeVerCursor);
    for (QWidget* edge : {m_edgeRight, m_edgeBottom}) {
        edge->hide();
        edge->installEventFilter(this);
    }

    m_revealDelay.setSingleShot(true);
    m_revealDelay.setInterval(kRevealDelayMs);
    connect(&m_revealDelay, &QTimer::timeout, this, [this] {
        if (m_headerMode == HeaderMode::Reveal) {
            setHeaderRevealed(true, true);
        }
    });
    m_revealHide.setSingleShot(true);
    m_revealHide.setInterval(kRevealHideMs);
    connect(&m_revealHide, &QTimer::timeout, this, [this] {
        // A menu of the header, keyboard focus in it, or the preview seam
        // keeps it up.
        if (m_workspaceMenuOpen || m_revealHeld || headerHasFocus()) {
            return;
        }
        setHeaderRevealed(false, false);
    });
}

int ContainerWidget::headerExtent() const
{
    if (!m_groupHeader) {
        return kTitleBarHeight;
    }
    if (!m_structuredChrome) {
        return 0;
    }
    return (m_headerMode == HeaderMode::Always || m_minimised) ? WorkspaceGroupHeader::kHeight : 0;
}

void ContainerWidget::setArrangeActive(bool active)
{
    if (m_arrangeActive == active) {
        return;
    }
    m_arrangeActive = active;
    if (!active) {
        // Leaving Arrange restores each mode; a held edit is cancelled.
        m_recoverChrome = false;
    }
    updateChrome();
}

void ContainerWidget::setHeaderRevealedForPreview(bool revealed)
{
    m_revealHeld = revealed;
    m_revealDelay.stop();
    m_revealHide.stop();
    setHeaderRevealed(revealed, false);
}

void ContainerWidget::setWorkspaceTitle(const QString& title)
{
    if (m_groupHeader) {
        m_groupHeader->setTitle(title);
    }
}

void ContainerWidget::setWorkspaceSource(const QString& text, const QColor& colour)
{
    if (m_groupHeader) {
        m_groupHeader->setSource(text, colour);
    }
}

bool ContainerWidget::headerHasFocus() const
{
    // Keyboard focus only: Tab onto the container, or into the header. A
    // click on the container's body is not a reveal.
    QWidget* focused = QApplication::focusWidget();
    return focused != nullptr
        && ((focused == this && m_keyboardFocus)
            || (m_groupHeader != nullptr && m_groupHeader->isAncestorOf(focused)));
}

void ContainerWidget::setHeaderRevealed(bool revealed, bool animate)
{
    if (!m_groupHeader) {
        return;
    }
    if (m_headerRevealed != revealed) {
        m_headerRevealed = revealed;
        m_revealFade->stop();
        if (revealed && animate) {
            m_headerOpacity->setOpacity(0.0);
            m_revealFade->setStartValue(0.0);
            m_revealFade->setEndValue(kRevealOpacity);
            m_revealFade->start();
        } else {
            m_headerOpacity->setOpacity(kRevealOpacity);
        }
    }
    updateWorkspaceChrome();
}

void ContainerWidget::trackRevealBand(const QPoint& globalPos)
{
    if (!m_groupHeader || !m_structuredChrome || m_headerMode != HeaderMode::Reveal) {
        m_revealDelay.stop();
        m_revealHide.stop();
        return;
    }
    const QPoint local = mapFromGlobal(globalPos);
    const bool inBand = isVisible() && rect().contains(local) && local.y() < kRevealBand;
    if (inBand) {
        m_revealHide.stop();
        if (!m_headerRevealed && !m_revealDelay.isActive()) {
            m_revealDelay.start();
        }
        return;
    }
    m_revealDelay.stop();
    if (m_headerRevealed && !m_revealHeld && !m_revealHide.isActive()) {
        m_revealHide.start();
    }
}

void ContainerWidget::updateWorkspaceChrome()
{
    m_titleBar->setVisible(false);
    // Keyboard focus on the container (or in its header) reveals at once.
    if (m_headerMode == HeaderMode::Reveal && !m_headerRevealed && headerHasFocus()) {
        setHeaderRevealed(true, true);
        return;
    }
    if (m_headerMode == HeaderMode::Reveal && m_headerRevealed && !headerHasFocus()
        && !m_revealHeld && !m_revealHide.isActive() && !m_revealDelay.isActive()) {
        trackRevealBand(QCursor::pos());
    }
    const bool inLayout = m_headerMode == HeaderMode::Always || m_minimised;
    const bool overlayShown = !inLayout
        && (m_arrangeActive || m_recoverChrome
            || (m_headerMode == HeaderMode::Reveal && m_headerRevealed));
    auto* box = static_cast<QVBoxLayout*>(layout());
    const bool placedInLayout = box->indexOf(m_groupHeader) >= 0;
    if (inLayout && !placedInLayout) {
        box->insertWidget(0, m_groupHeader);
    } else if (!inLayout && placedInLayout) {
        box->removeWidget(m_groupHeader);
    }
    m_headerOpacity->setEnabled(!inLayout);
    if (m_revealFade->state() != QAbstractAnimation::Running) {
        m_headerOpacity->setOpacity(kRevealOpacity);
    }
    m_groupHeader->setVisible(inLayout || overlayShown);
    m_revealShadow->setVisible(overlayShown);
    m_groupHeader->setArrangeAccent(m_arrangeActive);
    m_groupHeader->setLocked(m_locked);
    m_groupHeader->setFloating(isFloating());

    const bool resizable = m_arrangeActive && !m_locked && !isPanelDocked();
    // A frameless floating window keeps its resize corner, inside its bounds.
    const bool floatingCorner = isFloating() && !m_locked;
    m_resizeGrip->setVisible((resizable || floatingCorner) && !m_autoHeight);
    m_edgeRight->setVisible(resizable);
    m_edgeBottom->setVisible(resizable && !m_autoHeight);
    m_arrangeFrame->setVisible(m_arrangeActive);
    m_btnFloat->setEnabled(!m_locked);
    m_btnAxis->setEnabled(!m_locked);
    m_btnPin->setEnabled(!m_locked);
    placeWorkspaceOverlays();
}

void ContainerWidget::placeWorkspaceOverlays()
{
    if (!m_groupHeader) {
        return;
    }
    const auto place = [](QWidget* w, const QRect& r) {
        if (w->geometry() != r) {
            w->setGeometry(r);
        }
        if (w->isVisible()) {
            w->raise();
        }
    };
    const bool overlaid = static_cast<QVBoxLayout*>(layout())->indexOf(m_groupHeader) < 0;
    if (overlaid) {
        place(m_groupHeader, QRect(0, 0, width(), WorkspaceGroupHeader::kHeight));
    }
    if (auto* host = qobject_cast<ContainerContentHost*>(m_content)) {
        // How far the overlaid group header reaches into the content.
        const int reach = overlaid && !m_groupHeader->isHidden()
            ? WorkspaceGroupHeader::kHeight - host->mapTo(this, QPoint()).y()
            : 0;
        host->setTopOverlayInset(reach);
    }
    place(m_revealShadow, QRect(0, WorkspaceGroupHeader::kHeight, width(), kRevealShadowPx));
    place(m_arrangeFrame, rect());
    place(m_edgeRight, QRect(width() - kArrangeEdgePx, 0, kArrangeEdgePx, height()));
    place(m_edgeBottom, QRect(0, height() - kArrangeEdgePx, width(), kArrangeEdgePx));
    place(m_resizeGrip, QRect(width() - WorkspaceCornerGrip::kSize, height() - WorkspaceCornerGrip::kSize,
                              WorkspaceCornerGrip::kSize, WorkspaceCornerGrip::kSize));
}

void ContainerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_groupHeader) {
        placeWorkspaceOverlays();
    }
}

void ContainerWidget::populateWorkspaceMenu(QMenu& menu)
{
    if (isOverlayDocked()) {
        auto* axis = menu.addAction(tr("Axis Lock %1 (cycle)").arg(m_btnAxis->text()), this,
                                    [this] { cycleAxisLock(false); });
        axis->setEnabled(!m_locked);
    }
    if (isFloating()) {
        auto* pin = menu.addAction(tr("Pin on Top"), this, [this] { setPinOnTop(!m_pinOnTop); });
        pin->setCheckable(true);
        pin->setChecked(m_pinOnTop);
        pin->setEnabled(!m_locked);
    }
    auto* lock = menu.addAction(tr("Lock"), this, [this] { emit lockRequested(!m_locked); });
    lock->setCheckable(true);
    lock->setChecked(m_locked);
    const QString floatText = isFloating()
        ? (m_popOutShell ? tr("Return All Objects") : tr("Dock")) : tr("Pop Out");
    auto* pop = menu.addAction(floatText, this, [this] { m_btnFloat->click(); });
    pop->setEnabled(!m_locked);
    menu.addAction(tr("Container Settings…"), this, [this] { emit settingsRequested(); });
    menu.addSeparator();
    auto* header = menu.addMenu(tr("Header"));
    for (auto mode : {HeaderMode::Always, HeaderMode::Reveal, HeaderMode::Hidden}) {
        auto* action = header->addAction(mode == HeaderMode::Always   ? tr("Always")
                                         : mode == HeaderMode::Reveal ? tr("Reveal")
                                                                      : tr("Hidden"));
        action->setCheckable(true);
        action->setChecked(mode == m_headerMode);
        connect(action, &QAction::triggered, this, [this, mode] { emit headerModeRequested(mode); });
    }
    // Docks in the strip for that edge; Place Free puts the container over
    // the spectrum, which a drop never does.
    auto* edge = menu.addMenu(tr("Move to Edge"));
    const std::pair<DockEdge, QString> edges[] = {{DockEdge::Left, tr("Left")},
                                                  {DockEdge::Right, tr("Right")},
                                                  {DockEdge::Top, tr("Top")},
                                                  {DockEdge::Bottom, tr("Bottom")}};
    for (const auto& [value, name] : edges) {
        auto* action = edge->addAction(name, this, [this, value] { emit moveToEdgeRequested(value); });
        action->setCheckable(true);
        action->setChecked(isPanelDocked() && m_dockEdge == value);
        action->setEnabled(!m_locked);
    }
    edge->setEnabled(!m_locked);
    if (!isOverlayDocked()) {
        auto* free = menu.addAction(tr("Place Free"), this, [this] { emit placeFreeRequested(); });
        free->setToolTip(tr("Place over the spectrum, inside the main window"));
        free->setEnabled(!m_locked);
    }
    auto* arrange = menu.addAction(tr("Arrange Workspace"), this,
                                   [this] { emit arrangeWorkspaceRequested(!m_arrangeActive); });
    arrange->setCheckable(true);
    arrange->setChecked(m_arrangeActive);
    if (auto* host = qobject_cast<ContainerContentHost*>(m_content)) {
        menu.addSeparator();
        host->addContentsMenu(menu);
    }
}

void ContainerWidget::showWorkspaceOptions()
{
    OwnedMenu owned(this);
    QMenu& menu = owned.menu();
    populateWorkspaceMenu(menu);
    m_workspaceMenuOpen = true;
    QPushButton* button = m_groupHeader->optionsButton();
    const QPointer<ContainerWidget> self(this);
    menu.exec(button->mapToGlobal(QPoint(0, button->height())));
    if (!self) {
        return;
    }
    m_workspaceMenuOpen = false;
    trackRevealBand(QCursor::pos());
}

void ContainerWidget::setSliceAvailability(std::function<bool(int sliceId)> available)
{
    m_sliceAvailable = std::move(available);
}

void ContainerWidget::populateSourceMenu(QMenu& menu)
{
    menu.setToolTipsVisible(true);
    // The tick follows the slice the chip names (sliceId first), so a
    // container keyed by sliceId never ticks its stale rxSource.
    const int checkedRx = m_sourceMenuRx > 0 ? m_sourceMenuRx : m_rxSource;
    for (int rx = kFirstRxSource; rx <= kLastRxSource; ++rx) {
        const QChar letter(QLatin1Char(static_cast<char>('A' + rx - 1)));
        auto* action = menu.addAction(tr("RX %1").arg(letter), this,
                                      [this, rx] { emit sourceRequested(rx); });
        action->setCheckable(true);
        action->setChecked(!m_followsSelectedRx && checkedRx == rx);
        const bool exists = !m_sliceAvailable || m_sliceAvailable(sliceIdForRxSource(rx));
        action->setEnabled(exists);
        if (!exists) {
            action->setToolTip(tr("Slice %1 is not open").arg(letter));
        }
    }
    menu.addSeparator();
    auto* active = menu.addAction(tr("Active slice"), this,
                                  [this] { emit sourceRequested(kActiveSliceSource); });
    active->setCheckable(true);
    active->setChecked(m_followsSelectedRx);
    active->setToolTip(tr("Follow the selected slice"));
}

void ContainerWidget::showSourceMenu()
{
    OwnedMenu owned(this);
    QMenu& menu = owned.menu();
    populateSourceMenu(menu);
    m_workspaceMenuOpen = true;
    QWidget* chip = m_groupHeader->sourceChip();
    const QPointer<ContainerWidget> self(this);
    menu.exec(chip->mapToGlobal(QPoint(0, chip->height())));
    if (!self) {
        return;
    }
    m_workspaceMenuOpen = false;
    trackRevealBand(QCursor::pos());
}

// Nereus-origin reserved chrome: visibility never changes the content viewport.
void ContainerWidget::setPopOutShell(bool shell)
{
    m_popOutShell = shell;
    updateTitleBar();
}
void ContainerWidget::setHeaderMode(HeaderMode mode)
{
    if (!m_structuredChrome) {
        m_structuredChrome = true;
        layout()->removeWidget(m_titleBar);
        m_headerSlot->layout()->addWidget(m_titleBar);
        if (!m_groupHeader) {
            m_headerSlot->show();
        }
        setFocusPolicy(Qt::StrongFocus);
        qApp->installEventFilter(this);
        connect(qApp, &QApplication::focusChanged, this,
                [this](QWidget *, QWidget *) { updateChrome(); });
    }
    if(m_headerMode!=mode) {m_recoverChrome=false;}
    if (m_groupHeader && m_headerMode != mode) {
        m_revealHeld = false;
        m_revealDelay.stop();
        m_revealHide.stop();
        m_headerRevealed = false;
    }
    m_headerMode = mode;
    m_titleBarVisible = mode != HeaderMode::Hidden;
    updateChrome();
}
bool ContainerWidget::chromeVisible() const
{
    // Workspace preview: the group header is the chrome.
    return m_groupHeader ? m_groupHeader->isVisible() : m_titleBar->isVisible();
}
void ContainerWidget::recoverChrome()
{
    m_recoverChrome = true;
    updateChrome();
}
void ContainerWidget::updateChrome()
{
    if (!m_structuredChrome) {
        return;
    }
    if (m_groupHeader) {
        updateWorkspaceChrome();
        return;
    }
    QWidget *focused = QApplication::focusWidget();
    const bool focus = focused && (focused == this || isAncestorOf(focused));
    const bool shift = m_shiftChrome || (QApplication::keyboardModifiers() & Qt::ShiftModifier);
    const bool visible = m_headerMode == HeaderMode::Always ||
                         (m_headerMode == HeaderMode::Reveal && (m_hoverChrome || focus)) ||
                         m_recoverChrome || shift;
    m_titleBar->setVisible(visible);
    m_resizeGrip->setVisible(visible && !m_locked && !m_autoHeight && !isPanelDocked());
    m_resizeGrip->move(width() - 12, height() - 12);
    m_btnFloat->setEnabled(!m_locked);
    m_btnAxis->setEnabled(!m_locked);
    m_btnPin->setEnabled(!m_locked);
}
void ContainerWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_structuredChrome) {
        QWidget::contextMenuEvent(event);
        return;
    }
    // On the heap under the container: an action can remove the
    // container, and its shell, while the menu is open (OwnedMenu.h).
    OwnedMenu owned(this);
    QMenu& menu = owned.menu();
    menu.addAction(tr("Container Settings…"), this, [this] { emit settingsRequested(); });
    menu.addAction(tr("Hide container (retain placement)"), this,
                   [this] { emit hideContainerRequested(); });
    if (m_popOutShell) {
        auto *back = menu.addAction(tr("Return all objects and close shell"), this,
                                    [this] { emit returnContainerRequested(); });
        back->setEnabled(!m_locked);
    }
    menu.addAction(tr("Reveal controls"), this, &ContainerWidget::recoverChrome);
    auto *header = menu.addMenu(tr("Header"));
    for (auto mode : {HeaderMode::Always, HeaderMode::Reveal, HeaderMode::Hidden}) {
        const QString hiddenLabel = m_groupHeader ? tr("Hidden (Arrange or Reveal controls recovers it)")
                                                  : tr("Hidden (hold Shift to recover)");
        auto *action =
            header->addAction(mode == HeaderMode::Always   ? tr("Always visible")
                              : mode == HeaderMode::Reveal ? tr("Reveal on hover/focus")
                                                           : hiddenLabel);
        action->setCheckable(true);
        action->setChecked(mode == m_headerMode);
        connect(action, &QAction::triggered, this,
                [this, mode] { emit headerModeRequested(mode); });
    }
    if (m_groupHeader) {
        auto *arrange = menu.addAction(tr("Arrange Workspace"), this,
                                       [this] { emit arrangeWorkspaceRequested(!m_arrangeActive); });
        arrange->setCheckable(true);
        arrange->setChecked(m_arrangeActive);
    }
    if (auto *host = qobject_cast<ContainerContentHost *>(m_content)) {
        host->addContentsMenu(menu);
    }
    m_workspaceMenuOpen = true;
    const QPointer<ContainerWidget> self(this);
    menu.exec(event->globalPos());
    if (!self) {
        return;
    }
    m_workspaceMenuOpen = false;
}

void ContainerWidget::setContentMinimumWidth(int width)
{
    // JJ's choice (2026-10-08): never narrower than the content. A splitter,
    // edge strip or floating window holding the container then keeps the
    // content whole, and the 260 px floor still applies.
    m_contentMinimumWidth = qMax(0, width);
    const int minimum = qMax(kMinContainerWidth, m_contentMinimumWidth);
    if (minimumWidth() != minimum) {
        setMinimumWidth(minimum);
    }
}

void ContainerWidget::setContent(QWidget* widget)
{
    QLayout* layout = m_contentHolder->layout();
    // A new content announces its own minimum width.
    setContentMinimumWidth(0);
    if (m_content) {
        layout->removeWidget(m_content);
        // Detach SYNCHRONOUSLY before deleteLater. When the content is a
        // MeterWidget (WA_NativeWindow QRhiWidget), leaving it under this
        // container's HWND through an upcoming reparent blocks the fresh
        // MeterWidget's D3D11 swapchain with E_ACCESSDENIED on Windows.
        m_content->hide();
        m_content->setParent(nullptr);
        m_content->deleteLater();
    }
    m_content = widget;
    if (m_content) {
        m_content->setParent(m_contentHolder);
        m_content->setMouseTracking(true);
        m_content->installEventFilter(this);
        // Enable mouse tracking + event filter on all descendants so hover
        // detection works even when mouse is over child widgets (scroll areas, applets).
        for (QWidget* child : m_content->findChildren<QWidget*>()) {
            child->setMouseTracking(true);
            child->installEventFilter(this);
        }
        layout->addWidget(m_content);
    }
    emit contentChanged(m_content);
}

void ContainerWidget::updateTitleBar()
{
    // Thetis ucMeter.cs:605-624 — adapted for 3 dock modes
    if (isFloating()) {
        m_btnFloat->setText(QStringLiteral("\u2199"));
        m_btnFloat->setToolTip(
            m_popOutShell ? QStringLiteral("Return all objects to their remembered containers")
                          : QStringLiteral("Dock"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(true);
    } else if (isPanelDocked()) {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(false);
    } else {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(true);
        m_btnPin->setVisible(false);
    }
}

void ContainerWidget::updateTitle()
{
    // Thetis ucMeter.cs:625-639 ("RX" + the receiver number); NereusSDR
    // names the container's slice.
    QString firstLine = m_notes.section(QLatin1Char('\n'), 0, 0);
    QString title = sliceNameForRxSource(m_rxSource);
    if (!firstLine.isEmpty()) {
        title += QStringLiteral(" ") + firstLine;
    }
    m_titleLabel->setText(title);
    if (m_groupHeader && m_groupHeader->title().isEmpty()) {
        m_groupHeader->setTitle(firstLine.isEmpty() ? title : firstLine);
    }
}

void ContainerWidget::setupBorder()
{
    // Thetis ucMeter.cs:640-643, extended with highlight outline
    // (Phase 3G-6). When m_highlighted the accent outline overrides the
    // normal border regardless of m_border, so the user always sees
    // which container the settings dialog is editing.
    if (m_groupHeader) {
        // Workspace preview: the glass plate, radius 10, a 1 px
        // border-subtle edge (or the 2 px accent while highlighted).
        setAttribute(Qt::WA_StyledBackground, true);
        const QString edge = m_highlighted
            ? QStringLiteral("border: 2px solid %1;").arg(Workspace::role("accent", "#00b4d8"))
            : m_border ? QStringLiteral("border: 1px solid %1;").arg(Workspace::role("border-subtle", "#203040"))
                       : QStringLiteral("border: none;");
        setStyleSheet(QStringLiteral("ContainerWidget { background: %1; %2 border-radius: %3px; }")
                          .arg(Workspace::glassPanelFill(), edge)
                          .arg(Workspace::kGlassPanelRadius));
    } else if (WorkspacePreview::enabled() && (m_highlighted || m_border)) {
        // Workspace preview: the highlight is the accent; the plain
        // border is #203040's border-subtle role.
        setStyleSheet(QStringLiteral("ContainerWidget { border: %1px solid %2; }")
                          .arg(m_highlighted ? 2 : 1)
                          .arg(m_highlighted ? Workspace::role("accent", "#00b4d8")
                                             : Workspace::role("border-subtle", "#203040")));
    } else if (m_highlighted) {
        setStyleSheet(QStringLiteral(
            "ContainerWidget { border: 2px solid #00b4d8; }"));
    } else if (m_border) {
        setStyleSheet(QStringLiteral(
            "ContainerWidget { border: 1px solid #203040; }"));
    } else {
        setStyleSheet(QString());
    }
}

// --- Property setters ---

void ContainerWidget::setId(const QString& id) { m_id = id; m_id.remove(QLatin1Char('|')); }
void ContainerWidget::setRxSource(int rx)
{
    const bool changed = (m_rxSource != rx);
    m_rxSource = rx;
    updateTitle();
    if (changed) { emit rxSourceChanged(rx); }
}

QString ContainerWidget::sliceNameForRxSource(int rx)
{
    if (rx < 1 || rx > 26) { return QStringLiteral("Slice"); }
    return QStringLiteral("Slice %1").arg(QChar(QLatin1Char(static_cast<char>('A' + rx - 1))));
}

void ContainerWidget::setDockMode(DockMode mode)
{
    bool changed = (m_dockMode != mode);
    m_dockMode = mode;

    // Enforce minimum width in all dock modes so the applets are always usable.
    // Never below what the content needs either (JJ, 2026-10-08).
    setMinimumWidth(qMax(kMinContainerWidth, m_contentMinimumWidth));

    if (m_groupHeader) {
        // Workspace preview: a free or floating container covers whatever
        // lies beneath it (VFO flags, the spectrum) with the opaque panel
        // surface over its whole rect, the plate's rounded corners included.
        // In a strip the app background shows around the plate instead.
        const bool covers = mode != DockMode::PanelDocked;
        QPalette surface = palette();
        surface.setColor(QPalette::Window, QColor(Workspace::role("panel", "#0f0f1a")));
        setPalette(surface);
        setAutoFillBackground(covers);
    }

    if (changed) {
        updateTitleBar();
        emit dockModeChanged(mode);
    }
}

void ContainerWidget::setAxisLock(AxisLock lock)
{
    m_axisLock = lock;

    // Update axis button icon — from Thetis ucMeter.cs:936-968
    static const QChar arrows[] = {
        QChar(0x2190),  // LEFT
        QChar(0x2196),  // TOPLEFT
        QChar(0x2191),  // TOP
        QChar(0x2197),  // TOPRIGHT
        QChar(0x2192),  // RIGHT
        QChar(0x2198),  // BOTTOMRIGHT
        QChar(0x2193),  // BOTTOM
        QChar(0x2199),  // BOTTOMLEFT
    };
    int idx = static_cast<int>(lock);
    if (idx >= 0 && idx < 8) {
        m_btnAxis->setText(QString(arrows[idx]));
    }
}

void ContainerWidget::cycleAxisLock(bool reverse)
{
    // From Thetis ucMeter.cs:912-935
    int n = static_cast<int>(m_axisLock);
    if (reverse) {
        n--;
    } else {
        n++;
    }
    if (n > static_cast<int>(AxisLock::BottomLeft)) {
        n = static_cast<int>(AxisLock::Left);
    }
    if (n < static_cast<int>(AxisLock::Left)) {
        n = static_cast<int>(AxisLock::BottomLeft);
    }
    setAxisLock(static_cast<AxisLock>(n));
    emit dockedMoved();
}

void ContainerWidget::setPinOnTop(bool pin)
{
    // From Thetis ucMeter.cs:974-978
    m_pinOnTop = pin;
    AppIcon::apply(m_btnPin, pin ? QStringLiteral("pinned") : QStringLiteral("pin"), kPinIconPx);
    setTopMost();
}

void ContainerWidget::setTopMost()
{
    // From Thetis ucMeter.cs:980-993
    if (isFloating() && parentWidget()) {
        FloatingContainer* fc = qobject_cast<FloatingContainer*>(parentWidget());
        if (fc) {
            bool wasVisible = fc->isVisible();
            if (m_pinOnTop) {
                fc->setWindowFlags(fc->windowFlags() | Qt::WindowStaysOnTopHint);
            } else {
                fc->setWindowFlags(fc->windowFlags() & ~Qt::WindowStaysOnTopHint);
            }
            if (wasVisible) {
                fc->show();
            }
        }
    }
}

void ContainerWidget::setBorder(bool border) { m_border = border; setupBorder(); }
void ContainerWidget::setLocked(bool locked)
{
    m_locked = locked;
    updateChrome();
}
void ContainerWidget::setContainerEnabled(bool enabled) { m_enabled = enabled; }
void ContainerWidget::setShowOnRx(bool show) { m_showOnRx = show; }
void ContainerWidget::setShowOnTx(bool show) { m_showOnTx = show; }
void ContainerWidget::setHiddenByMacro(bool hidden) { m_hiddenByMacro = hidden; }
void ContainerWidget::setContainerMinimises(bool minimises) { m_containerMinimises = minimises; }
void ContainerWidget::setContainerHidesWhenRxNotUsed(bool hides) { m_containerHidesWhenRxNotUsed = hides; }

void ContainerWidget::setNotes(const QString& notes)
{
    QString scrubbed = notes;
    scrubbed.remove(QLatin1Char('|'));
    if (m_notes == scrubbed) { return; }
    m_notes = scrubbed;
    updateTitle();
    emit notesChanged(m_notes);
}

void ContainerWidget::setBackgroundColor(const QColor& color)
{
    if(!color.isValid() || m_backgroundColor==color) {return;}
    m_backgroundColor=color;
    publishUserBackground();
    if (m_groupHeader && color.rgba() == kLegacyContentBackground) {
        // Workspace preview: the default background is the glass plate.
        m_contentHolder->setStyleSheet(QStringLiteral("background: transparent;"));
        return;
    }
    m_contentHolder->setStyleSheet(QStringLiteral("background: %1;").arg(color.name(QColor::HexArgb)));
}

// Before the content holder's stylesheet changes, so the meters it
// re-polishes read the new colour (WorkspacePreview::dataBackground()).
void ContainerWidget::publishUserBackground()
{
    const bool user = m_backgroundColor.rgba() != kLegacyContentBackground;
    setProperty(WorkspacePreview::kUserBackgroundProperty, user ? m_backgroundColor : QColor());
}

void ContainerWidget::setNoControls(bool noControls) { m_noControls = noControls; }
void ContainerWidget::setAutoHeight(bool autoHeight)
{
    m_autoHeight = autoHeight;
    updateChrome();
}

void ContainerWidget::setTitleBarVisible(bool visible)
{
    if (m_titleBarVisible == visible) { return; }
    m_titleBarVisible = visible;
    if (m_groupHeader && m_structuredChrome) {
        updateChrome();
    } else if (m_titleBar) {
        m_titleBar->setVisible(visible);
    }
    emit titleBarVisibilityChanged(visible);
    update();
}

void ContainerWidget::setMinimised(bool minimised)
{
    if (m_minimised == minimised) { return; }
    m_minimised = minimised;
    // Collapse the content area so only the title bar shows. The title
    // bar stays visible regardless of m_titleBarVisible while minimised
    // so the user retains a handle to un-minimise. FloatingContainer
    // (commit 10) picks up minimisedChanged() to resize its window.
    if (m_contentHolder) {
        m_contentHolder->setVisible(!minimised);
    }
    if (m_groupHeader && m_structuredChrome) {
        updateChrome();
    } else if (minimised && m_titleBar) {
        m_titleBar->setVisible(true);
    } else if (m_titleBar) {
        m_titleBar->setVisible(m_titleBarVisible);
    }
    emit minimisedChanged(minimised);
    update();
}

void ContainerWidget::setHighlighted(bool highlighted)
{
    if (m_highlighted == highlighted) { return; }
    m_highlighted = highlighted;
    setupBorder();
    update();
}
void ContainerWidget::setDockedLocation(const QPoint& loc) { m_dockedLocation = loc; }
void ContainerWidget::setDockedSize(const QSize& size) { m_dockedSize = size; }
void ContainerWidget::setDelta(const QPoint& delta) { m_delta = delta; }

void ContainerWidget::storeLocation()
{
    // From Thetis ucMeter.cs:567-572
    m_dockedLocation = pos();
    m_dockedSize = size();
}

void ContainerWidget::restoreLocation()
{
    // From Thetis ucMeter.cs:574-593
    bool moved = false;
    if (m_dockedLocation != pos()) {
        move(m_dockedLocation);
        moved = true;
    }
    if (m_dockedSize != size()) {
        resize(m_dockedSize);
        moved = true;
    }
    if (moved) {
        update();
    }
}

int ContainerWidget::roundToNearestTen(int value)
{
    return ((value + 5) / 10) * 10;
}

// --- Hover show/hide ---

void ContainerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_structuredChrome) {
        if (m_groupHeader) {
            trackRevealBand(event->globalPosition().toPoint());
        } else {
            m_hoverChrome = true;
            updateChrome();
        }
        if (m_dragging) {
            updateDrag(event->globalPosition().toPoint());
        }
        if (m_resizing) {
            updateResize(event->globalPosition().toPoint());
        }
        QWidget::mouseMoveEvent(event);
        return;
    }
    // From Thetis ucMeter.cs:1198-1229
    // Upstream inline attribution preserved verbatim (ucMeter.cs:1200):
    //   bool no_controls = _no_controls && !Common.ShiftKeyDown; //[2.10.3.6]MW0LGE no title or resize grabber, override by holding shift
    bool noControls = m_noControls && !(QApplication::keyboardModifiers() & Qt::ShiftModifier);

    if (!m_dragging && !noControls) {
        int y = static_cast<int>(event->position().y());
        if (y < kTitleHoverZone && !m_titleBar->isVisible()) {
            m_titleBar->setVisible(true);
            m_titleBar->raise();
        } else if (y >= kTitleBarHeight && m_titleBar->isVisible() && !m_dragging) {
            m_titleBar->setVisible(false);
        }
    }

    // Resize grip: only for overlay-docked and floating, not panel-docked
    if (!m_resizing && !noControls && !isPanelDocked()) {
        bool inGripRegion = event->position().x() > (width() - 16)
                         && event->position().y() > (height() - 16);
        if (inGripRegion && !m_resizeGrip->isVisible()) {
            m_resizeGrip->move(width() - 12, height() - 12);
            m_resizeGrip->setVisible(true);
            m_resizeGrip->raise();
        } else if (!inGripRegion && m_resizeGrip->isVisible() && !m_resizing) {
            m_resizeGrip->setVisible(false);
        }
    }

    if (m_dragging) {
        updateDrag(event->globalPosition().toPoint());
    }
    if (m_resizing) {
        updateResize(event->globalPosition().toPoint());
    }

    QWidget::mouseMoveEvent(event);
}

void ContainerWidget::leaveEvent(QEvent* event)
{
    if (m_structuredChrome) {
        m_hoverChrome = false;
        if (m_groupHeader) {
            trackRevealBand(QCursor::pos());
        }
        if(!m_dragging && !m_resizing) {m_recoverChrome = false;}
        updateChrome();
        QWidget::leaveEvent(event);
        return;
    }
    if (!m_dragging && !m_resizing) {
        m_titleBar->setVisible(false);
        m_resizeGrip->setVisible(false);
    }
    QWidget::leaveEvent(event);
}

// --- Event filter for title bar drag + resize grip ---

bool ContainerWidget::eventFilter(QObject* watched, QEvent* event)
{
    // Workspace preview: Escape cancels a drag or an edge drag; nothing of
    // it is kept.
    if (m_groupHeader && (m_edgePressed || m_dragging) && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        if (m_edgePressed) {
            endEdgeDrag(true, QCursor::pos());
        } else {
            cancelDrag();
        }
        return true;
    }
    if (m_structuredChrome && m_groupHeader) {
        // Workspace preview: only the top band reveals the header; Shift
        // does not (Hidden recovers through the menu and Arrange).
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
            if (watched == this) {
                const auto reason = static_cast<QFocusEvent*>(event)->reason();
                m_keyboardFocus = event->type() == QEvent::FocusIn
                    && (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason
                        || reason == Qt::ShortcutFocusReason);
            }
            QTimer::singleShot(0, this, &ContainerWidget::updateChrome);
        }
        if (event->type() == QEvent::MouseMove) {
            if (auto *widget = qobject_cast<QWidget *>(watched);
                widget && (widget == this || isAncestorOf(widget))) {
                trackRevealBand(static_cast<QMouseEvent *>(event)->globalPosition().toPoint());
            }
        }
    } else if (m_structuredChrome) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Shift) {
                m_shiftChrome = event->type() == QEvent::KeyPress;
                updateChrome();
            }
        }
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut ||
            event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            QTimer::singleShot(0, this, &ContainerWidget::updateChrome);
        }
        if (auto *widget = qobject_cast<QWidget *>(watched);
            widget && (widget == this || isAncestorOf(widget))) {
            if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
                m_hoverChrome = true;
                updateChrome();
            }
            if (event->type() == QEvent::Resize) {
                updateChrome();
            }
        }
    }
    // Workspace preview: the group header moves the container. Its buttons
    // accept their own presses, so only the stripe, grab handle, title,
    // chip and empty header area reach here. A panel-docked container
    // keeps its place (edges arrive with the four-edge strips).
    if (m_groupHeader && watched == m_groupHeader) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                if (!m_locked && !isPanelDocked()) {
                    beginDrag(me->globalPosition().toPoint());
                } else if (!m_locked) {
                    // A docked container moves between strips: past the
                    // drag distance the manager shows the destinations.
                    m_edgePressed = true;
                    m_edgeDragging = false;
                    m_edgePressGlobal = me->globalPosition().toPoint();
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_edgePressed) {
            const QPoint global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            if (!m_edgeDragging
                && (global - m_edgePressGlobal).manhattanLength() >= QApplication::startDragDistance()) {
                m_edgeDragging = true;
                m_groupHeader->setCursor(Qt::ClosedHandCursor);
                emit edgeDragStarted();
            }
            if (m_edgeDragging) {
                emit edgeDragMoved(global);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_edgePressed) {
            endEdgeDrag(false, static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            updateDrag(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            endDrag();
            return true;
        }
    }
    // Workspace preview: Arrange's inside edge zones resize one axis.
    if (m_groupHeader && (watched == m_edgeRight || watched == m_edgeBottom)
        && !m_locked && !isPanelDocked()) {
        if (event->type() == QEvent::MouseButtonPress
            && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            m_resizeAxes = watched == m_edgeRight ? Qt::Horizontal : Qt::Vertical;
            beginResize(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            updateResize(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
            endResize();
            m_resizeAxes = Qt::Horizontal | Qt::Vertical;
            return true;
        }
    }

    // Title bar drag
    if ((watched == m_titleBar || watched == m_titleLabel) && !m_locked) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !isPanelDocked()) {
                beginDrag(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateDrag(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            endDrag();
            return true;
        }
    }

    // Content area hover — detect mouse in title region when title bar is hidden
    // (hidden title bar collapses in layout, so content fills top area)
    bool isContentArea = (watched == m_contentHolder || watched == m_content);
    if (!isContentArea && m_content) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->isAncestorOf(m_content) == false && m_content->isAncestorOf(w)) {
            isContentArea = true;
        }
    }
    if (!m_structuredChrome && isContentArea && event->type() == QEvent::MouseMove && !m_locked) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        bool noControls = m_noControls && !(QApplication::keyboardModifiers() & Qt::ShiftModifier);
        if (!noControls) {
            QWidget* w = static_cast<QWidget*>(watched);
            int y = w->mapTo(this, me->position().toPoint()).y();
            if (y < kTitleHoverZone && !m_titleBar->isVisible()) {
                m_titleBar->setVisible(true);
                m_titleBar->raise();
            } else if (y >= kTitleBarHeight && m_titleBar->isVisible() && !m_dragging) {
                m_titleBar->setVisible(false);
            }
        }
    }

    // Resize grip (not for panel-docked)
    if (watched == m_resizeGrip && !m_locked && !m_autoHeight && !isPanelDocked()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizeAxes = Qt::Horizontal | Qt::Vertical;
                beginResize(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateResize(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
            endResize();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

// --- Drag logic ---

void ContainerWidget::beginDrag(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:281-294
    m_dragging = true;
    emit geometryInteractionStarted();
    // Closed hand while the drag runs (native shape; see the Qt 6.11 macOS
    // cursor crash note at the title label). Set on the bar too, since the
    // press can land on the bar beside the label.
    m_titleBar->setCursor(Qt::ClosedHandCursor);
    m_titleLabel->setCursor(Qt::ClosedHandCursor);
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::ClosedHandCursor);
    }
    if (isFloating()) {
        m_dragStartPos = globalPos - parentWidget()->pos();
        m_dragOrigin = parentWidget()->pos();
    } else {
        raise();
        m_dragStartPos = globalPos - pos();
        m_dragOrigin = pos();
    }
}

void ContainerWidget::updateDrag(const QPoint& globalPos)
{
    if (!m_dragging) {
        return;
    }

    if (isFloating()) {
        // From Thetis ucMeter.cs:319-345
        QPoint newPos = globalPos - m_dragStartPos;
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            newPos.setX(roundToNearestTen(newPos.x()));
            newPos.setY(roundToNearestTen(newPos.y()));
        }
        if (parentWidget() && parentWidget()->pos() != newPos) {
            parentWidget()->move(newPos);
        }
    } else {
        // From Thetis ucMeter.cs:346-374 — overlay-docked, clamped
        QPoint newPos = globalPos - m_dragStartPos;
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            newPos.setX(roundToNearestTen(newPos.x()));
            newPos.setY(roundToNearestTen(newPos.y()));
        }
        if (parentWidget()) {
            int maxX = parentWidget()->width() - width();
            int maxY = parentWidget()->height() - height();
            newPos.setX(std::clamp(newPos.x(), 0, std::max(0, maxX)));
            newPos.setY(std::clamp(newPos.y(), 0, std::max(0, maxY)));
        }
        if (pos() != newPos) {
            move(newPos);
            update();
        }
    }
}

void ContainerWidget::endDrag()
{
    // Workspace preview: optional grid alignment after release; Alt at
    // release places without it.
    if (m_groupHeader && m_gridPx > 1 && !(QApplication::keyboardModifiers() & Qt::AltModifier)) {
        if (isFloating() && parentWidget()) {
            const QPoint snapped = workspaceSnappedTopLeft(parentWidget()->pos(), m_gridPx);
            if (snapped != parentWidget()->pos()) {
                parentWidget()->move(snapped);
            }
        } else if (isOverlayDocked() && parentWidget()) {
            QPoint snapped = workspaceSnappedTopLeft(pos(), m_gridPx);
            snapped.setX(std::clamp(snapped.x(), 0, std::max(0, parentWidget()->width() - width())));
            snapped.setY(std::clamp(snapped.y(), 0, std::max(0, parentWidget()->height() - height())));
            if (snapped != pos()) {
                move(snapped);
            }
        }
    }
    m_dragging = false;
    emit geometryInteractionFinished();
    m_dragStartPos = QPoint();
    m_titleBar->unsetCursor();
    m_titleLabel->setCursor(Qt::OpenHandCursor);
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::OpenHandCursor);
    }
    if (isOverlayDocked()) {
        m_dockedLocation = pos();
        emit dockedMoved();
    }
}

void ContainerWidget::cancelDrag()
{
    // Back to where the drag began, so the commit after it changes nothing.
    if (!m_dragging) {
        return;
    }
    if (isFloating() && parentWidget()) {
        parentWidget()->move(m_dragOrigin);
    } else {
        move(m_dragOrigin);
    }
    m_dragging = false;
    emit geometryInteractionFinished();
    m_dragStartPos = QPoint();
    m_titleBar->unsetCursor();
    m_titleLabel->setCursor(Qt::OpenHandCursor);
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::OpenHandCursor);
    }
}

void ContainerWidget::endEdgeDrag(bool cancelled, const QPoint& globalPos)
{
    const bool wasDragging = m_edgeDragging;
    m_edgePressed = false;
    m_edgeDragging = false;
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::OpenHandCursor);
    }
    if (!wasDragging) {
        return;
    }
    if (cancelled) {
        emit edgeDragCancelled();
    } else {
        emit edgeDragFinished(globalPos);
    }
}

// --- Resize logic ---

void ContainerWidget::beginResize(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:400-407
    m_resizeStartGlobal = globalPos;
    m_resizeStartSize = isFloating() && parentWidget() ? parentWidget()->size() : size();
    m_resizing = true;
    emit geometryInteractionStarted();
    raise();
}

void ContainerWidget::updateResize(const QPoint& globalPos)
{
    if (!m_resizing) {
        return;
    }

    // From Thetis ucMeter.cs:489-518
    int dX = globalPos.x() - m_resizeStartGlobal.x();
    int dY = globalPos.y() - m_resizeStartGlobal.y();

    int newW = m_resizeStartSize.width() + dX;
    int newH = m_resizeStartSize.height() + dY;
    // Workspace preview edge zones resize one axis; otherwise both.
    if (!m_resizeAxes.testFlag(Qt::Horizontal)) {
        newW = m_resizeStartSize.width();
    }
    if (!m_resizeAxes.testFlag(Qt::Vertical)) {
        newH = m_resizeStartSize.height();
    }

    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        newW = roundToNearestTen(newW);
        newH = roundToNearestTen(newH);
    }

    doResize(newW, newH);
}

void ContainerWidget::endResize()
{
    m_resizing = false;
    emit geometryInteractionFinished();
    m_resizeStartGlobal = QPoint();
    if (isOverlayDocked()) {
        m_dockedSize = size();
    }
}

void ContainerWidget::doResize(int w, int h)
{
    // From Thetis ucMeter.cs:520-549
    w = std::max(w, std::max(kMinContainerWidth, minimumSizeHint().width()));
    h = std::max(h, std::max(kMinContainerHeight, minimumSizeHint().height()));

    if (isFloating()) {
        if (parentWidget()) {
            parentWidget()->resize(w, h);
        }
    } else {
        if (parentWidget()) {
            if (x() + w > parentWidget()->width()) {
                w = parentWidget()->width() - x();
            }
            if (y() + h > parentWidget()->height()) {
                h = parentWidget()->height() - y();
            }
        }
        QSize newSize(w, h);
        if (newSize != size()) {
            resize(newSize);
            update();
        }
    }
}

// --- Interactive item signal forwarding ---

void ContainerWidget::wireInteractiveItem(MeterItem* item)
{
    if (item->property("interactiveContainerWired").toBool()) { return; }
    item->setProperty("interactiveContainerWired",true);
    const auto forward = [this,item](const std::function<void()>& action) {
        if (item->property("containerUnsupportedSource").toBool()) {
            emit unavailableButtonClicked(item->property("unsupportedSourceReason").toString()); return;
        }
        const QVariant previous = property("containerDispatchContext");
        const QVariant context = item->property("containerSourceContext");
        if (context.isValid()) { setProperty("containerDispatchContext",context); }
        action(); setProperty("containerDispatchContext",previous);
    };
    if (auto* box = qobject_cast<ButtonBoxItem*>(item)) {
        connect(box, &ButtonBoxItem::unavailableButtonClicked, this,
                [this](int, const QString& reason) {
            emit unavailableButtonClicked(reason);
        });
    }
    if (auto* band = qobject_cast<BandButtonItem*>(item)) {
        connect(band, &BandButtonItem::bandClicked,
                this, [this,forward](int value) { forward([&] { emit bandClicked(value); }); });
        connect(band, &BandButtonItem::bandStackRequested,
                this, [this,forward](int value) { forward([&] { emit bandStackRequested(value); }); });
    } else if (auto* mode = qobject_cast<ModeButtonItem*>(item)) {
        connect(mode, &ModeButtonItem::modeClicked,
                this, [this,forward](int value) { forward([&] { emit modeClicked(value); }); });
    } else if (auto* filter = qobject_cast<FilterButtonItem*>(item)) {
        connect(filter, &FilterButtonItem::filterClicked,
                this, [this,forward](int value) { forward([&] { emit filterClicked(value); }); });
        connect(filter, &FilterButtonItem::filterContextRequested,
                this, [this,forward](int value) { forward([&] { emit filterContextRequested(value); }); });
    } else if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
        connect(ant, &AntennaButtonItem::antennaSelected,
                this, [this,forward](int value) { forward([&] { emit antennaSelected(value); }); });
        // Phase 3P-I-a T17 — late-added antenna items inherit the
        // container's current hasAlex state (set by MainWindow on
        // connect / currentRadioChanged via setBoardCapabilities).
        ant->setHasAlex(m_hasAlex);
    } else if (auto* step = qobject_cast<TuneStepButtonItem*>(item)) {
        connect(step, &TuneStepButtonItem::tuneStepSelected,
                this, [this,forward](int value) { forward([&] { emit tuneStepSelected(value); }); });
    } else if (auto* other = qobject_cast<OtherButtonItem*>(item)) {
        connect(other, &OtherButtonItem::otherButtonClicked,
                this, [this,forward](int value) { forward([&] { emit otherButtonClicked(value); }); });
        connect(other, &OtherButtonItem::macroTriggered,
                this, [this,forward](int value) { forward([&] { emit macroTriggered(value); }); });
    } else if (auto* voice = qobject_cast<VoiceRecordPlayItem*>(item)) {
        connect(voice, &VoiceRecordPlayItem::voiceAction,
                this, [this,forward](int value) { forward([&] { emit voiceAction(value); }); });
    } else if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
        connect(vfo, &VfoDisplayItem::frequencyChangeRequested,
                this, [this,forward](int64_t value) { forward([&] { emit frequencyChangeRequested(value); }); });
        connect(vfo, &VfoDisplayItem::bandStackRequested,
                this, [this,forward](int value) { forward([&] { emit bandStackRequested(value); }); });
        connect(vfo, &VfoDisplayItem::filterContextRequested,
                this, [this,forward](int) { forward([&] { emit vfoFilterContextRequested(); }); });
    }
}

// --- Serialization ---

QString ContainerWidget::axisLockToString(AxisLock lock)
{
    static const char* names[] = {
        "LEFT", "TOPLEFT", "TOP", "TOPRIGHT",
        "RIGHT", "BOTTOMRIGHT", "BOTTOM", "BOTTOMLEFT"
    };
    int idx = static_cast<int>(lock);
    return (idx >= 0 && idx < 8) ? QString::fromLatin1(names[idx]) : QStringLiteral("TOPLEFT");
}

AxisLock ContainerWidget::axisLockFromString(const QString& str)
{
    static const QMap<QString, AxisLock> map = {
        {QStringLiteral("LEFT"), AxisLock::Left},
        {QStringLiteral("TOPLEFT"), AxisLock::TopLeft},
        {QStringLiteral("TOP"), AxisLock::Top},
        {QStringLiteral("TOPRIGHT"), AxisLock::TopRight},
        {QStringLiteral("RIGHT"), AxisLock::Right},
        {QStringLiteral("BOTTOMRIGHT"), AxisLock::BottomRight},
        {QStringLiteral("BOTTOM"), AxisLock::Bottom},
        {QStringLiteral("BOTTOMLEFT"), AxisLock::BottomLeft},
    };
    return map.value(str.toUpper(), AxisLock::TopLeft);
}

QString ContainerWidget::dockModeToString(DockMode mode)
{
    switch (mode) {
    case DockMode::PanelDocked:   return QStringLiteral("PANEL");
    case DockMode::OverlayDocked: return QStringLiteral("OVERLAY");
    case DockMode::Floating:      return QStringLiteral("FLOATING");
    }
    return QStringLiteral("OVERLAY");
}

DockMode ContainerWidget::dockModeFromString(const QString& str)
{
    if (str == QStringLiteral("PANEL")) {
        return DockMode::PanelDocked;
    }
    if (str == QStringLiteral("FLOATING")) {
        return DockMode::Floating;
    }
    return DockMode::OverlayDocked;
}

QString ContainerWidget::serialize() const
{
    // Based on Thetis ucMeter.cs:1012-1038, extended with DockMode (field 23)
    QStringList p;
    p << m_id;                                                              // 0
    p << QString::number(m_rxSource);                                       // 1
    p << QString::number(m_dockedLocation.x());                             // 2
    p << QString::number(m_dockedLocation.y());                             // 3
    p << QString::number(m_dockedSize.width());                             // 4
    p << QString::number(m_dockedSize.height());                            // 5
    p << (isFloating() ? QStringLiteral("true") : QStringLiteral("false")); // 6 (Thetis compat)
    p << QString::number(m_delta.x());                                      // 7
    p << QString::number(m_delta.y());                                      // 8
    p << axisLockToString(m_axisLock);                                      // 9
    p << (m_pinOnTop ? QStringLiteral("true") : QStringLiteral("false"));   // 10
    p << (m_border ? QStringLiteral("true") : QStringLiteral("false"));     // 11
    p << m_backgroundColor.name(QColor::HexArgb);                           // 12
    p << (m_noControls ? QStringLiteral("true") : QStringLiteral("false")); // 13
    p << (m_enabled ? QStringLiteral("true") : QStringLiteral("false"));    // 14
    p << m_notes;                                                           // 15
    p << (m_containerMinimises ? QStringLiteral("true") : QStringLiteral("false")); // 16
    p << (m_autoHeight ? QStringLiteral("true") : QStringLiteral("false")); // 17
    p << (m_showOnRx ? QStringLiteral("true") : QStringLiteral("false"));   // 18
    p << (m_showOnTx ? QStringLiteral("true") : QStringLiteral("false"));   // 19
    p << (m_locked ? QStringLiteral("true") : QStringLiteral("false"));     // 20
    p << (m_containerHidesWhenRxNotUsed ? QStringLiteral("true") : QStringLiteral("false")); // 21
    p << (m_hiddenByMacro ? QStringLiteral("true") : QStringLiteral("false")); // 22
    p << dockModeToString(m_dockMode);                                      // 23 (NereusSDR extension)
    p << (m_titleBarVisible ? QStringLiteral("true") : QStringLiteral("false")); // 24 (Phase 3G-6)
    return p.join(QLatin1Char('|'));
}

bool ContainerWidget::deserialize(const QString& data)
{
    // Based on Thetis ucMeter.cs:1039-1160, backward-compatible
    if (data.isEmpty()) {
        return false;
    }

    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 13) {
        qCWarning(lcContainer) << "deserialize: too few fields:" << p.size();
        return false;
    }

    if (p[0].isEmpty()) {
        return false;
    }
    setId(p[0]);

    bool ok = false;
    int rx = p[1].toInt(&ok);
    if (!ok) { return false; }
    // R-R3-49 / R-R3-21 (fix wave M5): slices A to D are 1..4. A value
    // outside them (a hand-edited or damaged layout) loads as the nearest
    // slice, so the title and the buttons' reasons always name one.
    setRxSource(std::clamp(rx, kFirstRxSource, kLastRxSource));

    int x = p[2].toInt(&ok); if (!ok) { return false; }
    int y = p[3].toInt(&ok); if (!ok) { return false; }
    int w = p[4].toInt(&ok); if (!ok) { return false; }
    int h = p[5].toInt(&ok); if (!ok) { return false; }
    setDockedLocation(QPoint(x, y));
    setDockedSize(QSize(w, h));

    // Field 6: Thetis floating bool — used as fallback if field 23 absent
    bool thetisFloating = (p[6].toLower() == QStringLiteral("true"));

    x = p[7].toInt(&ok); if (!ok) { return false; }
    y = p[8].toInt(&ok); if (!ok) { return false; }
    setDelta(QPoint(x, y));

    setAxisLock(axisLockFromString(p[9]));
    setPinOnTop(p[10].toLower() == QStringLiteral("true"));
    setBorder(p[11].toLower() == QStringLiteral("true"));

    QColor bgColor(p[12]);
    if (bgColor.isValid()) {
        m_backgroundColor = bgColor;
        publishUserBackground();
        m_contentHolder->setStyleSheet(
            QStringLiteral("background: %1;").arg(bgColor.name(QColor::HexArgb)));
    }

    if (p.size() > 13) { setNoControls(p[13].toLower() == QStringLiteral("true")); }
    if (p.size() > 14) { setContainerEnabled(p[14].toLower() == QStringLiteral("true")); }
    if (p.size() > 15) { setNotes(p[15]); }
    if (p.size() > 16) { setContainerMinimises(p[16].toLower() == QStringLiteral("true")); }
    if (p.size() > 17) { setAutoHeight(p[17].toLower() == QStringLiteral("true")); }
    if (p.size() > 19) {
        setShowOnRx(p[18].toLower() == QStringLiteral("true"));
        setShowOnTx(p[19].toLower() == QStringLiteral("true"));
    }
    if (p.size() > 20) { setLocked(p[20].toLower() == QStringLiteral("true")); }
    if (p.size() > 21) { setContainerHidesWhenRxNotUsed(p[21].toLower() == QStringLiteral("true")); }
    if (p.size() > 22) { setHiddenByMacro(p[22].toLower() == QStringLiteral("true")); }

    // Field 23: NereusSDR DockMode extension
    if (p.size() > 23) {
        setDockMode(dockModeFromString(p[23]));
    } else {
        setDockMode(thetisFloating ? DockMode::Floating : DockMode::OverlayDocked);
    }

    // Field 24: Phase 3G-6 title-bar visibility toggle
    if (p.size() > 24) {
        setTitleBarVisible(p[24].toLower() == QStringLiteral("true"));
    }

    qCDebug(lcContainer) << "Deserialized:" << m_id << "rx:" << m_rxSource
                          << "mode:" << dockModeToString(m_dockMode)
                          << "pos:" << m_dockedLocation << "size:" << m_dockedSize;
    return true;
}

// Phase 3P-I-a T17 — propagate hasAlex to every AntennaButtonItem
// nested inside this container's content tree. Walks all MeterWidgets
// reachable from m_content via findChildren (handles nested applet
// panels that host a header MeterWidget as well as flat content).
void ContainerWidget::setBoardCapabilities(const BoardCapabilities& caps)
{
    m_hasAlex = caps.hasAlex;
    if (!m_content) { return; }

    // The content may be the MeterWidget directly, or an applet panel
    // hosting one as its header. findChildren catches both.
    const QList<MeterWidget*> meters =
        m_content->findChildren<MeterWidget*>();
    QList<MeterWidget*> scan = meters;
    if (auto* asMeter = qobject_cast<MeterWidget*>(m_content)) {
        scan.prepend(asMeter);
    }
    for (MeterWidget* mw : scan) {
        for (MeterItem* item : mw->items()) {
            if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
                ant->setHasAlex(m_hasAlex);
            }
        }
    }
}

} // namespace NereusSDR
