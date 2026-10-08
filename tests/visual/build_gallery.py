#!/usr/bin/env python3
# no-port-check: NereusSDR-original visual preview tool; gallery page.
#
# tests/visual/build_gallery.py  (NereusSDR)
#
#   build_gallery.py <run-dir>
#
# Writes <run-dir>/index.html: a self-contained dark page (no external CSS,
# fonts or scripts) over a capture.py run. One tab per profile; in each,
# the scenes grouped (main window, crops, dialogs, Setup pages); for every
# image a fit view, a true-scale view (CSS width = logical width, so the
# browser shows it at the size the app would draw on a screen of that DPR),
# a link to the PNG and a metadata table from its sidecar. The PNGs are
# linked relatively, not embedded, so the page travels with its run dir.
#
# Modification history (NereusSDR):
#   2026-10-07 - Written for the workspace visual review. J.J. Boyd
#                (KG4VCF), AI-assisted via Anthropic Claude Code.

import html
import json
import pathlib
import sys

GROUPS = [
    ("window", "Main window"),
    ("floating", "Floating windows"),
    ("crop", "Detail crops"),
    ("crop-3x", "Detail crops, 3x nearest-neighbour"),
    ("dialog", "Dialogs"),
    ("setup", "Setup pages"),
]


def esc(v):
    return html.escape(str(v), quote=True)


def group_of(meta):
    kind = meta.get("kind", "window")
    if kind == "dialog" and str(meta.get("scene", "")).startswith("dialog-setup-"):
        return "setup"
    return kind


def fmt_list(items):
    if not items:
        return "none"
    out = []
    for it in items:
        if isinstance(it, dict):
            item = it.get("item") or it.get("name") or ""
            reason = it.get("reason") or it.get("region") or ""
            out.append("%s: %s" % (item, reason) if reason else json.dumps(it, sort_keys=True))
        else:
            out.append(str(it))
    return "<br>".join(esc(x) for x in out)


def fmt_checks(checks):
    if not checks:
        return "none"
    rows = []
    for c in checks:
        rows.append("%s: %.0f Hz target %.1f dBm, displayed %.2f dBm (%s); waterfall %.1f..%.1f dBm" % (
            c.get("pan"), c.get("target_hz", 0), c.get("target_dbm", 0),
            c.get("displayed_dbm", 0), "within 1 dB" if c.get("within_1db") else "OUTSIDE 1 dB",
            c.get("waterfall_low_dbm", 0), c.get("waterfall_high_dbm", 0)))
    return "<br>".join(esc(r) for r in rows)


def meta_table(meta):
    env = meta.get("environment", {})
    font = env.get("font", {})
    size = meta.get("logical_size", [0, 0])
    dev = meta.get("device_size", [0, 0])
    rows = [
        ("Scene", meta.get("scene")),
        ("Profile", meta.get("profile")),
        ("Kind", meta.get("kind")),
        ("Logical size", "%d x %d" % tuple(size)),
        ("DPR", meta.get("dpr")),
        ("Device size", "%d x %d" % tuple(dev)),
        ("QT_SCALE_FACTOR", meta.get("qt_scale_factor", env.get("qt_scale_factor"))),
        ("Primary screen DPR (probed)", meta.get("probed_primary_screen_dpr")),
        ("Screen DPR seen by the window", env.get("primary_screen_dpr_scaled")),
        ("Build tag", env.get("build_tag")),
        ("Git HEAD", "%s%s" % (meta.get("git_head", "?"), " (dirty)" if meta.get("git_dirty") else "")),
        ("Qt", "runtime %s, compiled %s" % (env.get("qt_runtime"), env.get("qt_compiled"))),
        ("Platform / OS", "%s / %s" % (env.get("platform"), env.get("os"))),
        ("Style", env.get("style")),
        ("Font", "%s, %s pt, %s px" % (font.get("family"), font.get("point_size"), font.get("pixel_size"))),
        ("RHI backend", env.get("spectrum_rhi_backend")),
        ("PNG sha256", meta.get("sha256")),
        ("Provenance", meta.get("provenance")),
    ]
    if meta.get("enlargement"):
        rows.append(("Enlargement", meta.get("enlargement")))
    body = "".join("<tr><th>%s</th><td>%s</td></tr>" % (esc(k), esc(v)) for k, v in rows)
    crops = meta.get("crops") or []
    crop_txt = "<br>".join(esc("%s: logical %s, device %s" % (c.get("name"), c.get("logical"), c.get("device")))
                           for c in crops) or "none"
    body += "<tr><th>Crops</th><td>%s</td></tr>" % crop_txt
    body += "<tr><th>Spectrum checks</th><td>%s</td></tr>" % fmt_checks(meta.get("spectrum_checks"))
    body += "<tr><th>Not shown</th><td>%s</td></tr>" % fmt_list(meta.get("not_shown"))
    body += "<tr><th>Unfrozen</th><td>%s</td></tr>" % fmt_list(meta.get("unfrozen"))
    body += "<tr><th>Frozen</th><td>%s</td></tr>" % fmt_list(meta.get("frozen"))
    body += "<tr><th>Notes</th><td>%s</td></tr>" % fmt_list(meta.get("notes"))
    return "<table class='meta'>%s</table>" % body


def card(rel_png, meta, idx):
    size = meta.get("logical_size", [0, 0])
    title = pathlib.Path(rel_png).stem
    return (
        "<figure class='card' id='img%d'>"
        "<figcaption><span class='name'>%s</span>"
        "<span class='ctl'>"
        "<button type='button' data-view='fit' class='on'>Fit</button>"
        "<button type='button' data-view='true'>True scale</button>"
        "<a href='%s' target='_blank' rel='noopener'>PNG</a>"
        "<span class='dim'>%d x %d @ %s</span></span></figcaption>"
        "<div class='frame fit'><img loading='lazy' src='%s' alt='%s' data-lw='%d'></div>"
        "<details><summary>Metadata</summary>%s</details>"
        "</figure>"
    ) % (idx, esc(title), esc(rel_png), size[0], size[1], esc(meta.get("dpr")),
         esc(rel_png), esc(title), size[0], meta_table(meta))


CSS = """
:root { color-scheme: dark; --bg:#0f1218; --panel:#171c25; --line:#2a3240; --text:#d8dee9;
        --muted:#8a94a6; --accent:#38bdf8; }
* { box-sizing: border-box; }
body { margin:0; background:var(--bg); color:var(--text);
       font: 14px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
header { padding:16px; border-bottom:1px solid var(--line); }
h1 { font-size:18px; margin:0 0 4px; }
.sub { color:var(--muted); font-size:13px; }
nav.tabs { display:flex; flex-wrap:wrap; gap:4px; padding:8px 16px; border-bottom:1px solid var(--line);
           position:sticky; top:0; background:var(--bg); z-index:2; }
nav.tabs button { background:var(--panel); color:var(--text); border:1px solid var(--line);
                  border-radius:6px; padding:6px 10px; cursor:pointer; font:inherit; }
nav.tabs button.on { border-color:var(--accent); color:var(--accent); }
section.tab { display:none; padding:8px 16px 40px; }
section.tab.on { display:block; }
h2 { font-size:16px; margin:24px 0 8px; color:var(--accent); }
h3 { font-size:14px; margin:16px 0 8px; color:var(--muted); font-weight:600; }
.card { margin:0 0 20px; background:var(--panel); border:1px solid var(--line); border-radius:8px;
        overflow:hidden; }
figcaption { display:flex; flex-wrap:wrap; gap:8px; align-items:center; justify-content:space-between;
             padding:8px 10px; border-bottom:1px solid var(--line); }
.name { font-weight:600; word-break:break-all; }
.ctl { display:flex; gap:6px; align-items:center; flex-wrap:wrap; }
.ctl button { background:transparent; color:var(--muted); border:1px solid var(--line); border-radius:5px;
              padding:2px 8px; cursor:pointer; font:inherit; font-size:12px; }
.ctl button.on { color:var(--accent); border-color:var(--accent); }
.ctl a { color:var(--accent); font-size:12px; }
.dim { color:var(--muted); font-size:12px; }
.frame { overflow:auto; background:#000; }
.frame.fit img { display:block; max-width:100%; height:auto; }
.frame.true img { display:block; max-width:none; height:auto; image-rendering:auto; }
details { padding:6px 10px; }
summary { cursor:pointer; color:var(--muted); }
table.meta { border-collapse:collapse; margin-top:6px; font-size:12px; width:100%; }
table.meta th { text-align:left; color:var(--muted); font-weight:500; padding:3px 10px 3px 0;
                vertical-align:top; white-space:nowrap; width:1%; }
table.meta td { padding:3px 0; word-break:break-all; }
.empty { color:var(--muted); }
"""

JS = """
document.querySelectorAll('nav.tabs button').forEach(function (b) {
  b.addEventListener('click', function () {
    document.querySelectorAll('nav.tabs button').forEach(function (x) { x.classList.toggle('on', x === b); });
    document.querySelectorAll('section.tab').forEach(function (s) { s.classList.toggle('on', s.id === b.dataset.tab); });
    try { localStorage.setItem('nereusGalleryTab', b.dataset.tab); } catch (e) {}
  });
});
document.querySelectorAll('.card').forEach(function (c) {
  var frame = c.querySelector('.frame'), img = c.querySelector('img');
  c.querySelectorAll('.ctl button').forEach(function (b) {
    b.addEventListener('click', function () {
      c.querySelectorAll('.ctl button').forEach(function (x) { x.classList.toggle('on', x === b); });
      var t = b.dataset.view === 'true';
      frame.className = 'frame ' + (t ? 'true' : 'fit');
      img.style.width = t ? img.dataset.lw + 'px' : '';
    });
  });
});
try {
  var saved = localStorage.getItem('nereusGalleryTab');
  var btn = saved && document.querySelector('nav.tabs button[data-tab="' + saved + '"]');
  if (btn) { btn.click(); }
} catch (e) {}
"""


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: build_gallery.py <run-dir>")
    run_dir = pathlib.Path(sys.argv[1]).resolve()
    manifest_path = run_dir / "manifest.json"
    if not manifest_path.exists():
        sys.exit("no manifest.json in %s" % run_dir)
    manifest = json.loads(manifest_path.read_text())

    profiles = list(manifest.get("profiles", {}).keys())
    by_profile = {p: [] for p in profiles}
    failures = []
    for run in manifest.get("runs", []):
        if run.get("returncode") != 0:
            failures.append(run)
            continue
        for img in run.get("images", []):
            meta = json.loads((run_dir / img["json"]).read_text())
            by_profile.setdefault(run["profile"], []).append((img["png"], meta))

    tabs, sections = [], []
    idx = 0
    for n, profile in enumerate(p for p in profiles if by_profile.get(p)):
        tab_id = "tab%d" % n
        tabs.append("<button type='button' data-tab='%s' class='%s'>%s <span class='dim'>(%d)</span></button>" % (
            tab_id, "on" if n == 0 else "", esc(profile), len(by_profile[profile])))
        info = manifest["profiles"].get(profile, {})
        parts = ["<p class='sub'>Logical %s, DPR %s, QT_SCALE_FACTOR %s</p>" % (
            esc("x".join(str(v) for v in info.get("logical_size", []))), esc(info.get("dpr")),
            esc(info.get("qt_scale_factor")))]
        for key, label in GROUPS:
            items = [(png, meta) for png, meta in by_profile[profile] if group_of(meta) == key]
            if not items:
                continue
            parts.append("<h2>%s</h2>" % esc(label))
            current_scene = None
            for png, meta in items:
                if key in ("crop", "crop-3x") and meta.get("scene") != current_scene:
                    current_scene = meta.get("scene")
                    parts.append("<h3>%s</h3>" % esc(current_scene))
                parts.append(card(png, meta, idx))
                idx += 1
        sections.append("<section class='tab %s' id='%s'>%s</section>" % (
            "on" if n == 0 else "", tab_id, "".join(parts)))

    fail_html = ""
    if failures:
        fail_html = "<p class='sub'>Failed runs: %s</p>" % esc(", ".join(
            "%s @ %s" % (f["scene"], f["profile"]) for f in failures))
    not_shown = "; ".join(x.get("item", "") for x in manifest.get("not_shown", []))
    head = "%s%s" % (str(manifest.get("git_head", ""))[:10], " (dirty)" if manifest.get("git_dirty") else "")
    page = (
        "<!doctype html><html lang='en'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>NereusSDR Visual Baseline</title><style>%s</style></head><body>"
        "<header><h1>NereusSDR visual baseline</h1>"
        "<div class='sub'>%d images, captured %s from %s, provenance %s. Probed screen DPR %s. "
        "Not shown in any scene: %s.</div>%s</header>"
        "<nav class='tabs'>%s</nav>%s<script>%s</script></body></html>"
    ) % (CSS, manifest.get("image_count", 0), esc(manifest.get("started_utc", "")), esc(head),
         esc(manifest.get("provenance", "")), esc(manifest.get("probed_primary_screen_dpr")),
         esc(not_shown), fail_html, "".join(tabs), "".join(sections), JS)
    out = run_dir / "index.html"
    out.write_text(page)
    print("wrote %s (%d images)" % (out, idx))
    return 0


if __name__ == "__main__":
    sys.exit(main())
