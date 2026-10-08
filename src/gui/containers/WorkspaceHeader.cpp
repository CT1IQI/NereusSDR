// src/gui/containers/WorkspaceHeader.cpp

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
// src/gui/containers/WorkspaceHeader.cpp  (NereusSDR)
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
// See WorkspaceHeader.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "WorkspaceHeader.h"

#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspaceStyle.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>

namespace NereusSDR {

namespace {

// Workspace review proposal (Task 3 brief), not Longpath values.
constexpr int kGroupGripWidth = 14;
constexpr int kStripeWidth = 3;                 // From Longpath src/gui/applets/GridCellWidget.cpp:73 [@551576e3]
constexpr int kGroupButtonWidth = 16;           // From Longpath src/gui/applets/GridCellWidget.cpp:155 [@551576e3]
constexpr int kGroupButtonHeight = 14;          // From Longpath src/gui/applets/GridCellWidget.cpp:155 [@551576e3]
constexpr int kEntryButtonWidth = 14;
constexpr int kEntryButtonHeight = 12;
constexpr int kEntryGripWidth = 12;
constexpr int kGroupGripPx = 11;                // From Longpath src/gui/applets/GridCellWidget.cpp:83 [@551576e3]
constexpr int kEntryGripPx = 10;
constexpr int kChipHeight = 13;
constexpr int kChipRadius = 4;
constexpr int kChipPadding = 4;
constexpr int kChipBorderAlpha = 153;           // 60 % of 255
constexpr int kLockSize = 9;
constexpr int kHeaderTopRadius = 9;             // kGlassPanelRadius - 1, From Longpath src/gui/applets/GridCellWidget.cpp:63 [@551576e3]
constexpr int kOverlayPlateAlpha = 224;

const QString kGripGlyph = QStringLiteral("⋮⋮");   // From Longpath src/gui/applets/GridCellWidget.cpp:81 [@551576e3]

QString headerButtonCss()
{
    // From Longpath src/gui/applets/GridCellWidget.cpp:144-152 [@551576e3]
    return QStringLiteral(
        "QPushButton { background: transparent; border: none;"
        "  color: %1; font-size: 11px; padding: 0; }"
        "QPushButton:hover { background: %2; color: %3;"
        "  border-radius: %4px; }"
        "QPushButton:disabled { color: %5; }")
        .arg(Workspace::role("text-scale", Workspace::kTextScale),
             Workspace::role("button-hover", Workspace::kButtonHover),
             Workspace::role("text", Workspace::kTextPrimary))
        .arg(Workspace::kGlassChipRadius)   // nie Radius 3 (Hausstil)
        .arg(Workspace::role("text-inactive", Workspace::kTextInactive));
}

QPushButton* makeButton(const QString& glyph, int w, int h, const QString& tip, QWidget* parent)
{
    auto* b = new QPushButton(glyph, parent);
    b->setFixedSize(w, h);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    b->setToolTip(tip);
    b->setStyleSheet(headerButtonCss());
    return b;
}

QLabel* makeGrip(int px, const char* colour, const char* hover, int w, int h, QWidget* parent)
{
    auto* grip = new QLabel(kGripGlyph, parent);
    grip->setObjectName(QStringLiteral("workspaceGrip"));
    grip->setAlignment(Qt::AlignCenter);
    grip->setFixedSize(w, h);
    grip->setCursor(Qt::OpenHandCursor);
    grip->setAttribute(Qt::WA_Hover, true);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; background: transparent; }"
        "QLabel:hover { color: %3; }")
                            .arg(QString::fromLatin1(colour))
                            .arg(px)
                            .arg(QString::fromLatin1(hover)));
    return grip;
}

} // namespace

// ---- WorkspaceSourceChip ----------------------------------------------------

WorkspaceSourceChip::WorkspaceSourceChip(QWidget* parent) : QWidget(parent)
{
    setFont(Workspace::monoFont(font(), Workspace::kFontCaption));
    setFixedHeight(kChipHeight);
    hide();
}

void WorkspaceSourceChip::setSource(const QString& text, const QColor& colour)
{
    m_text = text;
    m_colour = colour;
    setFixedWidth(sizeHint().width());
    setVisible(!text.isEmpty());
    update();
}

QSize WorkspaceSourceChip::sizeHint() const
{
    const QFontMetrics fm(font());
    return {fm.horizontalAdvance(m_text) + 2 * kChipPadding + 2, kChipHeight};
}

void WorkspaceSourceChip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor border = m_colour;
    border.setAlpha(kChipBorderAlpha);
    p.setPen(QPen(border, 1.0));
    p.setBrush(QColor(Workspace::role("tile-active-bg", Workspace::kTileActiveBg)));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kChipRadius, kChipRadius);
    p.setPen(m_colour);
    p.drawText(rect(), Qt::AlignCenter, m_text);
}

// ---- WorkspaceLockGlyph -----------------------------------------------------

WorkspaceLockGlyph::WorkspaceLockGlyph(QWidget* parent) : QWidget(parent)
{
    setFixedSize(kLockSize, kLockSize);
    setToolTip(tr("Locked: placement and size are fixed until you unlock"));
    hide();
}

void WorkspaceLockGlyph::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor c(Workspace::role("text-scale", Workspace::kTextScale));
    // Shackle: an arc over the body.
    p.setPen(QPen(c, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(2.0, 0.6, 5.0, 6.0), 0, 180 * 16);
    p.drawLine(QPointF(2.0, 3.6), QPointF(2.0, 4.4));
    p.drawLine(QPointF(7.0, 3.6), QPointF(7.0, 4.4));
    // Body.
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(0.5, 4.0, 8.0, 5.0), 1.0, 1.0);
}

// ---- WorkspaceCornerGrip ----------------------------------------------------

WorkspaceCornerGrip::WorkspaceCornerGrip(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("workspaceCornerGrip"));
    setFixedSize(kSize, kSize);
    setCursor(Qt::SizeFDiagCursor);
    setToolTip(tr("Drag to resize"));
}

void WorkspaceCornerGrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Workspace::role("text-scale", Workspace::kTextScale)));
    // Three diagonals of dots toward the corner: 1, 2 and 3 dots.
    constexpr double kStep = 3.5;
    constexpr double kDot = 1.0;
    for (int diagonal = 0; diagonal < 3; ++diagonal) {
        for (int i = 0; i <= diagonal; ++i) {
            const double x = kSize - 2.0 - i * kStep;
            const double y = kSize - 2.0 - (diagonal - i) * kStep;
            p.drawEllipse(QPointF(x, y), kDot, kDot);
        }
    }
}

// ---- WorkspaceGroupHeader ---------------------------------------------------

WorkspaceGroupHeader::WorkspaceGroupHeader(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("workspaceGroupHeader"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kHeight);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);

    auto* row = new QHBoxLayout(this);
    // From Longpath src/gui/applets/GridCellWidget.cpp:66-67 [@551576e3]
    row->setContentsMargins(2, 0, 4, 0);
    row->setSpacing(4);

    // Die gelbe Anfassmarke, wie an den eigenen Fenstern und wie in
    // der Vorlage. Sie sagt „hier anfassen" — ein Kopf ohne diese Marke sieht
    // aus wie einer, den man nicht anfassen kann.
    auto* stripe = new QLabel(this);
    stripe->setObjectName(QStringLiteral("workspaceStripe"));
    stripe->setFixedSize(kStripeWidth, kHeight);
    // Aus der Palette: derselbe Ton wie an den Fensterleisten und an
    // den Containern (Style::kAmberText). Hier stand #d8a13a — eine
    // eigene Zahl fuer dieselbe Marke.
    stripe->setStyleSheet(QStringLiteral("background: %1;")
                              .arg(Workspace::role("measured", Workspace::kAmberText)));
    row->addWidget(stripe);

    m_grip = makeGrip(kGroupGripPx, Workspace::kTextScale, Workspace::kTextPrimary,
                      kGroupGripWidth, kHeight, this);
    m_grip->setToolTip(tr("Drag to move"));
    row->addWidget(m_grip);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("workspaceGroupTitle"));
    // Versal mit weiter Laufweite (Hausstil Regel 3, umgesetzt mit der
    // Richtung "Glas & Tiefe" am 2026-09-17). Die Groesse kommt aus
    // capsFont, nicht aus dem Stylesheet — dort gewinnt sie sonst
    // gegen jedes setFont darunter (HAUSSTIL.md, die Falle).
    m_title->setFont(Workspace::capsFont(font(), Workspace::kFontCaption));
    m_title->setStyleSheet(QStringLiteral("QLabel { color: %1; background: transparent; }")
                               .arg(Workspace::role("label-mid", Workspace::kLabelMid)));
    row->addWidget(m_title);

    m_chip = new WorkspaceSourceChip(this);
    row->addWidget(m_chip);

    m_lock = new WorkspaceLockGlyph(this);
    row->addWidget(m_lock);

    row->addStretch(1);

    m_popOut = makeButton(QStringLiteral("↗"), kGroupButtonWidth, kGroupButtonHeight,
                          tr("Pop out"), this);
    m_popOut->setObjectName(QStringLiteral("workspacePopOut"));
    connect(m_popOut, &QPushButton::clicked, this, &WorkspaceGroupHeader::popOutClicked);
    row->addWidget(m_popOut);

    m_options = makeButton(QStringLiteral("⋯"), kGroupButtonWidth, kGroupButtonHeight,
                           tr("Options"), this);
    m_options->setObjectName(QStringLiteral("workspaceOptions"));
    connect(m_options, &QPushButton::clicked, this, &WorkspaceGroupHeader::optionsClicked);
    row->addWidget(m_options);

    applyStyle();
}

void WorkspaceGroupHeader::applyStyle()
{
    // From Longpath src/gui/applets/GridCellWidget.cpp:59-63 [@551576e3]
    QString css = QStringLiteral(
        "QWidget#workspaceGroupHeader { %1 border-top-left-radius: %2px;"
        " border-top-right-radius: %2px; }")
                      .arg(Workspace::titleBarStyle())
                      .arg(kHeaderTopRadius);
    if (m_arrange) {
        css += QStringLiteral("QWidget#workspaceGroupHeader { border-bottom: 1px solid %1; }")
                   .arg(Workspace::role("accent", Workspace::kAccent));
    }
    setStyleSheet(css);
}

void WorkspaceGroupHeader::setTitle(const QString& title) { m_title->setText(title); }
QString WorkspaceGroupHeader::title() const { return m_title->text(); }
void WorkspaceGroupHeader::setSource(const QString& text, const QColor& colour)
{
    m_chip->setSource(text, colour);
}
QString WorkspaceGroupHeader::sourceText() const { return m_chip->isVisibleTo(this) ? m_chip->text() : QString(); }
void WorkspaceGroupHeader::setLocked(bool locked)
{
    m_lock->setVisible(locked);
    m_popOut->setEnabled(!locked);
}
void WorkspaceGroupHeader::setFloating(bool floating)
{
    m_popOut->setText(floating ? QStringLiteral("↙") : QStringLiteral("↗"));
    m_popOut->setToolTip(floating ? tr("Return") : tr("Pop out"));
}
void WorkspaceGroupHeader::setArrangeAccent(bool on)
{
    if (m_arrange == on) {
        return;
    }
    m_arrange = on;
    applyStyle();
}
QWidget* WorkspaceGroupHeader::grip() const { return m_grip; }

// ---- WorkspaceEntryHeader ---------------------------------------------------

WorkspaceEntryHeader::WorkspaceEntryHeader(const QString& entryId, QWidget* parent)
    : QWidget(parent), m_entryId(entryId)
{
    setObjectName(QStringLiteral("workspaceEntryHeader_") + entryId);
    setFixedHeight(kHeight);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("Drag to arrange; right-click for Move, Pop out and Return"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(2, 0, 4, 1);
    row->setSpacing(4);

    m_grip = makeGrip(kEntryGripPx, Workspace::kTextInactive, Workspace::kTextSecondary,
                      kEntryGripWidth, kHeight - 1, this);
    row->addWidget(m_grip);

    m_title = new QLabel(this);
    m_title->setFont(Workspace::capsFont(font(), Workspace::kFontCaption));
    m_title->setStyleSheet(QStringLiteral("QLabel { color: %1; background: transparent; }")
                               .arg(Workspace::role("text-scale", Workspace::kTextScale)));
    row->addWidget(m_title, 1);

    m_options = makeButton(QStringLiteral("⋯"), kEntryButtonWidth, kEntryButtonHeight,
                           tr("Entry options"), this);
    connect(m_options, &QPushButton::clicked, this, &WorkspaceEntryHeader::optionsClicked);
    row->addWidget(m_options);
}

void WorkspaceEntryHeader::setTitle(const QString& title) { m_title->setText(title); }
QString WorkspaceEntryHeader::title() const { return m_title->text(); }
void WorkspaceEntryHeader::setOverlay(bool overlay)
{
    if (m_overlay == overlay) {
        return;
    }
    m_overlay = overlay;
    update();
}

void WorkspaceEntryHeader::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    if (m_overlay) {
        QColor plate(Workspace::role("panel", Workspace::kPanelBg));
        plate.setAlpha(kOverlayPlateAlpha);
        p.fillRect(rect(), plate);
    }
    // Bottom hairline.
    p.fillRect(QRect(0, height() - 1, width(), 1),
               QColor(Workspace::role("border-subtle", Workspace::kBorderSubtle)));
}

} // namespace NereusSDR
