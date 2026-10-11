# Container headers, arranging and placement design

Status: design, agreed with JJ (KG4VCF) on 2026-10-10. Mock-ups, drawn over
real captures of the workspace look, are in
[2026-10-10-container-arrange-design/](2026-10-10-container-arrange-design/).
They show intent, not final art.

## Goal

Make the workspace look's containers easy to arrange and trustworthy to leave
alone:

* a clean screen with headers hidden, without losing the way to move a panel;
* one way to move a panel anywhere: drag it, and the edge it will land on
  shows itself;
* every panel and window starts where it was left, at the size it was left.

## Scope

The workspace look (`NEREUS_WORKSPACE_PREVIEW=1`). With the look off, the
chrome, menus and behaviour stay as they are, with one exception: the
placement rule in "Where things are on startup" fixes a bug that both looks
share, so it applies to both.

## Decisions (JJ, 2026-10-10)

| Question | Decision |
| --- | --- |
| Header choices | Two: shown or hidden. Each container remembers its own. The hover choice ("Reveal") is removed |
| Changing it | An eye button on the header (plain eye: the header stays; crossed-out eye: the header hides when not arranging), and "Keep Header Showing" in the right-click menu. No "hide all" or "show all" command |
| Arrange, held | Holding Shift alone for about a third of a second shows every header and enables arranging until Shift is let go. A click, a wheel turn or any other key while Shift is down cancels it, so Shift-tuning never triggers it |
| Arrange, switched on | The menu item and Ctrl+Shift+A stay. Escape now leaves Arrange |
| Arrange cue | None added. Every header showing and the existing highlight are the cue. No bar, no toast |
| Dragging while arranging | One rule for every container: near an edge of the main window the edge lights and a box shows where the panel will land; dropped over the spectrum it sits free; dropped outside the main window it pops out; a popped-out window dropped over the main window docks or sits free where it is dropped. Escape cancels a drag |
| Dragging when not arranging | As today |
| Pop out | "Pop Out" is added to the container's right-click menu |
| Stay on top | The pin button returns to a popped-out window's header, beside the return arrow, using the app's existing pin and pinned icons |
| Return | The return arrow puts a container back in the spot it was popped out of |
| Lock | Removed. A container with no header has no handle, so it cannot be moved by accident; moving anything else takes Arrange |
| Startup | Every panel and window starts where it was left, at the size it was left. The app moves a window only when its monitor is gone, and puts it back when the monitor returns. Nothing is reshuffled at startup |

Settled in this spec, listed for JJ's veto:

* A saved "Reveal" header loads as hidden. It was hidden most of the time, and
  Shift now does what the hover did.
* A saved locked container loads unlocked. Nothing else about it changes.
* "Reveal controls" leaves the right-click menu; holding Shift replaces it.
* "Axis Lock", "Move to Edge" and "Place Free" stay in the header's Options
  menu as they are. Dragging is the main way; the menu is the keyboard way.
* The two snap aids for a free panel (the 8 px grid, and the 10 px step while
  Ctrl is held) become one: the 8 px grid, which Alt bypasses.
* The pin button is sized to the header like its neighbours, not as large as
  in the mock-up.

## What exists today

All in `src/gui/containers/`.

* `HeaderMode { Always, Reveal, Hidden }` (`ContainerTypes.h`), saved per
  container as the integer `"header"` (`ContainerDocumentCodec.cpp`).
* The workspace header (`WorkspaceHeader.cpp`) is 18 px high with Pop out,
  Options and Close buttons, and a padlock that shows when a container is
  locked. Its Options menu (`ContainerWidget.cpp`) holds Axis Lock, Pin on Top
  (floating only), Lock, Pop Out or Dock, Container Settings, Header (three
  choices), Move to Edge, Place Free, Arrange Workspace and Contents.
* The body's right-click menu holds Container Settings, Hide container,
  Reveal controls, Header, Arrange Workspace and Contents. It has no Pop Out.
* The older title bar has a pin button (icons `pin` and `pinned` under
  `resources/icons/emoji/`). The look hides that title bar, so the only pin
  control left is the menu item.
* Dragging starts only from a visible header. A container in a strip docks
  when dragged within 48 px of an edge, with a preview
  (`ContainerManager.cpp`, `EdgeDockStrip.cpp`). A free container is kept
  inside the spectrum pane and cannot be dragged to an edge or out of the
  window. A popped-out window only moves; it cannot be dragged back in.
* Arrange Workspace is global, is not saved, and Escape does not leave it.
* The return arrow sends the main container to its strip and any other
  container to a free spot that is not the one it came from.
* Shift is in use for tuning steps on the spectrum, in the RX applet, on the
  VFO and in the equaliser editor. Every one of those uses Shift together with
  a click, a wheel turn or a drag.

### The stacking bug

Containers have come up on top of each other after a start. Read from the
code, in order of likelihood (none of these has a test today, and the first
is not yet reproduced):

1. Free containers are restored inside the `MainWindow` constructor, before
   the window is laid out. The spectrum pane they sit on is still at its
   unlaid-out size, and each container is clamped into it
   (`ContainerManager.cpp`, the reconcile step that calls `clampedGeometry`).
   They all land at the top left. The move and resize events that follow the
   first show then start the 200 ms save, and the clamped positions are saved
   as if the operator had chosen them.
2. A floating or popped-out window that is not wholly inside one screen is
   recentred on the main window (`FloatingContainer.cpp`,
   `ensureVisiblePosition`), with no offset between windows.
3. Starting with a smaller main window than at save pins free containers to an
   edge, and that position is saved too.
4. New pop-outs, new containers, Place Free and Recover all use one default
   spot with no offset.
5. Unplugging a monitor while running clamps windows to the nearest edge.
6. The first run with the look on copies free positions from the look-off
   layout, which was measured for a pane of a different width.

Saving itself is sound: the whole file is written atomically with a backup,
one profile cannot be opened twice, and a build that does not understand the
saved layout goes read-only rather than resetting it.

## Design

### Headers

`HeaderMode` becomes two values, shown and hidden. The saved integer keeps its
meaning for those two; the old middle value reads as hidden and is never
written again. The hover timers and focus rules that served Reveal are
removed.

The header gains an eye button to the left of Pop out. It is a toggle with a
tooltip that says what a click will do ("Hide this header when not arranging"
or "Keep this header showing"). The right-click menu's "Keep Header Showing"
is the same toggle, checked when the header is shown.

A header is drawn when its container's choice is shown, or while arranging.
Arranging never changes a container's choice.

### Arrange

Arranging is on when the switch is on (menu item or Ctrl+Shift+A) or while the
Shift hold is active. The two are independent: letting go of Shift never turns
the switch off.

The Shift hold, in one place in the main window's event handling:

* It starts a timer when Shift goes down with no other modifier, no mouse
  button down, and focus not in a text field. The delay is a named constant,
  0.3 s.
* Any mouse press, wheel turn or other key before the timer fires cancels it
  for that press of Shift.
* When the timer fires, arranging begins. It ends when Shift is released,
  unless a container drag is in progress, in which case it ends when the drag
  ends.
* Popped-out windows take part: Shift held while one of them is active shows
  headers in all of them and in the main window.

Escape leaves the switched-on Arrange when no drag is in progress. During a
drag, Escape cancels the drag and puts the container back where it started.

### Dragging while arranging

The drag handle is the header. While arranging, one rule decides where a
dragged container lands, whatever it was before:

| The pointer is | Result on release |
| --- | --- |
| Within the edge zone of the main window (48 px, as today) | Docks in that edge's strip at the previewed place. The edge lights and a box shows the place |
| Elsewhere over the spectrum pane | Sits free at that position, snapped to the 8 px grid unless Alt is held |
| Outside the main window | Pops out as a window at that position |

A popped-out window follows the same table when its header is dragged while
arranging. When not arranging, dragging a window only moves it, as today, so a
window can be parked over the main window without docking.

The edge preview is the one strips use today, extended to free and popped-out
sources. A container dragged out of a strip leaves a gap that closes on
release, not during the drag, so the layout does not jump under the pointer.

### Pop out, pin and return

* "Pop Out" in the body's right-click menu does what the header's Pop out
  button does.
* A popped-out window's header shows the pin button to the left of the return
  arrow: the `pin` icon when not on top, `pinned` when on top. The "Pin on
  Top" menu item stays and shows the same state.
* When a container is popped out, by button, menu or drag, the document
  records where it came from: the strip and its place in the order, or its
  free position and size. The return arrow puts it back there. If the place no
  longer exists (the strip is gone, or the free spot is outside the pane), it
  goes to the nearest place that fits and does not cover another free
  container. A container that has never been docked returns to the main
  strip.

### Lock

The setting, its menu item, the header padlock and the rule that held a strip
divider beside a locked container are removed. The saved `"locked"` field is
read and ignored, and no longer written.

### Where things are on startup

One rule: **the app never moves a container and then saves that as if the
operator had moved it.**

* **Remembered place and shown place are separate.** Each container keeps the
  position and size the operator last gave it. What is drawn may differ for a
  while (the window is too small, a monitor is missing); the remembered values
  are what is saved.
* **Only the operator's moves are saved.** A move or resize is written to the
  remembered place when it comes from a drag, a resize grip, a menu command or
  a settings dialog. Moves the app makes for itself (restore, clamping,
  layout, screen changes) are not.
* **Free containers are placed once the pane has its real size.** Restore
  creates them, and they are positioned after the main window's first layout,
  not in the constructor.
* **A window too small for a free container** shows it pulled inside the
  pane, and keeps the remembered place. When the pane is large enough again
  the container is back where it was.
* **Popped-out and floating windows remember their monitor** as well as their
  position and size. On start, a window whose monitor is present is put
  exactly where it was, with no "wholly inside the screen" test. A window
  whose monitor is absent, or whose remembered place is on no screen at all,
  is shown on the main window's screen and keeps its remembered monitor and
  place, so it returns there on a later start when the monitor is back.
* **Unplugging a monitor while running** follows the same rule: shown on a
  remaining screen, remembered place unchanged.
* **Two windows never get the same default spot.** A new pop-out, a new
  container, Place Free and Recover each offset from any container already at
  the spot they would use.
* **Look off to look on.** The first copy of free positions from the look-off
  layout is treated as remembered places for the look-on layout and goes
  through the same rule; nothing is clamped into the saved copy.

### Saved layout

Changes to the container document (`ContainerDocumentCodec.cpp`), all
readable by older builds, which keep unknown fields:

* `"header"`: written as shown or hidden only.
* `"locked"`: ignored on read, not written.
* New: where a popped-out container came from (strip and index, or free
  rectangle).
* New: the monitor a floating or popped-out window was on (name and serial,
  with its geometry at save, so a different monitor in the same place is not
  mistaken for it).

No schema version change is intended; the new fields are optional. The plan
confirms that against the codec's version handling before any field is added.

## Errors

* A saved monitor that cannot be matched: the window opens on the main
  window's screen; a line goes to the log category the container manager
  uses. No dialog.
* A saved place that fails to parse: that container takes a default place
  with an offset; the rest of the layout is unaffected.
* A drag that ends where nothing can take the container: the drag is
  cancelled.

## Acceptance and evidence

Software, offscreen tests:

1. Header: the eye and the menu item change one container only; a saved
   Reveal loads hidden; while arranging every header is drawn and no choice
   changes.
2. Shift hold: begins after the delay; does not begin when a click, wheel or
   key comes first, or when a text field has focus; ends on release; a drag in
   progress keeps it alive.
3. Drag rule: for a strip, a free and a popped-out source, a release in each
   of the three zones gives the result in the table; Escape restores the
   start.
4. Return: a container popped out of a strip, and one popped out of a free
   spot, return to the same place; the fallback never covers another free
   container.
5. Lock: a document with locked containers loads, saves without the field,
   and its dividers move.
6. Startup, one test per cause in "The stacking bug", each red before the
   change: a full `MainWindow` round trip with two free containers; a window
   smaller at start than at save, then large again; two windows on an absent
   monitor, then the monitor present again; two new pop-outs; the look-off to
   look-on copy. In every case the saved layout is unchanged after a start and
   quit with no operator action.

Rendered, by the capture tool at both profiles: the header with the eye in
both states; arranging; the edge preview for a free and a popped-out source;
the popped-out header with the pin in both states; the right-click menu. With
the look off, every existing scene is identical.

Native window, on macOS, with JJ's OK for the run: the drag across the main
window's edge in both directions and the Shift hold, since offscreen tests
cannot prove pointer behaviour between native views.

By JJ, on a build of his choosing: arrange his own layout, quit and start
several times with a window on each monitor, and confirm nothing moved.

## Mock-ups

* [Arranging](2026-10-10-container-arrange-design/1-arranging.png): every
  header shown, with the eye on each.
* [Header close-up](2026-10-10-container-arrange-design/1b-header-buttons.png):
  the eye in both states.
* [Drag to an edge](2026-10-10-container-arrange-design/2-drag-to-edge.png):
  the lit edge and the landing box.
* [Popped-out header](2026-10-10-container-arrange-design/3-popped-out-pin.png):
  the pin beside the return arrow. Drawn too large; see the veto list.
* [Right-click menu](2026-10-10-container-arrange-design/4-right-click-menu.png):
  today and proposed.

## Out of scope

* The chrome and menus with the look off.
* What containers hold, and the Contents menu.
* Strip scrolling and strip sizing, other than removing the Lock rule on
  dividers.
* Named layout presets.
* The iPhone app.
