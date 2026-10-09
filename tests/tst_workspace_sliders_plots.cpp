// no-port-check: NereusSDR-original test for JJ's speaker volume fill, the
// visible zoom track, the plot backgrounds that follow their container and
// two reference details of the workspace preview (src/gui/styles/
// WorkspaceComponents componentize(), WorkspacePreview::dataBackground(),
// ParametricEqWidget, AmpViewChart, SpectrumOverlayPanel and the visual
// tool's LongpathLook::themed()).
//
// Covers, with the switch on: the PC and RADIO speaker volume sliders keep a
// filled side up to the handle (D24) and every other slider stays unfilled
// (D5); the zoom bar's track is the dark inset with a grey rim all round,
// 8 px overall, with a 16 px handle (D27); the TX CFC / TX EQ plot and the amp
// view chart take their container's background (D22); the Longpath
// reference column keeps Longpath's own spin arrows; the overlay's step
// attenuator check box shows the decided checked indicator at its size.
// With the switch off, today's sheets and colours exactly.
//
// Modification history (NereusSDR):
//   2026-10-09 - Written for the speaker volume fill, the zoom track and
//                 the plot backgrounds. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - The RADIO fill and the Audio Setup reason lines take the
//                 workspace amber (JJ's choice A).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The zoom track test follows JJ's D27: the dark track with
//                 the grey rim, 8 px overall, the handle 16 px.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionSlider>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <utility>

#include "gui/AmpViewChart.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "visual/LongpathControlStyles.h"

using namespace NereusSDR;

namespace {

// The zoom bar's sheet under the pans (MainWindow, "Zoom slider bar below
// spectrum"), as the program sets it.
const char* const kZoomBarSheet =
    "QSlider { background: #0a0a14; }"
    "QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
    "QSlider::handle:horizontal { background: #00b4d8; width: 14px; margin: -4px 0; border-radius: 7px; }";

// The Audio Setup reason lines (AudioOutputsPage kReason), as the page
// sets them.
const char* const kReasonSheet = "QLabel { color: #e0a030; font-size: 11px; }";

class ProbeSlider : public QSlider
{
public:
    using QSlider::QSlider;
    QRect handleRect()
    {
        QStyleOptionSlider o;
        initStyleOption(&o);
        return style()->subControlRect(QStyle::CC_Slider, &o, QStyle::SC_SliderHandle, this);
    }
    QRect grooveRect()
    {
        QStyleOptionSlider o;
        initStyleOption(&o);
        return style()->subControlRect(QStyle::CC_Slider, &o, QStyle::SC_SliderGroove, this);
    }
};

QColor role(const char* r)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(r));
}

double luminance(const QColor& c)
{
    const auto lin = [](double v) {
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// The most common colour inside `r` of `img`.
QColor commonColour(const QImage& img, const QRect& r)
{
    QHash<QRgb, int> counts;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            ++counts[img.pixel(x, y) | 0xff000000u];
        }
    }
    QRgb best = 0;
    int most = -1;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() > most) {
            most = it.value();
            best = it.key();
        }
    }
    return QColor::fromRgb(best);
}

class WorkspaceApp
{
public:
    WorkspaceApp()
        : m_app(qobject_cast<QApplication*>(QCoreApplication::instance()))
        , m_palette(m_app->palette())
        , m_sheet(m_app->styleSheet())
    {
        WorkspacePreview::setEnabledForTest(true);
        applyWorkspacePalette(*m_app);
    }
    ~WorkspaceApp()
    {
        m_app->setStyleSheet(m_sheet);
        m_app->setPalette(m_palette);
        WorkspacePreview::setEnabledForTest(false);
    }
    WorkspaceApp(const WorkspaceApp&) = delete;
    WorkspaceApp& operator=(const WorkspaceApp&) = delete;

private:
    QApplication* m_app;
    QPalette m_palette;
    QString m_sheet;
};

// A speaker-sized slider (100 x 16, as the title bar sets it) at 60 %.
ProbeSlider* speakerSlider(QWidget& host, const char* sheet, bool filled)
{
    auto* s = new ProbeSlider(Qt::Horizontal, &host);
    if (filled) {
        s->setProperty(WorkspacePreview::kFilledSliderProperty, true);
    }
    s->setRange(0, 100);
    s->setValue(60);
    s->setFixedSize(100, 16);
    s->setStyleSheet(QLatin1String(sheet));
    return s;
}

// The colour of the groove halfway between its left end and the handle.
QColor leftOfHandle(ProbeSlider* s)
{
    const QRect hr = s->handleRect();
    const QImage img = s->grab().toImage();
    return img.pixelColor(hr.left() / 2, hr.center().y());
}

} // namespace

class TestWorkspaceSlidersPlots : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        WorkspacePreview::setEnabledForTest(false);
    }

    // D24: the PC speaker volume keeps its filled side, today's #00b4d8
    // through the translation: the accent role.
    void pcSpeakerSliderIsFilled()
    {
        WorkspaceApp look;
        QWidget host;
        ProbeSlider* s = speakerSlider(host, HeaderVolumeStyle::kPcSlider, true);
        WorkspacePreview::themeSubtree(&host);
        QCOMPARE(s->size(), QSize(100, 16));
        const QColor fill = leftOfHandle(s);
        QVERIFY2(fill == role("accent"), qPrintable(fill.name()));
        // D5's handle stays: the accent rim at its left edge.
        const QRect hr = s->handleRect();
        const QColor rim = s->grab().toImage().pixelColor(hr.left(), hr.center().y());
        QVERIFY2(rim == role("accent"), qPrintable(rim.name()));
    }

    // D24: the RADIO speaker volume keeps its filled side when enabled
    // (today's #e0a030 through the translation: the workspace amber,
    // JJ's choice A) and, as today, shows no fill when disabled.
    void radioSpeakerSliderIsFilled()
    {
        WorkspaceApp look;
        QWidget host;
        ProbeSlider* s = speakerSlider(host, HeaderVolumeStyle::kRadioSlider, true);
        WorkspacePreview::themeSubtree(&host);
        const QColor fill = leftOfHandle(s);
        QVERIFY2(fill == role("measured"), qPrintable(fill.name()));
        QCOMPARE(role("measured"), QColor(QStringLiteral("#d8a55f")));
        s->setEnabled(false);
        const QColor off = leftOfHandle(s);
        QVERIFY2(off == role("inset"), qPrintable(off.name()));
    }

    // The Audio Setup "No radio connected" lines take the same workspace
    // amber as the RADIO fill.
    void radioSpeakerReasonTakesTheAmber()
    {
        WorkspaceApp look;
        QWidget host;
        auto* reason = new QLabel(QStringLiteral("No radio connected"), &host);
        reason->setStyleSheet(QLatin1String(kReasonSheet));
        WorkspacePreview::themeSubtree(&host);
        reason->ensurePolished();
        const QColor c = reason->palette().color(QPalette::WindowText);
        QVERIFY2(c == role("measured"), qPrintable(c.name()));
    }

    // D5: a slider with the same sheet that is not a speaker volume, and
    // every other sheet-styled slider, keeps no filled side.
    void otherSlidersStayUnfilled()
    {
        WorkspaceApp look;
        QWidget host;
        ProbeSlider* s = speakerSlider(host, HeaderVolumeStyle::kPcSlider, false);
        WorkspacePreview::themeSubtree(&host);
        const QColor groove = leftOfHandle(s);
        QVERIFY2(groove == role("inset"), qPrintable(groove.name()));
    }

    // D27 (JJ's choice 3): the zoom bar's track stays the dark inset with a
    // brighter grey rim (the border role) on every side, 6 px inside a 1 px
    // rim (8 px overall), and both long edges read against the bar at about
    // 1.42:1; the handle stays 16 px overall. A slider with the zoom bar's
    // sheet that is not marked keeps the decided groove.
    void zoomTrackIsVisible()
    {
        WorkspaceApp look;
        const auto make = [](QWidget& host, bool visible) {
            auto* s = new ProbeSlider(Qt::Horizontal, &host);
            if (visible) {
                s->setProperty(WorkspacePreview::kVisibleTrackProperty, true);
            }
            s->setRange(1, 768);
            s->setValue(1);
            s->setFixedSize(400, 20);
            s->setStyleSheet(QLatin1String(kZoomBarSheet));
            WorkspacePreview::themeSubtree(&host);
            return s;
        };
        QWidget host;
        ProbeSlider* s = make(host, true);
        const QImage img = s->grab().toImage();
        // A column clear of the handle: the rows that differ from the bar.
        const int x = s->width() - 40;
        const QColor bar = img.pixelColor(x, 0);
        int top = -1;
        int bottom = -1;
        for (int y = 0; y < img.height(); ++y) {
            if (img.pixelColor(x, y) != bar) {
                if (top < 0) {
                    top = y;
                }
                bottom = y;
            }
        }
        QVERIFY2(top >= 0, qPrintable(bar.name()));
        QCOMPARE(bottom - top + 1, 8);
        QCOMPARE(img.pixelColor(x, top), role("border"));
        QCOMPARE(img.pixelColor(x, bottom), role("border"));
        for (int y = top + 1; y < bottom; ++y) {
            QVERIFY2(img.pixelColor(x, y) == role("inset"), qPrintable(img.pixelColor(x, y).name()));
        }
        const double edge = contrast(role("border"), bar);
        QVERIFY2(edge > 1.42 && edge < 1.43, qPrintable(QString::number(edge)));
        // The handle stays 16 px overall: the drawn rows through its middle.
        const int hx = s->handleRect().center().x();
        int handleRows = 0;
        for (int y = 0; y < img.height(); ++y) {
            if (img.pixelColor(hx, y) != bar) {
                ++handleRows;
            }
        }
        QCOMPARE(handleRows, 16);
        // Not marked: the decided groove.
        QWidget plainHost;
        ProbeSlider* plain = make(plainHost, false);
        const QImage pimg = plain->grab().toImage();
        const QColor groove = pimg.pixelColor(plain->width() - 40, plain->grooveRect().center().y());
        QVERIFY2(groove == role("inset"), qPrintable(groove.name()));
    }

    // D22: the TX CFC / TX EQ plot takes its container's background (the
    // dialog's app-bg), and so does its margin; a user container background
    // shows through.
    void parametricPlotFollowsTheContainer()
    {
        {
            WorkspaceApp look;
            QWidget host;
            auto* w = new ParametricEqWidget(&host);
            w->setEditorPresentationEnabled(true);
            w->resize(600, 300);
            const QImage img = w->grab().toImage();
            const QRect plot = w->plotRect().toAlignedRect().adjusted(2, 2, -2, -2);
            QCOMPARE(commonColour(img, plot), role("app-bg"));
            QCOMPARE(commonColour(img, QRect(0, 0, 8, 8)), role("app-bg"));
            const QColor user(QStringLiteral("#2a1e14"));
            host.setProperty(WorkspacePreview::kUserBackgroundProperty, user);
            const QImage userImg = w->grab().toImage();
            QCOMPARE(commonColour(userImg, plot), user);
        }
        QWidget host;
        auto* w = new ParametricEqWidget(&host);
        w->setEditorPresentationEnabled(true);
        w->resize(600, 300);
        const QImage img = w->grab().toImage();
        const QRect plot = w->plotRect().toAlignedRect().adjusted(2, 2, -2, -2);
        QCOMPARE(commonColour(img, plot), QColor(QStringLiteral("#0a0a18")));
        QCOMPARE(commonColour(img, QRect(0, 0, 8, 8)), QColor(QStringLiteral("#0f0f1a")));
    }

    // D22: the amp view chart takes its container's background; today's
    // Thetis black with the switch off.
    void ampViewChartFollowsTheContainer()
    {
        {
            WorkspaceApp look;
            AmpViewChart chart;
            chart.resize(500, 300);
            const QImage img = chart.grab().toImage();
            QCOMPARE(commonColour(img, img.rect()), role("app-bg"));
        }
        AmpViewChart chart;
        chart.resize(500, 300);
        const QImage img = chart.grab().toImage();
        QCOMPARE(commonColour(img, img.rect()), QColor(Qt::black));
    }

    // The Longpath reference column draws Longpath's own spin arrows
    // (#c8d8e8, Longpath resources/icons/spin-up.svg), not the workspace
    // variants, also while the preview is on.
    void referenceColumnKeepsLongpathsArrows()
    {
        WorkspaceApp look;
        const QString sheet = QStringLiteral(
            "QSpinBox::up-arrow { image: url(:/icons/spin-up.svg); }"
            "QSpinBox::down-arrow { image: url(:/icons/spin-down.svg); }");
        const QString out = Preview::LongpathLook::themed(sheet);
        QVERIFY2(out.contains(QStringLiteral(":/icons/spin-up.svg")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral(":/icons/spin-down.svg")), qPrintable(out));
        QVERIFY2(!out.contains(QStringLiteral("workspace")), qPrintable(out));
        QFile f(QStringLiteral(":/icons/spin-up.svg"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(f.readAll()).contains(QStringLiteral("fill=\"#c8d8e8\"")));
    }

    // D6 + D10: the overlay's step attenuator check box, checked, shows the
    // decided checked indicator (selected-border rim) at today's size.
    void attCheckBoxTakesTheDecidedCheckedState()
    {
        const auto indicatorOf = [](QCheckBox* check) {
            QStyleOptionButton option;
            option.initFrom(check);
            return check->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, check);
        };
        QSize todaySize;
        {
            QWidget host;
            host.resize(600, 400);
            new SpectrumOverlayPanel(&host);
            auto* check = host.findChild<QCheckBox*>(QStringLiteral("attEnableCheck"));
            QVERIFY(check != nullptr);
            check->setEnabled(true);
            check->setChecked(true);
            check->parentWidget()->layout()->activate();
            todaySize = check->sizeHint();
        }
        WorkspaceApp look;
        QWidget host;
        host.resize(600, 400);
        new SpectrumOverlayPanel(&host);
        auto* check = host.findChild<QCheckBox*>(QStringLiteral("attEnableCheck"));
        QVERIFY(check != nullptr);
        // Offline the panel disables the box (no step attenuator); the
        // scene shows it enabled.
        check->setEnabled(true);
        check->setChecked(true);
        QWidget* flyout = check->parentWidget();
        WorkspacePreview::themeSubtree(flyout);
        flyout->layout()->activate();
        QCOMPARE(check->sizeHint(), todaySize);
        const QRect ind = indicatorOf(check);
        QVERIFY(!ind.isEmpty());
        const QImage img = check->grab().toImage();
        const QColor rim = img.pixelColor(ind.left(), ind.center().y());
        QVERIFY2(rim == role("selected-border"), qPrintable(rim.name()));
        const QColor fill = img.pixelColor(ind.center());
        QVERIFY2(fill.blue() > fill.red() + 40, qPrintable(fill.name()));
    }

    // With the switch off: the speaker and zoom sheets come back as they
    // were, and the plots keep today's colours (above).
    void switchOffKeepsTodaysLook()
    {
        QWidget host;
        ProbeSlider* pc = speakerSlider(host, HeaderVolumeStyle::kPcSlider, true);
        auto* zoom = new QSlider(Qt::Horizontal, &host);
        zoom->setProperty(WorkspacePreview::kVisibleTrackProperty, true);
        zoom->setStyleSheet(QLatin1String(kZoomBarSheet));
        WorkspacePreview::themeSubtree(&host);
        QCOMPARE(pc->styleSheet(), QString::fromLatin1(HeaderVolumeStyle::kPcSlider));
        QCOMPARE(zoom->styleSheet(), QString::fromLatin1(kZoomBarSheet));
        QCOMPARE(WorkspacePreview::lookQss(QString::fromLatin1(HeaderVolumeStyle::kRadioSlider), pc),
                 QString::fromLatin1(HeaderVolumeStyle::kRadioSlider));
        const QColor fill = leftOfHandle(pc);
        QVERIFY2(fill == QColor(QStringLiteral("#00b4d8")), qPrintable(fill.name()));
    }

    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
    }
};

QTEST_MAIN(TestWorkspaceSlidersPlots)
#include "tst_workspace_sliders_plots.moc"
