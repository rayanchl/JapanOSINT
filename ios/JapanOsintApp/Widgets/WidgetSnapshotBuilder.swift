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
            let (unread, events) = try await (unreadReq, eventsReq)

            let latest = events.prefix(5).map { e in
                WidgetAlert(
                    id: e.id,
                    ruleName: e.rule_name,
                    title: e.item_title,
                    sourceId: e.item_source_id,
                    matchedAt: parseDate(e.matched_at) ?? Date()
                )
            }

            let snapshot = WidgetSnapshot(
                generatedAt: Date(),
                unreadAlerts: unread,
                latestAlerts: Array(latest),
                // Quake / warnings intentionally left empty rather than
                // fabricated — they get their own real source in a later pass.
                latestQuake: nil,
                activeWarnings: [],
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

    private static let isoFractional: ISO8601DateFormatter = {
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return f
    }()

    private static func parseDate(_ s: String) -> Date? {
        isoFractional.date(from: s) ?? ISO8601DateFormatter().date(from: s)
    }
}
