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
// =================================================================

#include "WorkspacePreview.h"

#include "QssTranslator.h"
#include "WorkspaceComponents.h"
#include "WorkspacePalette.h"

#include <QEvent>
#include <QObject>
#include <QSet>
#include <QVariant>
#include <QWidget>

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

const QSet<QString>& paletteValues()
{
    static const QSet<QString> values = [] {
        QSet<QString> s;
        for (const auto& e : Workspace::WorkspacePalette::glassAndDepth().entries()) {
            s.insert(QString::fromLatin1(e.hex).toLower());
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

bool skipped(const QWidget* w)
{
    if (w->property(kSkipProperty).toBool()) {
        return true;
    }
    for (const char* cls : kSkippedClasses) {
        if (w->inherits(cls)) {
            return true;
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
        const QSet<QString>& paletteHex = paletteValues();
        for (const QString& lit : r.unknownLiterals) {
            if (!paletteHex.contains(lit)) {
                ++unknownCounts()[lit];
            }
        }
    }
    const QString look = Workspace::Components::componentize(r.qss, w);
    if (look != current) {
        w->setStyleSheet(look);
    }
    w->setProperty(kThemedQssProperty, look);
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
    // The filter is a child of the dialog, so it goes with it.
    auto* filter = new ShowFilter(dialog);
    dialog->installEventFilter(filter);
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

QHash<QString, int> unknownLiteralCounts()
{
    return unknownCounts();
}

void resetUnknownLiteralCounts()
{
    unknownCounts().clear();
}

} // namespace NereusSDR::WorkspacePreview
