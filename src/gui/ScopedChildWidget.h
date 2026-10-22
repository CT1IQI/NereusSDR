// =================================================================
// src/gui/ScopedChildWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/ScopedChildWidget.h [@383f058], original header from
//   Longpath source is included below. Longpath took it from AetherSDR
//   (https://github.com/aethersdr/AetherSDR, src/gui/ScopedChildWidget.h
//   [@b9e96a94]); AetherSDR carries no per-file header and is GPLv3 at
//   project level.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22  Ported for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted transformation via Claude Code. Namespace
//               NereusSDR; every right-click and drop-down menu that was
//               built on the stack with a parent now uses this owner.
//   2026-10-22  Constructor arguments forwarded, so stack dialogs
//               (QMessageBox, QColorDialog, NereusSDR's own) and a QDrag
//               use it too. J.J. Boyd (KG4VCF), with AI-assisted
//               transformation via Claude Code.
// =================================================================
//
// --- From ScopedChildWidget.h ---
//
// =================================================================
// src/gui/ScopedChildWidget.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/ScopedChildWidget.h [@b9e96a94]
//
// AetherSDR (https://github.com/aethersdr/AetherSDR) fuehrt keine
// Kopfzeile je Datei; es gilt die Projektlizenz (GPLv3), dieselbe wie
// hier. Attribution auf Projektebene nach HOW-TO-PORT.md Regel 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-22 — Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude. Namensraum geaendert,
//                 Begruendung ausgeschrieben.
// =================================================================
//
// Ein QMenu oder QDialog auf dem Stapel, dessen Elternteil waehrend
// exec() stirbt, wird ZWEIMAL abgeraeumt: einmal von Qt (jedes Kind
// stirbt mit seinem Elternteil) und einmal beim Verlassen des Rahmens.
// Das passiert nicht nur beim Beenden — jede Aktion, die das eigene
// Fenster schliesst, kann es ausloesen, denn exec() dreht eine eigene
// Ereignisschleife, und in der laeuft alles weiter.
//
// Dieser Halter legt das Kind auf den Haufen, behaelt es in einem
// QPointer und raeumt es beim Verlassen des Rahmens auf — aber nur,
// wenn es noch da ist. Elternschaft, Menueverhalten und Modalitaet
// bleiben unveraendert; nur der Besitz wandert.
//
//     ScopedChildWidget<QMenu> owner(this);
//     QMenu& menu = *owner.get();
//     ...
//     if (menu.exec(pos) == action) { ... }   // auch wenn `this` stirbt
//
// Wer nach exec() noch etwas am Elternteil tut, prueft zusaetzlich mit
// einem eigenen QPointer, dass es das Elternteil noch gibt.
// =================================================================

#pragma once

#include <QPointer>
#include <QWidget>

#include <utility>

namespace NereusSDR {

// From Longpath src/gui/ScopedChildWidget.h:47-69 [@383f058]
// NereusSDR: the one explicit delete is the point of the helper: Qt may
// already have destroyed the widget with its parent, so ownership cannot
// sit in a unique_ptr; the QPointer says whether anything is left to free.
template <typename Widget>
class ScopedChildWidget final {
public:
    // NereusSDR: the constructor forwards every argument, so a dialog whose
    // constructor takes more than a parent (QMessageBox's icon, title and
    // text; a custom dialog's model) is built the same way. AetherSDR made
    // the same change in 9befcd24. With one QWidget* it is Longpath's.
    template <typename... Args>
    explicit ScopedChildWidget(Args&&... args)
        : m_widget(new Widget(std::forward<Args>(args)...))
    {
    }

    ~ScopedChildWidget()
    {
        // Der QPointer ist leer, wenn das Elternteil schon aufgeraeumt hat.
        delete m_widget.data();
    }

    ScopedChildWidget(const ScopedChildWidget&) = delete;
    ScopedChildWidget& operator=(const ScopedChildWidget&) = delete;

    Widget* get() const { return m_widget.data(); }
    explicit operator bool() const { return !m_widget.isNull(); }

private:
    QPointer<Widget> m_widget;
};

} // namespace NereusSDR
