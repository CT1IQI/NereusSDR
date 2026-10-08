// no-port-check: NereusSDR-original checks for the ported Longpath
// instruments and their container-content registration (workspace preview).
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - frequencyRowUsesSliceLetters. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QSplitter>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "core/antenna/AmateurBands.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/LongpathContent.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// The rich text of the instrument's VFO row (the only label with two
// <font> runs).
QString vfoRowText(const QWidget& w)
{
    for (const QLabel* l : w.findChildren<QLabel*>()) {
        if (l->text().count(QStringLiteral("<font")) == 2) { return l->text(); }
    }
    return {};
}

} // namespace

class TstLongpathInstruments : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void frequencyInstrumentSizesAtDefaultForm()
    {
        FrequencyInstrument w;
        QCOMPARE(w.form(), FrequencyInstrument::Form::NumberOnly);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        settle();
        QVERIFY(w.vfoRowVisible());
        // Longpath FrequencyInstrument.cpp:194-216: {max(240, digit row at
        // 12 px + 28), 74} with the A/B row shown. At 12 px the digit row
        // is well under 212 px, so the documented floor holds.
        QCOMPARE(w.minimumSizeHint(), QSize(240, 74));
        // FrequencyInstrument.cpp:218-253: {max(360, layout), max(40, layout)}.
        const QSize fromLayout = w.layout()->sizeHint();
        QCOMPARE(w.sizeHint(), QSize(qMax(360, fromLayout.width()), qMax(40, fromLayout.height())));
        QCOMPARE(w.sizeHint().width(), 360);
        // Hiding the A/B row gives its height back to the minimum.
        w.setVfoRowVisible(false);
        QVERIFY(w.minimumSizeHint().height() < 74);
        QVERIFY(w.minimumSizeHint().height() >= 40);
    }

    void stripFormsReserveTheirStripAtMinimum()
    {
        // NereusSDR fix: the BandStrip / FlatArc reservation is part of the
        // minimum height, so the digit row is not squeezed at the minimum.
        FrequencyInstrument number;
        FrequencyInstrument strip;
        strip.setForm(FrequencyInstrument::Form::BandStrip);
        FrequencyInstrument arc;
        arc.setForm(FrequencyInstrument::Form::FlatArc);
        for (FrequencyInstrument* w : {&number, &strip, &arc}) {
            w->show();
            QVERIFY(QTest::qWaitForWindowExposed(w));
        }
        settle();
        QVERIFY(strip.minimumSizeHint().height() > number.minimumSizeHint().height());
        QVERIFY(arc.minimumSizeHint().height() > strip.minimumSizeHint().height());
        QVERIFY(strip.minimumSizeHint().height() <= strip.sizeHint().height());
        QVERIFY(arc.minimumSizeHint().height() <= arc.sizeHint().height());
    }

    void barFooterOnlyWhenItFits()
    {
        // NereusSDR fix: the footer shows at the preferred size and is
        // hidden at the minimum instead of covering the trough.
        BarInstrument bar;
        bar.setPrimary(MeterBinding::TxSwr);
        bar.onReading(MeterBinding::TxSwr, 1.25);
        bar.resize(bar.sizeHint());
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        settle();
        auto* footer = bar.findChild<InstrumentFooter*>();
        QVERIFY(footer);
        QVERIFY(footer->isVisible());
        QVERIFY(bar.sizeHint().height() >= 74);
        bar.resize(bar.minimumSizeHint());
        settle();
        QVERIFY(!footer->isVisible());
        bar.resize(bar.sizeHint());
        settle();
        QVERIFY(footer->isVisible());
    }

    void frequencyGroupingAndParsing()
    {
        FrequencyInstrument w;
        w.setFrequency(14225000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("14.225.000"));
        w.setFrequency(7185000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("7.185.000"));
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.230.000")), 7230000.0);
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7230")), 7230000.0);
        QCOMPARE(AmateurBands::containing(14225000.0).name, QStringLiteral("20 m"));
    }

    void bandwidthFilterPaneSizes()
    {
        BandwidthFilterPane pane;
        QCOMPARE(pane.minimumSize(), QSize(180, 84));
        QCOMPARE(pane.sizeHint(), QSize(360, 140));
    }

    void needleWithoutReadingIsOffline()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* needle = qobject_cast<NeedleInstrument*>(widget.get());
        QVERIFY(needle);
        QCOMPARE(needle->primary(), int(MeterBinding::SignalPeak));
        QVERIFY(!needle->hasValue());
        QVERIFY(needle->isOffline());
    }

    void readingsWithoutProducerAreUnavailable()
    {
        // NoiseFloor (9) and RadeSnr (10) keep their descriptors but have no
        // NereusSDR producer.
        for (int id : {9, 10}) {
            const ReadingDescriptor* d = readingFor(id);
            QVERIFY(d);
            QVERIFY(d->hasScale);
            QVERIFY(!d->available);
        }
        QVERIFY(readingFor(MeterBinding::SignalPeak)->available);
        QVERIFY(readingFor(MeterBinding::TxSwr)->available);

        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        entry.config.insert(QString::fromLatin1(LongpathContent::kBindingKey), 9);
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* needle = qobject_cast<NeedleInstrument*>(widget.get());
        QVERIFY(needle);
        QVERIFY(needle->isOffline());
    }

    void txAlcHasThetisScale()
    {
        // NereusSDR addition: Thetis's ALC bar calibration (MeterManager.cs
        // AddALCBar): -30 dB at 0, 0 dB at 0.665, +12 dB at 0.99, red from 0.
        const ReadingDescriptor* d = readingFor(MeterBinding::TxAlc);
        QVERIFY(d);
        QVERIFY(d->hasScale);
        QVERIFY(d->available);
        QCOMPARE(d->unit, QStringLiteral("dB"));
        QCOMPARE(d->min, -30.0);
        QCOMPARE(d->max, 12.0);
        QVERIFY(d->threshold.has_value());
        QCOMPARE(*d->threshold, 0.0);
        QVERIFY(qAbs(d->fraction(-30.0)) < 1e-9);
        QVERIFY(qAbs(d->fraction(0.0) - 0.665) < 1e-9);
        QVERIFY(qAbs(d->fraction(12.0) - 0.99) < 1e-9);
        QVERIFY(qAbs(d->fraction(-15.0) - 0.3325) < 1e-9);
        QCOMPARE(d->ticks.size(), 6);
        QCOMPARE(d->minorTicks.size(), 6);

        BarInstrument bar;
        QVERIFY(bar.setPrimary(MeterBinding::TxAlc));
        bar.onReading(MeterBinding::TxAlc, -3.0);
        QVERIFY(bar.hasValue());
    }

    void registryRoundTripCreatesTwoNeedles()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        QStringList names;
        for (const auto& d : registry.descriptors()) {
            if (d.typeId.startsWith(QLatin1String("longpath."))) {
                QVERIFY(!d.singleton);
                QVERIFY(d.available);
                names.append(d.title);
            }
        }
        QCOMPARE(names, (QStringList{QStringLiteral("Frequency (Longpath)"), QStringLiteral("Needle meter (Longpath)"),
                                     QStringLiteral("Bar meter (Longpath)"), QStringLiteral("Filter passband (Longpath)")}));

        ContentEntry rx = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        ContentEntry tx = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        QCOMPARE(rx.name, QStringLiteral("Needle meter (Longpath)"));
        QVERIFY(rx.id != tx.id);
        QVERIFY(registry.validateEntry(rx).isEmpty());
        tx.config.insert(QString::fromLatin1(LongpathContent::kDirectionKey), QStringLiteral("tx"));

        QWidget parent;
        std::unique_ptr<QWidget> a(registry.createWidget(rx, &parent));
        std::unique_ptr<QWidget> b(registry.createWidget(tx, &parent));
        auto* first = qobject_cast<NeedleInstrument*>(a.get());
        auto* second = qobject_cast<NeedleInstrument*>(b.get());
        QVERIFY(first && second && first != second);
        QCOMPARE(first->primary(), int(MeterBinding::SignalPeak));
        QCOMPARE(second->primary(), int(MeterBinding::TxPower));
        QCOMPARE(a->property("containerEntryId").toString(), rx.id);
        QCOMPARE(b->property("containerEntryId").toString(), tx.id);

        // Through a container host: one widget per entry, never shared.
        QTemporaryDir directory;
        AppSettings settings{directory.filePath("longpath.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContainerDocument c;
        c.id = QStringLiteral("station");
        c.name = QStringLiteral("Station");
        c.layout = ContentLayout::VerticalStack;
        c.dockMode = DockMode::OverlayDocked;
        c.geometry = QRect(10, 10, 380, 700);
        rx.id = QStringLiteral("station-rx");
        tx.id = QStringLiteral("station-tx");
        c.contents = {rx, tx};
        WorkspaceDocument document;
        document.containers.append(c);
        document.mainContainerId = c.id;
        const auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(c.id);
        QVERIFY(host);
        const auto needles = host->findChildren<NeedleInstrument*>();
        QCOMPARE(needles.size(), 2);
        QSet<int> primaries;
        for (auto* n : needles) { primaries.insert(n->primary()); }
        QCOMPARE(primaries, (QSet<int>{MeterBinding::SignalPeak, MeterBinding::TxPower}));
    }

    void frequencyRowUsesSliceLetters()
    {
        // Upstream defaults: A for this instrument, B for the other.
        FrequencyInstrument plain;
        QCOMPARE(plain.ownLetter(), QLatin1Char('A'));
        QCOMPARE(plain.otherLetter(), QLatin1Char('B'));

        // No radio here: a slice's frequency change warns that the TX
        // frequency is not pushed. Expected, so kept out of the output.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false"));
        const auto restoreRules = qScopeGuard([] { QLoggingCategory::setFilterRules(QString()); });
        RadioModel model;
        model.addSlice();
        model.addSlice();
        QCOMPARE(model.slices().size(), 2);
        SliceModel* sliceA = model.slices().at(0);
        SliceModel* sliceB = model.slices().at(1);
        QCOMPARE(sliceA->sliceLetter(), QLatin1Char('A'));
        QCOMPARE(sliceB->sliceLetter(), QLatin1Char('B'));
        sliceA->setFrequency(7185000.0);
        sliceB->setFrequency(14225000.0);

        SliceModel* target = sliceB;
        LongpathContent::Sources sources;
        sources.radio = &model;
        sources.sliceForContext = [&target](const QJsonObject&) { return target; };
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, sources);
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyType));

        QWidget parent;
        std::unique_ptr<QWidget> onB(registry.createWidget(entry, &parent));
        auto* freqB = qobject_cast<FrequencyInstrument*>(onB.get());
        QVERIFY(freqB);
        QCOMPARE(freqB->ownLetter(), QLatin1Char('B'));
        QCOMPARE(freqB->otherLetter(), QLatin1Char('A'));
        const QString rowB = vfoRowText(*freqB);
        QVERIFY2(rowB.contains(QStringLiteral(">B 14.225.000<")), qPrintable(rowB));
        QVERIFY2(rowB.contains(QStringLiteral(">A 7.185.000<")), qPrintable(rowB));
        QVERIFY(rowB.indexOf(QStringLiteral(">B ")) < rowB.indexOf(QStringLiteral(">A ")));

        target = sliceA;
        std::unique_ptr<QWidget> onA(registry.createWidget(entry, &parent));
        auto* freqA = qobject_cast<FrequencyInstrument*>(onA.get());
        QVERIFY(freqA);
        const QString rowA = vfoRowText(*freqA);
        QVERIFY2(rowA.contains(QStringLiteral(">A 7.185.000<")), qPrintable(rowA));
        QVERIFY2(rowA.contains(QStringLiteral(">B 14.225.000<")), qPrintable(rowA));
        QVERIFY(rowA.indexOf(QStringLiteral(">A ")) < rowA.indexOf(QStringLiteral(">B ")));
    }

    void barDefaultsAndReadings()
    {
        ContainerContentRegistry registry;
        MeterPoller poller;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kBarType));
        entry.context.insert(QStringLiteral("sliceId"), 0);
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* bar = qobject_cast<BarInstrument*>(widget.get());
        QVERIFY(bar);
        QVERIFY(bar->isCompact());
        QCOMPARE(bar->primary(), int(MeterBinding::TxSwr));
        QVERIFY(!bar->hasValue());
        // Another context's reading is not this bar's.
        emit poller.readingUpdated(QJsonObject{{"sliceId", 1}}, MeterBinding::TxSwr, 1.25);
        QVERIFY(!bar->hasValue());
        emit poller.readingUpdated(entry.context, MeterBinding::TxSwr, 1.25);
        QVERIFY(bar->hasValue());
        emit poller.readingUpdated(entry.context, MeterBinding::TxSwr, kNoMeterReadingDbm);
        QVERIFY(!bar->hasValue());
    }

    void traceFromRenderedTakesBucketMaxima()
    {
        // 100 pixels over 10 kHz centred on 14.200 MHz: 100 Hz per pixel.
        QVector<float> pixels(100, -120.0f);
        pixels[55] = -60.0f;   // 14.200500 .. 14.200600 MHz
        const auto trace = LongpathContent::traceFromRendered(pixels, 14200000.0, 10000.0,
                                                              14200000.0, 14201000.0, 5);
        QCOMPARE(trace.size(), 5);
        QCOMPARE(trace.at(2), -60.0f);
        QCOMPARE(trace.at(0), -120.0f);
        // Off the pan: no trace rather than a partial one.
        QVERIFY(LongpathContent::traceFromRendered(pixels, 14200000.0, 10000.0,
                                                   14190000.0, 14201000.0, 5).isEmpty());
    }
};

QTEST_MAIN(TstLongpathInstruments)
#include "tst_longpath_instruments.moc"
