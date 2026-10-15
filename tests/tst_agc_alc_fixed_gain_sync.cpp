// no-port-check: NereusSDR-original unit-test file; no Thetis logic ported.
// =================================================================
// tests/tst_agc_alc_fixed_gain_sync.cpp  (NereusSDR)
// =================================================================
//
// Settings > DSP > AGC/ALC: the Fixed Gain spinner follows the slice. A
// write from elsewhere (a remote window, the phone, a future AGC-T in Off)
// shows on an open page, and the update does not echo back into the model.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-15 - New test file: model-to-spinner sync for Fixed Gain.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSpinBox>

#include "gui/setup/DspSetupPages.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

QSpinBox* spinnerById(QWidget* page, const char* id)
{
    for (QSpinBox* spin : page->findChildren<QSpinBox*>()) {
        if (spin->property("nereusSetupId").toString() == QLatin1String(id)) {
            return spin;
        }
    }
    return nullptr;
}

} // namespace

class TestAgcAlcFixedGainSync : public QObject {
    Q_OBJECT

private slots:
    void modelChangeUpdatesSpinnerWithoutEcho()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);

        AgcAlcSetupPage page(&model);
        QSpinBox* spin = spinnerById(&page, "dsp.agcAlc.agcFixedGain");
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), slice->agcFixedGain());

        QSignalSpy modelSpy(slice, &SliceModel::agcFixedGainChanged);
        QSignalSpy spinSpy(spin, QOverload<int>::of(&QSpinBox::valueChanged));

        slice->setAgcFixedGain(45);
        QCOMPARE(spin->value(), 45);
        // One model emit, none from the spinner: no echo back.
        QCOMPARE(modelSpy.count(), 1);
        QCOMPARE(spinSpy.count(), 0);

        // The spinner still writes the model.
        spin->setValue(30);
        QCOMPARE(slice->agcFixedGain(), 30);
    }
};

QTEST_MAIN(TestAgcAlcFixedGainSync)
#include "tst_agc_alc_fixed_gain_sync.moc"
