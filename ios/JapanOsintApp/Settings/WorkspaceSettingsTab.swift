import SwiftUI

/// Workspace-scoped configuration, surfaced as its own Console destination
/// alongside "API keys" and "Settings". Mounted inside Console's
/// NavigationStack (RootView/ConsoleHub) — Console owns the surrounding stack
/// so destinations don't nest nav chrome.
///
/// Currently hosts the owner-only key-edit policy (moved out of Settings ›
/// Admin); kept as a Form section so further workspace settings can slot in
/// without another top-level destination.
struct WorkspaceSettingsTab: View {
    @EnvironmentObject var auth: AuthSession
    @Environment(\.theme) private var theme

    private var role: String? { auth.me?.tenant?.role }
    private var isOwner: Bool { role == "owner" }
    /// Owner and admins manage members, permissions, and workspace queries.
    private var canManage: Bool { role == "owner" || role == "admin" }

    var body: some View {
        Form {
            if canManage {
                Section {
                    NavigationLink {
                        WorkspaceMembersView()
                    } label: {
                        Label("Members & permissions", systemImage: "person.2.fill")
                    }
                    NavigationLink {
                        WorkspaceQueryView()
                    } label: {
                        Label("Queries", systemImage: "magnifyingglass.circle.fill")
                    }
                } header: {
                    Text("Workspace")
                } footer: {
                    Text("Invite users by email and manage their roles, or build FTS / LLM-pipeline queries and save them as alerts.")
                        .font(.caption2)
                }
            }

            if isOwner {
                Section {
                    NavigationLink {
                        KeyPolicySettingsView()
                    } label: {
                        Label("Key-edit policy", systemImage: "person.2.badge.key.fill")
                    }
                } header: {
                    Text("Credentials")
                } footer: {
                    Text("Choose who, besides owner and admins, may edit this workspace's API keys.")
                        .font(.caption2)
                }
            }

            if !canManage {
                Section {
                    Text("Workspace settings are managed by the workspace owner or an admin.")
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                }
            }
        }
        .compatGroupedForm()
        .themedScreenBackground(theme)
        .navigationTitle("Workspace")
        .compatInlineTitle()
    }
}
