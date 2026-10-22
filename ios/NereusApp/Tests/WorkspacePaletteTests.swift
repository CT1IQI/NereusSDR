// NereusSDR for iOS: tests of the workspace tokens, the treatments and the kept data colors
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-49 (tokens), R-IOS-50 (treatments) and R-IOS-54 (kept colors),
/// docs/architecture/2026-10-20-phone-workspace-look-design.md sections 4, 5
/// and 10. The desktop tokens' hex values are checked against the desktop's
/// header by tests/compliance/test_ios_workspace_palette.py.
@Suite("Workspace palette", .serialized)
@MainActor
struct WorkspacePaletteTests {
    /// A color's 0xRRGGBB in sRGB.
    static func hex(_ color: Color) -> UInt32 {
        var red: CGFloat = 0, green: CGFloat = 0, blue: CGFloat = 0, alpha: CGFloat = 0
        UIColor(color).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
        func byte(_ value: CGFloat) -> UInt32 { UInt32((min(max(value, 0), 1) * 255).rounded()) }
        return byte(red) << 16 | byte(green) << 8 | byte(blue)
    }

    static func shown(_ value: UInt32) -> String { String(format: "#%06x", value) }

    @Test func txRedResolvesToTheDesktopsRed() {
        var red: CGFloat = 0, green: CGFloat = 0, blue: CGFloat = 0, alpha: CGFloat = 0
        UIColor(WorkspacePalette.txRed).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
        #expect(Int((red * 255).rounded()) == 0xC2)
        #expect(Int((green * 255).rounded()) == 0x5A)
        #expect(Int((blue * 255).rounded()) == 0x5C)
        #expect(alpha == 1)
    }

    @Test func computedValuesEqualTheDesktopsComputation() {
        let expected: [(String, Color, UInt32)] = [
            ("litGreenTop", WorkspacePalette.litGreenTop, 0x306247),
            ("litGreenMid", WorkspacePalette.litGreenMid, 0x254D38),
            ("litGreenBottom", WorkspacePalette.litGreenBottom, 0x1D3D2C),
            ("litGreenText", WorkspacePalette.litGreenText, 0xBBE1C6),
            ("litGreenRim", WorkspacePalette.litGreenRim, 0x3E7E5C),
            ("litGreenRimTop", WorkspacePalette.litGreenRimTop, 0x48946B),
            ("litAmberTop", WorkspacePalette.litAmberTop, 0x4C3B16),
            ("litAmberMid", WorkspacePalette.litAmberMid, 0x33280F),
            ("litAmberBottom", WorkspacePalette.litAmberBottom, 0x20190A),
            ("litAmberRimTop", WorkspacePalette.litAmberRimTop, 0x83672F),
            ("glassSelBorderTop", WorkspacePalette.glassSelBorderTop, 0x4D89C1),
        ]
        for (name, color, value) in expected {
            #expect(Self.shown(Self.hex(color)) == Self.shown(value), "\(name)")
        }
    }

    @Test func phoneStepsAreTheSpecsValues() {
        let expected: [(String, Color, UInt32)] = [
            ("textBright", WorkspacePalette.textBright, 0xECECF0),
            ("accentFill", WorkspacePalette.accentFill, 0x4A7BA8),
            ("accentText", WorkspacePalette.accentText, 0x5E8DB8),
            ("tabChosen", WorkspacePalette.tabChosen, 0x8FB4D9),
            ("noticeWarn", WorkspacePalette.noticeWarn, 0xA8853F),
            ("keyedRim", WorkspacePalette.keyedRim, 0xE0A3A4),
        ]
        for (name, color, value) in expected {
            #expect(Self.shown(Self.hex(color)) == Self.shown(value), "\(name)")
        }
    }

    @Test func shapesAndSteps() {
        #expect(WorkspacePalette.chipRadius == 7)
        #expect(WorkspacePalette.panelRadius == 10)
        #expect(WorkspacePalette.raisedMidStop == 0.55)
        #expect(WorkspacePalette.disabledOpacity == 0.4)
        #expect(WorkspacePalette.disabledOpacity == SetupNumberRow.greyedOpacity)
    }

    @Test func lookForegrounds() {
        #expect(Self.hex(workspaceForeground(.raised)) == Self.hex(WorkspacePalette.textPrimary))
        #expect(Self.hex(workspaceForeground(.glass)) == Self.hex(WorkspacePalette.glassSelText))
        #expect(Self.hex(workspaceForeground(.litGreen)) == 0xBBE1C6)
        #expect(Self.hex(workspaceForeground(.litAmber)) == 0xD8A55F)
        #expect(Self.hex(workspaceForeground(.keyed)) == 0xFFFFFF)
        #expect(Self.hex(workspaceForeground(.danger)) == 0xF0DCDC)
    }

    // MARK: Kept colors (W2, R-IOS-54)

    @Test func keptColorsDoNotChange() {
        let expected: [(String, Color, UInt32)] = [
            ("BandColours.frequency", BandColours.frequency, 0x00E5FF),
            ("BandColours.bandwidth", BandColours.bandwidth, 0x00C8FF),
            ("BandColours.rxAntenna", BandColours.rxAntenna, 0x4488FF),
            ("BandColours.timestamp", BandColours.timestamp, 0xC8DCFF),
            ("DialColours.indexGlow", DialColours.indexGlow, 0x00E5FF),
            ("DialColours.indexTick", DialColours.indexTick, 0xC8F8FF),
            ("ChromeColours.txFilterEdge", ChromeColours.txFilterEdge, 0xFF7833),
        ]
        for (name, color, value) in expected {
            #expect(Self.shown(Self.hex(color)) == Self.shown(value), "\(name)")
        }
    }

    /// Each slice letter's color as the Core sends it today (ControlRanges
    /// kSliceColours, src/core/ControlRanges.h:1041-1050), and the neutral
    /// grey for a slice the catalogue has no color for.
    @Test func sliceColorsParseAsToday() {
        let today: [(String, UInt32)] = [
            ("A", 0x00D4FF), ("B", 0xFF40FF), ("C", 0x40FF40), ("D", 0xFFFF00),
            ("E", 0xFFA000), ("F", 0x00E0C0), ("G", 0xFF6080), ("H", 0xB080FF),
        ]
        let colours = today.map { "#" + String(format: "%06X", $0.1) }
        for (index, (letter, value)) in today.enumerated() {
            #expect(BandSlice.letter(forIndex: index) == letter)
            let hex = colours[index]
            #expect(BandSlice.colour(forIndex: index, in: colours) == hex)
            #expect(Self.shown(Self.hex(BandColours.slice(hex))) == Self.shown(value), "slice \(letter)")
        }
        #expect(Self.hex(BandColours.slice(BandSlice.colourUnknown)) == 0x8090A0)
        #expect(Self.hex(BandColours.slice("not a colour")) == 0x8090A0)
    }

    // MARK: The switch

    /// The switch's binding, changed from outside the view.
    @MainActor final class SwitchState: ObservableObject {
        @Published var isOn = false
    }

    private struct SwitchRoot: View {
        @ObservedObject var state: SwitchState
        let style: Bool

        var body: some View {
            Group {
                if style {
                    Toggle("Mute", isOn: $state.isOn).toggleStyle(WorkspaceSwitchStyle())
                } else {
                    Toggle("Mute", isOn: $state.isOn).toggleStyle(.switch)
                }
            }
            .accessibilityIdentifier("workspaceSwitch")
            .padding(.horizontal, 16)
            .frame(width: 320, height: 60)
            .background(WorkspacePalette.panelBg)
        }
    }

    /// Hosts the switch in a window and runs `body` with the window.
    private func hosted(style: Bool, state: SwitchState, _ body: (UIWindow) async throws -> Void) async throws {
        let size = CGSize(width: 320, height: 60)
        let window = try BandFlagShotTests.window(size: size)
        let host = UIHostingController(rootView: SwitchRoot(state: state, style: style).preferredColorScheme(.dark))
        // The window is shorter than the phone's safe area; lay out in all of it.
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try await body(window)
    }

    /// How many of the window's drawn pixels are the inset's ground and how
    /// many lie in the glass, eight bits a channel at one pixel a point.
    private func faceCounts(_ window: UIWindow) -> (inset: Int, glass: Int) {
        let format = UIGraphicsImageRendererFormat()
        format.preferredRange = .standard
        format.scale = 1
        let image = UIGraphicsImageRenderer(bounds: window.bounds, format: format).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        guard let cgImage = image.cgImage else {
            return (0, 0)
        }
        let width = cgImage.width, height = cgImage.height
        var bytes = [UInt8](repeating: 0, count: width * height * 4)
        guard let context = CGContext(data: &bytes, width: width, height: height, bitsPerComponent: 8,
                                      bytesPerRow: width * 4,
                                      space: CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB(),
                                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
            return (0, 0)
        }
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))
        let inset = Self.hex(WorkspacePalette.insetBg)
        var counts = (inset: 0, glass: 0)
        for index in stride(from: 0, to: bytes.count, by: 4) {
            let value = UInt32(bytes[index]) << 16 | UInt32(bytes[index + 1]) << 8 | UInt32(bytes[index + 2])
            if value == inset {
                counts.inset += 1
            } else if Self.isGlass(value) {
                counts.glass += 1
            }
        }
        return counts
    }

    /// The switch reads to VoiceOver as the stock switch does: the same
    /// traits and the same on and off value.
    @Test func switchKeepsTheStockSwitchsAccessibility() async throws {
        for isOn in [false, true] {
            let stockState = SwitchState(), styledState = SwitchState()
            stockState.isOn = isOn
            styledState.isOn = isOn
            var stock: (UIAccessibilityTraits, String?) = ([], nil)
            try await hosted(style: false, state: stockState) { window in
                let element = try #require(SetupTypedEntryTests.element("workspaceSwitch", in: window))
                stock = (element.accessibilityTraits, element.accessibilityValue)
            }
            try await hosted(style: true, state: styledState) { window in
                let element = try #require(SetupTypedEntryTests.element("workspaceSwitch", in: window))
                #expect(element.accessibilityTraits == stock.0, "on \(isOn)")
                #expect(element.accessibilityValue == stock.1, "on \(isOn)")
                #expect(element.accessibilityLabel == "Mute")
                #expect(element.accessibilityTraits.contains(.toggleButton) ==
                        stock.0.contains(.toggleButton))
            }
            #expect(stock.1 == (isOn ? "1" : "0"))
        }
    }

    /// Flipping the binding moves the face between the inset (off) and the
    /// glass (on), counted over the whole window: the ground is kPanelBg
    /// and the label kTextPrimary, so nothing else is drawn in either.
    @Test func switchFaceFollowsTheBinding() async throws {
        let state = SwitchState()
        // A track of 51 by 31 less its knob and rim leaves well over 200 pixels of face.
        let face = 200
        try await hosted(style: true, state: state) { window in
            let off = faceCounts(window)
            #expect(off.inset > face && off.glass == 0, "off: \(off)")

            // The knob slides over 0.2 s; wait for the face to settle.
            state.isOn = true
            // A few edge pixels on the glass's dark end may round to the
            // inset's ground; the inset's face is gone.
            _ = await ShotWait.until(seconds: 3) { faceCounts(window).glass > face }
            let on = faceCounts(window)
            #expect(on.glass > face && on.inset * 10 < off.inset, "on: \(on), off: \(off)")

            state.isOn = false
            _ = await ShotWait.until(seconds: 3) { faceCounts(window).glass == 0 }
            let offAgain = faceCounts(window)
            #expect(offAgain.inset > face && offAgain.glass == 0, "off again: \(offAgain)")
        }
    }

    /// Between the glass's two ends, channel by channel.
    static func isGlass(_ value: UInt32) -> Bool {
        let top = hex(WorkspacePalette.glassSelTop), bottom = hex(WorkspacePalette.glassSelBot)
        return [16, 8, 0].allSatisfy { shift in
            let channel = (value >> UInt32(shift)) & 0xFF
            let a = (top >> UInt32(shift)) & 0xFF, b = (bottom >> UInt32(shift)) & 0xFF
            return channel >= min(a, b) && channel <= max(a, b)
        }
    }
}
