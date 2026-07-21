import Foundation
import Combine

/// Single injected entry point to the backend.
///
/// Replaces the ~40 ad-hoc `API(baseURL: settings.backendBaseURL)`
/// constructions scattered across views. Construct it once from
/// `AppSettings` and inject via `.environmentObject`; call sites read
/// `apiClient.api` to get a value-type `API` bound to the *current* base
/// URL, so a Settings change to the backend URL is picked up everywhere
/// without restart.
///
/// The bearer / `X-Tenant-Id` header stamping and the coalesced 401-refresh
/// logic live in `API` + `AuthTokenBox` (process-wide, so the off-main-actor
/// `API.request` path can reach them); this type is the DI seam that owns
/// *which* base URL those requests target.
@MainActor
final class APIClient: ObservableObject {
    /// Mirrors `AppSettings.backendBaseURL`. Published so SwiftUI views that
    /// hold an `@EnvironmentObject APIClient` re-render if it ever changes,
    /// though in practice call sites just read `.api` per request.
    @Published private(set) var baseURL: String

    private unowned let settings: AppSettings
    private var cancellable: AnyCancellable?

    init(settings: AppSettings) {
        self.settings = settings
        self.baseURL = settings.backendBaseURL
        // Keep in lockstep with the user-chosen backend URL.
        cancellable = settings.objectWillChange
            .receive(on: RunLoop.main)
            .sink { [weak self] _ in
                guard let self else { return }
                let latest = self.settings.backendBaseURL
                if latest != self.baseURL { self.baseURL = latest }
            }
    }

    /// A fresh value-type `API` bound to the current backend URL. Cheap to
    /// construct (just wraps a String); always reflects the latest setting.
    var api: API { API(baseURL: baseURL) }
}
