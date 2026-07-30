import Foundation
import SwiftUI
import Combine

// ─────────────────────────────────────────────────────────────────────────────
// Cases (roadmap item 14) — the shared list store plus the vocabulary helpers
// every Cases screen renders with.
//
// The store owns exactly one thing: the paged list of `CaseSummary` rows and
// the mutations that change a row's identity in that list (create / update /
// status / delete). Per-case detail — items, activity, notes, roster — is NOT
// cached here; `CaseDetailView` owns it, because a case workspace is a screen
// you sit in and re-read, not a value other screens need.
//
// `activeCaseId` is the reason this is a store and not a `@State` array: the
// "Add to case" sheet is reachable from anywhere in the app, and defaulting it
// to the case the analyst is working in is the difference between one tap and
// a scroll through fifty folders.
//
// TWO SERVER-SHAPE NOTES the UI has to live with (both reported upstream):
//  · `PageInfo.cursor` decodes the key `cursor`, but casesapi.c emits
//    `page.next_cursor`. Until `PageInfo` learns that key, `nextCursor` is
//    always nil and the list is effectively one page of 50. The load-more path
//    below is written against `PageInfo.cursor` exactly as specified, so it
//    starts working the moment the DTO gains the field — no UI change needed.
//  · GET /api/cases DOES return a per-row `item_count`, but `CaseSummary` has
//    no such property so it is dropped at decode. `itemCounts` below is a
//    best-effort cache filled in from `CaseDetail.item_counts` once a case has
//    been opened; a row whose count is unknown renders NO count chip rather
//    than a made-up zero.
// ─────────────────────────────────────────────────────────────────────────────

@MainActor
final class CaseStore: ObservableObject {

    /// Loaded rows, always sorted newest-updated first.
    @Published private(set) var cases: [CaseSummary] = []
    /// First-page / refresh load.
    @Published private(set) var loading = false
    /// Keyset "load more" load — separate so the list doesn't blank out.
    @Published private(set) var loadingMore = false
    /// Last surfaced failure. Settable so a screen can clear it when its
    /// alert is dismissed.
    @Published var error: String?
    /// The case "Add to case" defaults to. Set by opening a case, by pinning
    /// into one, and by the explicit "Make active case" action.
    @Published var activeCaseId: String?
    /// Server-side `?status=` filter. nil == every status.
    @Published private(set) var statusFilter: String?
    /// Keyset cursor for the next page; nil == no further pages.
    @Published private(set) var nextCursor: String?
    /// case id → total pinned items, learned from `CaseDetail.item_counts`.
    @Published private(set) var itemCounts: [String: Int] = [:]

    private let apiClient: APIClient
    private var hasLoaded = false
    private static let pageSize = 50

    init(apiClient: APIClient) {
        self.apiClient = apiClient
    }

    // MARK: - Derived

    var activeCase: CaseSummary? {
        guard let activeCaseId else { return nil }
        return cases.first { $0.id == activeCaseId }
    }

    var canLoadMore: Bool { nextCursor != nil && !loading && !loadingMore }

    /// nil == "we have never opened this case, so we don't know". Callers must
    /// render nothing rather than 0 — an empty case and an unknown case are
    /// different claims.
    func itemCount(for id: String) -> Int? { itemCounts[id] }

    func noteItemCount(_ n: Int, for id: String) { itemCounts[id] = n }

    // MARK: - Loading

    func loadIfNeeded() async {
        if !hasLoaded { await reload() }
    }

    func setFilter(_ status: String?) async {
        guard status != statusFilter else { return }
        statusFilter = status
        nextCursor = nil
        await reload()
    }

    func reload() async {
        loading = true
        defer { loading = false; hasLoaded = true }
        do {
            let env = try await apiClient.api.cases(status: statusFilter,
                                                    limit: Self.pageSize,
                                                    cursor: nil)
            cases = Self.sorted(env.data)
            nextCursor = env.page?.cursor
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    /// Appends the next keyset page. Dedupes by id — a case whose `updated_at`
    /// changed between page 1 and page 2 can legitimately appear twice.
    func loadMore() async {
        guard let cursor = nextCursor, !loading, !loadingMore else { return }
        loadingMore = true
        defer { loadingMore = false }
        do {
            let env = try await apiClient.api.cases(status: statusFilter,
                                                    limit: Self.pageSize,
                                                    cursor: cursor)
            var merged = cases
            var seen = Set(merged.map(\.id))
            for row in env.data where !seen.contains(row.id) {
                seen.insert(row.id)
                merged.append(row)
            }
            cases = Self.sorted(merged)
            nextCursor = env.page?.cursor
            error = nil
        } catch let e {
            // Keep the cursor so the row can be retried; only report.
            error = e.localizedDescription
        }
    }

    // MARK: - Mutations

    /// Throws so the presenting sheet can render the failure inline next to the
    /// field the user just typed into, instead of a detached banner.
    @discardableResult
    func create(name: String, summary: String?, priority: Int) async throws -> CaseDetail {
        let trimmedName = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedSummary = summary?.trimmingCharacters(in: .whitespacesAndNewlines)
        let detail = try await apiClient.api.caseCreate(
            name: trimmedName,
            summary: (trimmedSummary?.isEmpty ?? true) ? nil : trimmedSummary,
            priority: priority)
        apply(detail)
        activeCaseId = detail.id
        Haptics.success()
        return detail
    }

    @discardableResult
    func update(_ id: String, fields: [String: Any]) async throws -> CaseDetail {
        let detail = try await apiClient.api.caseUpdate(id, fields: fields)
        apply(detail)
        return detail
    }

    /// Optimistic status flip with rollback — same shape as `AlertsTab.toggle`.
    /// The row keeps its optimistic status until the server answers; on success
    /// `apply` drops it from the list if it no longer matches `statusFilter`.
    func setStatus(_ id: String, to status: String) async {
        guard let idx = cases.firstIndex(where: { $0.id == id }) else { return }
        let original = cases[idx]
        cases[idx] = original.replacingStatus(status)
        Haptics.selection()
        do {
            let detail = try await apiClient.api.caseUpdate(id, fields: ["status": status])
            apply(detail)
            error = nil
        } catch let e {
            if let i = cases.firstIndex(where: { $0.id == original.id }) {
                cases[i] = original
            }
            error = e.localizedDescription
            Haptics.error()
        }
    }

    /// Destructive and irreversible: the server deletes case_items,
    /// case_activity and case_members in the same transaction, and annotations
    /// cascade with the case. Callers MUST gate this behind an explicit typed
    /// confirmation. Returns true only when the server confirmed.
    @discardableResult
    func delete(_ id: String) async -> Bool {
        let snapshot = cases
        let wasActive = (activeCaseId == id)
        cases.removeAll { $0.id == id }
        if wasActive { activeCaseId = nil }
        Haptics.warning()
        do {
            try await apiClient.api.caseDelete(id)
            itemCounts.removeValue(forKey: id)
            error = nil
            return true
        } catch let e {
            cases = snapshot
            if wasActive { activeCaseId = id }
            error = e.localizedDescription
            Haptics.error()
            return false
        }
    }

    /// Pin a reference into a case. Throws so the "Add to case" sheet can keep
    /// the user in place and show what went wrong instead of dismissing on a
    /// failure. The row reconciliation afterwards is deliberately NOT awaited —
    /// the pin already succeeded, and the picker's whole job is to get out of
    /// the way fast.
    func pin(caseId: String, refType: String, refId: String,
             label: String? = nil) async throws {
        _ = try await apiClient.api.casePin(caseId, refType: refType,
                                            refId: refId, label: label)
        activeCaseId = caseId
        Task { await self.refreshRow(caseId) }
    }

    func unpin(caseId: String, refType: String, refId: String) async throws {
        try await apiClient.api.caseUnpin(caseId, refType: refType, refId: refId)
        Task { await self.refreshRow(caseId) }
    }

    /// Re-read ONE row after a mutation that has already reported its own
    /// success or failure to the user. Silent on error on purpose: this is
    /// background reconciliation of `updated_at` / item count, not an action
    /// the user is waiting on, and a second error banner for an operation that
    /// visibly succeeded would be misinformation.
    func refreshRow(_ id: String) async {
        guard let detail = try? await apiClient.api.caseDetail(id) else { return }
        apply(detail)
        if let counts = detail.item_counts {
            itemCounts[id] = counts.values.reduce(0, +)
        }
    }

    // MARK: - Internals

    private func apply(_ detail: CaseDetail) {
        let row = Self.summary(from: detail)
        if let f = statusFilter, row.status != f {
            // It no longer belongs in the filtered view the user is looking at.
            cases.removeAll { $0.id == row.id }
            return
        }
        if let i = cases.firstIndex(where: { $0.id == row.id }) {
            cases[i] = row
        } else {
            cases.append(row)
        }
        cases = Self.sorted(cases)
    }

    /// `CaseDetail` carries no `created_by`, so the projected summary reports
    /// nil rather than guessing an author.
    static func summary(from d: CaseDetail) -> CaseSummary {
        CaseSummary(id: d.id,
                    name: d.name,
                    summary: d.summary,
                    status: d.status,
                    priority: d.priority,
                    created_by: nil,
                    created_at: d.created_at,
                    updated_at: d.updated_at,
                    closed_at: d.closed_at,
                    // Detail carries per-ref-type counts; the summary shows the
                    // total (nil stays nil so "unknown" isn't rendered as 0).
                    item_count: d.item_counts?.values.reduce(0, +))
    }

    private static func sorted(_ rows: [CaseSummary]) -> [CaseSummary] {
        rows.sorted { a, b in
            a.updated_at == b.updated_at
                ? a.id < b.id
                : a.updated_at > b.updated_at
        }
    }
}

extension CaseSummary {
    /// Optimistic copy for the swipe actions. `closed_at` is deliberately left
    /// alone — inventing a close timestamp locally would put a value on screen
    /// that no server ever produced.
    func replacingStatus(_ newStatus: String) -> CaseSummary {
        CaseSummary(id: id,
                    name: name,
                    summary: summary,
                    status: newStatus,
                    priority: priority,
                    created_by: created_by,
                    created_at: created_at,
                    updated_at: updated_at,
                    closed_at: closed_at,
                    item_count: item_count)
    }
}

// MARK: - Vocabularies (casesapi.h)

/// status: open | closed | archived
enum CaseStatus {
    static let all: [String] = ["open", "closed", "archived"]

    static func label(_ s: String) -> String {
        switch s {
        case "open":     return "Open"
        case "closed":   return "Closed"
        case "archived": return "Archived"
        default:         return s
        }
    }

    static func tone(_ s: String) -> Pill.Tone {
        switch s {
        case "open":     return .success
        case "closed":   return .neutral
        case "archived": return .info
        default:         return .neutral
        }
    }

    static func icon(_ s: String) -> String {
        switch s {
        case "open":     return "folder.fill"
        case "closed":   return "checkmark.seal.fill"
        case "archived": return "archivebox.fill"
        default:         return "folder"
        }
    }
}

/// priority: integer 0…5 (0 = none … 5 = critical); the server 400s anything
/// else, so the pickers only ever offer this range.
enum CasePriority {
    static let range: [Int] = [0, 1, 2, 3, 4, 5]

    static func label(_ p: Int) -> String {
        switch p {
        case 0: return "None"
        case 1: return "Low"
        case 2: return "Moderate"
        case 3: return "High"
        case 4: return "Severe"
        case 5: return "Critical"
        default: return "P\(p)"
        }
    }

    static func tone(_ p: Int) -> Pill.Tone {
        switch p {
        case 0, 1: return .neutral
        case 2:    return .info
        case 3:    return .warning
        default:   return .danger
        }
    }
}

/// ref_type: intel_item | entity | breach_item | feature | camera | search_run
enum CaseRefType {
    static let all: [String] = ["intel_item", "entity", "breach_item",
                                "feature", "camera", "search_run"]

    static func label(_ t: String) -> String {
        switch t {
        case "intel_item":  return "Intel items"
        case "entity":      return "Entities"
        case "breach_item": return "Breach records"
        case "feature":     return "Map features"
        case "camera":      return "Cameras"
        case "search_run":  return "Search runs"
        default:            return t
        }
    }

    static func short(_ t: String) -> String {
        switch t {
        case "intel_item":  return "intel"
        case "entity":      return "entities"
        case "breach_item": return "breaches"
        case "feature":     return "features"
        case "camera":      return "cameras"
        case "search_run":  return "searches"
        default:            return t
        }
    }

    static func icon(_ t: String) -> String {
        switch t {
        case "intel_item":  return "newspaper.fill"
        case "entity":      return "point.3.connected.trianglepath.dotted"
        case "breach_item": return "lock.trianglebadge.exclamationmark"
        case "feature":     return "mappin.and.ellipse"
        case "camera":      return "video.fill"
        case "search_run":  return "magnifyingglass"
        default:            return "doc.text"
        }
    }

    /// Stable section order for the Findings pane — vocabulary order, not
    /// whatever the dictionary hands back.
    static func sortIndex(_ t: String) -> Int {
        all.firstIndex(of: t) ?? all.count
    }
}

/// activity kind: created | updated | status_changed | item_pinned |
/// item_unpinned | members_changed | comment. Everything except `comment` is
/// written by the server — that is what makes the feed a history rather than a
/// chat log.
enum CaseActivityKind {
    static func label(_ k: String) -> String {
        switch k {
        case "created":         return "Case created"
        case "updated":         return "Fields edited"
        case "status_changed":  return "Status changed"
        case "item_pinned":     return "Finding pinned"
        case "item_unpinned":   return "Finding unpinned"
        case "members_changed": return "Roster changed"
        case "comment":         return "Comment"
        default:                return k
        }
    }

    static func icon(_ k: String) -> String {
        switch k {
        case "created":         return "sparkles"
        case "updated":         return "pencil"
        case "status_changed":  return "arrow.triangle.swap"
        case "item_pinned":     return "pin.fill"
        case "item_unpinned":   return "pin.slash"
        case "members_changed": return "person.2.fill"
        case "comment":         return "bubble.left.fill"
        default:                return "clock"
        }
    }

    static func tone(_ k: String) -> Pill.Tone {
        switch k {
        case "comment":       return .accent
        case "item_pinned":   return .success
        case "item_unpinned": return .warning
        case "created":       return .info
        default:              return .neutral
        }
    }

    /// A server-written row is history; a comment is something a person chose
    /// to say. Worth distinguishing visually.
    static func isSystem(_ k: String) -> Bool { k != "comment" }
}

// MARK: - Timestamps

/// Tolerant parser for the Cases subtree.
///
/// The shared `relativeTime(_:)` helper only accepts ISO-8601, but `cases`,
/// `case_items`, `case_activity` and `annotations` all default their timestamp
/// columns to SQLite's `datetime('now')`, which emits `YYYY-MM-DD HH:MM:SS`
/// (UTC, no `T`, no zone). Feeding that to ISO8601DateFormatter yields nil and
/// every row on the screen would read "—". Snapshot `captured_at` IS real
/// ISO-8601, so both shapes have to work.
func caseDate(_ raw: String?) -> Date? {
    guard let raw, !raw.isEmpty else { return nil }

    let isoFractional = ISO8601DateFormatter()
    isoFractional.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
    if let d = isoFractional.date(from: raw) { return d }

    let iso = ISO8601DateFormatter()
    iso.formatOptions = [.withInternetDateTime]
    if let d = iso.date(from: raw) { return d }

    let sqlite = DateFormatter()
    sqlite.locale = Locale(identifier: "en_US_POSIX")
    sqlite.timeZone = TimeZone(identifier: "UTC")
    sqlite.dateFormat = "yyyy-MM-dd HH:mm:ss"
    if let d = sqlite.date(from: raw) { return d }

    sqlite.dateFormat = "yyyy-MM-dd"
    return sqlite.date(from: raw)
}

func caseRelativeTime(_ raw: String?) -> String {
    guard let d = caseDate(raw) else { return "—" }
    let f = RelativeDateTimeFormatter()
    f.unitsStyle = .abbreviated
    return f.localizedString(for: d, relativeTo: Date())
}

/// Unparseable input falls back to the raw server string rather than "—": if
/// the backend starts emitting a shape we don't know, showing it beats hiding
/// it.
func caseAbsoluteTime(_ raw: String?) -> String {
    guard let d = caseDate(raw) else { return raw ?? "—" }
    let f = DateFormatter()
    f.dateStyle = .medium
    f.timeStyle = .short
    return f.string(from: d)
}
