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
    @State private var statusRows: [StatusRow]
    @State private var inFlight: Bool = false
    @State private var error: String?
    @State private var savedHint: Bool = false

    init(row: KeyRow,
         scope: KeyScope,
         canManage: Bool,
         statusRows: [StatusRow] = [],
         onUpdate: @escaping (KeyRow) -> Void) {
        self.row = row
        self.scope = scope
        self.canManage = canManage
        self.onUpdate = onUpdate
        self._workingRow = State(initialValue: row)
        self._statusRows = State(initialValue: statusRows)
    }

    private var api: API { apiClient.api }

    /// Whether `workingRow` currently has a value set in *this* store
    /// (tenant key for workspace, overlay value for platform).
    private var hasOwnValue: Bool { workingRow.byok }

    private var sourcesUsingKey: [StatusRow] {
        statusRows.filter { r in
            r.envVars?.contains { $0.name == row.name } == true
        }
    }

    private func role(for r: StatusRow) -> String? {
        r.envVars?.first(where: { $0.name == row.name })?.role
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
                                UIPasteboard.general.string = revealed
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
                                .textInputAutocapitalization(.never)
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
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .presentationDetents([.medium, .large])
    }

    // MARK: - Used by

    @ViewBuilder
    private var usedBySection: some View {
        Section {
            if sourcesUsingKey.isEmpty {
                Text("No sources reference this key in the current registry.")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            } else {
                ForEach(sourcesUsingKey.groupedAsCollectors()) { collector in
                    VStack(alignment: .leading, spacing: 4) {
                        HStack(spacing: 6) {
                            Text(collector.name)
                                .font(.caption.bold())
                                .foregroundStyle(theme.text)
                            Text("\(collector.sourceCount) source\(collector.sourceCount == 1 ? "" : "s")")
                                .font(.caption2.monospacedDigit())
                                .foregroundStyle(theme.textMuted)
                            if let cat = collector.category {
                                Text("· \(cat)")
                                    .font(.caption2)
                                    .foregroundStyle(theme.textMuted)
                            }
                        }
                        VStack(spacing: 1) {
                            ForEach(collector.sources) { r in
                                usedByRow(r)
                            }
                        }
                        .clipShape(RoundedRectangle(cornerRadius: 6))
                    }
                }
            }
        } header: {
            Text("Used by")
        } footer: {
            if !sourcesUsingKey.isEmpty {
                Text("Tap a source to jump to its detail in the Sources tab.")
                    .font(.caption2)
            }
        }
    }

    private func usedByRow(_ r: StatusRow) -> some View {
        VStack(spacing: 6) {
            Button {
                let target = r.id
                dismiss()
                nav.showSource(target)
            } label: {
                HStack(spacing: 8) {
                    Circle().fill(statusColor(r.status))
                        .frame(width: 8, height: 8)
                    VStack(alignment: .leading, spacing: 1) {
                        Text(r.name ?? r.id)
                            .font(.caption)
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                        if let cat = r.category {
                            Text(cat)
                                .font(.caption2)
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                    Spacer()
                    if let rl = role(for: r) {
                        rolePill(rl)
                    }
                    Image(systemName: "chevron.right")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
                .padding(.vertical, 4)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            if r.requiresKey == true {
                ProbeActionsView(row: r) { updated in
                    applyStatusUpdate(updated)
                }
            }
        }
    }

    private func applyStatusUpdate(_ updated: StatusRow) {
        if let i = statusRows.firstIndex(where: { $0.id == updated.id }) {
            statusRows[i] = updated
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
