// NereusSDR for iOS: Setup in the workspace panels: pictures at default and accessibility sizes, and where a value row lays out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-51 (W6, W7, W9; spec sections 7.1, 7.2, 7.4): Setup's pages drawn
/// in the workspace panels on the phone, at the default text size and at
/// accessibility extra large: Navigation, DSP's AGC/ALC page the Core
/// describes, and a described page whose rows are greyed with their
/// reasons. At the accessibility sizes a number row's value field goes
/// under its title; at the default size it sits beside it. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the pages are written there as PNGs.
@Suite("Setup in the workspace panels", .serialized)
@MainActor
struct SetupWorkspaceShotTests {
    typealias Described = SetupDescribedPagesTests

    static let navigation: [SetupTree.Route] = [.category("General"), .page(.navigation)]
    static let agcAlc: [SetupTree.Route] = [.category("DSP"), .described(category: "dsp", page: "dsp.agcAlc")]
    /// This phone's peak settings, greyed by the fake Core's display extras with their values shown.
    static let peaks: [SetupTree.Route] = [.category("Display"),
                                           .described(category: "display", page: "display.spectrumPeaks")]
    static let multimeter: [SetupTree.Route] = [.category("Display"),
                                                .described(category: "display", page: "display.multimeter")]
    static let sizes: [(name: String, size: DynamicTypeSize)] = [("default", .large),
                                                                  ("axl", .accessibility3)]

    @Test("Navigation, AGC/ALC and a greyed described page, at default and accessibility extra large")
    func pictures() async throws {
        let rig = try await Described.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup(try Described.coreCategories())
        let app = rig.app
        #expect(await Described.settle { app.setupPages.isCurrent && app.setupFeed.description(for: "dsp") != nil })
        let numbers = try Self.numberRowIds(app, category: "dsp", page: "dsp.agcAlc")
        #expect(!numbers.isEmpty, "AGC/ALC has no number row to lay out")
        let peakNumbers = try Self.numberRowIds(app, category: "display", page: "display.spectrumPeaks")
        #expect(!peakNumbers.isEmpty, "Spectrum Peaks has no number row to lay out")

        for (name, size) in Self.sizes {
            _ = try await Self.shoot("setup-ws-navigation-\(name)", rig: rig, path: Self.navigation, dynamicType: size)
            let probe = try await Self.shoot("setup-ws-agc-\(name)", rig: rig, path: Self.agcAlc,
                                             dynamicType: size)
            Self.expectLayout(probe, ids: numbers, stacked: size.isAccessibilitySize)
            // Greyed number rows, each with its reason under it.
            let peaks = try await Self.shoot("setup-ws-peaks-\(name)", rig: rig, path: Self.peaks, dynamicType: size)
            Self.expectLayout(peaks, ids: peakNumbers, stacked: size.isAccessibilitySize)
        }

        // Away from the Core every row of a described page greys, with its reason under it.
        await app.disconnect()
        #expect(await Described.settle { !app.setupPages.isCurrent })
        for (name, size) in Self.sizes {
            _ = try await Self.shoot("setup-ws-greyed-\(name)", rig: rig, path: Self.multimeter, dynamicType: size)
        }
    }

    @Test("A value row's layout follows the text size: under its title only at the accessibility sizes")
    func valueRowStacksAtAccessibilitySizes() {
        for size in DynamicTypeSize.allCases {
            #expect(SetupValueRowLayout<EmptyView, EmptyView>.stacks(size) == size.isAccessibilitySize)
        }
    }

    /// I2 of the final review: a Setup slider is the stock slider, so the
    /// value moves only while its thumb is dragged, a tap on the track
    /// changes nothing, a vertical drag scrolls the page and VoiceOver
    /// adjusts it, all as the system slider does. Through the fake Core
    /// (the request seam): a tap on the slider (its activation) sends no
    /// request, and a VoiceOver adjust sends the value it moves to.
    @Test("A Setup slider is the stock slider: a tap sends no request, a VoiceOver adjust sends its value")
    func sliderSendsOnlyWhenMoved() async throws {
        let rig = try await Described.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup(try Described.coreCategories())
        let app = rig.app
        #expect(await Described.settle { app.setupPages.isCurrent && app.setupFeed.description(for: "dsp") != nil })
        let isSliderWrite: @Sendable (LinkMessage) -> Bool = { message in
            guard case .propertyWrite(let write) = message else { return false }
            return write.properties.contains { $0.name == "txLevelerMaxGain" }
        }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        try await Self.hosted(rig: rig, path: Self.agcAlc) { window in
            // The TX Leveler's Max Gain: editable with no slice chosen.
            let slider = try #require(Self.adjustable("dsp.agcAlc.txLevelerMaxGain", in: window),
                                      "AGC/ALC draws no adjustable slider for the TX Leveler's Max Gain")
            let before = slider.accessibilityValue

            // A tap on the slider gives it no value and sends nothing.
            _ = slider.accessibilityActivate()
            await ShotWait.laidOut(window)
            #expect(await rig.station.waitForMessage(within: .seconds(2), matching: isSliderWrite) == nil,
                    "a tap on the slider sent a request")
            #expect(slider.accessibilityValue == before, "a tap on the slider moved it")

            // VoiceOver's adjust moves it one step and sends that value.
            slider.accessibilityIncrement()
            if slider.accessibilityValue == before {
                slider.accessibilityDecrement()
            }
            let sent = await rig.station.waitForMessage(within: .seconds(10), matching: isSliderWrite)
            #expect(sent != nil, "a VoiceOver adjust of the slider sent no request")
        }
    }

    @Test("A swipe to delete starts only outside the row's fields (final review, Minor 4)")
    func swipeStartsOutsideTheFields() {
        let fields = [CGRect(x: 10, y: 4, width: 180, height: 32), CGRect(x: 200, y: 4, width: 90, height: 32)]
        #expect(!SetupSwipeToDelete.swipes(from: CGPoint(x: 150, y: 20), fields: fields))
        #expect(!SetupSwipeToDelete.swipes(from: CGPoint(x: 280, y: 20), fields: fields))
        #expect(SetupSwipeToDelete.swipes(from: CGPoint(x: 300, y: 20), fields: fields))
        #expect(SetupSwipeToDelete.swipes(from: CGPoint(x: 195, y: 20), fields: fields))
        #expect(SetupSwipeToDelete.swipes(from: CGPoint(x: 150, y: 20), fields: []))
    }

    /// The adjustable accessibility element under the element with `id`.
    static func adjustable(_ id: String, in root: NSObject) -> NSObject? {
        guard let control = SetupTypedEntryTests.element(id, in: root) else {
            return nil
        }
        var queue: [NSObject] = [control]
        var visited = 0
        while !queue.isEmpty, visited < 2_000 {
            let node = queue.removeFirst()
            visited += 1
            if node.accessibilityTraits.contains(.adjustable) {
                return node
            }
            if let elements = node.accessibilityElements as? [NSObject] {
                queue += elements
            } else {
                let count = node.accessibilityElementCount()
                if count != NSNotFound, count > 0 {
                    queue += (0..<count).compactMap { node.accessibilityElement(at: $0) as? NSObject }
                }
            }
            if let view = node as? UIView {
                queue += view.subviews
            }
        }
        return nil
    }

    /// The Setup tab at `path` in an upright phone's window, under the
    /// app's presentation root, for `body` to act on.
    static func hosted(rig: Described.Rig, path: [SetupTree.Route],
                       _ body: (UIWindow) async throws -> Void) async throws {
        let size = CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        rig.router.path = path
        let root = SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
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

    /// The number rows a described page draws, by their ids.
    static func numberRowIds(_ app: AppModel, category: String, page: String) throws -> [String] {
        let description = try #require(app.setupFeed.description(for: category))
        let found = try #require(description.pages.first { $0.id == page })
        return found.sections.flatMap(\.controls)
            .filter { DescribedControl.row(for: $0) == .numberRow && $0.readout == nil }
            .map(\.id)
    }

    /// Each number row's value field: under its title when `stacked`, beside it otherwise.
    static func expectLayout(_ probe: SetupLayoutProbe, ids: [String], stacked: Bool) {
        var measured = 0
        for id in ids {
            guard let label = probe.frame(id, .label), let value = probe.frame(id, .value) else {
                continue
            }
            measured += 1
            if stacked {
                #expect(value.minY >= label.maxY - 0.5, "\(id): the value field is not under its title")
                #expect(abs(value.minX - label.minX) < 1, "\(id): the value field does not start under its title")
            } else {
                #expect(value.minY < label.maxY && value.maxY > label.minY, "\(id): the value field is not beside its title")
                #expect(value.minX >= label.maxX, "\(id): the value field is not after its title")
            }
        }
        #expect(measured > 0, "No number row was laid out on the page")
    }

    /// Draws the Setup tab at `path` in an upright phone's window with the
    /// Setup switch, as the app's root gives it, and returns where its value
    /// rows were laid out.
    static func shoot(_ name: String, rig: Described.Rig, path: [SetupTree.Route],
                      dynamicType: DynamicTypeSize, height: CGFloat = 874,
                      width: CGFloat = 402) async throws -> SetupLayoutProbe {
        let size = CGSize(width: width, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        rig.router.path = path
        let probe = SetupLayoutProbe()
        let root = VStack(spacing: 0) {
            SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil)
            TabBar(selection: .constant(.setup), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .preferredColorScheme(.dark)
        .environment(\.dynamicTypeSize, dynamicType)
        .environment(\.setupLayoutProbe, probe)
        let host = ShotHosting.host(root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return probe
    }
}
