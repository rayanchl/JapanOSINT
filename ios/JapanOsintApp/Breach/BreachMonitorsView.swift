import SwiftUI
import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap item 24 — standing breach exposure monitors.
//
// THE IDENTIFIER IS SENT ONCE AND NEVER STORED.
// `POST /api/breach-monitors` takes the plaintext value, normalizes it exactly
// the way the breach index normalizes the matching type (so "+81 90-1234-5678"
// and "+819012345678" are the same order), hashes it, and stores ONLY the
// SHA-1. Nothing echoes the value back: not the list, not the detail, not the
// audit payload — the audit row carries a 10-character hash prefix, which is
// the same non-reversible key the index already publishes.
//
// That is the whole reason someone is willing to put their CEO's address into
// this form, so the UI says it at the point of entry rather than burying it in
// a settings screen. It also means this view can NEVER redisplay what you typed
// — there is nothing to redisplay — so the label is what identifies a monitor.
//
// Client-side validation below mirrors the server's `norm_ident()` per kind.
// It exists to save a round trip on the obvious mistakes, not to replace the
// server's check; the server still has the last word and its refusal is shown
// as written.
// ─────────────────────────────────────────────────────────────────────────────

/// The four identifier kinds a monitor can watch, with the shape rules the
/// server enforces.
enum BreachIdentifierKind: String, CaseIterable, Identifiable {
    case email, domain, username, phone

    var id: String { rawValue }

    var label: String {
        switch self {
        case .email:    return "Email"
        case .domain:   return "Domain"
        case .username: return "Username"
        case .phone:    return "Phone"
        }
    }

    var icon: String {
        switch self {
        case .email:    return "envelope.fill"
        case .domain:   return "globe"
        case .username: return "person.fill"
        case .phone:    return "phone.fill"
        }
    }

    var placeholder: String {
        switch self {
        case .email:    return "name@example.co.jp"
        case .domain:   return "example.co.jp"
        case .username: return "handle"
        case .phone:    return "+81 90-1234-5678"
        }
    }

    /// Shown under the field. Describes the shape, never an example of a real
    /// stored value.
    var shapeHint: String {
        switch self {
        case .email:
            return "A full address. It is not canonicalized — dots and plus-tags in the local part are kept, because two providers disagree about whether they matter."
        case .domain:
            return "A bare host. \"@example.co.jp\" and \"*.example.co.jp\" are accepted and stored as \"example.co.jp\". A domain monitor matches every address at that domain."
        case .username:
            return "Lower-cased and trimmed, nothing else. No spaces."
        case .phone:
            return "7–15 digits. Spaces, dashes and brackets are ignored; a leading + and country code are kept."
        }
    }

    var tone: Pill.Tone {
        switch self {
        case .email:    return .accent
        case .domain:   return .info
        case .username: return .success
        case .phone:    return .warning
        }
    }
}

/// Client-side mirror of the server's `norm_ident()`.
enum BreachIdentifier {
    /// The normalized form, or `nil` when the input cannot be an identifier of
    /// that kind.
    static func normalized(_ kind: BreachIdentifierKind, _ raw: String) -> String? {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty || trimmed.count > 320 { return nil }

        if kind == .phone {
            let plus = trimmed.hasPrefix("+")
            let digits = String(trimmed.filter { $0.isASCII && $0.isNumber })
            if digits.count < 7 || digits.count > 15 { return nil }
            return plus ? "+" + digits : digits
        }

        let lowered = trimmed.lowercased()
        if lowered.contains(where: { $0.isWhitespace }) { return nil }

        switch kind {
        case .email:
            let parts = lowered.split(separator: "@", omittingEmptySubsequences: false)
            guard parts.count == 2, !parts[0].isEmpty, !parts[1].isEmpty else {
                return nil
            }
            guard hasUsableDot(String(parts[1])) else { return nil }
            return lowered
        case .domain:
            var host = lowered
            if host.hasPrefix("@") { host.removeFirst() }
            if host.hasPrefix("*.") { host.removeFirst(2) }
            guard !host.isEmpty, !host.contains("@"), !host.contains("/") else {
                return nil
            }
            guard hasUsableDot(host) else { return nil }
            return host
        case .username:
            return lowered
        case .phone:
            return nil    // handled above
        }
    }

    /// A dot that is neither the first nor the last character.
    private static func hasUsableDot(_ host: String) -> Bool {
        guard let dot = host.firstIndex(of: ".") else { return false }
        if dot == host.startIndex { return false }
        if host.index(after: dot) == host.endIndex { return false }
        return true
    }

    /// A sentence naming what is wrong, or `nil` when the value is acceptable.
    static func problem(_ kind: BreachIdentifierKind, _ raw: String) -> String? {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty { return "Enter an identifier to watch." }
        if trimmed.count > 320 { return "That is too long to be an identifier." }
        if normalized(kind, raw) != nil { return nil }
        switch kind {
        case .email:
            return "That is not a full email address — it needs one @ and a host with a dot in it."
        case .domain:
            return "That is not a domain — it needs a dot, and no @ or / in it."
        case .username:
            return "A username cannot contain spaces."
        case .phone:
            return "A phone number needs 7 to 15 digits, optionally with a leading + and country code."
        }
    }
}

// MARK: - Screen

/// Breach exposure monitors. Push it (it does not own a `NavigationStack`) or
/// wrap it in one when presenting as a sheet.
struct BreachMonitorsView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var monitors: [BreachMonitor] = []
    @State private var loading = true
    @State private var error: String?
    @State private var notice: String?

    @State private var showCreate = false
    @State private var detailTarget: BreachMonitor?
    @State private var deleteTarget: BreachMonitor?
    @State private var showDelete = false
    @State private var busyID: String?

    var body: some View {
        Group {
            if loading && monitors.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if monitors.isEmpty, let error {
                OfflineStateView(kind: .error,
                                 title: "Couldn't load monitors",
                                 message: error,
                                 retry: { Task { await reload() } })
            } else if monitors.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Breach monitors")
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Button { showCreate = true } label: {
                    Image(systemName: "plus")
                }
                .accessibilityLabel("Add monitor")
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await reload() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading)
                .accessibilityLabel("Reload")
            }
        }
        .task { if monitors.isEmpty { await reload() } }
        .refreshable { await reload() }
        .sheet(isPresented: $showCreate) {
            BreachMonitorCreateSheet { created, initialHits in
                monitors.insert(created, at: 0)
                // A nil count means the server did not report one — NOT zero.
                if let n = initialHits {
                    notice = n > 0
                        ? "Monitor created — \(n) existing record\(n == 1 ? "" : "s") already match."
                        : "Monitor created. Nothing in the corpus matches it yet."
                } else {
                    notice = "Monitor created."
                }
            }
        }
        .sheet(item: $detailTarget) { monitor in
            BreachMonitorDetailSheet(monitor: monitor)
        }
        .confirmationDialog("Delete this monitor?",
                            isPresented: $showDelete,
                            titleVisibility: .visible,
                            presenting: deleteTarget) { monitor in
            Button("Delete", role: .destructive) { Task { await delete(monitor) } }
            Button("Cancel", role: .cancel) {}
        } message: { monitor in
            Text("\"\(displayLabel(monitor))\" stops watching. The stored hash is removed and cannot be recovered — you would have to enter the identifier again to recreate it.")
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            Section {
                privacyBanner
            }
            .listRowBackground(theme.surface)

            if let notice {
                Section {
                    Label(notice, systemImage: "checkmark.circle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.success)
                }
                .listRowBackground(theme.surface)
            }
            if let error, !monitors.isEmpty {
                Section {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.danger)
                }
                .listRowBackground(theme.surface)
            }

            Section {
                ForEach(monitors) { monitor in
                    row(monitor)
                }
            } header: {
                Text("WATCHING")
                    .font(Typography.display(10, weight: .semibold))
                    .tracking(1.2)
                    .foregroundStyle(theme.textMuted)
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
    }

    private func row(_ monitor: BreachMonitor) -> some View {
        Button {
            detailTarget = monitor
        } label: {
            HStack(spacing: Space.sm) {
                VStack(alignment: .leading, spacing: Space.xs) {
                    HStack(spacing: Space.sm) {
                        Pill(text: monitor.kind.uppercased(),
                             tone: kindTone(monitor.kind), size: .md)
                        Text(displayLabel(monitor))
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                    }
                    HStack(spacing: Space.sm) {
                        Text(monitor.last_checked_at == nil
                             ? "never checked"
                             : "checked \(relativeTime(monitor.last_checked_at))")
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                        if monitor.rule_id != nil {
                            Pill(text: "ALERTS", tone: .info, icon: "bell.fill")
                        }
                        Spacer(minLength: 0)
                    }
                }
                Spacer(minLength: Space.sm)
                if busyID == monitor.id {
                    ProgressView().controlSize(.small)
                } else {
                    hitCountView(monitor)
                }
                Image(systemName: "chevron.right")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
            .padding(.vertical, Space.xs)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .listRowBackground(theme.surface)
        .swipeActions(edge: .trailing, allowsFullSwipe: false) {
            Button(role: .destructive) {
                deleteTarget = monitor
                showDelete = true
            } label: {
                Label("Delete", systemImage: "trash")
            }
        }
    }

    /// `hit_count` is only sent on create/detail responses, not on the list —
    /// so an absent count means "not reported here", never "zero hits".
    @ViewBuilder
    private func hitCountView(_ monitor: BreachMonitor) -> some View {
        if let n = monitor.hit_count {
            Pill(text: "\(n)", tone: n > 0 ? .danger : .neutral, size: .md)
        } else {
            Pill(text: "—", tone: .neutral, size: .md)
        }
    }

    private var privacyBanner: some View {
        HStack(alignment: .top, spacing: Space.md) {
            Image(systemName: "lock.shield.fill")
                .font(.subheadline)
                .foregroundStyle(theme.accent)
                .frame(width: 24)
            VStack(alignment: .leading, spacing: 2) {
                Text("Only a hash is stored")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                Text("The address, domain, username or number you enter is sent once, hashed, and discarded. It is never written to the database, never returned by the API and never appears in an audit log — which is why a monitor is identified here by its label.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
        .padding(.vertical, Space.xs)
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label {
                Text("No monitors yet").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "shield.lefthalf.filled")
                    .foregroundStyle(theme.textMuted)
            }
        } description: {
            Text("Watch an address, domain, username or phone number for appearances in the breach corpus. Only a hash of it is stored.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            Button {
                showCreate = true
            } label: {
                Label("Add monitor", systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)
            .tint(theme.accent)
        }
    }

    // MARK: - Derived

    private func displayLabel(_ m: BreachMonitor) -> String {
        if let l = m.label, !l.trimmingCharacters(in: .whitespaces).isEmpty {
            return l
        }
        return "Unlabelled \(m.kind) monitor"
    }

    private func kindTone(_ kind: String) -> Pill.Tone {
        BreachIdentifierKind(rawValue: kind)?.tone ?? .neutral
    }

    // MARK: - Work

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            monitors = try await apiClient.api.breachMonitors()
            error = nil
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }

    private func delete(_ monitor: BreachMonitor) async {
        busyID = monitor.id
        defer { busyID = nil }
        do {
            try await apiClient.api.breachMonitorDelete(monitor.id)
            monitors.removeAll { $0.id == monitor.id }
            error = nil
            notice = "Monitor deleted."
            Haptics.success()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}

// MARK: - Create

/// Creation form. The identifier field is the one place in the app that ever
/// holds a plaintext monitored value, so the reassurance sits directly under
/// it — not in a help screen someone has to go looking for.
private struct BreachMonitorCreateSheet: View {
    /// `(created, initialHits)` — the create response reports how many corpus
    /// records already match, which is the first thing someone wants to know.
    /// `nil` means the server did not report a count, never "zero".
    let onCreated: (BreachMonitor, Int?) -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var kind: BreachIdentifierKind = .email
    @State private var value: String = ""
    @State private var label: String = ""
    @State private var saving = false
    @State private var error: String?
    @State private var showValidation = false

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Picker("Kind", selection: $kind) {
                        ForEach(BreachIdentifierKind.allCases) { k in
                            Label(k.label, systemImage: k.icon).tag(k)
                        }
                    }
                    .pickerStyle(.menu)
                } header: {
                    Text("Watch")
                }

                Section {
                    TextField("",
                              text: $value,
                              prompt: Text(kind.placeholder)
                                .foregroundColor(theme.accent))
                        .font(.system(.body, design: .monospaced))
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                        .compatKeyboard(email: kind == .email)

                    if showValidation, let problem = BreachIdentifier.problem(kind, value) {
                        Label(problem, systemImage: "exclamationmark.triangle.fill")
                            .font(.caption2)
                            .foregroundStyle(theme.warning)
                    }

                    HStack(alignment: .top, spacing: Space.sm) {
                        Image(systemName: "lock.shield.fill")
                            .font(.caption)
                            .foregroundStyle(theme.accent)
                        Text("Sent once, hashed, then discarded. Only the SHA-1 is stored — this value is never written to the database, never returned by the API, and never appears in an audit log. You will not see it again after saving.")
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                    .padding(.vertical, Space.xs)
                } header: {
                    Text(kind.label)
                } footer: {
                    Text(kind.shapeHint)
                        .font(.caption2)
                }

                Section {
                    TextField("Label", text: $label,
                              prompt: Text("e.g. CEO personal address"))
                        .autocorrectionDisabled()
                } header: {
                    Text("Label")
                } footer: {
                    Text("How this monitor is identified everywhere in the app. Because the identifier itself is never stored, a label is the only way to tell two monitors apart — pick something you will recognise, but nothing you would not want a colleague to read.")
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
            .navigationTitle("New monitor")
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
                        else { Text("Add").bold() }
                    }
                    .disabled(saving)
                }
            }
            .onChange(of: value) { _, _ in showValidation = false }
        }
        .compatSheetSizing(.form)
    }

    private func create() async {
        guard let normalized = BreachIdentifier.normalized(kind, value) else {
            showValidation = true
            error = BreachIdentifier.problem(kind, value)
            Haptics.warning()
            return
        }
        let trimmedLabel = label.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmedLabel.count > 200 {
            error = "That label is too long (200 characters maximum)."
            Haptics.warning()
            return
        }

        saving = true
        defer { saving = false }
        do {
            let created = try await apiClient.api.breachMonitorCreate(
                kind: kind.rawValue,
                value: normalized,
                label: trimmedLabel.isEmpty ? nil : trimmedLabel)
            // Drop the plaintext from memory the moment the request returns.
            value = ""
            Haptics.success()
            onCreated(created, created.hit_count)
            dismiss()
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}

// MARK: - Detail / hits

/// One monitor's current matches. There is no stored identifier to show, so the
/// header describes the monitor by label + kind and nothing else.
private struct BreachMonitorDetailSheet: View {
    let monitor: BreachMonitor

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var hits: [BreachHitRow] = []
    @State private var loading = true
    @State private var error: String?

    var body: some View {
        NavigationStack {
            Group {
                if loading && hits.isEmpty {
                    ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if hits.isEmpty, let error {
                    OfflineStateView(kind: .error,
                                     title: "Couldn't load matches",
                                     message: error,
                                     retry: { Task { await load() } })
                } else if hits.isEmpty {
                    clearState
                } else {
                    list
                }
            }
            .themedScreenBackground(theme)
            .navigationTitle(monitor.label ?? "\(monitor.kind.capitalized) monitor")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Done") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button { Task { await load() } } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .disabled(loading)
                }
            }
            .task { if hits.isEmpty { await load() } }
        }
        .compatSheetSizing(.large)
    }

    private var list: some View {
        List {
            if let error {
                Section {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.danger)
                }
                .listRowBackground(theme.surface)
            }
            Section {
                ForEach(hits) { hit in
                    hitRow(hit)
                }
            } header: {
                Text("\(hits.count) MATCH\(hits.count == 1 ? "" : "ES")")
                    .font(Typography.display(10, weight: .semibold))
                    .tracking(1.2)
                    .foregroundStyle(theme.textMuted)
            } footer: {
                Text("Each match is a record in a known breach corpus. Passwords and other secrets stay redacted — there is no reveal path from here.")
                    .font(.caption2)
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
        .refreshable { await load() }
    }

    private func hitRow(_ hit: BreachHitRow) -> some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            HStack(spacing: Space.sm) {
                Text(hit.title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
                Spacer(minLength: Space.sm)
                if hit.sensitive == true {
                    Pill(text: "SENSITIVE", tone: .danger, icon: "eye.slash")
                }
                if hit.verified == true {
                    Pill(text: "VERIFIED", tone: .success, icon: "checkmark.seal")
                }
            }
            HStack(spacing: Space.sm) {
                if let d = hit.breachDate {
                    Text(d)
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                if let n = hit.pwnCount {
                    Text("· \(n) accounts")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                if let t = hit.type {
                    Text("· \(t)")
                        .font(.caption2.monospaced())
                        .foregroundStyle(theme.textMuted)
                }
                Spacer(minLength: 0)
            }
            if !hit.dataClasses.isEmpty {
                FlowLayout(spacing: Space.xs) {
                    ForEach(hit.dataClasses, id: \.self) { c in
                        Pill(text: c.uppercased(), tone: .warning, maxWidth: 160)
                    }
                }
            }
            if hit.hasSecret == true {
                Label("A secret is stored for this record and stays redacted.",
                      systemImage: "key.slash")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
        .padding(.vertical, Space.xs)
        .listRowBackground(theme.surface)
    }

    private var clearState: some View {
        ContentUnavailableView {
            Label {
                Text("No matches").foregroundStyle(theme.text)
            } icon: {
                Image(systemName: "checkmark.shield")
                    .foregroundStyle(theme.success)
            }
        } description: {
            Text("Nothing in the breach corpus currently matches this monitor. That is a statement about the corpus this server has indexed, not a guarantee the identifier has never been exposed.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            Button {
                Task { await load() }
            } label: {
                Label("Check again", systemImage: "arrow.clockwise")
            }
            .buttonStyle(.bordered)
            .tint(theme.accent)
        }
    }

    private func load() async {
        loading = true
        defer { loading = false }
        let api = apiClient.api
        do {
            do {
                hits = try await api.breachMonitorHits(monitor.id).map { BreachHitRow($0) }
            } catch APIError.decoding(_) {
                // See `monitorHitsCompat` — the shipped DTO and the server's
                // payload disagree today. Everything else propagates.
                hits = try await monitorHitsCompat(api, id: monitor.id)
            }
            error = nil
        } catch {
            self.error = ServerError.message(error)
            Haptics.error()
        }
    }
}

// MARK: - Hit row model

/// One breach match, flattened from whichever shape the server returned.
struct BreachHitRow: Identifiable {
    let id: String
    let uid: String?
    let breachId: String?
    let name: String?
    let type: String?
    let breachDate: String?
    let pwnCount: Int?
    let dataClasses: [String]
    let verified: Bool?
    let sensitive: Bool?
    let hasSecret: Bool?

    var title: String {
        if let name, !name.isEmpty { return name }
        if let breachId, !breachId.isEmpty { return breachId }
        return "Unnamed breach"
    }

    init(_ b: EntityBreach) {
        id = b.breach_id
        uid = b.item_uid
        breachId = b.breach_id
        name = b.title ?? b.name
        type = nil
        breachDate = b.breach_date
        pwnCount = b.pwn_count
        dataClasses = b.data_classes
        verified = b.verified
        sensitive = b.sensitive
        hasSecret = nil
    }

    /// The DTO now models the real `/hits` payload (nested `breach{…}` + a
    /// top-level uid/type/has_secret), so flatten that shape directly instead of
    /// the old `EntityBreach` assumption the primary call used to make.
    init(_ h: BreachMonitorHit) {
        id = h.uid
        uid = h.uid
        breachId = h.breach_id
        name = h.breach?.title ?? h.breach?.name
        type = h.type
        breachDate = h.breach?.breach_date
        pwnCount = h.breach?.pwn_count
        dataClasses = h.breach?.data_classes ?? []
        verified = h.breach?.verified
        sensitive = h.breach?.sensitive
        hasSecret = h.has_secret
    }

    init(id: String, uid: String?, breachId: String?, name: String?,
         type: String?, breachDate: String?, pwnCount: Int?,
         dataClasses: [String], verified: Bool?, sensitive: Bool?,
         hasSecret: Bool?) {
        self.id = id; self.uid = uid; self.breachId = breachId
        self.name = name; self.type = type; self.breachDate = breachDate
        self.pwnCount = pwnCount; self.dataClasses = dataClasses
        self.verified = verified; self.sensitive = sensitive
        self.hasSecret = hasSecret
    }
}

// MARK: - Hits compatibility decode

/// `GET /api/breach-monitors/:id/hits` returns rows shaped
/// `{uid, type, breach_id, has_secret, count, first_seen, breach:{…}}`, but
/// `API.breachMonitorHits` decodes them as `[EntityBreach]`, which expects a
/// FLAT row with a non-optional `data_classes`. Those two do not match, so the
/// shipped call throws `APIError.decoding` against the current server.
///
/// `API+Roadmap.swift` and `Models+Roadmap.swift` are orchestrator-owned, so
/// this file cannot fix the DTO. Instead the detail sheet tries the shipped
/// call first and falls back to this decode of the SAME endpoint over the SAME
/// `API` value (its `makeURL`/`request`, its auth headers, its retry). Delete
/// this once `EntityBreach` and the payload agree.
private struct MonitorHitsWire: Decodable {
    struct Meta: Decodable {
        let name: String?
        let title: String?
        let domain: String?
        let breach_date: String?
        let pwn_count: Int?
        let data_classes: [String]?
        let verified: Bool?
        let sensitive: Bool?
    }
    struct Row: Decodable {
        let uid: String?
        let type: String?
        let breach_id: String?
        let has_secret: Bool?
        let count: Int?
        let first_seen: String?
        let breach: Meta?
    }
    let data: [Row]
}

private func monitorHitsCompat(_ api: API, id: String) async throws -> [BreachHitRow] {
    let url = try api.makeURL("/api/breach-monitors/\(breachMonitorPathSegment(id))/hits")
    let data = try await api.request(url)
    let wire = try JSONDecoder().decode(MonitorHitsWire.self, from: data)
    var rows: [BreachHitRow] = []
    for (i, r) in wire.data.enumerated() {
        rows.append(BreachHitRow(
            // `uid` is unique per corpus record; the index is only a fallback
            // so a malformed row can still render instead of colliding.
            id: r.uid ?? "hit-\(i)",
            uid: r.uid,
            breachId: r.breach_id,
            name: r.breach?.title ?? r.breach?.name ?? r.breach?.domain,
            type: r.type,
            breachDate: r.breach?.breach_date,
            pwnCount: r.breach?.pwn_count,
            dataClasses: r.breach?.data_classes ?? [],
            verified: r.breach?.verified,
            sensitive: r.breach?.sensitive,
            hasSecret: r.has_secret))
    }
    return rows
}

private func breachMonitorPathSegment(_ s: String) -> String {
    s.addingPercentEncoding(withAllowedCharacters: .alphanumerics
        .union(CharacterSet(charactersIn: "-._~"))) ?? s
}
