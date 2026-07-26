import Foundation

/// Baked-in production configuration for zero-paste onboarding.
///
/// These are the managed-service defaults `AppPreferences` seeds itself with,
/// so a fresh install reaches the onboarding "Connect" page with every field
/// pre-filled and the user can continue without typing anything. The fields
/// stay visible/editable so self-host users can point at their own backend.
///
/// The Supabase *anon* key is a publishable client key (not a secret), so it
/// is safe to ship in the app bundle — same as it would be in any web client.
///
// TODO: replace before release — production values supplied by the team.
enum BuildConfig {
    /// Managed backend base URL, e.g. https://api.japanosint.app
    ///
    /// Dev default differs by platform: the Mac app runs the `japanosint`
    /// backend on the *same* machine, so it points at loopback; iOS devices
    /// reach the dev Mac across the LAN. Use the Mac's Bonjour `.local`
    /// hostname instead of a raw IP so it survives DHCP lease changes
    /// (resolves on-LAN via mDNS; covered by Info.plist NSAllowsLocalNetworking).
    /// Both stay editable on the onboarding "Connect" page / in Settings for
    /// self-host + real backends.
    static let backendBaseURL: String = {
        #if os(macOS)
        return "http://127.0.0.1:4000"
        #else
        return "http://Rayans-MacBook-Pro.local:4000"
        #endif
    }()

    /// Supabase project URL, e.g. https://abcd.supabase.co
    static let supabaseURL = "https://cbdrdmmlgzqthvhcxnld.supabase.co"

    /// Supabase publishable anon key (client-safe, not a secret).
    static let supabaseAnonKey = "sb_publishable_nzjWkrp7sMNxrU0jyuUbMQ_Q9lAVFL_"
}
