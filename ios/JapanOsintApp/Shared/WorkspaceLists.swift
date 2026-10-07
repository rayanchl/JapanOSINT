import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Who wrote each row of a workspace list.
//
// Saved searches and search history are WORKSPACE-wide on read (decided
// 2026-10-05): every member sees every member's rows, each carrying its
// author's `user_id` and `mine`. `AuthorLabel` names that author the way the
// web client does (client/src/hooks/useMembers.js), through the roster any
// member may read (GET /api/members).
// ─────────────────────────────────────────────────────────────────────────────

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
