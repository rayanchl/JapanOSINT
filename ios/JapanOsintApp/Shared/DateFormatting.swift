import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// One ISO-8601 → "3h ago" helper for the whole app.
//
// This used to be three functions: a module-scope `relativeTime` in
// Intel/IntelSourceRow.swift plus two byte-identical `private func prettyDate`
// clones in Dashboard/SourceDashboardTab.swift and Database/SchedulerView.swift.
// The clones SHADOWED the module-scope one inside their own types, so the
// obvious call resolved to the local copy and any fix to the shared version
// silently skipped two screens.
//
// The server emits timestamps two ways — `datetime('now')` (no fractional
// seconds, sometimes no zone) and full RFC 3339 with milliseconds — so both
// formatter configurations are tried before giving up.
//
// NOTE: `LayersTab`'s formatter takes an already-parsed `Date` and Settings has
// its own absolute-date formatter; neither is this function and neither was
// folded in here.
// ─────────────────────────────────────────────────────────────────────────────

/// Parse a server timestamp: RFC 3339 with or without fractional seconds, or
/// SQLite's `datetime('now')` shape (`YYYY-MM-DD HH:MM:SS`, UTC, no `T`, no
/// zone), or a bare `YYYY-MM-DD`.
///
/// The SQLite shape is what `alert_events.matched_at`, `search_history.ts`,
/// `cases`, `annotations` and the scheduler's `last_check` hold, and this
/// function used to reject it — so every one of those rows read "—", and the
/// widget snapshot's `?? Date()` fallback stamped each alert as "now". The
/// Cases subtree had grown its own tolerant copy (`caseDate`) for exactly this;
/// it is folded in here and now delegates, so there is one parser again.
func isoToDate(_ s: String?) -> Date? {
    guard let s, !s.isEmpty else { return nil }
    let f1 = ISO8601DateFormatter()
    f1.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
    if let d = f1.date(from: s) { return d }
    let f2 = ISO8601DateFormatter()
    f2.formatOptions = [.withInternetDateTime]
    if let d = f2.date(from: s) { return d }
    let sqlite = DateFormatter()
    sqlite.locale = Locale(identifier: "en_US_POSIX")
    sqlite.timeZone = TimeZone(identifier: "UTC")
    sqlite.dateFormat = "yyyy-MM-dd HH:mm:ss"
    if let d = sqlite.date(from: s) { return d }
    sqlite.dateFormat = "yyyy-MM-dd"
    return sqlite.date(from: s)
}

/// An ISO-8601 timestamp as an abbreviated relative phrase ("3h ago"), or "—"
/// when it is absent or unparseable. Never returns the raw string: a bare
/// timestamp dropped into a row that otherwise reads "3h ago" looks like data,
/// not like a parse failure.
func relativeTime(_ iso: String?) -> String {
    guard let date = isoToDate(iso) else { return "—" }
    let f = RelativeDateTimeFormatter()
    f.unitsStyle = .abbreviated
    return f.localizedString(for: date, relativeTo: Date())
}
