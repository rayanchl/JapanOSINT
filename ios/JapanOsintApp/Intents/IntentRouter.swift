import Foundation
import Combine

// ─────────────────────────────────────────────────────────────────────────────
// In-process hand-off from App Intents, Spotlight opens and the share-queue
// drain to the running UI (roadmap 36).
//
// App Intents declared in the app target run in the app's OWN process when
// `openAppWhenRun` is true, so an intent can publish an action here and the
// shell (`RootView`) consumes it — no App Group round-trip is needed for the
// intent path (that indirection is only for the widget/share EXTENSIONS, which
// run out-of-process). One published action at a time; the consumer clears it.
// ─────────────────────────────────────────────────────────────────────────────

enum IntentAction: Equatable {
    /// Run an OSINT search for this query.
    case search(String)
    /// Look up breach exposure for an email / username / domain. Surfaced
    /// through the same search flow, which already returns entity + breach hits.
    case exposure(identifier: String)
}

@MainActor
final class IntentRouter: ObservableObject {
    static let shared = IntentRouter()
    private init() {}

    /// Non-nil while an action is waiting for the shell to handle it. The shell
    /// sets it back to nil after dispatching so the same action can't replay on
    /// the next view update.
    @Published var pending: IntentAction?

    func request(_ action: IntentAction) { pending = action }
}
