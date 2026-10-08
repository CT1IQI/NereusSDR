// no-port-check: NereusSDR-original test for the workspace preview's
// restricted QSS translator (src/gui/styles/QssTranslator) and the
// scoped subtree pass (src/gui/styles/WorkspacePreview).
//
// Covers the translator checks of the Longpath theme design: complete
// tokens, six versus eight digits, alpha and url() preservation,
// selectors, unknown literals, one left-to-right pass, and overlapping
// mappings generated afresh from the retained original.

#include <QtTest/QtTest>

#include <QLabel>
#include <QWidget>

#include "gui/styles/QssTranslator.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;
namespace QT = NereusSDR::QssTranslator;

class TestWorkspaceQssTranslator : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        WorkspacePreview::setEnabledForTest(false);
        WorkspacePreview::resetUnknownLiteralCounts();
    }

    void defaultTableRolesExistAndAmbiguousHasNoRow()
    {
        const auto& palette = Workspace::WorkspacePalette::glassAndDepth();
        for (const QT::Rule& rule : QT::defaultTable()) {
            QVERIFY2(palette.contains(rule.role), qPrintable(rule.role));
            QCOMPARE(rule.legacyHex, rule.legacyHex.toLower());
            QVERIFY2(rule.legacyHex != QLatin1String(QT::kAmbiguousHex),
                     "#203040 must not have a blanket row");
        }
    }

    void rewritesCompleteSixDigitTokens()
    {
        const QT::Result r = QT::translate(
            QStringLiteral("QLabel { color: #c8d8e8; border: 1px solid #205070; }"));
        QCOMPARE(r.qss, QStringLiteral("QLabel { color: #dcdce1; border: 1px solid #2c2c31; }"));
        QCOMPARE(r.rewritten, 2);
        QVERIFY(r.unknownLiterals.isEmpty());
    }

    void matchesCaseInsensitively()
    {
        QCOMPARE(QT::translate(QStringLiteral("color: #C8D8E8;")).qss,
                 QStringLiteral("color: #dcdce1;"));
    }

    void neverRewritesEightDigitOrLongerTokens()
    {
        const QT::Result r = QT::translate(QStringLiteral(
            "QWidget { background: #205070cc; color: #2050701; border-color: #205070; }"));
        QCOMPARE(r.qss, QStringLiteral(
            "QWidget { background: #205070cc; color: #2050701; border-color: #2c2c31; }"));
        // The eight-digit colour is reported; seven digits is not a colour.
        QCOMPARE(r.unknownLiterals, QStringList{QStringLiteral("#205070cc")});
    }

    void preservesAlphaFunctionsUrlsStringsAndComments()
    {
        const QString src = QStringLiteral(
            "QPushButton {"
            " background: rgba(32, 80, 112, 128);"
            " image: url(:/icons/#205070.svg);"
            " font-family: \"#c8d8e8\";"
            " /* was #205070 */"
            " color: #c8d8e8; }");
        const QString expected = QStringLiteral(
            "QPushButton {"
            " background: rgba(32, 80, 112, 128);"
            " image: url(:/icons/#205070.svg);"
            " font-family: \"#c8d8e8\";"
            " /* was #205070 */"
            " color: #dcdce1; }");
        QCOMPARE(QT::translate(src).qss, expected);
    }

    void preservesSelectors()
    {
        const QString src = QStringLiteral(
            "QWidget#c8d8e8 > QLabel#ff4444:hover { color: #c8d8e8; }");
        QCOMPARE(QT::translate(src).qss, QStringLiteral(
            "QWidget#c8d8e8 > QLabel#ff4444:hover { color: #dcdce1; }"));
    }

    void braceLessSheetIsDeclarations()
    {
        QCOMPARE(QT::translate(QStringLiteral("background: #0f0f1a; color: #8090a0;")).qss,
                 QStringLiteral("background: #08080a; color: #a6a6ac;"));
    }

    void unknownLiteralsStayAndAreReported()
    {
        const QT::Result r = QT::translate(QStringLiteral(
            "QLabel { color: #123456; background: #fff; border: 1px solid #205070; }"));
        QCOMPARE(r.qss, QStringLiteral(
            "QLabel { color: #123456; background: #fff; border: 1px solid #2c2c31; }"));
        QCOMPARE(r.unknownLiterals,
                 (QStringList{QStringLiteral("#123456"), QStringLiteral("#fff")}));
    }

    void ambiguousColourNeedsAnExplicitRole()
    {
        const QString src = QStringLiteral("QSplitter::handle { background: #203040; }");
        const QT::Result plain = QT::translate(src);
        QCOMPARE(plain.qss, src);
        QCOMPARE(plain.unknownLiterals, QStringList{QStringLiteral("#203040")});

        const QT::Result named = QT::translateWithAmbiguousRole(src, QStringLiteral("button-hover"));
        QCOMPARE(named.qss, QStringLiteral("QSplitter::handle { background: #26262b; }"));
        QVERIFY(named.unknownLiterals.isEmpty());
    }

    void ambiguousColourByContext()
    {
        QT::Options opts;
        opts.resolveAmbiguousByContext = true;
        QCOMPARE(QT::translate(QStringLiteral(
                     "QPushButton:hover { background: #203040; }"), opts).qss,
                 QStringLiteral("QPushButton:hover { background: #26262b; }"));
        QCOMPARE(QT::translate(QStringLiteral(
                     "QSlider::groove:horizontal { background: #203040; }"), opts).qss,
                 QStringLiteral("QSlider::groove:horizontal { background: #1f1f23; }"));
        QCOMPARE(QT::translate(QStringLiteral(
                     "QFrame { border: 1px solid #203040; }"), opts).qss,
                 QStringLiteral("QFrame { border: 1px solid #1f1f23; }"));
        // An explicit role at the call site wins over the context rule.
        opts.explicitRoles.insert(QStringLiteral("#203040"), QStringLiteral("button-alt-hover"));
        QCOMPARE(QT::translate(QStringLiteral(
                     "QFrame { border: 1px solid #203040; }"), opts).qss,
                 QStringLiteral("QFrame { border: 1px solid #2f3138; }"));
    }

    // A target value that is also a source key: one pass moves each
    // token at most once, and a second translation of the output would
    // move it again. Translating the retained original is stable.
    void overlappingMappingsUseTheRetainedOriginal()
    {
        const QT::Table table = {
            { QStringLiteral("#111111"), QStringLiteral("app-bg") },   // -> #08080a
            { QStringLiteral("#08080a"), QStringLiteral("panel") },    // -> #0c0c0e
        };
        QT::Options opts;
        opts.table = &table;

        const QString original = QStringLiteral("QWidget { color: #111111; background: #08080a; }");
        const QT::Result once = QT::translate(original, opts);
        QCOMPARE(once.qss, QStringLiteral("QWidget { color: #08080a; background: #0c0c0e; }"));
        QCOMPARE(once.rewritten, 2);

        const QT::Result twice = QT::translate(once.qss, opts);
        QCOMPARE(twice.qss, QStringLiteral("QWidget { color: #0c0c0e; background: #0c0c0e; }"));
        QVERIFY(twice.qss != once.qss);

        QCOMPARE(QT::translate(original, opts).qss, once.qss);
    }

    void switchOffLeavesWidgetsAlone()
    {
        QWidget root;
        root.setStyleSheet(QStringLiteral("background: #0f0f1a;"));
        WorkspacePreview::themeSubtree(&root);
        QCOMPARE(root.styleSheet(), QStringLiteral("background: #0f0f1a;"));
        QVERIFY(!root.property(WorkspacePreview::kOriginalQssProperty).isValid());
    }

    void subtreePassThemesFromTheRetainedOriginal()
    {
        WorkspacePreview::setEnabledForTest(true);
        QWidget root;
        root.setStyleSheet(QStringLiteral("background: #0f0f1a;"));
        auto* label = new QLabel(&root);
        label->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; background: #abcdef; }"));
        auto* kept = new QWidget(&root);
        kept->setProperty(WorkspacePreview::kSkipProperty, true);
        kept->setStyleSheet(QStringLiteral("background: #0f0f1a;"));
        auto* keptChild = new QLabel(kept);
        keptChild->setStyleSheet(QStringLiteral("color: #c8d8e8;"));

        WorkspacePreview::themeSubtree(&root);
        QCOMPARE(root.styleSheet(), QStringLiteral("background: #08080a;"));
        QCOMPARE(label->styleSheet(), QStringLiteral("QLabel { color: #dcdce1; background: #abcdef; }"));
        QCOMPARE(kept->styleSheet(), QStringLiteral("background: #0f0f1a;"));
        QCOMPARE(keptChild->styleSheet(), QStringLiteral("color: #c8d8e8;"));
        QCOMPARE(root.property(WorkspacePreview::kOriginalQssProperty).toString(),
                 QStringLiteral("background: #0f0f1a;"));

        // A second pass changes nothing and counts nothing twice.
        WorkspacePreview::themeSubtree(&root);
        QCOMPARE(root.styleSheet(), QStringLiteral("background: #08080a;"));
        QCOMPARE(WorkspacePreview::unknownLiteralCounts().value(QStringLiteral("#abcdef")), 1);

        // The widget restyles itself at run time: the new sheet becomes
        // the original.
        label->setStyleSheet(QStringLiteral("QLabel { color: #ff4444; }"));
        WorkspacePreview::themeSubtree(&root);
        QCOMPARE(label->styleSheet(), QStringLiteral("QLabel { color: #a86b6d; }"));
        QCOMPARE(label->property(WorkspacePreview::kOriginalQssProperty).toString(),
                 QStringLiteral("QLabel { color: #ff4444; }"));
    }

    void showFilterThemesDialogs()
    {
        WorkspacePreview::setEnabledForTest(true);
        QWidget dialog;
        dialog.setStyleSheet(QStringLiteral("background: #0f0f1a;"));
        WorkspacePreview::themeOnShow(&dialog);
        QCOMPARE(dialog.styleSheet(), QStringLiteral("background: #0f0f1a;"));
        dialog.show();
        QCOMPARE(dialog.styleSheet(), QStringLiteral("background: #08080a;"));
    }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
    }
};

QTEST_MAIN(TestWorkspaceQssTranslator)
#include "tst_workspace_qss_translator.moc"
