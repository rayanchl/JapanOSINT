import SwiftUI

/// Compact search field for the map's top bar. State + networking live in
/// `GeocodeSearchModel`; the floating dropdown is rendered by
/// `GeocodeSearchResultsDropdown` at the search-row level so it can lay out
/// at full width instead of being constrained to this field's narrow column.
struct GeocodeSearchBar: View {
    @ObservedObject var model: GeocodeSearchModel
    /// Triggered on submit (return key) and as the debounced search target.
    /// Caller wires `model.runSearch(api:featureSearch:)` here so the model
    /// stays UI-agnostic.
    let onSubmit: () -> Void
    /// Wired into the debounce path so each keystroke schedules a search via
    /// the same closure shape used by `onSubmit`.
    let onChange: (String) -> Void

    var body: some View {
        VStack(spacing: 4) {
            field
            if model.bilingual.hasTranslation {
                translationChip
            }
        }
        .modifier(BilingualSearchModifier(query: model.query, bilingual: $model.bilingual))
        .onChange(of: model.bilingual.translated) { _, _ in
            // Re-fire search once translation lands so the second-half hits
            // appear without the user needing to type again.
            guard !model.query.isEmpty else { return }
            onChange(model.query)
        }
    }

    private var field: some View {
        HStack(spacing: 6) {
            Image(systemName: "magnifyingglass")
                .foregroundStyle(.secondary)
            TextField("Search Japan", text: $model.query)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .submitLabel(.search)
                .onSubmit(onSubmit)
                .onChange(of: model.query) { _, new in
                    onChange(new)
                }
            if model.loading {
                ProgressView().controlSize(.mini)
            } else if !model.query.isEmpty {
                Button {
                    model.clear()
                } label: { Image(systemName: "xmark.circle.fill") }
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.horizontal, 10)
        .frame(height: 36)
        .background(Color(.tertiarySystemFill),
                    in: RoundedRectangle(cornerRadius: 10, style: .continuous))
        .animation(.easeOut(duration: 0.15), value: model.showResults)
    }

    private var translationChip: some View {
        HStack(spacing: 6) {
            Text("Also searching:")
                .font(.caption2)
                .foregroundStyle(.secondary)
            Text(model.bilingual.translated ?? "")
                .font(.system(.caption, design: .monospaced))
                .lineLimit(1)
            BilingualBadge(style: .full)
            Spacer(minLength: 0)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 3)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color(.tertiarySystemFill).opacity(0.6),
                    in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .transition(.opacity.combined(with: .move(edge: .top)))
    }
}
