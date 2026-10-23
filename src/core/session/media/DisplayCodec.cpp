// =================================================================
// src/core/session/media/DisplayCodec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 3.
//
// See DisplayCodec.h, docs/architecture/2026-09-20-display-codec-v1.md and
// docs/architecture/2026-10-22-display-codec-v2-intra-rows.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2 encoder: previous-bin Rice planes, the whole or
//               split frame, and public quantize helpers.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include "core/session/media/DisplayCodec.h"

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/IMediaTransport.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <utility>

namespace NereusSDR {
namespace {

constexpr quint32 kMagic = 0x4e534443U; // "NSDC"
constexpr quint8 kVersion = 1;
constexpr quint16 kHeaderBytes = DisplayCodecEncoder::kHeaderBytes;
constexpr quint8 kFlagKeyframe = 0x01;
constexpr quint8 kFlagWaterfallAdvance = 0x02;
constexpr quint8 kFlagWide = 0x04;
constexpr quint8 kKnownFlags = kFlagKeyframe | kFlagWaterfallAdvance | kFlagWide;
constexpr int kBlockSizes[] = {16, 32, 64, 128};

class Writer {
public:
    void u8(quint8 value) { m_bytes.append(static_cast<char>(value)); }
    void u16(quint16 value) {
        u8(static_cast<quint8>(value >> 8));
        u8(static_cast<quint8>(value));
    }
    void u32(quint32 value) {
        u8(static_cast<quint8>(value >> 24));
        u8(static_cast<quint8>(value >> 16));
        u8(static_cast<quint8>(value >> 8));
        u8(static_cast<quint8>(value));
    }
    void u64(quint64 value) {
        u32(static_cast<quint32>(value >> 32));
        u32(static_cast<quint32>(value));
    }
    void f32(float value) {
        quint32 bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }
    void bytes(const QByteArray& value) { m_bytes.append(value); }
    QByteArray take() { return std::move(m_bytes); }

private:
    QByteArray m_bytes;
};

class Reader {
public:
    explicit Reader(const QByteArray& bytes) : m_bytes(bytes) {}

    bool u8(quint8& value) {
        if (m_pos >= m_bytes.size()) { return false; }
        value = static_cast<quint8>(static_cast<unsigned char>(m_bytes.at(m_pos++)));
        return true;
    }
    bool u16(quint16& value) {
        quint8 a = 0, b = 0;
        if (!u8(a) || !u8(b)) { return false; }
        value = (static_cast<quint16>(a) << 8) | b;
        return true;
    }
    bool u32(quint32& value) {
        quint8 a = 0, b = 0, c = 0, d = 0;
        if (!u8(a) || !u8(b) || !u8(c) || !u8(d)) { return false; }
        value = (static_cast<quint32>(a) << 24) | (static_cast<quint32>(b) << 16)
              | (static_cast<quint32>(c) << 8) | d;
        return true;
    }
    bool u64(quint64& value) {
        quint32 high = 0, low = 0;
        if (!u32(high) || !u32(low)) { return false; }
        value = (static_cast<quint64>(high) << 32) | low;
        return true;
    }
    bool f32(float& value) {
        quint32 bits = 0;
        if (!u32(bits)) { return false; }
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }
    bool skip(int count) {
        if (count < 0 || count > m_bytes.size() - m_pos) { return false; }
        m_pos += count;
        return true;
    }
    int position() const { return m_pos; }
    bool atEnd() const { return m_pos == m_bytes.size(); }

private:
    const QByteArray& m_bytes;
    int m_pos {0};
};

bool finite(float value)
{
    return std::isfinite(static_cast<double>(value));
}

bool validContext(const DisplayCodecContext& context)
{
    const auto validRequiredLength = [](quint16 length) {
        return length > 0 && length <= DisplayCodecEncoder::kMaxSamplesPerPlane;
    };
    return finite(context.minDbm) && finite(context.maxDbm)
        && context.maxDbm > context.minDbm
        && validRequiredLength(context.traceSamples)
        && validRequiredLength(context.waterfallSamples)
        && context.wideSamples <= DisplayCodecEncoder::kMaxSamplesPerPlane;
}

bool sameContext(const DisplayCodecContext& left, const DisplayCodecContext& right)
{
    return left.endpointId == right.endpointId
        && left.contextGeneration == right.contextGeneration
        && left.minDbm == right.minDbm && left.maxDbm == right.maxDbm
        && left.traceSamples == right.traceSamples
        && left.waterfallSamples == right.waterfallSamples
        && left.wideSamples == right.wideSamples;
}

bool staleOrEqual(quint32 candidate, quint32 accepted)
{
    const quint32 difference = candidate - accepted;
    return difference == 0 || difference >= 0x80000000U;
}

quint8 quantize(float value, const DisplayCodecContext& context)
{
    return displayCodecQuantize(value, context.minDbm, context.maxDbm);
}

float dequantize(quint8 value, const DisplayCodecContext& context)
{
    return displayCodecDequantize(value, context.minDbm, context.maxDbm);
}

bool validInputPlane(const QVector<float>& samples, quint16 expected)
{
    if (samples.size() != expected) { return false; }
    return std::all_of(samples.cbegin(), samples.cend(), [](float sample) {
        return finite(sample);
    });
}

int residualWidth(const QVector<quint8>& values, const QVector<quint8>& previous,
                  int first, int count)
{
    int minimum = 0;
    int maximum = 0;
    for (int i = 0; i < count; ++i) {
        const int residual = static_cast<int>(values.at(first + i))
            - static_cast<int>(previous.at(first + i));
        minimum = std::min(minimum, residual);
        maximum = std::max(maximum, residual);
    }
    for (int width = 1; width <= 8; ++width) {
        const int bias = 1 << (width - 1);
        if (minimum >= -bias && maximum <= bias - 1) { return width; }
    }
    return 0; // absolute fallback
}

int payloadBytes(int count, int bits)
{
    return (count * bits + 7) / 8;
}

int chooseBlockSize(const QVector<quint8>& values, const QVector<quint8>* previous,
                    bool keyframe)
{
    int chosen = kBlockSizes[0];
    int bestCost = std::numeric_limits<int>::max();
    for (int candidate : kBlockSizes) {
        int cost = 3; // plane prefix
        for (int first = 0; first < values.size(); first += candidate) {
            const int count = std::min(candidate, static_cast<int>(values.size()) - first);
            int width = 0;
            if (!keyframe) { width = residualWidth(values, *previous, first, count); }
            const int dataBytes = width == 0 ? count : payloadBytes(count, width);
            cost += 5 + dataBytes;
        }
        if (cost < bestCost) {
            bestCost = cost;
            chosen = candidate;
        }
    }
    return chosen;
}

QByteArray packResiduals(const QVector<quint8>& values, const QVector<quint8>& previous,
                         int first, int count, int width)
{
    QByteArray packed(payloadBytes(count, width), '\0');
    const int bias = 1 << (width - 1);
    int bit = 0;
    for (int i = 0; i < count; ++i) {
        const int residual = static_cast<int>(values.at(first + i))
            - static_cast<int>(previous.at(first + i));
        const unsigned value = static_cast<unsigned>(residual + bias);
        for (int shift = width - 1; shift >= 0; --shift, ++bit) {
            if ((value & (1U << shift)) != 0U) {
                packed[bit / 8] = static_cast<char>(
                    static_cast<unsigned char>(packed.at(bit / 8)) | (1U << (7 - (bit % 8))));
            }
        }
    }
    return packed;
}

void writePlane(Writer& writer, const QVector<quint8>& values,
                const QVector<quint8>* previous, bool keyframe)
{
    const int blockSize = chooseBlockSize(values, previous, keyframe);
    int code = 0;
    while (kBlockSizes[code] != blockSize) { ++code; }
    writer.u8(static_cast<quint8>(code));
    writer.u16(static_cast<quint16>((values.size() + blockSize - 1) / blockSize));

    for (int first = 0; first < values.size(); first += blockSize) {
        const int count = std::min(blockSize, static_cast<int>(values.size()) - first);
        int width = keyframe ? 0 : residualWidth(values, *previous, first, count);
        if (width == 0) {
            writer.u8(1); // absolute
            writer.u8(8);
            writer.u8(static_cast<quint8>(count));
            writer.u16(static_cast<quint16>(count));
            for (int i = 0; i < count; ++i) { writer.u8(values.at(first + i)); }
            continue;
        }
        const QByteArray payload = packResiduals(values, *previous, first, count, width);
        writer.u8(0); // residual
        writer.u8(static_cast<quint8>(width));
        writer.u8(static_cast<quint8>(count));
        writer.u16(static_cast<quint16>(payload.size()));
        writer.bytes(payload);
    }
}

int unpackResidual(const unsigned char* payload, int sample, int width)
{
    unsigned packed = 0;
    const int firstBit = sample * width;
    for (int bit = 0; bit < width; ++bit) {
        const int offset = firstBit + bit;
        const unsigned byte = payload[offset / 8];
        packed = (packed << 1) | ((byte >> (7 - (offset % 8))) & 1U);
    }
    return static_cast<int>(packed) - (1 << (width - 1));
}

// The structure pass: every plane check that needs no decoder state (block
// size code, block count against the declared length, block sample counts,
// mode, bit width, payload length, overrun and truncation). It records where
// the plane starts and copies nothing.
bool checkPlane(Reader& reader, int expectedLength, bool keyframe, int& planeStart)
{
    planeStart = reader.position();
    quint8 blockCode = 0;
    quint16 blockCount = 0;
    if (!reader.u8(blockCode) || !reader.u16(blockCount) || blockCode >= 4) { return false; }
    const int blockSize = kBlockSizes[blockCode];
    const int expectedBlocks = (expectedLength + blockSize - 1) / blockSize;
    if (blockCount != expectedBlocks) { return false; }

    int first = 0;
    for (int block = 0; block < blockCount; ++block) {
        quint8 mode = 0, width = 0, count = 0;
        quint16 bytes = 0;
        if (!reader.u8(mode) || !reader.u8(width) || !reader.u8(count) || !reader.u16(bytes)) {
            return false;
        }
        const int expectedCount = std::min(blockSize, expectedLength - first);
        if (count != expectedCount || count == 0 || (mode != 0 && mode != 1)) { return false; }
        if (mode == 1) {
            if (width != 8 || bytes != count) { return false; }
        } else if (keyframe || width == 0 || width > 8 || bytes != payloadBytes(count, width)) {
            return false;
        }
        if (!reader.skip(bytes)) { return false; }
        first += count;
    }
    return first == expectedLength;
}

// The apply pass, over a plane checkPlane accepted: reads the block headers
// it already checked and reconstructs from the payload in place. The only
// failures left need history: a delta without a matching previous plane,
// and a residual that takes a value outside 0..255.
bool applyPlane(const QByteArray& packet, int planeStart, int expectedLength,
                const QVector<quint8>* previous, QVector<quint8>& reconstructed)
{
    const auto* data = reinterpret_cast<const unsigned char*>(packet.constData());
    const int blockCount = (static_cast<int>(data[planeStart + 1]) << 8) | data[planeStart + 2];
    int position = planeStart + 3;
    reconstructed.resize(expectedLength);
    int first = 0;
    for (int block = 0; block < blockCount; ++block) {
        const int mode = data[position];
        const int width = data[position + 1];
        const int count = data[position + 2];
        const int bytes = (static_cast<int>(data[position + 3]) << 8) | data[position + 4];
        const unsigned char* payload = data + position + 5;
        if (mode == 1) {
            for (int i = 0; i < count; ++i) { reconstructed[first + i] = payload[i]; }
        } else {
            if (!previous || previous->size() != expectedLength) { return false; }
            for (int i = 0; i < count; ++i) {
                const int value = static_cast<int>(previous->at(first + i))
                    + unpackResidual(payload, i, width);
                if (value < 0 || value > 255) { return false; }
                reconstructed[first + i] = static_cast<quint8>(value);
            }
        }
        position += 5 + bytes;
        first += count;
    }
    return true;
}

DisplayCodecDecodeResult result(DisplayCodecDisposition disposition, DisplayCodecReason reason)
{
    DisplayCodecDecodeResult decoded;
    decoded.disposition = disposition;
    decoded.reason = reason;
    return decoded;
}

// ---- NSDC v2 (docs/architecture/2026-10-22-display-codec-v2-intra-rows.md) ----

constexpr quint8 kVersion2 = 2;
constexpr quint8 kV2FlagWaterfallAdvance = 0x02;
constexpr quint8 kV2FlagWide = 0x04;
constexpr quint8 kV2FlagTrace = 0x08;
constexpr quint8 kV2FlagWaterfall = 0x10;
constexpr quint8 kV2FlagSplit = 0x20;
// The flag bytes the encoder writes (section 4.1).
constexpr quint8 kV2FlagsTraceOnly = kV2FlagTrace;                                   // 0x08
constexpr quint8 kV2FlagsWhole = kV2FlagTrace | kV2FlagWaterfall | kV2FlagWaterfallAdvance; // 0x1A
constexpr quint8 kV2FlagsRowPart = kV2FlagSplit | kV2FlagWaterfall | kV2FlagWaterfallAdvance; // 0x32
constexpr quint8 kV2FlagsTracePart = kV2FlagSplit | kV2FlagTrace | kV2FlagWaterfallAdvance;  // 0x2A
static_assert(kV2FlagsWhole == 0x1A && kV2FlagsRowPart == 0x32 && kV2FlagsTracePart == 0x2A);
constexpr quint8 kV2PlaneCoding = 2;
constexpr int kV2PlanePrefixBytes = 4;
constexpr int kV2BlockSizes[] = {16, 32, 64};
constexpr int kV2BlockSizeCount = 3;
constexpr int kV2RiceParameterBits = 3;
constexpr int kV2MaxRiceParameter = 6;
constexpr int kV2RawEscape = 7;

// The largest v2 frame (all three planes at their maximum length) stays well
// under the decoder's message limit, so the encoder never needs to refuse a
// valid frame for its size.
static_assert(DisplayCodecEncoder::kHeaderBytes
                  + 3 * displayCodecV2WorstCasePlaneBytes(DisplayCodecEncoder::kMaxSamplesPerPlane)
              <= DisplayCodecEncoder::kMaxEncodedBytes);

// Zigzag of the wrapping 8-bit residual x[i] - x[i-1], read as signed.
quint8 zigzagResidual(quint8 value, quint8 previous)
{
    const auto residual = static_cast<qint8>(static_cast<quint8>(value - previous));
    return residual >= 0 ? static_cast<quint8>(2 * residual)
                         : static_cast<quint8>(-2 * residual - 1);
}

// Bits of one block coded with Rice parameter k (0..6): per sample z >> k
// zeros, a one and k low bits.
int riceBlockBits(const quint8* zigzag, int count, int k)
{
    int bits = count * (1 + k);
    for (int i = 0; i < count; ++i) { bits += zigzag[i] >> k; }
    return bits;
}

// The fewest-bits coding of one block: ties go to the smaller k, and the raw
// escape only when it is strictly fewer bits than every k.
int chooseRiceParameter(const quint8* zigzag, int count, int& bits)
{
    int bestK = 0;
    int bestBits = riceBlockBits(zigzag, count, 0);
    for (int k = 1; k <= kV2MaxRiceParameter; ++k) {
        const int candidate = riceBlockBits(zigzag, count, k);
        if (candidate < bestBits) {
            bestBits = candidate;
            bestK = k;
        }
    }
    const int rawBits = 8 * count;
    if (rawBits < bestBits) {
        bestBits = rawBits;
        bestK = kV2RawEscape;
    }
    bits = bestBits;
    return bestK;
}

class BitWriter {
public:
    void put(quint32 value, int bits)
    {
        for (int bit = bits - 1; bit >= 0; --bit) {
            m_current = static_cast<quint8>((m_current << 1) | ((value >> bit) & 1U));
            if (++m_used == 8) {
                m_bytes.append(static_cast<char>(m_current));
                m_current = 0;
                m_used = 0;
            }
        }
    }
    void zeros(int count)
    {
        while (count > 0) {
            const int chunk = std::min(count, 32);
            put(0, chunk);
            count -= chunk;
        }
    }
    QByteArray finish()
    {
        if (m_used > 0) {
            m_bytes.append(static_cast<char>(m_current << (8 - m_used)));
            m_current = 0;
            m_used = 0;
        }
        return std::move(m_bytes);
    }

private:
    QByteArray m_bytes;
    quint8 m_current {0};
    int m_used {0};
};

class BitReader {
public:
    BitReader(const unsigned char* data, int bytes)
        : m_data(data), m_bitCount(static_cast<qint64>(bytes) * 8) {}

    bool bit(quint32& value)
    {
        if (m_position >= m_bitCount) { return false; }
        value = (m_data[m_position >> 3] >> (7 - (m_position & 7))) & 1U;
        ++m_position;
        return true;
    }
    bool bits(int count, quint32& value)
    {
        quint32 out = 0;
        for (int i = 0; i < count; ++i) {
            quint32 one = 0;
            if (!bit(one)) { return false; }
            out = (out << 1) | one;
        }
        value = out;
        return true;
    }
    qint64 position() const { return m_position; }
    qint64 bitCount() const { return m_bitCount; }

private:
    const unsigned char* m_data;
    qint64 m_bitCount;
    qint64 m_position {0};
};

void writeV2Header(Writer& writer, const DisplayCodecFrame& frame, quint8 flags)
{
    const DisplayCodecContext& context = frame.context;
    writer.u32(kMagic);
    writer.u8(kVersion2);
    writer.u8(flags);
    writer.u16(kHeaderBytes);
    writer.u32(context.endpointId);
    writer.u32(context.contextGeneration);
    writer.u32(frame.encoderSequence);
    writer.u64(frame.producerTimestamp);
    writer.f32(context.minDbm);
    writer.f32(context.maxDbm);
    writer.u16(context.traceSamples);
    writer.u16(context.waterfallSamples);
    writer.u16(context.wideSamples);
}

QByteArray v2Message(const DisplayCodecFrame& frame, quint8 flags,
                     std::initializer_list<const QByteArray*> planes)
{
    Writer writer;
    writeV2Header(writer, frame, flags);
    for (const QByteArray* plane : planes) { writer.bytes(*plane); }
    return writer.take();
}

} // namespace

quint8 displayCodecQuantize(float dbm, float minDbm, float maxDbm)
{
    const double scaled = (static_cast<double>(dbm) - minDbm)
        * 255.0 / (static_cast<double>(maxDbm) - minDbm);
    const long rounded = std::lround(std::clamp(scaled, 0.0, 255.0));
    return static_cast<quint8>(rounded);
}

float displayCodecDequantize(quint8 q, float minDbm, float maxDbm)
{
    // The negotiated endpoints are finite floats but their difference can
    // overflow float arithmetic (e.g. -FLT_MAX..FLT_MAX). Keep the complete
    // interpolation in double, clamp round-off to the finite source interval,
    // then narrow only the already-bounded result.
    const double minimum = minDbm;
    const double maximum = maxDbm;
    const double reconstructed = minimum
        + static_cast<double>(q) * (maximum - minimum) / 255.0;
    return static_cast<float>(std::clamp(reconstructed, minimum, maximum));
}

QByteArray encodeDisplayCodecV2Plane(const QVector<quint8>& samples)
{
    const int count = static_cast<int>(samples.size());
    if (count < 1 || count > DisplayCodecEncoder::kMaxSamplesPerPlane) { return {}; }

    QVector<quint8> zigzag(count, 0);
    for (int i = 1; i < count; ++i) {
        zigzag[i] = zigzagResidual(samples.at(i), samples.at(i - 1));
    }

    // Every block size codes every block its cheapest way; the plane keeps
    // the size with the fewest bytes, ties to the smaller size.
    int bestCode = 0;
    qint64 bestBytes = std::numeric_limits<qint64>::max();
    QVector<quint8> bestParameters;
    for (int code = 0; code < kV2BlockSizeCount; ++code) {
        const int blockSize = kV2BlockSizes[code];
        QVector<quint8> parameters;
        qint64 bits = 8;
        for (int first = 1; first < count; first += blockSize) {
            const int blockCount = std::min(blockSize, count - first);
            int blockBits = 0;
            parameters.append(static_cast<quint8>(
                chooseRiceParameter(zigzag.constData() + first, blockCount, blockBits)));
            bits += kV2RiceParameterBits + blockBits;
        }
        const qint64 bytes = (bits + 7) / 8;
        if (bytes < bestBytes) {
            bestBytes = bytes;
            bestCode = code;
            bestParameters = std::move(parameters);
        }
    }

    const int blockSize = kV2BlockSizes[bestCode];
    BitWriter stream;
    stream.put(samples.at(0), 8);
    int block = 0;
    for (int first = 1; first < count; first += blockSize, ++block) {
        const int blockCount = std::min(blockSize, count - first);
        const int k = bestParameters.at(block);
        stream.put(static_cast<quint32>(k), kV2RiceParameterBits);
        for (int i = first; i < first + blockCount; ++i) {
            if (k == kV2RawEscape) {
                stream.put(samples.at(i), 8);
                continue;
            }
            const quint32 z = zigzag.at(i);
            stream.zeros(static_cast<int>(z >> k));
            stream.put(1, 1);
            stream.put(z, k);
        }
    }
    const QByteArray payload = stream.finish();

    Writer writer;
    writer.u8(kV2PlaneCoding);
    writer.u8(static_cast<quint8>(bestCode));
    writer.u16(static_cast<quint16>(payload.size()));
    writer.bytes(payload);
    return writer.take();
}

bool decodeDisplayCodecV2Plane(const QByteArray& bytes, int& offset, int length,
                               QVector<quint8>& samples)
{
    if (offset < 0 || offset > bytes.size() || length < 1
        || length > DisplayCodecEncoder::kMaxSamplesPerPlane
        || bytes.size() - offset < kV2PlanePrefixBytes) {
        return false;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData()) + offset;
    const int coding = data[0];
    const int blockCode = data[1];
    const int payloadBytes = (static_cast<int>(data[2]) << 8) | data[3];
    if (coding != kV2PlaneCoding || blockCode >= kV2BlockSizeCount
        || payloadBytes > bytes.size() - offset - kV2PlanePrefixBytes) {
        return false;
    }

    BitReader reader(data + kV2PlanePrefixBytes, payloadBytes);
    QVector<quint8> decoded(length);
    quint32 value = 0;
    if (!reader.bits(8, value)) { return false; }
    decoded[0] = static_cast<quint8>(value);
    const int blockSize = kV2BlockSizes[blockCode];
    for (int first = 1; first < length; first += blockSize) {
        const int blockCount = std::min(blockSize, length - first);
        quint32 k = 0;
        if (!reader.bits(kV2RiceParameterBits, k)) { return false; }
        for (int i = first; i < first + blockCount; ++i) {
            if (k == kV2RawEscape) {
                if (!reader.bits(8, value)) { return false; }
                decoded[i] = static_cast<quint8>(value);
                continue;
            }
            // z >> k zeros, then a one. More zeros than 255 >> k would make
            // z above 255, so stop there rather than read on.
            quint32 quotient = 0;
            for (;;) {
                quint32 one = 0;
                if (!reader.bit(one)) { return false; }
                if (one != 0U) { break; }
                if (++quotient > (255U >> k)) { return false; }
            }
            quint32 low = 0;
            if (!reader.bits(static_cast<int>(k), low)) { return false; }
            const quint32 z = (quotient << k) | low;
            if (z > 255U) { return false; }
            const int residual = (z & 1U) != 0U ? -static_cast<int>((z + 1U) >> 1)
                                                : static_cast<int>(z >> 1);
            decoded[i] = static_cast<quint8>(decoded.at(i - 1) + residual);
        }
    }
    // The payload ends in the byte holding the last bit, and its pad is zero.
    if ((reader.position() + 7) / 8 != payloadBytes) { return false; }
    while (reader.position() < reader.bitCount()) {
        quint32 pad = 0;
        if (!reader.bit(pad) || pad != 0U) { return false; }
    }
    samples = std::move(decoded);
    offset += kV2PlanePrefixBytes + payloadBytes;
    return true;
}

qsizetype DisplayCodecV2Encoding::totalBytes() const
{
    qsizetype total = 0;
    for (const QByteArray& message : messages) { total += message.size(); }
    return total;
}

DisplayCodecV2Encoding encodeDisplayCodecV2(const DisplayCodecFrame& frame,
                                            quint32 admittedFrameBytes)
{
    const DisplayCodecContext& context = frame.context;
    DisplayCodecV2Encoding encoding;
    if (!validContext(context)
        || !validInputPlane(frame.traceDbm, context.traceSamples)
        || !validInputPlane(frame.waterfallDbm, context.waterfallSamples)
        || !validInputPlane(frame.wideDbm, context.wideSamples)) {
        return encoding;
    }
    const auto quantizeRow = [&context](const QVector<float>& input) {
        QVector<quint8> output;
        output.reserve(input.size());
        for (float value : input) { output.append(quantize(value, context)); }
        return output;
    };

    const QByteArray tracePlane = encodeDisplayCodecV2Plane(quantizeRow(frame.traceDbm));
    if (!frame.waterfallAdvance) {
        // No row: the trace only, always one message.
        encoding.messages.push_back(v2Message(frame, kV2FlagsTraceOnly, {&tracePlane}));
        return encoding;
    }

    const QByteArray waterfallPlane = encodeDisplayCodecV2Plane(quantizeRow(frame.waterfallDbm));
    const bool hasWide = context.wideSamples != 0;
    const QByteArray widePlane = hasWide ? encodeDisplayCodecV2Plane(quantizeRow(frame.wideDbm))
                                         : QByteArray();
    // 0x04 is set exactly when the waterfall plane is present and the wide
    // length is not zero.
    const quint8 wideFlag = hasWide ? kV2FlagWide : quint8 {0};

    const qsizetype wholeBytes = kHeaderBytes + tracePlane.size() + waterfallPlane.size()
        + widePlane.size();
    const qsizetype splitBytes = wholeBytes + kHeaderBytes;
    if (wholeBytes > IMediaTransport::kSctpDataPayloadBytes
        && splitBytes <= static_cast<qsizetype>(admittedFrameBytes)) {
        // Row part first, then the trace part (section 5).
        encoding.messages.push_back(v2Message(frame,
                                              static_cast<quint8>(kV2FlagsRowPart | wideFlag),
                                              {&waterfallPlane, &widePlane}));
        encoding.messages.push_back(v2Message(frame, kV2FlagsTracePart, {&tracePlane}));
        return encoding;
    }
    encoding.messages.push_back(v2Message(frame, static_cast<quint8>(kV2FlagsWhole | wideFlag),
                                          {&tracePlane, &waterfallPlane, &widePlane}));
    return encoding;
}

QByteArray encodeDisplayCodecAbsolutePlane(const QVector<float>& samplesDbm,
                                           float minDbm, float maxDbm)
{
    DisplayCodecContext context;
    context.minDbm = minDbm;
    context.maxDbm = maxDbm;
    if (samplesDbm.isEmpty() || samplesDbm.size() > DisplayCodecEncoder::kMaxSamplesPerPlane
        || !finite(minDbm) || !finite(maxDbm) || !(maxDbm > minDbm)
        || !validInputPlane(samplesDbm, static_cast<quint16>(samplesDbm.size()))) {
        return {};
    }
    QVector<quint8> values(samplesDbm.size());
    for (int i = 0; i < samplesDbm.size(); ++i) {
        values[i] = quantize(samplesDbm.at(i), context);
    }
    Writer writer;
    writePlane(writer, values, nullptr, true);
    return writer.take();
}

bool decodeDisplayCodecAbsolutePlane(const QByteArray& bytes, int& offset, int length,
                                     float minDbm, float maxDbm,
                                     QVector<float>& samplesDbm)
{
    if (offset < 0 || offset > bytes.size() || length <= 0
        || length > DisplayCodecEncoder::kMaxSamplesPerPlane
        || !finite(minDbm) || !finite(maxDbm) || !(maxDbm > minDbm)) {
        return false;
    }
    Reader reader(bytes);
    if (!reader.skip(offset)) { return false; }
    int planeStart = 0;
    if (!checkPlane(reader, length, true, planeStart)) { return false; }
    QVector<quint8> values;
    if (!applyPlane(bytes, planeStart, length, nullptr, values)) { return false; }
    DisplayCodecContext context;
    context.minDbm = minDbm;
    context.maxDbm = maxDbm;
    QVector<float> decoded(length);
    for (int i = 0; i < length; ++i) {
        decoded[i] = dequantize(values.at(i), context);
    }
    samplesDbm = std::move(decoded);
    offset = reader.position();
    return true;
}

DisplayCodecEncoder::DisplayCodecEncoder(int deadZone)
    : m_deadZone(std::clamp(deadZone, 0, 255))
{
}

void DisplayCodecEncoder::reset()
{
    m_hasHistory = false;
    m_trace.clear();
    m_waterfall.clear();
    m_wide.clear();
    m_sinceKeyframe = 0;
}

QByteArray DisplayCodecEncoder::encode(const DisplayCodecFrame& frame, bool requestKeyframe)
{
    const DisplayCodecContext& context = frame.context;
    if (!validContext(context)
        || !validInputPlane(frame.traceDbm, context.traceSamples)
        || !validInputPlane(frame.waterfallDbm, context.waterfallSamples)
        || !validInputPlane(frame.wideDbm, context.wideSamples)) {
        return {};
    }

    const bool contextChanged = !m_hasHistory || !sameContext(context, m_context);
    if (m_hasHistory) {
        // An encoder is endpoint-owned. Same-generation shape changes are a
        // caller bug: accepting them would emit a keyframe its decoder rejects.
        if (context.endpointId != m_context.endpointId
            || (context.contextGeneration == m_context.contextGeneration && contextChanged)
            || (context.contextGeneration != m_context.contextGeneration
                && staleOrEqual(context.contextGeneration, m_context.contextGeneration))
            || (!contextChanged && staleOrEqual(frame.encoderSequence, m_lastSequence))) {
            return {};
        }
    }
    const bool keyframe = contextChanged || requestKeyframe
        || m_sinceKeyframe >= kKeyframeInterval - 1;
    const QVector<quint8>* oldTrace = keyframe ? nullptr : &m_trace;
    const QVector<quint8>* oldWaterfall = keyframe ? nullptr : &m_waterfall;
    const QVector<quint8>* oldWide = keyframe ? nullptr : &m_wide;

    const auto reconstruct = [this, &context, keyframe](const QVector<float>& input,
                                                         const QVector<quint8>* previous) {
        QVector<quint8> output;
        output.reserve(input.size());
        for (int i = 0; i < input.size(); ++i) {
            quint8 value = quantize(input.at(i), context);
            if (!keyframe && std::abs(static_cast<int>(value) - static_cast<int>(previous->at(i)))
                <= m_deadZone) {
                value = previous->at(i);
            }
            output.append(value);
        }
        return output;
    };
    const QVector<quint8> trace = reconstruct(frame.traceDbm, oldTrace);
    const QVector<quint8> waterfall = reconstruct(frame.waterfallDbm, oldWaterfall);
    const QVector<quint8> wide = reconstruct(frame.wideDbm, oldWide);

    Writer writer;
    writer.u32(kMagic);
    writer.u8(kVersion);
    quint8 flags = keyframe ? kFlagKeyframe : 0;
    if (frame.waterfallAdvance) { flags |= kFlagWaterfallAdvance; }
    if (!wide.isEmpty()) { flags |= kFlagWide; }
    writer.u8(flags);
    writer.u16(kHeaderBytes);
    writer.u32(context.endpointId);
    writer.u32(context.contextGeneration);
    writer.u32(frame.encoderSequence);
    writer.u64(frame.producerTimestamp);
    writer.f32(context.minDbm);
    writer.f32(context.maxDbm);
    writer.u16(context.traceSamples);
    writer.u16(context.waterfallSamples);
    writer.u16(context.wideSamples);
    writePlane(writer, trace, oldTrace, keyframe);
    writePlane(writer, waterfall, oldWaterfall, keyframe);
    if (!wide.isEmpty()) { writePlane(writer, wide, oldWide, keyframe); }
    QByteArray encoded = writer.take();
    if (encoded.size() > kMaxEncodedBytes) { return {}; }

    m_hasHistory = true;
    m_context = context;
    m_lastSequence = frame.encoderSequence;
    m_trace = trace;
    m_waterfall = waterfall;
    m_wide = wide;
    m_sinceKeyframe = keyframe ? 0 : m_sinceKeyframe + 1;
    return encoded;
}

void DisplayCodecDecoder::reset()
{
    m_hasHistory = false;
    m_needsKeyframe = false;
    m_trace.clear();
    m_waterfall.clear();
    m_wide.clear();
}

DisplayCodecDecodeResult DisplayCodecDecoder::decode(const QByteArray& packet)
{
    if (packet.size() > DisplayCodecEncoder::kMaxEncodedBytes) {
        return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Oversized);
    }
    Reader reader(packet);
    quint32 magic = 0, endpointId = 0, generation = 0, sequence = 0;
    quint64 timestamp = 0;
    quint8 version = 0, flags = 0;
    quint16 headerBytes = 0, traceLength = 0, waterfallLength = 0, wideLength = 0;
    DisplayCodecContext context;
    if (!reader.u32(magic)) { return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Truncated); }
    if (magic != kMagic) { return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::BadMagic); }
    if (!reader.u8(version)) { return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Truncated); }
    if (version != kVersion) { return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::UnsupportedVersion); }
    if (!reader.u8(flags) || !reader.u16(headerBytes)) {
        return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Truncated);
    }
    if ((flags & ~kKnownFlags) != 0) { return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::UnknownFlags); }
    if (headerBytes != kHeaderBytes) { return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Malformed); }
    if (!reader.u32(endpointId) || !reader.u32(generation) || !reader.u32(sequence)
        || !reader.u64(timestamp) || !reader.f32(context.minDbm) || !reader.f32(context.maxDbm)
        || !reader.u16(traceLength) || !reader.u16(waterfallLength) || !reader.u16(wideLength)) {
        return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Truncated);
    }
    context.endpointId = endpointId;
    context.contextGeneration = generation;
    context.traceSamples = traceLength;
    context.waterfallSamples = waterfallLength;
    context.wideSamples = wideLength;
    const bool keyframe = (flags & kFlagKeyframe) != 0;
    const bool hasWide = (flags & kFlagWide) != 0;
    if (!validContext(context) || hasWide != (wideLength != 0)) {
        return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Malformed);
    }

    // The whole datagram's structure first (display codec document, "State
    // and recovery"): a malformed packet rejects as malformed whatever state
    // the decoder is in, before any endpoint, context, generation, sequence
    // or history rule is asked.
    int tracePlane = 0, waterfallPlane = 0, widePlane = 0;
    if (!checkPlane(reader, traceLength, keyframe, tracePlane)
        || !checkPlane(reader, waterfallLength, keyframe, waterfallPlane)
        || (hasWide && !checkPlane(reader, wideLength, keyframe, widePlane))
        || !reader.atEnd()) {
        return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Malformed);
    }

    // Then the endpoint, context, generation, sequence and history rules.
    const QVector<quint8>* oldTrace = nullptr;
    const QVector<quint8>* oldWaterfall = nullptr;
    const QVector<quint8>* oldWide = nullptr;
    bool sequenceGap = false;
    if (!m_hasHistory) {
        if (!keyframe) { return result(DisplayCodecDisposition::NeedKeyframe, DisplayCodecReason::NoHistory); }
    } else {
        if (context.endpointId != m_context.endpointId) {
            return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::ContextMismatch);
        }
        if (context.contextGeneration != m_context.contextGeneration) {
            if (staleOrEqual(context.contextGeneration, m_context.contextGeneration)) {
                return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::OldContext);
            }
            if (!keyframe) { return result(DisplayCodecDisposition::NeedKeyframe, DisplayCodecReason::ContextMismatch); }
        } else {
            if (!sameContext(context, m_context)) {
                return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::ContextMismatch);
            }
            if (staleOrEqual(sequence, m_lastSequence)) {
                return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::StaleSequence);
            }
            if (m_needsKeyframe && !keyframe) {
                return result(DisplayCodecDisposition::NeedKeyframe, DisplayCodecReason::SequenceGap);
            }
            if (!keyframe && sequence != m_lastSequence + 1U) {
                sequenceGap = true;
            }
        }
        if (!keyframe && sameContext(context, m_context)) {
            oldTrace = &m_trace;
            oldWaterfall = &m_waterfall;
            oldWide = &m_wide;
        }
    }

    if (sequenceGap) {
        m_needsKeyframe = true;
        return result(DisplayCodecDisposition::NeedKeyframe, DisplayCodecReason::SequenceGap);
    }

    // Then the datagram is applied.
    QVector<quint8> trace, waterfall, wide;
    if (!applyPlane(packet, tracePlane, traceLength, oldTrace, trace)
        || !applyPlane(packet, waterfallPlane, waterfallLength, oldWaterfall, waterfall)
        || (hasWide && !applyPlane(packet, widePlane, wideLength, oldWide, wide))) {
        return result(DisplayCodecDisposition::Rejected, DisplayCodecReason::Malformed);
    }

    DisplayCodecDecodeResult decoded;
    decoded.disposition = DisplayCodecDisposition::Accepted;
    decoded.reason = DisplayCodecReason::None;
    decoded.frame.context = context;
    decoded.frame.encoderSequence = sequence;
    decoded.frame.producerTimestamp = timestamp;
    decoded.frame.waterfallAdvance = (flags & kFlagWaterfallAdvance) != 0;
    decoded.frame.traceDbm.reserve(trace.size());
    decoded.frame.waterfallDbm.reserve(waterfall.size());
    decoded.frame.wideDbm.reserve(wide.size());
    for (quint8 value : trace) { decoded.frame.traceDbm.append(dequantize(value, context)); }
    for (quint8 value : waterfall) { decoded.frame.waterfallDbm.append(dequantize(value, context)); }
    for (quint8 value : wide) { decoded.frame.wideDbm.append(dequantize(value, context)); }

    m_hasHistory = true;
    if (keyframe) { m_needsKeyframe = false; }
    m_context = context;
    m_lastSequence = sequence;
    m_trace = std::move(trace);
    m_waterfall = std::move(waterfall);
    m_wide = std::move(wide);
    return decoded;
}

} // namespace NereusSDR
