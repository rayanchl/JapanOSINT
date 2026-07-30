import AppIntents

// Roadmap 36 — Siri phrases + Shortcuts app tiles for the app's intents.
//
// `AppShortcutsProvider` is discovered automatically by the system; no
// registration call is needed. Phrases MUST contain `\(.applicationName)` — the
// system rejects a provider whose phrases don't reference the app name.
struct JapanOsintShortcuts: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        // Phrases can only interpolate AppEntity/AppEnum parameters, never a
        // plain String — the metadata exporter rejects `\(\.$query)` here. The
        // query/identifier is collected at run time via `requestValueDialog`.
        AppShortcut(
            intent: SearchJapanOSINTIntent(),
            phrases: [
                "Search \(.applicationName)",
                "Run a \(.applicationName) search",
            ],
            shortTitle: "Search",
            systemImageName: "magnifyingglass"
        )
        AppShortcut(
            intent: CheckExposureIntent(),
            phrases: [
                "Check exposure in \(.applicationName)",
                "\(.applicationName) breach exposure",
            ],
            shortTitle: "Check exposure",
            systemImageName: "exclamationmark.shield"
        )
    }
}
