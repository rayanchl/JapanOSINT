import SwiftUI

/// Owner-only editor for who, besides owner/admin (who can always edit), may
/// set/clear this workspace's BYOK API keys. Backed by
/// `GET/PUT /api/tenant-keys/policy`. Admin & owner always retain edit
/// rights regardless of the choice here.
struct KeyPolicySettingsView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var policy: String = "owner_only"
    @State private var memberId: String?
    @State private var members: [KeyPolicyMember] = []
    @State private var loading = true
    @State private var saving = false
    @State private var error: String?
    @State private var savedHint = false

    private static let choices: [(value: String, label: String, help: String)] = [
        ("owner_only", "Owner only",
         "Only you (owner) and workspace admins can edit keys."),
        ("selected_member", "A chosen member",
         "One delegate you pick, plus owner/admins, can edit keys."),
        ("all_members", "All members",
         "Anyone in the workspace can edit this workspace's keys."),
    ]

    private var api: API { apiClient.api }

    var body: some View {
        Group {
            if loading {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                form
            }
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("Key-edit policy")
        .navigationBarTitleDisplayMode(.inline)
        .task { await load() }
    }

    private var form: some View {
        Form {
            Section {
                Picker("Who can edit keys", selection: $policy) {
                    ForEach(Self.choices, id: \.value) { c in
                        Text(c.label).tag(c.value)
                    }
                }
                .pickerStyle(.inline)
                .labelsHidden()
            } header: {
                Text("Policy")
            } footer: {
                Text(Self.choices.first { $0.value == policy }?.help ?? "")
                    .font(.caption2)
            }

            if policy == "selected_member" {
                Section {
                    if members.isEmpty {
                        Text("No other members in this workspace yet.")
                            .font(.caption)
                            .foregroundStyle(theme.textMuted)
                    } else {
                        Picker("Delegate", selection: Binding(
                            get: { memberId ?? "" },
                            set: { memberId = $0.isEmpty ? nil : $0 })
                        ) {
                            Text("None").tag("")
                            ForEach(members) { m in
                                Text("\(m.email) · \(m.role)").tag(m.id)
                            }
                        }
                    }
                } header: {
                    Text("Delegate")
                }
            }

            Section {
                Button {
                    Task { await save() }
                } label: {
                    HStack {
                        if saving { ProgressView().controlSize(.small) }
                        Text(saving ? "Saving…" : "Save policy")
                        Spacer()
                    }
                }
                .disabled(saving || (policy == "selected_member" && memberId == nil))
                if savedHint {
                    Label("Saved.", systemImage: "checkmark.circle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.success)
                }
                if let error {
                    Text(error)
                        .font(.caption)
                        .foregroundStyle(theme.warning)
                }
            } footer: {
                Text("Owner and admins can always edit keys regardless of this setting.")
                    .font(.caption2)
            }
        }
    }

    private func load() async {
        loading = true
        defer { loading = false }
        do {
            let r = try await api.keyPolicy()
            await MainActor.run {
                self.policy = r.policy
                self.memberId = r.memberId
                self.members = r.members
                self.error = nil
            }
        } catch {
            await MainActor.run { self.error = error.localizedDescription }
        }
    }

    private func save() async {
        error = nil
        savedHint = false
        saving = true
        defer { saving = false }
        do {
            let r = try await api.keyPolicySet(
                policy: policy,
                memberId: policy == "selected_member" ? memberId : nil)
            await MainActor.run {
                self.policy = r.policy
                self.memberId = r.memberId
                self.savedHint = true
            }
        } catch {
            await MainActor.run { self.error = error.localizedDescription }
        }
    }
}
