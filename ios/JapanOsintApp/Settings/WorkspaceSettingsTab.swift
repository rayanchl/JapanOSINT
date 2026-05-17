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

    var body: some View {
        Form {
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
            } else {
                Section {
                    Text("Workspace settings are managed by the workspace owner.")
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                }
            }

            Section {
                NavigationLink {
                    SourceScheduleSettingsView()
                } label: {
                    Label("Source scheduling", systemImage: "calendar.badge.clock")
                }
            } header: {
                Text("Data collection")
            } footer: {
                Text("Pick, per source, whether it's cron-collected for the map or only grabbed live from Search. Both still feed Intel everywhere.")
                    .font(.caption2)
            }
        }
        .scrollContentBackground(.hidden)
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("Workspace")
        .navigationBarTitleDisplayMode(.inline)
    }
}
