# Visual preview tool

`nereus_visual_preview` builds the real NereusSDR main window with no radio,
fills it with fixed illustrative data, and writes full-resolution PNGs. It
captures the app as it is today: it changes nothing about how the app looks.
`capture.py` runs it across scenes and screen profiles, and `build_gallery.py`
turns a run into a page you can browse.

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
python3 tests/visual/build_gallery.py <run-dir>            # writes <run-dir>/index.html
```

`--scenes` and `--profiles` take comma-separated names or shell globs.
`--jobs N` sets how many processes run at once (default 4).

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

## Determinism

The same scene at the same profile on the same machine and build gives
identical pixels; `capture.py --verify` runs each pair twice and prints
`IDENTICAL`, or the differing pixel count and bounding box, and exits
non-zero on any difference. What makes it repeatable:

- The spectrum and waterfall come from a seeded generator (noise
  +/-1.5 dB, SSB voice contours, CW carriers, FT8 tones), never from a radio
  or the clock. Each pan logs one check: the displayed level at its
  strongest signal, which must be within 1 dB of the stated level.
- The waterfall ticker is paused and every row is pushed explicitly.
- Every running timer under the window is stopped, as are the meter poller
  and the slice meter pump; the UTC clock reads 14:32:07 UTC, the network,
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
- The tool shows the current app only. Proposed designs are added as new
  scenes.
