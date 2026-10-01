import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 29 — original / translated / both, for an item carrying a
// `TranslationInfo` (served when the list was fetched with `?lang_view=`).
//
// `machine == true` is the load-bearing bit. A machine translation is a
// MACHINE'S READING of what a source said, not what the source said, and the
// difference decides whether a sentence can be quoted in a report. So the
// machine banner is rendered above the translated text in every mode that
// shows it — including "translated only", where there is no original beside it
// to make the distinction obvious. The engine is named, because "which engine"
// is the first question anyone asks of a suspicious rendering.
//
// Falls back cleanly: with no `TranslationInfo` there is no switch and no
// banner, just the original text plus (optionally) a quiet note that no
// translation exists.
//
// Self-contained: takes the strings and the `TranslationInfo`, owns only the
// selected mode, and navigates nowhere.
// ─────────────────────────────────────────────────────────────────────────────

struct TranslationToggle: View {

    enum DisplayMode: String, CaseIterable, Identifiable, Hashable {
        case original, translated, both
        var id: String { rawValue }
    }

    let originalTitle: String?
    let originalSummary: String?
    let originalBody: String?
    /// `nil` when the item carries no translation — the common case.
    let translation: TranslationInfo?
    /// BCP-47-ish code from the item (`IntelItem.language`), shown next to the
    /// original so "both" mode says which language is which.
    var originalLanguage: String? = nil
    /// Shows a one-line note when there is nothing to translate to. Set false
    /// where the host already explains it.
    var showsUnavailableNote: Bool = true
    /// Fires on every change so a host can persist the analyst's preference.
    var onModeChange: ((DisplayMode) -> Void)? = nil

    @Environment(\.theme) private var theme
    @State private var mode: DisplayMode

    init(originalTitle: String?,
         originalSummary: String?,
         originalBody: String?,
         translation: TranslationInfo?,
         originalLanguage: String? = nil,
         defaultMode: DisplayMode = .original,
         showsUnavailableNote: Bool = true,
         onModeChange: ((DisplayMode) -> Void)? = nil) {
        self.originalTitle = originalTitle
        self.originalSummary = originalSummary
        self.originalBody = originalBody
        self.translation = translation
        self.originalLanguage = originalLanguage
        self.showsUnavailableNote = showsUnavailableNote
        self.onModeChange = onModeChange
        // With no translation the only honest mode is the original.
        _mode = State(initialValue: translation == nil ? .original : defaultMode)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: Space.md) {
            if translation != nil {
                picker
            }
            if showsOriginal {
                originalBlock
            }
            if let t = translation, showsTranslated {
                translatedBlock(t)
            }
            if translation == nil, showsUnavailableNote {
                Text("No translation available for this item.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
    }

    // MARK: - Mode

    private var showsOriginal: Bool {
        translation == nil || mode == .original || mode == .both
    }

    private var showsTranslated: Bool {
        translation != nil && (mode == .translated || mode == .both)
    }

    private var picker: some View {
        Picker("View", selection: $mode) {
            Text("Original").tag(DisplayMode.original)
            Text(targetLanguageName).tag(DisplayMode.translated)
            Text("Both").tag(DisplayMode.both)
        }
        .pickerStyle(.segmented)
        .onChange(of: mode) { _, newValue in
            Haptics.selection()
            onModeChange?(newValue)
        }
        .accessibilityLabel("Text language")
    }

    /// "English" for `en`, falling back to the raw code. Named from the payload
    /// rather than hardcoded, so a `lang_view=fr` item labels itself correctly.
    private var targetLanguageName: String {
        guard let lang = translation?.lang, !lang.isEmpty else { return "Translated" }
        let base = String(lang.prefix(2)).lowercased()
        if let name = Locale.current.localizedString(forLanguageCode: base), !name.isEmpty {
            return name.capitalized
        }
        return lang.uppercased()
    }

    // MARK: - Original

    private var originalBlock: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            if mode == .both, translation != nil {
                sectionLabel(originalLabelText, icon: "text.quote", tone: theme.textMuted)
            }
            if let title = originalTitle, !title.isEmpty {
                JapaneseAware(text: title,
                              font: .headline,
                              foregroundStyle: AnyShapeStyle(theme.text))
            }
            if let text = firstNonEmpty([originalBody, originalSummary]) {
                JapaneseAware(text: text,
                              font: .body,
                              foregroundStyle: AnyShapeStyle(theme.text))
            }
        }
    }

    private var originalLabelText: String {
        guard let lang = originalLanguage, !lang.isEmpty else { return "Original" }
        let base = String(lang.prefix(2)).lowercased()
        if let name = Locale.current.localizedString(forLanguageCode: base), !name.isEmpty {
            return "Original · \(name.capitalized)"
        }
        return "Original · \(lang.uppercased())"
    }

    // MARK: - Translation

    @ViewBuilder
    private func translatedBlock(_ t: TranslationInfo) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            provenanceBanner(t)

            if let title = t.title, !title.isEmpty {
                Text(title)
                    .font(.headline)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if let text = firstNonEmpty([t.body, t.summary]) {
                Text(text)
                    .font(.body)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if t.title == nil && t.summary == nil && t.body == nil {
                Text("The translation carried no text for this item.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
            if t.machine {
                Text("Machine output. Verify against the original before quoting it in a report.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            (t.machine ? theme.warning : theme.accentAlt).opacity(0.10),
            in: RoundedRectangle(cornerRadius: Radius.md)
        )
        .overlay(
            RoundedRectangle(cornerRadius: Radius.md)
                .strokeBorder((t.machine ? theme.warning : theme.accentAlt).opacity(0.35),
                              lineWidth: Stroke.hairline)
        )
    }

    /// Always visible above translated text. Never abbreviated away, never
    /// collapsed behind a disclosure: the claim "this is not the source's own
    /// words" has to travel with the words.
    private func provenanceBanner(_ t: TranslationInfo) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: Space.sm) {
            if t.machine {
                Pill(text: "MACHINE TRANSLATION", tone: .warning,
                     icon: "cpu", maxWidth: 210)
            } else {
                Pill(text: "PUBLISHED TRANSLATION", tone: .info,
                     icon: "text.badge.checkmark", maxWidth: 220)
            }
            Text(engineLine(t))
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)
                .lineLimit(2)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
    }

    private func engineLine(_ t: TranslationInfo) -> String {
        var parts: [String] = []
        if t.machine {
            if let engine = t.engine, !engine.isEmpty {
                parts.append("engine: \(engine)")
            } else {
                parts.append("engine not reported")
            }
        } else {
            parts.append("supplied by the source")
        }
        if let at = t.translated_at, !at.isEmpty {
            parts.append(relativeTime(at))
        }
        return parts.joined(separator: " · ")
    }

    // MARK: - Bits

    private func sectionLabel(_ text: String, icon: String, tone: Color) -> some View {
        HStack(spacing: Space.xs) {
            Image(systemName: icon)
                .font(.caption2)
            Text(text)
                .font(.caption2.weight(.semibold))
        }
        .foregroundStyle(tone)
    }

    private func firstNonEmpty(_ candidates: [String?]) -> String? {
        for c in candidates {
            if let c, !c.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                return c
            }
        }
        return nil
    }
}
