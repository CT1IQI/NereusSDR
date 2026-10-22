// no-port-check: NereusSDR-original regression test.
// =================================================================
// tests/tst_slice_filter_constrain.cpp  (NereusSDR)
// =================================================================
//
// Filter edges are constrained as Thetis's ConstrainFilter does
// (console.cs:35044-35131 [v2.10.3.15]). Thetis runs it in
// UpdateRX1Filters / UpdateRX2Filters (console.cs:7580, 7700
// [v2.10.3.15]), the one path every receive filter change takes, and
// then refuses a zero-width result (console.cs:7582). The whole-passband
// moves (filter shift and width sliders, whole-filter drag, the meter
// filter drag) call it first with filterShift = true so the width is
// kept (console.cs:35202, 35420, 50635, 50711; MeterManager.cs:19399).
//
// Two rules:
//   sideband - an edge may not cross 0 Hz the wrong way for the mode;
//              only when "Limit filter edges to sidebands" is on, which
//              is off by default (console.cs:7516).
//   cap      - |edge| <= MaxFilterShift and MaxFilterWidth, both 10000
//              by default (console.cs:13208, 13221); not in FM.
//
// Before this test SliceModel::setFilter took any value: a 250 kHz
// filter, a zero-width filter, and an LSB edge across zero with the
// limit on.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Claude Code. Cases
//                 informed by Longpath tests/tst_filter_constrain.cpp
//                 (commit 6a5df5f1); no code taken.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceFilterConstrain : public QObject
{
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // -- The 10 kHz cap: always on, every mode but FM --------------------

    void capClampsBothEdges()
    {
        int low = -100000;
        int high = 100000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::USB));
        QCOMPARE(low, -10000);
        QCOMPARE(high, 10000);
    }

    void capLeavesANormalFilterAlone()
    {
        int low = -2850;
        int high = -150;
        QVERIFY(!SliceModel::constrainFilter(low, high, DSPMode::LSB));
        QCOMPARE(low, -2850);
        QCOMPARE(high, -150);
    }

    void fmIsNotConstrained()
    {
        int low = -50000;
        int high = 50000;
        QVERIFY(!SliceModel::constrainFilter(low, high, DSPMode::FM, false, true));
        QCOMPARE(low, -50000);
        QCOMPARE(high, 50000);
    }

    void setFilterAppliesTheCap()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 250000);
        QCOMPARE(s.filterLow(), 100);
        QCOMPARE(s.filterHigh(), 10000);
    }

    void singleEdgeSettersApplyTheCap()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        s.setFilterHigh(40000);
        QCOMPARE(s.filterHigh(), 10000);
        s.setFilterLow(-40000);
        QCOMPARE(s.filterLow(), -10000);
    }

    void capKeepsTheWidthWhenShifting()
    {
        // A 3000 Hz passband moved past +10 kHz: with filterShift the other
        // edge moves by the same amount.
        int low = 8000;
        int high = 11000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::USB, true));
        QCOMPARE(high, 10000);
        QCOMPARE(low, 7000);
    }

    void shiftFilterKeepsTheWidth()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 3100);
        s.shiftFilter(8000, 11000);
        QCOMPARE(s.filterLow(), 7000);
        QCOMPARE(s.filterHigh(), 10000);
    }

    // -- Sideband crossing: only with the limit on ------------------------

    void limitIsOffByDefault()
    {
        SliceModel s;
        QVERIFY(!s.limitFiltersToSidebands());
        s.setDspMode(DSPMode::LSB);
        s.setFilter(-2850, 500);
        QCOMPARE(s.filterLow(), -2850);
        QCOMPARE(s.filterHigh(), 500);
    }

    void lsbUpperEdgeStopsAtZero()
    {
        SliceModel s;
        s.setDspMode(DSPMode::LSB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(-2850, 500);
        QCOMPARE(s.filterLow(), -2850);
        QCOMPARE(s.filterHigh(), 0);
    }

    void usbLowerEdgeStopsAtZero()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(-400, 2700);
        QCOMPARE(s.filterLow(), 0);
        QCOMPARE(s.filterHigh(), 2700);
    }

    void singleEdgeSetterCannotCrossZero()
    {
        SliceModel s;
        s.setDspMode(DSPMode::LSB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(-2850, -150);
        s.setFilterHigh(900);
        QCOMPARE(s.filterHigh(), 0);
    }

    void cwAndDigitalFollowTheirSideband()
    {
        int low = -600;
        int high = 200;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::CWL, false, true));
        QCOMPARE(high, 0);

        low = -200;
        high = 600;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::DIGU, false, true));
        QCOMPARE(low, 0);
    }

    void amKeepsZeroInsideThePassband()
    {
        int low = 200;
        int high = 4000;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::AM, false, true));
        QCOMPARE(low, 0);
        QCOMPARE(high, 4000);

        low = -4000;
        high = -200;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::SAM, false, true));
        QCOMPARE(low, -4000);
        QCOMPARE(high, 0);
    }

    void sidebandShiftKeepsTheWidth()
    {
        // A 2400 Hz LSB passband shifted 500 Hz past zero.
        int low = -1900;
        int high = 500;
        QVERIFY(SliceModel::constrainFilter(low, high, DSPMode::LSB, true, true));
        QCOMPARE(high, 0);
        QCOMPARE(low, -2400);
    }

    // -- Zero width -------------------------------------------------------

    void setFilterRefusesZeroWidth()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(100, 2900);
        QSignalSpy spy(&s, &SliceModel::filterChanged);
        s.setFilter(1500, 1500);
        QCOMPARE(s.filterLow(), 100);
        QCOMPARE(s.filterHigh(), 2900);
        QCOMPARE(spy.count(), 0);
    }

    void setFilterRefusesAFilterConstrainedToZeroWidth()
    {
        // USB with the limit on: (-500, 0) becomes (0, 0).
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(100, 2900);
        s.setFilter(-500, 0);
        QCOMPARE(s.filterLow(), 100);
        QCOMPARE(s.filterHigh(), 2900);
    }

    // -- A mode change takes the same path (SetRX1Mode -> UpdateRX1Filters) --

    void modeChangeConstrainsTheSavedFilter()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        const QString prefix = QStringLiteral("Slice%1/Band%2/Mode%3/")
                                   .arg(s.sliceIndex())
                                   .arg(bandKeyName(Band::Band20m))
                                   .arg(SliceModel::modeName(DSPMode::LSB));
        AppSettings::instance().setValue(prefix + QStringLiteral("FilterLow"), -60000);
        AppSettings::instance().setValue(prefix + QStringLiteral("FilterHigh"), -150);
        s.setDspMode(DSPMode::LSB);
        QCOMPARE(s.filterLow(), -10000);
        QCOMPARE(s.filterHigh(), -150);
    }
};

QTEST_MAIN(TestSliceFilterConstrain)
#include "tst_slice_filter_constrain.moc"
