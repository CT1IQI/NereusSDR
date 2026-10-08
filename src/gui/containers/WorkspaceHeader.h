#pragma once

// src/gui/containers/WorkspaceHeader.h

// --- From src/gui/applets/GridCellWidget.cpp ---
// =================================================================
// src/gui/applets/GridCellWidget.cpp  (Longpath)
// =================================================================
// Siehe GridCellWidget.h — Kopfleiste plus N Inhalte.
//
// Die Kopfleiste ist Zeile fuer Zeile das, was
// AppletPanelWidget::wrapWithTitleBar bisher gebaut hat (Griff ⋮⋮ in
// kTextScale 10 px, Titel in kTitleText 10 px fett, Style::
// titleBarStyle(), Hoehe kTitleBarH bzw. 22 mit nachgestelltem
// Widget). Schritt 1 soll dasselbe Bild ergeben; wer hier etwas
// aendert, aendert das Aussehen jedes Applets auf einmal.
// =================================================================

// =================================================================
// src/gui/containers/WorkspaceHeader.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/applets/GridCellWidget.cpp at revision
//   551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banner is copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
// The compact workspace headers of the workspace preview: the group
// header a container carries at its top (stripe, grab handle, title,
// source chip, lock glyph, pop-out and options buttons), the entry
// header above each entry of a multi-content container, and the corner
// resize grip drawn inside a container's bounds while Arrange is on.
// The layout follows Longpath's GridCellWidget title bar; the values
// that are not Longpath's (heights, the entry header, the chip, the
// lock glyph, the dot corner) are the workspace review's proposal.
// Reachable only while WorkspacePreview::enabled() is true.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The source chip is a click target: it opens the source
//                 menu. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-08 - The entry header carries a pop-out button left of its
//                 options button. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Both headers end with a hide button (Longpath's close
//                 cross, last and rightmost); it hides, never deletes.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QColor>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

namespace NereusSDR {

// The source chip ("RX A") in a group header: mono text in the slice
// colour on a dark chip with a slice-coloured border. A click on it opens
// the source menu, so it accepts its own presses like the header buttons.
class WorkspaceSourceChip final : public QWidget {
    Q_OBJECT
public:
    explicit WorkspaceSourceChip(QWidget* parent = nullptr);
    void setSource(const QString& text, const QColor& colour);
    QString text() const { return m_text; }
    QSize sizeHint() const override;
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
private:
    QString m_text;
    QColor m_colour;
    bool m_pressed{false};
};

// A small padlock, drawn so it takes the header's glyph colour.
class WorkspaceLockGlyph final : public QWidget {
    Q_OBJECT
public:
    explicit WorkspaceLockGlyph(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
};

// The 12x12 bottom-right resize corner: three diagonals of dots.
class WorkspaceCornerGrip final : public QWidget {
    Q_OBJECT
public:
    static constexpr int kSize = 12;
    explicit WorkspaceCornerGrip(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
};

// A container's group header. Dragging the header (its stripe, grab
// handle, title or empty area) moves the container; the owner installs an
// event filter on the header for that. The buttons and the source chip
// accept their own presses, so a press on them never reaches the filter.
class WorkspaceGroupHeader final : public QWidget {
    Q_OBJECT
public:
    static constexpr int kHeight = 18;

    explicit WorkspaceGroupHeader(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    QString title() const;
    // Empty text hides the chip.
    void setSource(const QString& text, const QColor& colour);
    QString sourceText() const;
    void setLocked(bool locked);
    // ↗ (pop out) while docked, ↙ (return) while floating.
    void setFloating(bool floating);
    // Arrange: the bottom border becomes the accent.
    void setArrangeAccent(bool on);
    bool arrangeAccent() const { return m_arrange; }

    QWidget* grip() const;
    QPushButton* popOutButton() const { return m_popOut; }
    QPushButton* optionsButton() const { return m_options; }
    // The hide button, last and rightmost. Enabled exactly when the
    // pop-out button is (setLocked).
    QPushButton* hideButton() const { return m_hide; }
    WorkspaceSourceChip* sourceChip() const { return m_chip; }

signals:
    void popOutClicked();
    void optionsClicked();
    void sourceClicked();
    void hideClicked();

private:
    void applyStyle();

    QLabel* m_grip{nullptr};
    QLabel* m_title{nullptr};
    WorkspaceSourceChip* m_chip{nullptr};
    WorkspaceLockGlyph* m_lock{nullptr};
    QPushButton* m_popOut{nullptr};
    QPushButton* m_options{nullptr};
    QPushButton* m_hide{nullptr};
    bool m_arrange{false};
};

// The header above one entry of a multi-content container. The owner
// sets its drag property on the header itself; the pop-out, options and
// hide buttons accept their own presses. The owner enables the pop-out
// button exactly when the entry menu's Pop Out is enabled, and the hide
// button with it (setActionsEnabled); neither is ever hidden.
class WorkspaceEntryHeader final : public QWidget {
    Q_OBJECT
public:
    static constexpr int kHeight = 16;

    explicit WorkspaceEntryHeader(const QString& entryId, QWidget* parent = nullptr);

    QString entryId() const { return m_entryId; }
    void setTitle(const QString& title);
    QString title() const;
    // Over content (not in a layout): a translucent plate behind it.
    void setOverlay(bool overlay);
    QPushButton* popOutButton() const { return m_popOut; }
    QPushButton* optionsButton() const { return m_options; }
    QPushButton* hideButton() const { return m_hide; }
    // Pop out and hide, enabled together.
    void setActionsEnabled(bool enabled);

signals:
    void popOutClicked();
    void optionsClicked();
    void hideClicked();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_entryId;
    QLabel* m_grip{nullptr};
    QLabel* m_title{nullptr};
    QPushButton* m_popOut{nullptr};
    QPushButton* m_options{nullptr};
    QPushButton* m_hide{nullptr};
    bool m_overlay{false};
};

} // namespace NereusSDR
