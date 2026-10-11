#pragma once

// =================================================================
// src/gui/containers/ContainerManager.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-11 - Windows remember their monitor and no two share a default
//                 spot: screens are read through a ScreenSource, a window
//                 whose monitor is absent is shown on the main window's
//                 screen with its remembered place left alone
//                 (screensChanged), and every default placement goes
//                 through defaultWindowRect. A layout saved with no pane
//                 size takes the pane's settled size for its anchor. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Containers keep the place the operator gave them: one
//                 remembered rect per container, changed only by an
//                 operator move (rememberOperatorGeometry) or a document
//                 commit and the only geometry saveState writes; free
//                 containers are shown from it once the pane has its size
//                 (placeFreeContainers). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - applyEdgeStripContentHints and
//                 scheduleEdgeStripContentHints: a strip's floor follows
//                 its containers' content minimum at once. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - geometryCommitPendingForTest, so tests wait on the
//                 geometry capture itself rather than a fixed time. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - A duplicated container is always shown, floating, next
//                 to its original (showDuplicate), in both looks. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Containers > Hidden reads hiddenItems() in both looks.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: hidden containers and entries are
//                 listed for Containers > Hidden and shown again where they
//                 were; a container whose entries are all hidden leaves the
//                 screen with them. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the header source menu commits the
//                 container source as Container Settings does (and Active
//                 slice as rxSourceMode followSelectedRx); a following
//                 container's chip reads Active. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview review fixes: edge drag mode
//                 tracking. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: geometry capture suppression for
//                 workspace profile switches by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: four-edge strips, Move to Edge,
//                 Place Free, the edge drag preview and grid alignment by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: app-level Arrange mode by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-04 — Track pending container placement by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 — Draft-only edits and inert cached previews by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

#include <QObject>
#include <QList>
#include <QMap>
#include <QSize>
#include <QPointer>
#include <QTimer>
#include <QSet>
#include "ContainerDocument.h"

#include <functional>
#include <optional>

class QSplitter;
class QSplitterHandle;
class QWidget;

namespace NereusSDR {

class ContainerWorkspaceStore;
class EdgeDockArea;
class ContainerContentRegistry;
class ContainerContentHost;
class ContainerWidget;
class FloatingContainer;
class MeterItem;
class MeterPoller;
class MeterWidget;
class ScreenSource;
struct ScreenInfo;
enum class DockMode;

// Factory that materializes the inner content widget for a restored
// container. MainWindow registers one so that the panel container gets
// an AppletPanelWidget while user-created containers get a bare
// MeterWidget. If unset, ContainerManager defaults to a fresh
// MeterWidget — sufficient for unit tests and the common case.
using ContainerContentFactory =
    std::function<QWidget*(const QString& id, int rxSource)>;

class ContainerArrangeController;

class ContainerManager : public QObject {
    Q_OBJECT

public:
    explicit ContainerManager(QWidget* dockParent, QSplitter* splitter,
                              QObject* parent = nullptr);
    ~ContainerManager() override;

    void setWorkspaceAdapter(ContainerWorkspaceStore* store, ContainerContentRegistry* registry);
    ContainerWorkspaceStore* workspaceStore() const;
    ContainerContentRegistry* contentRegistry() const;
    ContainerContentHost* contentHost(const QString& id) const;
    void reconcileWorkspace(const WorkspaceDocument& document);
    CommitResult commitWorkspace(const WorkspaceDocument& document, quint64 expectedRevision);
    void setTransmitting(bool transmitting);
    void setPreviewPoller(MeterPoller* poller);
    MeterPoller* previewPoller() const;
    ContainerArrangeController* arrangeController() const { return m_arrange; }
    // Workspace preview: Arrange shows every header and the resize handles.
    // Runtime only, never saved; locks still hold.
    void setArrangeMode(bool on);
    bool arrangeMode() const { return m_arrangeMode; }
    QString storageError() const { return m_storageError; }
    bool isReconciling() const { return m_reconciling; }
    // Workspace profiles: while suppressed, no geometry is captured into
    // the workspace (a profile apply settles its layout first). Turning it
    // off drops what the apply's layout passes queued.
    void setGeometryCaptureSuppressed(bool on);
    bool geometryCaptureSuppressed() const { return m_geometryCaptureSuppressed; }

    // --- Workspace preview: four-edge strips ---
    // Set before the first reconcile. With an area and the preview on,
    // PanelDocked containers go to the strip for their edge, in document
    // order, instead of the splitter.
    void setEdgeDockArea(EdgeDockArea* area);
    EdgeDockArea* edgeDockArea() const;
    // Docks a container in `edge`'s strip at `index` (the position among
    // the strip's containers as shown; -1 appends). Commits a draft.
    void moveContainerToEdge(const QString& id, DockEdge edge, int index = -1);
    // Places a container free over the spectrum pane (OverlayDocked).
    void placeContainerFree(const QString& id);
    // Optional 8 px alignment for free and floating placement, saved in
    // the workspace arrangement. Off by default.
    void setGridAlignment(bool on);
    bool gridAlignment() const;
    // Test and preview seam: the target bands and a destination preview
    // at `edge`, `index`, as during a header drag.
    void showDropPreviewForTest(DockEdge edge, int index);
    void clearDropPreviewForTest();
    // Test seam: a geometry capture is scheduled and has not run yet.
    bool geometryCommitPendingForTest() const { return m_geometryCommit.isActive(); }

    // --- Workspace preview: the header source menu ---
    // Whether a slice id exists, for the menu's enabled slices. MainWindow
    // sets it from the RadioModel; unset, every slice counts.
    void setSliceAvailability(std::function<bool(int sliceId)> available);
    // Commits a container's source as Container Settings' Slice combo does:
    // `rxSource` 1..4 is a fixed slice (A to D), and
    // ContainerWidget::kActiveSliceSource follows the selected receiver.
    // Returns true when the draft was saved.
    bool commitContainerSource(const QString& id, int rxSource);

    // --- Container lifecycle ---
    ContainerWidget* createContainer(int rxSource, DockMode mode);
    void destroyContainer(const QString& id);

    // Create a new floating container that clones the user-editable
    // state of an existing container. Meter items are NOT copied here;
    // block 3's dialog rewrite wires the Duplicate button and will
    // copy items via MeterWidget::serializeItems round-trip.
    // From Thetis MeterManager.cs duplicate-container path.
    ContainerWidget* duplicateContainer(const QString& id);

    // How far a duplicate sits from its original, down and to the right,
    // so it does not cover it exactly. NereusSDR-original value.
    static constexpr int kDuplicateOffsetPx = 24;
    // Makes `copy`, a duplicate of `original`, a shown floating container
    // (visible, enabled, at least one widget showing) next to the
    // original's floating geometry, or its on-screen rect when docked,
    // offset by kDuplicateOffsetPx. Reconcile keeps it on screen. Used by
    // duplicateContainer and by Container Settings > Duplicate.
    void showDuplicate(ContainerDocument& copy, const ContainerDocument& original) const;

    // --- Dock mode transitions ---
    void floatContainer(const QString& id);
    void dockContainer(const QString& id);
    void panelDockContainer(const QString& id);
    void overlayDockContainer(const QString& id);
    void recoverContainer(const QString& id);

    // --- Axis-lock repositioning (overlay-docked only) ---
    // With a workspace store this shows the free containers in a pane of
    // (hDelta, vDelta), from their remembered places; it never re-bases them.
    void updateDockedPositions(int hDelta, int vDelta);

    // --- The place the operator gave a container ---
    // Each container has one remembered rect, kept in the document's
    // `geometry`. It changes only here and through document commits (menu
    // commands, dialogs); a move the app makes for itself (restore,
    // clamping, layout, a screen change) never changes it, and saveState()
    // writes it, never the widget's rect.
    //
    // Takes the container's rect as it is now for its remembered place.
    // Called when the operator finishes a drag or a grip resize that moved
    // or resized it. For a free container it also records the pane's size,
    // the baseline its anchor works from. Nothing while geometry capture is
    // suppressed.
    void rememberOperatorGeometry(const QString& id);
    // Shows every free container from its remembered place in the dock
    // parent's current rect: anchored by its AxisLock for the pane's change
    // since the place was remembered, then pulled inside the pane. Display
    // only; safe to call repeatedly. Nothing until the dock parent has a
    // size of its own (a top-level one always has; a child one from its
    // first resize, which also places them).
    void placeFreeContainers();

    // --- Windows and their monitors ---
    // How far a default placement steps, down and to the right, past a
    // spot another container already has. NereusSDR-original value.
    static constexpr int kCascadeOffsetPx = 24;
    // A default place for a floating window, in screen coordinates:
    // `wanted`, held on its screen, then moved down and right by
    // kCascadeOffsetPx as many times as needed until no visible free or
    // floating container has the same top-left.
    QRect defaultWindowRect(const QRect& wanted) const;
    // Test seam: the screens every placement here consults (the
    // application's own when null). Not owned.
    void setScreenSourceForTest(ScreenSource* source);
    // A screen came or went. A floating window whose monitor is absent is
    // shown on the main window's screen; one whose monitor is back returns
    // to its remembered place. Display only: nothing is committed.
    void screensChanged();

    // --- Panel width (QSplitter state) ---
    void saveSplitterState();
    void restoreSplitterState();

    // --- Persistence ---
    void saveState();
    void restoreState();

    // Register the content factory used by restoreState() to populate
    // each restored container. Must be set before restoreState().
    void setContentFactory(ContainerContentFactory factory);

    // --- Queries ---
    QList<ContainerWidget*> allContainers() const;
    ContainerWidget* container(const QString& id) const;
    ContainerWidget* panelContainer() const;
    int containerCount() const;

    // --- Unit-mode fan-out (Task 3.2) ---
    // Iterates every MeterItem across all containers and invokes fn on each.
    // Used by MultimeterPage to broadcast unit-mode / show-decimal changes
    // to all live MeterItem instances without requiring the items to poll
    // AppSettings on every paint.
    void forEachMeterItem(std::function<void(MeterItem*)> fn);

    // --- Visibility ---
    void setContainerVisible(const QString& id, bool visible);

    // --- Containers > Hidden (both looks) ---
    // One hidden thing: a container (entryId empty) or an entry. `label`
    // is the container's name, or "Entry (Container)".
    struct HiddenItem {
        QString containerId;
        QString entryId;
        QString label;
    };
    // Every hidden container (its saved visibility off, or disabled) and
    // every hidden entry, in document order. Empty with no store.
    QVector<HiddenItem> hiddenItems() const;
    // Shows a hidden container where it was (visible and enabled; its
    // placement and geometry are kept). Returns true when saved.
    bool showHiddenContainer(const QString& id);
    // Shows a hidden entry, and its container with it, where it was.
    bool showHiddenEntry(const QString& entryId);
    // The title an entry is shown with: its name, else its type's title.
    QString entryTitle(const ContentEntry& entry) const;

signals:
    void containerAdded(const QString& id);
    void containerRemoved(const QString& id);
    // Emitted when a container's notes change (notes are the source
    // of the display title). MainWindow consumes this in commit 45 to
    // rebuild the Containers → Edit Container submenu.
    void containerTitleChanged(const QString& id, const QString& title);
    // Emitted whenever a MeterWidget becomes (or appears as a
    // descendant of) a container's content. MainWindow connects this
    // to MeterPoller::addTarget so user-created and restored
    // containers both flow into the WDSP/MMIO poll loop. Without this
    // hookup the poller only knew about the panel container's initial
    // m_meterWidget and every other container's items sat at default
    // values forever — symptom: bars frozen at frac=0, needles never
    // moving.
    void meterReadyForPolling(MeterWidget* meter);
    void meterContextReady(MeterWidget* meter, const QJsonObject& context);
    void workspaceReconciled();
    void previewPresentationRequested(MeterWidget* meter, const QJsonObject& context);
    void workspaceError(const QString& error);
    void arrangeModeChanged(bool on);
    void gridAlignmentChanged(bool on);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    bool commitDockMode(const QString& id, DockMode mode);
    bool edgeMode() const;
    void layoutEdgeStrips(const WorkspaceDocument& document);
    // Each edge strip's floor and preferred cross size from its containers
    // (the side strips' floors alone when `sideStripsOnly`).
    void applyEdgeStripContentHints(bool sideStripsOnly);
    void scheduleEdgeStripContentHints();
    void captureArrangement(WorkspaceDocument& document) const;
    // True when the edge-area handle borders a locked container, or a strip
    // that holds one.
    bool edgeHandleTouchesLockedContainer(QSplitterHandle* handle) const;
    void finishEdgeDrag(const QString& id, const QPoint& globalPos);
    // Size for a strip container leaving its strip: its width, its
    // content's height.
    QSize freePlacementSize(const QString& id, const ContainerDocument& d) const;
    QPointer<EdgeDockArea> m_edgeArea;
    QString m_edgeDragId;
    // The dragged container's mode when its edge drag started.
    DockMode m_edgeDragMode = DockMode::PanelDocked;
    void applyWorkspaceHeader(ContainerWidget* c, const ContainerDocument& d);
    bool m_arrangeMode = false;
    bool m_geometryCaptureSuppressed = false;
    ContainerArrangeController* m_arrange = nullptr;
    QPointer<MeterPoller> m_previewPoller;
    QPointer<ContainerWorkspaceStore> m_store;
    QPointer<ContainerContentRegistry> m_registry;
    std::function<bool(int)> m_sliceAvailable;
    bool m_reconciling = false;
    bool m_transmitting = false;
    bool effectiveVisible(const ContainerDocument& document) const;
    // A child dock parent has no size of its own before its first resize.
    bool dockParentLaidOut() const;
    void placeFreeContainersIn(const QSize& pane);
    // The rect a container has now: its shell's when floating.
    QRect liveGeometry(const QString& id) const;
    // Remembered places, by container id. The pane size is the baseline a
    // free container's anchor works from; absent until the pane has a size.
    QMap<QString, QRect> m_rememberedGeometry;
    QMap<QString, QSize> m_rememberedPane;
    QMap<QString, std::optional<QSize>> m_documentPane;
    // Each container's rect when its drag or grip resize started.
    QMap<QString, QRect> m_interactionStart;
    bool m_dockParentResized = false;
    // A place saved with no pane size takes the pane's size once the pane
    // has stopped changing after its first layout; until then that
    // baseline follows the pane.
    bool m_paneSettled = false;
    QTimer m_paneSettle;
    QSet<QString> m_provisionalPane;
    // --- Windows and their monitors ---
    QRect defaultWindowRect(const QRect& wanted, const QString& exceptId) const;
    // defaultWindowRect for a free container, in the pane's coordinates.
    QRect defaultFreeRect(const QRect& wanted, const QString& exceptId) const;
    // True when a floating window can be shown at its remembered place.
    bool windowIsHome(const QRect& remembered, const std::optional<SavedScreen>& screen) const;
    // Where a window that cannot be shown at its remembered place goes.
    QRect displacedWindowRect(const QString& id, const QRect& remembered,
                              const std::optional<SavedScreen>& screen, const QSize& size) const;
    // Shows a window at its remembered place when it is home; otherwise
    // where displacedWindowRect says, unless it is already shown away
    // from its place on a present screen. `fromDocument`: the document
    // was just applied, so a window that is home is put at its place.
    void placeFloatingWindow(const QString& id, const QRect& remembered,
                             const std::optional<SavedScreen>& screen, FloatingContainer* form,
                             bool fromDocument);
    // `wanted` on `screen`, off every other visible container's top-left.
    QRect windowRectOn(const QRect& wanted, const ScreenInfo& screen, const QString& exceptId) const;
    ScreenInfo mainWindowScreen() const;
    // The usable height of the screen `target` is on.
    int availableHeightFor(const QWidget* target) const;
    ScreenSource* m_screens{nullptr};
    // The monitor each floating window's remembered place was chosen on.
    QMap<QString, std::optional<SavedScreen>> m_rememberedScreen;
    // Floating windows shown away from their remembered place.
    QSet<QString> m_displaced;
    QString m_storageError;
    QTimer m_geometryCommit;
    bool m_edgeHintsPending = false;
    QSet<QString> m_pendingGeometry;
    QMap<QString, QRect> m_documentGeometry;
    void setMeterFloating(ContainerWidget* container, FloatingContainer* form);
    void returnMeterFromFloating(ContainerWidget* container, FloatingContainer* form);
    void wireContainer(ContainerWidget* container);

    // Windows D3D11 QRhiWidget (WA_NativeWindow) cannot survive the HWND
    // recreation that setParent() triggers when moving a container between
    // top-level windows. Call extractMeterItems() before a reparent to
    // serialize and detach the MeterWidget, then installFreshMeter() after
    // the reparent to rebuild meter content in the new parent's HWND
    // context. No-op when the container holds placeholder content.
    QString extractMeterItems(ContainerWidget* container);
    void    installFreshMeter(ContainerWidget* container, const QString& payload);

    QWidget* m_dockParent{nullptr};
    QSplitter* m_splitter{nullptr};
    QMap<QString, ContainerWidget*> m_containers;
    QMap<QString, FloatingContainer*> m_floatingForms;
    QString m_panelContainerId;
    ContainerContentFactory m_contentFactory;

    // Restore path guard. During restoreState() a Floating container
    // goes through setContent(placeholder) + setMeterFloating (which
    // does extractMeterItems/installFreshMeter to survive the native-
    // window reparent), triggering contentChanged → meterReadyForPolling
    // twice: once for the soon-to-be-discarded placeholder, once for
    // the final fresh meter. Listeners (MeterPoller) ended up tracking
    // the placeholder and the real meter, with the placeholder dangling
    // after setMeterFloating destroyed it. Setting this flag suppresses
    // the interim announcements; restoreState emits exactly one manual
    // announcement per container after the final content is in place.
    bool m_suppressMeterAnnouncements{false};
};

} // namespace NereusSDR
