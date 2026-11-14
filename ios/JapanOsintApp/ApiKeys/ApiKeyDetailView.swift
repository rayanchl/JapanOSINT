import SwiftUI

/// Detail sheet for one API key. Reveal value (Face ID), edit + save (Face
/// ID), or clear it (Face ID). Works for both the workspace BYOK store and
/// the operator platform overlay (selected by `scope`). The list view is
/// updated via the `onUpdate` closure so callers don't re-fetch the whole
/// list after every mutation.
struct ApiKeyDetailView: View {
    let row: KeyRow
    let scope: KeyScope
    /// Owner-policy gate (workspace). False → reveal-only, no edit/clear.
    let canManage: Bool
    let onUpdate: (KeyRow) -> Void

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var revealedValue: String?
    @State private var newValue: String = ""
    @State private var workingRow: KeyRow
    /// Sources the SERVER reports as waiting for this key (a filtered, paged
    /// search of `health=gated`, whose rows carry `credentials_required`), not
    /// a scan of the whole registry. Best-effort.
    @State private var usingRows: [SourceSearchRow] = []
    @State private var usingTotal: Int?
    @State private var usingLoading = false
    @State private var usingError: String?
    @State private var inFlight: Bool = false
    @State private var error: String?
    @State private var savedHint: Bool = false

    init(row: KeyRow,
         scope: KeyScope,
         canManage: Bool,
         onUpdate: @escaping (KeyRow) -> Void) {
        self.row = row
        self.scope = scope
        self.canManage = canManage
        self.onUpdate = onUpdate
        self._workingRow = State(initialValue: row)
    }

    private var api: API { apiClient.api }

    /// Whether `workingRow` currently has a value set in *this* store
    /// (tenant key for workspace, overlay value for platform).
    private var hasOwnValue: Bool { workingRow.byok }

    /// Most rows listed (each may fetch its own probe state), with the
    /// remainder stated rather than silently cut.
    private static let usingMax = 50

    private func loadUsing() async {
        usingLoading = true
        defer { usingLoading = false }
        var q = SourceSearchQuery()
        q.health = "gated"
        var found: [SourceSearchRow] = []
        var offset = 0
        // Walk the gated sources 500 at a time (a bounded number of pages) and
        // keep those that name this key.
        for _ in 0..<10 {
            do {
                let env = try await api.searchSources(q, limit: 500, offset: offset)
                let rows = env.data ?? []
                found += rows.filter { $0.health?.credentials_required?.contains(row.name) == true }
                offset += rows.count
                if env.page?.has_more != true || rows.isEmpty { break }
            } catch {
                usingError = error.localizedDescription
                break
            }
        }
        usingTotal = found.count
        usingRows = Array(found.prefix(Self.usingMax))
    }

    /// Shown when Reveal returns no value. For workspace scope an admin-set
    /// key has no tenant value to reveal — make explicit that the key exists
    /// but its value is hidden from members.
    private var revealEmptyMessage: String {
        guard scope == .workspace else { return "(no value set)" }
        return workingRow.source == "platform"
            ? "(provided by admin — value hidden)"
            : "(no key set for this workspace)"
    }

    private var sourceLabel: String {
        switch workingRow.source {
        case "tenant":   return scope == .workspace ? "Your key" : "Overlay"
        case "platform": return scope == .workspace ? "Set by admin" : "Platform default"
        default:         return "Not set"
        }
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    LabeledContent("Name") {
                        Text(workingRow.name)
                            .font(.system(.body, design: .monospaced))
                    }
                    LabeledContent("Role") {
                        Text(workingRow.role.capitalized)
                            .foregroundStyle(theme.textMuted)
                    }
                    LabeledContent("Status") {
                        Text(workingRow.set ? "Set" : "Unset")
                            .foregroundStyle(workingRow.set ? theme.success : theme.warning)
                    }
                    LabeledContent("Source") {
                        Text(sourceLabel)
                            .foregroundStyle(workingRow.byok ? theme.accent : theme.textMuted)
                    }
                } header: {
                    Text("Key")
                } footer: {
                    if scope == .workspace {
                        Text("Your key is encrypted per workspace. When unset, collectors fall back to the platform default automatically.")
                            .font(.caption2)
                    }
                }

                Section {
                    if let revealed = revealedValue {
                        if revealed.isEmpty {
                            Text(revealEmptyMessage)
                                .foregroundStyle(theme.textMuted)
                                .italic()
                        } else {
                            TextField("Value", text: .constant(revealed), axis: .vertical)
                                .font(.system(.body, design: .monospaced))
                                .disabled(true)
                            Button {
                                Clipboard.copy(revealed)
                            } label: {
                                Label("Copy", systemImage: "doc.on.doc")
                            }
                        }
                    } else {
                        Button {
                            Task { await reveal() }
                        } label: {
                            Label(inFlight ? "Authenticating…" : "Reveal value",
                                  systemImage: "faceid")
                        }
                        .disabled(inFlight)
                    }
                } header: {
                    Text("Current value")
                } footer: {
                    Text(scope == .workspace
                         ? "Reveals your workspace's own key only (never the platform key). Requires Face ID."
                         : "Reveal requires Face ID (or device passcode).")
                        .font(.caption2)
                }

                if canManage {
                    Section {
                        HStack(spacing: 8) {
                            SecureField("New value", text: $newValue)
                                .font(.system(.body, design: .monospaced))
                                .compatNoAutocap()
                                .autocorrectionDisabled(true)
                                .submitLabel(.go)
                                .onSubmit {
                                    if !newValue.isEmpty && !inFlight {
                                        Task { await save() }
                                    }
                                }
                            Button {
                                Task { await save() }
                            } label: {
                                Group {
                                    if inFlight {
                                        ProgressView().controlSize(.small)
                                    } else {
                                        Image(systemName: "checkmark.circle.fill")
                                            .font(.title3)
                                            .accessibilityLabel("Saved")
                                    }
                                }
                                .frame(width: 28, height: 28)
                            }
                            .buttonStyle(.borderless)
                            .disabled(inFlight || newValue.isEmpty)
                            .accessibilityLabel("Save")
                        }
                    } header: {
                        Text("Modify")
                    } footer: {
                        Text("Save requires Face ID. Empty values aren't accepted here — use 'Clear' below to drop back to the platform default.")
                            .font(.caption2)
                    }

                    if hasOwnValue {
                        Section {
                            Button(role: .destructive) {
                                Task { await clear() }
                            } label: {
                                Label(inFlight ? "Clearing…" : "Clear",
                                      systemImage: "xmark.circle")
                            }
                            .disabled(inFlight)
                        } footer: {
                            Text(scope == .workspace
                                 ? "Removes your workspace's key. Collectors fall back to the platform default if one exists."
                                 : "Removes the overlay value. The server falls back to the .env-baked value if any.")
                                .font(.caption2)
                        }
                    }
                } else {
                    Section {
                        Label("Editing is restricted by your workspace owner.",
                              systemImage: "lock.fill")
                            .font(.caption)
                            .foregroundStyle(theme.textMuted)
                    }
                }

                usedBySection

                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.warning)
                    }
                }

                if savedHint {
                    Section {
                        Label("Saved.", systemImage: "checkmark.circle.fill")
                            .foregroundStyle(theme.success)
                    }
                }
            }
            .navigationTitle(row.name)
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatPrimary) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .compatSheetSizing(.medium)
        .task { await loadUsing() }
    }

    // MARK: - Used by

    @ViewBuilder
    private var usedBySection: some View {
        Section {
            if usingLoading && usingRows.isEmpty {
                ProgressView()
            } else if let usingError, usingRows.isEmpty {
                Text("Couldn't look up which sources use this key: \(usingError)")
                    .font(.caption)
                    .foregroundStyle(theme.warning)
            } else if usingRows.isEmpty {
                Text("No source is currently waiting for this key.")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            } else {
                ForEach(usingRows) { r in
                    UsedByRow(source: r, keyName: row.name) {
                        dismiss()
                        nav.showSource(r.id)
                    }
                }
                if let total = usingTotal, total > usingRows.count {
                    Text("Showing \(usingRows.count) of \(total) sources waiting for this key.")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
        } header: {
            Text("Waiting for this key")
        } footer: {
            Text("Sources the server reports as gated on this key. A source whose key is already set is no longer gated and is not listed. Tap one to open its detail.")
                .font(.caption2)
        }
    }

    private func rolePill(_ role: String) -> some View {
        Text(role.uppercased())
            .font(.caption2.bold())
            .padding(.horizontal, 5).padding(.vertical, 1)
            .background(roleColor(role).opacity(0.18), in: Capsule())
            .foregroundStyle(roleColor(role))
    }

    private func roleColor(_ role: String) -> Color {
        switch role {
        case "required": return theme.accent
        case "anyOf":    return theme.warning
        default:         return theme.textMuted
        }
    }

    private func statusColor(_ s: String?) -> Color {
        switch s {
        case "online":   return theme.success
        case "degraded": return theme.warning
        case "offline":  return theme.danger
        default:         return theme.textMuted
        }
    }

    // MARK: - Actions

    private func reveal() async {
        error = nil
        inFlight = true
        defer { inFlight = false }
        switch await BiometricAuth.authenticate(reason: "Reveal API key value") {
        case .failure(let msg):
            error = msg
            return
        case .success:
            break
        }
        do {
            let v = scope == .workspace
                ? try await api.tenantKeyValue(name: workingRow.name)
                : try await api.apiKeyValue(name: workingRow.name)
            await MainActor.run {
                self.revealedValue = v.value ?? ""
                self.newValue = v.value ?? ""
            }
        } catch {
            await MainActor.run { self.error = error.localizedDescription }
        }
    }

    private func save() async {
        guard !newValue.isEmpty else { return }
        error = nil
        savedHint = false
        inFlight = true
        defer { inFlight = false }
        switch await BiometricAuth.authenticate(reason: "Save API key") {
        case .failure(let msg):
            error = msg
            return
        case .success:
            break
        }
        do {
            let updated = try await write(value: newValue)
            await MainActor.run {
                self.workingRow = updated
                self.revealedValue = newValue
                self.savedHint = true
                self.onUpdate(updated)
            }
        } catch {
            await MainActor.run { self.error = error.localizedDescription }
        }
    }

    private func clear() async {
        error = nil
        savedHint = false
        inFlight = true
        defer { inFlight = false }
        switch await BiometricAuth.authenticate(reason: "Clear API key") {
        case .failure(let msg):
            error = msg
            return
        case .success:
            break
        }
        do {
            let updated = try await write(value: "")
            await MainActor.run {
                self.workingRow = updated
                // Force a re-reveal — the server may now resolve to a
                // different (platform) value or none at all.
                self.revealedValue = nil
                self.newValue = ""
                self.savedHint = true
                self.onUpdate(updated)
            }
        } catch {
            await MainActor.run { self.error = error.localizedDescription }
        }
    }

    /// Routes the write to the right store and maps the response back into a
    /// `KeyRow`, preserving `role` (the tenant write response omits it).
    private func write(value: String) async throws -> KeyRow {
        switch scope {
        case .workspace:
            let w = try await api.tenantKeySet(name: workingRow.name, value: value)
            return KeyRow(name: w.name, role: workingRow.role,
                          set: w.set, byok: w.byok, source: w.source)
        case .platform:
            let m = try await api.apiKeySet(name: workingRow.name, value: value)
            return KeyRow(m)
        }
    }
}


/// One source waiting on a key: its name and status, a jump to its detail, and
/// — once its own `/api/status/:id` row has loaded — the probe controls for a
/// key-gated source. The row is fetched per visible source (at most a few
/// dozen), never as part of a registry dump.
private struct UsedByRow: View {
    let source: SourceSearchRow
    let keyName: String
    let open: () -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @State private var statusRow: StatusRow?

    var body: some View {
        VStack(spacing: 6) {
            Button(action: open) {
                HStack(spacing: 8) {
                    Circle().fill(dotColor)
                        .frame(width: 8, height: 8)
                    VStack(alignment: .leading, spacing: 1) {
                        Text(source.displayName)
                            .font(.caption)
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                        if let cat = source.category {
                            Text(cat)
                                .font(.caption2)
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                    Spacer()
                    Image(systemName: "chevron.right")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .accessibilityHidden(true)   // navigation affordance
                }
                .padding(.vertical, 4)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            if let row = statusRow, row.requiresKey == true {
                ProbeActionsView(row: row) { updated in
                    statusRow = updated
                }
            }
        }
        .task(id: source.id) {
            statusRow = try? await apiClient.api.statusRow(source.id)
        }
    }

    private var dotColor: Color {
        switch source.statusKind {
        case .online:   return theme.success
        case .degraded: return theme.warning
        case .offline:  return theme.danger
        default:        return theme.textMuted
        }
    }
}
