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
        accountEmail?.lowercased() == "rayanchouialkessal@gmail.com"
    }
}
