import Foundation
import Combine

/// State + networking for the map's geocode search bar. Owned by `MapTab` so
/// the input field (`GeocodeSearchBar`) and the floating results overlay
/// (`GeocodeSearchResultsDropdown`) can be hosted separately — the dropdown
/// renders at the *searchRow* level rather than inside the narrow search
/// field, giving the result list the full row width to lay out in.
/// Tagged variant of GeoFeature so the dropdown can mark rows that were
/// only matched by the auto-translated query. The original GeoFeature stays
/// in the value so existing tap handlers (flyTo, spotlight) keep working.
struct TaggedFeatureHit: Identifiable {
    let feature: GeoFeature
    let viaTranslation: Bool
    var id: String { "\(feature.id)|\(viaTranslation ? "t" : "o")" }
}

@MainActor
final class GeocodeSearchModel: ObservableObject {
    @Published var query: String = ""
    @Published var hits: [GeocodeHit] = []
    @Published var featureHits: [TaggedFeatureHit] = []
    @Published var loading: Bool = false
    @Published var showResults: Bool = false
    /// Snapshot of the bilingual query for the in-flight search. Written by
    /// `BilingualSearchModifier` (via the view that owns this model) and
    /// consumed by `runSearch`. Drives the "Also searching" chip in the bar.
    @Published var bilingual: BilingualQuery = .empty

    private var debounceTask: Task<Void, Never>?

    /// Cancels any pending debounce, clears local + remote hits, hides the
    /// dropdown. Used by the input field's clear button.
    func clear() {
        debounceTask?.cancel()
        query = ""
        hits = []
        featureHits = []
        showResults = false
        bilingual = .empty
    }

    /// 300ms-debounced wrapper around `runSearch`. Empty queries hide results
    /// immediately (no debounce) so the dropdown closes the moment the user
    /// backspaces to nothing.
    func debounce(_ text: String,
                  api: @escaping () -> API,
                  featureSearch: @escaping (String) -> [GeoFeature]) {
        debounceTask?.cancel()
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        if trimmed.isEmpty {
            hits = []
            featureHits = []
            showResults = false
            return
        }
        debounceTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: 300_000_000)
            guard let self, !Task.isCancelled else { return }
            await self.runSearch(api: api(), featureSearch: featureSearch)
        }
    }

    /// Local active-layer search runs first (synchronous + cheap) so results
    /// surface before the geocode round-trip completes. Geocode failures are
    /// swallowed silently — the dropdown just keeps whatever feature hits it
    /// already has.
    ///
    /// Bilingual: when `bilingual.translated` is set, local feature search
    /// runs against both strings and dedupes by feature id; the geocode
    /// request sends both to the backend (`q`, `qAlt`) in one round-trip.
    func runSearch(api: API, featureSearch: (String) -> [GeoFeature]) async {
        let trimmed = query.trimmingCharacters(in: .whitespaces)
        guard !trimmed.isEmpty else { return }

        // Local feature hits — dedup by feature id, original-query first.
        var seenIds = Set<String>()
        var tagged: [TaggedFeatureHit] = []
        for f in featureSearch(trimmed) where !seenIds.contains(f.id) {
            seenIds.insert(f.id)
            tagged.append(TaggedFeatureHit(feature: f, viaTranslation: false))
        }
        if let alt = bilingual.translated, alt != trimmed {
            for f in featureSearch(alt) where !seenIds.contains(f.id) {
                seenIds.insert(f.id)
                tagged.append(TaggedFeatureHit(feature: f, viaTranslation: true))
            }
        }
        featureHits = tagged
        if !tagged.isEmpty { showResults = true }

        loading = true
        defer { loading = false }
        do {
            let resp = try await api.geocode(query: trimmed, queryAlt: bilingual.translated)
            hits = resp.results
            showResults = !hits.isEmpty || !featureHits.isEmpty
        } catch {
            hits = []
            showResults = !featureHits.isEmpty
        }
    }
}
