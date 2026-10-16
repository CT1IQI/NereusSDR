// =================================================================
// tests/tst_daemon_display_send_thread.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. Display send thread.
//
// The Core's displays leave on a thread of their own. When the desktop app
// hosts the Core its main thread also draws the window, and Direct3D can
// hold that thread for most of a second per frame on a locked Windows
// session; the displays must keep going through such a wait.
//
// The Core is the real DaemonMediaController behind a real station
// session. A feeder thread hands the radio's I/Q to RadioModel as the radio
// connection's thread does, the FFT engines make frames on their own
// threads, and the transport records every display message it is given:
// when, and on which thread. With frames flowing the test holds the main
// thread for 500 ms. Every display message must have been sent off the
// main thread, and none of the gaps between them during the hold may reach
// 50 ms. Two pans on two receivers at 60 frames a second each, so a frame
// one pan's own pacing drops never shows as a gap.
//
// A frame the display thread leaves to the main thread and a retune made
// before the main thread gets to it: no context goes out for the center the
// receiver has left.
//
// Modification history (NereusSDR):
//   2026-10-15  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest>

#include <QElapsedTimer>
#include <QJsonObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include "RealtimeTestLoad.h"
#include "core/AppSettings.h"
#include "core/FFTEngine.h"
#include "core/HpsdrModel.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DaemonSpectrumSource.h"
#include "core/session/media/IMediaTransport.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
constexpr quint32 kFirstEndpoint = 7;
constexpr quint32 kSecondEndpoint = 8;
constexpr int kDisplayFps = 60;
constexpr int kHoldMs = 500;
constexpr qint64 kMaxGapNs = 50'000'000;
constexpr int kSendsBeforeHold = 60;

qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Every display message the transport was given, from any thread.
struct SendLog {
    struct Send {
        std::thread::id thread;
        qint64 atNs = 0;
    };
    mutable std::mutex mutex;
    std::vector<Send> sends;

    void record()
    {
        const std::lock_guard<std::mutex> lock(mutex);
        sends.push_back({std::this_thread::get_id(), nowNs()});
    }
    std::vector<Send> copy() const
    {
        const std::lock_guard<std::mutex> lock(mutex);
        return sends;
    }
};

// The transport's display send for another thread: it records and takes
// every message while the transport is ready.
class RecordingDisplaySink final : public MediaDisplaySink {
public:
    explicit RecordingDisplaySink(std::shared_ptr<SendLog> log) : m_log(std::move(log)) {}

    bool ready() const override { return readyState.load(std::memory_order_acquire); }
    IMediaTransport::DisplaySendResult submitDisplay(const QByteArray& message) override
    {
        if (!ready() || message.isEmpty()) {
            return IMediaTransport::DisplaySendResult::Refused;
        }
        m_log->record();
        return IMediaTransport::DisplaySendResult::Sent;
    }
    bool displayBusy() const override { return false; }
    IMediaTransport::DisplaySendResult submitIq(const QByteArray&) override
    {
        return IMediaTransport::DisplaySendResult::Refused;
    }
    bool iqBusy() const override { return false; }

    std::atomic<bool> readyState{false};

private:
    std::shared_ptr<SendLog> m_log;
};

// A transport that gives a display sink, as the Core's own does. A display
// message handed to it on its owner's thread is recorded too, with that
// thread.
class SinkTransport final : public IMediaTransport {
public:
    SinkTransport(std::shared_ptr<SendLog> log, QObject* parent)
        : IMediaTransport(parent)
        , m_log(log)
        , m_sink(std::make_shared<RecordingDisplaySink>(log))
    {
    }
    bool start(const StartOptions&) override { return true; }
    void stop() override { m_sink->readyState.store(false, std::memory_order_release); }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& bytes) override
    {
        if (!isReady() || bytes.isEmpty()) { return false; }
        m_log->record();
        return true;
    }
    bool sendRtp(const QByteArray&) override { return isReady(); }
    bool isReady() const override { return m_sink->ready(); }
    std::shared_ptr<MediaDisplaySink> displaySink() override { return m_sink; }
    void becomeReady()
    {
        m_sink->readyState.store(true, std::memory_order_release);
        emit ready();
    }

private:
    std::shared_ptr<SendLog> m_log;
    std::shared_ptr<RecordingDisplaySink> m_sink;
};

QJsonObject plane()
{
    return {{QStringLiteral("detector"), 0},
            {QStringLiteral("averageMode"), -1},
            {QStringLiteral("averageAlpha"), 0.0}};
}

QJsonObject subscription(quint32 endpointId, int sliceId, double centerHz)
{
    return {{QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
            {QStringLiteral("revision"), 1},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), 1024},
            {QStringLiteral("windowType"), static_cast<int>(WindowFunction::Hann)},
            {QStringLiteral("centreHz"), centerHz},
            {QStringLiteral("spanHz"), 48000.0},
            {QStringLiteral("pixels"), 128},
            {QStringLiteral("fps"), kDisplayFps},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("trace"), plane()},
            {QStringLiteral("waterfall"), plane()},
            {QStringLiteral("minDbm"), -180.0},
            {QStringLiteral("maxDbm"), 0.0},
            {QStringLiteral("wideSpanFactor"), 0.0}};
}

// Hands each stream's I/Q to RadioModel from a thread of its own, at the
// stream's sample rate on the wall clock, as the radio connection's thread
// does in the Core.
class Feeder {
public:
    Feeder(RadioModel* radio, QList<int> streams, int sampleRateHz)
        : m_radio(radio), m_streams(std::move(streams)), m_sampleRateHz(sampleRateHz)
    {
    }
    ~Feeder() { stop(); }

    void start()
    {
        m_thread = std::thread([this] { run(); });
    }
    void stop()
    {
        m_stop.store(true, std::memory_order_release);
        if (m_thread.joinable()) { m_thread.join(); }
    }

private:
    void run()
    {
        constexpr int kChunk = 1024;
        std::minstd_rand noise{20261015U};
        std::uniform_real_distribution<float> sample(-1.0e-3f, 1.0e-3f);
        QVector<float> iq(kChunk * 2);
        const qint64 startNs = nowNs();
        qint64 fed = 0;
        while (!m_stop.load(std::memory_order_acquire)) {
            const qint64 due = (nowNs() - startNs) * m_sampleRateHz / 1'000'000'000LL;
            while (fed < due && !m_stop.load(std::memory_order_acquire)) {
                for (float& value : iq) { value = sample(noise); }
                for (int stream : m_streams) {
                    QMetaObject::invokeMethod(m_radio, "rawIqDataForStream",
                                              Qt::DirectConnection, Q_ARG(int, stream),
                                              Q_ARG(QVector<float>, iq));
                }
                fed += kChunk;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    RadioModel* m_radio;
    QList<int> m_streams;
    qint64 m_sampleRateHz;
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
};

struct Harness {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    std::shared_ptr<SendLog> log = std::make_shared<SendLog>();
    QPointer<SinkTransport> mediaTransport;
    DaemonMediaController controller;
    int firstSlice{-1};
    int secondSlice{-1};

    Harness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
        , controller(&server, &radio, nullptr, [this](QObject* parent) -> IMediaTransport* {
            mediaTransport = new SinkTransport(log, parent);
            return mediaTransport;
        })
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        // One pan each, so each slice has a receiver stream of its own.
        firstSlice = radio.addSlice(QStringLiteral("pan-0"));
        secondSlice = radio.addSlice(QStringLiteral("pan-1"));
        server.setMediaEnabled(true);
    }

    int streamOf(int sliceId)
    {
        const SliceModel* const slice = radio.sliceById(sliceId);
        return slice ? slice->streamIndex() : -1;
    }

    bool start()
    {
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        if (!QTest::qWaitFor([this] {
                return server.mediaAvailable() && client.mediaAvailable();
            }, 10'000)) {
            return false;
        }
        if (!client.sendMediaControl({{QStringLiteral("op"), QStringLiteral("start")},
                                      {QStringLiteral("connectionId"),
                                       QLatin1String(kConnectionId)}},
                                     client.sessionEpoch())) {
            return false;
        }
        if (!QTest::qWaitFor([this] { return !mediaTransport.isNull(); }, 10'000)) {
            return false;
        }
        mediaTransport->becomeReady();
        return true;
    }

    bool ask(quint32 endpointId, int sliceId)
    {
        return client.sendMediaControl(
            subscription(endpointId, sliceId, radio.streamCentreHz(streamOf(sliceId))),
            client.sessionEpoch());
    }

    void finish() { client.disconnectFromStation(QStringLiteral("test complete")); }
};

} // namespace

class TstDaemonDisplaySendThread : public QObject {
    Q_OBJECT
private slots:
    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }

    void displaysKeepGoingWhileTheMainThreadIsHeld()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        Harness h;
        QVERIFY(h.firstSlice >= 0 && h.secondSlice >= 0);
        const int firstStream = h.streamOf(h.firstSlice);
        const int secondStream = h.streamOf(h.secondSlice);
        QVERIFY(firstStream >= 0 && secondStream >= 0 && firstStream != secondStream);
        QVERIFY(h.start());
        QVERIFY(h.ask(kFirstEndpoint, h.firstSlice));
        QVERIFY(h.ask(kSecondEndpoint, h.secondSlice));
        QTRY_COMPARE(h.controller.activeEndpointCount(), 2);

        Feeder feeder(&h.radio, {firstStream, secondStream}, 192000);
        feeder.start();
        // Displays flowing from both pans before the hold.
        QVERIFY(QTest::qWaitFor([&] {
            return static_cast<int>(h.log->copy().size()) >= kSendsBeforeHold;
        }, 30'000));

        // The main thread does nothing else for 500 ms, as it does while
        // Direct3D waits on a locked Windows session.
        const qint64 holdStartNs = nowNs();
        QThread::msleep(kHoldMs);
        const qint64 holdEndNs = nowNs();

        // Displays carry on once the main thread is free again.
        const std::size_t sentByHoldEnd = h.log->copy().size();
        QVERIFY(QTest::qWaitFor([&] {
            return h.log->copy().size() >= sentByHoldEnd + 10;
        }, 10'000));
        feeder.stop();

        const std::thread::id mainThread = std::this_thread::get_id();
        const std::vector<SendLog::Send> sends = h.log->copy();
        const auto onMain = std::count_if(sends.cbegin(), sends.cend(),
            [mainThread](const SendLog::Send& send) { return send.thread == mainThread; });
        qint64 previousNs = holdStartNs;
        qint64 maxGapNs = 0;
        int duringHold = 0;
        for (const SendLog::Send& send : sends) {
            if (send.atNs < holdStartNs || send.atNs > holdEndNs) { continue; }
            maxGapNs = std::max(maxGapNs, send.atNs - previousNs);
            previousNs = send.atNs;
            ++duringHold;
        }
        maxGapNs = std::max(maxGapNs, holdEndNs - previousNs);
        const QString detail =
            QStringLiteral("%1 of %2 display messages were sent on the main thread; "
                           "%3 were sent during the %4 ms hold, the longest gap %5 ms")
                .arg(onMain)
                .arg(sends.size())
                .arg(duringHold)
                .arg(kHoldMs)
                .arg(static_cast<double>(maxGapNs) / 1.0e6, 0, 'f', 1);
        qInfo().noquote() << detail;
        QVERIFY2(onMain == 0, qPrintable(detail));
        QVERIFY2(maxGapNs < kMaxGapNs, qPrintable(detail));
        h.finish();
    }

    // A frame the display thread leaves to the main thread (here for a
    // configure) and a retune the main thread makes before it gets to that
    // frame: the frame is of the old center and is dropped. The pan's next
    // context is the new center's, and none goes out for the center the
    // receiver has already left.
    void aFrameHeldForTheMainThreadIsDroppedByARetune()
    {
        Harness h;
        QVERIFY(h.firstSlice >= 0);
        QVERIFY(h.start());
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QVERIFY(h.ask(kFirstEndpoint, h.firstSlice));
        QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
        QTRY_COMPARE(h.controller.activeSourceCount(), 1);
        QVERIFY(h.controller.displaysSentFromThread());
        DaemonSpectrumSource* const source = &h.controller.sharedSpectrum()->source();
        const MediaSourceKey key = source->activeSources().constFirst();
        const auto contextCentres = [&controls] {
            QList<double> centres;
            for (const QList<QVariant>& arguments : std::as_const(controls)) {
                const QJsonObject message = arguments.constFirst().toJsonObject();
                if (message.value(QStringLiteral("op")).toString() == QStringLiteral("context")
                    && message.value(QStringLiteral("endpointId")).toInteger()
                        == qint64{kFirstEndpoint}) {
                    centres.append(message.value(QStringLiteral("sourceCentreHz")).toDouble());
                }
            }
            return centres;
        };
        // Frames from the engine's slot, as the FFT engine thread publishes
        // them, until this pan is sending.
        QTRY_VERIFY_WITH_TIMEOUT(
            source->publishFrameForTest(key, DaemonSpectrumSource::monotonicNowNs()), 10'000);
        QVERIFY(QTest::qWaitFor([&] {
            source->publishFrameForTest(key, DaemonSpectrumSource::monotonicNowNs());
            return h.log->copy().size() >= 5;
        }, 10'000));

        const double centre = std::round(h.radio.streamCentreHz(key.streamIndex));
        const double leftCentre = centre + 1000.0;
        const double newCentre = centre + 2000.0;
        controls.clear();
        // From here the main thread runs no event until the second retune.
        QVERIFY(h.radio.requestStreamCentre(h.firstSlice, leftCentre));
        QVERIFY(source->publishFrameForTest(key, DaemonSpectrumSource::monotonicNowNs()));
        // The display thread takes the frame in and, the pan waiting on a
        // configure, leaves the frame to this thread.
        QElapsedTimer taking;
        taking.start();
        while (source->frameWaitingForTest(key) && taking.elapsed() < 10'000) {
            QThread::msleep(5);
        }
        QVERIFY(!source->frameWaitingForTest(key));
        QVERIFY(h.radio.requestStreamCentre(h.firstSlice, newCentre));

        QVERIFY(QTest::qWaitFor([&] {
            source->publishFrameForTest(key, DaemonSpectrumSource::monotonicNowNs());
            return contextCentres().contains(newCentre);
        }, 10'000));
        const QList<double> centres = contextCentres();
        QStringList sent;
        for (double sentCentre : centres) {
            sent.append(QString::number(sentCentre, 'f', 0));
        }
        const QString detail = QStringLiteral("contexts after the retunes: %1")
                                   .arg(sent.join(QStringLiteral(", ")));
        QVERIFY2(!centres.contains(leftCentre), qPrintable(detail));
        QCOMPARE(centres.constFirst(), newCentre);
        h.finish();
    }
};

QTEST_MAIN(TstDaemonDisplaySendThread)
#include "tst_daemon_display_send_thread.moc"
