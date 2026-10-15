// =================================================================
// tests/tst_remote_step_persistence.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. A remote window's tune step
// against the Core that holds it: the window shows the step the Core has,
// and a step the Core refuses goes back and says why.
//
// Step persistence lane (2026-10-15), JJ's report "step selection does not
// seem to persist" on a remote window that had just listened in:
//   (a) a step change the Core refuses (another device controls the slice,
//       and the window has not been told yet) puts the window back on the
//       Core's step and shows the Core's reason, instead of leaving the
//       refused step on screen while the Core keeps its own;
//   (a) a slice the window listens to holds the step back with the reason,
//       and the flag's step button is disabled;
//   (b) a step the controlling window sets reaches the Core and is the
//       window's step again after the link drops and comes back.
//
// Loopback WebSocket and test radio models only: no RF, no radio, no audio
// device. Every run is offscreen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-15  J.J. Boyd / KG4VCF  Step persistence lane. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QLoggingCategory>
#include <QPushButton>
#include <QSignalSpy>

#include "core/SliceOwnership.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/OperatorReasonText.h"
#include "gui/TuneStepLabel.h"
#include "gui/widgets/StatusToast.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using namespace NereusSDR::Test;

namespace {

constexpr int kCoreStepHz = 100;
constexpr int kWindowStepHz = 1000;
const QString kSelf = QStringLiteral("token:1");

RemoteWindowHarness::Options sharingOptions(int stationSlices, const QString& layout)
{
    RemoteWindowHarness::Options options;
    options.stationSlices = stationSlices;
    options.panLayout = layout;
    options.sliceAccess = true;
    return options;
}

// Starts the window's own connection and waits until it shares slices and
// its mirror names it the controller of each slice it adopted.
bool connectSharing(RemoteWindowHarness& h)
{
    h.startStartupConnection();
    StationClient* client = h.client();
    if (!client || !QTest::qWaitFor([client] { return client->stationLinkReady(); }, 10000)) {
        return false;
    }
    if (!client->remoteSliceAccessAvailable()) { return false; }
    return QTest::qWaitFor([&h, client] {
        const QList<int> live = h.station().sliceOwnership()->liveSlices();
        for (int id : live) {
            const auto entry = client->sliceAccess()->entry(id);
            if (!entry || entry->controllerDeviceId != kSelf) { return false; }
        }
        return !live.isEmpty();
    }, 10000);
}

// A named phone on the Core, as tst_remote_window_harness admits one.
QByteArray admitPhone(RemoteWindowHarness& h, QObject& session)
{
    DeviceSessionRegistry::Entry phone;
    phone.deviceId = QByteArrayLiteral("phone-device-id-for-step-persistence");
    phone.kind = DeviceSessionRegistry::Kind::Paired;
    phone.name = QStringLiteral("Living room iPhone");
    phone.shortName = QStringLiteral("iPhone");
    phone.deviceKind = QStringLiteral("phone");
    const bool admitted = h.server().deviceSessions()->admit(phone, &session).admission
        == DeviceSessionRegistry::Admission::Admitted;
    return admitted ? phone.deviceId : QByteArray();
}

VfoWidget* flagFor(RemoteWindowHarness& h, int sliceId)
{
    for (VfoWidget* flag : h.window()->findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == sliceId) { return flag; }
    }
    return nullptr;
}

QPushButton* stepButtonOf(VfoWidget* flag, int stepHz)
{
    if (!flag) { return nullptr; }
    for (QPushButton* button : flag->findChildren<QPushButton*>()) {
        if (button->text() == formatTuneStepLabel(stepHz)) { return button; }
    }
    return nullptr;
}

int toastsSaying(RemoteWindowHarness& h, const QString& words)
{
    int count = 0;
    for (StatusToast* toast : h.window()->findChildren<StatusToast*>()) {
        if (toast->message() == words) { ++count; }
    }
    return count;
}

} // namespace

class TestRemoteStepPersistence : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("remote-step-persistence")));
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
    }

    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    // (a) The window changes the step in the moment between the Core giving
    // the slice to another device and the window hearing of it. The Core
    // refuses the write and keeps its step; the window must show the Core's
    // step again, and the Core's reason, not the step it asked for.
    void refusedStepGoesBackToTheCoresStepAndSaysWhy()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        StationClient* client = h.client();
        SliceModel* coreSlice = h.station().sliceById(1);
        SliceModel* windowSlice = h.remoteModel()->sliceById(1);
        QVERIFY(coreSlice && windowSlice);
        coreSlice->setStepHz(kCoreStepHz);
        QTRY_COMPARE(windowSlice->stepHz(), kCoreStepHz);

        h.coreLink()->holdSliceAccessUpdates();
        h.station().sliceOwnership()->setOwner(1, phone);
        QTRY_VERIFY(h.coreLink()->heldSliceAccessCount() > 0);
        QVERIFY(!windowSlice->isReadOnlyListener());

        QSignalSpy results(client, &StationClient::propertyWriteCompleted);
        windowSlice->setStepHz(kWindowStepHz);
        QTRY_VERIFY(!results.isEmpty());
        QCOMPARE(results.first().at(0).toByteArray(), QByteArrayLiteral("slice:1"));
        QCOMPARE(results.first().at(1).toByteArray(), QByteArrayLiteral("stepHz"));
        QVERIFY(!results.first().at(3).toBool());
        const QString reason = results.first().at(4).toString();
        QVERIFY(!reason.isEmpty());
        QCOMPARE(coreSlice->stepHz(), kCoreStepHz);

        QCOMPARE(windowSlice->stepHz(), kCoreStepHz);
        QTRY_COMPARE(toastsSaying(h, OperatorReasonText::forDisplay(reason)), 1);

        h.coreLink()->releaseSliceAccessUpdates();
        QTRY_VERIFY(windowSlice->isReadOnlyListener());
        QCOMPARE(windowSlice->stepHz(), kCoreStepHz);
    }

    // (a) A slice the window listens to: the step is held with the reason,
    // nothing is sent, and the flag's step button cannot be pressed.
    void listenedSliceHoldsTheStepWithTheReason()
    {
        RemoteWindowHarness h(sharingOptions(2, QStringLiteral("2v")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        SliceModel* coreSlice = h.station().sliceById(1);
        SliceModel* windowSlice = h.remoteModel()->sliceById(1);
        QVERIFY(coreSlice && windowSlice);
        coreSlice->setStepHz(kCoreStepHz);
        QTRY_COMPARE(windowSlice->stepHz(), kCoreStepHz);

        h.station().sliceOwnership()->setOwner(1, phone);
        QTRY_VERIFY(windowSlice->isReadOnlyListener());
        QTRY_VERIFY(flagFor(h, 1) && flagFor(h, 1)->isListening());
        QPushButton* step = stepButtonOf(flagFor(h, 1), kCoreStepHz);
        QVERIFY(step);
        QVERIFY(!step->isEnabled());

        QSignalSpy held(h.client(), &StationClient::sliceAccessHeld);
        windowSlice->setStepHz(kWindowStepHz);
        QCOMPARE(held.size(), 1);
        const QString reason = held.first().at(1).toString();
        QVERIFY(!reason.isEmpty());
        QCOMPARE(reason, windowSlice->readOnlyListenerReason());
        QCOMPARE(windowSlice->stepHz(), kCoreStepHz);
        QTest::qWait(300);
        QCOMPARE(coreSlice->stepHz(), kCoreStepHz);
    }

    // (b) The controlling window's step is the Core's, and is the window's
    // again after the link drops and the window reconnects.
    void controllingWindowsStepReachesTheCoreAndComesBackAfterReconnect()
    {
        RemoteWindowHarness h(sharingOptions(1, QStringLiteral("1")));
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        SliceModel* coreSlice = h.station().sliceById(0);
        SliceModel* windowSlice = h.remoteModel()->sliceById(0);
        QVERIFY(coreSlice && windowSlice);
        coreSlice->setStepHz(kCoreStepHz);
        QTRY_COMPARE(windowSlice->stepHz(), kCoreStepHz);

        windowSlice->setStepHz(kWindowStepHz);
        QTRY_COMPARE(coreSlice->stepHz(), kWindowStepHz);

        StationClient* client = h.client();
        h.dropLink();
        QTRY_VERIFY(!client->isHandshakeComplete());
        QTRY_VERIFY_WITH_TIMEOUT(client->isHandshakeComplete(), 15000);
        SliceModel* again = h.remoteModel()->sliceById(0);
        QVERIFY(again);
        QTRY_COMPARE(again->stepHz(), kWindowStepHz);
        QCOMPARE(h.station().sliceById(0)->stepHz(), kWindowStepHz);
    }
};

QTEST_MAIN(TestRemoteStepPersistence)
#include "tst_remote_step_persistence.moc"
