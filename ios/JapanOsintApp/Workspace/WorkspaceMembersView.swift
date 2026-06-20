import SwiftUI

/// Members & permissions management for a workspace. Owner/admin only —
/// reached from the Workspace console destination. Backed by
/// `GET/POST/PATCH/DELETE /api/members*`:
///   • invite a user by email (added immediately if they already exist,
///     otherwise queued as a pending invite claimed on first sign-in),
///   • change a member's role,
///   • remove a member or revoke a pending invite.
struct WorkspaceMembersView: View {
    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var auth: AuthSession
    @Environment(\.theme) private var theme

    @State private var members: [WorkspaceMember] = []
    @State private var invites: [WorkspaceInvite] = []
    @State private var loading = true
    @State private var working = false
    @State private var error: String?
    @State private var inviteEmail = ""
    @State private var inviteRole: WorkspaceRole = .analyst
    @State private var lastStatus: String?

    private var api: API { apiClient.api }
    private var myUserId: String? { auth.me?.user?.id }

    var body: some View {
        Group {
            if loading && members.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                form
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Members")
        .compatInlineTitle()
        .task { await load() }
        .refreshable { await load() }
    }

    private var form: some View {
        Form {
            Section {
                TextField("name@example.com", text: $inviteEmail)
                    .compatNoAutocap()
                    .autocorrectionDisabled()
                    .compatKeyboard(email: true)
                    .font(.system(.body, design: .monospaced))
                Picker("Role", selection: $inviteRole) {
                    ForEach(WorkspaceRole.allCases) { r in Text(r.label).tag(r) }
                }
                Button {
                    Task { await invite() }
                } label: {
                    HStack {
                        if working { ProgressView().controlSize(.small) }
                        Text(working ? "Inviting…" : "Invite")
                        Spacer()
                    }
                }
                .disabled(working || !inviteEmail.contains("@"))
                if let lastStatus {
                    Label(statusBlurb(lastStatus), systemImage: "checkmark.circle.fill")
                        .font(.caption)
                        .foregroundStyle(theme.success)
                }
            } header: {
                Text("Invite by email")
            } footer: {
                Text("If the address already has an account, they join immediately. Otherwise the invite is held and applied the first time they sign in.")
                    .font(.caption2)
            }

            Section {
                if members.isEmpty {
                    Text("No members yet.").font(.caption).foregroundStyle(theme.textMuted)
                } else {
                    ForEach(members) { m in memberRow(m) }
                }
            } header: {
                Text("Members (\(members.count))")
            }

            if !invites.isEmpty {
                Section {
                    ForEach(invites) { inv in
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(inv.email)
                                    .font(.system(.body, design: .monospaced))
                                    .lineLimit(1)
                                Text("invited as \(inv.role)")
                                    .font(.caption2).foregroundStyle(theme.textMuted)
                            }
                            Spacer()
                            Button(role: .destructive) {
                                Task { await revoke(inv) }
                            } label: { Image(systemName: "xmark.circle") }
                            .buttonStyle(.borderless)
                        }
                    }
                } header: {
                    Text("Pending invites (\(invites.count))")
                }
            }

            if let error {
                Section { Text(error).font(.caption).foregroundStyle(theme.danger) }
            }
        }
        .compatGroupedForm()
    }

    @ViewBuilder
    private func memberRow(_ m: WorkspaceMember) -> some View {
        HStack(spacing: Space.md) {
            VStack(alignment: .leading, spacing: 2) {
                Text(m.email)
                    .font(.system(.body, design: .monospaced))
                    .lineLimit(1)
                if m.user_id == myUserId {
                    Text("you").font(.caption2).foregroundStyle(theme.textMuted)
                }
            }
            Spacer()
            Picker("", selection: Binding(
                get: { m.role },
                set: { newRole in Task { await setRole(m, newRole) } }
            )) {
                ForEach(WorkspaceRole.allCases) { r in Text(r.label).tag(r.rawValue) }
            }
            .labelsHidden()
            .disabled(working)
        }
        .swipeActions(edge: .trailing) {
            Button(role: .destructive) { Task { await remove(m) } } label: {
                Label("Remove", systemImage: "trash")
            }
        }
    }

    private func statusBlurb(_ s: String) -> String {
        switch s {
        case "added":          return "Added to workspace."
        case "already_member": return "Already a member."
        case "invited":        return "Invite sent — applied on first sign-in."
        default:               return "Done."
        }
    }

    // MARK: - Actions

    private func load() async {
        loading = true
        defer { loading = false }
        do {
            let r = try await api.membersList()
            members = r.members
            invites = r.invites
            error = nil
        } catch let e {
            error = e.localizedDescription
        }
    }

    private func invite() async {
        let em = inviteEmail.trimmingCharacters(in: .whitespacesAndNewlines)
        guard em.contains("@") else { error = "Enter a valid email"; return }
        working = true
        defer { working = false }
        do {
            lastStatus = try await api.memberInvite(email: em, role: inviteRole.rawValue)
            inviteEmail = ""
            error = nil
            Haptics.success()
            await load()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func setRole(_ m: WorkspaceMember, _ role: String) async {
        guard role != m.role else { return }
        working = true
        defer { working = false }
        do {
            try await api.memberSetRole(userId: m.user_id, role: role)
            Haptics.selection()
            await load()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func remove(_ m: WorkspaceMember) async {
        do {
            try await api.memberRemove(userId: m.user_id)
            Haptics.warning()
            await load()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func revoke(_ inv: WorkspaceInvite) async {
        do {
            try await api.inviteRevoke(id: inv.id)
            await load()
        } catch let e {
            error = e.localizedDescription
        }
    }
}
