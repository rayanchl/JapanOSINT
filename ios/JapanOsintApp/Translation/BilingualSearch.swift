import SwiftUI
import Translation

/// Direction of the auto-translation. `.enToJa` means the user typed
/// English and we translated to Japanese; `.jaToEn` is the reverse.
enum BilingualDirection: Equatable {
    case enToJa
    case jaToEn

    var sourceLanguage: Locale.Language {
        switch self {
        case .enToJa: return Locale.Language(identifier: "en")
        case .jaToEn: return Locale.Language(identifier: "ja")
        }
    }
    var targetLanguage: Locale.Language {
        switch self {
        case .enToJa: return Locale.Language(identifier: "ja")
        case .jaToEn: return Locale.Language(identifier: "en")
        }
    }
}

/// Snapshot of "what's the user searching for, and what did we translate it to".
/// Both halves are sent to the backend's bilingual endpoint in one request.
struct BilingualQuery: Equatable {
    let original: String
    let translated: String?
    let direction: BilingualDirection?

    static let empty = BilingualQuery(original: "", translated: nil, direction: nil)

    /// True when there's a meaningful second query to issue alongside the original.
    var hasTranslation: Bool {
        guard let t = translated else { return false }
        let trimmed = t.trimmingCharacters(in: .whitespaces)
        return !trimmed.isEmpty && trimmed.caseInsensitiveCompare(original) != .orderedSame
    }
}

/// SwiftUI modifier that watches `query`, decides translation direction via
/// `isJapanese(_:)`, and writes the translated string back through `bilingual`.
/// Apple's TranslationSession is view-attached (via `.translationTask`), so
/// every consumer of this feature attaches the modifier to its own view.
///
/// Honors `AppSettings.autoTranslateSearch` — when off, `bilingual.translated`
/// stays nil and the backend gets a single-query request (back-compat).
struct BilingualSearchModifier: ViewModifier {
    let query: String
    @Binding var bilingual: BilingualQuery

    @EnvironmentObject var settings: AppSettings
    @State private var config: TranslationSession.Configuration?
    @State private var lastTranslatedFor: String? = nil

    func body(content: Content) -> some View {
        content
            .onChange(of: query) { _, newValue in
                refreshConfig(for: newValue)
            }
            .onChange(of: settings.autoTranslateSearch) { _, enabled in
                if !enabled {
                    bilingual = BilingualQuery(original: query, translated: nil, direction: nil)
                    config = nil
                    lastTranslatedFor = nil
                } else {
                    refreshConfig(for: query)
                }
            }
            .onAppear { refreshConfig(for: query) }
            .translationTask(config) { session in
                let trimmed = query.trimmingCharacters(in: .whitespaces)
                guard
                    settings.autoTranslateSearch,
                    !trimmed.isEmpty,
                    trimmed.count >= 2
                else { return }
                do {
                    let response = try await session.translate(trimmed)
                    let translated = response.targetText
                        .trimmingCharacters(in: .whitespaces)
                    let useful = !translated.isEmpty
                        && translated.caseInsensitiveCompare(trimmed) != .orderedSame
                    await MainActor.run {
                        bilingual = BilingualQuery(
                            original: trimmed,
                            translated: useful ? translated : nil,
                            direction: isJapanese(trimmed) ? .jaToEn : .enToJa
                        )
                        lastTranslatedFor = trimmed
                    }
                } catch {
                    // Silent — Apple surfaces its own download prompt for
                    // missing language packs. Result: bilingual.translated
                    // stays nil and the search runs single-query.
                    await MainActor.run {
                        bilingual = BilingualQuery(
                            original: trimmed,
                            translated: nil,
                            direction: nil
                        )
                    }
                }
            }
    }

    /// Decide direction, rebuild (or invalidate) the configuration so
    /// `.translationTask` fires for the new query. `invalidate()` is Apple's
    /// documented re-trigger — reassigning an equal config is a no-op.
    private func refreshConfig(for value: String) {
        let trimmed = value.trimmingCharacters(in: .whitespaces)
        guard settings.autoTranslateSearch, trimmed.count >= 2 else {
            bilingual = BilingualQuery(original: trimmed, translated: nil, direction: nil)
            config = nil
            lastTranslatedFor = nil
            return
        }
        // Eagerly publish the original so consumers can issue the single-query
        // request before translation resolves. translated is cleared until the
        // session callback fills it in.
        bilingual = BilingualQuery(
            original: trimmed,
            translated: nil,
            direction: isJapanese(trimmed) ? .jaToEn : .enToJa
        )
        let direction: BilingualDirection = isJapanese(trimmed) ? .jaToEn : .enToJa
        let nextSource = direction.sourceLanguage
        let nextTarget = direction.targetLanguage
        // Apple's re-trigger contract: when the config's source/target are
        // already correct, calling invalidate() through the @State binding
        // re-fires .translationTask without recreating the configuration.
        // Reassigning an equal-valued Configuration is a no-op (value
        // equality), so a fresh translate would otherwise never run.
        if let existing = config,
           existing.source == nextSource,
           existing.target == nextTarget {
            config?.invalidate()
        } else {
            config = TranslationSession.Configuration(
                source: nextSource,
                target: nextTarget
            )
        }
        _ = lastTranslatedFor  // silence unused-write warning on debug builds
    }
}

/// Visual indicator that a row/section came from Apple's auto-translation.
/// `.full` is the chip used in the header banner below the search input;
/// `.compact` is the inline marker next to individual translated rows.
struct BilingualBadge: View {
    enum Style { case full, compact }
    var style: Style = .full
    @Environment(\.theme) private var theme

    var body: some View {
        switch style {
        case .full:
            HStack(spacing: 4) {
                Image(systemName: "globe")
                    .font(.system(size: 10, weight: .semibold))
                Text("Apple Translated")
                    .font(.caption2.weight(.semibold))
            }
            .foregroundStyle(theme.textMuted)
            .padding(.horizontal, 6)
            .padding(.vertical, 2)
            .background(theme.surface, in: Capsule())
            .overlay(Capsule().strokeBorder(theme.textMuted.opacity(0.25), lineWidth: 0.5))
        case .compact:
            HStack(spacing: 3) {
                Image(systemName: "globe")
                    .font(.system(size: 9, weight: .semibold))
                Text("translated")
                    .font(.caption2)
            }
            .foregroundStyle(theme.textMuted)
            .padding(.horizontal, 5)
            .padding(.vertical, 1)
            .background(theme.surfaceElevated, in: Capsule())
        }
    }
}
