import Foundation

/// Thin Supabase GoTrue (auth) REST client. We deliberately avoid the
/// supabase-swift SDK — the server only needs a valid HS256 access token in
/// `Authorization: Bearer`, and password/refresh grants are three plain
/// POSTs. Calls go straight to the Supabase project (not through `API`), so
/// the 401→refresh path in `API.request()` can't recurse.
struct SupabaseAuth: Sendable {
    let projectURL: String   // e.g. https://abcd.supabase.co
    let anonKey: String      // publishable anon key (safe on-device)

    struct Session: Decodable, Sendable {
        let access_token: String
        let refresh_token: String
        let expires_in: Int?
        let token_type: String?
        let user: SBUser?
    }
    struct SBUser: Decodable, Sendable {
        let id: String
        let email: String?
    }

    /// Result of `/signup`. GoTrue returns a `Session` only when
    /// email-confirmation is OFF; when it's ON the body is a bare User
    /// object with no tokens — `.confirmationRequired`.
    enum SignUpOutcome: Sendable {
        case session(Session)
        case confirmationRequired
    }

    enum AuthError: LocalizedError {
        case badConfig
        case server(Int, String)
        case decoding

        var errorDescription: String? {
            switch self {
            case .badConfig: return "Supabase URL or anon key is missing/invalid."
            case .server(let code, let msg):
                return "Supabase \(code): \(msg.isEmpty ? "request failed" : msg)"
            case .decoding: return "Could not read the Supabase response."
            }
        }
    }

    private func endpoint(_ path: String) throws -> URL {
        let trimmed = projectURL.trimmingCharacters(in: CharacterSet(charactersIn: " /"))
        guard !trimmed.isEmpty, !anonKey.isEmpty,
              let u = URL(string: "\(trimmed)/auth/v1/\(path)") else {
            throw AuthError.badConfig
        }
        return u
    }

    func signIn(email: String, password: String) async throws -> Session {
        let data = try await send(path: "token?grant_type=password",
                                  body: ["email": email, "password": password])
        return try decodeSession(data)
    }

    /// `/signup` yields a Session only when email-confirmation is OFF. When
    /// it's ON, GoTrue replies HTTP 200 with a bare User object (no tokens) —
    /// we report `.confirmationRequired` rather than failing the strict
    /// Session decode and mis-reporting it as a transport error.
    func signUp(email: String, password: String) async throws -> SignUpOutcome {
        let data = try await send(path: "signup",
                                  body: ["email": email, "password": password])
        if let s = try? JSONDecoder().decode(Session.self, from: data),
           !s.access_token.isEmpty {
            return .session(s)
        }
        return .confirmationRequired
    }

    func refresh(refreshToken: String) async throws -> Session {
        let data = try await send(path: "token?grant_type=refresh_token",
                                  body: ["refresh_token": refreshToken])
        return try decodeSession(data)
    }

    /// Redirect the provider returns to. Must match a URL added to the
    /// Supabase project's allowed Redirect URLs and the app's CFBundleURLTypes.
    static let oauthRedirect = "japanosint://auth-callback"
    static let oauthScheme = "japanosint"

    /// Builds the GoTrue `/authorize` URL for a social provider (PKCE).
    func oauthAuthorizeURL(provider: String, codeChallenge: String) throws -> URL {
        let base = try endpoint("authorize")
        guard var comps = URLComponents(url: base, resolvingAgainstBaseURL: false) else {
            throw AuthError.badConfig
        }
        comps.queryItems = [
            .init(name: "provider", value: provider),
            .init(name: "redirect_to", value: Self.oauthRedirect),
            .init(name: "code_challenge", value: codeChallenge),
            .init(name: "code_challenge_method", value: "s256"),
        ]
        guard let u = comps.url else { throw AuthError.badConfig }
        return u
    }

    /// Completes a social sign-in from the provider callback URL. Handles
    /// PKCE (`?code=`) and the implicit (`#access_token=`) fallback.
    func completeOAuth(callback: URL, verifier: String) async throws -> Session {
        let comps = URLComponents(url: callback, resolvingAgainstBaseURL: false)
        let query = comps?.queryItems ?? []
        func q(_ n: String) -> String? { query.first { $0.name == n }?.value }

        if let err = q("error_description") ?? q("error") {
            throw AuthError.server(-1, err.replacingOccurrences(of: "+", with: " "))
        }
        if let code = q("code") {
            let data = try await send(path: "token?grant_type=pkce",
                                      body: ["auth_code": code, "code_verifier": verifier])
            return try decodeSession(data)
        }
        // Implicit fallback: tokens arrive in the URL fragment.
        if let frag = comps?.fragment {
            var f: [String: String] = [:]
            for kv in frag.split(separator: "&") {
                let p = kv.split(separator: "=", maxSplits: 1)
                if p.count == 2 {
                    f[String(p[0])] = String(p[1]).removingPercentEncoding ?? String(p[1])
                }
            }
            if let at = f["access_token"], let rt = f["refresh_token"] {
                return Session(access_token: at, refresh_token: rt,
                               expires_in: f["expires_in"].flatMap { Int($0) },
                               token_type: f["token_type"], user: nil)
            }
        }
        throw AuthError.decoding
    }

    private func decodeSession(_ data: Data) throws -> Session {
        do { return try JSONDecoder().decode(Session.self, from: data) }
        catch { throw AuthError.decoding }
    }

    /// POST to a GoTrue endpoint; returns the raw 2xx body or throws
    /// `.server`/`.badConfig`. Decoding is left to the caller so `/signup`
    /// can tolerate the tokenless User response.
    private func send(path: String, body: [String: String]) async throws -> Data {
        let finalURL = try endpoint(path)
        var req = URLRequest(url: finalURL)
        req.httpMethod = "POST"
        req.timeoutInterval = 20
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue(anonKey, forHTTPHeaderField: "apikey")
        req.httpBody = try JSONSerialization.data(withJSONObject: body)

        let (data, resp) = try await URLSession.shared.data(for: req)
        guard let http = resp as? HTTPURLResponse else {
            throw AuthError.server(-1, "no response")
        }
        guard (200..<300).contains(http.statusCode) else {
            // GoTrue error bodies look like {"error":"...","error_description":"..."}
            let msg = (try? JSONSerialization.jsonObject(with: data) as? [String: Any])
                .flatMap { $0?["error_description"] as? String ?? $0?["msg"] as? String }
                ?? String(data: data, encoding: .utf8) ?? ""
            throw AuthError.server(http.statusCode, msg)
        }
        return data
    }
}
