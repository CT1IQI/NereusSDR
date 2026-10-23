// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_display_codec_v2.cpp  (NereusSDR)
// =================================================================
//
// Display codec v2 Task 1: the NSDC v2 encoder
// (docs/architecture/2026-10-22-display-codec-v2-intra-rows.md, sections
// 4 to 6 and 8). Hand-checked planes as literal bytes, the encoder's
// deterministic choices, the plane bound for every length, the frame
// layout, the one-message-or-two rule, the scene sizes of section 6, and
// the golden vector file, which every plane and frame entry must re-encode
// to exactly.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2 encoder, bound, split rule, scene sizes and golden
//               vector checks. J.J. Boyd (KG4VCF), AI-assisted via Claude
//               Code.
// =================================================================

#include <QtTest>

#include <QSet>

#include <cstring>
#include <optional>

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/IMediaTransport.h"

#include "DisplayCodecV2Vectors.h"

using namespace NereusSDR;
namespace Vectors = NereusSDR::Test::DisplayCodecV2Vectors;

namespace {

constexpr quint32 kHeader = DisplayCodecEncoder::kHeaderBytes;

QByteArray bytesOf(std::initializer_list<int> values)
{
    QByteArray out;
    for (int value : values) { out.append(static_cast<char>(value)); }
    return out;
}

QVector<quint8> qRow(std::initializer_list<int> values)
{
    QVector<quint8> out;
    for (int value : values) { out.append(static_cast<quint8>(value)); }
    return out;
}

QVector<quint8> noiseRow(int n, quint64 seed)
{
    Vectors::SplitMix64 rng(seed);
    QVector<quint8> out(n);
    for (int i = 0; i < n; ++i) { out[i] = rng.byte(); }
    return out;
}

/// Residual 0 for `period` samples, then +/-100 for `period`, from sample 1:
/// a row on which blocks of exactly `period` samples win.
QVector<quint8> burstRow(int n, int period)
{
    QVector<quint8> out(n);
    out[0] = 50;
    for (int i = 1; i < n; ++i) {
        const bool burst = ((i - 1) / period) % 2 == 1;
        const int step = burst ? ((i % 2) == 0 ? 100 : -100) : 0;
        out[i] = static_cast<quint8>(out.at(i - 1) + step);
    }
    return out;
}

DisplayCodecFrame frameOf(const QVector<quint8>& trace, const QVector<quint8>& waterfall,
                          const QVector<quint8>& wide, bool advance, quint32 sequence = 9)
{
    DisplayCodecFrame frame;
    frame.context.endpointId = 0x01020304U;
    frame.context.contextGeneration = 0x0A0B0C0DU;
    frame.context.minDbm = Vectors::kMinDbm;
    frame.context.maxDbm = Vectors::kMaxDbm;
    frame.context.traceSamples = static_cast<quint16>(trace.size());
    frame.context.waterfallSamples = static_cast<quint16>(waterfall.size());
    frame.context.wideSamples = static_cast<quint16>(wide.size());
    frame.encoderSequence = sequence;
    frame.producerTimestamp = 0x1122334455667788ULL;
    frame.waterfallAdvance = advance;
    frame.traceDbm = Vectors::dequantizeRow(trace);
    frame.waterfallDbm = Vectors::dequantizeRow(waterfall);
    frame.wideDbm = Vectors::dequantizeRow(wide);
    return frame;
}

quint32 u32At(const QByteArray& bytes, int offset)
{
    const auto* d = reinterpret_cast<const unsigned char*>(bytes.constData()) + offset;
    return (static_cast<quint32>(d[0]) << 24) | (static_cast<quint32>(d[1]) << 16)
        | (static_cast<quint32>(d[2]) << 8) | d[3];
}

quint16 u16At(const QByteArray& bytes, int offset)
{
    const auto* d = reinterpret_cast<const unsigned char*>(bytes.constData()) + offset;
    return static_cast<quint16>((static_cast<quint16>(d[0]) << 8) | d[1]);
}

quint8 flagsOf(const QByteArray& message)
{
    return static_cast<quint8>(message.at(5));
}

/// The header fields of a v2 message other than its flags, against `frame`.
void verifyHeader(const QByteArray& message, const DisplayCodecFrame& frame)
{
    QVERIFY(message.size() >= static_cast<int>(kHeader));
    QCOMPARE(u32At(message, 0), 0x4e534443U);
    QCOMPARE(static_cast<int>(static_cast<quint8>(message.at(4))), 2);
    QCOMPARE(u16At(message, 6), static_cast<quint16>(kHeader));
    QCOMPARE(u32At(message, 8), frame.context.endpointId);
    QCOMPARE(u32At(message, 12), frame.context.contextGeneration);
    QCOMPARE(u32At(message, 16), frame.encoderSequence);
    const quint64 timestamp = (static_cast<quint64>(u32At(message, 20)) << 32) | u32At(message, 24);
    QCOMPARE(timestamp, frame.producerTimestamp);
    quint32 minBits = 0;
    quint32 maxBits = 0;
    std::memcpy(&minBits, &frame.context.minDbm, sizeof(minBits));
    std::memcpy(&maxBits, &frame.context.maxDbm, sizeof(maxBits));
    QCOMPARE(u32At(message, 28), minBits);
    QCOMPARE(u32At(message, 32), maxBits);
    QCOMPARE(u16At(message, 36), frame.context.traceSamples);
    QCOMPARE(u16At(message, 38), frame.context.waterfallSamples);
    QCOMPARE(u16At(message, 40), frame.context.wideSamples);
}

/// Reads the planes after the header, in order, and requires the message to
/// end exactly at the last one.
void verifyPlanes(const QByteArray& message, const QList<QVector<quint8>>& expected)
{
    int offset = static_cast<int>(kHeader);
    for (const QVector<quint8>& plane : expected) {
        QVector<quint8> decoded;
        QVERIFY(decodeDisplayCodecV2Plane(message, offset, static_cast<int>(plane.size()), decoded));
        QCOMPARE(decoded, plane);
    }
    QCOMPARE(offset, static_cast<int>(message.size()));
}

QByteArray bitsToBytes(const QString& bits)
{
    QByteArray out((bits.size() + 7) / 8, '\0');
    for (int i = 0; i < bits.size(); ++i) {
        if (bits.at(i) == QLatin1Char('1')) {
            out[i / 8] = static_cast<char>(static_cast<unsigned char>(out.at(i / 8)) | (0x80U >> (i % 8)));
        }
    }
    return out;
}

QByteArray planeWithPayload(int blockCode, const QByteArray& payload)
{
    QByteArray plane = bytesOf({2, blockCode, static_cast<int>(payload.size() >> 8),
                                static_cast<int>(payload.size() & 0xff)});
    plane.append(payload);
    return plane;
}

constexpr quint32 v1Plane(quint32 n) { return displayCodecWorstCasePlaneBytes(n); }
constexpr quint32 v2Plane(quint32 n) { return displayCodecV2WorstCasePlaneBytes(n); }

} // namespace

class TstDisplayCodecV2 : public QObject {
    Q_OBJECT

private slots:
    void quantizeHelpersRoundTrip();
    void handCheckedPlanes();
    void riceParameterTiesAndEscape();
    void eachBlockSizeCanWin();
    void planeDecoderRejectsMalformed();
    void boundArithmetic();
    void noiseRoundTripsWithinTheBoundForEveryLength();
    void structuredRowsRoundTrip();
    void encodingIsDeterministic();
    void invalidFramesEncodeNothing();
    void wholeFrameLayout();
    void splitRule();
    void framesStayWithinTheirBound();
    void sceneSizes_data();
    void sceneSizes();
    void sceneRowIsAPureFunction();
    void goldenVectorsReencode();
};

void TstDisplayCodecV2::quantizeHelpersRoundTrip()
{
    for (int q = 0; q <= 255; ++q) {
        const float dbm = displayCodecDequantize(static_cast<quint8>(q), -150.0f, -30.0f);
        QCOMPARE(static_cast<int>(displayCodecQuantize(dbm, -150.0f, -30.0f)), q);
    }
    QCOMPARE(static_cast<int>(displayCodecQuantize(-200.0f, -150.0f, -30.0f)), 0);
    QCOMPARE(static_cast<int>(displayCodecQuantize(0.0f, -150.0f, -30.0f)), 255);
    QCOMPARE(displayCodecDequantize(0, -150.0f, -30.0f), -150.0f);
    QCOMPARE(displayCodecDequantize(255, -150.0f, -30.0f), -30.0f);
}

void TstDisplayCodecV2::handCheckedPlanes()
{
    // {100}: the first sample only.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({100})), bytesOf({0x02, 0x00, 0x00, 0x01, 0x64}));
    // {100, 101, 99}: z = 2 and 3. k = 1 (9 bits) ties k = 2 and wins as the
    // smaller; bits 01100100 001 010 011, then pad.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({100, 101, 99})),
             bytesOf({0x02, 0x00, 0x00, 0x03, 0x64, 0x29, 0x80}));
    // {0, 255}: residual -1, z = 1. k = 0 (01) ties k = 1 (11) and wins;
    // bits 00000000 000 01, then pad.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({0, 255})),
             bytesOf({0x02, 0x00, 0x00, 0x02, 0x00, 0x08}));

    for (const QVector<quint8>& row : {qRow({100}), qRow({100, 101, 99}), qRow({0, 255})}) {
        const QByteArray plane = encodeDisplayCodecV2Plane(row);
        int offset = 0;
        QVector<quint8> decoded;
        QVERIFY(decodeDisplayCodecV2Plane(plane, offset, static_cast<int>(row.size()), decoded));
        QCOMPARE(decoded, row);
        QCOMPARE(offset, static_cast<int>(plane.size()));
    }
}

void TstDisplayCodecV2::riceParameterTiesAndEscape()
{
    // {0, 32}: z = 64. k = 5 (00 1 00000) and k = 6 (01 000000) are both 8
    // bits, as is the escape: the smallest k wins and the escape is not
    // taken. Bits 00000000 101 00100000.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({0, 32})),
             bytesOf({0x02, 0x00, 0x00, 0x03, 0x00, 0xA4, 0x00}));
    // {0, 128}: residual -128, z = 255. Every k is more than 8 bits (k = 6 is
    // 10), so the escape: bits 00000000 111 10000000.
    QCOMPARE(encodeDisplayCodecV2Plane(qRow({0, 128})),
             bytesOf({0x02, 0x00, 0x00, 0x03, 0x00, 0xF0, 0x00}));
}

void TstDisplayCodecV2::eachBlockSizeCanWin()
{
    // Equal bytes at every size: the smallest (code 0).
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(qRow({100, 101, 99})).at(1)), 0);
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(burstRow(513, 16)).at(1)), 0);
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(burstRow(769, 32)).at(1)), 1);
    QCOMPARE(static_cast<int>(encodeDisplayCodecV2Plane(noiseRow(783, 77)).at(1)), 2);
}

void TstDisplayCodecV2::planeDecoderRejectsMalformed()
{
    const QByteArray good = encodeDisplayCodecV2Plane(qRow({100, 101, 99}));
    const auto rejects = [](const QByteArray& bytes, int length) {
        int offset = 0;
        QVector<quint8> samples = qRow({7});
        const bool ok = decodeDisplayCodecV2Plane(bytes, offset, length, samples);
        return !ok && offset == 0 && samples == qRow({7});
    };
    QVERIFY(!rejects(good, 3));

    QByteArray coding = good;
    coding[0] = 1;
    QVERIFY(rejects(coding, 3));
    coding[0] = 3;
    QVERIFY(rejects(coding, 3));
    QByteArray block = good;
    block[1] = 3;
    QVERIFY(rejects(block, 3));
    block[1] = static_cast<char>(0xFF);
    QVERIFY(rejects(block, 3));

    // A payload length past the end, and a prefix cut short.
    QByteArray overrun = good;
    overrun[3] = 4;
    QVERIFY(rejects(overrun, 3));
    QVERIFY(rejects(good.left(3), 3));
    for (int cut = 0; cut < good.size(); ++cut) { QVERIFY(rejects(good.left(cut), 3)); }

    // Payload bytes left after the last sample.
    QByteArray trailing = good;
    trailing[3] = 4;
    trailing.append('\0');
    QVERIFY(rejects(trailing, 3));
    // Fewer samples than coded leaves payload bytes too.
    QVERIFY(rejects(good, 2));
    // More samples than coded runs out of payload.
    QVERIFY(rejects(good, 4));

    // A nonzero pad bit: {0, 255} uses 13 bits of 16.
    QVERIFY(rejects(bytesOf({0x02, 0x00, 0x00, 0x02, 0x00, 0x09}), 2));
    QVERIFY(rejects(bytesOf({0x02, 0x00, 0x00, 0x02, 0x00, 0x0C}), 2));

    // z above 255: k = 0 with 256 zeros before the one.
    const QString zBits = QStringLiteral("00000000") + QStringLiteral("000")
        + QString(256, QLatin1Char('0')) + QStringLiteral("1");
    QVERIFY(rejects(planeWithPayload(0, bitsToBytes(zBits)), 2));
    // The same stream with 255 zeros is z = 255, which decodes.
    const QString maxBits = QStringLiteral("00000000") + QStringLiteral("000")
        + QString(255, QLatin1Char('0')) + QStringLiteral("1");
    int offset = 0;
    QVector<quint8> samples;
    QVERIFY(decodeDisplayCodecV2Plane(planeWithPayload(0, bitsToBytes(maxBits)), offset, 2, samples));
    QCOMPARE(samples, qRow({0, 128}));

    // Lengths outside 1..4096 and offsets outside the bytes.
    QVERIFY(rejects(good, 0));
    QVERIFY(rejects(good, DisplayCodecEncoder::kMaxSamplesPerPlane + 1));
    offset = static_cast<int>(good.size()) + 1;
    QVERIFY(!decodeDisplayCodecV2Plane(good, offset, 3, samples));
    offset = -1;
    QVERIFY(!decodeDisplayCodecV2Plane(good, offset, 3, samples));

    // A plane read at an offset advances past it only.
    QByteArray twoPlanes = good + encodeDisplayCodecV2Plane(qRow({0, 255}));
    offset = static_cast<int>(good.size());
    QVERIFY(decodeDisplayCodecV2Plane(twoPlanes, offset, 2, samples));
    QCOMPARE(samples, qRow({0, 255}));
    QCOMPARE(offset, static_cast<int>(twoPlanes.size()));

    QVERIFY(encodeDisplayCodecV2Plane({}).isEmpty());
    QVERIFY(encodeDisplayCodecV2Plane(QVector<quint8>(DisplayCodecEncoder::kMaxSamplesPerPlane + 1))
                .isEmpty());
}

void TstDisplayCodecV2::boundArithmetic()
{
    QCOMPARE(v2Plane(0), 0U);
    QCOMPARE(v2Plane(1), 5U);
    QCOMPARE(v2Plane(783), 792U);
    QCOMPARE(v2Plane(4096), 4124U);
    for (quint32 n = 1; n <= static_cast<quint32>(DisplayCodecEncoder::kMaxSamplesPerPlane); ++n) {
        const quint32 blocks = (n - 1 + 63) / 64;
        QCOMPARE(v2Plane(n), 4 + n + (3 * blocks + 7) / 8);
        QVERIFY2(v2Plane(n) <= v1Plane(n), qPrintable(QString::number(n)));
    }
    static_assert(displayCodecV2WorstCasePlaneBytes(783) == 792);
    static_assert(displayCodecV2WorstCasePlaneBytes(4096) == 4124);
}

void TstDisplayCodecV2::noiseRoundTripsWithinTheBoundForEveryLength()
{
    Vectors::SplitMix64 rng(1234);
    for (int n = 1; n <= DisplayCodecEncoder::kMaxSamplesPerPlane; ++n) {
        QVector<quint8> row(n);
        for (int i = 0; i < n; ++i) { row[i] = rng.byte(); }
        const QByteArray plane = encodeDisplayCodecV2Plane(row);
        QVERIFY2(static_cast<quint32>(plane.size()) <= v2Plane(static_cast<quint32>(n)),
                 qPrintable(QStringLiteral("n %1: %2 bytes").arg(n).arg(plane.size())));
        int offset = 0;
        QVector<quint8> decoded;
        QVERIFY2(decodeDisplayCodecV2Plane(plane, offset, n, decoded), qPrintable(QString::number(n)));
        QVERIFY2(decoded == row, qPrintable(QString::number(n)));
        QCOMPARE(offset, static_cast<int>(plane.size()));
    }
}

void TstDisplayCodecV2::structuredRowsRoundTrip()
{
    for (int n : {1, 2, 3, 16, 17, 64, 65, 129, 783, 1024, 4096}) {
        QVector<quint8> equal(n, 93);
        QVector<quint8> ramp(n);
        QVector<quint8> alternating(n);
        QVector<quint8> full(n, 255);
        for (int i = 0; i < n; ++i) {
            ramp[i] = static_cast<quint8>(i & 255);
            alternating[i] = (i % 2) == 0 ? 0 : 255;
        }
        for (const QVector<quint8>& row : {equal, ramp, alternating, full, burstRow(n, 16)}) {
            const QByteArray plane = encodeDisplayCodecV2Plane(row);
            QVERIFY(static_cast<quint32>(plane.size()) <= v2Plane(static_cast<quint32>(n)));
            int offset = 0;
            QVector<quint8> decoded;
            QVERIFY(decodeDisplayCodecV2Plane(plane, offset, n, decoded));
            QCOMPARE(decoded, row);
        }
    }
}

void TstDisplayCodecV2::encodingIsDeterministic()
{
    const QVector<quint8> row = noiseRow(783, 5);
    QCOMPARE(encodeDisplayCodecV2Plane(row), encodeDisplayCodecV2Plane(row));
    Vectors::SceneRun run(Vectors::Scene::PeakBins167);
    QVector<float> trace;
    QVector<float> waterfall;
    run.next(trace, waterfall);
    const DisplayCodecFrame frame =
        frameOf(Vectors::quantizeRow(trace), Vectors::quantizeRow(waterfall), {}, true);
    const quint32 admitted = spectrumDisplayCost(783, 30, false)->maximumFrameBytes;
    QCOMPARE(encodeDisplayCodecV2(frame, admitted).messages,
             encodeDisplayCodecV2(frame, admitted).messages);
}

void TstDisplayCodecV2::invalidFramesEncodeNothing()
{
    const DisplayCodecFrame good = frameOf(qRow({1, 2, 3}), qRow({4, 5, 6}), {}, true);
    QCOMPARE(encodeDisplayCodecV2(good, 10'000).messages.size(), size_t {1});

    DisplayCodecFrame frame = good;
    frame.traceDbm[1] = std::numeric_limits<float>::quiet_NaN();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.waterfallDbm.removeLast();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.maxDbm = frame.context.minDbm;
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.minDbm = std::numeric_limits<float>::infinity();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.traceSamples = 0;
    frame.traceDbm.clear();
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.wideSamples = 2;
    frame.wideDbm = {-100.0f};
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
    frame = good;
    frame.context.waterfallSamples = DisplayCodecEncoder::kMaxSamplesPerPlane + 1;
    frame.waterfallDbm = QVector<float>(DisplayCodecEncoder::kMaxSamplesPerPlane + 1, -100.0f);
    QVERIFY(encodeDisplayCodecV2(frame, 10'000).messages.empty());
}

void TstDisplayCodecV2::wholeFrameLayout()
{
    const QVector<quint8> trace = qRow({10, 20, 30, 40, 50});
    const QVector<quint8> waterfall = qRow({200, 190, 180, 170});
    const QVector<quint8> wide = qRow({1, 2, 3});

    // Trace and waterfall: 0x1A, planes in order, the message ending at the
    // waterfall plane.
    DisplayCodecFrame frame = frameOf(trace, waterfall, {}, true);
    DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QVERIFY(!encoding.split());
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1A});
    verifyHeader(encoding.messages[0], frame);
    verifyPlanes(encoding.messages[0], {trace, waterfall});
    QCOMPARE(encoding.totalBytes(), encoding.messages[0].size());

    // With a wide row: 0x1E and three planes.
    frame = frameOf(trace, waterfall, wide, true);
    encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1E});
    verifyHeader(encoding.messages[0], frame);
    verifyPlanes(encoding.messages[0], {trace, waterfall, wide});

    // No waterfall advance: 0x08, the trace only, the header still carrying
    // every row length.
    frame = frameOf(trace, waterfall, wide, false);
    encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x08});
    verifyHeader(encoding.messages[0], frame);
    verifyPlanes(encoding.messages[0], {trace});

    // The exact bytes of a small whole frame.
    frame = frameOf(qRow({100}), qRow({0, 255}), {}, true, 0x01020304U);
    frame.producerTimestamp = 5;
    const QByteArray expected = bytesOf({0x4E, 0x53, 0x44, 0x43, 0x02, 0x1A, 0x00, 0x2A,
                                         0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D,
                                         0x01, 0x02, 0x03, 0x04,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
                                         0xC3, 0x16, 0x00, 0x00, 0xC1, 0xF0, 0x00, 0x00,
                                         0x00, 0x01, 0x00, 0x02, 0x00, 0x00,
                                         0x02, 0x00, 0x00, 0x01, 0x64,
                                         0x02, 0x00, 0x00, 0x02, 0x00, 0x08});
    encoding = encodeDisplayCodecV2(frame, 10'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(encoding.messages[0], expected);
}

void TstDisplayCodecV2::splitRule()
{
    static_assert(IMediaTransport::kSctpDataPayloadBytes == 876);
    QVector<float> traceDbm;
    QVector<float> waterfallDbm;

    // A 783-pixel peak-scene frame splits within the budget's admitted cost:
    // the row part (0x32) first, then the trace part (0x2A), both with the
    // frame's header fields.
    Vectors::SceneRun peak(Vectors::Scene::PeakBins167);
    peak.next(traceDbm, waterfallDbm);
    const QVector<quint8> trace = Vectors::quantizeRow(traceDbm);
    const QVector<quint8> waterfall = Vectors::quantizeRow(waterfallDbm);
    const quint32 admitted = spectrumDisplayCost(783, 30, false)->maximumFrameBytes;
    DisplayCodecFrame frame = frameOf(trace, waterfall, {}, true);
    DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
    QVERIFY(encoding.split());
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x32});
    QCOMPARE(flagsOf(encoding.messages[1]), quint8 {0x2A});
    verifyHeader(encoding.messages[0], frame);
    verifyHeader(encoding.messages[1], frame);
    verifyPlanes(encoding.messages[0], {waterfall});
    verifyPlanes(encoding.messages[1], {trace});
    QVERIFY(encoding.totalBytes() <= admitted);
    QCOMPARE(encoding.totalBytes(),
             encodeDisplayCodecV2(frame, 0).totalBytes() + static_cast<qsizetype>(kHeader));

    // With a wide row: 0x36 then 0x2A.
    Vectors::SceneRun wideRun(Vectors::Scene::PeakBins167, 768, 99);
    QVector<float> wideDbm;
    QVector<float> unused;
    wideRun.next(unused, wideDbm);
    const QVector<quint8> wide = Vectors::quantizeRow(wideDbm);
    frame = frameOf(trace, waterfall, wide, true);
    encoding = encodeDisplayCodecV2(frame, spectrumDisplayCost(783, 30, true)->maximumFrameBytes);
    QVERIFY(encoding.split());
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x36});
    QCOMPARE(flagsOf(encoding.messages[1]), quint8 {0x2A});
    verifyPlanes(encoding.messages[0], {waterfall, wide});
    verifyPlanes(encoding.messages[1], {trace});

    // Too big for one datagram but the parts over the admitted cost: whole.
    frame = frameOf(trace, waterfall, {}, true);
    const qsizetype whole = encodeDisplayCodecV2(frame, 0).totalBytes();
    QVERIFY(whole > IMediaTransport::kSctpDataPayloadBytes);
    encoding = encodeDisplayCodecV2(frame, static_cast<quint32>(whole + kHeader - 1));
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1A});
    encoding = encodeDisplayCodecV2(frame, static_cast<quint32>(whole + kHeader));
    QVERIFY(encoding.split());

    // A flat 783-pixel frame fits one datagram whole.
    Vectors::SceneRun flat(Vectors::Scene::Flat);
    flat.next(traceDbm, waterfallDbm);
    frame = frameOf(Vectors::quantizeRow(traceDbm), Vectors::quantizeRow(waterfallDbm), {}, true);
    encoding = encodeDisplayCodecV2(frame, admitted);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x1A});
    QVERIFY(encoding.totalBytes() <= IMediaTransport::kSctpDataPayloadBytes);

    // A 600-pixel uniform-noise frame admitted exactly its whole size stays
    // whole.
    frame = frameOf(noiseRow(600, 11), noiseRow(600, 12), {}, true);
    const qsizetype noiseWhole = encodeDisplayCodecV2(frame, 0).totalBytes();
    QVERIFY(noiseWhole > IMediaTransport::kSctpDataPayloadBytes);
    encoding = encodeDisplayCodecV2(frame, static_cast<quint32>(noiseWhole));
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(encoding.totalBytes(), noiseWhole);

    // A trace-only frame is one message however large.
    frame = frameOf(noiseRow(4096, 13), noiseRow(4096, 14), {}, false);
    encoding = encodeDisplayCodecV2(frame, 100'000);
    QCOMPARE(encoding.messages.size(), size_t {1});
    QCOMPARE(flagsOf(encoding.messages[0]), quint8 {0x08});
    QVERIFY(encoding.totalBytes() > IMediaTransport::kSctpDataPayloadBytes);
}

void TstDisplayCodecV2::framesStayWithinTheirBound()
{
    // Noise rows at every length, with wide 0 and 768: the total never
    // passes what the budget admits, and a whole message never passes the v1
    // keyframe of its shape.
    const QVector<quint8> wide768 = noiseRow(SpectrumEndpoint::kMaxWideSamples, 21);
    Vectors::SplitMix64 rng(1234);
    for (int n = 1; n <= DisplayCodecEncoder::kMaxSamplesPerPlane; ++n) {
        QVector<quint8> trace(n);
        QVector<quint8> waterfall(n);
        for (int i = 0; i < n; ++i) {
            trace[i] = rng.byte();
            waterfall[i] = rng.byte();
        }
        for (const bool withWide : {false, true}) {
            const QVector<quint8> wide = withWide ? wide768 : QVector<quint8>();
            const DisplayCodecFrame frame = frameOf(trace, waterfall, wide, true);
            const quint32 admitted = spectrumDisplayCost(n, 30, withWide)->maximumFrameBytes;
            const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
            const quint32 keyframe = kHeader + 2 * v1Plane(static_cast<quint32>(n))
                + v1Plane(static_cast<quint32>(wide.size()));
            QVERIFY2(!encoding.messages.empty(), qPrintable(QString::number(n)));
            QVERIFY2(encoding.totalBytes() <= static_cast<qsizetype>(admitted),
                     qPrintable(QStringLiteral("n %1 wide %2").arg(n).arg(wide.size())));
            if (!encoding.split()) {
                QVERIFY2(encoding.totalBytes() <= static_cast<qsizetype>(keyframe),
                         qPrintable(QString::number(n)));
            }
        }
    }

    // Trace 783 with every wide length: admitted by the budget (which
    // charges A(768) whatever the wide length) and by the frame's own shape.
    const QVector<quint8> trace = noiseRow(783, 31);
    const QVector<quint8> waterfall = noiseRow(783, 32);
    const quint32 budget = spectrumDisplayCost(783, 30, true)->maximumFrameBytes;
    for (int w = 1; w <= SpectrumEndpoint::kMaxWideSamples; ++w) {
        const DisplayCodecFrame frame = frameOf(trace, waterfall, noiseRow(w, 1000 + w), true);
        const quint32 own = kHeader + 2 * v1Plane(783) + v1Plane(static_cast<quint32>(w));
        for (const quint32 admitted : {budget, own}) {
            const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
            QVERIFY2(encoding.totalBytes() <= static_cast<qsizetype>(admitted),
                     qPrintable(QStringLiteral("wide %1").arg(w)));
            for (const QByteArray& message : encoding.messages) {
                QVERIFY(static_cast<quint32>(message.size()) <= own);
            }
        }
    }
}

void TstDisplayCodecV2::sceneSizes_data()
{
    QTest::addColumn<int>("scene");
    QTest::addColumn<double>("bytes");
    using S = Vectors::Scene;
    QTest::newRow("peak 167") << static_cast<int>(S::PeakBins167) << 930.0;
    QTest::newRow("peak trace averaged") << static_cast<int>(S::PeakBins167TraceAveraged) << 784.0;
    QTest::newRow("average 167") << static_cast<int>(S::AverageBins167) << 757.0;
    QTest::newRow("average trace averaged") << static_cast<int>(S::AverageBins167TraceAveraged)
                                            << 710.0;
    QTest::newRow("peak IIR 0.3") << static_cast<int>(S::PeakIir) << 743.0;
    QTest::newRow("average 8") << static_cast<int>(S::AverageBins8) << 1064.0;
    QTest::newRow("average 8 trace averaged") << static_cast<int>(S::AverageBins8TraceAveraged)
                                              << 949.0;
    QTest::newRow("raw") << static_cast<int>(S::Raw) << 1312.0;
    QTest::newRow("uniform noise") << static_cast<int>(S::UniformNoise) << 1668.0;
    QTest::newRow("flat") << static_cast<int>(S::Flat) << 258.0;
    QTest::newRow("ramp") << static_cast<int>(S::Ramp) << 386.0;
    QTest::newRow("1024 px average") << static_cast<int>(S::Average1024) << 988.0;
    QTest::newRow("4096 px peak") << static_cast<int>(S::Peak4096) << 4821.0;
}

void TstDisplayCodecV2::sceneSizes()
{
    QFETCH(int, scene);
    QFETCH(double, bytes);
    constexpr int kFrames = 300;
    Vectors::SceneRun run(static_cast<Vectors::Scene>(scene));
    const int pixels = run.pixels();
    const quint32 admitted = spectrumDisplayCost(pixels, 30, false)->maximumFrameBytes;
    QVector<float> trace;
    QVector<float> waterfall;
    double total = 0.0;
    for (int f = 0; f < kFrames; ++f) {
        run.next(trace, waterfall);
        DisplayCodecFrame frame;
        frame.context.endpointId = 1;
        frame.context.contextGeneration = 1;
        frame.context.minDbm = Vectors::kMinDbm;
        frame.context.maxDbm = Vectors::kMaxDbm;
        frame.context.traceSamples = static_cast<quint16>(pixels);
        frame.context.waterfallSamples = static_cast<quint16>(pixels);
        frame.encoderSequence = static_cast<quint32>(f + 1);
        frame.producerTimestamp = static_cast<quint64>(f + 1);
        frame.waterfallAdvance = true;
        frame.traceDbm = trace;
        frame.waterfallDbm = waterfall;
        const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
        QVERIFY(!encoding.messages.empty());
        QVERIFY(encoding.totalBytes() <= static_cast<qsizetype>(admitted));
        total += static_cast<double>(encoding.totalBytes());
    }
    const double mean = total / kFrames;
    QVERIFY2(mean <= 1.03 * bytes,
             qPrintable(QStringLiteral("mean %1 bytes against %2").arg(mean, 0, 'f', 1).arg(bytes)));
}

void TstDisplayCodecV2::sceneRowIsAPureFunction()
{
    using S = Vectors::Scene;
    QCOMPARE(Vectors::sceneRow(S::PeakBins167, 783, 1234, 17),
             Vectors::sceneRow(S::PeakBins167, 783, 1234, 17));
    QVERIFY(Vectors::sceneRow(S::PeakBins167, 783, 1234, 17)
            != Vectors::sceneRow(S::PeakBins167, 783, 1234, 18));
    QVERIFY(Vectors::sceneRow(S::PeakBins167, 783, 1234, 17)
            != Vectors::sceneRow(S::PeakBins167, 783, 1235, 17));
    // splitmix64's first outputs from 0 (its published reference values).
    Vectors::SplitMix64 rng(0);
    QCOMPARE(rng.next(), 0xE220A8397B1DCDAFULL);
    QCOMPARE(rng.next(), 0x6E789E6AA1B965F4ULL);
}

void TstDisplayCodecV2::goldenVectorsReencode()
{
    QJsonObject document;
    QString error;
    QVERIFY2(Vectors::readVectors(Vectors::vectorsPath(), document, error), qPrintable(error));

    const QJsonArray planes = document.value(QStringLiteral("planes")).toArray();
    QSet<QString> planeNames;
    QSet<int> blockSizes;
    for (const QJsonValue& value : planes) {
        const QJsonObject plane = value.toObject();
        const QString name = plane.value(QStringLiteral("name")).toString();
        planeNames.insert(name);
        const QVector<quint8> samples =
            Vectors::fromJsonArray(plane.value(QStringLiteral("samples")).toArray());
        const QByteArray encoded = encodeDisplayCodecV2Plane(samples);
        QVERIFY2(Vectors::toHex(encoded) == plane.value(QStringLiteral("plane")).toString(),
                 qPrintable(name));
        const int blockSize = plane.value(QStringLiteral("blockSize")).toInt();
        QCOMPARE(blockSize, 16 << static_cast<int>(encoded.at(1)));
        blockSizes.insert(blockSize);
        int offset = 0;
        QVector<quint8> decoded;
        QVERIFY2(decodeDisplayCodecV2Plane(encoded, offset, static_cast<int>(samples.size()), decoded),
                 qPrintable(name));
        QCOMPARE(decoded, samples);
    }
    for (const char* name : {"hand-one-sample", "hand-three-samples", "hand-wrap"}) {
        QVERIFY2(planeNames.contains(QLatin1String(name)), name);
    }
    QCOMPARE(blockSizes, (QSet<int> {16, 32, 64}));

    const QJsonArray frames = document.value(QStringLiteral("frames")).toArray();
    QSet<QList<int>> flagShapes;
    for (const QJsonValue& value : frames) {
        const QJsonObject entry = value.toObject();
        const QString name = entry.value(QStringLiteral("name")).toString();
        const DisplayCodecFrame frame =
            Vectors::frameFromJson(entry.value(QStringLiteral("frame")).toObject());
        const auto admitted =
            static_cast<quint32>(entry.value(QStringLiteral("admittedFrameBytes")).toInteger());
        const DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, admitted);
        const QJsonArray expected = entry.value(QStringLiteral("messages")).toArray();
        QVERIFY2(Vectors::toJsonHexArray(encoding.messages) == expected, qPrintable(name));
        QList<int> shape;
        for (const QByteArray& message : encoding.messages) {
            shape.append(flagsOf(message));
            // The builder the hand-stated sequences use writes the same bytes.
            QVERIFY2(Vectors::buildMessage(frame, flagsOf(message)) == message, qPrintable(name));
        }
        flagShapes.insert(shape);
    }
    for (const QList<int>& shape : {QList<int> {0x08}, QList<int> {0x1A}, QList<int> {0x1E},
                                    QList<int> {0x32, 0x2A}, QList<int> {0x36, 0x2A}}) {
        QVERIFY(flagShapes.contains(shape));
    }

    const QJsonArray sequences = document.value(QStringLiteral("sequences")).toArray();
    QVERIFY(!sequences.isEmpty());
    for (const QJsonValue& value : sequences) {
        const QJsonObject entry = value.toObject();
        QCOMPARE(entry.value(QStringLiteral("messages")).toArray().size(),
                 entry.value(QStringLiteral("results")).toArray().size());
        QVERIFY(entry.value(QStringLiteral("counters")).isObject());
    }
}

QTEST_APPLESS_MAIN(TstDisplayCodecV2)
#include "tst_display_codec_v2.moc"
