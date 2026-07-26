import Foundation
import Combine

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 18 — timeline state.
//
// Owns the window / bucket / source-filter triple, the histogram, the keyset-
// paged event list, and the two failure modes that must never reach the user as
// a raw HTTP body:
//
//   • `window_too_wide` (400) — the server refuses windows wider than 366 days
//     because this DB is ~2.9 GB and an unbounded scan is a DoS against our own
//     server. Rendered as an explanation plus a one-tap recovery.
//   • `meta.notes` — most importantly `case_items_table_missing`, which means
//     the server answered 200 with an EMPTY timeline rather than silently
//     answering a different question. Swallowing that note would turn a
//     deliberate "I can't filter by case here" into "this case has no events".
//
// Time vocabulary is shared with the map on purpose. `anchor` is the same
// instant `PlaybackState.at` holds (nil == LIVE), so the timeline's upper bound
// and the map's replay instant are one clock, not two.
// ─────────────────────────────────────────────────────────────────────────────

/// Window presets. Deliberately shaped like `TimeWindow` in
/// `Map/PlaybackState.swift` — Int rawValue in seconds, `.label` for the chip —
/// so the two strips read as the same control. `TimeWindow` stops at 7 days
/// because that is all the map's layer cache holds; the timeline reads straight
/// out of SQLite, so it can go further.
enum TimelineWindow: Int, CaseIterable, Identifiable, Sendable {
    case h24 = 86_400
    case d7  = 604_800
    case d30 = 2_592_000

    var id: Int { rawValue }
    var seconds: Int { rawValue }

    var label: String {
        switch self {
        case .h24: return "24h"
        case .d7:  return "7d"
        case .d30: return "30d"
        }
    }
}

/// Histogram granularity. The rawValue is the wire value the server expects
/// (`bucket=hour|day`); anything else is a 400 `invalid_bucket`.
enum TimelineBucketSize: String, CaseIterable, Identifiable, Sendable {
    case hour
    case day

    var id: String { rawValue }

    var label: String {
        switch self {
        case .hour: return "Hourly"
        case .day:  return "Daily"
        }
    }

    /// Width of one bar, in seconds. Used to turn a tapped bucket into the
    /// `since`/`until` pair that narrows the event list to it.
    var seconds: TimeInterval {
        switch self {
        case .hour: return 3600
        case .day:  return 86_400
        }
    }
}

/// Push destination for an event that resolves to a real intel item. A named
/// type rather than a bare `String` so registering
/// `.navigationDestination(for:)` here can never collide with another screen in
/// the same `NavigationStack` that also pushes strings.
struct TimelineItemRoute: Hashable {
    let uid: String
    let title: String
}

/// One row in the source filter. A struct rather than a tuple because `ForEach`
/// needs an `Identifiable` (or a key path, and key paths don't address tuple
/// elements).
struct TimelineSourceOption: Identifiable, Hashable {
    let id: String
    let name: String
}

@MainActor
final class TimelineModel: ObservableObject {

    // ── Request state (user-driven) ────────────────────────────────────────

    @Published var window: TimelineWindow = .d7
    @Published var bucket: TimelineBucketSize = .day
    /// `source_id` equality, applied by the server to the *item* behind every
    /// stream so the filter means exactly one thing. nil = all sources.
    @Published var sourceFilter: String?
    /// Optional case scope. When set, the server may answer with the
    /// `case_items_table_missing` / `cases_table_missing` notes — see
    /// `explain(note:)`.
    @Published var caseId: String?
    /// Upper bound of the window. nil == LIVE (now). Mirrors
    /// `PlaybackState.at` so scrubbing the map moves the timeline with it.
    @Published var anchor: Date?

    // ── Response state ─────────────────────────────────────────────────────

    /// Histogram for the WHOLE window. Deliberately not replaced while a bar is
    /// focused, so the user never loses the shape they just tapped into.
    @Published private(set) var buckets: [TimelineBucket] = []
    @Published private(set) var events: [TimelineEvent] = []
    @Published private(set) var meta: TimelineMeta?
    /// Bar the user tapped. Non-nil narrows the event list to that bar's span
    /// via a refetch — not a client-side filter, because the list is keyset
    /// paged and an old bucket's events are usually not resident.
    @Published private(set) var focused: TimelineBucket?

    @Published private(set) var loading = false
    @Published private(set) var loadingMore = false
    @Published private(set) var hasMore = false
    @Published private(set) var error: String?
    /// Set when the server rejected the window with `window_too_wide`. Held
    /// separately from `error` so the UI can offer the recovery ("narrow to
    /// 30 days") instead of just printing a failure.
    @Published private(set) var windowTooWide = false

    /// Source list for the filter sheet. Best-effort: if it fails we still let
    /// the user pick from the sources actually seen in the loaded events.
    @Published private(set) var sources: [IntelSource] = []
    @Published private(set) var sourcesError: String?

    // ── Paging bookkeeping ─────────────────────────────────────────────────

    private let pageLimit = 100
    private var cursor: String?
    /// Window actually sent to the server, snapshotted at page 1. Page 2 must
    /// not silently slide forward because `Date()` moved on between taps.
    private var reqSince = Date()
    private var reqUntil = Date()
    /// Monotonic request id. A response whose token is stale is dropped rather
    /// than allowed to overwrite a newer window.
    private var loadToken = 0

    // ── Derived ────────────────────────────────────────────────────────────

    var isReplaying: Bool { anchor != nil }
    var isEmpty: Bool { events.isEmpty && buckets.isEmpty }
    var isFiltered: Bool { sourceFilter != nil || focused != nil }

    /// Window currently displayed, for the range caption under the histogram.
    var displayedSince: Date { reqSince }
    var displayedUntil: Date { reqUntil }

    /// Column totals across the visible histogram.
    var totals: (intel: Int, mentions: Int, alerts: Int) {
        var i = 0, m = 0, a = 0
        for b in buckets { i += b.intel; m += b.mentions; a += b.alerts }
        return (i, m, a)
    }

    /// Tallest bar, used to scale the strip. Never zero, so the draw code can
    /// divide by it unconditionally.
    var peak: Int { max(1, buckets.map { $0.total }.max() ?? 1) }

    /// Sources offered by the filter sheet: the server's catalogue when we have
    /// it, otherwise whatever the loaded events actually mention.
    var selectableSources: [TimelineSourceOption] {
        if !sources.isEmpty {
            return sources.map { TimelineSourceOption(id: $0.id, name: $0.name) }
        }
        var seen: [String] = []
        for e in events {
            if let s = e.source_id, !s.isEmpty, !seen.contains(s) { seen.append(s) }
        }
        return seen.sorted().map { TimelineSourceOption(id: $0, name: $0) }
    }

    // ── Loading ────────────────────────────────────────────────────────────

    /// Full reload: clears the focused bar and refetches window + histogram.
    func reload(using api: API) async {
        focused = nil
        await fetch(using: api, more: false, refreshBuckets: true)
    }

    /// Narrow the event list to one histogram bar (or clear it when nil). The
    /// histogram itself is left alone.
    func focus(_ bucket: TimelineBucket?, using api: API) async {
        if let b = bucket, focused?.t == b.t {
            focused = nil                       // tapping the lit bar clears it
        } else {
            focused = bucket
        }
        await fetch(using: api, more: false, refreshBuckets: focused == nil)
    }

    /// Next keyset page. No-op unless the last page came back full.
    func loadMore(using api: API) async {
        guard hasMore, !loading, !loadingMore else { return }
        await fetch(using: api, more: true, refreshBuckets: false)
    }

    private func fetch(using api: API, more: Bool, refreshBuckets: Bool) async {
        loadToken &+= 1
        let token = loadToken

        if more {
            loadingMore = true
        } else {
            loading = true
            events = []
            cursor = nil
            hasMore = false
            reqUntil = anchor ?? Date()
            reqSince = reqUntil.addingTimeInterval(-Double(window.seconds))
        }

        // Events come from the focused bar's span when one is lit; the
        // histogram request (when we refresh it) always spans the full window.
        var evSince = reqSince
        var evUntil = reqUntil
        if let f = focused, let start = TimelineFormat.date(from: f.t) {
            evSince = start
            // The server's window predicate is BETWEEN, inclusive at both ends,
            // so stop one second short of the next bar to avoid pulling its
            // first event into this one.
            evUntil = start.addingTimeInterval(bucket.seconds - 1)
        }

        do {
            let env = try await api.timeline(
                since: TimelineFormat.iso(evSince),
                until: TimelineFormat.iso(evUntil),
                bucket: bucket.rawValue,
                source: sourceFilter,
                caseId: caseId,
                limit: pageLimit,
                cursor: more ? cursor : nil
            )
            guard token == loadToken else { return }

            let batch = env.data.events
            if more {
                events.append(contentsOf: batch)
            } else {
                events = batch
            }
            if refreshBuckets && !more {
                buckets = env.data.buckets
            }

            // Next-page token. The server names the field `next_cursor`, while
            // the shared `PageInfo` DTO decodes `cursor` — so when that comes
            // back nil we rebuild the identical keyset token client-side from
            // the last row. Same shape, same codec: base64url of
            // {"p":<ts>,"u":<key>} (see the cursor codec in timelineapi.c).
            if batch.count == pageLimit, let last = batch.last {
                cursor = env.page?.cursor
                    ?? TimelineModel.keysetCursor(ts: last.ts, key: last.key)
                hasMore = cursor != nil
            } else {
                cursor = nil
                hasMore = false
            }

            meta = env.meta
            error = nil
            windowTooWide = false
        } catch {
            guard token == loadToken else { return }
            apply(error)
        }

        guard token == loadToken else { return }
        if more { loadingMore = false } else { loading = false }
    }

    /// Best-effort source catalogue for the filter sheet. Failure is recorded
    /// and shown in the sheet rather than swallowed — the fallback list (source
    /// ids seen in the loaded events) still works, but the user is told the
    /// list is partial.
    func loadSourcesIfNeeded(using api: API) async {
        guard sources.isEmpty else { return }
        do {
            sources = try await api.intelSources().data
            sourcesError = nil
        } catch {
            sourcesError = "Couldn't load the full source list (\(error.localizedDescription)). "
                + "Showing only the sources present in the events below."
        }
    }

    // ── Mutations that need a refetch ──────────────────────────────────────

    func setWindow(_ w: TimelineWindow, using api: API) async {
        guard w != window else { return }
        window = w
        await reload(using: api)
    }

    func setBucket(_ b: TimelineBucketSize, using api: API) async {
        guard b != bucket else { return }
        bucket = b
        await reload(using: api)
    }

    func setSource(_ id: String?, using api: API) async {
        guard id != sourceFilter else { return }
        sourceFilter = id
        await reload(using: api)
    }

    /// Drop the source filter and the focused histogram bar in one refetch,
    /// so "Clear filters" costs one round trip rather than two.
    func clearFilters(using api: API) async {
        sourceFilter = nil
        await reload(using: api)
    }

    /// Recovery offered next to the `window_too_wide` explanation.
    func narrowToLargestAllowed(using api: API) async {
        window = .d30
        windowTooWide = false
        await reload(using: api)
    }

    // ── Errors ─────────────────────────────────────────────────────────────

    private func apply(_ err: Error) {
        windowTooWide = false
        guard let apiErr = err as? APIError, case .http(let code, let body) = apiErr else {
            error = err.localizedDescription
            return
        }
        if body.contains("window_too_wide") {
            windowTooWide = true
            error = nil
            return
        }
        error = TimelineModel.explain(httpCode: code, body: body)
    }

    /// Turn the server's machine codes into something an analyst can act on.
    /// Unrecognised bodies fall through to the raw text — an unexplained error
    /// is still better than a silently unchanged screen.
    static func explain(httpCode: Int, body: String) -> String {
        if body.contains("invalid_bucket") {
            return "The server rejected the histogram granularity. Only hourly and daily buckets exist."
        }
        if body.contains("invalid_window") {
            return "The start of the range is after its end."
        }
        if body.contains("invalid_time") {
            return "The server couldn't parse one of the range bounds."
        }
        if body.contains("invalid_bbox") {
            return "The map area filter isn't a valid west,south,east,north box."
        }
        if body.contains("invalid_limit") {
            return "The page size was rejected by the server."
        }
        if body.contains("not_found") {
            return "That case doesn't exist, or it belongs to another tenant."
        }
        if body.contains("server_error") {
            return "The server failed while building the timeline. Try a narrower range."
        }
        if httpCode == 401 || httpCode == 403 {
            return "You're not signed in, or this workspace doesn't grant timeline access."
        }
        return "HTTP \(httpCode): \(body.prefix(200))"
    }

    /// Human reading of a `meta.notes` entry. These are load-bearing: the
    /// server emits them precisely when it answered a *narrower* question than
    /// the one that was asked, and hiding that is how a screen starts lying.
    static func explain(note: String) -> String {
        switch note {
        case "case_items_table_missing":
            return "This deployment has no case_items table yet, so the case filter can't be "
                + "applied. The server returned an empty timeline rather than quietly showing "
                + "you the whole workspace under a case's name."
        case "cases_table_missing":
            return "This deployment has no cases table, so case ownership couldn't be verified. "
                + "The filter still ran and every stream stayed scoped to your workspace."
        case "cursor_ignored":
            return "The paging cursor was malformed and was ignored — you're looking at the "
                + "first page, not the next one."
        default:
            return note
        }
    }

    /// Notes that mean "the answer is narrower than your question" warrant the
    /// warning tone; the rest are informational.
    static func isNarrowing(note: String) -> Bool {
        note == "case_items_table_missing" || note == "cursor_ignored"
    }

    // ── Keyset cursor codec ────────────────────────────────────────────────

    /// base64url (unpadded) of `{"p":<ts>,"u":<key>}` — byte-compatible with the
    /// server's `b64url` in timelineapi.c, which rejects `=` padding outright.
    /// Key order is irrelevant: the server reads the fields by name.
    static func keysetCursor(ts: String, key: String) -> String? {
        let obj: [String: String] = ["p": ts, "u": key]
        guard let data = try? JSONSerialization.data(withJSONObject: obj) else { return nil }
        var s = data.base64EncodedString()
        s = s.replacingOccurrences(of: "+", with: "-")
        s = s.replacingOccurrences(of: "/", with: "_")
        while s.hasSuffix("=") { s.removeLast() }
        return s.isEmpty ? nil : s
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Formatting
// ─────────────────────────────────────────────────────────────────────────────

/// Date plumbing for the timeline. Formatters are built per call rather than
/// cached in a global: `DateFormatter`/`ISO8601DateFormatter` aren't thread
/// safe, and every other date helper in this app (see `IntelSourceRow.swift`)
/// does the same.
enum TimelineFormat {

    /// The server speaks `%Y-%m-%dT%H:%M:%SZ` (the `TSF` macro in
    /// timelineapi.c) for both event timestamps and bucket keys. Fractional
    /// seconds and SQLite's bare `YYYY-MM-DD HH:MM:SS` are accepted too, so a
    /// column that ever reaches us unformatted still parses.
    static func date(from s: String?) -> Date? {
        guard let s, !s.isEmpty else { return nil }
        let f1 = ISO8601DateFormatter()
        f1.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        if let d = f1.date(from: s) { return d }
        let f2 = ISO8601DateFormatter()
        f2.formatOptions = [.withInternetDateTime]
        if let d = f2.date(from: s) { return d }
        let f3 = DateFormatter()
        f3.locale = Locale(identifier: "en_US_POSIX")
        f3.timeZone = TimeZone(secondsFromGMT: 0)
        f3.dateFormat = "yyyy-MM-dd HH:mm:ss"
        return f3.date(from: s)
    }

    /// UTC wire form for `since` / `until`.
    static func iso(_ d: Date) -> String {
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime]
        f.timeZone = TimeZone(secondsFromGMT: 0)
        return f.string(from: d)
    }

    /// `MM-dd HH:mm` in JST — the same shape the map's scrub bubble uses, so a
    /// timestamp reads identically on both screens.
    static func stamp(_ d: Date) -> String {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.dateFormat = "MM-dd HH:mm"
        f.timeZone = TimeZone(identifier: "Asia/Tokyo")
        return f.string(from: d)
    }

    static func stamp(iso s: String?) -> String {
        guard let d = date(from: s) else { return "—" }
        return stamp(d)
    }

    /// Axis / focus caption for one histogram bar.
    static func bucketLabel(_ t: String, size: TimelineBucketSize) -> String {
        guard let d = date(from: t) else { return t }
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.timeZone = TimeZone(identifier: "Asia/Tokyo")
        f.dateFormat = size == .hour ? "MM-dd HH:00" : "MM-dd"
        return f.string(from: d)
    }

    /// "3h ago" style, matching the map's LIVE pill.
    static func ago(_ d: Date) -> String {
        let s = -d.timeIntervalSinceNow
        if s < 60    { return "\(Int(max(0, s)))s ago" }
        if s < 3600  { return "\(Int(s / 60))m ago" }
        if s < 86400 { return "\(Int(s / 3600))h ago" }
        return "\(Int(s / 86400))d ago"
    }
}
