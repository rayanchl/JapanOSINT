import Foundation
#if os(iOS)
import BackgroundTasks
#endif

// ─────────────────────────────────────────────────────────────────────────────
// BGTaskScheduler wiring (roadmap 37) — the freshness engine behind the widgets.
//
// `register` MUST run before the app finishes launching (called from
// `JapanOsintApp.init`). The two identifiers must also appear verbatim in
// Info.plist under `BGTaskSchedulerPermittedIdentifiers`, and the target needs
// the Background Modes capability (fetch + processing) — see
// docs/ios-extensions-plan.md. Without the capability, `register` returns false
// and `submit` throws; both are handled so the app still launches cleanly.
//
// The task delta-syncs (currently: refreshes the widget snapshot) and chains the
// next request. It is the difference between a widget that updates ~every
// 15 min best-effort and one that is only ever as fresh as the last time the
// user opened the app.
// ─────────────────────────────────────────────────────────────────────────────

// PLATFORM. BGTaskScheduler is iOS-only, and this file carried no guard at all,
// so the macOS destination of this shared target did not compile. The three
// entry points below keep their signatures on every platform — the four call
// sites in JapanOsintApp.swift are deliberately left untouched — and the
// iOS-only machinery is compiled out rather than the callers being wrapped.
//
// macOS gets NO-OPS, not an alternative implementation. AppKit's nearest
// equivalent is NSBackgroundActivityScheduler, which has different semantics
// (no BGProcessingTask-style "charging and on Wi-Fi" contract, no Info.plist
// identifier registry), and whether the Mac app should refresh in the
// background at all is a product decision, not something to infer from a
// compile error. The widget snapshot still refreshes whenever the app is
// foregrounded, so nothing silently stops working — it just is not proactive.

enum BackgroundRefresh {
    static let refreshTaskId = "com.rayanchl.japanosint.refresh"
    static let syncTaskId    = "com.rayanchl.japanosint.sync"

#if !os(iOS)
    /// See the note above: deliberate no-ops, so callers stay platform-agnostic.
    static func register(apiProvider: @escaping () -> API) {}
    static func scheduleRefresh() {}
    static func scheduleSync() {}
#else

    /// The ONLY work these tasks do is refresh the widget snapshot, and that
    /// snapshot lives in the App Group container. While the App Group is not
    /// wired up (see `ios/EXTENSION_TARGETS_TODO.md`) every wake-up would fetch
    /// the inbox over the network and then discard the result — spending the
    /// system's background budget, and the user's battery and data, on a write
    /// that cannot land. So the whole subsystem stays dark until the container
    /// exists. Registration, submission and execution all gate on this.
    private static var isUseful: Bool { AppGroup.isConfigured }

    /// Register handlers. `apiProvider` is a closure so the current `API`
    /// (which tracks the backend URL) is resolved at run time, not capture time.
    ///
    /// ISOLATION. Everything in this file is main-actor by default
    /// (`SWIFT_DEFAULT_ACTOR_ISOLATION = MainActor`), but `using: nil` gives the
    /// launch handler a *background* queue, so the handler body could not name
    /// `handle`/`scheduleRefresh` without an isolation error. Registering on
    /// `.main` makes the handler's real execution context match the isolation
    /// the code already assumes, and `assumeIsolated` then holds. The handler
    /// itself is trivial — it chains the next request, spins up a `Task` and
    /// installs an expiration handler; the actual fetch runs off-main inside
    /// that `Task`, so nothing heavy lands on the main queue.
    ///
    /// `{ scheduleRefresh() }` rather than the bare `scheduleRefresh`: an
    /// *unapplied* reference to a main-actor method is formed as a nonisolated
    /// function value, which is exactly the conversion Swift 6 rejects. The
    /// closure literal instead picks up `@MainActor` from the parameter type
    /// and calls the method from inside the isolation it belongs to.
    static func register(apiProvider: @escaping () -> API) {
        guard isUseful else { return }
        BGTaskScheduler.shared.register(
            forTaskWithIdentifier: refreshTaskId, using: .main) { task in
            MainActor.assumeIsolated {
                handle(task, apiProvider: apiProvider, reschedule: { scheduleRefresh() })
            }
        }
        BGTaskScheduler.shared.register(
            forTaskWithIdentifier: syncTaskId, using: .main) { task in
            MainActor.assumeIsolated {
                handle(task, apiProvider: apiProvider, reschedule: { scheduleSync() })
            }
        }
    }

    /// Ask for a best-effort refresh ~15 min out. Safe to call repeatedly; the
    /// scheduler coalesces. `try?` swallows the throw the capability isn't set.
    static func scheduleRefresh() {
        guard isUseful else { return }
        let req = BGAppRefreshTaskRequest(identifier: refreshTaskId)
        req.earliestBeginDate = Date(timeIntervalSinceNow: 15 * 60)
        try? BGTaskScheduler.shared.submit(req)
    }

    /// Larger, opportunistic sync. The OS runs these when charging / on Wi-Fi.
    static func scheduleSync() {
        guard isUseful else { return }
        let req = BGProcessingTaskRequest(identifier: syncTaskId)
        req.requiresNetworkConnectivity = true
        req.requiresExternalPower = false
        req.earliestBeginDate = Date(timeIntervalSinceNow: 60 * 60)
        try? BGTaskScheduler.shared.submit(req)
    }

    // `reschedule` is `@MainActor` because that is what the two things passed
    // to it actually are: under the target's MainActor-by-default isolation,
    // `scheduleRefresh`/`scheduleSync` are main-actor methods, and BGTask's
    // launch handler is itself main-actor. Typing the parameter as a plain
    // `() -> Void` silently demanded a nonisolated function and produced a
    // Swift 6 isolation error at both call sites.
    private static func handle(_ task: BGTask,
                               apiProvider: @escaping () -> API,
                               reschedule: @MainActor @escaping () -> Void) {
        // Chain the next occurrence first — if the work below crashes, the
        // series still continues on the next launch.
        reschedule()
        // Belt and braces: a request submitted before the capability was
        // removed could still be delivered. Complete immediately rather than
        // spending the wake-up on a fetch nothing can read.
        guard isUseful else {
            task.setTaskCompleted(success: true)
            return
        }
        let api = apiProvider()
        let work = Task {
            await WidgetSnapshotBuilder.refresh(api: api)
            task.setTaskCompleted(success: !Task.isCancelled)
        }
        task.expirationHandler = { work.cancel() }
    }
#endif
}
