import SwiftUI
import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap item 38 — saved searches.
//
// A saved search is a private bookmark: `/api/saved-searches` carries
// `tenant_id AND user_id` on every statement, including the fetch-by-id and the
// DELETE, so nothing here is visible to a workspace admin.
//
// Two server behaviours shape this screen and are worth stating up front:
//
//  · RUN IS BOOKKEEPING. `POST /:id/run` bumps `run_count`, stamps
//    `last_run_at` and writes a history row. It does NOT execute the query —
//    the server replies `meta.executed = false` and tells the caller to
//    re-issue the query itself against the endpoint for `data.kind`. So this
//    view calls `savedSearchRun` for the bookkeeping and hands the row to
//    `onRun` so the host surface can actually perform the search. The UI says
//    this plainly rather than implying results are about to appear.
//
//  · ONLY `kind == "intel"` CONVERTS TO AN ALERT. Alert matching runs over the
//    intel ingest chokepoint; an entity / breach / map / osint search has no
//    corresponding stream of new items, so a synthesised rule would either
//    never fire or match everything. The server refuses with a written
//    explanation. We disable the action for kinds that cannot map — and if the
//    server refuses anyway, its sentence is what the user reads, never a raw
//    `HTTP 400: {"error":…}`.
//
// `ServerError` (Shared/ExportSheet.swift) is the one place that unwraps those
// `{"error":"…"}` bodies.
// ─────────────────────────────────────────────────────────────────────────────

/// What the caller wants to save. Handed in by the surface that owns the live
/// query; this view never invents params of its own.
struct SavedSearchDraft: Identifiable {
    let id = UUID()
    /// `intel` | `osint` | `entity` | `breach` | `map` — the server rejects
    /// anything else, and the kind is immutable after creation.
    let kind: String
    /// Pre-filled into the name field. May be empty; an unnamed saved search is
    /// a legitimate "just take me back there" bookmark.
    let suggestedName: String
    /// The query params, serialized verbatim into `params`.
    let params: [String: Any]

    init(kind: String, suggestedName: String = "", params: [String: Any]) {
        self.kind = kind
        self.suggestedName = suggestedName
        self.params = params
    }
}

/// Summarizes a `params` bag for a one-line row. Shared with
/// `SearchHistoryView`, which shows the same shape.
enum SearchParamsSummary {
    static func text(_ params: [String: AnyCodable]?) -> String? {
        guard let params, !params.isEmpty else { return nil }
        for key in ["q", "query", "nl_query", "value", "term", "name"] {
            if let v = params[key]?.value as? String, !v.isEmpty { return v }
        }
        var parts: [String] = []
        for k in params.keys.sorted() {
            guard let boxed = params[k], let v = boxed.value else { continue }
            let s = display(v)
            if s.isEmpty { continue }
            parts.append("\(k)=\(s)")
            if parts.count == 4 { break }
        }
        return parts.isEmpty ? nil : parts.joined(separator: " · ")
    }

    static func display(_ v: Any) -> String {
        if let s = v as? String { return s }
        if let b = v as? Bool   { return b ? "true" : "false" }
        if let n = v as? Int    { return String(n) }
        if let d = v as? Double { return String(d) }
        if let a = v as? [Any]  { return a.map { display($0) }.joined(separator: ",") }
        return ""
    }
}

/// Human label + tone for a saved-search / history `kind`.
enum SearchKindStyle {
    static func label(_ kind: String) -> String {
        switch kind {
        case "intel":  return "INTEL"
        case "osint":  return "OSINT"
        case "entity": return "ENTITY"
        case "breach": return "BREACH"
        case "map":    return "MAP"
        default:       return kind.uppercased()
        }
    }

    static func tone(_ kind: String) -> Pill.Tone {
        switch kind {
        case "intel":  return .accent
        case "osint":  return .info
        case "entity": return .success
        case "breach": return .danger
        case "map":    return .warning
        default:       return .neutral
        }
    }
}

/// Explains, in the server's own terms, why a kind cannot become an alert.
private let nonIntelAlertExplanation =
    "Only intel searches become alert rules. Alert matching runs over incoming intel items, so an entity, breach, map or OSINT search has no stream of new items to match against — a rule made from one would either never fire or match everything."

// MARK: - Screen

/// Saved searches list. Push it (it does not own a `NavigationStack`) or wrap
/// it in one when presenting as a sheet.
struct SavedSearchesView: View {
    /// Restrict the list to one kind. `nil` shows every kind.
    var kindFilter: String? = nil
    /// Non-nil ⇒ the "Save current search" affordance appears, pre-filled.
    var currentSearch: SavedSearchDraft? = nil
    /// Called after the run counter is bumped, so the host can actually
    /// re-issue the query. The server does not execute anything.
    var onRun: ((SavedSearch) -> Void)? = nil

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var items: [SavedSearch] = []
    @State private var loading = true
    @State private var error: String?
    @State private var busyID: String?
    @State private var notice: String?

    @State private var showSaveSheet = false
    @State private var alertTarget: SavedSearch?

    @State private var renameTarget: SavedSearch?
    @State private var showRename = false
    @State private var renameText = ""

    @State private var deleteTarget: SavedSearch?
    @State private var showDelete = false

    @State private var blockedKind: String?
    @State private var showBlocked = false

    var body: some View {
        Group {
            if loading && items.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if items.isEmpty, let error {
                OfflineStateView(kind: .error,
                                 title: "Couldn't load saved searches",
                                 message: error,
                                 retry: { Task { await reload() } })
            } else if items.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Saved searches")
        .toolbar {
            if currentSearch != nil {
                ToolbarItem(placement: .compatPrimary) {
                    Button { showSaveSheet = true } label: {
                        Image(systemName: "plus")
                    }
                    .accessibilityLabel("Save current search")
                }
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await reload() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading)
                .accessibilityLabel("Reload")
            }
        }
        .task { if items.isEmpty { await reload() } }
        .refreshable { await reload() }
        .sheet(isPresented: $showSaveSheet) {
            if let draft = currentSearch {
                SaveSearchSheet(draft: draft) { created in
                    items.insert(created, at: 0)
                    notice = "Saved."
                }
            }
        }
        .sheet(item: $alertTarget) { search in
            SavedSearchAlertSheet(search: search) { message in
                notice = message
            }
        }
        .alert("Rename saved search", isPresented: $showRename,
               presenting: renameTarget) { search in
            TextField("Name", text: $renameText)
            Button("Cancel", role: .cancel) {}
            Button("Save") { Task { await rename(search) } }
        } message: { _ in
            Text("Leave the name empty to make this an unnamed bookmark.")
        }
        .confirmationDialog("Delete this saved search?",
                            isPresented: $showDelete,
                            titleVisibility: .visible,
                            presenting: deleteTarget) { search in
            Button("Delete", role: .destructive) { Task { await delete(search) } }
            Button("Cancel", role: .cancel) {}
        } message: { search in
            Text("\"\(displayName(search))\" is removed for good. Its run history stays in your search history until you clear that too.")
        }
        .alert("Can't turn this into an alert", isPresented: $showBlocked,
               presenting: blockedKind) { _ in
            Button("OK", role: .cancel) {}
        } message: { _ in
            Text(nonIntelAlertExplanation)
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            if let notice {
                Section {
                    Label(notice, systemImage: "checkmark.circle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.success)
                }
                .listRowBackground(theme.surface)
            }
            if let error, !items.isEmpty {
                Section {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.danger)
                }
                .listRowBackground(theme.surface)
            }

            if !pinned.isEmpty {
                Section {
                    ForEach(pinned) { row($0) }
                } header: {
                    sectionLabel("Pinned")
                }
            }
            if !unpinned.isEmpty {
                Section {
                    ForEach(unpinned) { row($0) }
                } header: {
                    sectionLabel(pinned.isEmpty ? "Saved" : "Everything else")
                } footer: {
                    Text("Saved searches are private to your account. Workspace owners and admins cannot read them.")
                        .font(.caption2)
                }
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
    }

    private func row(_ search: SavedSearch) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(spacing: Space.sm) {
                Pill(text: SearchKindStyle.label(search.kind),
                     tone: SearchKindStyle.tone(search.kind), size: .md)
                Text(displayName(search))
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                Spacer(minLength: Space.sm)
                if busyID == search.id {
                    ProgressView().controlSize(.small)
                } else {
                    menu(for: search)
                }
            }
            if let summary = SearchParamsSummary.text(search.params) {
                Text(summary)
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(2)
            }
            HStack(spacing: Space.sm) {
                Label("\(search.run_count)", systemImage: "play.circle")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                Text("·").font(.caption2).foregroundStyle(theme.textMuted)
                Text(search.last_run_at == nil
                     ? "never run"
                     : "last run \(relativeTime(search.last_run_at))")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                Spacer(minLength: 0)
            }
        }
        .padding(.vertical, Space.xs)
        .listRowBackground(theme.surface)
        .swipeActions(edge: .trailing, allowsFullSwipe: false) {
            Button(role: .destructive) {
                deleteTarget = search
                showDelete = true
            } label: {
                Label("Delete", systemImage: "trash")
            }
        }
        .swipeActions(edge: .leading, allowsFullSwipe: true) {
            Button {
                Task { await run(search) }
            } label: {
                Label("Run", systemImage: "play.fill")
            }
            .tint(theme.accent)
        }
    }

    private func menu(for search: SavedSearch) -> some View {
        Menu {
            Button {
                Task { await run(search) }
            } label: {
                Label("Run", systemImage: "play.fill")
            }
            Button {
                renameTarget = search
                renameText = search.name ?? ""
                showRename = true
            } label: {
                Label("Rename", systemImage: "pencil")
            }
            if search.kind == "intel" {
                Button {
                    alertTarget = search
                } label: {
                    Label("Turn into alert", systemImage: "bell.badge")
                }
            } else {
                // The conversion itself is absent rather than disabled-and-
                // silent: a dead menu row teaches nothing. This one explains.
                Button {
                    blockedKind = search.kind
                    showBlocked = true
                } label: {
                    Label("Why can't this be an alert?", systemImage: "bell.slash")
                }
            }
            Divider()
            Button(role: .destructive) {
                deleteTarget = search
                showDelete = true
            } label: {
                Label("Delete", systemImage: "trash")
            }
        } label: {
            Image(systemName: "ellipsis.circle")
                .foregroundStyle(theme.textMuted)
        }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label {
                Text("No saved searches").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "bookmark").foregroundStyle(theme.textMuted)
            }
        } description: {
            Text(currentSearch == nil
                 ? "Save a search from the Search tab and it will appear here, private to your account."
                 : "Tap + to save the search you are looking at. It stays private to your account.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            if currentSearch != nil {
                Button {
                    showSaveSheet = true
                } label: {
                    Label("Save current search", systemImage: "plus")
                }
                .buttonStyle(.borderedProminent)
                .tint(theme.accent)
            }
        }
    }

    private func sectionLabel(_ t: String) -> some View {
        Text(t.uppercased())
            .font(Typography.display(10, weight: .semibold))
            .tracking(1.2)
            .foregroundStyle(theme.textMuted)
    }

    // MARK: - Derived

    private var pinned: [SavedSearch] { items.filter { $0.pinned } }
    private var unpinned: [SavedSearch] { items.filter { !$0.pinned } }

    private func displayName(_ s: SavedSearch) -> String {
        if let n = s.name, !n.trimmingCharacters(in: .whitespaces).isEmpty {
            return n
        }
        return SearchParamsSummary.text(s.params) ?? "Untitled search"
    }

    // MARK: - Work

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            items = try await apiClient.api.savedSearches(kind: kindFilter)
            error = nil
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }

    /// Bookkeeping bump, then hand off. `savedSearchRun` deliberately does not
    /// execute the query — see the file header.
    private func run(_ search: SavedSearch) async {
        busyID = search.id
        defer { busyID = nil }
        do {
            try await apiClient.api.savedSearchRun(search.id)
            error = nil
            if onRun == nil {
                // No host to execute it — say so rather than leaving the user
                // waiting for results that are never coming.
                let where_ = SearchKindStyle.label(search.kind).lowercased()
                notice = "Marked as run. The server records the run but does not execute it — re-issue it from the \(where_) surface to see results."
            } else {
                notice = nil
            }
            Haptics.success()
            onRun?(search)
            await reload()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }

    private func rename(_ search: SavedSearch) async {
        let trimmed = renameText.trimmingCharacters(in: .whitespacesAndNewlines)
        busyID = search.id
        defer { busyID = nil }
        do {
            let updated = try await savedSearchRenameCall(apiClient.api,
                                                          id: search.id,
                                                          name: trimmed)
            if let i = items.firstIndex(where: { $0.id == updated.id }) {
                items[i] = updated
            }
            error = nil
            notice = "Renamed."
            Haptics.success()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }

    private func delete(_ search: SavedSearch) async {
        busyID = search.id
        defer { busyID = nil }
        do {
            try await apiClient.api.savedSearchDelete(search.id)
            items.removeAll { $0.id == search.id }
            error = nil
            notice = "Deleted."
            Haptics.success()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}

// MARK: - Save sheet

/// "Save current search". Kind is fixed by the caller and immutable server-side
/// (it selects both the execution endpoint and whether to-alert is legal), so
/// it is shown, not edited.
private struct SaveSearchSheet: View {
    let draft: SavedSearchDraft
    let onSaved: (SavedSearch) -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var name: String = ""
    @State private var saving = false
    @State private var error: String?

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Name", text: $name,
                              prompt: Text("Optional"))
                        .autocorrectionDisabled()
                } header: {
                    Text("Name")
                } footer: {
                    Text("Optional. An unnamed saved search still works — it just shows its query instead of a title.")
                        .font(.caption2)
                }

                Section {
                    LabeledContent("Kind") {
                        Pill(text: SearchKindStyle.label(draft.kind),
                             tone: SearchKindStyle.tone(draft.kind), size: .md)
                    }
                    if let summary = paramsSummary {
                        LabeledContent("Query") {
                            Text(summary)
                                .font(.caption.monospaced())
                                .foregroundStyle(theme.textMuted)
                                .multilineTextAlignment(.trailing)
                                .lineLimit(3)
                        }
                    }
                } header: {
                    Text("What gets saved")
                } footer: {
                    Text(draft.kind == "intel"
                         ? "Intel searches can later be turned into an alert rule."
                         : "Only intel searches can later become alert rules — this kind cannot. The kind is fixed once saved.")
                        .font(.caption2)
                }

                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                            .textSelection(.enabled)
                    }
                }
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("Save search")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button {
                        Task { await save() }
                    } label: {
                        if saving { ProgressView().controlSize(.small) }
                        else { Text("Save").bold() }
                    }
                    .disabled(saving)
                }
            }
            .onAppear { if name.isEmpty { name = draft.suggestedName } }
        }
        .compatSheetSizing(.form)
    }

    private var paramsSummary: String? {
        var boxed: [String: AnyCodable] = [:]
        for (k, v) in draft.params { boxed[k] = AnyCodable(v) }
        return SearchParamsSummary.text(boxed)
    }

    private func save() async {
        saving = true
        defer { saving = false }
        do {
            let created = try await apiClient.api.savedSearchCreate(
                name: name.trimmingCharacters(in: .whitespacesAndNewlines),
                kind: draft.kind,
                params: draft.params)
            Haptics.success()
            onSaved(created)
            dismiss()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}

// MARK: - Saved search → alert rule

/// The upsell. Collects one delivery channel and posts it to
/// `POST /api/saved-searches/:id/to-alert`, which re-enters the ordinary alert
/// rule validator — so the channel rules here are exactly the alert editor's
/// rules, and any refusal comes back in the server's own words.
private struct SavedSearchAlertSheet: View {
    let search: SavedSearch
    let onCreated: (String) -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var ruleName: String = ""
    @State private var channelType: AlertChannel.Kind = .webhook
    @State private var target: String = ""
    @State private var secret: String = ""
    @State private var saving = false
    @State private var error: String?

    var body: some View {
        NavigationStack {
            Form {
                if search.kind != "intel" {
                    Section {
                        Label(nonIntelAlertExplanation,
                              systemImage: "exclamationmark.triangle.fill")
                            .font(.caption)
                            .foregroundStyle(theme.warning)
                    }
                }

                Section {
                    TextField("Rule name", text: $ruleName,
                              prompt: Text("Optional"))
                        .autocorrectionDisabled()
                } header: {
                    Text("Rule")
                } footer: {
                    Text("Defaults to the saved search's own name.")
                        .font(.caption2)
                }

                Section {
                    Picker("Deliver to", selection: $channelType) {
                        ForEach(AlertChannel.Kind.allCases) { k in
                            Text(k.label).tag(k)
                        }
                    }
                    .pickerStyle(.segmented)

                    TextField("",
                              text: $target,
                              prompt: Text(channelType == .email
                                           ? "alerts@example.co.jp"
                                           : "https://example.com/hook")
                                .foregroundColor(theme.accent))
                        .font(.system(.body, design: .monospaced))
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                        .compatKeyboard(email: channelType == .email)

                    if channelType == .webhook {
                        SecureField("",
                                    text: $secret,
                                    prompt: Text("Signing secret (≥16 characters)")
                                        .foregroundColor(theme.accent))
                            .font(.system(.body, design: .monospaced))
                            .compatNoAutocap()
                            .autocorrectionDisabled()
                    }
                } header: {
                    Text("Channel")
                } footer: {
                    Text(channelType == .webhook
                         ? "The secret signs every delivery so your endpoint can verify it came from this server. It must be at least 16 characters."
                         : "The rule fires on new intel items that match this search. At least one channel is required.")
                        .font(.caption2)
                }

                Section {
                    Text("Time bounds are dropped when converting. An alert scores each new item as it arrives, so a fixed historical window would produce a rule that can never fire again.")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }

                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                            .textSelection(.enabled)
                    }
                }
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("Turn into alert")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button {
                        Task { await create() }
                    } label: {
                        if saving { ProgressView().controlSize(.small) }
                        else { Text("Create").bold() }
                    }
                    .disabled(saving || search.kind != "intel")
                }
            }
            .onAppear { if ruleName.isEmpty { ruleName = search.name ?? "" } }
        }
        .compatSheetSizing(.form)
    }

    /// Client-side mirror of the server's channel validator, so the common
    /// mistakes never cost a round trip. The server still has the last word.
    private func localProblem() -> String? {
        let t = target.trimmingCharacters(in: .whitespaces)
        if t.isEmpty { return "Add a delivery target." }
        switch channelType {
        case .email:
            let parts = t.split(separator: "@", omittingEmptySubsequences: false)
            guard parts.count == 2, !parts[0].isEmpty, !parts[1].isEmpty,
                  parts[1].contains(".") , !parts[1].hasPrefix("."),
                  !parts[1].hasSuffix(".") else {
                return "That is not a valid email address."
            }
            if t.contains(where: { $0.isWhitespace }) {
                return "That is not a valid email address."
            }
        case .webhook:
            if !t.hasPrefix("http://") && !t.hasPrefix("https://") {
                return "A webhook target must be an http:// or https:// URL."
            }
            if secret.count < 16 {
                return "The webhook signing secret must be at least 16 characters."
            }
        }
        return nil
    }

    private func create() async {
        if let problem = localProblem() {
            error = problem
            Haptics.warning()
            return
        }
        saving = true
        defer { saving = false }

        var channel: [String: Any] = [
            "type": channelType.rawValue,
            "target": target.trimmingCharacters(in: .whitespaces)
        ]
        if channelType == .webhook { channel["secret"] = secret }

        do {
            try await apiClient.api.savedSearchToAlert(search.id,
                                                       channels: [channel])
            Haptics.success()
            onCreated("Alert rule created. Manage it under Console › Alerts.")
            dismiss()
        } catch {
            // The server's refusal IS the explanation — for a non-intel kind it
            // writes a full sentence naming the kind, and for a channel problem
            // it writes the alert editor's own wording. Show it as written.
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}

// MARK: - The one call API+Roadmap.swift does not expose

/// `PATCH /api/saved-searches/:id` — rename.
///
/// `API+Roadmap.swift` exposes list / create / delete / run / to-alert but no
/// update, and that file is owned by the orchestrator, so the single call this
/// screen needs lives here as a file-private helper over the SAME `API` value:
/// its `makeURL` + `patch` helpers, its bearer/tenant headers, its coalesced
/// 401 refresh. It is not a second networking stack, and it should be deleted
/// the moment `API.savedSearchUpdate` lands.
///
/// An empty `name` is sent as an empty string, which the server trims to NULL —
/// that is how a bookmark is deliberately made unnamed. `kind` is never sent:
/// the server rejects an attempt to change it.
private func savedSearchRenameCall(_ api: API, id: String,
                                   name: String) async throws -> SavedSearch {
    let body = try JSONSerialization.data(withJSONObject: ["name": name])
    let env: SavedSearchEnvelope = try await api.patch(
        "/api/saved-searches/\(savedSearchPathSegment(id))", body: body)
    return env.data
}

/// Percent-escape one path segment (saved-search ids are uuids today, but the
/// escaping is what keeps that true if they ever stop being).
private func savedSearchPathSegment(_ s: String) -> String {
    s.addingPercentEncoding(withAllowedCharacters: .alphanumerics
        .union(CharacterSet(charactersIn: "-._~"))) ?? s
}
