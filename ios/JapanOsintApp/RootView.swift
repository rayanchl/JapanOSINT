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
    @EnvironmentObject var mapNav: MapNavigation
    @Environment(\.theme) private var theme
    @Environment(\.horizontalSizeClass) private var hSize

    var body: some View {
        Group {
            if hSize == .regular {
                splitView
            } else {
                tabView
            }
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
    }

    @State private var sidebar: SidebarItem? = .workspace(.map)

    private var splitView: some View {
        NavigationSplitView {
            List(selection: $sidebar) {
                Section {
                    sidebarLabel(.workspace(.map),    icon: "map.fill",          title: "Map")
                    sidebarLabel(.workspace(.intel),  icon: "newspaper.fill",    title: "Intel")
                    sidebarLabel(.workspace(.saved),  icon: "star.fill",         title: "Saved")
                } header: {
                    Text("Workspace")
                }

                Section {
                    sidebarLabel(.console(.sources),   icon: "chart.pie.fill",               title: "Sources")
                    sidebarLabel(.console(.database),  icon: "cylinder.split.1x2.fill",      title: "Database")
                    sidebarLabel(.console(.scheduler), icon: "calendar.badge.clock",         title: "Scheduler")
                    sidebarLabel(.console(.followLog), icon: "scroll",                       title: "Follow log")
                    sidebarLabel(.console(.cameras),   icon: "video.fill",                   title: "Camera discovery")
                    sidebarLabel(.console(.alerts),    icon: "bell.badge.fill",              title: "Alerts")
                    sidebarLabel(.console(.apiKeys),   icon: "key.fill",                     title: "API keys")
                    sidebarLabel(.console(.settings),  icon: "gearshape.fill",               title: "Settings")
                } header: {
                    Text("Console")
                }
            }
            .navigationTitle("JapanOSINT")
            .scrollContentBackground(.hidden)
            .background(theme.surface.ignoresSafeArea())
        } detail: {
            detailView
        }
        // Mirror cross-tab deep links into the sidebar selection on iPad.
        // Phone gets this for free via `selectedTab` driving the TabView.
        .onChange(of: mapNav.selectedTab) { _, raw in
            guard let tab = AppTab(rawValue: raw) else { return }
            switch tab {
            case .map, .intel, .saved: sidebar = .workspace(tab)
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

    @ViewBuilder
    private var detailView: some View {
        switch sidebar {
        case .workspace(.map),  .none: MapTab()
        case .workspace(.intel):       IntelTab()
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
