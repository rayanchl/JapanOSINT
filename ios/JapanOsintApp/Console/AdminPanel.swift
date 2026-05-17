import SwiftUI

/// Single home for every platform-operator-only surface. These all hit
/// server endpoints gated by `requirePlatformOperator`
/// (server/src/middleware/keyAccess.js) and affect every workspace on the
/// host, so the whole panel is gated to the super-admin account via
/// `AuthSession.isPlatformAdmin`. The Console row that pushes this is gated
/// the same way; the inner `isPlatformAdmin` check is belt-and-suspenders.
///
/// Pushed inside Console's `NavigationStack(path:)` (phone) or RootView's
/// detail stack (iPad), so it carries no `NavigationStack` of its own and the
/// destinations use plain value-less `NavigationLink`s — the same pattern the
/// old `SettingsTab.adminSection` used for Platform API keys.
struct AdminPanel: View {
    @EnvironmentObject var auth: AuthSession
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    /// Server-restart UI state. Restarting forces every collector to re-read
    /// API keys + other env vars after editing them in the API Keys tab.
    @State private var restarting: Bool = false
    @State private var restartError: String?
    @State private var confirmRestart: Bool = false

    var body: some View {
        Group {
            if auth.isPlatformAdmin {
                panel
            } else {
                ContentUnavailableView(
                    "Operator access only",
                    systemImage: "lock.shield",
                    description: Text("This panel is restricted to the platform operator account.")
                )
            }
        }
        .navigationTitle("Admin")
        .background(theme.surface.ignoresSafeArea())
    }

    private var panel: some View {
        List {
            Section {
                navRow(icon: "cylinder.split.1x2.fill",
                       title: "Database",
                       subtitle: "Browse collected tables") { DatabaseTab() }
                navRow(icon: "calendar.badge.clock",
                       title: "Scheduler",
                       subtitle: "Collection cadence") { SchedulerTab() }
                navRow(icon: "scroll",
                       title: "Follow log",
                       subtitle: "Live activity stream") { FollowLogTab() }
                navRow(icon: "key.horizontal.fill",
                       title: "Platform API keys",
                       subtitle: "Server-wide default credentials") {
                    ApiKeysView(scope: .platform)
                }
            } header: {
                sectionLabel("Operator tools")
            } footer: {
                Text("Platform keys are the server-wide defaults every workspace falls back to.")
                    .font(.caption2)
            }

            Section {
                serverRestartButton
            } header: {
                sectionLabel("Server")
            } footer: {
                Text("Restart forces every collector to re-read API keys and other env vars; the dev server respawns automatically in 2–4 s.")
                    .font(.caption2)
            }
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .background(theme.surface.ignoresSafeArea())
        .disabled(restarting)
    }

    // MARK: - Server control

    /// Restarting the backend affects every workspace on the host. A
    /// confirmation dialog is enough friction for this destructive action.
    @ViewBuilder
    private var serverRestartButton: some View {
        Button(role: .destructive) {
            confirmRestart = true
        } label: {
            HStack {
                Image(systemName: "arrow.clockwise.circle.fill")
                Text(restarting ? "Restarting…" : "Restart server")
                Spacer()
                if restarting { ProgressView().controlSize(.small) }
            }
        }
        .disabled(restarting)
        // Dialog attached to the trigger Button, not the Section. Section-
        // hosted .confirmationDialog renders the body buttons but swallows
        // the `message:` text on iOS 17+; per-button hosting works.
        .confirmationDialog("Restart server?",
                            isPresented: $confirmRestart,
                            titleVisibility: .visible) {
            Button("Restart", role: .destructive) {
                Haptics.tap(.medium)
                Task { await doRestart() }
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("All in-flight requests will be dropped. The app reconnects automatically once the server is back.")
        }
        if let restartError {
            Text(restartError)
                .font(.caption)
                .foregroundStyle(theme.warning)
        }
    }

    private func doRestart() async {
        restartError = nil
        restarting = true
        defer { restarting = false }
        do {
            try await apiClient.api.restartServer()
        } catch {
            // Connection-reset is expected — the server kills the socket as
            // it tears down. Treat any post-POST failure as "probably
            // restarting" and fall through to the health poll for liveness.
        }
        // Poll /api/health up to 30 s (1 s cadence) until the server is back.
        let deadline = Date().addingTimeInterval(30)
        while Date() < deadline {
            try? await Task.sleep(for: .seconds(1))
            if (try? await apiClient.api.health()) != nil {
                Haptics.success()
                return
            }
        }
        restartError = "Server didn't come back within 30 s. Check the host."
        Haptics.error()
    }

    // MARK: - Row styling (mirrors ConsoleHub.row)

    private func navRow<Destination: View>(
        icon: String,
        title: String,
        subtitle: String,
        @ViewBuilder destination: @escaping () -> Destination
    ) -> some View {
        NavigationLink {
            destination()
        } label: {
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
