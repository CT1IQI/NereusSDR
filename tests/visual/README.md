# Visual preview tool

`nereus_visual_preview` builds the real NereusSDR main window with no radio,
fills it with fixed illustrative data, and writes full-resolution PNGs. It
captures the app as it is today: it changes nothing about how the app looks.
`capture.py` runs it across scenes and screen profiles, and `build_gallery.py`
turns a run into a page you can browse. For a design review, `sourcemap.py`
colours each capture by where its parts come from, `compare_retained.py`
proves retained parts are pixel-identical to today's app, and
`build_gallery.py` on a run root builds the one review page from both.

## Build

The target is excluded from `all`, so build it on its own in an already
configured build directory:

```
cmake --build build --target nereus_visual_preview
```

## Run

```
python3 tests/visual/capture.py --out <run-dir>            # every scene, every profile
python3 tests/visual/capture.py --out <run-dir> --scenes 'baseline-*' --profiles 'mac-*'
python3 tests/visual/capture.py --list                     # the planned runs
python3 tests/visual/capture.py --verify --scenes baseline-two-band --profiles 'mac-1512x869@2'
python3 tests/visual/capture.py --verify --against <stored-run-dir> --scenes baseline-two-band
python3 tests/visual/build_gallery.py <run-dir>            # writes <run-dir>/index.html
python3 tests/visual/build_gallery.py <run-root>           # review page, see Design review
```

`--scenes` and `--profiles` take comma-separated names or shell globs.
`--jobs N` sets how many processes run at once (default 4).
`--verify --against <dir>` captures each pair once into a temporary
directory and compares it with the stored `<dir>/<profile>/<image>.png`;
it never writes into `<dir>`.

`nereus_visual_preview --dump-geometry <file>` writes one line per visible
widget of the right panel and the pan 1 toolbar (`region path x,y wxh`),
so two scenes can be diffed to prove the workspace look moves nothing.

Each run writes `<run-dir>/<profile>/<image>.png` with a `<image>.json`
sidecar next to it, and `<run-dir>/manifest.json`. The sidecar records the
scene, profile, logical and device sizes, DPR, `QT_SCALE_FACTOR`, the screen
DPR, build tag, git HEAD and dirty state, Qt versions, platform, OS, style,
font, spectrum RHI backend, the PNG's sha256, crop rectangles, the spectrum
level checks, and what is frozen, unfrozen or not shown.

The binary itself runs one scene per process:

```
nereus_visual_preview --list
nereus_visual_preview --probe
nereus_visual_preview --scene <name> --size WxH --dpr d --out <dir> [--profile <name>]
```

It exits non-zero when the window's DPR differs from `--dpr` by more than
0.001 or its client size differs from `--size`, so a profile the screen
cannot produce fails instead of producing a wrong image.

## Design review

A run root holds two capture runs side by side:

```
<run-root>/baseline/   capture.py --out of the baseline-* scenes (today's app)
<run-root>/proposed/   capture.py --out of the proposed scenes
<run-root>/verify.txt  optional: the output of capture.py --verify
```

```
python3 tests/visual/sourcemap.py <run-root-or-run-dir> [--out <dir>] [--force] [--quiet]
python3 tests/visual/compare_retained.py <run-root> [--out <dir>]
python3 tests/visual/build_gallery.py <run-root> [--out <dir>] [--verify-log <file>] [--no-tools]
```

With no `--out` each tool writes into the run root. `--out` mirrors the
run root's layout under another directory and never writes into the run
root, which the tools then only read.

`build_gallery.py <run-root>` runs the other two first (`--no-tools` skips
them) and writes one self-contained `index.html`: a header with revision,
Qt, platform, style, fonts, profiles and the verify status; one section per
handoff item with its scenes; per-profile tabs; a Capture / Source map
toggle; proposed alone or side by side with its baseline counterpart; fit
or true scale; the crops with links to the @3x crops; the status of every
proposed element, including behaviour that stills cannot show; the retained
proof; and the known limits. Every image carries its sha256 and size.
Planned scenes with no capture yet show a "not captured yet" tile. The
build exits non-zero if any captured scene is missing from the page.
Nothing under a `reference/` folder is ever read or linked.

The verify status comes from `--verify-log`, else from `verify*.txt` or
`verify*.log` in the run root or `--out`; with none the header says so in
red. Deep links: `index.html?section=<id>&profile=<name>&view=map,pair,true`
shows one section with the source map, the baseline pair and true scale on.

### Region contract

`sourcemap.py` reads `regions` from each sidecar. A region has `rect` and
optionally `visible_rect` as `[x, y, w, h]` in logical px of the captured
top-level window, a `provenance` (`retained`, `longpath-port`, `proposed`,
`exists-not-wired`, `not-implemented`), and optionally `z`, `depth`, a
`label` and a widget `class`. A sidecar-level `"units": "device"` (or a
per-region one) switches to device px. A crop sidecar carries its parent's
regions in window coordinates; the tool offsets them by the crop's own rect
from `crops` and scaled by the image width over the crop's logical width. A `visible_rect` of
`[0, 0, 0, 0]`, or any region clipped to nothing, is skipped. The top-most
region wins: by `z` or `depth` when given (higher on top), else the
smaller of two overlapping regions.

Each `<image>.sourcemap.png` dims the capture to 45%, fills each region in
its colour at 35% and outlines it 2 device px: retained #3fa7a0, Longpath
port #d8a55f, proposed #4a7ba8, exists not wired #9a7fd1, not implemented
#7e7e85 hatched. A legend with counts sits under the image. Maps are
rewritten only when older than the capture or sidecar, or with `--force`.

### Retained proof

`compare_retained.py` pairs each proposed scene with a baseline scene at
the same profile from the sidecars, never from a fixed list: an explicit
`baseline_counterpart` key first, then `look-dialog-X` to `dialog-X` and
`setup-look-<page>` to `dialog-setup-<page>`, then the same
`spectrum_checks` data (pan centre, sample rate, span, reference level,
seeded signals) and transmit keying, preferring `baseline-two-band`,
`baseline-four-rx` and `baseline-tx`. It compares the slice flag crop and
the spectrum plot area (SpectrumWidget regions if the sidecar has them,
else the area between the strip, bar and panel crops, with the flag and
pan 1 toolbar crops excluded) and writes
`retained/retained-comparison.json` with the differing pixel count, the
largest channel delta and the bounding box, plus a diff PNG per pair
(baseline dimmed, differing pixels magenta, bounding box amber, excluded
areas grey). Each entry in `comparisons` has `status` `compared` (with
`identical` true or false) or `not-comparable` (sizes differ, with the
reason); `not_possible` lists each scene and target with no counterpart,
crop or area, and why.
The flag has a translucent background, so a flag over a different spectrum
differs where the spectrum shows through.

## Safety

The window is never shown on screen (`WA_DontShowOnScreen` on every window,
captured with an in-process `grab()`), and on macOS the process stays
background-only. It runs on a throwaway settings profile in the Qt test
sandbox (removed at exit) and refuses to start if settings would resolve to
the real NereusSDR settings. No scene opens a connection, starts discovery,
or keys a radio: the transmit scene drives display-only seams and refuses to
run if a radio connection exists.

## Profiles

| Profile | Logical size | DPR | Scenes |
| --- | --- | --- | --- |
| `mac-1512x869@2` | 1512x869 | 2.0 | all, plus detail crops |
| `laptop-1280x800@1` | 1280x800 | 1.0 | all |
| `laptop-1280x800@1.25` | 1280x800 | 1.25 | main window scenes |
| `laptop-1280x800@1.5` | 1280x800 | 1.5 | main window scenes |
| `fhd-1920x1080@1` | 1920x1080 | 1.0 | main window scenes |

`capture.py` probes the primary screen's DPR and sets
`QT_SCALE_FACTOR = dpr / screen DPR`, so on a Retina Mac (screen DPR 2) the
1.0 profile runs with a factor of 0.5. At `mac-1512x869@2` each main window
image also gets five crops in device pixels (slice A flag, right panel, top
strip, status bar, pan 1 toolbar), each with a 3x nearest-neighbour copy.

## Scenes

`--list` prints them. `baseline-*` scenes pose the main window (layouts 1,
2v and 2x2, transmit, the Main Panel header modes, the Main Panel floating);
`dialog-*` scenes open one dialog each, including one per Setup page (the
list comes from the running Setup dialog). Scenes live in a table in
`PreviewScenes.cpp`; add a scene by appending a row.

### Workspace look scenes

`look-two-band`, `look-four-rx`, `look-tx` and `look-dialog-*` (one per
`dialog-*` scene) pose the same state as the scene they are named after
with the workspace preview switch on, so they show the proposed Longpath
look (the Glass and Depth palette in `src/gui/styles/WorkspacePalette`).
The binary sets the switch itself for a `look-*` scene; `--workspace-preview`
sets it for any scene. In the app the switch is the environment variable
`NEREUS_WORKSPACE_PREVIEW=1`; with it unset nothing changes, and the
`baseline-*` and `dialog-*` scenes stay pixel-identical (check with
`--verify --against`).

The look is applied two ways:

- (a) Explicit roles at call sites, under `if (WorkspacePreview::enabled())`:
  the application palette and baseline stylesheet, the main window
  stylesheet, the top strip and menu bar, the splitter handle, the pan
  toolbar and corner buttons, the pan badges, the status bar frame, the
  container title bars, borders and scroll bars.
- (b) A scoped pass, `WorkspacePreview::themeSubtree`, that rewrites the
  retained stylesheets of a subtree through `QssTranslator`: container
  content, status bar content, pan toolbar flyouts and the Setup, TX EQ,
  TX CFC, Container Settings, Pan Layout and PureSignal dialogs.

(b) is preview scaffolding, not the final migration: it shows what the
look would be before each call site is moved to a role.

The slice flag, spectrum and waterfall plot colours, meter artwork and
EQ/CFC graph band colours are excluded. The sidecar of a look image has
`provenance: proposed-look`, a `look_coverage` block (explicit roles,
scoped pass, not migrated) and `unknown_literals_top30`, the colour
literals the scoped pass met and left alone.

### Proposed header scenes

`header-always`, `header-reveal-idle`, `header-reveal-shown`,
`header-hidden` and `arrange-on` pose the two-band data with the workspace
preview switch on (the binary sets it for these scenes) and show the
proposed compact container headers: the Main Panel in Always, Reveal (idle,
then revealed through `ContainerWidget::setHeaderRevealedForPreview`),
Hidden, and Always with Arrange on (`ContainerManager::setArrangeMode`).
At the crop profile each adds a `panel-top` crop, the Main Panel's top
120 logical px, and its @3x enlargement. Their sidecars have
`provenance: proposed` and a `proposed_values` block with the sizes and
colours the scene shows.

### Proposed four-edge workspace scenes

`edge-right`, `edge-left`, `edge-top-bottom`, `edge-all-four`,
`edge-all-four-clean`, `free-container`, `floating-container`, `drag-preview-left` and
`drag-preview-between` pose the two-band data with the workspace preview
switch on and show containers docked in the strips on all four edges of
the central workspace (`src/gui/containers/EdgeDockStrip`), free over the
spectrum and floating in their own window. The scenes add containers
from existing content types: "Meters" (the S-Meter preset and the
Power/SWR face), "Band" (the band and mode button boxes, one row each)
and "Levels" (two bar meters). Every container's header is Always,
except in `edge-all-four-clean`, where it is Reveal with the pointer away,
so no header shows or takes room. `free-container` uses Options > Place
Free, moves the container to the lower right of the 20 m waterfall and
shows the header in Reveal, shown; `floating-container` also writes the
floating window as `floating-container--floating`; the drag previews turn
Arrange on and call `ContainerManager::showDropPreviewForTest`. At the
crop profile each adds `strip-left`, `strip-top` and `strip-bottom` crops
for the strips in use. Their sidecars have `provenance: proposed` and a
`proposed_values` block, and the notes list each strip's size and
containers.

### Longpath instrument scenes

`instruments-sheet` builds the ported Longpath instruments directly, with
fixed values, on their own top-level window (not the main window): the
frequency instrument in its three forms, the bandwidth filter pane with a
fixed trace, the bar (TX: SWR, and TX: ALC at a fixed -3 dB on the
Thetis ALC scale) and the needle (RX: Signal Peak and TX: Forward
Power), each at its preferred and its minimum size.
At the crop profile it adds `frequency-digits`, `frequency-digits-min`,
`needle-hub` and `needle-hub-min` crops with @3x enlargements. Capture it
at `mac-1512x869@2` and `laptop-1280x800@1`
(`--profiles 'mac-1512x869@2,laptop-1280x800@1'`).

`mixed-container` adds a "Station" container in the left strip that
mixes Longpath instruments with Thetis meter faces, all on slice A:
frequency, filter passband, `meter.sMeter`, Longpath needle,
`meter.crossNeedle` and Longpath bar, headers Always. The Main Panel's
"SIGNAL STRENGTH" dial (`applet:s_meter`) is a singleton the Main Panel
holds, so the Station container uses the `meter.sMeter` face instead.
`mixed-container-reveal` is the same with every header Reveal. After the
data is fed, each needle gets one reading through
`MeterPoller::readingUpdated` and each filter pane one trace refresh from
the fed pan. The sidecars carry a `regions` array (each entry's rect and
visible rect, headers, viewport and strips in window coordinates) with a
per-region provenance: `longpath-port`, `retained` or `proposed`.

### Workspace profile scenes

The profile scenes seed four workspace profiles into the isolated settings
profile (fixed save time 2026-10-07 14:32 UTC) on the same two-band
station: slice A 7.185 LSB on 40 m, slice B 14.225 USB on 20 m. A profile
holds container arrangement only, so each sidecar's `slices` array shows
the same slice states in all four.

- `profile-ssb`: Main Panel right, Station container left, headers Reveal.
- `profile-dx`: Main Panel right, Longpath frequency A and B left, Longpath
  signal needles A and B top, headers Always with source chips. Needle B
  stays offline: the meter poller has no slice B reading in the preview, and the scene does not fake one.
- `profile-contest`: all four strips (Meters top, Levels and Filter A
  left, Band bottom, Main Panel right), headers Always.
- `profile-ft8`: a narrow right strip with frequency and filter for slice
  B, the Main Panel in the left strip, headers Hidden.
- `profile-rail-rest`: the profile rail at rest (only the active badge, left
  of the centre title), plus a `top-strip` crop.
- `profile-rail-open`: the rail fanned out as a popover, the pointer over
  the D badge and its tooltip composited.
- `profile-new-menu` and `profile-manage-dialog`: the rail's New menu and
  the Workspace Profiles dialog, composited over the window and also
  captured alone (`--menu`, `--dialog`). Capture them at
  `mac-1512x869@2` and `laptop-1280x800@1`.

Every profile scene's sidecar carries a `rail_measure` object: the
collapsed and open rail widths, the title and network readout edges, the
free space between them and how far the open popover covers the readout.

## Determinism

The same scene at the same profile on the same machine and build gives
identical pixels; `capture.py --verify` runs each pair twice and prints
`IDENTICAL`, or the differing pixel count and bounding box, and exits
non-zero on any difference. What makes it repeatable:

- The spectrum and waterfall come from a seeded band model, never from a
  radio or the clock. Each band has its own noise floor (80 m -100,
  40 m -104, 20 m -110, 15 m -114 dBm, measured as the median of the
  displayed trace). The spectrum is one settled frame with about 3 dB of
  per-bin jitter. Waterfall rows are generated one per row period (30 ms by
  default, the waterfall's own setting), each row a fresh draw of Rayleigh
  noise (its deepest nulls held at -4 dB, see below) under the band's
  signals at that instant: SSB voices with a syllabic envelope, pauses and
  formant contours across 300-2700 Hz, CW keyed in Morse at 22 wpm, and FT8
  as 8-tone FSK at 6.25 Hz spacing and 0.16 s symbols in 15 s cycles. Each
  pan logs one check: the displayed level at its strongest signal, which
  must be within 1 dB of the stated level, and the displayed noise median.
- The waterfall AGC (on by default) keys its palette to each row's minimum
  and maximum less and plus 12 dB. Unbounded Rayleigh nulls drag the low end
  down and paint the noise green, so the row noise is held at no more than
  4 dB under its mean, and rows run hotter than the averaged trace (speech
  peaks +6 dB, CW key-down +3 dB), as live rows do. 120 rows are pushed
  ahead of the visible ones so the AGC has settled. A band whose strongest
  signal is only a little over the noise still paints its noise lighter,
  as the app does live.
- The waterfall ticker is paused and every row is pushed explicitly.
- Every running timer under the window is stopped, as are the meter poller
  and the slice meter pump; the UTC clock reads 14:32:10 UTC, the network,
  latency and CPU readouts are fixed, and the S-meter needle is allowed to
  settle before capture.
- Anything that could not be frozen is listed under `unfrozen` in the
  sidecar.

## Limits

- Pixels are only comparable on the same OS, Qt version and fonts. On a Mac,
  the 1, 1.25 and 1.5 profiles show the layout at those sizes, but Core Text
  rasterises the glyphs: Windows ClearType and Linux FreeType hinting give
  different text shapes and widths at the same scale.
- Fractional scale factors are applied by Qt on top of the screen's DPR, not
  by a real 125% or 150% Windows display.
- Amp, Tuner and RF-Kit applets are not shown: they appear only once a
  device setting that starts discovery or a connection is on.
- The tool shows the current app, plus the proposed workspace look in the
  `look-*` scenes, the proposed headers in the `header-*` and
  `arrange-on` scenes and the four-edge workspace in the `edge-*`,
  `free-container`, `floating-container` and `drag-preview-*` scenes and the
  Longpath instruments in `instruments-sheet` and `mixed-container*` and the
  workspace profiles in the `profile-*` scenes. Other proposed designs are added as new scenes.
