import SwiftUI

/// Top-level shell.
///
/// Phone (compact horizontal size class) → 4-tab `TabView`:
///   Map · Intel · Saved · Console
/// Everything that used to be a separate tab (Sources, Database, Scheduler,
/// Cameras, Follow log, API Keys, Settings) lives inside Console — see
/// `ConsoleHub`. iOS truncates a phone TabView at 5 tabs into a generic
/// "More" drawer, so anything past tab 4 used to lose all styling.
///
/// iPad (regular horizontal size class) → `NavigationSplitView` with a single
/// sidebar that exposes every workspace + every Console destination, so
/// admin surfaces are reachable in one tap instead of two.
struct RootView: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var auth: AuthSession
    @EnvironmentObject var mapNav: MapNavigation
    @Environment(\.theme) private var theme
    @Environment(\.horizontalSizeClass) private var hSize

    /// Owner/admins reach the Workspace console surface (members, queries).
    private var canManageWorkspace: Bool {
        let role = auth.me?.tenant?.role
        return role == "owner" || role == "admin"
    }

    var body: some View {
        Group {
            switch auth.gate {
            case .loading:
                LoadingGate()
                    .transition(.opacity)
            case .onboarding:
                OnboardingFlow()
                    .transition(.opacity)
            case .ready:
                shell
                    .transition(.opacity)
            }
        }
        #if os(macOS)
        // Pin a usable minimum window size across every gate (loading,
        // onboarding, ready) so the pre-auth window isn't tiny either. The
        // scene sets the launch/default size.
        .frame(minWidth: 1040, minHeight: 640)
        #endif
        // Crossfade between gates so onboarding fades out cleanly into the
        // app instead of hard-cutting.
        .animation(.easeInOut(duration: 0.45), value: auth.gate)
        // Guarantee a fresh post-onboarding session always lands on the Map
        // (covers both the phone TabView and the iPad split view).
        .onChange(of: auth.gate) { _, gate in
            if gate == .ready { mapNav.selectedTab = AppTab.map.rawValue }
        }
    }

    @ViewBuilder
    private var shell: some View {
        Group {
            #if os(macOS)
            // macOS is always a sidebar app — the bottom TabView is an
            // iPhone idiom. (The window minimum is pinned on the RootView
            // body so it applies across every gate.)
            splitView
            #else
            if hSize == .regular {
                splitView
            } else {
                tabView
            }
            #endif
        }
        .sensoryFeedback(.selection, trigger: mapNav.selectedTab)
    }

    // MARK: - Phone (compact)

    private var tabView: some View {
        TabView(selection: $mapNav.selectedTab) {
            MapTab()
                .tabItem { Label("Map", systemImage: "map.fill") }
                .tag(AppTab.map.rawValue)

            IntelTab()
                .tabItem { Label("Intel", systemImage: "newspaper.fill") }
                .tag(AppTab.intel.rawValue)

            SearchTab()
                .tabItem { Label("Search", systemImage: "magnifyingglass") }
                .tag(AppTab.search.rawValue)

            SavedTab()
                .tabItem { Label("Saved", systemImage: "star.fill") }
                .tag(AppTab.saved.rawValue)

            ConsoleHub()
                .tabItem { Label("Console", systemImage: "slider.horizontal.3") }
                .tag(AppTab.console.rawValue)
        }
        .background(theme.surface)
    }

    // MARK: - iPad (regular)

    /// Sidebar items. Map / Intel / Saved are top-level workspaces; every
    /// Console destination is also a sidebar row so iPad gets one-tap access
    /// to admin surfaces instead of "Console → row".
    enum SidebarItem: Hashable {
        case workspace(AppTab)
        case console(ConsoleDestination)
        case entities          // entity browser (iPad sidebar; phone reaches
                               // it via push from entity chips)
    }

    @State private var sidebar: SidebarItem? = .workspace(.map)

    private var splitView: some View {
        NavigationSplitView {
            sidebarList
                #if os(macOS)
                .navigationSplitViewColumnWidth(min: 220, ideal: 240, max: 300)
                #endif
        } detail: {
            detailView
        }
        #if os(macOS)
        .navigationSplitViewStyle(.balanced)
        #endif
        // Mirror cross-tab deep links into the sidebar selection on iPad.
        // Phone gets this for free via `selectedTab` driving the TabView.
        .onChange(of: mapNav.selectedTab) { _, raw in
            guard let tab = AppTab(rawValue: raw) else { return }
            switch tab {
            case .map, .intel, .saved, .search: sidebar = .workspace(tab)
            case .console:
                if let dest = mapNav.consolePath.first {
                    sidebar = .console(dest)
                }
            }
        }
        .onChange(of: mapNav.consolePath) { _, newPath in
            if let dest = newPath.first {
                sidebar = .console(dest)
            }
        }
    }

    private var sidebarList: some View {
        List(selection: $sidebar) {
                Section {
                    sidebarLabel(.workspace(.map),    icon: "map.fill",          title: "Map")
                    sidebarLabel(.workspace(.intel),  icon: "newspaper.fill",    title: "Intel")
                    sidebarLabel(.workspace(.search), icon: "magnifyingglass",   title: "Search")
                    sidebarLabel(.entities,           icon: "point.3.connected.trianglepath.dotted", title: "Entities")
                    sidebarLabel(.workspace(.saved),  icon: "star.fill",         title: "Saved")
                } header: {
                    Text("Workspace")
                }

                Section {
                    sidebarLabel(.console(.sources),   icon: "chart.pie.fill",               title: "Sources")
                    sidebarLabel(.console(.cameras),   icon: "video.fill",                   title: "Camera discovery")
                    sidebarLabel(.console(.alerts),    icon: "bell.badge.fill",              title: "Alerts")
                    sidebarLabel(.console(.apiKeys),   icon: "key.fill",                     title: "API keys")
                    if canManageWorkspace {
                        sidebarLabel(.console(.workspace), icon: "person.2.badge.key.fill",  title: "Workspace")
                    }
                    sidebarLabel(.console(.settings),  icon: "gearshape.fill",               title: "Settings")
                } header: {
                    Text("Console")
                }

                if auth.isPlatformAdmin {
                    Section {
                        sidebarLabel(.console(.admin), icon: "lock.shield.fill", title: "Admin")
                    } header: {
                        Text("Admin")
                    }
                }
            }
            .navigationTitle("JapanOSINT")
            #if os(macOS)
            .listStyle(.sidebar)
            #endif
            .themedScreenBackground(theme)
    }

    @ViewBuilder
    private var detailView: some View {
        switch sidebar {
        case .workspace(.map),  .none: MapTab()
        case .workspace(.intel):       IntelTab()
        case .workspace(.search):      SearchTab()
        case .entities:                EntitiesView()
        case .workspace(.saved):       SavedTab()
        case .workspace(.console):     ConsoleHub()
        case .console(let dest):
            // Console destinations need their own NavigationStack on iPad
            // since they're no longer hosted by Console's stack here.
            NavigationStack {
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
        }
    }

    private func sidebarLabel(_ item: SidebarItem,
                              icon: String,
                              title: String) -> some View {
        Label(title, systemImage: icon).tag(item)
    }
}

// MARK: - Loading gate (with backend-unreachable escape)

/// The launch spinner. If bootstrap can't reach the backend (`connectionStalled`)
/// — or it's just taking a while — it offers a "Server settings" escape so a
/// returning user can fix the backend URL without being trapped on the spinner.
private struct LoadingGate: View {
    @EnvironmentObject private var auth: AuthSession
    @Environment(\.theme) private var theme
    @State private var showSettings = false
    @State private var slow = false

    var body: some View {
        ZStack {
            theme.surface.ignoresSafeArea()
            VStack(spacing: 16) {
                ProgressView().tint(theme.accent)
                if auth.connectionStalled {
                    VStack(spacing: 6) {
                        Image(systemName: "wifi.exclamationmark")
                            .font(.title2).foregroundStyle(theme.warning)
                        Text("Can't reach the backend")
                            .font(.headline).foregroundStyle(theme.text)
                        Text("Check the server URL or that the backend is running on this network.")
                            .font(.caption).foregroundStyle(theme.textMuted)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 32)
                    }
                    .transition(.opacity)
                }
                if auth.connectionStalled || slow {
                    Button {
                        showSettings = true
                    } label: {
                        Label("Server settings", systemImage: "server.rack")
                    }
                    .buttonStyle(.bordered)
                    .transition(.opacity)
                }
            }
            .animation(.easeInOut(duration: 0.25), value: auth.connectionStalled)
            .animation(.easeInOut(duration: 0.25), value: slow)
        }
        .task {
            // Surface the escape after a short wait even if not yet flagged
            // stalled, so a long hang is never a dead end.
            try? await Task.sleep(nanoseconds: 3_000_000_000)
            slow = true
        }
        .sheet(isPresented: $showSettings) { ConnectionSettingsSheet() }
    }
}

/// Minimal sheet to edit the backend URL and retry, reachable from the loading
/// spinner — the one place a returning user can fix connectivity pre-login.
private struct ConnectionSettingsSheet: View {
    @EnvironmentObject private var settings: AppSettings
    @EnvironmentObject private var auth: AuthSession
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss
    @State private var retrying = false

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("http://host.local:4072", text: $settings.backendBaseURL)
                        .textContentType(.URL)
                        .compatKeyboardURL()
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                        .font(.system(.body, design: .monospaced))
                        .fontDesign(.monospaced)
                } header: {
                    Text("Backend URL")
                } footer: {
                    Text("Point the app at your JapanOSINT backend (host name or IP). Saves instantly.")
                }

                Section {
                    Button {
                        Task {
                            retrying = true
                            await auth.retryBootstrap()
                            retrying = false
                            dismiss()
                        }
                    } label: {
                        HStack {
                            if retrying { ProgressView().controlSize(.small) }
                            Text(retrying ? "Connecting…" : "Retry connection")
                        }
                    }
                    .disabled(retrying || settings.backendBaseURL.isEmpty)
                }
            }
            .compatGroupedForm()
            .navigationTitle("Connection")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Close") { dismiss() }
                }
            }
        }
    }
}
