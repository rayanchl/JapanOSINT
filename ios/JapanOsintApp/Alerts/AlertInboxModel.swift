import Foundation
import SwiftUI
import Combine

// ─────────────────────────────────────────────────────────────────────────────
// Store for the cross-rule notification inbox (roadmap 11).
//
// Owned by whoever wants the tab badge — the badge and the list MUST read the
// same `unreadCount`, so this is a `@StateObject` at the root and an
// `@ObservedObject` inside `AlertInboxView`. Two independent counters would
// drift the moment one of them is refreshed and the other isn't, and a badge
// that disagrees with the list is worse than no badge.
//
// The API is passed in per call (`reload(api:)`) rather than captured, because
// `APIClient.api` is rebuilt whenever the user edits the backend URL in
// Settings — the same convention `GeocodeSearchModel` uses.
//
// Every mutation here is optimistic-with-rollback: the row flips instantly, and
// if the server refuses, the previous state comes back AND `error` is set. No
// action silently no-ops.
// ─────────────────────────────────────────────────────────────────────────────

@MainActor
final class AlertInboxModel: ObservableObject {

    enum Filter: String, CaseIterable, Identifiable {
        case all, unread
        var id: String { rawValue }
        var label: String { self == .all ? "All" : "Unread" }
    }

    /// A mute the user performed in this session. Kept so the UI can offer a
    /// one-tap undo and can say, in words, that muting is temporary and per
    /// rule — not a deletion.
    struct MutedRule: Identifiable, Equatable {
        let ruleId: String
        let ruleName: String
        /// Human phrase for the mute window, e.g. "1 hour".
        let durationLabel: String
        var id: String { ruleId }
    }

    /// Which slice the list shows. The view observes this and reloads — the
    /// unread filter is applied server-side (`?unread=1`) so "unread" means the
    /// same thing here as it does in the badge count.
    @Published var filter: Filter = .all

    @Published private(set) var events: [AlertInboxEvent] = []
    /// Read by the tab badge. Counts every unread, non-suppressed event for the
    /// tenant — not just the ones currently loaded into `events`.
    @Published private(set) var unreadCount: Int = 0
    @Published private(set) var loading = false
    @Published private(set) var mutedRuleIds: Set<String> = []
    @Published var error: String?
    @Published var lastMute: MutedRule?

    /// How many rows to pull. The server clamps at 500.
    private let pageLimit = 200

    // MARK: - Queries

    func isMuted(_ ruleId: String) -> Bool { mutedRuleIds.contains(ruleId) }

    // MARK: - Loading

    func reload(api: API) async {
        loading = true
        defer { loading = false }
        do {
            events = try await api.alertInbox(unreadOnly: filter == .unread,
                                              limit: pageLimit)
            error = nil
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
        await refreshUnreadCount(api: api)
    }

    /// Refresh only the badge. Cheap enough to call whenever the app comes back
    /// to the foreground or another screen changes a rule.
    func refreshUnreadCount(api: API) async {
        do {
            unreadCount = try await api.alertUnreadCount()
        } catch let e {
            // Don't clobber a more specific list error, but don't swallow this
            // either — a stale badge is exactly the failure mode to avoid.
            if error == nil {
                error = "Unread count unavailable: \(e.localizedDescription)"
            }
        }
    }

    // MARK: - Read state

    func markRead(_ event: AlertInboxEvent, api: API) async {
        guard event.unread else { return }
        let snapshotEvents = events
        let snapshotCount  = unreadCount
        markLocallyRead(event.id)
        unreadCount = max(0, unreadCount - 1)
        Haptics.selection()
        do {
            try await api.alertMarkRead(event.id)
            // In the unread slice a read row no longer belongs to the list.
            if filter == .unread { events.removeAll { $0.id == event.id } }
        } catch let e {
            events = snapshotEvents
            unreadCount = snapshotCount
            error = e.localizedDescription
            Haptics.error()
        }
    }

    func markAllRead(api: API) async {
        let snapshotEvents = events
        let snapshotCount  = unreadCount
        for e in events where e.unread { markLocallyRead(e.id) }
        unreadCount = 0
        do {
            try await api.alertMarkAllRead()
            if filter == .unread { events = [] }
            Haptics.success()
        } catch let e {
            events = snapshotEvents
            unreadCount = snapshotCount
            error = e.localizedDescription
            Haptics.error()
        }
    }

    // MARK: - Muting (per rule, reversible)

    /// Mute the rule that produced an event. `durationSec` is always concrete —
    /// an indefinite mute is deliberately not offered from the inbox, because
    /// "I don't want this right now" is a different intent from "turn this rule
    /// off forever", which lives in the rule editor.
    func mute(ruleId: String, ruleName: String, durationSec: Int,
              durationLabel: String, api: API) async {
        do {
            try await api.alertMute(ruleId, durationSec: durationSec)
            mutedRuleIds.insert(ruleId)
            lastMute = MutedRule(ruleId: ruleId, ruleName: ruleName,
                                 durationLabel: durationLabel)
            error = nil
            Haptics.warning()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    func unmute(ruleId: String, api: API) async {
        do {
            try await api.alertUnmute(ruleId)
            mutedRuleIds.remove(ruleId)
            if lastMute?.ruleId == ruleId { lastMute = nil }
            error = nil
            Haptics.success()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    func dismissMuteBanner() { lastMute = nil }

    // MARK: - Local mutation

    /// Rebuild the row with `unread == false`. `AlertInboxEvent` is a value
    /// type of `let`s, so the optimistic flip is a replacement rather than an
    /// in-place edit; the timestamp is a local placeholder that the next reload
    /// replaces with the server's.
    private func markLocallyRead(_ id: String) {
        guard let i = events.firstIndex(where: { $0.id == id }) else { return }
        let e = events[i]
        events[i] = AlertInboxEvent(
            id: e.id,
            rule_id: e.rule_id,
            rule_name: e.rule_name,
            item_uid: e.item_uid,
            matched_at: e.matched_at,
            read_at: ISO8601DateFormatter().string(from: Date()),
            unread: false,
            delivered_channels: e.delivered_channels,
            item_title: e.item_title,
            item_source_id: e.item_source_id,
            item_link: e.item_link
        )
    }
}
