// no-port-check: NereusSDR-original checks for the ported Longpath
// instruments and their container-content registration (workspace preview).
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QLayout>
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
#include "gui/instruments/LongpathContent.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/BandwidthFilterPane.h"

using namespace NereusSDR;

namespace {

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
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
