# Display codec v2: every row stands alone

Status: design for JJ's approval (2026-10-22). No code yet.

JJ's decision of 2026-10-22: "yes do C then build A". Step 1 (C) is a Core
setting that sends every v1 display frame as a keyframe, with no wire change,
being built on `claude/display-keyframe-every-frame`; it is the bench baseline
for this design. Step 2 (A) is this document: a new display codec version in
which every waterfall row and every trace is coded on its own, losslessly, so
a lost message costs exactly its own row and nothing waits for a keyframe.

Inputs: the loss study of 2026-10-22 (the crew folder's
`study-waterfall-loss.md`), the v1 design
([display codec v1](2026-09-20-display-codec-v1.md)), `DisplayCodec.{h,cpp}`,
the Core sender (`DaemonMediaController`), the window side
(`RemoteMediaController`, `RemoteDisplayPresenter`, `SpectrumWidget`) and the
iPhone decoder (`ios/NereusKit/Sources/NereusMedia/DisplayFrameDecoder.swift`).

## 1. The problem in one paragraph

A v1 frame is a 42-byte header, a trace plane and a waterfall plane. Between
keyframes each plane is a temporal delta against the previous frame
(DisplayCodec.cpp:236-263), so a lost message breaks the chain: the decoder
returns `NeedKeyframe` and drops every later delta (DisplayCodec.cpp:576-592),
the window asks for a keyframe at most every 200 ms
(RemoteMediaController.cpp:8413-8414), the Core grants at most 5 a second
(DaemonMediaController.cpp:279, 4223), and meanwhile the window repeats the
last row and later blends the gap (RemoteDisplayPresenter.cpp:163-213,
286-306). On JJ's STRIX link (media RTT 71 ms, every frame two datagrams on an
unordered channel with no retransmission) one lost datagram costs about seven
made-up rows. That is the smear.

## 2. What the references do (study, then choose)

Every reference the study read sends rows that stand alone:

- OpenWebRX (develop `640c5b0b`, csdr `1f15b8c5`): one message per row, 4-bit
  IMA ADPCM reset for every row with 10 pad samples (csdr adpcm.cpp:165-170,
  253-260). TCP, no gap fill.
- KiwiSDR (master `efb38e2b`): a 16-byte header with a sequence
  (rx_waterfall.h:73-89), 8-bit dB, IMA ADPCM fresh for every row
  (rx_waterfall.cpp:1577-1584, 1660-1667).
- piHPSDR (`4aa95c5`): one byte per bin, zlib per row, over UDP
  (server_thread.c:221-232); a loss costs one row (client_thread.c:642-653).
- AetherSDR (`1e0718ad`) and Flex: waterfall tiles of absolute 16-bit bins,
  self-contained; a partial tile is dropped (PanadapterStream.cpp:1071-1075,
  1094-1095).
- WebRTC video keeps temporal prediction and pays for it with NACK, FEC,
  picture-loss requests and intra refresh, because a video intra frame is 5 to
  10 times a delta frame. Ours is not: section 6 measures an intra row at or
  below today's temporal delta.

What we take: self-contained rows (all of them) and one row per message where
it fits (OpenWebRX, KiwiSDR, piHPSDR). What we do not take: ADPCM. The study
measured IMA ADPCM at our 0.47 dB quantum with 3 to 10 dB error at carrier
edges (p99), and JJ asked for lossless. zlib is lossless but costs more CPU
for less gain on 783-byte rows than the coder below, and it would be a new
dependency on the phone's decode path. The entropy code is a Rice code with a
parameter per block and an escape for incompressible blocks, the same
structure FLAC uses for its residual partitions.

## 3. Choosing the predictor and entropy code

Measured with a scratch driver (outside the repository, in the crew
scratchpad as `codecv2/v2sim.cpp` and `codecv2/fast.cpp`; it links the
repository's real `DisplayCodec.cpp` for the v1 figures). Scenes are the
study's: 783 pixels, a 120 dB window (quantum 0.47 dB), 12 carriers, 4 keyed
CW signals and 2 SSB-like bands, 1,500 frames each; plus raw single-bin rows,
uniform noise (the worst case), flat, a drifting ramp, a 1,024-pixel average
pan and a 4,096-pixel peak pan. Every candidate was decoded back and compared
with the encoder's quantized input: zero mismatches on every scene.

Mean bytes per plane (without the 4-byte v2 plane prefix):

| Candidate | Peak 167 | Avg 167 | Peak+IIR | Avg 8 | Raw 1 bin | Noise | 4096 px |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Previous bin, Rice per 16 | 421 | **353** | **347** | 488 | 620 | 801 | 2,372 |
| Previous bin, Rice per 32 | 423 | 380 | 362 | 487 | 614 | 793 | 2,365 |
| Previous bin, Rice per 64 | 426 | 396 | 372 | **486** | 610 | **788** | 2,369 |
| Two-tap (2a - b), Rice per 32 | 471 | 404 | 397 | 534 | 672 | 793 | 2,648 |
| Median of last three, Rice per 32 | 447 | 427 | 401 | 510 | 616 | 793 | 2,437 |
| Mean of last two, Rice per 32 | 431 | 404 | 379 | 495 | 610 | 793 | 2,376 |
| Previous or two-tap per block, Rice per 32 | 426 | 375 | 362 | 490 | 617 | 796 | 2,377 |
| Previous bin, adaptive Rice (no side data) | 422 | 399 | 375 | 486 | 608 | 841 | 2,360 |
| Order-0 entropy of previous-bin residuals (floor) | 393 | 304 | 311 | 460 | 586 | 758 | 2,280 |
| v1 temporal delta, today (for scale) | 512 | 414 | 370 | 581 | 745 | 821 | 2,908 |

Choices, and why:

- **Predictor: the previous bin.** The two-tap predictor and the medians lose
  everywhere: display rows are mostly noise around a smooth floor, and a
  longer predictor adds that noise rather than cancelling it. Choosing between
  predictors per block gains nothing worth a bit per block. In one dimension a
  median of `a`, `b` and `2a - b` is always `a`, so the "median" tried is the
  median of the last three bins.
- **Entropy code: Rice with one parameter `k` per block, and an escape.** It is
  within 4 to 16 percent of the order-0 entropy floor, decodes with shifts and
  a leading-zero count, and ports to Swift in about a hundred lines. Adaptive
  Rice (a running parameter, as JPEG-LS does) needs no side data but loses on
  averaged rows and, on noise, exceeds the v1 bound (841 bytes against 821),
  so it would need an escape anyway. An arithmetic code could take back the
  last 30 to 50 bytes a plane, at a cost in complexity and phone porting that
  this does not justify.
- **Block size chosen per plane: 16, 32 or 64.** No one size wins: 16 is best
  on averaged and smoothed rows, 64 on raw and noisy rows. The encoder tries
  all three and keeps the smallest, as v1 already does with its block sizes.
  Mean plane sizes with that choice, including the 4-byte plane prefix: peak
  424, average 356, peak with IIR 350, average of 8 bins 489, raw 613, noise
  791, flat 107, ramp 171, 1,024 px 451, 4,096 px 2,368.

## 4. Wire format

All multibyte integers are unsigned network byte order, as v1.

### 4.1 Header

The 42-byte header is v1's, field for field and offset for offset, with
`version = 2`. Endpoint ID stays at offset 8 and context generation at 12, so
the window's routing (RemoteMediaController.cpp:8451-8464) reads both
versions alike. The header still carries the full context (interval and all
three row lengths) whatever planes follow, so a message is self-describing.

Flags at offset 5 change meaning in version 2:

| Bit | Name | Meaning |
| --- | --- | --- |
| `0x02` | waterfall advance | This frame advances the waterfall (as v1). |
| `0x04` | wide plane | A wide (3D) plane follows the waterfall plane. |
| `0x08` | trace plane | A trace plane is present. |
| `0x10` | waterfall plane | A waterfall plane is present. |
| `0x20` | split | The frame was sent as two messages (section 5). |
| `0x01`, `0x40`, `0x80` | | Reject (`UnknownFlags`). |

The valid combinations, and nothing else (`Malformed` otherwise):

| Flags | Message |
| --- | --- |
| `0x08` | A whole frame that does not advance the waterfall: the trace only. |
| `0x1A` | A whole frame: trace and waterfall. |
| `0x1E` | A whole frame: trace, waterfall and wide. |
| `0x2A` | The trace part of a split frame. |
| `0x32` | The row part of a split frame: waterfall. |
| `0x36` | The row part of a split frame: waterfall and wide. |

The wide plane is present exactly when the waterfall plane is present and
the header's wide length is not zero. There is no keyframe flag: every
version 2 message is decodable alone.

A frame that does not advance the waterfall carries no waterfall plane in
v2. Today the waterfall plane rides every frame and the window uses it only
when the frame advances (RemoteMediaController.cpp:8797, SpectrumWidget.cpp:2891,
FilterDisplayItem.cpp:211). With the default 30 ms waterfall period at 30 fps
every frame advances (framesPerLine 1), so this saves bytes only for a slower
waterfall.

### 4.2 Plane

Planes follow the header in trace, waterfall, wide order, those present only.

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 1 | coding: `2` (previous-bin Rice); any other value rejects |
| 1 | 1 | block size code: `0` = 16, `1` = 32, `2` = 64; 3 and above reject |
| 2 | 2 | payload bytes |
| 4 | payload | the bit stream |

The bit stream is read most significant bit first:

1. The first sample, 8 bits, as its quantized value.
2. Then samples 1 to n-1 in blocks of the coded size (the last may be short).
   Each block starts with `k`, 3 bits.
   - `k` = 7: the block's samples follow as raw 8-bit quantized values.
   - `k` = 0 to 6: each sample is a residual against the sample before it,
     `r = ((x[i] - x[i-1] + 128) mod 256) - 128`, zigzag-mapped to
     `z = 2r` when `r >= 0` and `z = -2r - 1` otherwise (so `z` is 0 to 255),
     written as `z >> k` zero bits, a one bit, then the low `k` bits of `z`.
     The decoder rebuilds `x[i] = (x[i-1] + r) mod 256`.
3. Zero bits pad the stream to a whole byte.

Quantization is v1's exactly (`round((dbm - minDbm) * 255 / (maxDbm -
minDbm))`, clipped), so v2 reconstructs what a v1 keyframe reconstructs, bit
for bit. The dead zone does not exist in v2: it only ever applied against a
previous frame.

The decoder rejects as `Malformed`: a payload that runs out, a `z` above 255,
a payload with bytes left after the last sample, any nonzero pad bit, and a
plane prefix out of range. It does not require the encoder's choices to be
the smallest; any valid stream decodes.

The encoder, for each block size, picks per block the `k` from 0 to 6 with
the fewest bits, ties to the smaller `k`, and the raw escape only when it is
strictly smaller; then it keeps the block size with the smallest plane, ties
to the smaller size. This is deterministic, so golden vectors are stable.

### 4.3 Exact sizes and the bound

A v2 plane of `n` samples is at most

    P(n) = 4 + n + ceil(3 * ceil((n - 1) / 64) / 8)

bytes: the 4-byte prefix, the first sample, every other sample raw, and a
3-bit `k` per 64-sample block (the encoder never does worse than its 64-byte
all-raw choice). The scratch check ran every `n` from 1 to 4,096 with
incompressible rows: every plane decoded exactly and stayed within `P(n)`,
and `P(n) <= A(n)` (v1's `3 + 5*ceil(n/128) + n`) for every `n`. For example
`P(783) = 792` against `A(783) = 821`, and `P(4096) = 4,124` against 4,259.

So a whole v2 frame is never larger than a v1 keyframe of the same shape,
`42 + 2*A(n) + A(wide)`, which is what `DisplayBudget` already charges
(DisplayBudget.cpp:132-141, DisplayBudget.h:64-73). **The display budget,
its charge and its wire do not change**, on the Core, the window
(`RemoteDisplayAllocator`) or the phone (`DisplayQualityAllocator`).

## 5. One message or two

The study suggested two, so that each fits one datagram. The rule:

- A frame is sent whole when the whole message fits one SCTP DATA chunk
  (876 bytes, `IMediaTransport::kSctpDataPayloadBytes`), and always when it
  carries no row (`0x08`).
- Otherwise it is split into a row part (waterfall, and the wide plane when
  present) and a trace part, both with the frame's sequence and timestamp,
  provided the two parts together (one more 42-byte header) stay within the
  v1 keyframe bound for the shape. When they would not (possible only for
  nearly incompressible rows of 640 pixels or fewer), it is sent whole.

Why: a frame bigger than one datagram is lost when any of its datagrams is
lost. Whole, a two-datagram frame loses its row with about twice the
datagram loss rate; split, each part is one datagram, so the row is lost at
the datagram loss rate and a lost trace part no longer costs a row. The
datagram count is the same. The extra header costs about 5 percent of the
bytes of a split frame.

For JJ's 783-pixel pan (peak scene) a whole v2 frame averages 890 bytes, just
over one datagram, so 87 percent of frames split into two parts of about 466
bytes, each one datagram. Average and smoothed scenes fit one datagram whole.

Send order: the row part first, then the trace part. The display channel
holds at most one message when SCTP cannot take it at once, and refuses the
next (v1 document, "Transport limits"). If that happens to the first part,
the second is the one refused, and losing a trace costs less than losing a
row. If the first part is refused, neither is sent (the frame is dropped, as
today). A refused second part is counted.

Pacing and the admitted cost (DaemonMediaController.cpp:6551-6559) are
checked against the frame's total bytes, before the first part is offered. A
v2 frame over its admitted cost cannot happen by section 4.3; if it ever did
it is dropped and logged, with no keyframe forced (there is none to force).

Rows bigger than one datagram stay one message: a 4,096-pixel row part is
about 2,410 bytes, three datagrams. Cutting a row into self-contained pieces
would make a lost datagram cost part of a row, which the window cannot draw
today without a change to its gap fill (a visual change), so it is not in
this design.

The display extras datagram (NSDX) still follows each frame once, unchanged.

## 6. Measured estimate: v2 against v1

Mean bytes per frame (trace and waterfall, header included), kbps per pan at
20 and 30 fps (the study assumed 20 rows a second for JJ's pan; the default
is 30), and datagrams per frame. "Keyframe every frame" is Step 1. In these
scenes trace and waterfall are the same row; a real trace has its own
detector and is usually smoother, so the trace plane will often be smaller.

| Scene | Pixels | v1 today, B | Keyframe every frame (Step 1), B | v2, B | v1 kbps 20/30 | Step 1 kbps 20/30 | v2 kbps 20/30 | Datagrams v1 / Step 1 / v2 | v2 against v1 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Peak, 167 bins/px | 783 | 1,072 | 1,684 | 926 | 172 / 257 | 269 / 404 | 148 / 222 | 2.00 / 2 / 1.87 | -14% |
| Average, 167 bins/px | 783 | 877 | 1,684 | 755 | 140 / 210 | 269 / 404 | 121 / 181 | 1.46 / 2 / 1.00 | -14% |
| Peak + IIR 0.3 | 783 | 781 | 1,684 | 741 | 125 / 187 | 269 / 404 | 119 / 178 | 1.01 / 2 / 1.00 | -5% |
| Average, 8 bins/px | 783 | 1,207 | 1,684 | 1,062 | 193 / 290 | 269 / 404 | 170 / 255 | 2.00 / 2 / 2.00 | -12% |
| Raw, 1 bin/px | 783 | 1,532 | 1,684 | 1,310 | 245 / 368 | 269 / 404 | 210 / 314 | 2.00 / 2 / 2.00 | -14% |
| Uniform noise (worst) | 783 | 1,684 | 1,684 | 1,666 | 269 / 404 | 269 / 404 | 267 / 400 | 2.00 / 2 / 2.00 | -1% |
| Flat | 783 | 326 | 1,684 | 256 | 52 / 78 | 269 / 404 | 41 / 61 | 1.01 / 2 / 1.00 | -21% |
| Drifting ramp | 783 | 674 | 1,684 | 384 | 108 / 162 | 269 / 404 | 61 / 92 | 1.01 / 2 / 1.00 | -43% |
| Average | 1,024 | 1,106 | 2,176 | 986 | 177 / 265 | 348 / 522 | 158 / 237 | 2.01 / 3 / 2.00 | -11% |
| Peak | 4,096 | 5,881 | 8,560 | 4,819 | 941 / 1,411 | 1,370 / 2,054 | 771 / 1,157 | 7.03 / 10 / 6.00 | -18% |

v2 is smaller than today's v1 on every scene, and its largest frame on the
783-pixel scenes (1,666 bytes, uniform noise) is below the 1,684-byte v1
keyframe. What a lost datagram costs:

- v1 today: the frame, then every frame until a granted keyframe arrives
  (about seven made-up rows on the STRIX link).
- Step 1: one frame (a row and a trace), but every frame is the keyframe size.
- v2: one row or one trace, never both, never more.

## 7. Version negotiation

Pattern: the hello's declared features (station link v1, "Declared
features": a receiver ignores a name it does not know), as `remoteTx` and
`vax` do.

- **Client declares** `features.displayCodec` 2 in its `hello` once its
  decoder reads v2. The desktop window declares it in the same change as the
  Core encoder. The iPhone app declares it only when its decoder lands
  (section 8).
- **Core answers** a peer that declared it, with media enabled, by sending
  `displayCodecVersion` 2 in the minor-11 capabilities block (after the last
  entry there today), and by encoding every display endpoint of that peer's
  media session in v2, its transmit display included. Every other peer gets
  no entry and exactly today's v1 bytes. `tst_display_extras`' older-peer
  golden (`tests/OlderPeerDisplayRun.h`) and the link conformance fixtures
  hold that byte for byte.
- **Decoders read both.** The window's decoder chooses by the version byte,
  so a v2 window on an older Core (which ignores the feature and sends v1)
  works exactly as today, keyframe requests and all. The capability is the
  Core's statement for logs, the support bundle and tests; no decode depends
  on it.
- **Fallback switch.** A Core setting `RemoteDisplayCodecV2` (AppSettings,
  `"True"` by default, no UI) set to `"False"` makes the Core ignore the
  feature and send v1 to everyone. It exists for side-by-side bench runs and
  as a field fallback. It takes effect at the next media connection.
- The Step 1 keyframe-every-frame setting applies to v1 endpoints only;
  for a v2 endpoint there is nothing for it to do.

No client breaks: an old window or phone never declares the feature and sees
nothing new; a new window with an old Core sees v1; a new window with a new
Core sees v2 and still reads a v1 frame if the fallback switch is set.

## 8. The iPhone app

The phone decodes NSDC v1 in `DisplayFrameDecoder.swift` and asks for
keyframes through `MediaControlClient` (5 a second per endpoint). Plan:

- **Meanwhile:** it declares nothing new, so the new Core sends it v1, and
  Step 1's keyframe-every-frame setting already removes its smear when JJ
  turns it on.
- **Then, in a follow-up change right after the desktop one:** a v2 path in
  `DisplayFrameDecoder` (header and flag rules, the Rice plane, the part
  pairing of section 9), `features.displayCodec` 2 in the phone's `hello`,
  and its keyframe requests left in place for v1 Cores. The desktop change
  writes golden vectors (encoder input rows, v2 messages, split and whole,
  plus malformed cases) under `tests/data/display_codec_v2/`, and both the
  C++ and the NereusKit tests read the same files, so the two decoders are
  held to one encoder.
- Why not in the same change: the phone ships on its own `ios-v` tags
  through App Store Connect, and keeping it on v1 until its build is out
  costs nothing because the Core serves both.

## 9. The decoder and the window

### 9.1 Decoder state

A v2 decoder keeps no history a message depends on. It keeps:

- the accepted context (endpoint, generation, interval, row lengths) and the
  last sequence, for the same stale and old-context rules as v1;
- at most one held part of a split frame, waiting for its partner;
- the last decoded trace and the last decoded row (waterfall and wide) of the
  context, used only to fill a frame's absent plane (below), never to decode.

Order of checks is v1's: the whole message's structure first (header, flag
combination, every plane), then endpoint, context, generation and sequence,
then the message is applied. A newer generation is accepted at once: there
is no `NeedKeyframe` and no `NoHistory` in v2.

### 9.2 Pairing the parts of a split frame

The channel is unordered, so either part may come first.

- A part arrives with nothing held: it is held.
- Its partner arrives (same generation and sequence, the other part): one
  whole frame is released.
- A message of a newer sequence or generation arrives while a part is held:
  the held part is released first as a partial frame, then the new message
  is handled.
- A part older than the last released sequence is stale and rejected, as v1
  rejects a stale frame; a row that arrives after its slot has passed is
  not drawn late.

`decode()` therefore returns zero, one or two frames, oldest first (the v1
path never returns more than one). Both parts are sent in the same sender
turn, so the hold normally lasts well under a millisecond; a part whose
partner was lost waits one frame period, for the next message.

A released frame always has full-length planes, so nothing downstream sees
a short frame:

- Trace part only (its row was lost): the trace, waterfall advance clear, and
  the waterfall plane filled with the last decoded row (minimum dBm when there
  is none). The window never draws it as a row; the next row fills the slot
  (section 9.3).
- Row part only (its trace was lost): the row, waterfall advance set, and the
  last decoded trace, marked `traceCarried`. The window passes such a frame to
  the presenter as not current for measurement (the flag `noteLoss()` already
  sets, RemoteDisplayPresenter.cpp:241-252), so the IMD measurement never
  counts a repeated trace.
- A whole frame without a row (`0x08`): the trace, waterfall advance clear,
  the waterfall filled the same way.

The decoder counts frames lost (sequence gaps) and parts lost (partials
released) for the window's diagnostics line.

### 9.3 Keyframes, the interval and the gap fill

- **Keyframe requests:** a v2 decoder never asks, so the window sends no
  `keyframe` operation for a v2 binding. The Core still answers the operation
  (a v1 peer needs it) and for a v2 endpoint it is a no-op.
- **`kKeyframeInterval` (120):** unchanged for v1; meaningless for v2, whose
  encoder has no history. The Core's "force a keyframe after a refused or
  oversized frame" (DaemonMediaController.cpp:6558, 6596, 7307, 7336) is a no-op for v2.
- **Gap fill: unchanged.** The presenter's blend (RemoteDisplayPresenter.cpp:
  161-215) already fills the slots between two real rows from the producer
  timestamps, so a lost row is one blended row between its neighbors and the
  time axis stays true. The repeat path (:286-306) runs only while waiting
  for a keyframe, which a v2 binding never does, so it falls silent. No
  drawing behavior changes, so there is nothing for JJ to rule on here.
- **What the window changes, none of it visual:** route version 2 to the
  decoder, loop over the frames `decode()` returns, mark a `traceCarried`
  frame not current for measurement, and add `displayMessagesLost` and
  `displayPartsLost` to the window's display diagnostics beside today's
  `keyframeWaits` (which stays, and should read 0 on v2).
- **Transmit display:** the same encoder and decoder; frames go straight to
  the transmit display as today (RemoteMediaController.cpp:8489-8495,
  8512-8522), minus the keyframe request.

## 10. Performance

Measured in the scratch program on this Mac (Apple Silicon, one thread, at
low priority while JJ was using it), with a straightforward implementation
(64-bit bit buffer to write, one bit at a time to read):

| Work | 783 px | 4,096 px |
| --- | ---: | ---: |
| v2 encode, one plane | 8 us | 50 us |
| v2 decode, one plane | 13 us | 77 us |
| v1 encode, one frame (two planes, with float quantizing) | 44 us | 395 us |
| v1 decode, one frame | 38 us | 149 us |

The v2 figures leave out float quantizing, which both versions do the same
way. Even so, v2 is no more work than v1: it does one pass per block size
over 8-bit residuals where v1 does four block sizes of bit-width search.

Estimates for the Cores and the phone (not measured; the implementation
measures them, below): a Pi 4 (Cortex-A72) runs scalar integer code like this
about 3 to 4 times slower than this Mac, so about 30 us per 783-pixel plane;
four pans at 30 fps is under 1 percent of one core. The worst Rock 5C load
named in v1 (4,096 points, 60 fps, with 3D) is about 200 us of encode per
frame on its A76 cores, about 1 percent of one core. An iPhone's decode
cost is in the range of this Mac's. The implementation adds a timed
benchmark to the codec test (a `QBENCHMARK` over the study scenes) and
records it on the Pi 4 and Rock 5C Cores as part of the bench run in
section 11; the Core session installs those builds.

## 11. Tests

1. **Codec test** (`tst_display_codec_v2`, beside `tst_display_budget`): the
   four study scenes plus noisy, flat, drifting, raw and 4,096-pixel rows.
   Random drops and Gilbert-Elliott bursts (2 percent loss, mean burst 3),
   applied per datagram, with split and whole frames. Pass when every
   surviving message decodes alone, bit-exact to the encoder's quantized
   input; the mean frame is at most the v1 mean on the same rows; no frame
   exceeds the v1 keyframe for its shape; and every `n` from 1 to 4,096 with
   each wide length keeps `P(n) <= A(n)` and a split frame within the v1
   bound. Plus the malformed set: every invalid flag combination, coding and
   block codes out of range, payload overrun, trailing bytes, nonzero pad, a
   `z` above 255, truncation at every byte, and structure checked before
   state (as v1's tests do). Plus pairing: either part first, a lost part, a
   duplicate, a stale part, a generation change while a part is held.
2. **Presenter test:** v2 frames through the decoder and
   `RemoteDisplayPresenter` under the same loss: real rows drawn equals rows
   sent minus rows lost; one blended row per lost row (bursts up to
   `kMaxGapRows`); no repeated rows; no keyframe waits or requests; a
   `traceCarried` frame never advances the IMD measurement.
3. **Interop:**
   - a new Core and a peer without the feature: byte-identical v1 (the
     older-peer golden and link conformance fixtures);
   - a new Core and a v2 window: v2 frames, `displayCodecVersion` 2, no
     keyframe operations;
   - an old Core (a peer that ignores the feature) and a v2 window: v1
     frames, keyframe recovery as today;
   - the fallback switch set: v1 to a v2 window;
   - the golden vectors read by the C++ test now and by NereusKit's test in
     the phone change.
4. **Bench**, 30 to 45 minutes on the STRIX link, Step 1 first, then v2:
   Core `keyframeRequests` flat; window `keyframeWaits` 0 outside context
   changes; blended plus repeated rows per lost datagram about 1 (today about
   7). The Core's `daemon display diagnostics` line gains mean bytes per
   frame and the count of split frames and refused second parts (today it
   logs only the largest frames).

## 12. What changes where

- `DisplayCodec.{h,cpp}`: a v2 encoder (or a version on the encoder) and the
  decoder's v2 path, `decode()` returning 0 to 2 frames.
- `DaemonMediaController`: the encoder chosen per peer, split and send order,
  totals for pacing, the diagnostics fields.
- `StationServer` / `StationCapabilities`: `displayCodecVersion` for a
  declaring peer; `StationClient`: the window's `features.displayCodec` 2.
- `RemoteMediaController`: routing, the frame loop, `traceCarried`, the new
  counters.
- Docs: this file, a pointer from the v1 document, station link v1's
  feature and capability lists.
- Unchanged: `DisplayBudget`, the allocators, the presenter's drawing, the
  NSDX extras, the PS3D codec, the phone (until its own change).

## 13. For JJ's veto

Each of these is settled in the design above; say no to any and it changes.

1. **Split a frame that would need two datagrams into a row message and a
   trace message.** This halves how often a row is lost on your link, for
   about 5 percent more bytes on those frames. The simpler alternative sends
   every frame whole and lets the transport cut it. Recommendation: split.
2. **A hidden Core setting that sends the old format to everyone.** No UI,
   on the new format by default; it lets the bench compare old and new on
   the same Core and is a quick way back if something goes wrong in the
   field. Recommendation: keep it.
3. **The iPhone gets the new format in a follow-up change right after the
   desktop one,** and until then gets today's format, with the
   keyframe-every-frame setting from Step 1 as its fix. Recommendation: yes,
   so the phone's App Store build does not hold up the desktop.
4. **Keep the waterfall's gap fill exactly as it is:** a lost row is drawn as
   one row blended from its neighbors, so the time axis stays true. The
   alternative is to show a lost row as a visible gap (a dark line).
   Recommendation: keep the blend.
