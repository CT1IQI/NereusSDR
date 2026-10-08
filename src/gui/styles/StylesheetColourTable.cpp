// =================================================================
// src/gui/styles/StylesheetColourTable.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See StylesheetColourTable.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08 - Written by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include "StylesheetColourTable.h"

namespace NereusSDR::Workspace::StylesheetColours {

namespace {

constexpr const char* kBg = "background";
constexpr const char* kBorder = "border";
constexpr const char* kText = "text";
constexpr const char* kAny = "";

} // namespace

// Grouped by role. The comment names the use sites the row was checked
// against (file under src/gui, line at the time of writing).
const QVector<ColourRow>& rows()
{
    static const QVector<ColourRow> table = {
        // ── Surfaces ─────────────────────────────────────────────
        { "#141c28", kBg, "app-bg" },            // NetworkDiagnosticsDialog:206 dialog
        { "#101a26", kBg, "panel" },             // RemoteDiagnosticsDialog:224 detail scroll
        { "#101b2a", kBg, "panel" },             // TitleBar:203 route popup frame
        { "#131326", kBg, "panel" },             // SetupDialog:363 tree, DspSetupPages:2758 and
                                                 // FilterPresetsSetupPage:81,123 tables and lists
        { "#16202c", kBg, "panel" },             // PanadapterApplet:410 floating title bar
        { "#0a0c14", kBg, "panel" },             // SpectrumWidget:662 rgba(10,12,20,200) label
        { "#101824", kBg, "panel" },             // NoticeCard:50 rgba(16,24,36,230) card
        { "#0d1b28", kBg, "statusbar-bg" },      // ConnectionPanel:613 status strip

        // ── Recessed ─────────────────────────────────────────────
        { "#0a0a15", kBg, "inset" },             // ConnectionPanel:371 radio table
        { "#0f0f1e", kBg, "inset-top" },         // SpotHubDialog:275 alternate rows
        { "#111122", kBg, "inset-top" },         // ContainerSettingsDialog:906 frame,
                                                 // MmioEndpointsDialog:95 and
                                                 // MmioVariablePickerPopup:78 alternate rows

        // ── Controls ─────────────────────────────────────────────
        { "#12202e", kBg, "button" },            // ConnectionPanel:387,450,1351 header, popup, menu;
                                                 // AddCustomRadioDialog:115 field
        { "#152535", kBg, "button" },            // DeviceCard:70,78, AudioVaxPage:112,164,
                                                 // AudioAdvancedPage:99,106 combos and popups
        { "#172534", kBg, "button" },            // ContainerContentRegistry:463 tile
        { "#172535", kBg, "button" },            // RemoteDiagnosticsDialog:115,118 tabs, combo
        { "#1a1a2e", kBg, "button" },            // SpotHubDialog:213,217,248,256,286,346 fields,
                                                 // header, tabs
        { "#1a2030", kBg, "button" },            // FilterPresetsSetupPage:78,127,247,262,
                                                 // DspSetupPages:2762,2919, FilterPresetEditDialog:61
        { "#1b3044", kBg, "button" },            // TitleBar:205 route popup buttons
        { "#1e2c3a", kBg, "button" },            // NetworkDiagnosticsDialog:245 Reset
        { "#1e3048", kBg, "button" },            // NoticeCard:53 button
        { "#1e3050", kBg, "button" },            // NetworkDiagnosticsDialog:267 Close
        { "#202838", kBg, "button" },            // FilterPresetsSetupPage:80,264, DspSetupPages:2922,
                                                 // FilterPresetEditDialog:63 spin and drop-down buttons
        { "#292f39", kBg, "button" },            // CoresSetupPage:128 tab gradient bottom
        { "#141e2d", kBg, "button" },            // rgba(20,30,45,*): SpectrumOverlayPanel:147,304,1440,
                                                 // DiversityDialog:179, StyleConstants kOverlayBtnBg
        { "#1e2837", kBg, "button" },            // SpectrumOverlayPanel:515 rgba(30,40,55,220)
        { "#233347", kBg, "raised-top" },        // CoresSetupPage:120 button gradient top
        { "#253545", kBg, "button-hover" },      // NetworkDiagnosticsDialog:251 :hover
        { "#253d60", kBg, "button-hover" },      // NetworkDiagnosticsDialog:273 :hover
        { "#254060", kBg, "button-hover" },      // SpectrumOverlayPanel:900 :hover
        { "#2a4055", kBg, "button-hover" },      // SpectrumOverlayMenu:96 :hover
        { "#2a4060", kBg, "button-hover" },      // DspSetupPages:2781,2926,
                                                 // FilterPresetsSetupPage:156,296,
                                                 // FilterPresetEditDialog:66 :hover
        { "#3f444d", kBg, "button-hover" },      // CoresSetupPage:128 tab gradient top
        { "#3b424e", kBg, "button-hover" },      // CoresSetupPage:130 selected tab gradient bottom
        { "#3b424e", kBorder, "button-hover" },  // CoresSetupPage:131 selected tab bottom edge
        { "#203b52", kBg, "button-alt-hover" },  // RemoteDiagnosticsDialog:116 selected tab
        { "#535966", kBg, "button-alt-hover" },  // CoresSetupPage:130 selected tab gradient top
        { "#0d2040", kBg, "raised-bottom" },     // SpectrumOverlayPanel:901 :pressed
        { "#16232f", kBg, "raised-bottom" },     // SpectrumOverlayMenu:97 :pressed
        { "#1a2530", kBg, "raised-bottom" },     // NetworkDiagnosticsDialog:252 :pressed
        { "#1a2840", kBg, "raised-bottom" },     // DspSetupPages:2782, FilterPresetsSetupPage:157,
                                                 // FilterPresetEditDialog:67 :pressed
        { "#1a2a48", kBg, "raised-bottom" },     // NetworkDiagnosticsDialog:274 :pressed
        { "#2e4050", kBorder, "border" },        // NetworkDiagnosticsDialog:246
        { "#2e4060", kBorder, "border" },        // NetworkDiagnosticsDialog:268
        { "#2e4e6e", kBorder, "border" },        // SpectrumOverlayMenu:71,83,93, MainWindow:1696
        { "#305070", kBorder, "border" },        // SpectrumWidget:9150
        { "#36506a", kBorder, "border" },        // TitleBar:203,205 route popup
        { "#3a3a3a", kBorder, "border" },        // CatLogWindow:47, TciLogWindow:129 log view
        { "#406080", kBorder, "border" },        // NoticeCard:50 card
        { "#5078a0", kBorder, "border" },        // NoticeCard:53 button
        { "#506070", kBorder, "border" },        // SpectrumOverlayPanel:817,826 fill swatch frame
        { "#53606d", kBorder, "border" },        // CoresSetupPage:162 tab row underline
        { "#334455", kText, "border" },          // AboutDialog:195,302,342,405 QFrame line colour
        { "#59616a", kBorder, "border-muted" },  // CoresSetupPage:129 tab
        { "#4a5a6a", kBg, "border-muted" },      // MasterOutputWidget:104 disabled handle

        // ── Disabled ─────────────────────────────────────────────
        { "#303850", kBg, "disabled-bg" },       // ConnectionPanel:513, AddCustomRadioDialog:137
        { "#404060", kBg, "disabled-bg" },       // SpotHubDialog:229
        { "#202030", kBorder, "disabled-border" }, // FilterPresetsSetupPage:297
        { "#4a5a6a", kText, "disabled-text" },   // MasterOutputWidget:96,116,127 :disabled
        { "#505060", kText, "disabled-text" },   // FilterPresetsSetupPage:297 :disabled
        { "#606878", kText, "disabled-text" },   // ConnectionPanel:513, AddCustomRadioDialog:137

        // ── Text ─────────────────────────────────────────────────
        { "#d0d0d0", kText, "text" },            // CatLogWindow:47, TciLogWindow:128 log text
        { "#d8e4f0", kText, "text" },            // NoticeCard:52,53
        { "#dddddd", kText, "text" },            // Hl2IoBoardTab:403 "#ddd" value
        { "#e0e8f0", kText, "text" },            // RxApplet:724,771 menus, PopupMenuStyle kPopupMenu
        { "#9fb4c8", kText, "title-text" },      // PanadapterApplet:425 floating title
        { "#80a0b0", kText, "text-secondary" },  // DisplaySetupPages:1819,2043
        { "#99aabb", kText, "text-secondary" },  // AboutDialog:225,335
        { "#9aa5b1", kText, "text-secondary" },  // AmpApplet:227,245, Rf2ksApplet:169,254,274,
                                                 // RfKitPage:200,285, FourO3APage:267
        { "#a0b0c0", kText, "text-secondary" },  // SpotHubDialog:240,1297,1411,1454,
                                                 // AddCustomRadioDialog:406, SupportDialog:250
        { "#a0b8c8", kText, "text-secondary" },  // MetricLabel:66 value
        { "#aaaaaa", kText, "text-secondary" },  // Hl2IoBoardTab:399,427,431,460,475,585 "#aaa"
        { "#aabbcc", kText, "text-secondary" },  // AboutDialog:187,398
        { "#b0c0d0", kText, "text-secondary" },  // TransmitSetupPages:1117 note
        { "#cccccc", kText, "text-secondary" },  // Tgxl/PgxlAdvancedPage "#ccc" on the neutral chip
        { "#5a8aaa", kText, "text-tertiary" },   // DspSetupPages:754 note
        { "#667788", kText, "text-tertiary" },   // AboutDialog:447-502 small print
        { "#6a8090", kText, "text-tertiary" },   // PanadapterApplet:435 dock button
        { "#7d8893", kText, "text-tertiary" },   // Rf2ksApplet:77 nickname
        { "#808080", kText, "text-tertiary" },   // SpotHubDialog:229-2546 labels, :disabled
        { "#808890", kText, "text-tertiary" },   // SpotHubDialog:257,346,682,1630,1819,2613
        { "#888888", kText, "text-tertiary" },   // "#888": LanScanDialog:203, DspOptionsPage:720,
                                                 // FourO3APage:235-747, OcOutputsHfTab:165,
                                                 // DiagnosticsPhaseHPages:127; Hl2IoBoardTab:735
        { "#999999", kText, "text-tertiary" },   // Tgxl/PgxlAdvancedPage "#999" info
        { "#404858", kText, "text-inactive" },   // MainWindow:12090-15128 and the notch/TCI off
                                                 // state, ConnectionPanel:521,529,1353, StatusBadge Off
        { "#404858", kBg, "text-inactive" },     // StatusBadge Off and OverflowChip rgba(64,72,88,46)
        { "#506070", kText, "text-inactive" },   // :disabled in AudioOutputsPage, AudioTxInputPage,
                                                 // DeviceCard, AudioVaxPage; VaxApplet tag labels
        { "#587080", kText, "text-inactive" },   // PopupMenuStyle kPopupMenu disabled item
        { "#5a7085", kText, "text-inactive" },   // PanadapterApplet:419 grip
        { "#6080a0", kText, "text-inactive" },   // PureSignalApplet:117 off label

        // ── Accent and information ───────────────────────────────
        { "#00c8ff", kText, "accent" },          // SetupPage:351,402, TransmitSetupPages:1191,1282,
                                                 // DspSetupPages:1257, RxApplet:805 value labels
        { "#00d4f0", kText, "accent" },          // DisplaySetupPages:2251 section label
        { "#5985b8", kBorder, "accent" },        // AddCustomRadioDialog:709,792
        { "#2a5a8a", kBorder, "accent" },        // AudioVaxPage:137 BYO status
        { "#5fa8ff", kText, "accent" },          // NetworkDiagnosticsDialog:82, StatusBadge Info
        { "#5fa8ff", kBg, "accent" },            // StatusBadge Info rgba(95,168,255,26)
        { "#66ccff", kText, "accent" },          // MainWindow:13046 TCI "#6cf" clients
        { "#66cce6", kText, "accent" },          // TitleBar:205 route popup
        { "#6699ff", kText, "accent" },          // Hl2IoBoardTab:407,782, OcOutputsHfTab:200 RX
        { "#80b0d0", kText, "accent" },          // PaSetupPages:296 info banner
        { "#8aabcf", kText, "accent" },          // AddCustomRadioDialog:794 info
        { "#00d4f8", kBorder, "selected-border" }, // ContainerSettingsDialog:277 OK button
        { "#0f2030", kBg, "badge-info-bg" },     // AudioVaxPage:136 BYO status
        { "#101828", kBg, "badge-info-bg" },     // AddCustomRadioDialog:792 info frame

        // ── Selection ────────────────────────────────────────────
        { "#005578", kBg, "selected-top" },      // ConnectionPanel:383,451,1352 selected row/item
        { "#2a5a8a", kBg, "selected-top" },      // RxApplet:727,774, PopupMenuStyle kPopupMenu
        { "#4a6f8f", kBg, "selected-top" },      // TciLogWindow:130 selection
        { "#e0f0ff", kText, "selected-text" },   // SpotHubDialog:283 selected row
        { "#0096b7", kBg, "sel-hover" },         // ConnectionPanel:512, AddCustomRadioDialog:136,
                                                 // SupportDialog:307 primary :hover
        { "#00c8e8", kBg, "sel-hover" },         // ContainerSettingsDialog:282 :hover
        { "#00c8f0", kBg, "sel-hover" },         // SpotHubDialog:228, MainWindow:18345 :hover
        { "#33c8e8", kBg, "sel-hover" },         // AboutDialog:516 :hover

        // ── Good and live ────────────────────────────────────────
        { "#0a1a10", kBg, "ok-bg" },             // RadioStatusPage:129 connected bar
        { "#0f2a1a", kBg, "ok-bg" },             // AudioVaxPage:125 native status
        { "#152530", kBg, "ok-bg" },             // DeviceCard:109 good chip
        { "#2a6a3a", kBorder, "ok-border" },     // AudioVaxPage:126
        { "#00aa00", kText, "ok" },              // AntennaAlexAlex2Tab:499 active label
        { "#00d286", kText, "ok" },              // CoresSetupPage:602 current Core
        { "#33aa33", kText, "ok" },              // RxApplet:1199 AGC info
        { "#5fff8a", kText, "ok" },              // StatusBadge On
        { "#5fff8a", kBg, "ok" },                // StatusBadge On rgba(95,255,138,26)
        { "#7ec850", kText, "ok" },              // RfKitPage:752,769, FourO3APage:424
        { "#80c8a0", kText, "ok" },              // DeviceCard:112 good chip
        { "#20c060", kText, "live" },            // MainWindow:18297 status label
        { "#4caf50", kText, "live" },            // SpotHubDialog:577,652,1804, FourO3APage:705,732,
                                                 // RadeApplet state label
        { "#66ff66", kText, "live" },            // MainWindow:13042 TCI "#6f6" on

        // ── Measured and warning ─────────────────────────────────
        { "#2a1a00", kBg, "measured-bg" },       // AudioVaxPage:102,147, AudioAdvancedPage:87
        { "#2a2010", kBg, "measured-bg" },       // SpotHubDialog:500 banner
        { "#302810", kBg, "measured-bg" },       // NnrControls:243 chip
        { "#3a2a00", kBg, "measured-bg" },       // TitleBar:852 feature button, NyiOverlay:10
        { "#3a2a10", kBg, "measured-bg" },       // RadioInfoTab:199 banner
        { "#806020", kBorder, "measured-border" }, // TitleBar:852, SpotHubDialog:501
        { "#806820", kBorder, "measured-border" }, // NnrControls:243
        { "#a07020", kBorder, "measured-border" }, // RadioInfoTab:199
        { "#b87300", kBorder, "measured-border" }, // AudioVaxPage:103,148, AudioAdvancedPage:88
        { "#e8a030", kBorder, "measured" },      // AudioAdvancedPage:94 :hover edge
        { "#e8a030", kText, "measured" },        // AudioVaxPage:105,150,215, AudioAdvancedPage:90
        { "#b08020", kText, "measured" },        // ConnectionPanel:462 note
        { "#d09060", kText, "measured" },        // PaSetupPages:295 warning banner
        { "#d9a441", kText, "measured" },        // AmpApplet:235, Rf2ksApplet:263, TunerApplet:176 stale
        { "#e0a030", kText, "measured" },        // AudioOutputsPage:71,77 reason
        { "#ffc800", kText, "measured" },        // DspOptionsPage:173 warning sign
        { "#ffc800", kBg, "measured" },          // RxApplet:2314 overload rgba(255,200,0,0.20)
        { "#ffc800", kBorder, "measured" },      // RxApplet:2316 overload rgba(255,200,0,0.40)
        { "#ffcc66", kText, "measured" },        // SpotHubDialog:500, RfKitPage, FourO3APage,
                                                 // RadioInfoTab:204 pending and notice text
        { "#ffd166", kText, "measured" },        // NnrControls:243 chip
        { "#ffd700", kText, "measured" },        // RxApplet:2315 overload, StatusBadge and
                                                 // AdcOverloadBadge Warn
        { "#ffd700", kBg, "measured" },          // StatusBadge and AdcOverloadBadge rgba(255,215,0,30)
        { "#e6c200", kText, "warn" },            // SpotHubDialog:836-3340 warnings, RadeApplet label
        { "#e87c1e", kText, "warn" },            // PgxlAdvancedPage:481
        { "#e8c01e", kText, "warn" },            // Tgxl/PgxlAdvancedPage warnLabel

        // ── Transmit ─────────────────────────────────────────────
        { "#eecc66", kText, "tx" },              // MainWindow:13053 TCI "#ec6" client holds TX
        { "#ff6655", kText, "tx" },              // Hl2IoBoardTab:781, OcOutputsHfTab:207 TX
        { "#ff6060", kText, "tx" },              // StatusBadge and AdcOverloadBadge Tx
        { "#ff6060", kBg, "tx" },                // StatusBadge and AdcOverloadBadge rgba(255,96,96,51)
        { "#ff6060", kBorder, "tx" },            // StationBlock:158 rgba(255,96,96,102)
        { "#ff6868", kText, "tx" },              // RxApplet:2324 overload
        { "#ff5a5a", kBg, "tx" },                // RxApplet:2323 overload rgba(255,90,90,0.25)
        { "#ff5a5a", kBorder, "tx" },            // RxApplet:2325 overload rgba(255,90,90,0.50)
        { "#ff8080", kText, "tx" },              // TxApplet:3118 take transmit while on air

        // ── Danger and errors ────────────────────────────────────
        { "#2a1010", kBg, "badge-tx-bg" },       // AddCustomRadioDialog:780 error frame
        { "#301520", kBg, "badge-tx-bg" },       // DeviceCard:119 error chip
        { "#401010", kBg, "badge-tx-bg" },       // ContainerSettingsDialog:1135 delete
        { "#4a2020", kBg, "badge-tx-bg" },       // ConnectionPanel:525,645 destructive
        { "#602020", kBg, "danger-bg" },         // ConnectionPanel:528,648,
                                                 // ContainerSettingsDialog:1137 :hover
        { "#b02020", kBg, "danger-bg" },         // ReplaceDeviceDialog:210, TakeTransmitDialog:131
        { "#b03030", kBg, "danger-bg" },         // SpotHubDialog:2644
        { "#cc4040", kBg, "danger-bg" },         // MainWindow:18356 Report a Bug
        { "#c8a8a8", kText, "danger-text" },     // ConnectionPanel:525,645
        { "#ffb0b0", kText, "danger-text" },     // ContainerSettingsDialog:1135, ContentPropertyEditor:350
        { "#603040", kBorder, "danger" },        // DeviceCard:120
        { "#6a3030", kBorder, "danger" },        // ConnectionPanel:526,646
        { "#802020", kBorder, "danger" },        // SpotHubDialog:2645, ContainerSettingsDialog:1135
        { "#804040", kBorder, "danger" },        // AddCustomRadioDialog:759
        { "#c14848", kBorder, "danger" },        // AddCustomRadioDialog:780
        { "#ff6666", kBorder, "danger" },        // Hl2IoBoardTab:498
        { "#c83232", kBg, "danger" },            // AntennaAlexAntennaControlTab:233 rgba(200,50,50,0.08)
        { "#c83232", kBorder, "danger" },        // AntennaAlexAntennaControlTab:233 rgba(200,50,50,0.4)
        { "#e04848", kText, "danger" },          // SoundSystemLine:31, AudioVaxPage:227
        { "#e05060", kText, "danger" },          // DeviceCard:122
        { "#e06060", kText, "danger" },          // AddCustomRadioDialog:758
        { "#e07070", kText, "danger" },          // AddCustomRadioDialog:782
        { "#e53935", kText, "danger" },          // FourO3APage:710,713,741
        { "#ff0000", kText, "danger" },          // GeneralOptionsPage:709 "red" warning
        { "#ff6666", kText, "danger" },          // RxApplet:763 :hover, GeneralSetupPages:102,
                                                 // Hl2IoBoardTab:498
        { "#ffa3a3", kText, "danger" },          // CoresSetupPage:209,341

        // ── Badges ───────────────────────────────────────────────
        { "#555555", kBg, "badge-off-bg" },      // Tgxl/PgxlAdvancedPage "#555" neutral chip
    };
    return table;
}

const QVector<KeptLiteral>& kept()
{
    static const QVector<KeptLiteral> list = {
        // ── Lit and checked states (render as today) ─────────────
        { "#000", kText, "lit: text on the lit Hl2 I/O pin badge (Hl2IoBoardTab:850)" },
        { "#000000", kText, "lit: text on a lit fill (RxApplet:1844 :checked, PureSignalApplet:108)" },
        { "#001a33", kBg, "lit: MON on (TxApplet:770)" },
        { "#3399ff", kBorder, "lit: MON on (TxApplet:771)" },
        { "#006030", kBg, "lit: OPERATE on (Tuner, Amp, Rf2ks, TxApplet)" },
        { "#007040", kBg, "lit: OPERATE on, hover (Tuner, Amp, Rf2ksApplet)" },
        { "#008040", kBorder, "lit: OPERATE on (Tuner, Amp, Rf2ks, TxApplet)" },
        { "#8a6000", kBg, "lit: BYPASS (TunerApplet:929)" },
        { "#9a7000", kBg, "lit: BYPASS hover (TunerApplet:931)" },
        { "#a07000", kBorder, "lit: BYPASS (TunerApplet:929)" },
        { "#008ba8", kBorder, "lit: the checked filter pill border (SpotHubDialog:268); the same "
                              "value edges the Start button at :227, so a row would move the pill" },
        { "#00e060", kText, "lit: Setup toggle :checked (SetupPage:380)" },
        { "#1a5030", kBg, "lit: Setup toggle :checked (SetupPage:380)" },
        { "#1a2a1a", kBg, "lit: the on half of the RxApplet and flag DSP badges (RxApplet:2356)" },
        { "#2a3a2a", kBg, "lit: on badge hover (RxApplet:2358)" },
        { "#adff2f", kAny, "lit: on badge edge and text, also the off badge's hover edge "
                           "(RxApplet:1164,2356-2378)" },
        { "#445", kBorder, "on/off pair: the off half of the RxApplet badge (RxApplet:1162,2376)" },
        { "#556", kText, "on/off pair: the off half of the RxApplet badge (RxApplet:1163,2377)" },
        { "#1a1a1a", kBg, "two jobs: the off badge fill of an on/off pair (RxApplet:1162,2376) "
                          "and the CAT/TCI log view fill (CatLogWindow:47, TciLogWindow:127)" },
        { "#206030", kBg, "lit: SpotHub toggle on (SpotHubDialog:221,2724)" },
        { "#603020", kBg, "on/off pair: SpotHub toggle off (SpotHubDialog:223,2726)" },
        { "#305040", kBorder, "on/off pair: SpotHub toggle edge (SpotHubDialog:222,2725)" },
        { "#20c060", kBg, "lit: PureSignal on badge (PureSignalApplet:107)" },
        { "#4488ff", kAny, "lit: :checked text (RxApplet:691), the same value unchecked at :712, "
                           "and a spot colour default (SpotHubDialog:1107)" },
        { "#66aaff", kText, "on/off pair: hover of the RxApplet:712 text (RxApplet:715)" },
        { "#20e070", kAny, "lit: on dot (TransmitSetupPages:1144)" },
        { "#6a3030", kBg, "lit: flag :checked (VfoWidget:853,2698)" },
        { "#2a2a1a", kBg, "lit: flag :checked (VfoWidget:786)" },
        { "#ffcc44", kText, "lit: flag :checked (VfoWidget:786)" },
        { "#ffffff", kAny, "text on a filled or lit state; upstream keeps white rowless "
                           "(WorkspacePalette legacyRows)" },
        { "#fff", kAny, "text on a filled or lit state, as #ffffff" },
        { "white", kAny, "text on a filled state, as #ffffff; Hl2OptionsTab:502 data readout" },
        { "rgba(0,112,192,180)", kBg, "lit: the overlay's active button (SpectrumOverlayPanel:154, "
                                      "kMenuBtnActive) shares the value with its hover" },

        // ── Indicators, LEDs and state dots ──────────────────────
        { "#00cc00", kBg, "LED (AntennaAlexAlex2Tab:496)" },
        { "#00cc44", kBg, "LED (AntennaAlexAlex1Tab:844, Alex2Tab:665, OcOutputsHfTab:625)" },
        { "#33dd55", kBorder, "LED edge (OcOutputsHfTab:626)" },
        { "#444444", kBg, "LED off (AntennaAlexAlex1Tab:847, Alex2Tab:668)" },
        { "#666666", kBg, "LED off (AntennaAlexAlex2Tab:203,502)" },
        { "#22cc44", kAny, "LED and lit pin badge (Hl2IoBoardTab:261,681,846,849,942)" },
        { "#44ff44", kBg, "LED on (Hl2IoBoardTab:788, OcLedStripWidget:146)" },
        { "#88ff88", kBorder, "LED on edge (Hl2IoBoardTab:788, OcLedStripWidget:146)" },
        { "#222", kBg, "LED and pin badge off (Hl2IoBoardTab, OcLedStripWidget:148)" },
        { "#444", kAny, "LED and pin badge off fill and edge, and the Hl2 group frame edge "
                        "(Hl2IoBoardTab:373-837)" },
        { "#555", kBorder, "LED off edge (Hl2IoBoardTab, OcLedStripWidget:148) and the "
                           "Hl2OptionsTab:503 readout edge" },
        { "#606060", kBg, "LED off (Hl2IoBoardTab:253,264)" },
        { "#2a2a2a", kBg, "two jobs: Hl2 group frame (Hl2IoBoardTab:373,721) and the "
                          "slice-colour button fill (RxApplet:1842)" },
        { "#1a2f1a", kBg, "lit: Hl2 pin frame on (Hl2IoBoardTab:714,847)" },
        { "#2a5a2a", kBorder, "lit: Hl2 pin frame on (Hl2IoBoardTab:714)" },
        { "#ff4500", kBg, "LED TX (OcLedStripWidget:141)" },
        { "#ff7f50", kBorder, "LED TX edge (OcLedStripWidget:141)" },
        { "#33dd88", kBg, "state dot (SoundSystemLine:27, AudioVaxPage:223)" },
        { "#e04848", kBg, "state dot (SoundSystemLine:29, AudioVaxPage:225)" },
        { "#7a8088", kAny, "RADE state dot and label, idle (RadeApplet:143,151)" },
        { "#4caf50", kBg, "RADE state dot, synced (RadeApplet:76)" },
        { "#e6c200", kBg, "RADE state dot, searching (RadeApplet:75)" },
        { "#a78bfa", kAny, "RADE purple: the mode's data colour (RadeApplet:54, VfoWidget:1242)" },
        { "#ffe4c4", kBg, "PureSignal form status colours, as Thetis (PsForm:156)" },
        { "black", kAny, "PureSignal form status colours, as Thetis (PsForm:156,165); "
                         "Hl2OptionsTab:502 data readout" },
        { "#00ff00", kAny, "PureSignal LED (PsForm:1024) and spot colour defaults" },
        { "#ffff00", kAny, "PureSignal LED (PsForm:1024), slice D and spot colour defaults" },
        { "#101010", kText, "text on a state fill (PsaIndicatorWidget:524)" },
        { "#696969", kBg, "PureSignal banner state, Thetis DimGray: PS off or no reading "
                          "(PsaIndicatorWidget:144,446, sheet at :524)" },
        { "#2e8b57", kBg, "PureSignal banner state, Thetis SeaGreen: PS on, not keyed "
                          "(PsaIndicatorWidget:145,447)" },
        { "#1e90ff", kBg, "PureSignal banner state, Thetis DodgerBlue: feedback above 181 "
                          "(PsaIndicatorWidget:149,470)" },
        // Already a workspace value: the call site builds it from a role
        // when the switch is on, so it is never a legacy literal.
        { "#423414", kBg, "feature button hover, shiftL(measured-bg, 10) set by TitleBar:862 "
                          "with the switch on" },
        { "#ffaa00", kAny, "SWR protection LED (TxApplet:1222)" },
        { "#39c167", kAny, "connection state pill, online (ConnectionPanel:155)" },
        { "#d39c2a", kAny, "connection state pill, stale (ConnectionPanel:156)" },
        { "#c14848", kText, "connection state pill, offline (ConnectionPanel:157,624)" },
        { "#208040", kBg, "LED (CatApplet:237)" },
        { "#a04040", kBg, "LED (CatApplet:237)" },
        { "#34c759", kBg, "state dot (Rf2ksApplet:627)" },
        { "#e64949", kBg, "state dot (Rf2ksApplet:628)" },
        { "#00cc66", kBg, "state dot (TciApplet:72)" },
        { "#00e0e0", kBg, "state dot (TciApplet:73)" },
        { "#d4a017", kBg, "Clarity badge, paused (SpectrumOverlayPanel:1517)" },
        { "#22b14c", kBg, "Clarity badge, active (SpectrumOverlayPanel:1518)" },
        { "#2d8a2d", kBg, "status pill, good (Tgxl/PgxlAdvancedPage)" },
        { "#aa2222", kBg, "status pill, fault (Tgxl/PgxlAdvancedPage)" },
        { "#c88000", kBg, "status pill, warning (Tgxl/PgxlAdvancedPage)" },
        { "#888", kBg, "status pill, unknown (Tgxl/PgxlAdvancedPage)" },

        // ── Data colours ─────────────────────────────────────────
        { "#00d4ff", kAny, "slice A colour (ControlRanges kSliceColours) in slice buttons; "
                           "DiversityDialog:380 marks a stored slot the same way" },
        { "#ff40ff", kAny, "slice B colour" },
        { "#40ff40", kAny, "slice C colour" },
        { "#ffa000", kAny, "slice E colour" },
        { "#00e0c0", kAny, "slice F colour" },
        { "#ff6080", kAny, "slice G colour" },
        { "#b080ff", kAny, "slice H colour" },
        { "#ff0000", kAny, "spot colour default (SpotHubDialog:1365 Calling Me) in a swatch" },
        { "red", kBg, "same value as the Calling Me swatch default; a row for #ff0000 "
                      "background would recolour the swatch (GeneralOptionsPage:1172)" },
        { "#00ffff", kAny, "spot colour default (SpotHubDialog:1333)" },
        { "#d2b48c", kAny, "spot colour default (SpotHubDialog:860)" },
        { "#00e5ff", kAny, "spectrum fill colour default in its swatch (SpectrumOverlayPanel:817); "
                           "frequency digits on the flag (VfoWidget:903)" },
        { "#e0a030", kBg, "the RADIO speaker slider's identity amber beside the PC slider "
                          "(MasterOutputWidget kRadioSlider)" },
        { "#003060", kAny, "ClientChain category badge (ClientChainApplet:398)" },
        { "#004060", kAny, "ClientChain category badge (ClientChainApplet:370)" },
        { "#60b8e0", kAny, "ClientChain category badge (ClientChainApplet:398)" },
        { "#3a0a0a", kAny, "ClientChain category badge (ClientChainApplet:460)" },
        { "#402000", kAny, "ClientChain category badge (ClientChainApplet:403)" },
        { "#c08040", kAny, "ClientChain category badge (ClientChainApplet:403)" },
        { "#804000", kAny, "ClientChain category badge (ClientChainApplet:326,383)" },
        { "rgba(255,120,60,46)", kAny, "TX filter overlay fill, user setting (StyleConstants:386)" },
        { "#ff7833", kAny, "TX filter overlay edge, user setting (StyleConstants:387)" },
        { "#ffaa70", kAny, "TX filter overlay label, user setting (StyleConstants:388)" },

        // ── Two jobs a property kind cannot separate ─────────────
        { "#1a3a5a", kBg, "two jobs: the STANDBY button (Tuner, Amp, Rf2ksApplet, PhoneCwApplet) "
                          "and the spot table's selected row (SpotHubDialog:282)" },
        { "#1a3050", kBg, "two jobs: the Re-tune button (SpectrumOverlayPanel:898) and the "
                          "selected table row (DspSetupPages:2761, FilterPresetsSetupPage:126)" },
        { "#1d3045", kBg, "two jobs: a :hover (AudioVaxPage:119,171) and a list selection "
                          "(AudioAdvancedPage:107)" },

        // ── No role for the shade ────────────────────────────────
        { "#3a2500", kBg, "amber button :hover; no measured hover role (AudioAdvancedPage:94)" },
        { "#1a1000", kBg, "amber button :pressed; no measured pressed role (AudioAdvancedPage:95)" },
        { "#504000", kBg, "amber button :hover; no measured hover role (TitleBar:854)" },
        { "#a08030", kBorder, "amber button :hover edge (TitleBar:854)" },
        { "#c84040", kBg, "danger button :hover; no danger hover role (SpotHubDialog:2646)" },
        { "#902020", kBg, "danger button :pressed; no danger pressed role (SpotHubDialog:2647)" },
        { "#dd5050", kBg, "danger button :hover; no danger hover role (MainWindow:18358)" },

        // ── White washes ─────────────────────────────────────────
        // A translucent white lightens whatever is below it; no role is a wash.
        { "rgba(255,255,255,0.06)", kAny, "white wash: overload badge idle (RxApplet:1358,2305)" },
        { "rgba(255,255,255,0.45)", kAny, "white wash: overload badge idle (RxApplet:1359,2306)" },
        { "rgba(255,255,255,0.12)", kAny, "white wash: overload badge idle (RxApplet:1360,2307)" },
        { "rgba(255,255,255,0.1)", kAny, "white wash (OcOutputsHfTab:391,630)" },
        { "rgba(255,255,255,0.2)", kAny, "white wash (OcOutputsHfTab:392,631)" },
        { "rgba(255,255,255,0.5)", kAny, "white wash (OcOutputsTab:148)" },
        { "rgba(255,255,255,40)", kAny, "white wash: overlay button edge (SpectrumOverlayPanel:148)" },
        { "rgba(255,255,255,50)", kAny, "white wash (VfoWidget:906)" },
        { "rgba(255,255,255,80)", kAny, "white wash (VfoWidget:1300)" },

        // ── The slice flag (VfoWidget, VfoStyles) ────────────────
        // Skipped by design (design-facts B11); listed so the decision is
        // explicit when a flag sheet reaches a popup the flag opens.
        { "#6888a0", kAny, "slice flag (VfoStyles.h:50, VfoWidget)" },
        { "#ffc040", kAny, "slice flag (VfoWidget:2691)" },
        { "#d09020", kAny, "slice flag (VfoWidget:2690)" },
        { "rgba(20,30,50,230)", kAny, "slice flag (VfoWidget:3202,3211)" },
        { "rgba(40,55,80,240)", kAny, "slice flag (VfoWidget:3206)" },
        { "rgba(80,100,130,180)", kAny, "slice flag (VfoWidget:3202,3211)" },
        { "rgba(204,32,32,220)", kAny, "slice flag (VfoWidget:3215)" },
        { "rgba(255,100,100,80)", kAny, "slice flag (VfoWidget:3336)" },
        { "rgba(255,100,100,120)", kAny, "slice flag (VfoWidget:3338)" },
        { "#505050", kAny, "slice flag (VfoWidget:1091)" },
        { "#e0e040", kAny, "slice flag (VfoWidget:1127)" },
    };
    return list;
}

} // namespace NereusSDR::Workspace::StylesheetColours
