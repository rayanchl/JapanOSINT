import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Offset-paged workspace lists, and who wrote each row.
//
// /api/saved-searches, /api/search-history and /api/intel/items/:uid/evidence
// answer `page:{limit, offset, count, total, has_more}` (see `OffsetPage`).
// This app used to read each of them as one bare page — 50 saved searches,
// 100 history entries, 50 custody records — with nothing on screen saying that
// more existed. `PageBoundFooter` states the measured bound ("showing 50 of
// 120") and loads the next page while the server says more remain.
//
// Saved searches and search history are WORKSPACE-wide on read (decided
// 2026-10-05): every member sees every member's rows, each carrying its
// author's `user_id` and `mine`. `AuthorLabel` names that author the way the
// web client does (client/src/hooks/useMembers.js), through the roster any
// member may read (GET /api/members).
// ─────────────────────────────────────────────────────────────────────────────

/// The most rows one request may ask for: savedsearchapi.c and evidence.c
/// clamp `limit` to 200.
let offsetListMaxPage = 200

enum PageBound {
    /// "120 saved searches" when everything is loaded, "showing 50 of 120 …"
    /// when not, "50 … loaded · more on the server" when the server said more
    /// exist but could not count them, and nil when there is nothing measured
    /// to say (silence beats a number nobody measured).
    static func label(shown: Int, page: OffsetPage?, noun: String) -> String? {
        if let total = page?.total {
            return shown >= total ? "\(total) \(noun)" : "showing \(shown) of \(total) \(noun)"
        }
        if page?.has_more == true {
            return "\(shown) \(noun) loaded · more on the server (no total reported)"
        }
        return nil
    }

    /// True while rows the server holds are not on screen.
    static func isPartial(shown: Int, page: OffsetPage?) -> Bool {
        if let total = page?.total { return shown < total }
        return page?.has_more == true
    }

    /// Append a page to the rows already held, dropping any row already
    /// present: offset paging shifts by one when a row is inserted or deleted
    /// between two requests, and a repeated id is that shift, not a new record.
    static func append<Row: Identifiable>(_ rows: [Row], _ next: [Row]) -> [Row] {
        var seen = Set(rows.map(\.id))
        var out = rows
        for r in next where seen.insert(r.id).inserted { out.append(r) }
        return out
    }

    /// Page size for a silent reload after a mutation: as many rows as are on
    /// screen (up to the server's clamp), so deleting one row does not collapse
    /// a list the user has paged through.
    static func reloadSize(shown: Int, pageSize: Int) -> Int {
        min(offsetListMaxPage, max(pageSize, shown))
    }
}

/// Footer for an offset-paged list: the measured bound plus "Load more" while
/// the server reports more rows.
struct PageBoundFooter: View {
    let shown: Int
    let page: OffsetPage?
    let noun: String
    let loadingMore: Bool
    var error: String? = nil
    let loadMore: () async -> Void

    @Environment(\.theme) private var theme

    var body: some View {
        // Nothing measured, nothing pending, nothing failed: no row at all.
        if PageBound.label(shown: shown, page: page, noun: noun) != nil
            || page?.has_more == true || error != nil {
            content
        }
    }

    private var content: some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(spacing: Space.sm) {
                if let text = PageBound.label(shown: shown, page: page, noun: noun) {
                    Text(text)
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(PageBound.isPartial(shown: shown, page: page)
                                         ? theme.accent : theme.textMuted)
                }
                Spacer(minLength: 0)
                if page?.has_more == true {
                    if loadingMore {
                        ProgressView().controlSize(.small)
                    } else {
                        Button("Load more") { Task { await loadMore() } }
                            .buttonStyle(.bordered)
                            .controlSize(.small)
                    }
                }
            }
            if let error {
                Text("Next page failed: \(error)")
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
            }
        }
    }
}

enum AuthorLabel {
    /// `[user_id: email]` for the active workspace.
    static func names(_ roster: WorkspaceMembersResponse?) -> [String: String] {
        var out: [String: String] = [:]
        for m in roster?.members ?? [] where !m.user_id.isEmpty {
            out[m.user_id] = m.email.isEmpty ? m.user_id : m.email
        }
        return out
    }

    /// `mine` wins ("you"); a member on the roster shows their email; an id no
    /// longer on it is "a former member" rather than a raw uuid; with no roster
    /// loaded (yet, or it failed) the row says "a teammate"; a row with no
    /// author at all says that.
    static func text(userId: String?, mine: Bool?, names: [String: String]) -> String {
        if mine == true { return "you" }
        guard let userId, !userId.isEmpty else { return "unknown author" }
        if names.isEmpty { return "a teammate" }
        return names[userId] ?? "a former member"
    }
}
