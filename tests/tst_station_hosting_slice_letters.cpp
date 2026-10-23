// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_hosting_slice_letters.cpp  (NereusSDR)
// =================================================================
//
// JJ 2026-10-23: "the core headless and core inside window should be the
// same experience." A paired device that comes back after a Core restart
// gets the same slice letter, with that slice's saved settings, whether the
// Core restarted headless or inside a desktop window, in either direction.
// Inside a window, the window's own slice takes a free letter and never
// the returning device's.
//
// The logs this follows (Zephyrus G2E Core, 2026-10-22/23): at 22:25 the
// headless Core had JJ's window on slice A and saved its step and AGC-T
// AUTO there; the Core stopped without a leave; at 23:37 it came back
// inside the desktop window, the window took A, and the device was given
// a new slice B, so its A settings never came back to it.
//
// Each restart here drops the Core's model and server without a
// session.leave, keeping its settings and security directory, as a
// process restart does. "Headless" is DaemonApp's start (receive layout
// prepared before the slices exist); "inside the window" is the desktop's
// (StationHost with the window as the hosting device, then the connect's
// starting slices).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-23: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/ReceiveLayoutStore.h"
#include "core/station/StationSliceOwnershipPolicy.h"

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:01");
const QString kWindowName = QStringLiteral("Zephyrus");
constexpr int kDeviceStepHz = 1000;
constexpr double kDeviceFrequencyHz = 7074000.0;

RadioInfo benchRadio()
{
    RadioInfo info;
    info.macAddress = kMac;
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    return info;
}

void makeServer(Core& core)
{
    core.server = std::make_unique<StationServer>(
        core.model.get(), *core.settings,
        NereusSDR::Test::withSharedTlsIdentity(
            NereusSDR::Test::seedCoreIdentity(core.securityDir.path())));
    core.server->setHeartbeatIntervalMs(0);
    core.server->deviceSessions()->setClock([&core]() { return core.now; });
}

// The Core stops with its devices still on it (no session.leave): what it
// last saved is what the next start reads.
void stopWithoutLeave(Core& core)
{
    if (core.model) {
        for (SliceModel* slice : core.model->slices()) {
            slice->saveToSettings(slice->band());
        }
        core.model->requestSettingsSave();
        core.model->flushPendingSettingsSave();
    }
    core.server.reset();
    core.model.reset();
}

// DaemonApp's start: the receive layout first, then the configured slice
// when the layout made none, then admission.
void startHeadless(Core& core, int sliceLimit = 5)
{
    core.model = std::make_unique<RadioModel>();
    RadioModel& model = *core.model;
    model.setBoardForTest(HPSDRHW::HermesLite);
    model.prepareReceiveLayout(kMac);
    model.configureStreamPool(sliceLimit, sliceLimit, 192000);
    if (model.slices().isEmpty()) {
        model.addSlice(QStringLiteral("pan-0"));
    }
    model.bindUnboundSlices();
    model.setLastRadioInfoForTest(benchRadio());
    model.setConnectionStateForTest(ConnectionState::Connected);
    model.completeReceiveLayoutStartup();
    makeServer(core);
}

// The desktop's start with Run Core on: StationHost makes the server with
// the window as the hosting device before the radio connects, then the
// connect makes the starting slices and selects the window's. Returns the
// window's slice id.
int startInWindow(Core& core, int sliceLimit = 5)
{
    core.model = std::make_unique<RadioModel>();
    RadioModel& model = *core.model;
    model.setBoardForTest(HPSDRHW::HermesLite);
    model.setLastRadioInfoForTest(benchRadio());
    model.configureStreamPool(sliceLimit, sliceLimit, 192000);
    makeServer(core);
    const QByteArray& station = SliceOwnership::stationDevice();
    core.server->deviceSessions()->registerHostingDevice(station, kWindowName, kWindowName);
    core.server->setStationDeviceWords(kWindowName, kWindowName);
    model.setHostsStationDevice(true);
    StationSliceOwnershipPolicy::activate(&model, core.server.get(), [] { return true; });
    const int windowSlice = model.addStartingSlices(kMac);
    if (SliceModel* slice = model.sliceById(windowSlice)) {
        model.setActiveSlice(static_cast<int>(model.slices().indexOf(slice)));
    }
    model.setConnectionStateForTest(ConnectionState::Connected);
    return windowSlice;
}

QList<int> ownedBy(const Core& core, const Device& device)
{
    return core.model->sliceOwnership()->ownedBy(device.key.fingerprint());
}

// The device's settings on its slice, as JJ set them.
void setDeviceSettings(Core& core, int sliceId, int stepHz = kDeviceStepHz)
{
    SliceModel* slice = core.model->sliceById(sliceId);
    QVERIFY(slice);
    slice->setFrequency(kDeviceFrequencyHz);
    slice->setStepHz(stepHz);
    slice->setAutoAgcEnabled(true);
}

void verifyDeviceSettings(const Core& core, int sliceId, int stepHz = kDeviceStepHz)
{
    const SliceModel* slice = core.model->sliceById(sliceId);
    QVERIFY(slice);
    QCOMPARE(slice->frequency(), kDeviceFrequencyHz);
    QCOMPARE(slice->stepHz(), stepHz);
    QVERIFY(slice->autoAgcEnabled());
}

// The slice is the window's own: the station device owns it and holds it
// for nobody.
bool windowOwns(const Core& core, int sliceId)
{
    const SliceOwnership::Mark mark = core.model->sliceOwnership()->mark(sliceId);
    return mark.owner == SliceOwnership::stationDevice() && mark.heldFor.isEmpty();
}

} // namespace

class TestStationHostingSliceLetters : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // 22:25 headless, 23:37 inside the window: the device keeps A with its
    // step and AUTO, and the window takes B.
    void headlessThenInsideTheWindowKeepsTheDevicesLetter()
    {
        Core core;
        stopWithoutLeave(core);
        Device phone(QStringLiteral("Mac window"), QStringLiteral("computer"));

        startHeadless(core);
        core.pair(phone);
        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        setDeviceSettings(core, 0);
        stopWithoutLeave(core);

        const int windowSlice = startInWindow(core);
        QCOMPARE(windowSlice, 1);
        QVERIFY(windowOwns(core, 1));
        QCOMPARE(core.model->activeSlice(), core.model->sliceById(1));
        const SliceOwnership::Mark held = core.model->sliceOwnership()->mark(0);
        QCOMPARE(held.owner, SliceOwnership::stationDevice());
        QCOMPARE(held.heldFor, phone.key.fingerprint());

        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        verifyDeviceSettings(core, 0);
        QVERIFY(windowOwns(core, 1));
        QCOMPARE(core.model->slices().size(), 2);
    }

    // Inside the window the device is given B; after a headless restart B
    // is still its letter, with its settings.
    void insideTheWindowThenHeadlessKeepsTheDevicesLetter()
    {
        Core core;
        stopWithoutLeave(core);
        Device phone(QStringLiteral("Mac window"), QStringLiteral("computer"));

        QCOMPARE(startInWindow(core), 0);
        core.pair(phone);
        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{1});
        QVERIFY(windowOwns(core, 0));
        setDeviceSettings(core, 1);
        stopWithoutLeave(core);

        startHeadless(core);
        QVERIFY(core.model->sliceById(1));
        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{1});
        verifyDeviceSettings(core, 1);
    }

    // The whole run from the logs: headless (22:25), inside the window
    // (23:37), the window again after a quit without a leave (07:33),
    // headless again (07:57). The device is on A throughout.
    void theSequenceFromTheLogsKeepsADeviceOnItsLetterThroughEveryRestart()
    {
        Core core;
        stopWithoutLeave(core);
        Device phone(QStringLiteral("Mac window"), QStringLiteral("computer"));

        startHeadless(core);
        core.pair(phone);
        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        setDeviceSettings(core, 0);
        stopWithoutLeave(core);

        QCOMPARE(startInWindow(core), 1);
        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        verifyDeviceSettings(core, 0);
        setDeviceSettings(core, 0, 500);
        stopWithoutLeave(core);

        QCOMPARE(startInWindow(core), 1);
        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        verifyDeviceSettings(core, 0, 500);
        setDeviceSettings(core, 0, 250);
        stopWithoutLeave(core);

        startHeadless(core);
        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        verifyDeviceSettings(core, 0, 250);
    }

    // Headless to headless, as before: the letter and settings come back.
    void headlessThenHeadlessKeepsTheDevicesLetter()
    {
        Core core;
        stopWithoutLeave(core);
        Device phone(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        startHeadless(core);
        core.pair(phone);
        core.pair(iphone);
        QVERIFY(admitted(core.signIn(phone)));
        QVERIFY(admitted(core.signIn(iphone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        QCOMPARE(ownedBy(core, iphone), QList<int>{1});
        setDeviceSettings(core, 1);
        stopWithoutLeave(core);

        startHeadless(core);
        QVERIFY(admitted(core.signIn(iphone)));
        QCOMPARE(ownedBy(core, iphone), QList<int>{1});
        verifyDeviceSettings(core, 1);
    }

    // The saved layout fills every letter the radio has: the window still
    // takes one (the last), and the device whose letter it took keeps that
    // slice in its saved slices, with its settings, for when a letter is
    // free.
    void aFullLayoutLeavesTheWindowTheLastLetter()
    {
        Core core;
        stopWithoutLeave(core);
        Device phone(QStringLiteral("Mac window"), QStringLiteral("computer"));
        Device iphone(QStringLiteral("iPhone"), QStringLiteral("phone"));

        startHeadless(core, 2);
        core.pair(phone);
        core.pair(iphone);
        QVERIFY(admitted(core.signIn(phone)));
        QVERIFY(admitted(core.signIn(iphone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
        QCOMPARE(ownedBy(core, iphone), QList<int>{1});
        setDeviceSettings(core, 1);
        stopWithoutLeave(core);

        QCOMPARE(startInWindow(core, 2), 1);
        QVERIFY(windowOwns(core, 1));
        const QList<SavedSlice> kept =
            DeviceLayoutStore::load(AppSettings::instance(), kMac, iphone.key.fingerprint());
        QCOMPARE(kept.size(), 1);
        QCOMPARE(kept.first().id, 1);
        QCOMPARE(kept.first().frequencyHz, kDeviceFrequencyHz);

        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{0});
    }

    // Run Core turned on after the radio connected: the window already has
    // A, so the device's saved A goes to its saved slices and comes back,
    // with its settings, on the lowest free letter.
    void runCoreTurnedOnLaterKeepsTheDevicesSettings()
    {
        Core core;
        stopWithoutLeave(core);
        Device phone(QStringLiteral("Mac window"), QStringLiteral("computer"));

        startHeadless(core);
        core.pair(phone);
        QVERIFY(admitted(core.signIn(phone)));
        setDeviceSettings(core, 0);
        stopWithoutLeave(core);

        core.model = std::make_unique<RadioModel>();
        RadioModel& model = *core.model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.setLastRadioInfoForTest(benchRadio());
        model.configureStreamPool(5, 5, 192000);
        QCOMPARE(model.addStartingSlices(kMac), 0);
        model.setConnectionStateForTest(ConnectionState::Connected);
        makeServer(core);
        core.server->deviceSessions()->registerHostingDevice(
            SliceOwnership::stationDevice(), kWindowName, kWindowName);
        model.setHostsStationDevice(true);
        StationSliceOwnershipPolicy::activate(&model, core.server.get(), [] { return true; });
        QCOMPARE(model.slices().size(), 1);

        QVERIFY(admitted(core.signIn(phone)));
        QCOMPARE(ownedBy(core, phone), QList<int>{1});
        verifyDeviceSettings(core, 1);
    }
};

QTEST_MAIN(TestStationHostingSliceLetters)
#include "tst_station_hosting_slice_letters.moc"
