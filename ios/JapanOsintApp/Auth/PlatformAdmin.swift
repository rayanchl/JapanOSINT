import Foundation

extension AuthSession {
    /// Platform-operator gate. Mirrors the server's `requirePlatformOperator`
    /// intent (server/src/middleware/keyAccess.js): operator-only surfaces
    /// (raw DB explorer, scheduler, follow log, platform API keys, server
    /// restart) affect every workspace on the host, so they're restricted to
    /// the platform super-admin account rather than any workspace owner/admin.
    ///
    /// Hiding these surfaces for non-operators also means they never hit the
    /// server's `403 {"error":"Platform operator access not configured"}`.
    var isPlatformAdmin: Bool {
        // The operator's address is deployment configuration (Info.plist key
        // JOPlatformAdminEmail), not source. Unset: no one is shown the
        // operator surfaces; the server enforces the gate either way.
        guard let configured = (Bundle.main.object(forInfoDictionaryKey: "JOPlatformAdminEmail") as? String)?
            .trimmingCharacters(in: .whitespaces).lowercased(), !configured.isEmpty else { return false }
        return accountEmail?.lowercased() == configured
    }
}
