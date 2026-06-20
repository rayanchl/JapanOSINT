import Foundation
import Combine

/// Owns the authenticated session and drives the top-level onboarding gate.
///
/// Responsibilities:
///  - bootstrap on launch: decide `loading` → `onboarding` | `ready`
///  - sign in / sign up / sign out against Supabase
///  - keep `AuthTokenBox` (read by `API.request`) in lockstep with the token
///  - install a refresh hook so a 401 mid-session self-heals once
///  - resolve the active tenant via `GET /api/me`
@MainActor
final class AuthSession: ObservableObject {
    enum Gate: Equatable {
        case loading
        case onboarding
        case ready
    }

    @Published private(set) var gate: Gate = .loading
    @Published private(set) var me: MeResponse?
    @Published private(set) var lastError: String?

    private unowned let settings: AppSettings

    init(settings: AppSettings) {
        self.settings = settings
        AuthTokenBox.shared.setRefreshHandler { [weak self] in
            await self?.performRefresh() ?? false
        }
    }

    // MARK: - Derived

    var accountEmail: String? { me?.user?.email }
    var tenantName: String? { me?.tenant?.name }
    var memberships: [MeTenant] { me?.memberships ?? [] }

    private var supabase: SupabaseAuth {
        // Point-of-use fallback so a future field rename can't silently empty
        // the config. The init-time backfill in AppSettings is the primary fix.
        SupabaseAuth(
            projectURL: settings.supabaseURL.isEmpty ? BuildConfig.supabaseURL : settings.supabaseURL,
            anonKey:    settings.supabaseAnonKey.isEmpty ? BuildConfig.supabaseAnonKey : settings.supabaseAnonKey)
    }
    private var api: API { API(baseURL: settings.backendBaseURL) }

    // MARK: - Launch

    /// Called once from the App `.task`. Never throws — every failure path
    /// resolves to a concrete gate so the UI is never stuck on a spinner.
    func bootstrap() async {
        guard settings.onboardingCompleted else {
            gate = .onboarding
            return
        }
        // Re-arm the token box from the Keychain so requests this launch
        // carry the bearer even before /api/me round-trips.
        let access = KeychainStore.get(KeychainStore.Account.accessToken)
        applyToken(access)

        do {
            let m = try await api.me()
            adopt(m)
            gate = .ready
        } catch APIError.http(let code, _) where code == 401 {
            // Token expired between launches — try one refresh. Route through
            // the box so this coalesces with any concurrent /api refresh
            // instead of racing it (Supabase refresh tokens are single-use).
            if await AuthTokenBox.shared.coalescedRefresh(), let m = try? await api.me() {
                adopt(m)
                gate = .ready
            } else {
                gate = .onboarding
            }
        } catch {
            // Network/offline or server down: don't lock a returning user
            // out. We already re-armed the cached token; proceed.
            gate = .ready
        }
    }

    // MARK: - Sign in / up

    func signIn(email: String, password: String) async throws {
        let s = try await supabase.signIn(email: email, password: password)
        try await establish(session: s)
    }

    func signUp(email: String, password: String) async throws {
        switch try await supabase.signUp(email: email, password: password) {
        case .session(let s):
            try await establish(session: s)
        case .confirmationRequired:
            // Email-confirmation is enabled on the project.
            throw SupabaseAuth.AuthError.server(
                200, "Check your inbox to confirm your email, then sign in.")
        }
    }

    /// Social sign-in (Google / X / Apple / GitHub). Opens the provider in an
    /// `ASWebAuthenticationSession`, exchanges the PKCE code for a Session,
    /// then establishes it exactly like password sign-in. New users are
    /// auto-provisioned a workspace by the server's tenant middleware.
    func signIn(provider: OAuthProvider) async throws {
        let pkce = PKCEPair()
        let url = try supabase.oauthAuthorizeURL(
            provider: provider.rawValue, codeChallenge: pkce.challenge)
        let runner = WebAuthRunner()
        let callback = try await runner.authenticate(
            url: url, callbackScheme: SupabaseAuth.oauthScheme)
        let session = try await supabase.completeOAuth(
            callback: callback, verifier: pkce.verifier)
        try await establish(session: session)
    }

    func signOut() {
        KeychainStore.delete(KeychainStore.Account.accessToken)
        KeychainStore.delete(KeychainStore.Account.refreshToken)
        AuthTokenBox.shared.set(accessToken: nil, tenantId: nil)
        me = nil
        settings.onboardingCompleted = false
        gate = .onboarding
    }

    /// Switch the active tenant (multi-membership users). Persists the choice
    /// and re-reads /api/me with the new `X-Tenant-Id`.
    func switchTenant(_ tenantId: String) async {
        settings.activeTenantId = tenantId
        AuthTokenBox.shared.set(
            accessToken: KeychainStore.get(KeychainStore.Account.accessToken),
            tenantId: tenantId.isEmpty ? nil : tenantId)
        if let m = try? await api.me() { adopt(m) }
    }

    /// Finalises onboarding once the wizard has a usable session (or the
    /// backend is legacy single-tenant and needs no auth at all).
    func completeOnboarding() {
        settings.onboardingCompleted = true
        settings.requestMapIntro = true
        gate = .ready
    }

    // MARK: - Internals

    private func establish(session: SupabaseAuth.Session) async throws {
        KeychainStore.set(session.access_token, for: KeychainStore.Account.accessToken)
        KeychainStore.set(session.refresh_token, for: KeychainStore.Account.refreshToken)
        applyToken(session.access_token)
        let m = try await api.me()
        adopt(m)
    }

    /// Mirror server identity into local state + token box. Picks up the
    /// resolved tenant id so subsequent requests pin to it.
    private func adopt(_ m: MeResponse) {
        me = m
        let tenantId = m.tenant?.id ?? (settings.activeTenantId.isEmpty ? nil : settings.activeTenantId)
        if let tid = m.tenant?.id { settings.activeTenantId = tid }
        AuthTokenBox.shared.set(
            accessToken: KeychainStore.get(KeychainStore.Account.accessToken),
            tenantId: tenantId)
    }

    private func applyToken(_ token: String?) {
        let tid = settings.activeTenantId.isEmpty ? nil : settings.activeTenantId
        AuthTokenBox.shared.set(accessToken: token, tenantId: tid)
    }

    /// Refresh hook — also installed into `AuthTokenBox` for the 401 retry.
    /// `nonisolated` boundary handled by hopping to the main actor for the
    /// `settings`/Keychain reads.
    private func performRefresh() async -> Bool {
        guard let refresh = KeychainStore.get(KeychainStore.Account.refreshToken),
              !settings.supabaseURL.isEmpty else { return false }
        do {
            let s = try await supabase.refresh(refreshToken: refresh)
            KeychainStore.set(s.access_token, for: KeychainStore.Account.accessToken)
            KeychainStore.set(s.refresh_token, for: KeychainStore.Account.refreshToken)
            applyToken(s.access_token)
            return true
        } catch {
            return false
        }
    }
}
