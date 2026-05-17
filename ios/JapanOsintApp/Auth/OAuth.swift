import AuthenticationServices
import CryptoKit
import Foundation
import Security
import SwiftUI
import UIKit

/// Supabase social sign-in (Google / X / Apple / GitHub) via the GoTrue
/// `/authorize` endpoint and an in-app `ASWebAuthenticationSession`.
///
/// We use the **PKCE** flow (not implicit): GoTrue redirects back with a
/// short-lived `?code=`, which we exchange for a Session at
/// `token?grant_type=pkce`. PKCE keeps tokens out of the URL/browser history
/// and works even when implicit grant is disabled on the project.
///
/// Project setup required (Supabase dashboard, can't be done in code):
///   • Authentication → Providers: enable & configure each provider.
///   • Authentication → URL Configuration → Redirect URLs: add
///     `japanosint://auth-callback`.

enum OAuthProvider: String, CaseIterable, Identifiable, Sendable {
    // rawValue == GoTrue provider id. `twitter` is X; `facebook` is FB.
    case apple, google, github, twitter, facebook

    var id: String { rawValue }

    /// User-facing label. `twitter` is the GoTrue provider id for X.
    var label: String {
        switch self {
        case .apple:    return "Apple"
        case .google:   return "Google"
        case .github:   return "GitHub"
        case .twitter:  return "X"
        case .facebook: return "Facebook"
        }
    }

    /// Apple ships its real logo as an SF Symbol; every other provider's
    /// real brand mark is a vector asset in Assets.xcassets.
    var usesSFSymbol: Bool { self == .apple }

    /// SF Symbol name (Apple) or asset-catalog image name (the rest).
    var logoName: String {
        switch self {
        case .apple:    return "apple.logo"
        case .google:   return "OAuthGoogle"
        case .github:   return "OAuthGitHub"
        case .twitter:  return "OAuthX"
        case .facebook: return "OAuthFacebook"
        }
    }

    // MARK: - Brand styling (per each platform's sign-in guidelines)

    /// Button fill. Apple/X → black, Google/GitHub → white, FB → FB blue.
    var brandBackground: Color {
        switch self {
        case .apple, .twitter: return .black
        case .google, .github: return .white
        case .facebook:        return Color(hex: "1877F2")
        }
    }

    /// Label/text color against `brandBackground`.
    var brandForeground: Color {
        switch self {
        case .apple, .twitter, .facebook: return .white
        case .google, .github:            return .black
        }
    }

    /// White buttons need a hairline so they read on a light surface.
    var brandNeedsBorder: Bool {
        switch self {
        case .google, .github: return true
        default:               return false
        }
    }
}

/// RFC 7636 PKCE pair. Verifier is 32 random bytes (base64url, 43 chars);
/// challenge is base64url(SHA256(verifier)).
struct PKCEPair: Sendable {
    let verifier: String
    let challenge: String

    init() {
        var bytes = [UInt8](repeating: 0, count: 32)
        _ = SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes)
        verifier = Data(bytes).base64URLEncodedString()
        let digest = SHA256.hash(data: Data(verifier.utf8))
        challenge = Data(digest).base64URLEncodedString()
    }
}

private extension Data {
    func base64URLEncodedString() -> String {
        base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .replacingOccurrences(of: "=", with: "")
    }
}

/// Drives a single `ASWebAuthenticationSession` round-trip. Held by the
/// caller (`AuthSession`) for the lifetime of the flow.
@MainActor
final class WebAuthRunner: NSObject, ASWebAuthenticationPresentationContextProviding {
    private var session: ASWebAuthenticationSession?

    /// Opens `url`, returns the provider's callback URL (custom scheme).
    /// Throws `SupabaseAuth.AuthError.server` on user-cancel / failure.
    func authenticate(url: URL, callbackScheme: String) async throws -> URL {
        try await withCheckedThrowingContinuation { cont in
            let s = ASWebAuthenticationSession(
                url: url, callbackURLScheme: callbackScheme
            ) { callback, error in
                if let callback {
                    cont.resume(returning: callback)
                } else {
                    let msg = (error as? ASWebAuthenticationSessionError)?
                        .code == .canceledLogin
                        ? "Sign-in cancelled."
                        : (error?.localizedDescription ?? "Sign-in failed.")
                    cont.resume(throwing: SupabaseAuth.AuthError.server(-1, msg))
                }
            }
            s.presentationContextProvider = self
            // Reuse existing provider cookies so the user isn't forced to
            // re-enter Google/GitHub creds every time.
            s.prefersEphemeralWebBrowserSession = false
            session = s
            if !s.start() {
                cont.resume(throwing:
                    SupabaseAuth.AuthError.server(-1, "Could not start sign-in."))
            }
        }
    }

    func presentationAnchor(for _: ASWebAuthenticationSession) -> ASPresentationAnchor {
        let scenes = UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
        if let key = scenes.flatMap(\.windows).first(where: \.isKeyWindow) {
            return key
        }
        if let anyWindow = scenes.flatMap(\.windows).first {
            return anyWindow
        }
        // In iOS 26 both `UIWindow.init()` and `init(frame:)` are deprecated;
        // `init(windowScene:)` is the only non-deprecated path. A
        // foreground-active UIWindowScene is always present while
        // ASWebAuthenticationSession is being presented, so `scenes.first`
        // is non-nil here.
        return ASPresentationAnchor(windowScene: scenes.first!)
    }
}
