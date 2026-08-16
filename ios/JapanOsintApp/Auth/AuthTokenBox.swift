import Foundation

/// Process-wide, thread-safe holder for the current Supabase access token +
/// active tenant id, plus a refresh hook.
///
/// `API` is a value type constructed ad-hoc in ~40 call sites and its
/// `request()` runs off the main actor, so it can't reach the `@MainActor`
/// `AuthSession`. This box is the bridge: `AuthSession` pushes the latest
/// token/tenant here whenever they change, and `API.request()` reads them
/// synchronously to stamp `Authorization` / `X-Tenant-Id` headers.
///
/// The refresh closure lets `request()` recover from a 401: it awaits a
/// token refresh (performed by `AuthSession` against Supabase directly — not
/// through `API`, so there's no recursion) and signals whether to retry.
/// `coalescedRefresh()` guarantees every concurrent 401 shares ONE refresh
/// task so Supabase's single-use rotating refresh token is never burned by
/// parallel refreshes and the Keychain only ever has one writer.
final class AuthTokenBox: @unchecked Sendable {
    static let shared = AuthTokenBox()

    private let lock = NSLock()
    private var _accessToken: String?
    private var _tenantId: String?
    private var _refresh: (@Sendable () async -> Bool)?
    /// The single in-flight refresh. Every concurrent 401 awaits THIS task
    /// rather than kicking off its own — Supabase rotates the refresh token
    /// on every use and is single-use, so parallel refreshes would burn the
    /// token and a late one could clobber the Keychain with a stale value.
    private var _inFlightRefresh: Task<Bool, Never>?

    private init() {}

    var accessToken: String? {
        lock.lock(); defer { lock.unlock() }
        return _accessToken
    }

    var tenantId: String? {
        lock.lock(); defer { lock.unlock() }
        return _tenantId
    }

    func set(accessToken: String?, tenantId: String?) {
        lock.lock()
        _accessToken = accessToken
        _tenantId = tenantId
        lock.unlock()
    }

    func setRefreshHandler(_ handler: (@Sendable () async -> Bool)?) {
        lock.lock()
        _refresh = handler
        lock.unlock()
    }

    /// Coalesced refresh. The FIRST caller to hit a 401 spins up the single
    /// refresh task; every concurrent caller awaits the same task and shares
    /// its result. The task is cleared once it completes so the next genuine
    /// expiry can refresh again. Returns `false` if no refresh hook is armed
    /// (legacy single-tenant mode).
    func coalescedRefresh() async -> Bool {
        guard let task = claimRefresh() else { return false }
        return await task.value
    }

    /// Synchronous half of `coalescedRefresh` — returns the task to await, or
    /// `nil` when no refresh hook is armed. `NSLock.lock()`/`unlock()` are
    /// unavailable from async contexts (a hard error under Swift 6), and
    /// rightly so: a lock must never be held across a suspension point. Keeping
    /// the whole critical section in a non-async method is what guarantees that
    /// — the caller only awaits once the lock is already released.
    private func claimRefresh() -> Task<Bool, Never>? {
        lock.lock(); defer { lock.unlock() }
        if let inFlight = _inFlightRefresh { return inFlight }
        guard let refresh = _refresh else { return nil }
        let task = Task<Bool, Never> {
            let ok = await refresh()
            // Clear the slot from inside the task so any caller that arrives
            // after we finished but before we cleared still got our result.
            self.clearInFlight()
            return ok
        }
        _inFlightRefresh = task
        return task
    }

    private func clearInFlight() {
        lock.lock(); defer { lock.unlock() }
        _inFlightRefresh = nil
    }
}
