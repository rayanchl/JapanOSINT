import SwiftUI

/// The fourth tab. Hosts every admin / operations surface that used to be a
/// separate top-level tab. iOS's TabView truncates at 5 tabs into a generic
/// "More" list — collapsing here keeps each destination styled and reachable.
///
/// Provides the surrounding `NavigationStack` so each destination
/// (`SourceDashboardTab`, `DatabaseTab`, etc.) can drop its own NavigationStack
/// wrapper and avoid nested nav chrome.
struct ConsoleHub: View {
    @EnvironmentObject var mapNav: MapNavigation
    @EnvironmentObject var auth: AuthSession
    @Environment(\.theme) private var theme

    /// The Workspace destination hosts owner/admin surfaces (members,
    /// permissions, queries), so hide the row entirely for everyone else
    /// rather than show an empty screen.
    private var canManageWorkspace: Bool {
        let role = auth.me?.tenant?.role
        return role == "owner" || role == "admin"
    }

    /// Disconnect confirmation — mirrors the gate in Settings › Account.
    @State private var confirmDisconnect = false

    var body: some View {
        NavigationStack(path: $mapNav.consolePath) {
            List {
                Section {
                    row(.sources,   icon: "chart.pie.fill",
                        title: "Sources",
                        subtitle: "Status, charts, collectors")
                } header: {
                    sectionLabel("Operations")
                }

                Section {
                    row(.cameras, icon: "video.fill",
                        title: "Camera discovery",
                        subtitle: "Probe public webcams")
                    row(.alerts, icon: "bell.badge.fill",
                        title: "Alerts",
                        subtitle: "Rules, channels, history")
                } header: {
                    sectionLabel("Discovery")
                }

                Section {
                    row(.apiKeys,  icon: "key.fill",
                        title: "API keys",
                        subtitle: "Credentials & overlays")
                    if canManageWorkspace {
                        row(.workspace, icon: "person.2.badge.key.fill",
                            title: "Workspace",
                            subtitle: "Members, permissions, queries")
                    }
                    row(.settings, icon: "gearshape.fill",
                        title: "Settings",
                        subtitle: "Backend, appearance, limits")
                } header: {
                    sectionLabel("Configuration")
                }

                if auth.isPlatformAdmin {
                    Section {
                        row(.admin, icon: "lock.shield.fill",
                            title: "Admin panel",
                            subtitle: "Database, scheduler, logs, server")
                    } header: {
                        sectionLabel("Admin")
                    }
                }

                accountSection
            }
            .compatInsetGroupedListStyle()
            .themedScreenBackground(theme)
            .navigationTitle("Console")
            .navigationDestination(for: ConsoleDestination.self) { dest in
                destinationView(for: dest)
            }
        }
    }

    @ViewBuilder
    private func destinationView(for dest: ConsoleDestination) -> some View {
        switch dest {
        case .sources:    SourceDashboardTab()
        case .database:   DatabaseTab()
        case .scheduler:  SchedulerTab()
        case .cameras:    CameraDiscoveryTab()
        case .followLog:  FollowLogTab()
        case .apiKeys:    ApiKeysTab()
        case .alerts:     AlertsTab()
        case .settings:   SettingsTab()
        case .workspace:  WorkspaceSettingsTab()
        case .admin:      AdminPanel()
        }
    }

    /// Copied from Settings › Account so the signed-in identity and the
    /// disconnect escape hatch are reachable from the Console foot without a
    /// trip into Settings. `auth.signOut()` wipes Keychain tokens and flips the
    /// gate back to onboarding.
    @ViewBuilder
    private var accountSection: some View {
        Section {
            if let email = auth.accountEmail {
                LabeledContent("Signed in as") {
                    Text(email)
                        .font(.caption.monospaced())
                        .foregroundStyle(theme.textMuted)
                        .textSelection(.enabled)
                }
            }
            if let tenant = auth.tenantName {
                LabeledContent("Workspace") {
                    Text(tenant)
                        .font(.caption.monospaced())
                        .foregroundStyle(theme.textMuted)
                        .textSelection(.enabled)
                }
            }
            Button(role: .destructive) {
                confirmDisconnect = true
            } label: {
                HStack {
                    Image(systemName: "rectangle.portrait.and.arrow.right")
                    Text("Disconnect")
                    Spacer()
                }
            }
            .confirmationDialog("Disconnect?",
                                isPresented: $confirmDisconnect,
                                titleVisibility: .visible) {
                Button("Disconnect", role: .destructive) {
                    Haptics.tap(.medium)
                    auth.signOut()
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("Signs you out and returns to the connect & sign-in screen. Saved items and cached layers on this device are kept.")
            }
        } header: {
            sectionLabel("Account")
        } footer: {
            Text("Use this to switch backend, workspace, or account without reinstalling the app.")
                .font(.caption2)
        }
    }

    private func row(_ dest: ConsoleDestination,
                     icon: String,
                     title: String,
                     subtitle: String) -> some View {
        NavigationLink(value: dest) {
            HStack(spacing: Space.md) {
                Image(systemName: icon)
                    .font(.body)
                    .foregroundStyle(theme.accent)
                    .frame(width: 28, height: 28)
                    .background(
                        theme.accent.opacity(0.12),
                        in: RoundedRectangle(cornerRadius: Radius.sm)
                    )
                VStack(alignment: .leading, spacing: 1) {
                    Text(title)
                        .font(.body.weight(.medium))
                        .foregroundStyle(theme.text)
                    Text(subtitle)
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
            .padding(.vertical, 2)
        }
    }

    private func sectionLabel(_ text: String) -> some View {
        Text(text.uppercased())
            .font(Typography.display(10, weight: .semibold))
            .tracking(1.2)
            .foregroundStyle(theme.textMuted)
    }
}
