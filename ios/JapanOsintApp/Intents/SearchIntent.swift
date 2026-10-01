import AppIntents

// Roadmap 36 — "Search JapanOSINT for …" via Siri / Shortcuts / Spotlight.
//
// The intent lives in the app target, so `perform()` runs in-process: it hands
// the query to `IntentRouter` and asks the system to foreground the app, where
// `RootView` switches to the Search tab and starts the run. Keeping the network
// work in the app means one auth path and one error surface.
struct SearchJapanOSINTIntent: AppIntent {
    static var title: LocalizedStringResource = "Search JapanOSINT"
    static var description = IntentDescription(
        "Run an OSINT search in JapanOSINT.")

    /// Bring the app to the foreground; the search UI is where results render.
    static var openAppWhenRun = true

    @Parameter(title: "Query", requestValueDialog: "What should I search for?")
    var query: String

    @MainActor
    func perform() async throws -> some IntentResult {
        IntentRouter.shared.request(.search(query))
        return .result()
    }
}
