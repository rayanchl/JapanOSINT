import Foundation
import BackgroundTasks

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

enum BackgroundRefresh {
    static let refreshTaskId = "com.rayanchl.japanosint.refresh"
    static let syncTaskId    = "com.rayanchl.japanosint.sync"

    /// Register handlers. `apiProvider` is a closure so the current `API`
    /// (which tracks the backend URL) is resolved at run time, not capture time.
    static func register(apiProvider: @escaping () -> API) {
        BGTaskScheduler.shared.register(
            forTaskWithIdentifier: refreshTaskId, using: nil) { task in
            handle(task, apiProvider: apiProvider, reschedule: scheduleRefresh)
        }
        BGTaskScheduler.shared.register(
            forTaskWithIdentifier: syncTaskId, using: nil) { task in
            handle(task, apiProvider: apiProvider, reschedule: scheduleSync)
        }
    }

    /// Ask for a best-effort refresh ~15 min out. Safe to call repeatedly; the
    /// scheduler coalesces. `try?` swallows the throw the capability isn't set.
    static func scheduleRefresh() {
        let req = BGAppRefreshTaskRequest(identifier: refreshTaskId)
        req.earliestBeginDate = Date(timeIntervalSinceNow: 15 * 60)
        try? BGTaskScheduler.shared.submit(req)
    }

    /// Larger, opportunistic sync. The OS runs these when charging / on Wi-Fi.
    static func scheduleSync() {
        let req = BGProcessingTaskRequest(identifier: syncTaskId)
        req.requiresNetworkConnectivity = true
        req.requiresExternalPower = false
        req.earliestBeginDate = Date(timeIntervalSinceNow: 60 * 60)
        try? BGTaskScheduler.shared.submit(req)
    }

    private static func handle(_ task: BGTask,
                               apiProvider: @escaping () -> API,
                               reschedule: @escaping () -> Void) {
        // Chain the next occurrence first — if the work below crashes, the
        // series still continues on the next launch.
        reschedule()
        let api = apiProvider()
        let work = Task {
            await WidgetSnapshotBuilder.refresh(api: api)
            task.setTaskCompleted(success: !Task.isCancelled)
        }
        task.expirationHandler = { work.cancel() }
    }
}
