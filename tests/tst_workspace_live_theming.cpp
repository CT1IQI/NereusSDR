// no-port-check: NereusSDR-original test for the workspace preview's live
// scoped pass (WorkspacePreview::installLiveTheming, installed by
// applyWorkspacePalette).
//
// With the preview switch on, every widget gets the scoped pass when it is
// shown, with no call site: a Setup page and tab realized after the dialog
// is open, a dialog that never asks for it, a kPopupMenu menu (also one
// opened from a skipped widget), a label added to an open dialog and a
// sheet set later. With the switch off nothing changes.
//
// Also the preview's table grid: FreeDV Reporter's QTableView (no
// stylesheet of its own; a plain table stands in for it here, since the
// dialog needs a reporter client) drew its grid lines in the cell
// background's colour. Grid lines and header section edges take the
// workspace roles Longpath uses for them.
//
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace visual review. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QHeaderView>
#include <QMenu>
#include <QStandardItemModel>
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>

#include "gui/DiversityDialog.h"
#include "gui/NetworkDiagnosticsDialog.h"
#include "gui/SetupDialog.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/styles/QssTranslator.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

// The classes the scoped pass leaves alone (WorkspacePreview.cpp).
constexpr const char* kSkipped[] = {
    "NereusSDR::VfoWidget",          "NereusSDR::VfoLevelBar",
    "NereusSDR::ScrollableLabel",    "NereusSDR::FmOptContainer",
    "NereusSDR::DigOffsetContainer", "NereusSDR::RttyMarkShiftContainer",
    "NereusSDR::VaxChannelSelector", "NereusSDR::SpectrumWidget",
    "NereusSDR::ColorSwatchButton",  "NereusSDR::GradientPickerWidget",
};

bool skippedWidget(const QWidget* w)
{
    if (w->property(WorkspacePreview::kSkipProperty).toBool()) {
        return true;
    }
    for (const char* cls : kSkipped) {
        if (w->inherits(cls)) {
            return true;
        }
    }
    return false;
}

QString describe(const QWidget* w)
{
    return QStringLiteral("%1 '%2'").arg(QLatin1String(w->metaObject()->className()),
                                         w->objectName());
}

struct PassReport {
    // Shown widgets with a stylesheet the pass has not handled.
    QStringList unthemed;
    // Widgets whose sheet held a legacy literal the pass replaced.
    int translated = 0;
};

bool holdsLegacyLiteral(const QString& qss)
{
    const QString lower = qss.toLower();
    for (const QssTranslator::Rule& rule : QssTranslator::defaultTable()) {
        if (lower.contains(rule.legacyHex.toLower())) {
            return true;
        }
    }
    return false;
}

void walk(QWidget* w, PassReport& report)
{
    if (!w->isVisible() || skippedWidget(w)) {
        return;
    }
    if (!w->styleSheet().isEmpty()) {
        const QVariant themed = w->property(WorkspacePreview::kThemedQssProperty);
        if (!themed.isValid() || themed.toString() != w->styleSheet()) {
            report.unthemed.append(describe(w));
        }
        const QString original = w->property(WorkspacePreview::kOriginalQssProperty).toString();
        if (holdsLegacyLiteral(original) && original != w->styleSheet()) {
            ++report.translated;
        }
    }
    for (QObject* o : w->children()) {
        if (o->isWidgetType() && !static_cast<QWidget*>(o)->isWindow()) {
            walk(static_cast<QWidget*>(o), report);
        }
    }
}

PassReport passReport(QWidget* root)
{
    PassReport report;
    walk(root, report);
    return report;
}

// Every widget of the application that the pass has marked.
int markedWidgets()
{
    int marked = 0;
    for (QWidget* w : QApplication::allWidgets()) {
        if (w->property(WorkspacePreview::kThemedQssProperty).isValid()
            || w->property(WorkspacePreview::kOriginalQssProperty).isValid()) {
            ++marked;
        }
    }
    return marked;
}


struct TableColours {
    QColor grid;
    QColor cell;
    QColor headerEdge;
    QColor headerFill;
};

// A table like FreeDV Reporter's: grid on, no stylesheet, a horizontal
// header with three sections, shown and grabbed at its own size.
TableColours tableColours()
{
    QStandardItemModel model(3, 3);
    QTableView table;
    table.setModel(&model);
    table.setShowGrid(true);
    table.verticalHeader()->setVisible(false);
    table.resize(320, 160);
    table.show();
    QCoreApplication::processEvents();

    const QImage view = table.viewport()->grab().toImage();
    const int gridX = table.columnViewportPosition(0) + table.columnWidth(0) - 1;
    const int midY = table.rowViewportPosition(0) + table.rowHeight(0) / 2;
    QHeaderView* header = table.horizontalHeader();
    const QImage head = header->grab().toImage();
    const int edgeX = header->sectionViewportPosition(0) + header->sectionSize(0) - 1;
    TableColours c;
    c.grid = view.pixelColor(gridX, midY);
    // Right of the line: the cell left of it is the current index.
    c.cell = view.pixelColor(gridX + 3, midY);
    c.headerEdge = head.pixelColor(edgeX, head.height() / 2);
    c.headerFill = head.pixelColor(edgeX + 3, head.height() / 2);
    return c;
}

QColor roleColour(const char* role)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(role));
}

constexpr auto kLegacyPanel = "background: #0f0f1a;";
constexpr auto kLegacyLabel = "QLabel { color: #c8d8e8; background: #abcdef; }";
constexpr auto kLegacyLater = "QLabel { color: #8090a0; }";

} // namespace

class TestWorkspaceLiveTheming : public QObject
{
    Q_OBJECT

private slots:
    // Runs first, before the switch is ever on: nothing is translated.
    void switchOffChangesNothing()
    {
        WorkspacePreview::setEnabledForTest(false);
        RadioModel model;

        SetupDialog setup(&model);
        setup.show();
        setup.selectPage(QStringLiteral("NR/ANF"));
        QWidget* page = setup.realizedPageForTest(QStringLiteral("NR/ANF"));
        QVERIFY(page != nullptr);
        auto* tabs = page->findChild<QTabWidget*>();
        QVERIFY(tabs != nullptr);
        tabs->setCurrentIndex(tabs->count() - 1);

        DiversityDialog diversity(&model);
        diversity.show();

        QWidget host;
        host.setProperty(WorkspacePreview::kSkipProperty, true);
        QMenu menu(&host);
        menu.setStyleSheet(QString::fromLatin1(kPopupMenu));
        menu.addAction(QStringLiteral("Ant 1"));
        menu.popup(QPoint(10, 10));
        QVERIFY(menu.isVisible());

        QDialog plain;
        plain.setStyleSheet(QString::fromLatin1(kLegacyPanel));
        auto* label = new QLabel(QStringLiteral("added"), &plain);
        label->setStyleSheet(QString::fromLatin1(kLegacyLabel));
        plain.show();
        label->setStyleSheet(QString::fromLatin1(kLegacyLater));
        QCoreApplication::processEvents();

        QCOMPARE(menu.styleSheet(), QString::fromLatin1(kPopupMenu));
        QCOMPARE(plain.styleSheet(), QString::fromLatin1(kLegacyPanel));
        QCOMPARE(label->styleSheet(), QString::fromLatin1(kLegacyLater));
        QCOMPARE(markedWidgets(), 0);
        menu.hide();
    }

    // The switch off: the table draws as main.cpp's dark theme draws it
    // (applyDarkPalette and applyAppBaselineQss), and no table rule of the
    // preview is in the application stylesheet.
    void tableGridUnchangedWithTheSwitchOff()
    {
        WorkspacePreview::setEnabledForTest(false);
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        const TableColours c = tableColours();
        qInfo("switch off: grid %s cell %s header edge %s header fill %s",
              qPrintable(c.grid.name()), qPrintable(c.cell.name()),
              qPrintable(c.headerEdge.name()), qPrintable(c.headerFill.name()));
        // Fusion's grid: Window darker 120, clear of the dark theme's cells.
        QCOMPARE(c.grid.name(), qApp->palette().color(QPalette::Window).darker(120).name());
        QVERIFY(c.grid != c.cell);
        QVERIFY(!qApp->styleSheet().contains(QLatin1String("gridline-color")));
        QVERIFY(!qApp->styleSheet().contains(QLatin1String("QHeaderView::section")));
    }

    void setupPagesAndTabsRealizedLaterAreThemed()
    {
        turnOn();
        RadioModel model;
        SetupDialog setup(&model);
        setup.show();
        QVERIFY(!setup.isPageRealizedForTest(QStringLiteral("NR/ANF")));

        setup.selectPage(QStringLiteral("NR/ANF"));
        QWidget* page = setup.realizedPageForTest(QStringLiteral("NR/ANF"));
        QVERIFY(page != nullptr);
        QVERIFY(page->isVisible());
        PassReport first = passReport(page);
        QVERIFY2(first.unthemed.isEmpty(), qPrintable(first.unthemed.join(QLatin1String(", "))));
        QVERIFY(first.translated > 0);

        auto* tabs = page->findChild<QTabWidget*>();
        QVERIFY(tabs != nullptr);
        QVERIFY(tabs->count() > 1);
        tabs->setCurrentIndex(tabs->count() - 1);
        QWidget* lastTab = tabs->currentWidget();
        QVERIFY(lastTab->isVisible());
        PassReport later = passReport(lastTab);
        QVERIFY2(later.unthemed.isEmpty(), qPrintable(later.unthemed.join(QLatin1String(", "))));
        QVERIFY(later.translated > 0);
    }

    void dialogsWithoutACallSiteAreThemed()
    {
        turnOn();
        RadioModel model;

        DiversityDialog diversity(&model);
        diversity.show();
        PassReport d = passReport(&diversity);
        QVERIFY2(d.unthemed.isEmpty(), qPrintable(d.unthemed.join(QLatin1String(", "))));
        QVERIFY(d.translated > 0);

        // Offline: no connection and no audio engine; it only reads stats.
        NetworkDiagnosticsDialog network(&model, nullptr);
        network.show();
        PassReport n = passReport(&network);
        QVERIFY2(n.unthemed.isEmpty(), qPrintable(n.unthemed.join(QLatin1String(", "))));
        QVERIFY(n.translated > 0);

        QDialog plain;
        plain.setStyleSheet(QString::fromLatin1(kLegacyPanel));
        plain.show();
        QCOMPARE(plain.styleSheet(), QStringLiteral("background: #08080a;"));
    }

    void popupMenuIsThemedAlsoFromASkippedWidget()
    {
        turnOn();
        QWidget host;
        host.setProperty(WorkspacePreview::kSkipProperty, true);
        QMenu menu(&host);
        menu.setStyleSheet(QString::fromLatin1(kPopupMenu));
        menu.addAction(QStringLiteral("Ant 1"));
        menu.popup(QPoint(10, 10));
        QVERIFY(menu.isVisible());

        const QString sheet = menu.styleSheet();
        QVERIFY2(!sheet.contains(QLatin1String("#1a2a3a"), Qt::CaseInsensitive), qPrintable(sheet));
        QVERIFY2(!sheet.contains(QLatin1String("#304050"), Qt::CaseInsensitive), qPrintable(sheet));
        QCOMPARE(menu.property(WorkspacePreview::kOriginalQssProperty).toString(),
                 QString::fromLatin1(kPopupMenu));
        menu.hide();
    }

    void addedAndRestyledWidgetsAreThemedOnce()
    {
        turnOn();
        WorkspacePreview::resetUnknownLiteralCounts();
        QDialog plain;
        auto* layout = new QVBoxLayout(&plain);
        plain.show();

        // Added to a dialog that is already open.
        auto* label = new QLabel(QStringLiteral("added"));
        label->setStyleSheet(QString::fromLatin1(kLegacyLabel));
        layout->addWidget(label);
        QTRY_VERIFY(label->isVisible());
        QCOMPARE(label->styleSheet(),
                 QStringLiteral("QLabel { color: #dcdce1; background: #abcdef; }"));
        QCOMPARE(WorkspacePreview::unknownLiteralCounts().value(QStringLiteral("#abcdef")), 1);

        // Shown again: not translated again.
        plain.hide();
        plain.show();
        QCOMPARE(WorkspacePreview::unknownLiteralCounts().value(QStringLiteral("#abcdef")), 1);
        QCOMPARE(label->property(WorkspacePreview::kOriginalQssProperty).toString(),
                 QString::fromLatin1(kLegacyLabel));

        // A new sheet set while shown is themed, once.
        label->setStyleSheet(QString::fromLatin1(kLegacyLater));
        QVERIFY(label->styleSheet() != QString::fromLatin1(kLegacyLater));
        QCOMPARE(label->property(WorkspacePreview::kOriginalQssProperty).toString(),
                 QString::fromLatin1(kLegacyLater));
        const QString themed = label->styleSheet();
        QCOMPARE(WorkspacePreview::lookQss(QString::fromLatin1(kLegacyLater), label), themed);
    }

    // The switch on: grid lines in border-subtle (Longpath tableStyle's
    // gridline-color) and the header's section edges in border, both clear
    // of the cell and header fill. Fusion drew the grid in Window darker
    // 120: #070708 on cells of #050507.
    void tableGridAndHeaderEdgesReadInThePreview()
    {
        turnOn();
        const TableColours c = tableColours();
        qInfo("switch on: grid %s cell %s header edge %s header fill %s",
              qPrintable(c.grid.name()), qPrintable(c.cell.name()),
              qPrintable(c.headerEdge.name()), qPrintable(c.headerFill.name()));
        QVERIFY(c.grid != c.cell);
        QCOMPARE(c.grid.name(), roleColour("border-subtle").name());
        QVERIFY(c.headerEdge != c.headerFill);
        QCOMPARE(c.headerEdge.name(), roleColour("border").name());
    }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
    }

private:
    static void turnOn()
    {
        WorkspacePreview::setEnabledForTest(true);
        applyWorkspacePalette(*qApp);
    }
};

QTEST_MAIN(TestWorkspaceLiveTheming)
#include "tst_workspace_live_theming.moc"
