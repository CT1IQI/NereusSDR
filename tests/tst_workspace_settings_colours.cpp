// no-port-check: NereusSDR-original test for the workspace preview's
// colours by meaning on Settings pages, dialogs and the applets' lit
// states (the colour round's items, each at its cause).
//
// Every case runs with the workspace look on and the live pass installed,
// as main.cpp does with the switch on. A case reads the sheet a widget
// ends up with, or a pixel of its grab, and checks the colour its meaning
// takes: the decided status tones (D21: ok #4caf6a, warn #d8a55f, danger
// #c25a5c), the container's surface for panels and data backgrounds
// (D22), the secondary text for notes, the glass for a selection, the
// engaged green for an amplifier in operate and the lit green (D15) for a
// checked processing button.
//
// The applet cases pose their states after the window is shown and the
// live pass has run, through the applets' own setters or a tuner model
// fed injected report lines; nothing connects to an amplifier, a tuner or
// a radio, and nothing starts discovery.
//
// Modification history (NereusSDR):
//   2026-10-09 - Written for the colour round on Settings pages, dialogs
//                 and applet states. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyleFactory>

#include <cmath>
#include <memory>

#include "core/AppSettings.h"
#include "core/TgxlConnection.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/PsaIndicatorWidget.h"
#include "gui/VaxFirstRunDialog.h"
#include "gui/applets/AmpApplet.h"
#include "gui/applets/Rf2ksApplet.h"
#include "gui/applets/TunerApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/diagnostics/RadioStatusPage.h"
#include "gui/meters/MeterPoller.h"
#include "gui/setup/hardware/AntennaAlexAlex2Tab.h"
#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/DiversityRadarWidget.h"
#include "models/RadioModel.h"
#include "models/TunerModel.h"

using namespace NereusSDR;

namespace {

QString role(const char* name)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(name)).name();
}

// The decided status tones (D21).
constexpr auto kOk = "#4caf6a";
constexpr auto kWarn = "#d8a55f";
constexpr auto kDanger = "#c25a5c";

int distance(const QColor& a, const QColor& b)
{
    return std::abs(a.red() - b.red()) + std::abs(a.green() - b.green())
        + std::abs(a.blue() - b.blue());
}

// Shown, with the live pass done.
void showAndSettle(QWidget& w)
{
    w.show();
    QApplication::processEvents();
    QTest::qWait(30);
    QApplication::processEvents();
}

QColor centre(QWidget* w)
{
    const QImage img = w->grab().toImage();
    return img.pixelColor(img.width() / 2, img.height() / 2);
}

QPushButton* buttonWithText(QWidget& root, const QString& text)
{
    for (auto* b : root.findChildren<QPushButton*>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

class TstWorkspaceSettingsColours : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(NereusSDR::Test::RemoteWindowHarness::useIsolatedProfile(QStringLiteral("workspace-settings-colours")));
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        WorkspacePreview::setEnabledForTest(true);
        auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
        QVERIFY(app != nullptr);
        applyDarkPalette(*app);
        applyAppBaselineQss(*app);
        applyWorkspacePalette(*app);
    }

    // The mechanism: a sheet a shown widget sets later, on a state
    // change, is translated by the live pass (WorkspacePreview.cpp
    // LiveThemingFilter, StyleChange). Holds before this round too.
    void sheetSetAfterShowIsTranslated()
    {
        QWidget host;
        auto* button = new QPushButton(QStringLiteral("state"), &host);
        showAndSettle(host);
        button->setStyleSheet(QStringLiteral("QPushButton { background: #172534; }"));
        QApplication::processEvents();
        QVERIFY2(!button->styleSheet().contains(QLatin1String("#172534")),
                 qPrintable(button->styleSheet()));
        // The row's role (button), raised by componentize().
        QVERIFY2(button->styleSheet().contains(QLatin1String("qlineargradient")), qPrintable(button->styleSheet()));
    }

    // Item 1: a disabled 4O3A page paints the workspace page, not the
    // platform's disabled window (AppTheme.h applyWorkspacePalette).
    void disabledPageKeepsTheWorkspaceSurface()
    {
        const QPalette p = QApplication::palette();
        QCOMPARE(p.color(QPalette::Disabled, QPalette::Window).name(), role("app-bg"));
        QWidget page;
        page.setAutoFillBackground(true);
        page.resize(40, 40);
        page.setEnabled(false);
        showAndSettle(page);
        QCOMPARE(centre(&page).name(), role("app-bg"));
    }

    // Item 1: the PGXL and TGXL warnings are the decided warn.
    void fourO3AWarningsAreWarn()
    {
        for (const char* sheet : { "color: #e8c01e;", "color: #e87c1e; font-style: italic;" }) {
            const QString look = WorkspacePreview::lookQss(QString::fromLatin1(sheet));
            QVERIFY2(look.contains(QLatin1String(kWarn)), qPrintable(look));
        }
    }

    // Item 2: the NNR session chip is a warning badge: warn text on the
    // badge's warn ground, no edge of its own.
    void nnrChipIsAWarningBadge()
    {
        const QString look = WorkspacePreview::lookQss(QStringLiteral(
            "QLabel { color: #ffd166; background: #302810; border: 1px solid #806820;"
            " border-radius: 3px; padding: 3px; font-size: 10px; font-weight: bold; }"));
        QVERIFY2(look.contains(QLatin1String(kWarn)), qPrintable(look));
        QVERIFY2(look.contains(QStringLiteral("background: %1").arg(role("badge-warn-bg"))), qPrintable(look));
        QVERIFY2(look.contains(QStringLiteral("solid %1").arg(role("badge-warn-bg"))), qPrintable(look));
    }

    // Item 3: Reset is danger; the lit state-machine step is ok, the
    // others the surface; the status bar the surface.
    void hl2IoResetAndStepsTakeTheirMeaning()
    {
        RadioModel radio;
        Hl2IoBoardTab tab(&radio);
        showAndSettle(tab);
        QPushButton* reset = buttonWithText(tab, QStringLiteral("Reset I/O board"));
        QVERIFY(reset != nullptr);
        QVERIFY2(reset->styleSheet().contains(QLatin1String(kDanger)), qPrintable(reset->styleSheet()));
        int lit = 0;
        int off = 0;
        for (auto* f : tab.findChildren<QFrame*>()) {
            const QString s = f->styleSheet();
            if (!s.contains(QLatin1String("padding: 2px"))) {
                continue;
            }
            QVERIFY2(!s.contains(QLatin1String("#22cc44")) && !s.contains(QLatin1String("#222")),
                     qPrintable(s));
            if (s.contains(QStringLiteral("2px solid %1").arg(QLatin1String(kOk)))) {
                ++lit;
            } else if (s.contains(role("panel"))) {
                ++off;
            }
        }
        QCOMPARE(lit, 1);
        QCOMPARE(off, 11);
    }

    // Item 4: the PA Gain note is the secondary text.
    void paGainNoteIsSecondaryText()
    {
        const QString look = WorkspacePreview::lookQss(QStringLiteral("QLabel { color: #80b0d0; padding: 8px; }"));
        QVERIFY2(look.contains(role("text-secondary")), qPrintable(look));
    }

    // Item 5: the Radio Status bar is a panel on the page; "No issues
    // found" is ok.
    void radioStatusBarIsAPanel()
    {
        RadioModel radio;
        RadioStatusPage page(&radio);
        showAndSettle(page);
        bool bar = false;
        for (auto* f : page.findChildren<QFrame*>()) {
            const QString s = f->styleSheet();
            QVERIFY2(!s.contains(QLatin1String("#0a1a10")) && !s.contains(role("ok-bg")), qPrintable(s));
            bar = bar || (s.contains(QStringLiteral("background: %1").arg(role("panel")))
                          && s.contains(QStringLiteral("solid %1").arg(role("border"))));
        }
        QVERIFY(bar);
        for (auto* list : page.findChildren<QListWidget*>()) {
            for (int i = 0; i < list->count(); ++i) {
                if (list->item(i)->text().startsWith(QLatin1String("No issues found"))) {
                    QCOMPARE(list->item(i)->foreground().color().name(), QString::fromLatin1(kOk));
                }
            }
        }
    }

    // Item 6: the polar plot's background is the container's surface (D22).
    // Inside the sensitivity lobe the sample is the lobe's translucent ink
    // (0,220,255,60; DiversityRadarWidget.cpp:220-222) over that surface;
    // the ink itself is unchanged.
    void diversityPlotBackgroundIsTheSurface()
    {
        DiversityRadarWidget radar;
        radar.resize(200, 200);
        showAndSettle(radar);
        const QImage img = radar.grab().toImage();
        const QColor panel(role("panel"));
        const double a60 = 60.0 / 255.0;
        const QColor lobeOverPanel(int(std::lround(panel.red() * (1 - a60))),
                                   int(std::lround(panel.green() * (1 - a60) + 220 * a60)),
                                   int(std::lround(panel.blue() * (1 - a60) + 255 * a60)));
        const double r = (200 - 4) * 0.5;
        int surface = 0;
        QStringList seen;
        for (double k : { 0.4, 0.8 }) {
            for (double deg : { 30.0, 150.0, 210.0, 330.0 }) {
                const double a = qDegreesToRadians(deg);
                const QColor px = img.pixelColor(int(100 + std::cos(a) * r * k),
                                                 int(100 + std::sin(a) * r * k));
                surface += (distance(px, panel) <= 3 || distance(px, lobeOverPanel) <= 3) ? 1 : 0;
                seen << px.name();
            }
        }
        QVERIFY2(surface >= 7, qPrintable(QStringLiteral("%1 of 8: %2").arg(surface).arg(seen.join(u' '))));
    }

    // Item 7: the VAX product captions are the secondary text, not the
    // selection blue; the "not detected" emphasis is warn.
    void vaxCaptionsAndEmphasis()
    {
        VaxFirstRunDialog dialog(FirstRunScenario::MacNative, {});
        showAndSettle(dialog);
        int captions = 0;
        bool warn = false;
        for (auto* l : dialog.findChildren<QLabel*>()) {
            const QString s = l->styleSheet();
            if (s.contains(QLatin1String("letter-spacing"))) {
                QVERIFY2(!s.contains(role("accent")), qPrintable(s));
                captions += s.contains(role("text-secondary")) ? 1 : 0;
            }
            warn = warn || l->text().contains(QStringLiteral("color:%1'>The HAL plugin").arg(QLatin1String(kWarn)));
        }
        QVERIFY(captions > 0);
        QVERIFY(warn);
    }

    // Item 8: "Currently selected" reads as the page's secondary text.
    void alex2SelectedLabelIsSecondaryText()
    {
        RadioModel radio;
        AntennaAlexAlex2Tab tab(&radio);
        showAndSettle(tab);
        int found = 0;
        for (auto* l : tab.findChildren<QLabel*>()) {
            if (l->text().startsWith(QLatin1String("Currently selected"))) {
                QVERIFY2(l->styleSheet().contains(role("text-secondary")), qPrintable(l->styleSheet()));
                ++found;
            }
        }
        QCOMPARE(found, 1);
    }

    // Item 9: the Container Settings preview paints the window's content
    // background (ContainerWidget::contentBackground()), and a preview
    // tile lets it show through.
    void containerPreviewPaintsTheWindowBackground()
    {
        ContainerContentRegistry registry;
        MeterPoller poller;
        ContainerPreviewWidget preview(registry, poller);
        ContainerDocument doc;
        doc.layout = ContentLayout::VerticalStack;
        doc.contents.append(registry.makeEntry(QStringLiteral("applet:s_meter")));
        preview.setDocument(doc);
        preview.resize(300, 240);
        showAndSettle(preview);
        QVERIFY2(preview.styleSheet().contains(role("panel")), qPrintable(preview.styleSheet()));
        bool tile = false;
        for (auto* w : preview.findChildren<QWidget*>()) {
            QVERIFY2(!w->styleSheet().contains(QLatin1String("#172534")), qPrintable(w->styleSheet()));
            tile = tile || w->styleSheet().startsWith(QLatin1String("background:transparent"));
        }
        QVERIFY(tile);
        QCOMPARE(centre(&preview).name(), role("panel"));
    }

    // JJ's live build: PS-A checked after the pass is the lit green (D15).
    void psaCheckedAfterShowIsLit()
    {
        RadioModel radio;
        TxApplet tx(&radio);
        showAndSettle(tx);
        auto* psa = tx.findChild<QPushButton*>(QStringLiteral("TxAppletPsaBtn"));
        QVERIFY(psa != nullptr);
        psa->setVisible(true);
        {
            const QSignalBlocker block(psa);
            psa->setChecked(true);
        }
        QApplication::processEvents();
        QVERIFY2(!psa->styleSheet().contains(QLatin1String("#006030")), qPrintable(psa->styleSheet()));
        const QColor px = centre(psa);
        QVERIFY2(distance(px, QColor(0x00, 0x60, 0x30)) > 20, qPrintable(px.name()));
        QVERIFY2(px.green() > px.red() && px.green() > px.blue(), qPrintable(px.name()));
    }

    // OPERATE on, posted after the pass, is the engaged green (dsp-on, as
    // Longpath's OPERATE), in all three applets.
    void operateOnAfterShowIsTheEngagedGreen()
    {
        const QString engaged = role("dsp-on-bg");
        {
            TgxlConnection connection;
            TunerModel tuner;
            tuner.bindConnection(&connection);
            TunerApplet applet(nullptr, &tuner);
            showAndSettle(applet);
            connection.injectLineForTesting(QStringLiteral("V1.2.17"));
            connection.injectLineForTesting(QStringLiteral("S0|state state=1 bypass=0"));
            QApplication::processEvents();
            QPushButton* op = applet.operateButtonForTesting();
            QCOMPARE(op->text(), QStringLiteral("OPERATE"));
            QVERIFY2(op->styleSheet().contains(engaged), qPrintable(op->styleSheet()));
            QVERIFY2(!op->styleSheet().contains(QLatin1String("#006030")), qPrintable(op->styleSheet()));
        }
        {
            AmpApplet applet(nullptr);
            showAndSettle(applet);
            applet.setState(QStringLiteral("OPERATE"));
            QApplication::processEvents();
            QPushButton* op = buttonWithText(applet, QStringLiteral("OPERATE"));
            QVERIFY(op != nullptr);
            QVERIFY2(op->styleSheet().contains(engaged), qPrintable(op->styleSheet()));
        }
        {
            Rf2ksApplet applet(nullptr);
            showAndSettle(applet);
            applet.setOperateMode(QStringLiteral("OPERATE"));
            QApplication::processEvents();
            QPushButton* op = buttonWithText(applet, QStringLiteral("OPERATE"));
            QVERIFY(op != nullptr);
            QVERIFY2(op->styleSheet().contains(engaged), qPrintable(op->styleSheet()));
        }
    }

    // The selected tuner antenna, posted after the pass, takes the engaged
    // green its sheet shares with OPERATE (TunerApplet.cpp:1004-1009): the
    // buttons are not checkable, so the live pass reads a flat blue as an
    // action button, and the glass would need them checkable (stopped for
    // JJ).
    void selectedAntennaAfterShowIsTheEngagedGreen()
    {
        TgxlConnection connection;
        TunerModel tuner;
        tuner.bindConnection(&connection);
        TunerApplet applet(nullptr, &tuner);
        showAndSettle(applet);
        // The antenna row shows for a TGXL with the switch, reached
        // here as a station state: model fields only, no connection
        // (TunerModel.cpp:39-80).
        TunerModel::StationConnectionState station;
        station.phase = TunerModel::ConnectionPhase::Connected;
        tuner.setStationConnectionState(station);
        connection.injectLineForTesting(QStringLiteral("V1.2.17"));
        connection.injectLineForTesting(QStringLiteral("S0|state state=1 bypass=0 antA=1 3way=1"));
        QApplication::processEvents();
        QPushButton* ant2 = buttonWithText(applet, QStringLiteral("ANT 2"));
        QVERIFY(ant2 != nullptr);
        QVERIFY2(ant2->styleSheet().contains(role("dsp-on-bg")), qPrintable(ant2->styleSheet()));
        QVERIFY2(!ant2->styleSheet().contains(QLatin1String("#006030")), qPrintable(ant2->styleSheet()));
        QVERIFY(ant2->isVisibleTo(&applet));
        QCOMPARE(centre(ant2).name(), role("dsp-on-bg"));
    }

    // The PureSignal banner's status fills are the decided tones (D21).
    void pureSignalBadgeIsStatus()
    {
        PsaIndicatorWidget badge(nullptr);
        showAndSettle(badge);
        badge.setPsEnabled(true);
        badge.setMox(false);
        QApplication::processEvents();
        QCOMPARE(badge.psBackgroundColor().name(), QString::fromLatin1(kOk));
    }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
    }
};

QTEST_MAIN(TstWorkspaceSettingsColours)
#include "tst_workspace_settings_colours.moc"
