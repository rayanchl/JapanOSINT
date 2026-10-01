import AppIntents

// Roadmap 36 — "Check exposure for …" via Siri / Shortcuts.
//
// Routed through the search flow, which already surfaces entity and breach hits
// for an identifier. The identifier is handed to the app in-process and never
// stored by the intent itself.
struct CheckExposureIntent: AppIntent {
    static var title: LocalizedStringResource = "Check exposure"
    static var description = IntentDescription(
        "Look up breach exposure for an email, username or domain in JapanOSINT.")

    static var openAppWhenRun = true

    @Parameter(title: "Email, username or domain",
               requestValueDialog: "Whose exposure should I check?")
    var identifier: String

    @MainActor
    func perform() async throws -> some IntentResult {
        IntentRouter.shared.request(.exposure(identifier: identifier))
        return .result()
    }
}
