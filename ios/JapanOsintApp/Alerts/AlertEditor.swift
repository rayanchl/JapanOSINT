import SwiftUI

/// Create / edit form for an alert rule. Sheet-presented.
///
/// Fields:
///   - Name + enabled toggle (toggle hidden on Create until first save)
///   - Predicate: FTS query, comma-separated source-ids, comma-separated
///     tags (any-of)
///   - Channels: dynamic list of (type, target) rows. Webhook also takes
///     a 16-char-minimum signing secret.
///   - Throttle: dedup window seconds + storm cap per hour
///
/// Validation mirrors the server's `validateRule` — better to surface
/// errors before the round-trip.
struct AlertEditor: View {
    let onSave: (AlertRule) -> Void

    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var name: String
    @State private var enabled: Bool
    @State private var q: String
    @State private var sourcesCSV: String
    @State private var tagsCSV: String
    @State private var channels: [AlertChannel]
    @State private var dedupSec: Int
    @State private var stormCap: Int

    @State private var saving = false
    @State private var error: String?

    private let ruleId: String
    private let isCreate: Bool

    init(rule: AlertRule, onSave: @escaping (AlertRule) -> Void) {
        self.onSave = onSave
        self.ruleId = rule.id
        self.isCreate = rule.id.isEmpty
        _name = State(initialValue: rule.name)
        _enabled = State(initialValue: rule.enabled)
        _q = State(initialValue: rule.predicate.q ?? "")
        _sourcesCSV = State(initialValue: (rule.predicate.source_ids ?? []).joined(separator: ", "))
        _tagsCSV = State(initialValue: (rule.predicate.tags_any ?? []).joined(separator: ", "))
        _channels = State(initialValue: rule.channels)
        _dedupSec = State(initialValue: rule.dedup_window_sec)
        _stormCap = State(initialValue: rule.storm_cap_per_hour)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Rule") {
                    TextField("Name", text: $name)
                        .textInputAutocapitalization(.sentences)
                    if !isCreate {
                        Toggle("Enabled", isOn: $enabled)
                    }
                }

                Section {
                    TextField("FTS query (e.g. phishing AND tld:.jp)", text: $q)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .font(.system(.body, design: .monospaced))
                        .fontDesign(.monospaced)
                    TextField("Source IDs (comma-separated)", text: $sourcesCSV)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .font(.system(.body, design: .monospaced))
                        .fontDesign(.monospaced)
                    TextField("Tags any-of (comma-separated)", text: $tagsCSV)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                } header: {
                    Text("Match when…")
                } footer: {
                    Text("All filled fields combine with AND. Leave everything blank to match every new item (use a tight throttle if you do).")
                        .font(.caption2)
                }

                Section {
                    ForEach(Array(channels.enumerated()), id: \.offset) { idx, _ in
                        channelEditor(at: idx)
                    }
                    .onDelete { channels.remove(atOffsets: $0) }

                    Menu {
                        Button { channels.append(AlertChannel(type: .email, target: "", secret: nil)) }
                        label: { Label("Email", systemImage: "envelope") }
                        Button { channels.append(AlertChannel(type: .webhook, target: "", secret: "")) }
                        label: { Label("Webhook", systemImage: "link") }
                    } label: {
                        Label("Add channel", systemImage: "plus.circle")
                    }
                } header: {
                    Text("Deliver to")
                } footer: {
                    Text("Webhook receivers can verify each call's HMAC-SHA256 signature using the secret you paste below. Min 16 characters.")
                        .font(.caption2)
                }

                Section("Throttle") {
                    Stepper(value: $dedupSec, in: 0...86400, step: 300) {
                        HStack {
                            Text("Dedup window")
                            Spacer()
                            Text(formatSeconds(dedupSec))
                                .font(.caption.monospacedDigit())
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                    Stepper(value: $stormCap, in: 1...10000, step: 10) {
                        HStack {
                            Text("Max fires / hour")
                            Spacer()
                            Text("\(stormCap)")
                                .font(.caption.monospacedDigit())
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                }

                if let error {
                    Section {
                        Text(error).font(.caption).foregroundStyle(theme.danger)
                    }
                }
            }
            .navigationTitle(isCreate ? "New alert" : "Edit alert")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button(isCreate ? "Create" : "Save") {
                        Task { await save() }
                    }
                    .disabled(saving)
                }
            }
        }
        .presentationDetents([.large])
    }

    // MARK: - Channel row editor

    @ViewBuilder
    private func channelEditor(at idx: Int) -> some View {
        let ch = channels[idx]
        VStack(alignment: .leading, spacing: Space.sm - 2) {
            HStack(spacing: Space.sm) {
                Image(systemName: ch.type == .email ? "envelope.fill" : "link")
                    .foregroundStyle(theme.accent)
                Text(ch.type.label.uppercased())
                    .font(.caption2.bold())
                    .tracking(0.6)
                    .foregroundStyle(theme.accent)
                Spacer()
            }
            TextField(
                ch.type == .email ? "email@example.com" : "https://example.com/hook",
                text: Binding(
                    get: { channels[idx].target },
                    set: { channels[idx].target = $0 }
                )
            )
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .keyboardType(ch.type == .email ? .emailAddress : .URL)
            .font(.system(.body, design: .monospaced))
            .fontDesign(.monospaced)

            if ch.type == .webhook {
                SecureField("Signing secret (≥16 chars)", text: Binding(
                    get: { channels[idx].secret ?? "" },
                    set: { channels[idx].secret = $0 }
                ))
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .font(.system(.body, design: .monospaced))
                .fontDesign(.monospaced)
            }
        }
        .padding(.vertical, 2)
    }

    // MARK: - Save

    private func save() async {
        saving = true
        defer { saving = false }

        let trimmedName = name.trimmingCharacters(in: .whitespaces)
        if trimmedName.isEmpty { error = "Name is required"; return }
        if channels.isEmpty { error = "Add at least one channel"; return }
        for (i, ch) in channels.enumerated() {
            if ch.target.trimmingCharacters(in: .whitespaces).isEmpty {
                error = "Channel \(i + 1) is missing a target"; return
            }
            if ch.type == .webhook {
                let secret = (ch.secret ?? "").trimmingCharacters(in: .whitespaces)
                // Server preserves "••••" placeholder when updating without
                // re-entering the secret. Accept that as "unchanged".
                if secret.count < 16 && secret != "••••" {
                    error = "Webhook secret must be at least 16 characters"; return
                }
            }
        }

        var predicate = AlertPredicate()
        let trimQ = q.trimmingCharacters(in: .whitespaces)
        if !trimQ.isEmpty { predicate.q = trimQ }
        let srcs = splitCSV(sourcesCSV)
        if !srcs.isEmpty { predicate.source_ids = srcs }
        let tags = splitCSV(tagsCSV)
        if !tags.isEmpty { predicate.tags_any = tags }

        let rule = AlertRule(
            id: ruleId, name: trimmedName, enabled: enabled,
            predicate: predicate,
            channels: channels,
            dedup_window_sec: dedupSec,
            storm_cap_per_hour: stormCap,
            muted_until: nil,
            created_at: nil, updated_at: nil
        )

        do {
            let api = API(baseURL: settings.backendBaseURL)
            let saved = isCreate ? try await api.alertCreate(rule) : try await api.alertUpdate(rule)
            onSave(saved)
            Haptics.success()
            dismiss()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func splitCSV(_ s: String) -> [String] {
        s.split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    private func formatSeconds(_ s: Int) -> String {
        if s == 0 { return "off" }
        if s < 60 { return "\(s)s" }
        if s < 3600 { return "\(s / 60)m" }
        return "\(s / 3600)h"
    }
}
