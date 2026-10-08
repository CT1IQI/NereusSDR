// src/gui/styles/WorkspaceComponents.cpp

// --- From src/gui/StyleConstants.h ---
// =================================================================
// src/gui/StyleConstants.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Theme palette imported from AetherSDR `src/gui/ComboStyle.h` /
//                 `HGauge.h` / `SliceColors.h` and inline QColor calls
//                 in `MainWindow.cpp` / `VfoWidget.cpp`.
// =================================================================

// =================================================================
// src/gui/styles/WorkspaceComponents.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath src/gui/StyleConstants.h at revision
// 551576e30c42731c6d51ce005c5f23ec38be391c (values and fills only);
// see WorkspaceComponents.h. The upstream banner is copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//                 componentize() is NereusSDR-original.
// =================================================================

#include "WorkspaceComponents.h"

#include "WorkspacePalette.h"
#include "WorkspaceStyle.h"

#include <QColor>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace NereusSDR::Workspace::Components {

namespace {

// Hover of a raised control.
// From Longpath src/gui/StyleConstants.h:1119 [@551576e3]
constexpr auto kRaisedHoverTop = "#2a2a30";
constexpr auto kRaisedHoverBot = "#1a1a1e";
// Fields and value chips: sunkenFill(kInsetBg, 8, 4).
// From Longpath src/gui/StyleConstants.h:1020 [@551576e3]
constexpr int kFieldDeepen = 8;
constexpr int kFieldLift = 4;
// The light top edge of the selected border: shiftL(kGlassSelBorder, 16).
// From Longpath src/gui/StyleConstants.h:823 [@551576e3]
constexpr int kGlassBorderTopLift = 16;
// The light top edge of the slider handle border: shiftL(kAccent, 18).
// From Longpath src/gui/StyleConstants.h:918 [@551576e3]
constexpr int kHandleBorderTopLift = 18;

// NereusSDR workspace design: radii follow the control's real size.
constexpr int kRadiusSmall = 4;
constexpr int kRadiusLarge = 6;
constexpr int kRadiusSmallMaxHeight = 24;
// NereusSDR Style::kButtonH, used when the control's height is unknown.
constexpr int kDefaultControlHeight = 22;
// NereusSDR: lift of the borderless slider handle's accent pill.
constexpr int kAccentPillLift = 18;
constexpr int kAccentPillHoverLift = 30;
constexpr int kAccentPillHoverBase = 10;
// NereusSDR: the fill on a slider's value side keeps the groove's
// usual 2 px radius.
constexpr int kSliderFillRadius = 2;

QString hex(const char* h)
{
    return QString::fromLatin1(h);
}

QString vgrad(const QString& top, const QString& bottom)
{
    return QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)")
        .arg(top, bottom);
}

QString accentPillFill()
{
    const QColor accent(hex(kAccent));
    return vgrad(shiftL(accent, kAccentPillLift), accent.name());
}

QString accentPillHoverFill()
{
    const QColor accent(hex(kAccent));
    return vgrad(shiftL(accent, kAccentPillHoverLift), shiftL(accent, kAccentPillHoverBase));
}

QString squeeze(QString s)
{
    s.remove(QLatin1Char(' '));
    s.remove(QLatin1Char('\t'));
    s.remove(QLatin1Char('\n'));
    return s.toLower();
}

enum class Ours { None, Raised, Hover, Pressed, Glass, Field, AccentPill, AccentPillHover };

const QHash<QString, Ours>& ourFills()
{
    static const QHash<QString, Ours> fills = {
        { squeeze(raisedFill()), Ours::Raised },
        { squeeze(raisedHoverFill()), Ours::Hover },
        { squeeze(pressedFill()), Ours::Pressed },
        { squeeze(glassFill()), Ours::Glass },
        { squeeze(fieldFill()), Ours::Field },
        { squeeze(accentPillFill()), Ours::AccentPill },
        { squeeze(accentPillHoverFill()), Ours::AccentPillHover },
    };
    return fills;
}

const QSet<QString>& selectionColours()
{
    static const QSet<QString> set = [] {
        QSet<QString> s;
        const auto& p = WorkspacePalette::glassAndDepth();
        for (const char* r : { "sel-bg", "sel-hover", "sel-border", "accent", "selected-top",
                               "selected-bottom", "selected-border" }) {
            s.insert(p.color(QString::fromLatin1(r)).name());
        }
        return s;
    }();
    return set;
}

QString stripComments(const QString& s)
{
    QString out;
    out.reserve(s.size());
    for (qsizetype i = 0; i < s.size(); ++i) {
        if (s.at(i) == QLatin1Char('/') && i + 1 < s.size() && s.at(i + 1) == QLatin1Char('*')) {
            const qsizetype end = s.indexOf(QLatin1String("*/"), i + 2);
            if (end < 0) {
                break;
            }
            i = end + 1;
            continue;
        }
        out += s.at(i);
    }
    return out;
}

QColor parseColour(const QString& raw)
{
    const QString v = raw.trimmed();
    if (v.startsWith(QLatin1Char('#'))) {
        return QColor(v);
    }
    const QString lower = v.toLower();
    if (lower.startsWith(QLatin1String("rgb"))) {
        const qsizetype open = v.indexOf(QLatin1Char('('));
        const qsizetype close = v.lastIndexOf(QLatin1Char(')'));
        if (open < 0 || close <= open) {
            return {};
        }
        const QStringList parts = v.mid(open + 1, close - open - 1).split(QLatin1Char(','));
        if (parts.size() < 3) {
            return {};
        }
        QColor c(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
        if (parts.size() >= 4) {
            const QString a = parts[3].trimmed();
            c.setAlpha(a.contains(QLatin1Char('.')) ? int(a.toDouble() * 255.0) : a.toInt());
        }
        return c;
    }
    if (QColor::isValidColorName(v)) {
        return QColor::fromString(v);
    }
    return {};
}

enum class Fill { None, Neutral, Selection, Semantic, Clear, Ours, OtherGradient, Unknown };

struct FillInfo {
    Fill fill = Fill::None;
    Ours ours = Ours::None;
};

FillInfo classifyFill(const QString& rawValue)
{
    const QString v = stripComments(rawValue).trimmed();
    if (v.isEmpty()) {
        return {};
    }
    const QString lower = v.toLower();
    if (lower.startsWith(QLatin1String("qlineargradient")) || lower.startsWith(QLatin1String("qradialgradient"))
        || lower.startsWith(QLatin1String("qconicalgradient"))) {
        const Ours o = ourFills().value(squeeze(v), Ours::None);
        return { o == Ours::None ? Fill::OtherGradient : Fill::Ours, o };
    }
    if (lower == QLatin1String("transparent") || lower == QLatin1String("none")) {
        return { Fill::Clear, Ours::None };
    }
    const QColor c = parseColour(v);
    if (!c.isValid()) {
        return { Fill::Unknown, Ours::None };
    }
    if (c.alpha() < 255) {
        return { Fill::Clear, Ours::None };
    }
    if (selectionColours().contains(c.name())) {
        return { Fill::Selection, Ours::None };
    }
    const float s = c.hsvSaturationF();
    const float val = c.valueF();
    const int h = c.hsvHue();
    if (s < 0.25f) {
        return { Fill::Neutral, Ours::None };
    }
    // Dark legacy blues (#1a3a5a, #203040) are surfaces, not meaning.
    if (h >= 185 && h <= 255 && val < 0.45f) {
        return { Fill::Neutral, Ours::None };
    }
    return { Fill::Semantic, Ours::None };
}

// ---- Declarations ---------------------------------------------------

// A declaration list kept as the pieces between ';' so that untouched
// declarations come out byte-for-byte.
class Decls
{
public:
    explicit Decls(const QString& body)
    {
        int depth = 0;
        QChar quote;
        QString cur;
        for (qsizetype i = 0; i < body.size(); ++i) {
            const QChar c = body.at(i);
            if (!quote.isNull()) {
                cur += c;
                if (c == quote) {
                    quote = QChar();
                }
                continue;
            }
            if (c == QLatin1Char('/') && i + 1 < body.size() && body.at(i + 1) == QLatin1Char('*')) {
                qsizetype end = body.indexOf(QLatin1String("*/"), i + 2);
                end = end < 0 ? body.size() : end + 2;
                cur += body.mid(i, end - i);
                i = end - 1;
                continue;
            }
            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                quote = c;
            } else if (c == QLatin1Char('(')) {
                ++depth;
            } else if (c == QLatin1Char(')')) {
                depth = std::max(0, depth - 1);
            } else if (c == QLatin1Char(';') && depth == 0) {
                m_pieces.push_back(cur);
                cur.clear();
                continue;
            }
            cur += c;
        }
        m_pieces.push_back(cur);
    }

    QString render() const { return m_pieces.join(QLatin1Char(';')); }

    static QString nameOf(const QString& piece)
    {
        const QString s = stripComments(piece);
        const qsizetype colon = s.indexOf(QLatin1Char(':'));
        return colon < 0 ? QString() : s.left(colon).trimmed().toLower();
    }

    static QString valueOf(const QString& piece)
    {
        const qsizetype colon = piece.indexOf(QLatin1Char(':'));
        return colon < 0 ? QString() : piece.mid(colon + 1).trimmed();
    }

    // Index of the last declaration with one of the names, or -1.
    int findLast(std::initializer_list<const char*> names) const
    {
        for (int i = int(m_pieces.size()) - 1; i >= 0; --i) {
            const QString n = nameOf(m_pieces[i]);
            for (const char* want : names) {
                if (n == QLatin1String(want)) {
                    return i;
                }
            }
        }
        return -1;
    }

    QString value(int i) const { return valueOf(m_pieces[i]); }

    void replaceValue(int i, const QString& v)
    {
        m_pieces[i] = QStringLiteral(" %1: %2").arg(nameOf(m_pieces[i]), v);
    }

    // Sets `name` to `v`: replaces the last declaration of that name, or
    // adds one.
    void set(const char* name, const QString& v)
    {
        const int i = findLast({ name });
        if (i >= 0) {
            if (value(i) != v) {
                m_pieces[i] = QStringLiteral(" %1: %2").arg(QLatin1String(name), v);
            }
            return;
        }
        const QString piece = QStringLiteral(" %1: %2").arg(QLatin1String(name), v);
        if (stripComments(m_pieces.last()).trimmed().isEmpty()) {
            m_pieces.insert(m_pieces.size() - 1, piece);
        } else {
            m_pieces.push_back(piece);
            m_pieces.push_back(QString());
        }
    }

    bool has(const char* name) const { return findLast({ name }) >= 0; }

private:
    QStringList m_pieces;
};

struct BorderInfo {
    bool has = false;
    Fill colour = Fill::Unknown;
};

BorderInfo borderInfo(const Decls& d)
{
    BorderInfo b;
    const int i = d.findLast({ "border" });
    if (i >= 0) {
        const QString v = stripComments(d.value(i)).trimmed().toLower();
        if (!v.startsWith(QLatin1String("none")) && v != QLatin1String("0")
            && v != QLatin1String("0px")) {
            b.has = true;
            // Find the colour token.
            int depth = 0;
            QString tok;
            QStringList toks;
            for (QChar c : v) {
                if (c == QLatin1Char('(')) {
                    ++depth;
                } else if (c == QLatin1Char(')')) {
                    --depth;
                }
                if (c.isSpace() && depth == 0) {
                    if (!tok.isEmpty()) {
                        toks << tok;
                    }
                    tok.clear();
                } else {
                    tok += c;
                }
            }
            if (!tok.isEmpty()) {
                toks << tok;
            }
            for (const QString& t : toks) {
                const FillInfo f = classifyFill(t);
                if (f.fill != Fill::Unknown) {
                    b.colour = f.fill;
                }
            }
        }
    }
    const int bw = d.findLast({ "border-width" });
    if (bw >= 0) {
        const QString v = d.value(bw).trimmed();
        b.has = !(v == QLatin1String("0") || v == QLatin1String("0px"));
    }
    const int bc = d.findLast({ "border-color" });
    if (bc >= 0) {
        b.colour = classifyFill(d.value(bc)).fill;
    }
    return b;
}

// ---- Selectors ------------------------------------------------------

enum class Base { None, Button, Combo, Field, Label, Slider, TabBar };
enum class Kind { None, Button, Combo, Tab, Field, Label, SpinButton, Groove, SliderFill, SliderRecess, Handle };
enum class State { Normal, Hover, Pressed, Checked, Disabled, Other };

Base baseOfTypeName(const QString& t)
{
    static const QHash<QString, Base> m = {
        { QStringLiteral("QPushButton"), Base::Button },
        { QStringLiteral("QToolButton"), Base::Button },
        { QStringLiteral("QComboBox"), Base::Combo },
        { QStringLiteral("QLineEdit"), Base::Field },
        { QStringLiteral("QSpinBox"), Base::Field },
        { QStringLiteral("QDoubleSpinBox"), Base::Field },
        { QStringLiteral("QAbstractSpinBox"), Base::Field },
        { QStringLiteral("QLabel"), Base::Label },
        { QStringLiteral("QSlider"), Base::Slider },
        { QStringLiteral("QTabBar"), Base::TabBar },
    };
    return m.value(t, Base::None);
}

Base baseOfWidget(const QWidget* w)
{
    if (w == nullptr) {
        return Base::None;
    }
    if (w->inherits("QPushButton") || w->inherits("QToolButton")) {
        return Base::Button;
    }
    if (w->inherits("QComboBox")) {
        return Base::Combo;
    }
    if (w->inherits("QAbstractSpinBox") || w->inherits("QLineEdit")) {
        return Base::Field;
    }
    if (w->inherits("QLabel")) {
        return Base::Label;
    }
    if (w->inherits("QSlider")) {
        return Base::Slider;
    }
    if (w->inherits("QTabBar")) {
        return Base::TabBar;
    }
    return Base::None;
}

bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-');
}

struct Compound {
    QString type;
    QString id;
    QString sub;
    QStringList states;
};

QString lastCompound(const QString& selector)
{
    const QString s = stripComments(selector).trimmed();
    int depth = 0;
    qsizetype start = 0;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('[') || c == QLatin1Char('(')) {
            ++depth;
        } else if (c == QLatin1Char(']') || c == QLatin1Char(')')) {
            --depth;
        } else if (depth == 0 && (c.isSpace() || c == QLatin1Char('>'))) {
            start = i + 1;
        }
    }
    return s.mid(start);
}

Compound parseCompound(const QString& s)
{
    Compound c;
    qsizetype i = 0;
    const qsizetype n = s.size();
    auto readIdent = [&]() {
        const qsizetype b = i;
        while (i < n && isIdentChar(s.at(i))) {
            ++i;
        }
        return s.mid(b, i - b);
    };
    if (i < n && s.at(i) == QLatin1Char('*')) {
        ++i;
    } else {
        c.type = readIdent();
    }
    while (i < n) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('#')) {
            ++i;
            c.id = readIdent();
        } else if (ch == QLatin1Char('.')) {
            ++i;
            c.type = readIdent();
        } else if (ch == QLatin1Char('[')) {
            const qsizetype end = s.indexOf(QLatin1Char(']'), i);
            i = end < 0 ? n : end + 1;
        } else if (ch == QLatin1Char(':')) {
            if (i + 1 < n && s.at(i + 1) == QLatin1Char(':')) {
                i += 2;
                c.sub = readIdent().toLower();
            } else {
                ++i;
                bool neg = false;
                if (i < n && s.at(i) == QLatin1Char('!')) {
                    neg = true;
                    ++i;
                }
                const QString st = readIdent().toLower();
                c.states << (neg ? QLatin1Char('!') + st : st);
            }
        } else {
            ++i;
        }
    }
    return c;
}

struct Target {
    Kind kind = Kind::None;
    State state = State::Normal;
    int height = kDefaultControlHeight;
    bool vertical = false;
    // The block's control is the widget the sheet is set on.
    bool isOwner = false;

    bool operator==(const Target& o) const
    {
        return kind == o.kind && state == o.state;
    }
};

int heightOf(const QWidget* w)
{
    if (w == nullptr) {
        return kDefaultControlHeight;
    }
    if (w->minimumHeight() == w->maximumHeight() && w->maximumHeight() > 0) {
        return w->maximumHeight();
    }
    if (w->isVisible() && w->height() > 0) {
        return w->height();
    }
    return kDefaultControlHeight;
}

Target classify(const Compound& c, Base base, const QWidget* resolved)
{
    Target t;
    bool horizontal = false;
    bool dis = false;
    bool press = false;
    bool chk = false;
    bool hov = false;
    bool other = false;
    for (const QString& st : c.states) {
        if (st.startsWith(QLatin1Char('!'))) {
            continue;
        }
        if (st == QLatin1String("disabled")) {
            dis = true;
        } else if (st == QLatin1String("pressed")) {
            press = true;
        } else if (st == QLatin1String("on")) {
            (base == Base::Combo ? press : chk) = true;
        } else if (st == QLatin1String("checked") || st == QLatin1String("selected")) {
            chk = true;
        } else if (st == QLatin1String("hover")) {
            hov = true;
        } else if (st == QLatin1String("focus")) {
            other = true;
        } else if (st == QLatin1String("vertical")) {
            t.vertical = true;
        } else if (st == QLatin1String("horizontal")) {
            horizontal = true;
        }
    }
    Q_UNUSED(horizontal);
    t.state = dis     ? State::Disabled
            : other   ? State::Other
            : press   ? State::Pressed
            : chk     ? State::Checked
            : hov     ? State::Hover
                      : State::Normal;

    const QString& sub = c.sub;
    switch (base) {
    case Base::Button:
        t.kind = sub.isEmpty() ? Kind::Button : Kind::None;
        break;
    case Base::Combo:
        t.kind = sub.isEmpty() ? Kind::Combo : Kind::None;
        break;
    case Base::Field:
        if (sub.isEmpty()) {
            t.kind = Kind::Field;
        } else if (sub == QLatin1String("up-button") || sub == QLatin1String("down-button")) {
            t.kind = Kind::SpinButton;
        }
        break;
    case Base::Label:
        t.kind = sub.isEmpty() ? Kind::Label : Kind::None;
        break;
    case Base::Slider:
        if (sub == QLatin1String("groove")) {
            t.kind = Kind::Groove;
        } else if (sub == QLatin1String("handle")) {
            t.kind = Kind::Handle;
        } else if (sub == QLatin1String("sub-page")) {
            t.kind = t.vertical ? Kind::SliderRecess : Kind::SliderFill;
        } else if (sub == QLatin1String("add-page")) {
            t.kind = t.vertical ? Kind::SliderFill : Kind::SliderRecess;
        }
        break;
    case Base::TabBar:
        t.kind = sub == QLatin1String("tab") ? Kind::Tab : Kind::None;
        break;
    case Base::None:
        break;
    }
    if (resolved != nullptr && sub.isEmpty()) {
        t.height = heightOf(resolved);
    }
    return t;
}

Target classifySelector(const QString& selector, const QWidget* owner)
{
    const Compound c = parseCompound(lastCompound(selector));
    const QWidget* resolved = nullptr;
    Base base = Base::None;
    if (!c.type.isEmpty()) {
        base = baseOfTypeName(c.type);
        if (owner != nullptr && base != Base::None && owner->inherits(c.type.toLatin1().constData())
            && (c.id.isEmpty() || owner->objectName() == c.id)) {
            resolved = owner;
        } else if (owner != nullptr && !c.id.isEmpty()) {
            resolved = owner->findChild<QWidget*>(c.id);
        }
    } else {
        if (!c.id.isEmpty()) {
            if (owner != nullptr) {
                resolved = owner->objectName() == c.id ? owner : owner->findChild<QWidget*>(c.id);
            }
        } else {
            resolved = owner;
        }
        base = baseOfWidget(resolved);
    }
    Target t = classify(c, base, resolved);
    t.isOwner = resolved != nullptr && resolved == owner;
    return t;
}

Target classifySelectorless(const QWidget* owner)
{
    Target t = classify(Compound{}, baseOfWidget(owner), owner);
    t.isOwner = owner != nullptr;
    return t;
}

// ---- Treatment ------------------------------------------------------

void setBackground(Decls& d, int bgIndex, const QString& v)
{
    if (bgIndex >= 0) {
        d.replaceValue(bgIndex, v);
    } else {
        d.set("background", v);
    }
}

void raisedEdges(Decls& d)
{
    // From Longpath src/gui/StyleConstants.h:778 [@551576e3]
    d.set("border-top-color", hex(kGlassEdgeLight));
    d.set("border-bottom-color", hex(kGlassEdgeDark));
}

void pressedEdges(Decls& d)
{
    // From Longpath src/gui/StyleConstants.h:786 [@551576e3]
    // Gedrueckt: der Verlauf kippt.
    d.set("border-top-color", hex(kGlassEdgeDark));
    d.set("border-bottom-color", hex(kGlassEdgeLight));
}

void sunkenEdges(Decls& d)
{
    // From Longpath src/gui/StyleConstants.h:1014-1015 [@551576e3]
    d.set("border-top-color", hex(kGlassShade));
    d.set("border-bottom-color", hex(kGlassLight));
}

void glassTreatment(Decls& d)
{
    // From Longpath src/gui/StyleConstants.h:815-823 [@551576e3]
    setBackground(d, d.findLast({ "background", "background-color" }), glassFill());
    if (d.has("color")) {
        d.set("color", hex(kGlassSelText));
    }
    d.set("border-color", hex(kGlassSelBorder));
    d.set("border-top-color", shiftL(QColor(hex(kGlassSelBorder)), kGlassBorderTopLift));
}

void disabledTreatment(Decls& d, int bg, const BorderInfo& b)
{
    setBackground(d, bg, hex(kDisabledBg));
    if (d.has("color")) {
        d.set("color", hex(kDisabledText));
    }
    if (b.has || d.has("border-color")) {
        d.set("border-color", hex(kDisabledBorder));
    }
}

QString processBody(const QString& body, const Target& t, bool* restyledOut = nullptr)
{
    if (restyledOut != nullptr) {
        *restyledOut = false;
    }
    if (t.kind == Kind::None || t.state == State::Other) {
        return body;
    }
    Decls d(body);
    const int bg = d.findLast({ "background", "background-color" });
    const FillInfo f = bg >= 0 ? classifyFill(d.value(bg)) : FillInfo{};
    const bool restyle = f.fill == Fill::Neutral || f.fill == Fill::Selection || f.fill == Fill::Ours;
    if (!restyle) {
        return body;
    }
    if (restyledOut != nullptr) {
        *restyledOut = true;
    }
    const bool selected = f.fill == Fill::Selection || f.ours == Ours::Glass;
    const BorderInfo b = borderInfo(d);
    const bool neutralBorder = b.colour != Fill::Semantic;
    const QString radius = QStringLiteral("%1px").arg(radiusFor(t.height));

    switch (t.kind) {
    case Kind::Button:
    case Kind::Combo:
    case Kind::Tab:
        switch (t.state) {
        case State::Normal:
            if (selected) {
                glassTreatment(d);
            } else {
                setBackground(d, bg, raisedFill());
                if (b.has && neutralBorder) {
                    raisedEdges(d);
                }
            }
            break;
        case State::Hover:
            setBackground(d, bg, raisedHoverFill());
            break;
        case State::Pressed:
            setBackground(d, bg, pressedFill());
            pressedEdges(d);
            if (d.has("color")) {
                d.set("color", hex(kTextPrimary));
            }
            break;
        case State::Checked:
            glassTreatment(d);
            break;
        case State::Disabled:
            disabledTreatment(d, d.findLast({ "background", "background-color" }), b);
            break;
        case State::Other:
            break;
        }
        if (t.kind != Kind::Tab && (t.state == State::Normal || d.has("border-radius"))) {
            d.set("border-radius", radius);
        }
        break;
    case Kind::Field:
    case Kind::Label:
        if (t.kind == Kind::Label && (t.state != State::Normal || !b.has)) {
            return body;
        }
        if (t.state == State::Normal || t.state == State::Hover) {
            // A gradient background makes Qt draw a spin box without its
            // native buttons, which changes its size hint (measured by
            // tst_workspace_qss_translator). Fields take the flat inset
            // colour; the sunken edges carry the depth. Value chips
            // (labels) have no subcontrols and take the sunken fill.
            setBackground(d, bg, t.kind == Kind::Label ? fieldFill() : hex(kInsetBg));
            if (b.has && neutralBorder) {
                if (t.kind == Kind::Label) {
                    // From Longpath src/gui/StyleConstants.h:1045 [@551576e3]
                    d.set("border-color", hex(kInsetBorder));
                }
                sunkenEdges(d);
            }
            d.set("border-radius", radius);
        } else if (t.state == State::Disabled) {
            disabledTreatment(d, bg, b);
        }
        break;
    case Kind::SpinButton:
        if (t.state == State::Normal) {
            setBackground(d, bg, raisedFill());
        } else if (t.state == State::Hover) {
            setBackground(d, bg, raisedHoverFill());
        } else if (t.state == State::Pressed) {
            setBackground(d, bg, pressedFill());
        }
        break;
    case Kind::Groove:
    case Kind::SliderRecess:
        if (t.state == State::Normal) {
            setBackground(d, bg, fieldFill());
            if (b.has && neutralBorder) {
                sunkenEdges(d);
            }
        }
        break;
    case Kind::SliderFill:
        if (t.state == State::Normal) {
            setBackground(d, bg, hex(kAccent));
        }
        break;
    case Kind::Handle:
        if (t.state == State::Normal) {
            if (b.has) {
                // From Longpath src/gui/StyleConstants.h:906-918 [@551576e3]
                setBackground(d, bg, raisedFill());
                d.set("border-color", hex(kAccent));
                d.set("border-top-color", shiftL(QColor(hex(kAccent)), kHandleBorderTopLift));
            } else {
                setBackground(d, bg, accentPillFill());
            }
        } else if (t.state == State::Hover) {
            setBackground(d, bg, b.has ? hex(kGlassBtnTop) : accentPillHoverFill());
        }
        break;
    case Kind::None:
        break;
    }
    return d.render();
}

bool isCheckableButton(const QWidget* w)
{
    return w != nullptr && w->inherits("QAbstractButton") && w->property("checkable").toBool();
}

// Declarations of the pressed and selected states, for a checkable
// button whose sheet gives every state the same background: without
// them the look would draw checked and unchecked alike.
QString pressedDecls()
{
    Decls d(QString{});
    setBackground(d, -1, pressedFill());
    pressedEdges(d);
    return d.render();
}

QString checkedDecls()
{
    Decls d(QString{});
    glassTreatment(d);
    d.set("color", hex(kGlassSelText));
    return d.render();
}

QStringList splitSelectors(const QString& selectorText)
{
    QStringList out;
    int depth = 0;
    QString cur;
    for (QChar c : stripComments(selectorText)) {
        if (c == QLatin1Char('(') || c == QLatin1Char('[')) {
            ++depth;
        } else if (c == QLatin1Char(')') || c == QLatin1Char(']')) {
            --depth;
        }
        if (c == QLatin1Char(',') && depth == 0) {
            out << cur.trimmed();
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.trimmed().isEmpty()) {
        out << cur.trimmed();
    }
    return out;
}

} // namespace

QString raisedFill()
{
    // From Longpath src/gui/StyleConstants.h:1114 [@551576e3]
    return vgrad(hex(kGlassBtnTop), hex(kGlassBtnBot));
}

QString raisedHoverFill()
{
    // From Longpath src/gui/StyleConstants.h:1119 [@551576e3]
    return vgrad(hex(kRaisedHoverTop), hex(kRaisedHoverBot));
}

QString pressedFill()
{
    // From Longpath src/gui/StyleConstants.h:1121 [@551576e3]
    // aufgeklappt: gedrueckt, der Verlauf kippt
    return vgrad(hex(kGlassBtnBot), hex(kGlassBtnTop));
}

QString glassFill()
{
    // From Longpath src/gui/StyleConstants.h:819 [@551576e3]
    return vgrad(hex(kGlassSelTop), hex(kGlassSelBot));
}

QString sunkenFill(const char* baseHex, int deepen, int lift)
{
    // From Longpath src/gui/StyleConstants.h:723-731 [@551576e3]
    /// VERSENKT: genau andersherum — oben dunkel, unten heller. Dasselbe
    /// Mittel, umgedreht, und die Flaeche liegt drin statt drauf.
    const QColor base(hex(baseHex));
    return vgrad(shiftL(base, -deepen), shiftL(base, lift));
}

QString fieldFill()
{
    return sunkenFill(kInsetBg, kFieldDeepen, kFieldLift);
}

int radiusFor(int heightPx)
{
    return heightPx <= kRadiusSmallMaxHeight ? kRadiusSmall : kRadiusLarge;
}

QString componentize(const QString& qss, const QWidget* owner)
{
    if (!qss.contains(QLatin1Char('{'))) {
        const Target t = classifySelectorless(owner);
        bool restyled = false;
        const QString body = processBody(qss, t, &restyled);
        if (restyled && t.kind == Kind::Button && t.state == State::Normal
            && isCheckableButton(owner)) {
            // A brace-less sheet is a "*" rule (specificity 0), so the
            // state rules below win over it.
            return QStringLiteral("* {%1}\n*:pressed {%2}\n*:checked {%3}")
                .arg(body, pressedDecls(), checkedDecls());
        }
        return body;
    }

    QString out;
    out.reserve(qss.size() + qss.size() / 2);
    QStringList grooveSelectors;
    QStringList ownerButtonSelectors;
    const qsizetype n = qss.size();
    qsizetype i = 0;
    qsizetype selStart = 0;
    while (i < n) {
        const QChar c = qss.at(i);
        if (c == QLatin1Char('/') && i + 1 < n && qss.at(i + 1) == QLatin1Char('*')) {
            const qsizetype end = qss.indexOf(QLatin1String("*/"), i + 2);
            i = end < 0 ? n : end + 2;
            continue;
        }
        if (c != QLatin1Char('{')) {
            ++i;
            continue;
        }
        const QString selectorText = qss.mid(selStart, i - selStart);
        // Find the closing brace, skipping comments and strings.
        qsizetype j = i + 1;
        QChar quote;
        while (j < n) {
            const QChar d = qss.at(j);
            if (!quote.isNull()) {
                if (d == quote) {
                    quote = QChar();
                }
            } else if (d == QLatin1Char('"') || d == QLatin1Char('\'')) {
                quote = d;
            } else if (d == QLatin1Char('/') && j + 1 < n && qss.at(j + 1) == QLatin1Char('*')) {
                const qsizetype end = qss.indexOf(QLatin1String("*/"), j + 2);
                j = end < 0 ? n : end + 1;
            } else if (d == QLatin1Char('}')) {
                break;
            }
            ++j;
        }
        const QString body = qss.mid(i + 1, j - i - 1);

        const QStringList sels = splitSelectors(selectorText);
        Target t;
        bool first = true;
        bool agree = !sels.isEmpty();
        for (const QString& s : sels) {
            const Target st = classifySelector(s, owner);
            if (first) {
                t = st;
                first = false;
            } else if (!(st == t)) {
                agree = false;
            }
            if (st.kind == Kind::Groove) {
                grooveSelectors << s;
            }
        }
        out += selectorText;
        out += QLatin1Char('{');
        bool restyled = false;
        out += agree ? processBody(body, t, &restyled) : body;
        if (agree && restyled && t.kind == Kind::Button && t.state == State::Normal && t.isOwner) {
            ownerButtonSelectors << sels;
        }
        if (j < n) {
            out += QLatin1Char('}');
        }
        i = j + 1;
        selStart = i;
    }
    out += qss.mid(std::min(selStart, n));

    const QString lower = qss.toLower();

    // A checkable button whose own sheet has no pressed or checked rule.
    if (!ownerButtonSelectors.isEmpty() && isCheckableButton(owner)
        && !lower.contains(QLatin1String(":checked")) && !lower.contains(QLatin1String(":on"))) {
        QStringList pressed;
        QStringList checked;
        for (const QString& s : ownerButtonSelectors) {
            pressed << s + QStringLiteral(":pressed");
            checked << s + QStringLiteral(":checked");
        }
        if (!lower.contains(QLatin1String(":pressed"))) {
            out += QStringLiteral("\n%1 {%2}").arg(pressed.join(QStringLiteral(", ")), pressedDecls());
        }
        out += QStringLiteral("\n%1 {%2}").arg(checked.join(QStringLiteral(", ")), checkedDecls());
    }

    // The filled side of a styled groove, when the sheet has none.
    if (!grooveSelectors.isEmpty() && !lower.contains(QLatin1String("::sub-page"))
        && !lower.contains(QLatin1String("::add-page"))) {
        QStringList fills;
        for (const QString& s : grooveSelectors) {
            QString f = s;
            const bool vertical = s.toLower().contains(QLatin1String(":vertical"));
            f.replace(QLatin1String("::groove"),
                      vertical ? QLatin1String("::add-page") : QLatin1String("::sub-page"),
                      Qt::CaseInsensitive);
            fills << f;
        }
        out += QStringLiteral("%1 { background: %2; border-radius: %3px; }")
                   .arg(fills.join(QStringLiteral(", ")), hex(kAccent))
                   .arg(kSliderFillRadius);
    }
    return out;
}

} // namespace NereusSDR::Workspace::Components
