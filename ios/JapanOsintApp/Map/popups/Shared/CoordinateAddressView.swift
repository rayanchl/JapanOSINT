import SwiftUI
import CoreLocation

/// Three-line address/coordinate stack:
///   1. Japanese reverse-geocoded address (Nominatim, accept-language=ja)
///   2. English reverse-geocoded address (Nominatim, accept-language=en)
///   3. Numeric coordinates (always shown — the source of truth)
///
/// The two address rows fall through to a dash if their lookup failed; the
/// coordinate row is always available immediately and never blocks on the
/// network.
struct CoordinateAddressView: View {
    let coordinate: CLLocationCoordinate2D

    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme

    @State private var addressJa: String?
    @State private var addressEn: String?
    @State private var loading = true

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            addressRow(addressJa, isJapanese: true)
            addressRow(addressEn, isJapanese: false)
            Text(String(format: "%.5f, %.5f", coordinate.latitude, coordinate.longitude))
                .font(.system(.caption, design: .monospaced))
                .foregroundStyle(theme.textMuted)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .task(id: "\(coordinate.latitude),\(coordinate.longitude)") {
            await resolve()
        }
    }

    @ViewBuilder
    private func addressRow(_ value: String?, isJapanese: Bool) -> some View {
        if loading {
            HStack(spacing: 4) {
                ProgressView().controlSize(.mini)
                Text(isJapanese ? "Resolving 日本語…" : "Resolving English…")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            }
        } else if let value, !value.isEmpty {
            Text(value)
                .font(.caption)
                .foregroundStyle(theme.text)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
        } else {
            Text("—")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
        }
    }

    private func resolve() async {
        loading = true
        defer { loading = false }
        do {
            let r = try await API(baseURL: settings.backendBaseURL)
                .reverseGeocode(lat: coordinate.latitude, lon: coordinate.longitude)
            // JA: prefer the localised JA string; if the provider didn't
            // expose a language pair (Photon/GSI fallback) the canonical
            // `display_name` is the best guess and is usually JA-leaning.
            addressJa = r.display_name_ja ?? r.display_name
            // EN: only show when we genuinely got an English string back.
            // Falling back to `display_name` here would just print the JA
            // address a second time.
            addressEn = r.display_name_en
        } catch {
            addressJa = nil
            addressEn = nil
        }
    }
}
