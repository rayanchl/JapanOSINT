import Foundation
import LocalAuthentication

/// Wraps `LAContext` so callers don't have to worry about the canEvaluate /
/// evaluate dance or the NSError-throwing API. Uses
/// `.deviceOwnerAuthentication` (rather than `.deviceOwnerAuthenticationWithBiometrics`)
/// so a device without Face ID/Touch ID — or one where the user has the
/// biometric disabled — can still authenticate via the device passcode.
enum BiometricAuth {
    enum Result {
        case success
        case failure(String)   // already-localized error message for inline display
    }

    static func authenticate(reason: String) async -> Result {
        let ctx = LAContext()
        ctx.localizedFallbackTitle = "Use Passcode"
        var err: NSError?
        guard ctx.canEvaluatePolicy(.deviceOwnerAuthentication, error: &err) else {
            let msg = err?.localizedDescription ?? "Authentication is not available on this device."
            return .failure(msg)
        }
        do {
            let ok = try await ctx.evaluatePolicy(.deviceOwnerAuthentication, localizedReason: reason)
            return ok ? .success : .failure("Authentication failed.")
        } catch {
            return .failure(error.localizedDescription)
        }
    }
}
