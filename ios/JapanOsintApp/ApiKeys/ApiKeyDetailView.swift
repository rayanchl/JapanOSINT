import SwiftUI

/// Detail sheet for one API key. Reveal value (Face ID), edit + save (Face
/// ID), or clear the overlay (Face ID). The list view is updated via the
/// `onUpdate` closure so callers don't need to re-fetch the whole list after
/// every mutation.
struct ApiKeyDetailView: View {
    let meta: ApiKeyMeta
    let onUpdate: (ApiKeyMeta) -> Void

    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var revealedValue: String?
    @State private var newValue: String = ""
    @State private var workingMeta: ApiKeyMeta
    /// Local copy of `/api/status` rows (snapshot from parent) so we can
    /// reflect inline probe / consent changes from `ProbeActionsView`
    /// without round-tripping through the parent. Empty when the parent's
    /// status fetch failed — the "Used by" section then renders its empty
    /// state.
    @State private var statusRows: [StatusRow]
    @State private var inFlight: Bool = false
    @State private var error: String?
    @State private var savedHint: Bool = false

    init(meta: ApiKeyMeta,
         statusRows: [StatusRow] = [],
         onUpdate: @escaping (ApiKeyMeta) -> Void) {
        self.meta = meta
        self.onUpdate = onUpdate
        self._workingMeta = State(initialValue: meta)
        self._statusRows = State(initialValue: statusRows)
    }

    private var sourcesUsingKey: [StatusRow] {
        statusRows.filter { row in
            row.envVars?.contains { $0.name == meta.name } == true
        }
    }

    private func role(for row: StatusRow) -> String? {
        row.envVars?.first(where: { $0.name == meta.name })?.role
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    LabeledContent("Name") {
                        Text(workingMeta.name)
                            .font(.system(.body, design: .monospaced))
                    }
                    LabeledContent("Role") {
                        Text(workingMeta.role.capitalized)
                            .foregroundStyle(theme.textMuted)
                    }
                    LabeledContent("Status") {
                        Text(workingMeta.set ? "Set" : "Unset")
                            .foregroundStyle(workingMeta.set ? theme.success : theme.warning)
                    }
                    LabeledContent("Overlay") {
                        Text(workingMeta.hasOverlay ? "Yes" : "No")
                            .foregroundStyle(workingMeta.hasOverlay ? theme.accent : theme.textMuted)
                    }
                } header: {
                    Text("Key")
                }

                Section {
                    if let revealed = revealedValue {
                        if revealed.isEmpty {
                            Text("(no value set)")
                                .foregroundStyle(theme.textMuted)
                                .italic()
                        } else {
                            // Use a TextField for the revealed value so the
                            // user can scroll/select/copy. Disabled because
                            // edits go through the SecureField below.
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
                    Text("Reveal requires Face ID (or device passcode).")
                        .font(.caption2)
                }

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
                        // Inline trailing button — `.borderless` keeps the
                        // button's tap target tight to the icon so the
                        // surrounding row doesn't swallow taps meant for the
                        // text field.
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
                    Text("Save requires Face ID. Empty values aren't accepted here — use 'Clear override' below to drop back to the .env value.")
                        .font(.caption2)
                }

                if workingMeta.hasOverlay {
                    Section {
                        Button(role: .destructive) {
                            Task { await clear() }
                        } label: {
                            Label(inFlight ? "Clearing…" : "Clear override",
                                  systemImage: "xmark.circle")
                        }
                        .disabled(inFlight)
                    } footer: {
                        Text("Removes the value the iOS app stored on the server. The server falls back to the .env-baked value if any.")
                            .font(.caption2)
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
            .navigationTitle(meta.name)
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
                            ForEach(collector.sources) { row in
                                usedByRow(row)
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

    private func usedByRow(_ row: StatusRow) -> some View {
        VStack(spacing: 6) {
            Button {
                let target = row.id
                dismiss()
                nav.showSource(target)
            } label: {
                HStack(spacing: 8) {
                    Circle().fill(statusColor(row.status))
                        .frame(width: 8, height: 8)
                    VStack(alignment: .leading, spacing: 1) {
                        Text(row.name ?? row.id)
                            .font(.caption)
                            .foregroundStyle(theme.text)
                            .lineLimit(1)
                        if let cat = row.category {
                            Text(cat)
                                .font(.caption2)
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                    Spacer()
                    if let r = role(for: row) {
                        rolePill(r)
                    }
                    Image(systemName: "chevron.right")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
                .padding(.vertical, 4)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            if row.requiresKey == true {
                ProbeActionsView(row: row) { updated in
                    applyStatusUpdate(updated)
                }
            }
        }
    }

    /// Replace the matching row in the local `statusRows` snapshot so the
    /// "Used by" section reflects probe / consent changes immediately
    /// without re-fetching `/api/status`.
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
            let v = try await API(baseURL: settings.backendBaseURL)
                .apiKeyValue(name: workingMeta.name)
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
            let updated = try await API(baseURL: settings.backendBaseURL)
                .apiKeySet(name: workingMeta.name, value: newValue)
            await MainActor.run {
                self.workingMeta = updated
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
        switch await BiometricAuth.authenticate(reason: "Clear API key override") {
        case .failure(let msg):
            error = msg
            return
        case .success:
            break
        }
        do {
            let updated = try await API(baseURL: settings.backendBaseURL)
                .apiKeySet(name: workingMeta.name, value: "")
            await MainActor.run {
                self.workingMeta = updated
                // After clearing, force a re-reveal — the server may have
                // restored a different (.env) value or none at all.
                self.revealedValue = nil
                self.newValue = ""
                self.savedHint = true
                self.onUpdate(updated)
            }
        } catch {
            await MainActor.run { self.error = error.localizedDescription }
        }
    }
}
