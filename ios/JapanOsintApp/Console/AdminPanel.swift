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
        .themedScreenBackground(theme)
    }

    private let operatorFooter =
        "Platform keys are the server-wide defaults every workspace falls back to."
    private let serverFooter =
        "Restart forces every collector to re-read API keys and other env vars; the dev server respawns automatically in 2–4 s."

    private var panel: some View {
        #if os(macOS)
        // macOS: lay the sections out as separator-free `surfaceElevated`
        // cards in a ScrollView. A grouped `List` on macOS draws hairline
        // rules between every row under the inset style, and hiding them on
        // the List itself isn't reliable — cards give the grouped look with
        // zero system separators.
        ScrollView {
            VStack(alignment: .leading, spacing: Space.lg) {
                card(header: "Operator tools", footer: operatorFooter) {
                    operatorRows
                }
                card(header: "Server", footer: serverFooter) {
                    serverRestartButton
                }
            }
            .padding(Space.lg)
            .frame(maxWidth: 680, alignment: .leading)
            .frame(maxWidth: .infinity, alignment: .top)
        }
        .themedScreenBackground(theme)
        .disabled(restarting)
        #else
        List {
            Section {
                operatorRows
            } header: {
                sectionLabel("Operator tools")
            } footer: {
                Text(operatorFooter).font(.caption2)
            }

            Section {
                serverRestartButton
            } header: {
                sectionLabel("Server")
            } footer: {
                Text(serverFooter).font(.caption2)
            }
        }
        .compatInsetGroupedListStyle()
        .themedScreenBackground(theme)
        .disabled(restarting)
        #endif
    }

    /// Operator-tool navigation rows, shared by the iOS `List` and macOS cards.
    @ViewBuilder
    private var operatorRows: some View {
        navRow(icon: "cylinder.split.1x2.fill",
               title: "Database",
               subtitle: "Browse collected tables") { DatabaseTab() }
        navRow(icon: "calendar.badge.clock",
               title: "Scheduler",
               subtitle: "Collection cadence") { SchedulerTab() }
        navRow(icon: "slider.horizontal.3",
               title: "Source scheduling",
               subtitle: "Per-source map-cron vs search-only") { SourceScheduleSettingsView() }
        navRow(icon: "scroll",
               title: "Follow log",
               subtitle: "Live activity stream") { FollowLogTab() }
        navRow(icon: "wrench.and.screwdriver.fill",
               title: "Repair worker",
               subtitle: "Self-healing pipeline & fixes") { RepairWorkerView() }
        navRow(icon: "key.horizontal.fill",
               title: "Platform API keys",
               subtitle: "Server-wide default credentials") {
            ApiKeysView(scope: .platform)
        }
    }

    #if os(macOS)
    /// A titled, separator-free card used by the macOS panel layout.
    @ViewBuilder
    private func card<Content: View>(
        header: String,
        footer: String,
        @ViewBuilder content: () -> Content
    ) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            sectionLabel(header)
            VStack(alignment: .leading, spacing: Space.xs) {
                content()
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(Space.sm)
            .background(theme.surfaceElevated,
                        in: RoundedRectangle(cornerRadius: Radius.md))
            Text(footer)
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
        }
    }
    #endif

    // MARK: - Server control

    /// Restarting the backend affects every workspace on the host. A
    /// confirmation dialog is enough friction for this destructive action.
    @ViewBuilder
    private var serverRestartButton: some View {
        restartTrigger
            // Dialog attached to the trigger, not the Section. Section-hosted
            // .confirmationDialog renders the body buttons but swallows the
            // `message:` text on iOS 17+; per-trigger hosting works.
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

    /// The restart Button itself. iOS fills the grouped row edge-to-edge; macOS
    /// renders an intrinsically-sized bordered button pinned to the leading edge
    /// instead of a full-window-wide tap target.
    @ViewBuilder
    private var restartTrigger: some View {
        let button = Button(role: .destructive) {
            confirmRestart = true
        } label: {
            HStack(spacing: Space.sm) {
                Image(systemName: "arrow.clockwise.circle.fill")
                Text(restarting ? "Restarting…" : "Restart server")
                #if os(iOS)
                Spacer()
                #endif
                if restarting { ProgressView().controlSize(.small) }
            }
        }
        .disabled(restarting)

        #if os(macOS)
        HStack {
            button.buttonStyle(.bordered)
            Spacer()
        }
        #else
        button
        #endif
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
        let link = NavigationLink {
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
                #if os(macOS)
                Spacer(minLength: 0)
                Image(systemName: "chevron.right")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                #endif
            }
            .padding(.vertical, 2)
        }

        #if os(macOS)
        // Outside a `List` the NavigationLink would render as blue link text;
        // `.plain` restores the row look. Full-width tap target + hover fill.
        return link
            .buttonStyle(.plain)
            .padding(.vertical, Space.xs)
            .padding(.horizontal, Space.sm)
            .frame(maxWidth: .infinity, alignment: .leading)
            .contentShape(Rectangle())
            .compatHoverHighlight(cornerRadius: Radius.sm)
        #else
        return link
        #endif
    }

    private func sectionLabel(_ text: String) -> some View {
        Text(text.uppercased())
            .font(Typography.display(10, weight: .semibold))
            .tracking(1.2)
            .foregroundStyle(theme.textMuted)
    }
}
