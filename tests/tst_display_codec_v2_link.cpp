// =================================================================
// tests/tst_display_codec_v2_link.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. Display codec v2 (NSDC v2),
// Task 3: negotiation and the fallback setting.
//
// A peer whose hello declares displayCodec 2 is told displayCodecVersion 2,
// last before coreBuildInfo, and its media start may then ask for version 2
// display frames with displayCodecVersion 2. Any other start value is
// refused. A peer that did not declare the feature is sent exactly today's
// capabilities and may not ask. The Core's RemoteDisplayCodecV2 setting off
// withdraws the capability and the start field from every peer.
//
// The Core is the real StationServer and DaemonMediaController behind a
// loopback station session. No radio and no audio; displays only where a
// test feeds synthetic I/Q (Task 4: a v2 start's frames are version 2, a
// v1 start's version 1).
//
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd / KG4VCF  Task 4: the agreed version is the
//                                    version byte of every display frame.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-22  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <cmath>
#include <numbers>

#include "core/AppSettings.h"
#include "core/FFTEngine.h"
#include "core/HpsdrModel.h"
#include "core/session/MirrorSchema.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/IMediaTransport.h"
#include "core/spectrum/SpectrumDetectorMode.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
const QString kField = QStringLiteral("displayCodecVersion");

class FakeTransport final : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override { return true; }
    void stop() override {}
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& bytes) override
    {
        if (!readyState) { return false; }
        displays.append(bytes);
        return true;
    }
    bool sendRtp(const QByteArray&) override { return false; }
    bool isReady() const override { return readyState; }
    void becomeReady() { readyState = true; emit ready(); }

    bool readyState = false;
    QList<QByteArray> displays;
};

QVector<float> syntheticIq(int complexSamples, double cyclesPerSample)
{
    QVector<float> samples;
    samples.reserve(complexSamples * 2);
    for (int sample = 0; sample < complexSamples; ++sample) {
        const double phase = 2.0 * std::numbers::pi * cyclesPerSample * sample;
        samples.append(static_cast<float>(std::cos(phase)));
        samples.append(static_cast<float>(std::sin(phase)));
    }
    return samples;
}

QJsonObject spectrumPlane()
{
    return {{QStringLiteral("detector"), static_cast<int>(SpectrumDetectorMode::Peak)},
            {QStringLiteral("averageMode"), -1},
            {QStringLiteral("averageAlpha"), 0.0}};
}

struct Harness {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    QPointer<FakeTransport> mediaTransport;
    int transportsMade = 0;
    DaemonMediaController controller;
    Test::LoopbackTransport* clientLink = nullptr;

    Harness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
        , controller(&server, &radio, nullptr,
                     [this](QObject* parent) -> IMediaTransport* {
                         ++transportsMade;
                         mediaTransport = new FakeTransport(parent);
                         return mediaTransport;
                     },
                     [] { return qint64{10'000'000'000}; })
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        server.setMediaEnabled(true);
    }

    // The station session, up to media being available; no media start.
    bool connectSession()
    {
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        return QTest::qWaitFor([this] {
            return server.mediaAvailable() && client.mediaAvailable();
        }, 10'000);
    }

    // `version` undefined: a start without the field.
    bool startWith(const QJsonValue& version)
    {
        QJsonObject control{{QStringLiteral("op"), QStringLiteral("start")},
                            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}};
        if (!version.isUndefined()) {
            control.insert(kField, version);
        }
        return client.sendMediaControl(control, client.sessionEpoch());
    }

    // The properties of the first capabilities message the Core sent.
    QJsonArray capabilityProperties() const
    {
        for (const QByteArray& wire : clientLink->received()) {
            const QJsonObject message = QJsonDocument::fromJson(wire).object();
            if (message.value(QStringLiteral("type")).toString()
                == QStringLiteral("capabilities")) {
                return message.value(QStringLiteral("properties")).toArray();
            }
        }
        return {};
    }

    QStringList capabilityNames() const
    {
        QStringList names;
        for (const QJsonValue& property : capabilityProperties()) {
            names.append(property.toObject().value(QStringLiteral("name")).toString());
        }
        return names;
    }

    // A slice and its stream, so a subscribe has a source to show.
    int addFedSlice()
    {
        const int sliceId = radio.addSlice();
        if (sliceId >= 0) {
            streamIndex = radio.sliceById(sliceId)->streamIndex();
        }
        return sliceId;
    }

    void feedRadio()
    {
        QVERIFY(QMetaObject::invokeMethod(&radio, "rawIqDataForStream", Qt::DirectConnection,
                                          Q_ARG(int, streamIndex),
                                          Q_ARG(QVector<float>, syntheticIq(1026, 0.125))));
    }

    bool subscribe(quint32 endpointId, int sliceId)
    {
        const QJsonObject control{
            {QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
            {QStringLiteral("revision"), 1},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), 4096},
            {QStringLiteral("windowType"), static_cast<int>(WindowFunction::Hann)},
            {QStringLiteral("centreHz"), radio.streamCentreHz(streamIndex)},
            {QStringLiteral("spanHz"), 192000.0},
            {QStringLiteral("pixels"), 1024},
            {QStringLiteral("fps"), 60},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("trace"), spectrumPlane()},
            {QStringLiteral("waterfall"), spectrumPlane()},
            {QStringLiteral("minDbm"), -180.0},
            {QStringLiteral("maxDbm"), 0.0},
            {QStringLiteral("wideSpanFactor"), 0.0}};
        return client.sendMediaControl(control, client.sessionEpoch());
    }

    int streamIndex = -1;

    void finish() { client.disconnectFromStation(QStringLiteral("test complete")); }
};

// Sends the start (`version` undefined: without the field), a subscribe,
// and synthetic I/Q until the Core has sent `frames` display frames; then
// the version byte of every display frame it sent.
QList<int> versionBytesOfFrames(Harness& h, const QJsonValue& version, int frames)
{
    QList<int> versions;
    const int sliceId = h.addFedSlice();
    if (sliceId < 0) {
        return versions;
    }
    if (!h.connectSession() || !h.startWith(version)
        || !QTest::qWaitFor([&h] { return !h.mediaTransport.isNull(); }, 10'000)) {
        return versions;
    }
    h.mediaTransport->becomeReady();
    if (!h.subscribe(91, sliceId)) {
        return versions;
    }
    const auto codecFrames = [&h] {
        int count = 0;
        for (const QByteArray& message : h.mediaTransport->displays) {
            if (message.startsWith("NSDC")) {
                ++count;
            }
        }
        return count;
    };
    // A short count fails the caller's size check.
    const bool enough = QTest::qWaitFor([&] {
        h.feedRadio();
        return codecFrames() >= frames;
    }, 10'000);
    Q_UNUSED(enough);
    for (const QByteArray& message : h.mediaTransport->displays) {
        if (message.startsWith("NSDC") && message.size() > 4) {
            versions.append(static_cast<quint8>(message.at(4)));
        }
    }
    return versions;
}

// Sends each refused start, then a start without the field. The loopback
// delivers in order, so once the last start's transport exists every
// earlier start was handled: had any been taken, it would have made the
// transport (the later start on the same connection is then a no-op) and
// stored version 2.
void refusedThenPlainStarts(Harness& h, const QList<QJsonValue>& refused)
{
    for (const QJsonValue& value : refused) {
        QVERIFY(h.startWith(value));
    }
    QVERIFY(h.startWith(QJsonValue(QJsonValue::Undefined)));
    QTRY_VERIFY_WITH_TIMEOUT(!h.mediaTransport.isNull(), 10'000);
    QCOMPARE(h.transportsMade, 1);
    QCOMPARE(h.controller.displayCodecVersion(), quint32{1});
}

} // namespace

class TstDisplayCodecV2Link : public QObject {
    Q_OBJECT

    QString m_savedVersion;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QVector<float>>();
        qRegisterMetaType<MediaSourceKey>();
        // coreBuildInfo is sent only when the Core knows its version.
        m_savedVersion = QCoreApplication::applicationVersion();
        QCoreApplication::setApplicationVersion(QStringLiteral("2026.10.0"));
    }

    void cleanupTestCase() { QCoreApplication::setApplicationVersion(m_savedVersion); }

    void theSettingKeyAndDefault()
    {
        QCOMPARE(QString::fromLatin1(kRemoteDisplayCodecV2SettingsKey),
                 QStringLiteral("RemoteDisplayCodecV2"));
        Harness h;
        QVERIFY(h.server.displayCodecV2Enabled());
        h.server.setDisplayCodecV2Enabled(false);
        QVERIFY(!h.server.displayCodecV2Enabled());
        // No media session yet: nothing is available.
        h.server.setDisplayCodecV2Enabled(true);
        QVERIFY(!h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
    }

    // The entry reads back, and a bad entry reads as absent.
    void theCapabilityEntryRoundTrips()
    {
        StationCapabilities sent;
        sent.displayCodecVersion = 2;
        sent.rxMeterStampsVersion = 1;
        sent.coreBuildInfo = CoreBuildInfo{QStringLiteral("2026.10.0"), QStringLiteral("test@1")};
        const QList<MirrorUpdate> updates = sent.toUpdates();
        QList<QByteArray> names;
        for (const MirrorUpdate& update : updates) {
            names.append(update.name);
        }
        const qsizetype at = names.indexOf(QByteArrayLiteral("displayCodecVersion"));
        QVERIFY(at > 0);
        QCOMPARE(names.at(at - 1), QByteArrayLiteral("rxMeterStampsVersion"));
        QCOMPARE(names.at(at + 1), QByteArrayLiteral("coreBuildInfo"));
        QCOMPARE(names.last(), QByteArrayLiteral("coreBuildInfo"));
        QCOMPARE(StationCapabilities::fromUpdates(updates).displayCodecVersion, 2);

        // 0 sends no entry.
        StationCapabilities none;
        for (const MirrorUpdate& update : none.toUpdates()) {
            QVERIFY(update.name != QByteArrayLiteral("displayCodecVersion"));
        }
        QCOMPARE(StationCapabilities::fromUpdates(none.toUpdates()).displayCodecVersion, 0);

        // A string, or a value out of range, reads as absent.
        QList<MirrorUpdate> bad = updates;
        for (MirrorUpdate& update : bad) {
            if (update.name == QByteArrayLiteral("displayCodecVersion")) {
                update.kind = MirrorWireKind::Utf8;
                update.value = QStringLiteral("2");
            }
        }
        QCOMPARE(StationCapabilities::fromUpdates(bad).displayCodecVersion, 0);
        for (MirrorUpdate& update : bad) {
            if (update.name == QByteArrayLiteral("displayCodecVersion")) {
                update.kind = MirrorWireKind::Int64;
                update.value = qlonglong{0};
            }
        }
        QCOMPARE(StationCapabilities::fromUpdates(bad).displayCodecVersion, 0);
    }

    // A default Core tells a declaring peer version 2, last before
    // coreBuildInfo, and takes its start's displayCodecVersion 2.
    void aDeclaringPeerIsToldAndMayAsk()
    {
        Harness h;
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 2);
        const QStringList names = h.capabilityNames();
        QVERIFY(names.size() >= 3);
        QCOMPARE(names.last(), QStringLiteral("coreBuildInfo"));
        QCOMPARE(names.at(names.size() - 2), kField);
        QCOMPARE(names.at(names.size() - 3), QStringLiteral("rxMeterStampsVersion"));
        QVERIFY(h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
        QVERIFY(h.startWith(2));
        QTRY_VERIFY_WITH_TIMEOUT(!h.mediaTransport.isNull(), 10'000);
        QCOMPARE(h.transportsMade, 1);
        QCOMPARE(h.controller.displayCodecVersion(), quint32{2});
        h.finish();
    }

    // Only the integer 2 is taken; a start without the field agrees 1.
    void aStartTakesOnlyVersionTwo()
    {
        Harness h;
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 2);
        refusedThenPlainStarts(h, {QJsonValue(1), QJsonValue(3), QJsonValue(0),
                                   QJsonValue(true), QJsonValue(QStringLiteral("2")),
                                   QJsonValue(2.5)});
        h.finish();
    }

    // A peer that did not declare the feature is sent exactly today's
    // capabilities, and its start may not carry the field.
    void aPeerThatDidNotDeclareGetsTodaysCapabilities()
    {
        QJsonArray declared;
        {
            Harness h;
            QVERIFY(h.connectSession());
            declared = h.capabilityProperties();
            h.finish();
        }
        Harness h;
        h.client.withholdFeatureForTest(QByteArrayLiteral("displayCodec"));
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 0);
        QVERIFY(!h.capabilityNames().contains(kField));
        QVERIFY(!h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
        // Byte for byte the declaring peer's list without its one entry.
        QJsonArray stripped;
        for (const QJsonValue& property : declared) {
            if (property.toObject().value(QStringLiteral("name")).toString() != kField) {
                stripped.append(property);
            }
        }
        QCOMPARE(stripped.size(), declared.size() - 1);
        QCOMPARE(QJsonDocument(h.capabilityProperties()).toJson(QJsonDocument::Compact),
                 QJsonDocument(stripped).toJson(QJsonDocument::Compact));
        refusedThenPlainStarts(h, {QJsonValue(2)});
        h.finish();
    }

    // The fallback switch: off, a declaring peer is told nothing and its
    // start may not carry the field.
    void theFallbackSwitchWithdrawsIt()
    {
        Harness h;
        h.server.setDisplayCodecV2Enabled(false);
        QVERIFY(h.connectSession());
        QCOMPARE(h.client.capabilities().displayCodecVersion, 0);
        QVERIFY(!h.capabilityNames().contains(kField));
        QVERIFY(!h.server.displayCodecV2Available(h.server.mediaSessionEpoch()));
        refusedThenPlainStarts(h, {QJsonValue(2)});
        h.finish();
    }

    // Task 4: a v2 start's display frames all carry version byte 2; a
    // start without the field, on the same Core build in another harness,
    // gets version 1 frames.
    void theAgreedVersionIsEveryFramesVersionByte_data()
    {
        QTest::addColumn<bool>("v2");
        QTest::newRow("start agreed v2") << true;
        QTest::newRow("start without the field") << false;
    }

    void theAgreedVersionIsEveryFramesVersionByte()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        QFETCH(bool, v2);
        Harness h;
        const QList<int> versions = versionBytesOfFrames(
            h, v2 ? QJsonValue(2) : QJsonValue(QJsonValue::Undefined), 4);
        QVERIFY2(versions.size() >= 4, qPrintable(QString::number(versions.size())));
        QCOMPARE(h.controller.displayCodecVersion(), v2 ? quint32{2} : quint32{1});
        for (int version : versions) {
            QCOMPARE(version, v2 ? 2 : 1);
        }
        h.finish();
    }
};

QTEST_MAIN(TstDisplayCodecV2Link)
#include "tst_display_codec_v2_link.moc"
