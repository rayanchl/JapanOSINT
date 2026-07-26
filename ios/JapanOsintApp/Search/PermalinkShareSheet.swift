import SwiftUI
import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap item 38 — permalinks.
//
// A PERMALINK CARRIES VIEW STATE AND NOTHING ELSE.
// `/api/permalink` is a stateless codec: it never touches the database, never
// sees the tenant or the user, and the token it mints is UNSIGNED. It encodes a
// map camera, a filter bag, a layer list, a time window and at most two ids —
// all of it bounded and clamped by one normalizer on both encode and decode.
// The ids are there so a link reopens the right pane; they are not a grant. The
// entity and case routes re-check tenancy and membership from the session, and
// a token naming a case the viewer cannot open still 403s there.
//
// So: nothing in this sheet may imply that sharing a link shares access. The
// copy says the opposite, explicitly, right next to the share button.
//
// URL SHAPE ASSUMED HERE (the orchestrator owns registration + `onOpenURL`):
//
//     japanosint://permalink/<token>
//
// `japanosint` is already registered in Info.plist (CFBundleURLSchemes) for the
// OAuth callback, whose host is `auth-callback` — so `permalink` does not
// collide with it and NO Info.plist change is required. `PermalinkResolver`
// also accepts `?t=<token>` on any URL, so an https universal link can be added
// later without touching this file.
//
// Tokens are `v1.` + base64url, i.e. only `[A-Za-z0-9-_.]`, so they are already
// safe in a path component and need no escaping.
// ─────────────────────────────────────────────────────────────────────────────

/// Mints, formats and resolves permalink tokens.
///
/// The app-open side is deliberately here rather than in the view: the
/// orchestrator wires `.onOpenURL { url in … }` and needs the parsing and the
/// resolve call without presenting anything.
enum PermalinkResolver {
    static let scheme = "japanosint"
    static let host = "permalink"

    /// `japanosint://permalink/<token>`.
    static func url(for token: String) -> URL? {
        let clean = token.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !clean.isEmpty else { return nil }
        return URL(string: "\(scheme)://\(host)/\(clean)")
    }

    /// The token out of an incoming URL, or `nil` if this is not one of ours.
    ///
    /// Accepts `japanosint://permalink/<token>` and, so a web/universal link can
    /// be added later without a second parser, any URL carrying `?t=<token>`.
    static func token(from url: URL) -> String? {
        if let comps = URLComponents(url: url, resolvingAgainstBaseURL: false),
           let t = comps.queryItems?.first(where: { $0.name == "t" })?.value,
           !t.isEmpty {
            return t
        }
        guard url.scheme?.lowercased() == scheme else { return nil }
        guard url.host?.lowercased() == host else { return nil }
        let segments = url.pathComponents.filter { $0 != "/" && !$0.isEmpty }
        guard let first = segments.first, !first.isEmpty else { return nil }
        return first
    }

    /// Resolve a token to the canonical, fully bounded state object.
    ///
    /// Throws on a token the server rejects — an unknown version or a corrupt
    /// body is a hard refusal, never a best-effort parse, because a link that
    /// decodes to *something other than what was minted* is worse than an error.
    static func resolve(_ token: String, api: API) async throws -> [String: AnyCodable] {
        try await api.permalinkResolve(token)
    }

    /// Convenience for `onOpenURL`: returns `nil` when the URL is not a
    /// permalink, and otherwise the resolved state (or throws).
    static func state(from url: URL, api: API) async throws -> [String: AnyCodable]? {
        guard let token = token(from: url) else { return nil }
        return try await resolve(token, api: api)
    }

    /// Mint a token for `state`.
    ///
    /// Calls `API.permalinkMint` first. That method decodes `PermalinkToken`
    /// (`{"token":…}`) directly from the response, but the server actually
    /// replies `{"data":{"token":…,"version":"v1","state":{…}}}` — so the decode
    /// currently fails. Rather than leave the feature dead until
    /// `API+Roadmap.swift` (orchestrator-owned) is corrected, a decoding failure
    /// falls back to reading the real envelope off the SAME `API` value, using
    /// its own `makeURL`/`request` helpers, its auth headers and its retry. Any
    /// other error — auth, transport, `state_too_large` — propagates untouched.
    /// Delete `mintCompat` once the DTO matches.
    static func mint(_ state: [String: Any], api: API) async throws -> String {
        do {
            return try await api.permalinkMint(state)
        } catch APIError.decoding(_) {
            return try await mintCompat(state, api: api)
        }
    }

    private struct MintEnvelope: Decodable {
        struct Payload: Decodable {
            let token: String
            let version: String?
        }
        let data: Payload
    }

    private static func mintCompat(_ state: [String: Any],
                                   api: API) async throws -> String {
        let url = try api.makeURL("/api/permalink")
        let body = try JSONSerialization.data(withJSONObject: state)
        let data = try await api.request(url, method: "POST", body: body,
                                         timeout: 15)
        return try JSONDecoder().decode(MintEnvelope.self, from: data).data.token
    }
}

// MARK: - Sheet

/// Mints a permalink for a view state and offers it for sharing.
///
/// ```swift
/// .sheet(isPresented: $showShare) {
///     PermalinkShareSheet(state: [
///         "kind": "intel",
///         "params": ["q": query],
///         "map": ["lat": center.latitude, "lon": center.longitude, "zoom": zoom],
///         "layers": enabledLayerIds
///     ])
/// }
/// ```
///
/// The server drops any key outside its vocabulary (`kind`, `params`, `map`,
/// `layers`, `time`, `entity_id`, `case_id`) rather than carrying it, so a
/// token can never become a general-purpose payload stapled to a URL.
struct PermalinkShareSheet: View {
    /// The view state to encode. Values must be JSON-serializable.
    let state: [String: Any]
    /// Optional description of what the link reopens.
    var contextLabel: String? = nil

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var token: String?
    @State private var link: URL?
    @State private var minting = false
    @State private var error: String?
    @State private var copied = false

    var body: some View {
        NavigationStack {
            Form {
                if let contextLabel, !contextLabel.isEmpty {
                    Section {
                        Text(contextLabel)
                            .font(.caption)
                            .foregroundStyle(theme.textMuted)
                    } header: {
                        Text("Reopens")
                    }
                }

                if minting {
                    Section {
                        HStack(spacing: Space.sm) {
                            ProgressView().controlSize(.small)
                            Text("Minting link…")
                                .font(.caption)
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                } else if let link {
                    linkSection(link)
                } else if let error {
                    errorSection(error)
                } else {
                    Section {
                        Button {
                            Task { await mint() }
                        } label: {
                            Label("Create link", systemImage: "link.badge.plus")
                        }
                        .compatLargeControl()
                    }
                }

                notAGrantSection
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("Share view")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Close") { dismiss() }
                }
            }
            .task { if token == nil && !minting { await mint() } }
        }
        .compatSheetSizing(.compact)
    }

    // MARK: - Sections

    private func linkSection(_ url: URL) -> some View {
        Section {
            Text(url.absoluteString)
                .font(.caption.monospaced())
                .foregroundStyle(theme.text)
                .textSelection(.enabled)
                .lineLimit(4)
                .truncationMode(.middle)

            Button {
                Clipboard.copy(url.absoluteString)
                copied = true
                Haptics.success()
            } label: {
                Label(copied ? "Copied" : "Copy link",
                      systemImage: copied ? "checkmark" : "doc.on.doc")
            }

            ShareLink(item: url) {
                Label("Share link", systemImage: "square.and.arrow.up")
            }
            .compatLargeControl()

            Button {
                Task { await mint() }
            } label: {
                Label("Create a new link", systemImage: "arrow.clockwise")
            }
            .font(.caption)
        } header: {
            Text("Link")
        } footer: {
            Text("Links do not expire and are not revocable — they are just an encoded description of this view, not a record on the server.")
                .font(.caption2)
        }
    }

    private func errorSection(_ message: String) -> some View {
        Section {
            VStack(alignment: .leading, spacing: Space.xs) {
                Label("Couldn't create the link", systemImage: "xmark.octagon.fill")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.danger)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
            }
            .padding(.vertical, Space.xs)
            Button {
                Task { await mint() }
            } label: {
                Label("Try again", systemImage: "arrow.clockwise")
            }
        }
    }

    private var notAGrantSection: some View {
        Section {
            HStack(alignment: .top, spacing: Space.md) {
                Image(systemName: "exclamationmark.shield")
                    .font(.subheadline)
                    .foregroundStyle(theme.warning)
                    .frame(width: 24)
                VStack(alignment: .leading, spacing: 2) {
                    Text("This link is not access")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.text)
                    Text("It carries where you were looking — the map camera, the filters, which record was open — and nothing else. Whoever opens it still has to sign in, and still only sees what their own account is allowed to see. Sharing it does not share your data.")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
            .padding(.vertical, Space.xs)
        }
    }

    // MARK: - Work

    private func mint() async {
        minting = true
        defer { minting = false }
        error = nil
        copied = false
        do {
            let t = try await PermalinkResolver.mint(state, api: apiClient.api)
            token = t
            guard let url = PermalinkResolver.url(for: t) else {
                link = nil
                error = "The server returned a token this app could not turn into a link."
                Haptics.error()
                return
            }
            link = url
            Haptics.success()
        } catch {
            link = nil
            token = nil
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}
