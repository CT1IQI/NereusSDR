// =================================================================
// tests/tst_spectrum_receiver_grab.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Every receiver a pan shows can be grabbed: its filter edges (8 px outside
// the band, shrinking inside it so 6 px of body remain), its band body, and
// a strip 8 px either side of its center line. The selected receiver wins
// any overlap; a press on another receiver selects it and the same drag
// tunes it. A drag keeps the grab offset, a press with no move does not
// tune, and nothing tunes while the pan shows transmit.
//
// Driven through the widget's own event handlers with sent mouse events
// (offscreen, so this proves the hit test and the tune, not a real click
// through the macOS view tree).
// =================================================================
#include <QtTest/QtTest>
#include <QApplication>
#include <QMouseEvent>
#include <QSignalSpy>

#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

constexpr int    kW = 800;
constexpr int    kH = 400;
constexpr double kCenterHz = 14200000.0;
constexpr double kBandwidthHz = 80000.0;   // 100 Hz per pixel
constexpr double kHzPerPx = kBandwidthHz / kW;
constexpr int    kSpecY = 50;               // inside the spectrum area

// Mirrors SpectrumWidget::hzToX with the dBm strip hidden (the spectrum
// rect is the full width), truncating cast included.
int xForHz(double hz)
{
    const double lowHz = kCenterHz - kBandwidthHz / 2.0;
    return static_cast<int>((hz - lowHz) / kBandwidthHz * kW);
}

void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)), button, buttons,
                   Qt::NoModifier);
    QApplication::sendEvent(w, &me);
}

void press(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseButtonPress, QPoint(x, y), Qt::LeftButton, Qt::LeftButton);
}

void moveTo(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::LeftButton);
}

void hover(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::NoButton);
}

void release(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseButtonRelease, QPoint(x, y), Qt::LeftButton, Qt::NoButton);
}

struct Pan {
    SpectrumWidget sw;
    Pan()
    {
        sw.resize(kW, kH);
        sw.setDbmScaleVisible(false);
        sw.setFrequencyRange(kCenterHz, kBandwidthHz);
        sw.setConnectionState(ConnectionState::Connected);
        sw.setStepSize(100);
    }
    VfoWidget* receiver(int slice, double hz, int low, int high, bool selected)
    {
        VfoWidget* flag = sw.addVfoWidget(slice);
        flag->setFrequency(hz);
        flag->setFilter(low, high);
        flag->setStepHz(100);
        flag->setActiveSlice(selected);
        return flag;
    }
};

} // namespace

class TestSpectrumReceiverGrab : public QObject {
    Q_OBJECT
private slots:
    void narrow_band_center_press_tunes()
    {
        Pan p;
        p.receiver(0, kCenterHz, 0, 400, true);   // 4 px wide
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy panned(&p.sw, &SpectrumWidget::centerChanged);
        QSignalSpy edged(&p.sw, &SpectrumWidget::filterEdgeDragged);
        const int x = xForHz(kCenterHz + 200.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 20);
        release(&p.sw, x + 20);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 20 * kHzPerPx);
        QCOMPARE(panned.count(), 0);
        QCOMPARE(edged.count(), 0);
    }

    void wide_band_edge_zones()
    {
        // A 30 px band from the center line up.
        const int loX = xForHz(kCenterHz);
        {
            Pan p;
            p.receiver(0, kCenterHz, 0, 3000, true);
            QSignalSpy edged(&p.sw, &SpectrumWidget::filterEdgeDragged);
            press(&p.sw, loX + 3);   // 3 px inside the low edge
            moveTo(&p.sw, loX + 13);
            release(&p.sw, loX + 13);
            QCOMPARE(edged.count(), 1);
            QCOMPARE(edged.first().at(0).toInt(), 1000);
            QCOMPARE(edged.first().at(1).toInt(), 3000);
        }
        {
            Pan p;
            p.receiver(0, kCenterHz, 0, 3000, true);
            QSignalSpy edged(&p.sw, &SpectrumWidget::filterEdgeDragged);
            press(&p.sw, loX - 7);   // 7 px outside
            moveTo(&p.sw, loX - 17);
            release(&p.sw, loX - 17);
            QCOMPARE(edged.count(), 1);
            QCOMPARE(edged.first().at(0).toInt(), -1000);
        }
        {
            Pan p;
            p.receiver(0, kCenterHz, 0, 3000, true);
            QSignalSpy edged(&p.sw, &SpectrumWidget::filterEdgeDragged);
            QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
            QSignalSpy panned(&p.sw, &SpectrumWidget::centerChanged);
            press(&p.sw, loX - 9);   // 9 px outside: the pan
            moveTo(&p.sw, loX - 49);
            release(&p.sw, loX - 49);
            QCOMPARE(edged.count(), 0);
            QCOMPARE(tuned.count(), 0);
            QVERIFY(panned.count() >= 1);
        }
    }

    void center_strip_keeps_the_grab_offset()
    {
        Pan p;
        // The band sits 10 to 30 px left of the center line.
        p.receiver(0, kCenterHz, -3000, -1000, true);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        const int x = xForHz(kCenterHz) + 6;
        press(&p.sw, x, 5);
        moveTo(&p.sw, x + 10, 5);
        release(&p.sw, x + 10, 5);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), kCenterHz + 10 * kHzPerPx);
    }

    void press_and_release_without_a_move_does_not_tune()
    {
        Pan p;
        // Off the step grid, so a snap alone would move it.
        p.receiver(0, kCenterHz + 50.0, 0, 3000, true);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x, kSpecY + 3);   // vertical wobble only
        release(&p.sw, x, kSpecY + 3);
        QCOMPARE(tuned.count(), 0);
    }

    // Once the drag has left the press column, a return to it tunes back:
    // the receiver ends at its start, not at the last sample.
    void return_to_the_press_column_tunes_back()
    {
        Pan p;
        p.receiver(0, kCenterHz, 0, 3000, true);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 10);
        moveTo(&p.sw, x);
        release(&p.sw, x);
        QCOMPARE(tuned.count(), 2);
        QCOMPARE(tuned.at(0).first().toDouble(), kCenterHz + 10 * kHzPerPx);
        QCOMPARE(tuned.at(1).first().toDouble(), kCenterHz);
    }

    // The hover cursor uses the press's hit test: the drag cursor over a
    // receiver, the cross off it.
    void hover_cursor_follows_the_grab()
    {
        Pan p;
        p.receiver(0, kCenterHz, 0, 3000, true);
        hover(&p.sw, xForHz(kCenterHz + 1500.0));
        QCOMPARE(p.sw.cursor().shape(), Qt::SizeHorCursor);
        hover(&p.sw, xForHz(kCenterHz - 10000.0));
        QCOMPARE(p.sw.cursor().shape(), Qt::CrossCursor);
    }

    void press_on_another_receiver_selects_and_tunes_it()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        VfoWidget* b = p.receiver(1, kCenterHz + 10000.0, 0, 3000, false);
        QSignalSpy tunedPan(&p.sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy tunedA(a, &VfoWidget::frequencyChanged);
        QSignalSpy tunedB(b, &VfoWidget::frequencyChanged);
        QSignalSpy selectB(b, &VfoWidget::sliceActivationRequested);
        QSignalSpy edged(&p.sw, &SpectrumWidget::filterEdgeDragged);

        const int x = xForHz(kCenterHz + 11500.0);   // B's band body
        press(&p.sw, x);
        QCOMPARE(selectB.count(), 1);
        QCOMPARE(selectB.first().first().toInt(), 1);
        moveTo(&p.sw, x + 10);
        release(&p.sw, x + 10);
        QCOMPARE(tunedB.count(), 1);
        QCOMPARE(tunedB.first().first().toDouble(), kCenterHz + 10000.0 + 10 * kHzPerPx);
        QCOMPARE(tunedA.count(), 0);
        QCOMPARE(tunedPan.count(), 0);

        // B's edge, too, tunes B rather than resizing it. B is where the
        // drag above left it (its flag took the tune).
        tunedB.clear();
        QCOMPARE(b->frequency(), kCenterHz + 10000.0 + 10 * kHzPerPx);
        const int bLowX = xForHz(b->frequency());
        press(&p.sw, bLowX - 4);
        moveTo(&p.sw, bLowX + 6);
        release(&p.sw, bLowX + 6);
        QCOMPARE(edged.count(), 0);
        QCOMPARE(tunedB.count(), 1);
    }

    void selected_receiver_wins_an_overlap()
    {
        Pan p;
        VfoWidget* b = p.receiver(1, kCenterHz + 1000.0, 0, 3000, false);
        p.receiver(0, kCenterHz, 0, 3000, true);
        QSignalSpy tunedPan(&p.sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy tunedB(b, &VfoWidget::frequencyChanged);
        QSignalSpy selectB(b, &VfoWidget::sliceActivationRequested);
        const int x = xForHz(kCenterHz + 2000.0);   // inside both bands
        press(&p.sw, x);
        moveTo(&p.sw, x + 10);
        release(&p.sw, x + 10);
        QCOMPARE(selectB.count(), 0);
        QCOMPARE(tunedB.count(), 0);
        QCOMPARE(tunedPan.count(), 1);
        QCOMPARE(tunedPan.first().first().toDouble(), kCenterHz + 10 * kHzPerPx);
    }

    void transmit_overlay_withholds_every_drag()
    {
        Pan p;
        p.receiver(0, kCenterHz, 0, 3000, true);
        VfoWidget* b = p.receiver(1, kCenterHz + 10000.0, 0, 3000, false);
        QSignalSpy tunedPan(&p.sw, &SpectrumWidget::frequencyClicked);
        QSignalSpy tunedB(b, &VfoWidget::frequencyChanged);
        const int xA = xForHz(kCenterHz + 1500.0);
        const int xB = xForHz(kCenterHz + 11500.0);

        // Receiving, both drags tune: the presses land on the bands.
        press(&p.sw, xA);
        moveTo(&p.sw, xA + 10);
        release(&p.sw, xA + 10);
        QCOMPARE(tunedPan.count(), 1);

        p.sw.setMoxOverlay(true);
        press(&p.sw, xA);
        moveTo(&p.sw, xA + 20);
        release(&p.sw, xA + 20);
        press(&p.sw, xB);
        moveTo(&p.sw, xB + 20);
        release(&p.sw, xB + 20);
        QCOMPARE(tunedPan.count(), 1);
        QCOMPARE(tunedB.count(), 0);

        p.sw.setMoxOverlay(false);
        press(&p.sw, xB);
        moveTo(&p.sw, xB + 20);
        release(&p.sw, xB + 20);
        QCOMPARE(tunedB.count(), 1);
    }

    void locked_receiver_does_not_move()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kCenterHz, 0, 3000, true);
        a->setLocked(true);
        QSignalSpy tunedPan(&p.sw, &SpectrumWidget::frequencyClicked);
        const int x = xForHz(kCenterHz + 1500.0);
        press(&p.sw, x);
        moveTo(&p.sw, x + 20);
        release(&p.sw, x + 20);
        QCOMPARE(tunedPan.count(), 0);
    }
};

QTEST_MAIN(TestSpectrumReceiverGrab)
#include "tst_spectrum_receiver_grab.moc"
