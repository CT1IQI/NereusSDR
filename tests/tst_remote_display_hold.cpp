// =================================================================
// tests/tst_remote_display_hold.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. R-DISP-HOLD.
//
// A remote window's spectrum picture restarted by itself during receive:
// the grid follower moved the dBm scale 2 dB, the window asked the Core for
// the new range, the Core answered with a fresh endpoint whose averaged
// trace grew again from its seed, the regrowing trace pulled the pan's
// noise floor estimate down, and the grid follower moved the scale again.
//
// Two rules close the loop, and these tests hold both from the window's
// side: the window holds the dBm range it asked for while what the pan
// needs lies inside it (so the grid follower's moves ask nothing), and a
// context for the same picture blanks nothing the pan has drawn or
// measured.
//
// The planner is run by planNowForTest on a clock the test moves; nothing
// here is proved by waiting.
//
// R-DISP-RETUNE adds the third kind of context: the same picture moved in
// frequency (a retune, a pan drag). The drawn trace, the waterfall history
// and the noise floor carry on; the peak hold and the blobs empty.
//
// Modification history (NereusSDR):
//   2026-10-11  J.J. Boyd / KG4VCF  R-DISP-RETUNE: the moved picture tests.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest>

#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <random>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ClarityController.h"
#include "core/HpsdrModel.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/IMediaTransport.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/PanClarityRegistry.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteMediaController.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// The dBm window's headroom and whole-dB rounding, as the rule states them.
constexpr double kHeadroomDb = 10.0;

class DisplayTransport final : public IMediaTransport {
public:
    explicit DisplayTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override { return true; }
    void stop() override { active = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& packet) override
    {
        if (!active) { return false; }
        // A closed gate loses the packet, as a lossy link does.
        if (other && gateOpen) { other->deliver(packet); }
        return true;
    }
    bool sendRtp(const QByteArray&) override { return active; }
    bool isReady() const override { return active; }
    void activate() { active = true; emit ready(); }
    void deliver(const QByteArray& packet) { emit displayReceived(packet); }
    bool active = false;
    bool gateOpen = true;
    QPointer<DisplayTransport> other;
};

struct Ask {
    double minDbm = 0.0;
    double maxDbm = 0.0;
    bool mini = false;
};

struct Range {
    double minDbm = 0.0;
    double maxDbm = 0.0;
};

// What the pan needs coded: its scale joined with the waterfall levels in
// force (the levels Clarity set, or the stored ones).
Range neededBy(const SpectrumWidget* widget)
{
    const double panHigh = double(widget->refLevel());
    const double panLow = panHigh - double(widget->dynamicRange());
    const bool runtime = widget->clarityActive();
    const double low = runtime ? double(widget->wfActiveLowThreshold())
                               : double(widget->wfLowThreshold());
    const double high = runtime ? double(widget->wfActiveHighThreshold())
                                : double(widget->wfHighThreshold());
    return {std::min(panLow, low), std::max(panHigh, high)};
}

// One window, with or without a Core's media controller behind the station.
// Without one the station only receives what the window asks (the asks are
// counted as they leave the window); with one the Core answers with contexts
// and frames.
struct Fixture {
    QTemporaryDir dir;
    AppSettings settings;
    RadioModel station;
    std::unique_ptr<StationServer> server;
    QPointer<DisplayTransport> coreMedia;
    std::unique_ptr<DaemonMediaController> core;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&remote, &proxy};
    PanadapterStack stack;
    PanadapterApplet* pan = nullptr;
    SpectrumWidget* widget = nullptr;
    QPointer<DisplayTransport> windowMedia;
    std::unique_ptr<RemoteMediaController> window;
    qint64 now = 1'000;
    QList<Ask> asks;
    int sliceId = -1;
    int stream = -1;
    double centre = 0.0;
    std::minstd_rand noise{20261010U};

    explicit Fixture(bool withCore)
        : settings(dir.filePath(QStringLiteral("station.settings")))
    {
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        sliceId = station.addSlice();
        SliceModel* const slice = station.sliceById(sliceId);
        stream = slice ? slice->streamIndex() : -1;
        centre = stream >= 0 ? station.streamCentreHz(stream) : 0.0;
        server = std::make_unique<StationServer>(
            &station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server->setMediaEnabled(true);
        if (withCore) {
            server->setDisplayBudgetLimits({10'000'000, 10'000'000, 1});
            core = std::make_unique<DaemonMediaController>(
                server.get(), &station, nullptr, [this](QObject* owner) -> IMediaTransport* {
                    coreMedia = new DisplayTransport(owner);
                    return coreMedia;
                });
        }
        remote.audioEngine()->setMasterMuted(true); // The fixture opens no speaker.
        pan = stack.addPanadapter(QStringLiteral("pan"));
        stack.setActivePan(QStringLiteral("pan"));
        pan->addSlice(sliceId);
        pan->setActiveSliceIndex(sliceId);
        widget = pan->spectrumWidget();
        widget->setExtendedViewAllowed(false);
        widget->setDisplayWindowPreservingHistory(centre, 96000);
        widget->setVfoFrequency(centre);
        widget->setConnectionState(ConnectionState::Connected);
        widget->setDispNormalize(false);
        widget->setWfUseSpectrumMinMax(false);
        widget->setWfAgcEnabled(false);
        widget->setWaterfallNFAGCEnabled(false);
        widget->setAdjustGridMinToNoiseFloor(false);
        widget->setDbmRange(-140.0f, -40.0f);
        widget->setWfLowThreshold(-130.0f);
        widget->setWfHighThreshold(-60.0f);
        stack.resize(600, 700);
        stack.show();
    }

    bool start()
    {
        if (!QTest::qWaitForWindowExposed(&stack)) { return false; }
        window = std::make_unique<RemoteMediaController>(
            &client, &remote, &stack, nullptr,
            [this](QObject* owner) -> IMediaTransport* {
                windowMedia = new DisplayTransport(owner);
                return windowMedia;
            },
            [this] { return now; });
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        // Each ask is recorded as it leaves the window, inside the send.
        QObject::connect(clientLink, &Test::LoopbackTransport::outboundText, clientLink,
                         [this](const QByteArray& wire) { record(wire); });
        client.startSession(clientLink, server->token());
        server->acceptTransport(stationLink);
        if (!QTest::qWaitFor([this] {
                return client.mediaAvailable() && windowMedia && (!core || coreMedia);
            }, 10'000)) {
            return false;
        }
        if (core) {
            coreMedia->other = windowMedia;
            coreMedia->activate();
        }
        windowMedia->activate();
        return QTest::qWaitFor([this] { return panAsks() >= 1; }, 10'000);
    }

    void record(const QByteArray& wire)
    {
        if (!wire.contains("\"op\":\"subscribe\"")) { return; }
        static const QRegularExpression minimum(
            QStringLiteral("\"minDbm\":(-?[0-9.eE+-]+)"));
        static const QRegularExpression maximum(
            QStringLiteral("\"maxDbm\":(-?[0-9.eE+-]+)"));
        const QString text = QString::fromUtf8(wire);
        Ask ask;
        ask.minDbm = minimum.match(text).captured(1).toDouble();
        ask.maxDbm = maximum.match(text).captured(1).toDouble();
        ask.mini = wire.contains("\"displayRole\":\"mini\"");
        asks.append(ask);
    }

    int panAsks() const
    {
        return int(std::count_if(asks.cbegin(), asks.cend(),
                                 [](const Ask& ask) { return !ask.mini; }));
    }
    int miniAsks() const { return int(asks.size()) - panAsks(); }
    Ask lastAsk(bool mini = false) const
    {
        for (auto it = asks.crbegin(); it != asks.crend(); ++it) {
            if (it->mini == mini) { return *it; }
        }
        return {};
    }

    // A range that has held for longer than a frame period, planned: the
    // first tick sees it, the second finds it has held and asks if it must.
    void plan()
    {
        now += 60;
        window->planNowForTest();
        now += 60;
        window->planNowForTest();
    }

    // The receiver's samples: noise, so the trace has a floor of its own
    // well inside the dBm window (a silent receiver's floor is the window's
    // lower edge, which moves with the window).
    void feed()
    {
        QVector<float> iq(2048);
        std::uniform_real_distribution<float> sample(-1.0e-3f, 1.0e-3f);
        for (float& value : iq) { value = sample(noise); }
        QMetaObject::invokeMethod(&station, "rawIqDataForStream", Qt::DirectConnection,
                                  Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
    }

    // Feeds the receiver until `done`, letting the Core and the window run.
    bool run(const std::function<bool()>& done, int timeoutMs = 20'000)
    {
        return QTest::qWaitFor([&] { feed(); return done(); }, timeoutMs);
    }

    // The window has accepted the context that answers its latest ask.
    bool contextFollowsLastAsk() const
    {
        const Ask ask = lastAsk();
        const DisplayCodecContext& codec = widget->remoteCodecForTest();
        return codec.traceSamples != 0 && double(codec.minDbm) == ask.minDbm
            && double(codec.maxDbm) == ask.maxDbm;
    }

    void finish() { client.disconnectFromStation(QStringLiteral("test complete")); }
};

// Counts the window's "Core noise floor received" lines, which it logs once
// each time its received state goes from not received to received.
int g_noiseFloorReceivedLines = 0;
QtMessageHandler g_previousHandler = nullptr;

void countNoiseFloorReceived(QtMsgType type, const QMessageLogContext& context,
                             const QString& message)
{
    if (message.contains(QLatin1String("Core noise floor received for Clarity"))) {
        ++g_noiseFloorReceivedLines;
    }
    if (g_previousHandler) { g_previousHandler(type, context, message); }
}

} // namespace

class TstRemoteDisplayHold : public QObject
{
    Q_OBJECT

private:
    // Brings a pan with a Core behind it to a settled picture: a trace
    // averaged over a second, a scale placed around the trace's own noise
    // floor, and a noise floor estimate that has stopped moving.
    static bool settlePicture(Fixture& f, QSignalSpy& frames)
    {
        f.widget->setSpectrumAveraging(SpectrumAveraging::LogRecursive);
        f.widget->setSpectrumAverageTimeMs(1000);
        f.widget->setNoiseFloorFastAttack(false);
        if (!f.start()) { return false; }
        if (!f.run([&] { return f.contextFollowsLastAsk() && frames.size() >= 60; })) {
            return false;
        }
        // The estimate climbs from its start value over many frames: the
        // scale is placed only once it has stopped, or it would be placed
        // around a floor the trace does not have and the trace would sit
        // above the range the Core codes.
        if (!steadyFloor(f, frames)) { return false; }
        // The scale around the floor the trace really has.
        const float floor = std::round(f.widget->nfLerpAverageForTest());
        f.widget->setDbmRange(floor - 30.0f, floor + 70.0f);
        f.widget->setWfLowThreshold(floor - 20.0f);
        f.widget->setWfHighThreshold(floor + 40.0f);
        f.plan();
        if (!f.run([&] { return f.contextFollowsLastAsk(); })) { return false; }
        return steadyFloor(f, frames);
    }

    // The estimate holds within a tenth of a dB over 30 frames.
    static bool steadyFloor(Fixture& f, QSignalSpy& frames)
    {
        float mark = f.widget->nfLerpAverageForTest();
        qsizetype markFrames = frames.size();
        return f.run([&] {
            if (frames.size() - markFrames < 30) { return false; }
            const float value = f.widget->nfLerpAverageForTest();
            const bool steady = std::abs(value - mark) < 0.1f;
            mark = value;
            markFrames = frames.size();
            return steady;
        }, 60'000);
    }

private slots:
    void initTestCase()
    {
        const QString profile = QStringLiteral("remote-display-hold-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // The fault. The grid follower is on and noise floors move the grid
    // 2 dB at a time, down 8 dB and back. After the first accepted display
    // the window asks the Core nothing.
    void theGridFollowerMovingTheScaleAsksTheCoreNothing()
    {
        Fixture f(/*withCore=*/true);
        QSignalSpy frames(f.widget, &SpectrumWidget::spectrumFrameRendered);
        QVERIFY(f.start());
        QVERIFY(f.run([&] { return f.contextFollowsLastAsk() && frames.size() >= 5; }));
        // The grid follower as the operator has it: the grid's minimum 5 dB
        // under the noise floor, the range kept.
        f.widget->setNFOffsetGridFollow(-5);
        f.widget->setMaintainNFAdjustDelta(true);
        f.widget->setAdjustGridMinToNoiseFloor(true);
        f.widget->testApplyNoiseFloor(-115.0f);
        QCOMPARE(f.widget->gridMin(), -120);
        f.plan();
        QVERIFY(f.run([&] { return f.contextFollowsLastAsk(); }));
        const int accepted = f.panAsks();

        int moves = 0;
        const auto move = [&](float floorDbm) {
            const int before = f.widget->gridMin();
            f.widget->testApplyNoiseFloor(floorDbm);
            QCOMPARE(f.widget->gridMin(), before + (floorDbm < float(before + 5) ? -2 : 2));
            ++moves;
            f.plan();
            // The Core answers whatever was asked before the next move.
            QVERIFY(f.run([&] { return f.contextFollowsLastAsk(); }));
        };
        for (float floor = -117.0f; floor >= -123.0f; floor -= 2.0f) { move(floor); }
        for (float floor = -121.0f; floor <= -115.0f; floor += 2.0f) { move(floor); }
        QCOMPARE(moves, 8);
        QCOMPARE(f.widget->gridMin(), -120);
        QVERIFY2(f.panAsks() == accepted,
                 qPrintable(QStringLiteral("%1 asks for 8 grid moves of 2 dB")
                                .arg(f.panAsks() - accepted)));
        f.finish();
    }

    // The hold's edges. A first ask carries what the pan needs plus the
    // headroom on whole dB; a move inside the hold asks nothing; a move
    // past it asks exactly once, for a range that holds what the pan needs,
    // and that range is then held.
    void aMovePastTheHoldAsksOnceAndTheNewRangeIsHeld()
    {
        Fixture f(/*withCore=*/false);
        QVERIFY(f.start());
        // The pan shows -140..-40 and the waterfall -130..-60.
        QCOMPARE(f.panAsks(), 1);
        QCOMPARE(f.lastAsk().minDbm, -150.0);
        QCOMPARE(f.lastAsk().maxDbm, -30.0);

        // Inside the hold: to the edge of it, each way.
        f.widget->setDbmRange(-150.0f, -50.0f);
        f.plan();
        f.widget->setDbmRange(-130.0f, -30.0f);
        f.plan();
        f.widget->setDbmRange(-133.5f, -47.25f);
        f.plan();
        QCOMPARE(f.panAsks(), 1);

        // Past it by a quarter of a dB: once, with headroom, on whole dB.
        f.widget->setDbmRange(-150.25f, -50.25f);
        f.plan();
        QCOMPARE(f.panAsks(), 2);
        QCOMPARE(f.lastAsk().minDbm, -161.0);
        QCOMPARE(f.lastAsk().maxDbm, -40.0);
        // Held: planning again, and moving inside it, asks nothing.
        f.plan();
        f.widget->setDbmRange(-158.0f, -58.0f);
        f.plan();
        f.widget->setDbmRange(-150.25f, -50.25f);
        f.plan();
        QCOMPARE(f.panAsks(), 2);

        // Far wider than the pan needs: an edge twice the headroom outside
        // it is kept, an edge more than that is narrowed, once.
        f.widget->setDbmRange(-141.0f, -50.0f);
        f.plan();
        QCOMPARE(f.panAsks(), 2);
        f.widget->setDbmRange(-140.5f, -50.0f);
        f.plan();
        QCOMPARE(f.panAsks(), 3);
        QCOMPARE(f.lastAsk().minDbm, -151.0);
        QCOMPARE(f.lastAsk().maxDbm, -40.0);
        f.plan();
        QCOMPARE(f.panAsks(), 3);
        // The waterfall's levels are part of what the pan needs: with the
        // scale drawn in to -120, a waterfall at -135 keeps the range.
        f.widget->setWfLowThreshold(-135.0f);
        f.widget->setDbmRange(-120.0f, -50.0f);
        f.plan();
        QCOMPARE(f.panAsks(), 3);

        // An operator's drag of the scale past the hold: nothing while it
        // moves (each step comes before the last has held a frame period),
        // one ask when it stops.
        for (int step = 1; step <= 40; ++step) {
            f.widget->setDbmRange(-120.0f - float(step), -50.0f - float(step));
            f.now += 10;
            f.window->planNowForTest();
        }
        QCOMPARE(f.panAsks(), 3);
        f.plan();
        QCOMPARE(f.panAsks(), 4);
        QCOMPARE(f.lastAsk().minDbm, -170.0);
        QCOMPARE(f.lastAsk().maxDbm, -50.0);
        f.plan();
        QCOMPARE(f.panAsks(), 4);

        // The link's limits stand: the range never leaves -400..100 dBm.
        f.widget->setWfHighThreshold(95.0f);
        f.plan();
        QCOMPARE(f.lastAsk().maxDbm, 100.0);
        f.finish();
    }

    // A long seeded walk of the scale and of the waterfall levels set at
    // run time. Every ask holds what the pan needs with no more than the
    // headroom (and the rounding) to spare, and no ask comes before an edge
    // of what the pan needs has travelled a headroom less the rounding from
    // where it stood at the ask before: the range cannot creep, grow without
    // bound or alternate.
    void aLongWalkNeverAsksMoreOftenThanItsExcursionAllows()
    {
        Fixture f(/*withCore=*/false);
        f.remote.setClarityEnabled(true);
        PanClarityRegistry registry(&f.remote);
        registry.registerPan(QStringLiteral("pan"), f.widget, nullptr);
        SliceModel* const slice = f.station.sliceById(f.sliceId);
        registry.bindRemote(QStringLiteral("pan"), f.widget, slice,
                            {f.stream, slice->streamEpoch(), f.centre, 192000}, 1, 1, 1);
        ClarityController* const clarity = registry.controllerForPan(QStringLiteral("pan"));
        QVERIFY(clarity);
        double levelLow = -125.0;
        double levelHigh = -65.0;
        emit clarity->waterfallThresholdsChanged(float(levelLow), float(levelHigh));
        QVERIFY(f.widget->clarityActive());
        QVERIFY(f.start());
        QCOMPARE(f.panAsks(), 1);

        double panLow = -140.0;
        double panHigh = -40.0;
        Range atLastAsk = neededBy(f.widget);
        int counted = f.panAsks();
        std::mt19937 random(0x4e53U);
        std::uniform_int_distribution<int> which(0, 3);
        // Quarter-dB steps of up to 3 dB with a slow drift, so the walk
        // both dithers inside the hold and leaves it.
        std::uniform_int_distribution<int> step(-12, 12);
        constexpr int kSteps = 2000;
        for (int index = 0; index < kSteps; ++index) {
            const double delta = step(random) * 0.25 + (index % 400 < 200 ? -0.05 : 0.05);
            switch (which(random)) {
            case 0: panLow = std::clamp(panLow + delta, -220.0, panHigh - 20.0); break;
            case 1: panHigh = std::clamp(panHigh + delta, panLow + 20.0, 20.0); break;
            case 2: levelLow = std::clamp(levelLow + delta, -220.0, levelHigh - 10.0); break;
            default: levelHigh = std::clamp(levelHigh + delta, levelLow + 10.0, 20.0); break;
            }
            f.widget->setDbmRange(float(panLow), float(panHigh));
            emit clarity->waterfallThresholdsChanged(float(levelLow), float(levelHigh));
            f.plan();
            const Range needed = neededBy(f.widget);
            const Ask held = f.lastAsk();
            // What is held always holds what the pan needs.
            QVERIFY2(held.minDbm <= needed.minDbm && held.maxDbm >= needed.maxDbm,
                     qPrintable(QStringLiteral("step %1: %2..%3 does not hold %4..%5")
                                    .arg(index).arg(held.minDbm).arg(held.maxDbm)
                                    .arg(needed.minDbm).arg(needed.maxDbm)));
            // And is never more than twice the headroom wider on a side.
            QVERIFY(held.minDbm >= needed.minDbm - 2.0 * kHeadroomDb);
            QVERIFY(held.maxDbm <= needed.maxDbm + 2.0 * kHeadroomDb);
            if (f.panAsks() == counted) { continue; }
            QCOMPARE(f.panAsks(), counted + 1);
            counted = f.panAsks();
            // A new range: whole dB, the headroom outside what is needed.
            QCOMPARE(held.minDbm, std::floor(needed.minDbm - kHeadroomDb));
            QCOMPARE(held.maxDbm, std::ceil(needed.maxDbm + kHeadroomDb));
            // An edge travelled at least the headroom less the rounding.
            const double travelled = std::max(std::abs(needed.minDbm - atLastAsk.minDbm),
                                              std::abs(needed.maxDbm - atLastAsk.maxDbm));
            QVERIFY2(travelled >= kHeadroomDb - 1.0,
                     qPrintable(QStringLiteral("step %1: asked after %2 dB").arg(index)
                                    .arg(travelled)));
            atLastAsk = needed;
        }
        // The walk left the hold many times, and dithered far more often.
        QVERIFY2(counted > 5 && counted < kSteps / 10,
                 qPrintable(QStringLiteral("%1 asks in %2 steps").arg(counted).arg(kSteps)));
        f.finish();
    }

    // The mini display's range follows the same rule: the grid follower's
    // 2 dB moves ask nothing, a move past the hold asks once.
    void theMiniDisplayHoldsItsRangeTheSameWay()
    {
        Fixture f(/*withCore=*/false);
        QVERIFY(f.start());
        f.window->setMiniDisplaySlices({f.sliceId});
        f.plan();
        QCOMPARE(f.miniAsks(), 1);
        QCOMPARE(f.lastAsk(true).minDbm, -150.0);
        QCOMPARE(f.lastAsk(true).maxDbm, -30.0);

        f.widget->setNFOffsetGridFollow(-5);
        f.widget->setMaintainNFAdjustDelta(true);
        f.widget->setAdjustGridMinToNoiseFloor(true);
        for (float floor = -137.0f; floor >= -143.0f; floor -= 2.0f) {
            f.widget->testApplyNoiseFloor(floor);
            f.plan();
        }
        for (float floor = -141.0f; floor <= -135.0f; floor += 2.0f) {
            f.widget->testApplyNoiseFloor(floor);
            f.plan();
        }
        QCOMPARE(f.widget->gridMin(), -140);
        QVERIFY2(f.miniAsks() == 1,
                 qPrintable(QStringLiteral("%1 mini asks for 8 grid moves of 2 dB")
                                .arg(f.miniAsks() - 1)));
        QCOMPARE(f.panAsks(), 1);

        f.widget->testApplyNoiseFloor(-150.0f);
        f.plan();
        QCOMPARE(f.widget->gridMin(), -155);
        QCOMPARE(f.miniAsks(), 2);
        QCOMPARE(f.lastAsk(true).minDbm, -165.0);
        QCOMPARE(f.lastAsk(true).maxDbm, -45.0);
        f.plan();
        QCOMPARE(f.miniAsks(), 2);
        f.finish();
    }

    // A context for the same picture (the dBm range alone changed): the
    // painted trace and the peak hold are there before its keyframe
    // arrives and the pan's noise floor estimate is untouched; frames of the
    // new context then draw. A context for a new picture (a zoom) empties
    // the peak hold, as it does today.
    void aSamePictureContextBlanksNothingAndANewPictureDoes()
    {
        Fixture f(/*withCore=*/true);
        QSignalSpy frames(f.widget, &SpectrumWidget::spectrumFrameRendered);
        f.widget->setActivePeakHoldEnabled(true);
        QVERIFY(settlePicture(f, frames));
        QVERIFY(f.run([&] { return f.widget->activePeakHoldActive(); }));
        const int samples = f.widget->remoteCodecForTest().traceSamples;
        QVERIFY(samples > 0);
        QCOMPARE(int(f.widget->activePeakHoldPeaksForTest().size()), samples);

        // The Core's frames stop reaching the window: what the window shows
        // after the next context is what it kept, not a new keyframe.
        // Frames it had already received are still drawn at their times,
        // those of the old context at the frequency they were captured at
        // (R-R3-21), so the pixels may be those of any of them: what is
        // proved is that the trace, the peak hold and the noise floor
        // estimate are all still there.
        f.coreMedia->gateOpen = false;
        const QVector<float> painted = f.widget->renderedPixels();
        const quint32 generation = f.widget->remoteCodecForTest().contextGeneration;
        const int asked = f.panAsks();
        QVERIFY(!painted.isEmpty());

        // The dBm range moves past the hold: the same picture, asked again.
        f.widget->setDbmRange(f.widget->refLevel() - f.widget->dynamicRange() - 15.0f,
                              f.widget->refLevel() - 15.0f);
        f.plan();
        QCOMPARE(f.panAsks(), asked + 1);
        QVERIFY(f.run([&] { return f.contextFollowsLastAsk(); }));
        QVERIFY(f.widget->remoteCodecForTest().contextGeneration != generation);
        QVERIFY2(f.widget->remoteContextKeptPicture(),
                 "the window took a context for the same picture as a new picture");
        QCOMPARE(f.widget->renderedPixels().size(), painted.size());
        QVERIFY2(int(f.widget->activePeakHoldPeaksForTest().size()) == samples,
                 "the peak hold was emptied by a context for the same picture");
        QVERIFY(f.widget->activePeakHoldActive());

        // The new context's frames draw.
        f.coreMedia->gateOpen = true;
        const qsizetype before10 = frames.size();
        QVERIFY(f.run([&] { return frames.size() >= before10 + 10; }));
        QVERIFY(f.widget->activePeakHoldActive());

        // A new picture: the operator zooms in.
        f.coreMedia->gateOpen = false;
        const quint32 before = f.widget->remoteCodecForTest().contextGeneration;
        const int beforeAsks = f.panAsks();
        f.widget->setDisplayWindowPreservingHistory(f.centre, 48000);
        f.plan();
        QVERIFY(f.run([&] {
            f.plan();
            return f.panAsks() > beforeAsks && f.contextFollowsLastAsk()
                && f.widget->remoteCodecForTest().contextGeneration != before;
        }));
        QCOMPARE(int(f.widget->activePeakHoldPeaksForTest().size()), 0);
        QVERIFY(!f.widget->activePeakHoldActive());
        f.finish();
    }

    // The loop, closed. A grid move past the hold asks exactly once, and the
    // pan's noise floor estimate stays within 1 dB of where it was: the
    // averaged trace carried on at the Core, so there is nothing for the
    // grid follower to chase.
    void aMovePastTheHoldLeavesTheNoiseFloorWhereItWas()
    {
        Fixture f(/*withCore=*/true);
        QSignalSpy frames(f.widget, &SpectrumWidget::spectrumFrameRendered);
        QVERIFY(settlePicture(f, frames));
        const float floorBefore = f.widget->nfLerpAverageForTest();
        const int asked = f.panAsks();

        f.widget->setDbmRange(f.widget->refLevel() - f.widget->dynamicRange() - 15.0f,
                              f.widget->refLevel() - 15.0f);
        f.plan();
        QCOMPARE(f.panAsks(), asked + 1);
        float lowest = floorBefore;
        float highest = floorBefore;
        qsizetype seen = frames.size();
        qsizetype sinceContext = -1;
        QVERIFY(f.run([&] {
            if (frames.size() != seen) {
                seen = frames.size();
                lowest = std::min(lowest, f.widget->nfLerpAverageForTest());
                highest = std::max(highest, f.widget->nfLerpAverageForTest());
            }
            if (sinceContext < 0 && f.contextFollowsLastAsk()) { sinceContext = frames.size(); }
            // Two seconds of frames of the new context.
            return sinceContext >= 0 && frames.size() - sinceContext >= 60;
        }, 60'000));
        f.plan();
        QCOMPARE(f.panAsks(), asked + 1);
        QVERIFY2(floorBefore - lowest <= 1.0f && highest - floorBefore <= 1.0f,
                 qPrintable(QStringLiteral("the noise floor estimate was %1 dBm and ranged "
                                           "%2..%3 dBm after the ask")
                                .arg(floorBefore).arg(lowest).arg(highest)));
        f.finish();
    }

    // R-DISP-RETUNE. The operator drags the pan 12 kHz: the same span, the
    // same sample counts, another centre. The Core's averaged trace carried
    // on, so before the keyframe of the new context arrives the pan still
    // has a trace and its waterfall history, its noise floor estimate has
    // not moved and the noise floor is still "received"; the peak hold,
    // held per pixel, is empty. A zoom is still a new picture: it empties
    // the peak hold as well and the noise floor is awaited again.
    void aMovedPictureCarriesOnAndEmptiesOnlyThePeakHold()
    {
        Fixture f(/*withCore=*/true);
        PanClarityRegistry registry(&f.remote);
        registry.registerPan(QStringLiteral("pan"), f.widget, nullptr);
        QSignalSpy frames(f.widget, &SpectrumWidget::spectrumFrameRendered);
        f.widget->setActivePeakHoldEnabled(true);
        QVERIFY(settlePicture(f, frames));
        QVERIFY(f.run([&] { return f.widget->activePeakHoldActive(); }));
        const int samples = f.widget->remoteCodecForTest().traceSamples;
        QVERIFY(samples > 0);

        // The window logs the Core's noise floor once when it first arrives.
        g_noiseFloorReceivedLines = 0;
        g_previousHandler = qInstallMessageHandler(countNoiseFloorReceived);
        const auto restore = qScopeGuard([] { qInstallMessageHandler(g_previousHandler); });
        f.window->setClarityRegistry(&registry);
        QVERIFY(f.run([&] { return g_noiseFloorReceivedLines == 1; }));
        f.widget->tickWaterfallForTest();
        QVERIFY(f.widget->waterfallHistoryRowsForTest() > 0);

        // The Core's frames stop reaching the window: what it shows after
        // the context is what it kept, not a keyframe.
        f.coreMedia->gateOpen = false;
        const float floorBefore = f.widget->nfLerpAverageForTest();
        const int historyBefore = f.widget->waterfallHistoryRowsForTest();
        const quint32 generation = f.widget->remoteCodecForTest().contextGeneration;
        const int asked = f.panAsks();
        QVERIFY(!f.widget->renderedPixels().isEmpty());

        // 12 kHz is a whole number of bins at any FFT size, so the Core
        // grants the same sample counts.
        f.widget->setDisplayWindowPreservingHistory(f.centre + 12000.0, 96000);
        QVERIFY(f.run([&] {
            f.plan();
            return f.panAsks() > asked
                && f.widget->remoteCodecForTest().contextGeneration != generation
                && f.widget->remoteCodecForTest().traceSamples != 0;
        }));
        QVERIFY(!f.widget->remoteContextKeptPicture());
        QCOMPARE(int(f.widget->remoteCodecForTest().traceSamples), samples);
        QVERIFY2(f.widget->renderedPixels().size() == samples,
                 "the trace was blanked by a context for the picture moved");
        QVERIFY2(f.widget->waterfallHistoryRowsForTest() >= historyBefore,
                 "the waterfall history was dropped by a context for the picture moved");
        QCOMPARE(int(f.widget->activePeakHoldPeaksForTest().size()), 0);
        QVERIFY(!f.widget->activePeakHoldActive());
        QVERIFY2(std::abs(f.widget->nfLerpAverageForTest() - floorBefore) <= 1.0f,
                 qPrintable(QStringLiteral("the noise floor estimate went from %1 to %2 dBm")
                                .arg(floorBefore).arg(f.widget->nfLerpAverageForTest())));

        // The new context's frames draw, the estimate stays where it was and
        // the noise floor was never awaited again.
        f.coreMedia->gateOpen = true;
        float lowest = floorBefore;
        float highest = floorBefore;
        const qsizetype mark = frames.size();
        qsizetype seen = mark;
        QVERIFY(f.run([&] {
            if (frames.size() != seen) {
                seen = frames.size();
                lowest = std::min(lowest, f.widget->nfLerpAverageForTest());
                highest = std::max(highest, f.widget->nfLerpAverageForTest());
            }
            return frames.size() >= mark + 60;
        }, 60'000));
        QVERIFY2(floorBefore - lowest <= 1.0f && highest - floorBefore <= 1.0f,
                 qPrintable(QStringLiteral("the noise floor estimate was %1 dBm and ranged "
                                           "%2..%3 dBm after the move")
                                .arg(floorBefore).arg(lowest).arg(highest)));
        QVERIFY2(g_noiseFloorReceivedLines == 1,
                 "the noise floor was awaited again after a context for the picture moved");
        QCOMPARE(f.widget->remoteContextPicture(),
                 SpectrumWidget::RemoteContextPicture::Moved);
        QVERIFY(f.run([&] { return f.widget->activePeakHoldActive(); }));

        // A new picture: the operator zooms in.
        const quint32 before = f.widget->remoteCodecForTest().contextGeneration;
        const int beforeAsks = f.panAsks();
        f.widget->setDisplayWindowPreservingHistory(f.centre + 12000.0, 48000);
        QVERIFY(f.run([&] {
            f.plan();
            return f.panAsks() > beforeAsks
                && f.widget->remoteCodecForTest().contextGeneration != before
                && f.widget->remoteCodecForTest().traceSamples != 0
                && g_noiseFloorReceivedLines == 2;
        }));
        QCOMPARE(f.widget->remoteContextPicture(),
                 SpectrumWidget::RemoteContextPicture::New);
        f.finish();
    }

    // R-DISP-RETUNE. The mini display is centred on its slice, so every
    // tuning step asks for its picture moved. It keeps its painted frame
    // until the frames of the new centre arrive; taking the mini display
    // away still blanks it.
    void theMiniDisplayKeepsItsFrameThroughATuningStep()
    {
        Fixture f(/*withCore=*/true);
        QVERIFY(f.start());
        QSignalSpy miniFrames(f.window.get(), &RemoteMediaController::miniDisplayFrame);
        QSignalSpy unavailable(f.window.get(), &RemoteMediaController::miniDisplayUnavailable);
        f.window->setMiniDisplaySlices({f.sliceId});
        QVERIFY(f.run([&] { f.plan(); return miniFrames.size() >= 5; }));
        const double shownCentre = miniFrames.last().at(3).toDouble();
        const int miniAsked = f.miniAsks();
        unavailable.clear();

        SliceModel* const slice = f.remote.sliceById(f.sliceId);
        QVERIFY(slice);
        slice->setFrequency(slice->frequency() + 3000.0);
        QVERIFY(f.run([&] {
            f.plan();
            return f.miniAsks() > miniAsked && !miniFrames.isEmpty()
                && miniFrames.last().at(3).toDouble() != shownCentre;
        }));
        QVERIFY2(unavailable.isEmpty(),
                 qPrintable(QStringLiteral("the mini display was blanked %1 times by a "
                                           "tuning step").arg(unavailable.size())));

        f.window->setMiniDisplaySlices({});
        QTRY_VERIFY(!unavailable.isEmpty());
        f.finish();
    }
};

QTEST_MAIN(TstRemoteDisplayHold)
#include "tst_remote_display_hold.moc"
