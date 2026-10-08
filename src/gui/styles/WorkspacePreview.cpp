// =================================================================
// src/gui/styles/WorkspacePreview.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See WorkspacePreview.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Written by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - themeOnShow() installs its filter once per dialog.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - installLiveTheming(): the application-wide live pass.
//                 themeOne() records the themed sheet before it sets it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The unknown-literal count leaves out the colours the
//                 workspace look itself writes, not only palette values.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - roleColour() and roleColourName() for painted chrome
//                 and rich text. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include "WorkspacePreview.h"

#include "QssTranslator.h"
#include "WorkspaceComponents.h"
#include "WorkspacePalette.h"
#include "WorkspaceStyle.h"

#include <QApplication>
#include <QEvent>
#include <QMetaObject>
#include <QObject>
#include <QSet>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#include <optional>

namespace NereusSDR::WorkspacePreview {

namespace {

std::optional<bool>& switchState()
{
    static std::optional<bool> state;
    return state;
}

QHash<QString, int>& unknownCounts()
{
    static QHash<QString, int> counts;
    return counts;
}

// Colours the workspace look writes itself: palette values, the
// colours componentize() derives from them, and the workspace sheets'
// own literals. A sheet the program copies from an already themed
// widget brings them back to the translator; they are not legacy
// literals.
const QSet<QString>& workspaceValues()
{
    static const QSet<QString> values = [] {
        QSet<QString> s;
        for (const auto& e : Workspace::WorkspacePalette::glassAndDepth().entries()) {
            s.insert(QString::fromLatin1(e.hex).toLower());
        }
        for (const QString& v : Workspace::Components::generatedColours()) {
            s.insert(v);
        }
        const QssTranslator::Table noRows;
        const QssTranslator::KeptList noKept;
        QssTranslator::Options options;
        options.table = &noRows;
        options.kept = &noKept;
        const QString sheets = QString::fromLatin1(Workspace::kScrollBarStyle)
            + Workspace::titleBarStyle();
        for (const QString& v : QssTranslator::translate(sheets, options).unknownLiterals) {
            s.insert(v);
        }
        return s;
    }();
    return values;
}

// Class names whose subtree the scoped pass leaves alone.
//  - The slice flag and its helpers (design-facts B11): VfoWidget,
//    VfoLevelBar, ScrollableLabel, the VfoModeContainers widgets and
//    VaxChannelSelector (which copies VfoStyles' mode-button look).
//  - SpectrumWidget: plot colours, filter overlays and the flag's
//    floating buttons live there; its chrome is themed at call sites.
//  - Colour pickers whose stylesheet shows a user-chosen colour.
constexpr const char* kSkippedClasses[] = {
    "NereusSDR::VfoWidget",
    "NereusSDR::VfoLevelBar",
    "NereusSDR::ScrollableLabel",
    "NereusSDR::FmOptContainer",
    "NereusSDR::DigOffsetContainer",
    "NereusSDR::RttyMarkShiftContainer",
    "NereusSDR::VaxChannelSelector",
    "NereusSDR::SpectrumWidget",
    "NereusSDR::ColorSwatchButton",
    "NereusSDR::GradientPickerWidget",
};

bool skippedClass(const QMetaObject* meta)
{
    // The live pass asks this for every ancestor of every widget shown,
    // so the answer is kept per class.
    static QHash<const QMetaObject*, bool> known;
    const auto it = known.constFind(meta);
    if (it != known.constEnd()) {
        return it.value();
    }
    bool skip = false;
    for (const QMetaObject* m = meta; m != nullptr && !skip; m = m->superClass()) {
        for (const char* cls : kSkippedClasses) {
            if (qstrcmp(m->className(), cls) == 0) {
                skip = true;
                break;
            }
        }
    }
    known.insert(meta, skip);
    return skip;
}

bool skipped(const QWidget* w)
{
    if (w->property(kSkipProperty).toBool()) {
        return true;
    }
    return skippedClass(w->metaObject());
}

// True when `w` or an ancestor in its own window is skipped. The walk
// stops at the window: a menu or popup a skipped widget opens is a
// surface of its own.
bool skippedInWindow(const QWidget* w)
{
    for (const QWidget* p = w; p != nullptr; p = p->parentWidget()) {
        if (skipped(p)) {
            return true;
        }
        if (p->isWindow()) {
            break;
        }
    }
    return false;
}

void themeOne(QWidget* w)
{
    const QString current = w->styleSheet();
    const QVariant themed = w->property(kThemedQssProperty);
    const QVariant stored = w->property(kOriginalQssProperty);

    QString original;
    bool fresh = false;
    if (!stored.isValid() || !themed.isValid() || current != themed.toString()) {
        // First visit, or the widget set a new stylesheet since the
        // last pass: that stylesheet is the new original.
        original = current;
        w->setProperty(kOriginalQssProperty, original);
        fresh = true;
    } else {
        original = stored.toString();
    }

    if (original.isEmpty()) {
        w->setProperty(kThemedQssProperty, original);
        return;
    }

    QssTranslator::Options options;
    options.resolveAmbiguousByContext = true;
    const QssTranslator::Result r = QssTranslator::translate(original, options);
    if (fresh) {
        // A sheet set by a call-site role (applied when the switch is
        // on) already holds palette values; those are not legacy
        // literals and are left out of the report.
        const QSet<QString>& ours = workspaceValues();
        for (const QString& lit : r.unknownLiterals) {
            if (!ours.contains(lit)) {
                ++unknownCounts()[lit];
            }
        }
    }
    const QString look = Workspace::Components::componentize(r.qss, w);
    // Recorded first: setStyleSheet() sends StyleChange, and the live
    // pass must see this sheet as already themed.
    w->setProperty(kThemedQssProperty, look);
    if (look != current) {
        w->setStyleSheet(look);
    }
}

void themeRecursive(QWidget* w)
{
    if (skipped(w)) {
        return;
    }
    themeOne(w);
    const QObjectList kids = w->children();
    for (QObject* o : kids) {
        if (o->isWidgetType()) {
            themeRecursive(static_cast<QWidget*>(o));
        }
    }
}

constexpr auto kShowFilterName = "workspacePreviewShowFilter";

class ShowFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show && watched->isWidgetType()) {
            themeSubtree(static_cast<QWidget*>(watched));
        }
        return QObject::eventFilter(watched, event);
    }
};

constexpr auto kLiveFilterName = "workspacePreviewLiveTheming";

// The live pass for one widget, from a Show or StyleChange event. Its
// children get their own Show events, so it does not recurse.
void themeLive(QWidget* w)
{
    const QVariant themed = w->property(kThemedQssProperty);
    if (themed.isValid() && themed.toString() == w->styleSheet()) {
        return;
    }
    if (skippedInWindow(w)) {
        return;
    }
    themeOne(w);
}

class LiveThemingFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // Every event of the application passes here: anything but Show
        // and StyleChange leaves after one comparison.
        const QEvent::Type type = event->type();
        if (type != QEvent::Show && type != QEvent::StyleChange) {
            return false;
        }
        if (!watched->isWidgetType() || !enabled()) {
            return false;
        }
        auto* w = static_cast<QWidget*>(watched);
        // A new stylesheet on a hidden widget waits for its next Show,
        // when its size (and so its radius) is known.
        if (type == QEvent::StyleChange && !w->isVisible()) {
            return false;
        }
        themeLive(w);
        return false;
    }
};

} // namespace

bool enabled()
{
    std::optional<bool>& state = switchState();
    if (!state.has_value()) {
        state = qEnvironmentVariable("NEREUS_WORKSPACE_PREVIEW") == QLatin1String("1");
    }
    return *state;
}

void setEnabledForTest(bool on)
{
    switchState() = on;
}

void themeSubtree(QWidget* root)
{
    if (root == nullptr || !enabled()) {
        return;
    }
    themeRecursive(root);
}

void themeOnShow(QWidget* dialog)
{
    if (dialog == nullptr || !enabled()) {
        return;
    }
    // The filter is a child of the dialog, so it goes with it. One per
    // dialog: a second call (a dialog rebuilt or shown again) finds it.
    if (dialog->findChild<QObject*>(QString::fromLatin1(kShowFilterName),
                                    Qt::FindDirectChildrenOnly) != nullptr) {
        return;
    }
    auto* filter = new ShowFilter(dialog);
    filter->setObjectName(QString::fromLatin1(kShowFilterName));
    dialog->installEventFilter(filter);
}

void installLiveTheming(QApplication& app)
{
    if (!enabled()) {
        return;
    }
    if (app.findChild<QObject*>(QString::fromLatin1(kLiveFilterName),
                                Qt::FindDirectChildrenOnly) != nullptr) {
        return;
    }
    auto* filter = new LiveThemingFilter(&app);
    filter->setObjectName(QString::fromLatin1(kLiveFilterName));
    app.installEventFilter(filter);
}

QString lookQss(const QString& original, const QWidget* owner)
{
    if (!enabled()) {
        return original;
    }
    QssTranslator::Options options;
    options.resolveAmbiguousByContext = true;
    return Workspace::Components::componentize(
        QssTranslator::translate(original, options).qss, owner);
}

QColor roleColour(const QColor& legacy, const char* role)
{
    if (!enabled()) {
        return legacy;
    }
    if (!legacy.isValid() || role == nullptr) {
        return legacy;
    }
    QColor c = Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(role));
    if (!c.isValid()) {
        return legacy;
    }
    c.setAlpha(legacy.alpha());
    return c;
}

QString roleColourName(const char* legacy, const char* role)
{
    if (!enabled()) {
        return QString::fromLatin1(legacy);
    }
    const QColor c = roleColour(QColor::fromString(QLatin1StringView(legacy)), role);
    return c.name(c.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb);
}

QHash<QString, int> unknownLiteralCounts()
{
    return unknownCounts();
}

void resetUnknownLiteralCounts()
{
    unknownCounts().clear();
}

void passInputThrough(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }
    widget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    widget->setWindowFlag(Qt::WindowTransparentForInput, true);
    if (QWindow* window = widget->windowHandle()) {
        window->setFlag(Qt::WindowTransparentForInput, true);
    }
}

} // namespace NereusSDR::WorkspacePreview
