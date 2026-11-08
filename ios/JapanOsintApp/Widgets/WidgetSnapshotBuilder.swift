import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// App-side builder for the widget snapshot (roadmap 35/37).
//
// The widget extension cannot reach the Keychain-backed bearer token and is
// killed for slow work, so the APP fetches what the widgets render and writes it
// to the App Group via `SharedSnapshot.write`. This runs after a foreground
// inbox refresh, on scene activation, and from the background-refresh task.
//
// It is deliberately guarded on the App Group being configured: until the
// capability is added in Xcode (see docs/ios-extensions-plan.md) `snapshotURL`
// is nil, and we no-op silently rather than tripping `SharedSnapshot.write`'s
// debug assertion on every launch.
// ─────────────────────────────────────────────────────────────────────────────

enum WidgetSnapshotBuilder {

    /// Fetch the inbox slice the widgets need and persist it. Never throws — a
    /// widget update must not disturb whatever triggered it. Returns the
    /// snapshot it wrote, or nil if the App Group is unconfigured or the fetch
    /// failed (so a BGTask can report completion honestly).
    @discardableResult
    static func refresh(api: API) async -> WidgetSnapshot? {
        // No App Group yet → nothing to write to. Silent, not asserted.
        guard AppGroup.snapshotURL != nil else { return nil }
        do {
            async let unreadReq = api.alertUnreadCount()
            async let eventsReq = api.alertInbox(unreadOnly: false, limit: 5)
            let (unread, page) = try await (unreadReq, eventsReq)
            // The widget shows five rows and the badge carries the real count,
            // so the page's `total` is not needed here — `unread` is the truth.
            let events = page.events

            let latest = events.prefix(5).map { e in
                WidgetAlert(
                    id: e.id,
                    ruleName: e.rule_name,
                    title: e.item_title,
                    sourceId: e.item_source_id,
                    matchedAt: parseDate(e.matched_at) ?? Date()
                )
            }

            // Quake and warnings are best-effort: a failure leaves them
            // nil/empty (the widget then shows no quake rather than an old or
            // invented one) and never costs the alert half of the snapshot.
            async let quakeReq = latestQuake(api: api)
            async let warnReq = activeWarnings(api: api)
            let (quake, warnings) = await (quakeReq, warnReq)

            let snapshot = WidgetSnapshot(
                generatedAt: Date(),
                unreadAlerts: unread,
                latestAlerts: Array(latest),
                latestQuake: quake,
                activeWarnings: warnings,
                connected: true
            )
            SharedSnapshot.write(snapshot)
            return snapshot
        } catch {
            // Preserve the last good snapshot on a transient failure; do not
            // blank the widget by writing an empty/disconnected one.
            #if DEBUG
            print("[widget] snapshot refresh failed: \(error)")
            #endif
            return nil
        }
    }

    /// The newest stored JMA earthquake bulletin — GET /api/intel/items
    /// ?source=jma-earthquake&limit=1 (newest first). The collector
    /// (collectors/sources/jma_earthquake.c) stores `magnitude` and
    /// `max_intensity` as strings in properties, `published_at` from JMA's
    /// `at`, and lat/lon top-level. A row whose time does not parse is
    /// dropped rather than stamped "now".
    private static func latestQuake(api: API) async -> WidgetQuake? {
        guard let env = try? await api.intelItems(source: "jma-earthquake", limit: 1),
              let it = env.data.first,
              let at = isoToDate(it.published_at) else { return nil }
        let p = it.properties ?? [:]
        let mag: Double? = {
            if let s = p["magnitude"]?.value as? String { return Double(s) }
            if let d = p["magnitude"]?.value as? Double { return d }
            if let i = p["magnitude"]?.value as? Int { return Double(i) }
            return nil
        }()
        return WidgetQuake(title: it.title ?? (p["place"]?.value as? String) ?? "地震",
                           magnitude: mag,
                           intensity: p["max_intensity"]?.value as? String,
                           at: at, lat: it.lat, lon: it.lon)
    }

    /// Offices with at least one JMA warning or advisory in force, as their
    /// stored titles ("<office>: N件の警報・注意報"). Source: the
    /// `jma-warnings` collector (collectors/sources/jma_warnings.c), one row
    /// per office upserted each run with `properties.warning_count`. Only
    /// rows fetched in the last 12 hours count: an office row left over from
    /// a run days ago is a past warning, not an active one, and the default
    /// system-wide cadence is 6 h.
    private static func activeWarnings(api: API) async -> [String] {
        guard let env = try? await api.intelItems(source: "jma-warnings", limit: 100) else { return [] }
        let cutoff = Date().addingTimeInterval(-12 * 3600)
        return env.data.compactMap { it -> String? in
            guard let fetched = isoToDate(it.fetched_at), fetched >= cutoff else { return nil }
            let v = it.properties?["warning_count"]?.value
            let n = (v as? Int) ?? (v as? Double).map { Int($0) } ?? 0
            guard n > 0 else { return nil }
            return it.title
        }
    }

    /// `alert_events.matched_at` is SQLite `datetime('now')`, which ISO 8601
    /// alone rejects — and the `?? Date()` above then stamped every alert as
    /// "now". The shared parser reads both shapes.
    private static func parseDate(_ s: String) -> Date? { isoToDate(s) }
}
