import WidgetKit
import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// The one timeline provider every widget here shares. It reads the App Group
// snapshot the app wrote and NEVER touches the network — see the rationale in
// SharedSnapshot.swift.
// ─────────────────────────────────────────────────────────────────────────────

struct SnapshotEntry: TimelineEntry {
    let date: Date
    let snapshot: WidgetSnapshot
}

struct SnapshotProvider: TimelineProvider {

    /// Shown in the widget gallery. Plausible sample data, never real —
    /// the gallery renders before the user has granted anything.
    func placeholder(in context: Context) -> SnapshotEntry {
        SnapshotEntry(date: Date(), snapshot: WidgetSnapshot(
            generatedAt: Date(), unreadAlerts: 3,
            latestAlerts: [
                WidgetAlert(id: "1", ruleName: "Tokyo Bay",
                            title: "Tsunami advisory issued",
                            sourceId: "jma-tsunami", matchedAt: Date())
            ],
            latestQuake: WidgetQuake(title: "Off Ibaraki", magnitude: 4.2,
                                     intensity: "3", at: Date(),
                                     lat: 36.3, lon: 140.6),
            activeWarnings: ["Heavy rain – Kanto"], connected: true))
    }

    func getSnapshot(in context: Context,
                     completion: @escaping (SnapshotEntry) -> Void) {
        // The gallery preview must never look broken, so fall back to the
        // placeholder when nothing has been written yet.
        let s = SharedSnapshot.read()
        completion(SnapshotEntry(date: Date(),
                                 snapshot: s.connected ? s : placeholder(in: context).snapshot))
    }

    func getTimeline(in context: Context,
                     completion: @escaping (Timeline<SnapshotEntry>) -> Void) {
        let entry = SnapshotEntry(date: Date(), snapshot: SharedSnapshot.read())
        // `.after` is a REQUEST, not a guarantee — iOS decides the real budget.
        // 15 minutes matches the BGTask cadence in item 37; asking for less
        // just burns the allowance without making anything fresher, because
        // nothing new can appear until the app runs again anyway.
        let next = Date().addingTimeInterval(15 * 60)
        completion(Timeline(entries: [entry], policy: .after(next)))
    }
}
