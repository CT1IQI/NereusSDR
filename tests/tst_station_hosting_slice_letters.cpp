// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_hosting_slice_letters.cpp  (NereusSDR)
// =================================================================
//
// JJ 2026-10-23: "the core headless and core inside window should be the
// same experience." A paired device that comes back after a Core restart
// gets the same slice letter, with that slice's saved settings, whether the
// Core restarted headless or inside a desktop window, in either direction.
// Inside a window, the Core keeps the device's letter and settings for it
// and makes its slice only when it signs in; the window's own slice takes
// a letter nobody's device has, binds a receiver and carries transmit.
//
// The logs this follows (Zephyrus G2E Core, 2026-10-22/23): at 22:25 the
// headless Core had JJ's window on slice A and saved its step and AGC-T
// AUTO there; the Core stopped without a leave; at 23:37 it came back
// inside the desktop window, the window took A, and the device was given
// a new slice B, so its A settings never came back to it.
//
// Every start here is the production one: a StationHost (with the window
// as its hosting device inside the window, as GuiDesktopStationRuntime
// starts it; without one headless, as DaemonApp does, after the receive
// layout is prepared) and then RadioModel::connectToRadio against the
// P1FakeRadio (ConnectableRadioModel), which sizes the stream pool, binds
// the slices and loads the window slice's state. Every stop is the
// production one, with the devices still on the Core (no session.leave):
// inside the window, GuiDesktopStationRuntime::prepareForRetirement's
// steps (stopAllTx, the host's stop, saveForStationHandover); headless,
// DaemonApp's release (saveForStationHandover, then the host's stop).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-23: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-23: PR 627 fix wave: driven through StationHost and the real
//               connect and shutdown; the window keeps letters instead of
//               holding slices. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "UnusedLoopbackPort.h"
#include "core/ReceiveLayoutStore.h"
#include "core/station/StationHost.h"
#include "fakes/ConnectableRadioModel.h"

#include <QSslSocket>

#include <functional>
#include <vector>

using NereusSDR::Test::ConnectableRadioModel;

namespace {

// ConnectableRadioModel's radio.
const QString kMac = QStringLiteral("aa:bb:cc:11:22:33");
const QString kWindowName = QStringLiteral("Zephyrus");
constexpr int kDeviceStepHz = 1000;
constexpr double kDeviceFrequencyHz = 7074000.0;

enum class Hosting { Headless, Window, WindowRunCoreOff };

// One Core run: its host is stopped before its radio model goes.
struct Station {
    std::unique_ptr<ConnectableRadioModel> radio;
    std::unique_ptr<StationHost> host;
    std::vector<std::unique_ptr<LoopbackTransport>> apps;
    Hosting hosting = Hosting::Headless;
    // Set before the connect, while `radio` is still being made.
    RadioModel* radioModel = nullptr;

    RadioModel& model() { return *radioModel; }
    StationServer* server() const { return host ? host->server() : nullptr; }

    ~Station()
    {
        host.reset();
        radio.reset();
    }
};

class Sandbox {
public:
    Sandbox()
        : m_security(NereusSDR::Test::withSharedTlsIdentity(
              NereusSDR::Test::seedCoreIdentity(m_securityDir.path())))
    {
    }

    StationHostOptions options(Hosting hosting)
    {
        StationHostOptions options;
        options.settings = &AppSettings::instance();
        options.securityDirectory = m_security;
        options.remoteBind = QStringLiteral("127.0.0.1");
        options.remotePort = unusedLoopbackPort();
        options.statusPage = false;
        options.rendezvousServers.clear();
        if (hosting == Hosting::Window) {
            options.hostingDevice = StationHostOptions::HostingDevice{kWindowName, kWindowName};
        }
        return options;
    }

    // `beforeConnect` runs once the host is up and before the radio connects.
    std::unique_ptr<Station> start(Hosting hosting, HPSDRHW board = HPSDRHW::HermesLite,
                                   const std::function<void(Station&)>& beforeConnect = {})
    {
        auto station = std::make_unique<Station>();
        Station* const raw = station.get();
        raw->hosting = hosting;
        raw->radio = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local,
            [this, raw, hosting, &beforeConnect](RadioModel& model) {
                raw->radioModel = &model;
                if (hosting == Hosting::Headless) {
                    // DaemonApp::start: the layout first, then the
                    // configured slice when the layout made none.
                    model.prepareReceiveLayout(kMac);
                    if (model.slices().isEmpty()) {
                        model.addSlice(QStringLiteral("pan-0"));
                    }
                }
                if (hosting != Hosting::WindowRunCoreOff) {
                    raw->host = std::make_unique<StationHost>(&model, options(hosting));
                    raw->host->start();
                }
                if (beforeConnect) {
                    beforeConnect(*raw);
                }
            },
            board);
        if (!raw->radio) {
            return nullptr;
        }
        if (hosting == Hosting::Headless) {
            // DaemonApp::onRadioStateForRecovery on Connected.
            raw->model().completeReceiveLayoutStartup();
        }
        QCoreApplication::processEvents();
        return station;
    }

    // Run Core turned on in a window whose radio is already connected.
    void turnOnRunCore(Station& station)
    {
        station.hosting = Hosting::Window;
        station.host = std::make_unique<StationHost>(&station.model(), options(Hosting::Window));
        station.host->start();
    }

    // The Core stops with its devices still on it.
    static void stop(std::unique_ptr<Station>& station)
    {
        RadioModel& model = station->model();
        QString error;
        if (station->hosting == Hosting::Headless) {
            QVERIFY2(model.saveForStationHandover(&error), qPrintable(error));
            station->host->stop();
        } else {
            model.stopAllTx(QStringLiteral("The desktop Core is closing."));
            if (station->host) {
                station->host->stop();
            }
            QVERIFY2(model.saveForStationHandover(&error), qPrintable(error));
        }
        station.reset();
        RadioDiscovery::clearHoldOffForTest();
    }

    void pair(Station& station, const Device& device)
    {
        QVERIFY(station.server());
        // The store is the security directory's: kept across restarts.
        if (!station.server()->deviceStore()->find(device.key.fingerprint())) {
            QVERIFY(station.server()->deviceStore()->add(device.record()));
        }
    }

    static LoopbackTransport* signIn(Station& station, const Device& device)
    {
        StationServer* server = station.server();
        auto app = std::make_unique<LoopbackTransport>(QStringLiteral("app"));
        auto* peer = new LoopbackTransport(QStringLiteral("station"));
        peer->setPeerAddress(QStringLiteral("192.0.2.7"));
        peer->linkTo(app.get());
        LoopbackTransport* const client = app.get();
        station.apps.push_back(std::move(app));
        server->acceptTransport(peer);
        QTest::qWaitFor([client]() { return !client->received().isEmpty(); }, 5000);
        const QJsonObject hello = firstOfType(client->received(), QStringLiteral("hello"));
        const QByteArray challenge =
            StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        const SessionMessage auth = SessionMessages::authRequest(
            QString(), device.block(challenge, QByteArray::fromHex(pin.toLatin1()),
                                    server->stationIdentity().publicKeySpki()));
        Core::send(client, auth, kSessionProtocolMinor, kHolder);
        return client;
    }

private:
    QTemporaryDir m_securityDir;
    QString m_security;
};

QList<int> ownedBy(Station& station, const Device& device)
{
    return station.model().sliceOwnership()->ownedBy(device.key.fingerprint());
}

// The device's settings on its slice, as JJ set them.
void setDeviceSettings(Station& station, int sliceId, int stepHz = kDeviceStepHz)
{
    SliceModel* slice = station.model().sliceById(sliceId);
    QVERIFY(slice);
    slice->setFrequency(kDeviceFrequencyHz);
    slice->setStepHz(stepHz);
    slice->setAutoAgcEnabled(true);
}

void verifyDeviceSettings(Station& station, int sliceId, int stepHz = kDeviceStepHz)
{
    const SliceModel* slice = station.model().sliceById(sliceId);
    QVERIFY(slice);
    QCOMPARE(slice->frequency(), kDeviceFrequencyHz);
    QCOMPARE(slice->stepHz(), stepHz);
    QVERIFY(slice->autoAgcEnabled());
}

// The window's own slice: the station device owns it for nobody, it has a
// receiver, it carries transmit, and it is the window's active slice.
void verifyWindowSlice(Station& station, int sliceId)
{
    RadioModel& model = station.model();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice);
    const SliceOwnership::Mark mark = model.sliceOwnership()->mark(sliceId);
    QCOMPARE(mark.owner, SliceOwnership::stationDevice());
    QVERIFY(mark.heldFor.isEmpty());
    QVERIFY(slice->streamIndex() >= 0);
    QCOMPARE(model.txBoundSlice(), slice);
    QCOMPARE(model.activeSlice(), slice);
}

QList<int> sliceIds(Station& station)
{
    QList<int> ids;
    for (const SliceModel* slice : station.model().slices()) {
        ids.append(slice->sliceIndex());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

class TestStationHostingSliceLetters : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
    }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // 22:25 headless, 23:37 inside the window: the device keeps A with its
    // step and AUTO, and the window takes B. Nothing of A runs until the
    // device signs in.
    void headlessThenInsideTheWindowKeepsTheDevicesLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), QList<int>{1});
        verifyWindowSlice(*core, 1);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0);
        QCOMPARE(sliceIds(*core), (QList<int>{0, 1}));
        QVERIFY(core->model().reservedSliceLetters().isEmpty());
        verifyWindowSlice(*core, 1);
    }

    // Inside the window the device is given B; after a headless restart B
    // is still its letter, with its settings.
    void insideTheWindowThenHeadlessKeepsTheDevicesLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 0);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QVERIFY(core->model().sliceById(1));
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{1});
        verifyDeviceSettings(*core, 1);
    }

    // The whole run from the logs: headless (22:25), inside the window
    // (23:37), the window again after a quit without a leave (07:33),
    // headless again (07:57). The device is on A throughout.
    void theSequenceFromTheLogsKeepsADeviceOnItsLetterThroughEveryRestart()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0);
        setDeviceSettings(*core, 0, 500);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0, 500);
        setDeviceSettings(*core, 0, 250);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyDeviceSettings(*core, 0, 250);
    }

    // Headless to headless, as before: the letter and settings come back.
    void headlessThenHeadlessKeepsTheDevicesLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        verifyDeviceSettings(*core, 1);
    }

    // A device that signs in to the window's Core before its radio has
    // connected waits for the radio, then gets its own letter; the window
    // still gets a letter of its own and the other device's stays kept.
    void aDeviceAdmittedBeforeTheRadioConnectsGetsItsLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        bool admittedEarly = false;
        core = sandbox.start(Hosting::Window, HPSDRHW::HermesLite, [&](Station& station) {
            QVERIFY(!station.model().isConnected());
            admittedEarly = admitted(Sandbox::signIn(station, iphone));
            QVERIFY(ownedBy(station, iphone).isEmpty());
        });
        QVERIFY(core);
        QVERIFY(admittedEarly);
        verifyWindowSlice(*core, 2);
        QTRY_COMPARE(ownedBy(*core, iphone), QList<int>{1});
        verifyDeviceSettings(*core, 1);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        verifyWindowSlice(*core, 2);
    }

    // The saved layout keeps every letter the radio has (HermesII, two):
    // the window still takes one, the last, and the device whose letter it
    // took gets no slice while none is free; its slice and settings go to
    // its saved slices.
    void aFullLayoutLeavesTheWindowTheLastLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        auto core = sandbox.start(Hosting::Headless, HPSDRHW::HermesII);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        sandbox.pair(*core, iphone);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        QCOMPARE(ownedBy(*core, iphone), QList<int>{1});
        setDeviceSettings(*core, 1);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window, HPSDRHW::HermesII);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), QList<int>{1});
        verifyWindowSlice(*core, 1);
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        QVERIFY(admitted(Sandbox::signIn(*core, iphone)));
        QVERIFY(ownedBy(*core, iphone).isEmpty());
        const QList<SavedSlice> kept =
            DeviceLayoutStore::load(AppSettings::instance(), kMac, iphone.key.fingerprint());
        QCOMPARE(kept.size(), 1);
        QCOMPARE(kept.first().id, 1);
        QCOMPARE(kept.first().frequencyHz, kDeviceFrequencyHz);
        QVERIFY(!kept.first().settings.isEmpty());
    }

    // Run Core turned on after the radio connected, as in real use: the
    // window has had A all along and saved its own settings under A. The
    // device's A is not given to it (the window has it); it gets B, at its
    // layout's frequency, with the settings A holds now, the window's.
    void runCoreTurnedOnLaterGivesTheDeviceAFreeLetter()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::WindowRunCoreOff);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), QList<int>{0});
        SliceModel* windowSlice = core->model().sliceById(0);
        windowSlice->setStepHz(50);
        core->model().flushPendingSettingsSave();

        sandbox.turnOnRunCore(*core);
        QVERIFY(core->model().hostsStationDevice());
        QVERIFY(core->model().reservedSliceLetters().isEmpty());
        QCOMPARE(sliceIds(*core), QList<int>{0});

        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{1});
        SliceModel* device = core->model().sliceById(1);
        QCOMPARE(device->frequency(), kDeviceFrequencyHz);
        QCOMPARE(device->stepHz(), 50);
        QCOMPARE(core->model().txBoundSlice(), windowSlice);
    }

    // Run Core turned off: the model no longer hosts, keeps no letters, and
    // the layout keeps the device's letter for the next start.
    void stoppingTheHostEndsHostingAndKeepsTheLayout()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        setDeviceSettings(*core, 0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        QVERIFY(core->model().hostsStationDevice());
        QCOMPARE(core->model().reservedSliceLetters(), QList<int>{0});
        core->host->stop();
        QVERIFY(!core->model().hostsStationDevice());
        QVERIFY(core->model().reservedSliceLetters().isEmpty());
        core->model().requestSettingsSave();
        core->model().flushPendingSettingsSave();
        const auto layout = ReceiveLayoutStore::load(AppSettings::instance(), kMac);
        QCOMPARE(layout.state, ReceiveLayoutStore::LoadState::Loaded);
        QCOMPARE(layout.slices.size(), 1);
        QCOMPARE(layout.slices.first().id, 0);
        QCOMPARE(layout.slices.first().owner, mac.key.fingerprint());
    }

    // A headless layout's entries no paired device has (an unowned slice)
    // come through a run inside the window unchanged.
    void insideTheWindowKeepsTheHeadlessUnownedEntries()
    {
        Sandbox sandbox;
        Device mac(QStringLiteral("Mac window"), QStringLiteral("computer"));

        auto core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        sandbox.pair(*core, mac);
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QCOMPARE(ownedBy(*core, mac), QList<int>{0});
        setDeviceSettings(*core, 0);
        const int unowned = core->model().addSlice(QStringLiteral("pan-0"));
        QCOMPARE(unowned, 1);
        core->model().sliceOwnership()->setOwner(unowned, QByteArray());
        core->model().sliceById(unowned)->setFrequency(3573000.0);
        Sandbox::stop(core);

        core = sandbox.start(Hosting::Window);
        QVERIFY(core);
        verifyWindowSlice(*core, 1);
        Sandbox::stop(core);

        const auto layout = ReceiveLayoutStore::load(AppSettings::instance(), kMac);
        QCOMPARE(layout.state, ReceiveLayoutStore::LoadState::Loaded);
        QCOMPARE(layout.slices.size(), 2);
        QCOMPARE(layout.slices.at(0).id, 0);
        QCOMPARE(layout.slices.at(0).owner, mac.key.fingerprint());
        QCOMPARE(layout.slices.at(1).id, 1);
        QVERIFY(layout.slices.at(1).owner.isEmpty());
        QCOMPARE(layout.slices.at(1).frequencyHz, 3573000.0);

        core = sandbox.start(Hosting::Headless);
        QVERIFY(core);
        QCOMPARE(sliceIds(*core), (QList<int>{0, 1}));
        QVERIFY(admitted(Sandbox::signIn(*core, mac)));
        QVERIFY(ownedBy(*core, mac).contains(0));
        verifyDeviceSettings(*core, 0);
    }
};

QTEST_MAIN(TestStationHostingSliceLetters)
#include "tst_station_hosting_slice_letters.moc"
