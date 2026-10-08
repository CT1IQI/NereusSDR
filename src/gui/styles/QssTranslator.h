// =================================================================
// src/gui/styles/QssTranslator.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The restricted compatibility
// translator from the Longpath theme design (LT "Theme interfaces and
// migration"). Its legacy-hex table comes from WorkspacePalette, which
// carries the Longpath attribution; the algorithm here is NereusSDR's.
//
// Rewrites legacy colour literals in a retained stylesheet into the
// Glass and Depth palette:
//   - a colour token is '#' followed by a run of [0-9A-Za-z_-]; only a
//     complete run of exactly six hex digits with a table row is
//     rewritten, so "#205070ff" (eight digits, alpha) is never touched
//     as if it were "#205070", and "#2050701" is not a colour at all;
//   - text inside url(...), quoted strings and /* comments */ is copied
//     unchanged;
//   - when the sheet has braces, only declarations inside { } are
//     rewritten, so selectors (including "#objectName") are preserved;
//     a sheet without braces is a list of declarations;
//   - colour literals without a row (three-digit, eight-digit, unmapped
//     six-digit) stay as they are and are reported;
//   - one left-to-right pass, at most one rewrite per token.
//
// Difference from Longpath's Style::themed(): Longpath adds a row for
// every current value so that themed(themed(s)) == themed(s). NereusSDR
// does not; a target value may also be a source key, so translating an
// already-translated sheet can move a colour twice. Always translate
// the retained original, never widget->styleSheet() after theming
// (WorkspacePreview::themeSubtree keeps the original for this).
//
// #203040 has no table row. Callers name its role through
// Options::explicitRoles, or ask for the context rule
// (Options::resolveAmbiguousByContext): a slider groove or page gets
// "groove", a :hover/:pressed rule's background gets "button-hover",
// everything else "border-subtle".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace preview by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace NereusSDR::QssTranslator {

// One mapping: a lower-case "#rrggbb" literal and the palette role it
// becomes.
struct Rule {
    QString legacyHex;
    QString role;
};
using Table = QVector<Rule>;

// The table built from WorkspacePalette::legacyRows().
const Table& defaultTable();

// The legacy colour with no blanket row.
inline constexpr auto kAmbiguousHex = "#203040";

struct Options {
    // Table to use; nullptr means defaultTable().
    const Table* table = nullptr;
    // Per call site: lower-case legacy hex to role. Wins over the table.
    QHash<QString, QString> explicitRoles;
    // Resolve kAmbiguousHex by the selector and property it sits in.
    bool resolveAmbiguousByContext = false;
};

struct Result {
    QString qss;
    // Colour literals left as they were, lower case, one entry per
    // occurrence, in source order.
    QStringList unknownLiterals;
    int rewritten = 0;
};

Result translate(const QString& original, const Options& options = {});

// Shorthand: translate with one explicit role for #203040.
Result translateWithAmbiguousRole(const QString& original, const QString& roleFor203040);

} // namespace NereusSDR::QssTranslator
