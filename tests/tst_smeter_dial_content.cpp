// no-port-check: NereusSDR-original checks for added copies of the SIGNAL
// STRENGTH dial as container content (workspace preview).
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QLabel>
#include <QSplitter>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "gui/SMeterWidget.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/instruments/SMeterDialContent.h"
#include "gui/meters/MeterPoller.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// The dials a host built for added copies (never the Main Panel's).
QList<SMeterWidget*> dialCopies(const QWidget& host)
{
    QList<SMeterWidget*> out;
    for (SMeterWidget* w : host.findChildren<SMeterWidget*>()) {
        if (w->settingsStore() == SMeterWidget::SettingsStore::Instance) { out.append(w); }
    }
    return out;
}

SMeterWidget* dialFor(const QWidget& host, const QString& entryId)
{
    for (SMeterWidget* w : dialCopies(host)) {
        if (w->property("containerEntryId").toString() == entryId) { return w; }
    }
    return nullptr;
}

bool showsDuplicateLabel(const QWidget& host)
{
    for (const QLabel* l : host.findChildren<QLabel*>()) {
        if (l->text().contains(QLatin1String("duplicate"))) { return true; }
    }
    return false;
}

// The Main Panel's dial keys, as AppSettings holds them now.
QStringList globalDialKeys()
{
    QStringList out;
    for (const char* key : {"SMeter_TxSelect", "SMeter_RxSelect", "PeakHoldEnabled", "PeakDecayRate",
                            "SMeter_FaceStyle"}) {
        const QString k = QString::fromLatin1(key);
        out.append(k + QLatin1Char('=')
                   + (AppSettings::instance().contains(k) ? AppSettings::instance().value(k).toString()
                                                          : QStringLiteral("<absent>")));
    }
    return out;
}

ContainerDocument stationContainer(const QVector<ContentEntry>& contents)
{
    ContainerDocument c;
    c.id = QStringLiteral("station");
    c.name = QStringLiteral("Station");
    c.layout = ContentLayout::VerticalStack;
    c.dockMode = DockMode::OverlayDocked;
    c.geometry = QRect(10, 10, 380, 800);
    c.contents = contents;
    return c;
}

} // namespace

class TstSMeterDialContent : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void dialIsListedAsAnAdditionalType()
    {
        ContainerContentRegistry registry;
        SMeterDialContent::registerDial(registry, {});
        bool found = false;
        for (const auto& d : registry.descriptors()) {
            if (d.typeId == QLatin1String(SMeterDialContent::kDialType)) {
                found = true;
                QVERIFY(!d.singleton);
                QVERIFY(d.available);
                QCOMPARE(d.title, QStringLiteral("S-Meter dial (additional)"));
            }
        }
        QVERIFY(found);
        // A new copy starts from today's defaults.
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        QVERIFY(registry.validateEntry(entry).isEmpty());
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* dial = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(dial);
        QCOMPARE(dial->settingsStore(), SMeterWidget::SettingsStore::Instance);
        QCOMPARE(dial->txMode(), SMeterWidget::TxMode::Power);
        QCOMPARE(dial->rxMode(), SMeterWidget::RxMode::SMeter);
        QVERIFY(dial->peakHoldEnabled());
        QCOMPARE(dial->peakDecayRate(), SMeterWidget::DecayRate::Medium);
        QCOMPARE(dial->faceStyle(), SMeterWidget::FaceStyle::AgedCream);
        // No reading yet: "--", as a Longpath needle is offline.
        QCOMPARE(dial->sUnitsText(), QStringLiteral("--"));
    }

    void twoCopiesAreBothAlive()
    {
        MeterPoller poller;
        ContainerContentRegistry registry;
        SMeterWidget mainPanelDial;
        registry.attachSingleton(QStringLiteral("applet:s_meter"), &mainPanelDial);
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);

        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-two.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContentEntry a = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        ContentEntry b = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        a.id = QStringLiteral("dial-a");
        b.id = QStringLiteral("dial-b");
        WorkspaceDocument document;
        document.containers.append(stationContainer({a, b}));
        document.mainContainerId = QStringLiteral("station");
        const auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        QCOMPARE(dialCopies(*host).size(), 2);
        SMeterWidget* first = dialFor(*host, a.id);
        SMeterWidget* second = dialFor(*host, b.id);
        QVERIFY(first && second && first != second);
        QVERIFY(first->isVisible() && second->isVisible());
        QVERIFY(first->height() >= 130 && second->height() >= 130);
        QVERIFY(!showsDuplicateLabel(*host));
        // The Main Panel's dial stays where it was.
        QVERIFY(!host->isAncestorOf(&mainPanelDial));
        QCOMPARE(poller.readingConsumerCountForTest(), 2);
    }

    void eachCopyFollowsItsOwnSource()
    {
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);

        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-source.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContentEntry a = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        ContentEntry b = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        a.id = QStringLiteral("dial-rx-a");
        b.id = QStringLiteral("dial-rx-b");
        a.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
        b.context = QJsonObject{{QStringLiteral("rxSource"), 2}};
        WorkspaceDocument document;
        document.containers.append(stationContainer({a, b}));
        document.mainContainerId = QStringLiteral("station");
        auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        SMeterWidget* dialA = dialFor(*host, a.id);
        SMeterWidget* dialB = dialFor(*host, b.id);
        QVERIFY(dialA && dialB);
        const QJsonObject contextA = dialA->property("containerSourceContext").toJsonObject();
        const QJsonObject contextB = dialB->property("containerSourceContext").toJsonObject();
        QVERIFY(contextA != contextB);

        emit poller.readingUpdated(contextA, MeterBinding::SignalPeak, -63.0);
        emit poller.readingUpdated(contextB, MeterBinding::SignalPeak, -103.0);
        QCOMPARE(dialA->levelDbm(), -63.0f);
        QCOMPARE(dialB->levelDbm(), -103.0f);
        // A reading for a binding the RX mode does not read changes nothing.
        emit poller.readingUpdated(contextA, MeterBinding::SignalAvg, -20.0);
        QCOMPARE(dialA->levelDbm(), -63.0f);

        // The receiver behind A changed: A drops its value, B keeps its own.
        emit poller.rxSourceChanged(contextA);
        QCOMPARE(dialA->sUnitsText(), QStringLiteral("--"));
        QCOMPARE(dialB->levelDbm(), -103.0f);
        // A's receiver closed: no reading, "--".
        emit poller.readingUpdated(contextA, MeterBinding::SignalPeak, kNoMeterReadingDbm);
        QCOMPARE(dialA->sUnitsText(), QStringLiteral("--"));

        // Changing A's source moves it: B's context now reaches A.
        document.containers[0].contents[0].context = QJsonObject{{QStringLiteral("rxSource"), 2}};
        committed = manager.commitWorkspace(document, committed.revision);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        settle();
        host = manager.contentHost(QStringLiteral("station"));
        dialA = dialFor(*host, a.id);
        QVERIFY(dialA);
        QCOMPARE(dialA->property("containerSourceContext").toJsonObject(), contextB);
        emit poller.readingUpdated(contextB, MeterBinding::SignalPeak, -71.0);
        QCOMPARE(dialA->levelDbm(), -71.0f);
    }

    void settingsLiveInTheEntryNotTheGlobalKeys()
    {
        const QStringList globalBefore = globalDialKeys();
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);

        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-settings.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContentEntry a = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        ContentEntry b = registry.makeEntry(QString::fromLatin1(SMeterDialContent::kDialType));
        a.id = QStringLiteral("dial-edit");
        b.id = QStringLiteral("dial-keep");
        WorkspaceDocument document;
        document.containers.append(stationContainer({a, b}));
        document.mainContainerId = QStringLiteral("station");
        const auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        SMeterWidget* dial = dialFor(*host, a.id);
        QVERIFY(dial);
        // One poll fills the poller's cache for the dial's context: Signal
        // Peak -60 dBm (shown) and Sig Avg -88 dBm (cached).
        poller.setRxReadingSource([](const QJsonObject&, int binding) {
            return binding == MeterBinding::SignalAvg ? -88.0 : -60.0;
        });
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(dial->levelDbm(), -60.0f);

        // The right-click menu's setters.
        dial->setTxMode(QStringLiteral("SWR"));
        dial->setRxMode(QStringLiteral("Sig Avg"));
        dial->setPeakHoldEnabled(false);
        dial->setPeakDecayRate(QStringLiteral("Slow"));
        dial->setFaceStyle(SMeterWidget::FaceStyle::VuAmber);
        settle();
        // The new RX mode shows its binding's cached reading at once.
        QCOMPARE(dial->levelDbm(), -88.0f);

        const auto findEntry = [](const WorkspaceDocument& d, const QString& id) {
            for (const auto& e : d.containers.first().contents) {
                if (e.id == id) { return e; }
            }
            return ContentEntry{};
        };
        const WorkspaceDocument saved = store.snapshot();
        const QJsonObject config = findEntry(saved, a.id).config;
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kTxModeKey)).toString(), QStringLiteral("SWR"));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kRxModeKey)).toString(), QStringLiteral("Sig Avg"));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kPeakHoldKey)), QJsonValue(false));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kPeakDecayKey)).toString(), QStringLiteral("Slow"));
        QCOMPARE(config.value(QLatin1String(SMeterDialContent::kFaceKey)).toString(),
                 SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::VuAmber));
        // The other copy and the Main Panel's keys are untouched.
        QVERIFY(findEntry(saved, b.id).config.isEmpty());
        QCOMPARE(globalDialKeys(), globalBefore);
        // The edit did not rebuild the dial it came from.
        QCOMPARE(dialFor(*host, a.id), dial);

        // Reloaded from the stored document, a copy comes back as it was left.
        ContainerWorkspaceStore reloadStore{settings};
        const DocumentResult loaded = reloadStore.load();
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        const ContentEntry reloaded = findEntry(loaded.document, a.id);
        QCOMPARE(reloaded.config, config);
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(reloaded, &parent));
        auto* again = qobject_cast<SMeterWidget*>(widget.get());
        QVERIFY(again);
        QCOMPARE(again->txMode(), SMeterWidget::TxMode::SWR);
        QCOMPARE(again->rxMode(), SMeterWidget::RxMode::SignalAverage);
        QVERIFY(!again->peakHoldEnabled());
        QCOMPARE(again->peakDecayRate(), SMeterWidget::DecayRate::Slow);
        QCOMPARE(again->faceStyle(), SMeterWidget::FaceStyle::VuAmber);
        QCOMPARE(globalDialKeys(), globalBefore);
    }

    void savedSingletonWorkspaceLoadsUnchanged()
    {
        // A workspace saved before the dial was addable: the Main Panel's
        // dial (applet:s_meter) in its container.
        QTemporaryDir directory;
        AppSettings settings{directory.filePath("dial-legacy.xml")};
        WorkspaceDocument original;
        {
            ContainerContentRegistry before;
            SMeterWidget unused;
            before.attachSingleton(QStringLiteral("applet:s_meter"), &unused);
            ContainerWorkspaceStore store{settings};
            ContentEntry singleton = before.makeEntry(QStringLiteral("applet:s_meter"));
            singleton.id = QStringLiteral("main-s-meter");
            original.containers.append(stationContainer({singleton}));
            original.mainContainerId = QStringLiteral("station");
            const auto saved = store.commit(original, 0);
            QVERIFY2(saved.status == CommitStatus::Saved, qPrintable(saved.error));
            original = store.snapshot();
        }
        ContainerWorkspaceStore probe{settings};
        const QString key = probe.workspaceKey();
        const QString rawBefore = settings.value(key).toString();
        QVERIFY(!rawBefore.isEmpty());

        // Loaded with the dial registered.
        MeterPoller poller;
        ContainerContentRegistry registry;
        SMeterWidget mainPanelDial;
        registry.attachSingleton(QStringLiteral("applet:s_meter"), &mainPanelDial);
        LongpathContent::Sources sources;
        sources.poller = &poller;
        SMeterDialContent::registerDial(registry, sources);
        ContainerWorkspaceStore store{settings};
        const DocumentResult loaded = store.load();
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        QCOMPARE(loaded.document, original);
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        manager.reconcileWorkspace(loaded.document);
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(QStringLiteral("station"));
        QVERIFY(host);
        QVERIFY(host->isAncestorOf(&mainPanelDial));
        QCOMPARE(mainPanelDial.settingsStore(), SMeterWidget::SettingsStore::Global);
        QVERIFY(dialCopies(*host).isEmpty());
        QVERIFY(!showsDuplicateLabel(*host));
        QCOMPARE(store.snapshot(), original);
        QCOMPARE(settings.value(key).toString(), rawBefore);
    }
};

QTEST_MAIN(TstSMeterDialContent)
#include "tst_smeter_dial_content.moc"
