// NereusSDR for iOS: Setup in the workspace panels: pictures at default and accessibility sizes, and where a value row lays out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
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

    @Test("The Setup slider puts a value along its track, on its step, and takes a slider without steps")
    func sliderPlacesValues() {
        #expect(SetupSlider.fraction(-40, in: -80...0) == 0.5)
        #expect(SetupSlider.fraction(20, in: -80...0) == 1)
        #expect(SetupSlider.fraction(5, in: 5...5) == 0)
        #expect(SetupSlider.value(at: 0.52, in: 0...100, step: 5) == 50)
        #expect(SetupSlider.value(at: 1.4, in: 0...100, step: 5) == 100)
        #expect(SetupSlider.value(at: 0.25, in: 0...10, step: 0) == 2.5)
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
        .toggleStyle(WorkspaceSwitchStyle())
        .preferredColorScheme(.dark)
        .environment(\.dynamicTypeSize, dynamicType)
        .environment(\.setupLayoutProbe, probe)
        let host = UIHostingController(rootView: root)
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
