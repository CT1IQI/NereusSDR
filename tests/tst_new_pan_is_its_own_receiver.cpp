// =================================================================
// tests/tst_new_pan_is_its_own_receiver.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Multi-pan slice-to-DDC placement is
// a Phase 3F concept; Thetis has a fixed RX1/RX2 pair and no allocator to
// port from.
//
// Bench report 2026-07-30 (JJ, KG4VCF): "creating a second pan, then
// retuning the first or second pan causes both to tune... dragging and
// panning up and down the band from the waterfall controls both, they
// should be independent."
//
// Not a wiring fault. Every spectrum handler already resolves its own
// pan's slice through sliceForPan(panId), and that was verified. The
// coupling was one layer down: RadioModel::addSlice seeds a new slice on
// the ACTIVE slice's frequency so the pan opens on the band being worked,
// which put it inside the active stream's window, so the allocator shared
// the DDC. A DDC has one centre and feeds one FFT stream, so the two pans
// were two views of a single receiver. They moved together because they
// were one thing.
//
// Operator decision, 2026-07-30: a new pan is a new receiver, matching
// AetherSDR and Thetis RX1/RX2. A slice added to a pan that already has
// slices still shares that pan's receiver, which is what makes co-hosted
// slices on one panadapter free.
//
// These cases pin the placement, which is where the coupling lived. The
// operator-visible half (drag pan A, pan B stays put) is bench row 26.
//
// Modification history (NereusSDR):
//   2026-10-22: a lone slice that tunes well inside another receiver of its
//               pan goes back onto it (JJ's ruling 2026-10-22). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/SliceStreamAllocator.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestNewPanIsItsOwnReceiver : public QObject
{
    Q_OBJECT

private:
    static void seed(RadioModel& model, int ddcs = 5)
    {
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ ddcs, /*maxSlices*/ 5, 192000);
    }

private slots:

    // ── 1. The reported case ─────────────────────────────────────────────
    //
    // Two pans, same band, because the new pan is seeded on the band being
    // worked. They must still be two receivers.
    void a_second_pan_on_the_same_band_gets_its_own_ddc()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);

        // Seeded onto A's band, which is the behaviour that caused the
        // sharing and which is deliberately kept.
        QCOMPARE(sliceB->frequency(), sliceA->frequency());

        QVERIFY2(sliceA->streamIndex() >= 0 && sliceB->streamIndex() >= 0,
                 "both slices must be bound");
        QVERIFY2(sliceA->streamIndex() != sliceB->streamIndex(),
            "a new pan is a new receiver: sharing the DDC is what made "
            "panning either pan move both");
    }

    // ── 1b. A refused pan leaves nothing behind ──────────────────────────
    //
    // Codex review P1, PR #311. addSlice discarded the bind result, so a
    // refused placement still produced a wired, emitted slice with
    // streamIndex() == -1: a pan that looks populated, receives no I/Q, and
    // permanently burns one of the five slice slots. Reachable since the
    // allocator started refusing instead of falling back to sharing.
    //
    // Two DDCs, both claimed by pans, then a third pan asked for.
    void a_refused_pan_does_not_leave_a_dead_slice()
    {
        RadioModel model;
        seed(model, /*ddcs*/ 2);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(model.sliceById(a));
        model.sliceById(a)->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(model.sliceById(b));

        const int before = model.slices().size();
        QSignalSpy rejected(&model, &RadioModel::sliceAddRejected);

        const int c = model.addSlice(QStringLiteral("pan-2"));

        QCOMPARE(c, -1);
        QCOMPARE(model.slices().size(), before);
        QVERIFY2(model.sliceById(c) == nullptr,
                 "a refused slice must not remain addressable");
        QCOMPARE(rejected.count(), 1);

        // The slot it would have burned is still available: a slice that
        // CAN be placed still succeeds afterwards.
        const int d = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY2(model.sliceById(d) != nullptr,
                 "the refused slice must not have consumed a slice slot");
        QVERIFY(model.sliceById(d)->streamIndex() >= 0);
    }

    // ── 1c. Slice A survives being created before the pool exists ────────
    //
    // The rollback keys on the pool being sized, not on the bare bool.
    // bindSliceToStream also returns false when there is no pool yet, which
    // is exactly where connectToRadio creates Slice A (configureStreamPool
    // runs later, then bindUnboundSlices binds it). Rolling back on the bool
    // alone would delete Slice A on every cold start.
    void a_slice_created_before_the_pool_is_kept()
    {
        RadioModel model;                     // deliberately NOT seeded

        const int a = model.addSlice(QStringLiteral("pan-0"));

        QVERIFY2(a >= 0, "a slice created before connect must still be made");
        QVERIFY(model.sliceById(a) != nullptr);
        QCOMPARE(model.sliceById(a)->streamIndex(), -1);

        // And it binds once the pool arrives, which is the whole reason it
        // had to survive.
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 5, 192000);
        model.bindUnboundSlices();
        QVERIFY2(model.sliceById(a)->streamIndex() >= 0,
                 "bindUnboundSlices must adopt the pre-pool slice");
    }

    // ── 2. +RX on an existing pan still shares ───────────────────────────
    //
    // The other half of the rule. If this also claimed a DDC, co-hosted
    // slices would stop being free and a 5-DDC radio would run out after
    // five slices however they were arranged.
    void a_second_slice_on_the_same_pan_shares_its_receiver()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);

        QCOMPARE(sliceB->streamIndex(), sliceA->streamIndex());
    }

    // ── 3. Four pans, four receivers ─────────────────────────────────────
    //
    // The 2x2 layout, which is where an operator would most expect four
    // independent views.
    void four_pans_claim_four_distinct_ddcs()
    {
        RadioModel model;
        seed(model);

        QSet<int> streams;
        for (int i = 0; i < 4; ++i) {
            const int id = model.addSlice(QStringLiteral("pan-%1").arg(i));
            SliceModel* s = model.sliceById(id);
            QVERIFY(s);
            QVERIFY(s->streamIndex() >= 0);
            streams.insert(s->streamIndex());
        }

        QCOMPARE(streams.size(), 4);
    }

    // ── 4. A full DDC pool refuses rather than silently coupling ─────────
    //
    // This assertion was inverted on 2026-08-01. The original (PR #293) held
    // that a coupled pan beats no pan, so a full pool fell back to sharing.
    // The HL2 bench behind PR #311 showed that fallback is the same defect
    // this flag exists to prevent, arriving silently: the third pan opens,
    // looks independent, and tunes in lockstep with whichever pan it landed
    // on. On a 2-DDC radio "no third independent window" is the truth, and
    // saying so is better than a pan that lies about what it is.
    //
    // The slice is still created; only the stream bind is refused, and the
    // caller surfaces placement.reason.
    void a_new_pan_is_refused_when_no_ddc_is_free()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 2, /*maxSlices*/ 5);
        alloc.setDefaultSampleRateHz(192000);
        alloc.activateStream(0, 14200000.0, 192000);
        alloc.activateStream(1, 7100000.0, 192000);

        using Outcome = SliceStreamAllocator::Outcome;

        // A third pan on a frequency stream 0 already covers. Sharing would
        // succeed and is exactly what must not happen here.
        const auto p = alloc.placeSlice(14200000.0, /*preferOwnStream*/ true);
        QCOMPARE(p.outcome, Outcome::Rejected);
        QVERIFY2(!p.reason.isEmpty(),
                 "a refusal must carry a reason for the operator");
        QVERIFY2(!p.reason.contains(QStringLiteral("none covers")),
            "the pool is exhausted, not mistuned: telling the operator to "
            "retune sends them chasing a fix that cannot work");

        // The same frequency without the flag still shares, which is what
        // keeps co-hosted slices free.
        const auto shared = alloc.placeSlice(14200000.0, /*preferOwnStream*/ false);
        QCOMPARE(shared.outcome, Outcome::JoinedExisting);
        QCOMPARE(shared.streamIndex, 0);
    }

    // ── 5. The allocator honours the preference directly ─────────────────
    //
    // Below RadioModel, so a future caller reading the flag gets the same
    // answer without standing up a model.
    void the_allocator_prefers_a_free_ddc_when_asked()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 4, /*maxSlices*/ 4);
        alloc.setDefaultSampleRateHz(192000);
        alloc.activateStream(0, 14200000.0, 192000);

        using Outcome = SliceStreamAllocator::Outcome;

        // Same frequency, inside stream 0's window.
        const auto shared = alloc.placeSlice(14200000.0, /*preferOwnStream*/ false);
        QCOMPARE(shared.outcome, Outcome::JoinedExisting);
        QCOMPARE(shared.streamIndex, 0);

        const auto dedicated = alloc.placeSlice(14200000.0, /*preferOwnStream*/ true);
        QCOMPARE(dedicated.outcome, Outcome::NewStream);
        QVERIFY2(dedicated.streamIndex != 0,
                 "a dedicated request must not land on the occupied stream");
        QCOMPARE(dedicated.newStreamCentreHz, 14200000.0);
        QCOMPARE(dedicated.shiftOffsetHz, 0.0);
    }

    // ── 6. The preference does not change a retune ───────────────────────
    //
    // Only the first bind may claim. A retune has co-hosts that depend on
    // its window, and pulling a slice onto a fresh DDC there would strand
    // them or leave a hole in the enable mask.
    void retuning_inside_a_shared_window_still_shares()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-0"));   // co-host
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);
        const int shared = sliceA->streamIndex();
        QCOMPARE(sliceB->streamIndex(), shared);

        // Nudge B a little, still inside the 192 kHz window.
        sliceB->setFrequency(14210000.0);

        QCOMPARE(sliceB->streamIndex(), shared);
    }

    // ── 7. A lone slice rejoins its pan's other receiver ─────────────────
    //
    // JJ's ruling 2026-10-22. STRIX Core, 2026-10-22: A restored on
    // 3.87 MHz and B on 7.262 MHz, one pan, two receivers. A was tuned to
    // 7.268 MHz, 6 kHz from B, and kept a receiver of its own, so the
    // window's pan cleared on every A/B click.
    //
    // Two slices on pan-0, A on 80 m and B on 40 m on receivers of their
    // own. Returns false (with a test failure) when the setup did not land.
    static bool twoReceiversOnOnePan(RadioModel& model, SliceModel*& a, SliceModel*& b)
    {
        a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        if (!a) { return false; }
        a->setFrequency(3870000.0);
        b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        if (!b) { return false; }
        // Seeded on A's frequency, so B shares A's receiver; tuning it to
        // 40 m takes it out of that window onto a receiver of its own.
        b->setFrequency(7262000.0);
        return a->streamIndex() >= 0 && b->streamIndex() >= 0
            && a->streamIndex() != b->streamIndex();
    }

    void a_lone_slice_tuned_into_its_pans_other_receiver_rejoins_it()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const int oldStream = a->streamIndex();
        const int bStream = b->streamIndex();
        const double bCentre = model.streamAllocator().streamCentreHz(bStream);
        QCOMPARE(bCentre, 7262000.0);

        a->setFrequency(7268000.0);

        QCOMPARE(a->streamIndex(), bStream);
        QCOMPARE(a->shiftOffsetHz(), 6000.0);
        QVERIFY2(!model.streamAllocator().isStreamActive(oldStream),
                 "A's old receiver must be released");
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
        // B's receiver did not move: its centre, and B's place on it.
        QCOMPARE(model.streamAllocator().streamCentreHz(bStream), bCentre);
        QCOMPARE(b->streamIndex(), bStream);
        QCOMPARE(b->shiftOffsetHz(), 0.0);
    }

    // Outside the other receiver's window, a lone slice moves its own
    // receiver as before. Then, tuned into it, it rejoins.
    void a_lone_slice_outside_the_other_window_keeps_its_own_receiver()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const int aStream = a->streamIndex();

        a->setFrequency(7400000.0);   // 138 kHz from B's centre; window +-96 kHz
        QCOMPARE(a->streamIndex(), aStream);
        QCOMPARE(model.streamAllocator().streamCentreHz(aStream), 7400000.0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);

        a->setFrequency(7270000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    // Anti-flap: a slice leaves a shared receiver at the window edge and
    // rejoins only within the inner 80% of it (76.8 kHz of a 96 kHz half
    // window), so stepping across the edge does not bounce it.
    void a_slice_near_the_window_edge_does_not_flap_between_receivers()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const int bStream = b->streamIndex();
        const double bCentre = 7262000.0;

        a->setFrequency(bCentre + 85000.0);   // in B's window, not well inside
        QVERIFY(a->streamIndex() != bStream);

        a->setFrequency(bCentre + 70000.0);   // well inside: rejoins
        QCOMPARE(a->streamIndex(), bStream);

        a->setFrequency(bCentre + 97000.0);   // out of the window: leaves
        const int ownStream = a->streamIndex();
        QVERIFY(ownStream != bStream);
        a->setFrequency(bCentre + 95000.0);   // back a step: stays on its own
        QCOMPARE(a->streamIndex(), ownStream);
        a->setFrequency(bCentre + 97000.0);
        QCOMPARE(a->streamIndex(), ownStream);
        QCOMPARE(model.streamAllocator().streamCentreHz(bStream), bCentre);
    }

    // The receive antenna: joining a receiver whose slices are on another
    // antenna would change that receiver's routing, so it keeps today's
    // behaviour. On the same antenna it rejoins.
    void a_slice_on_another_antenna_keeps_its_own_receiver()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const int aStream = a->streamIndex();
        a->setRxAntenna(QStringLiteral("ANT1"));
        b->setRxAntenna(QStringLiteral("ANT2"));
        QVERIFY(a->rxAntenna() != b->rxAntenna());

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), aStream);
        QCOMPARE(model.streamAllocator().streamCentreHz(aStream), 7268000.0);

        a->setRxAntenna(b->rxAntenna());
        a->setFrequency(7269000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    // Another pan's receiver is never joined: that would couple the pans.
    void a_slice_does_not_rejoin_another_pans_receiver()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setFrequency(3870000.0);
        SliceModel* b = model.sliceById(model.addSlice(QStringLiteral("pan-1")));
        QVERIFY(b);
        b->setFrequency(7262000.0);
        const int aStream = a->streamIndex();
        QVERIFY(aStream != b->streamIndex());

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), aStream);

        // On B's pan, the same retune rejoins.
        a->setPanKey(QStringLiteral("pan-1"));
        a->setFrequency(7269000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    // C-Tune: a pinned slice still inside its own window is left there, as
    // C-Tune holds the pan still. A pinned slice that leaves its window joins
    // a receiver with the same pin; one with a different pin is not joined.
    void ctune_holds_a_pinned_slice_inside_its_own_window()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const int aStream = a->streamIndex();
        a->setFrequency(7350000.0);          // 88 kHz from B: not well inside
        QCOMPARE(a->streamIndex(), aStream);
        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), true));
        QVERIFY(model.requestStreamCtunPinned(b->sliceIndex(), true));

        a->setFrequency(7300000.0);          // own window, and well inside B's
        QCOMPARE(a->streamIndex(), aStream);
        QCOMPARE(model.streamAllocator().streamCentreHz(aStream), 7350000.0);

        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), false));
        QVERIFY(model.requestStreamCtunPinned(b->sliceIndex(), false));
        a->setFrequency(7301000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    void a_pinned_slice_leaving_its_window_joins_only_a_receiver_pinned_alike()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const int aStream = a->streamIndex();
        const int bStream = b->streamIndex();
        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), true));

        a->setFrequency(7268000.0);          // B's receiver is not pinned
        QCOMPARE(a->streamIndex(), aStream);

        a->setFrequency(3870000.0);
        QVERIFY(model.requestStreamCtunPinned(b->sliceIndex(), true));
        a->setFrequency(7268000.0);          // both pinned: the JJ sequence
        QCOMPARE(a->streamIndex(), bStream);
        QVERIFY(a->streamCtunPinned());
        QVERIFY(!model.streamAllocator().isStreamActive(aStream));
    }

    // Restore: two slices of one pan inside one window share one receiver
    // (bindUnboundSlices places a later slice on a covering window).
    void restored_slices_inside_one_window_share_one_receiver()
    {
        RadioModel model;                     // pool comes later, as at start
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setFrequency(7262000.0);
        SliceModel* b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(b);
        b->setFrequency(7268000.0);

        seed(model);
        model.bindUnboundSlices();

        QVERIFY(a->streamIndex() >= 0);
        QCOMPARE(b->streamIndex(), a->streamIndex());
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }
};

QTEST_MAIN(TestNewPanIsItsOwnReceiver)
#include "tst_new_pan_is_its_own_receiver.moc"
