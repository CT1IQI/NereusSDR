// =================================================================
// src/gui/styles/QssTranslator.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See QssTranslator.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace preview by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - A slider's filled page resolves #203040 to the accent.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "QssTranslator.h"

#include "WorkspacePalette.h"

#include <algorithm>

namespace NereusSDR::QssTranslator {

namespace {

bool isTokenChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-');
}

bool isHexDigit(QChar c)
{
    const char16_t u = c.unicode();
    return (u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'f') || (u >= u'A' && u <= u'F');
}

bool allHex(QStringView s)
{
    if (s.isEmpty()) {
        return false;
    }
    for (QChar c : s) {
        if (!isHexDigit(c)) {
            return false;
        }
    }
    return true;
}

// First row wins for a duplicated key.
QHash<QString, QString> buildIndex(const Table& table)
{
    QHash<QString, QString> index;
    for (const Rule& r : table) {
        const QString key = r.legacyHex.toLower();
        if (!index.contains(key)) {
            index.insert(key, r.role);
        }
    }
    return index;
}

const QHash<QString, QString>& defaultIndex()
{
    static const QHash<QString, QString> index = buildIndex(defaultTable());
    return index;
}

QString contextRoleForAmbiguous(const QString& selector, const QString& property)
{
    const QString sel = selector.toLower();
    const QString prop = property.trimmed().toLower();
    // A slider's filled part is the accent, not the groove: the sub-page
    // on a horizontal slider, the add-page on a vertical one (Qt draws
    // a vertical slider's sub-page above the handle). The same split as
    // WorkspaceComponents. Scroll bar pages are track, so they stay groove.
    const bool scrollBar = sel.contains(QLatin1String("qscrollbar"));
    const bool vertical = sel.contains(QLatin1String(":vertical"));
    const bool subPage = sel.contains(QLatin1String("::sub-page"));
    const bool addPage = sel.contains(QLatin1String("::add-page"));
    if (!scrollBar && ((subPage && !vertical) || (addPage && vertical))) {
        return QStringLiteral("accent");
    }
    if (sel.contains(QLatin1String("::groove")) || addPage || subPage) {
        return QStringLiteral("groove");
    }
    if (prop.startsWith(QLatin1String("border")) || prop.startsWith(QLatin1String("outline"))) {
        return QStringLiteral("border-subtle");
    }
    if ((sel.contains(QLatin1String(":hover")) || sel.contains(QLatin1String(":pressed")))
        && prop.startsWith(QLatin1String("background"))) {
        return QStringLiteral("button-hover");
    }
    return QStringLiteral("border-subtle");
}

bool startsWithUrl(const QString& s, qsizetype i)
{
    if (i + 4 > s.size()) {
        return false;
    }
    if (i > 0 && isTokenChar(s.at(i - 1))) {
        return false;
    }
    return QStringView(s).mid(i, 4).compare(QLatin1String("url("), Qt::CaseInsensitive) == 0;
}

} // namespace

const Table& defaultTable()
{
    static const Table table = [] {
        Table t;
        for (const auto& row : Workspace::WorkspacePalette::legacyRows()) {
            t.push_back({QString::fromLatin1(row.legacyHex), QString::fromLatin1(row.role)});
        }
        return t;
    }();
    return table;
}

Result translate(const QString& original, const Options& options)
{
    Result result;
    const auto& palette = Workspace::WorkspacePalette::glassAndDepth();
    const QHash<QString, QString> customIndex =
        options.table != nullptr ? buildIndex(*options.table) : QHash<QString, QString>{};
    const QHash<QString, QString>& index =
        options.table != nullptr ? customIndex : defaultIndex();
    const QString ambiguous = QString::fromLatin1(kAmbiguousHex);

    const bool hasBraces = original.contains(QLatin1Char('{'));
    bool inBlock = !hasBraces;
    bool inValue = false;
    QString selector;
    QString property;

    QString& out = result.qss;
    out.reserve(original.size());
    const qsizetype n = original.size();

    for (qsizetype i = 0; i < n;) {
        const QChar c = original.at(i);

        // Comments are copied untouched.
        if (c == QLatin1Char('/') && i + 1 < n && original.at(i + 1) == QLatin1Char('*')) {
            qsizetype end = original.indexOf(QLatin1String("*/"), i + 2);
            end = end < 0 ? n : end + 2;
            out += QStringView(original).mid(i, end - i);
            i = end;
            continue;
        }

        // Quoted strings are copied untouched.
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            qsizetype j = i + 1;
            while (j < n && original.at(j) != c) {
                j += (original.at(j) == QLatin1Char('\\')) ? 2 : 1;
            }
            j = std::min<qsizetype>(j + 1, n);
            out += QStringView(original).mid(i, j - i);
            if (hasBraces && !inBlock) {
                selector += QStringView(original).mid(i, j - i);
            }
            i = j;
            continue;
        }

        // url(...) is copied untouched.
        if (inBlock && startsWithUrl(original, i)) {
            qsizetype end = original.indexOf(QLatin1Char(')'), i + 4);
            end = end < 0 ? n : end + 1;
            out += QStringView(original).mid(i, end - i);
            i = end;
            continue;
        }

        if (hasBraces && c == QLatin1Char('{')) {
            inBlock = true;
            inValue = false;
            property.clear();
            out += c;
            ++i;
            continue;
        }
        if (hasBraces && c == QLatin1Char('}')) {
            inBlock = false;
            inValue = false;
            property.clear();
            selector.clear();
            out += c;
            ++i;
            continue;
        }
        if (inBlock && c == QLatin1Char(';')) {
            inValue = false;
            property.clear();
            out += c;
            ++i;
            continue;
        }
        if (inBlock && !inValue && c == QLatin1Char(':')) {
            inValue = true;
            out += c;
            ++i;
            continue;
        }

        if (c == QLatin1Char('#')) {
            qsizetype j = i + 1;
            while (j < n && isTokenChar(original.at(j))) {
                ++j;
            }
            const QString token = original.mid(i, j - i);
            if (!inBlock) {
                // A selector such as "QWidget#name" is preserved.
                selector += token;
                out += token;
                i = j;
                continue;
            }
            const QStringView digits = QStringView(token).mid(1);
            const bool colourLike = allHex(digits)
                && (digits.size() == 3 || digits.size() == 4 || digits.size() == 6
                    || digits.size() == 8);
            if (!colourLike) {
                out += token;
                i = j;
                continue;
            }
            const QString key = token.toLower();
            QString role;
            if (digits.size() == 6) {
                role = options.explicitRoles.value(key);
                if (role.isEmpty() && key == ambiguous && options.resolveAmbiguousByContext) {
                    role = contextRoleForAmbiguous(selector, property);
                }
                if (role.isEmpty()) {
                    role = index.value(key);
                }
            }
            const QString value = role.isEmpty() ? QString() : palette.qss(role);
            if (value.isEmpty()) {
                result.unknownLiterals.push_back(key);
                out += token;
            } else {
                out += value;
                ++result.rewritten;
            }
            i = j;
            continue;
        }

        if (hasBraces && !inBlock) {
            selector += c;
        } else if (inBlock && !inValue) {
            property += c;
        }
        out += c;
        ++i;
    }
    return result;
}

Result translateWithAmbiguousRole(const QString& original, const QString& roleFor203040)
{
    Options options;
    options.explicitRoles.insert(QString::fromLatin1(kAmbiguousHex), roleFor203040);
    return translate(original, options);
}

} // namespace NereusSDR::QssTranslator
