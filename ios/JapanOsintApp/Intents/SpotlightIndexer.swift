import Foundation
import CoreSpotlight
import UniformTypeIdentifiers

// ─────────────────────────────────────────────────────────────────────────────
// CoreSpotlight indexing for cases and saved items (roadmap 36).
//
// Privacy is enforced by an EXPLICIT allowlist, not by reflecting over the
// model: only user-authored titles/names (and a case summary) are indexed.
// Annotation bodies and breach identifiers are never written to the on-device
// index — a naive "index everything" is how sensitive OSINT material leaks into
// a system index we do not control. A new field is out until it is added here.
//
// A hit deep-links back via its `uniqueIdentifier` ("<domain>:<id>"); the shell
// handles `CSSearchableItemActionType` in `onContinueUserActivity`.
// ─────────────────────────────────────────────────────────────────────────────

enum SpotlightIndexer {
    static let caseDomain  = "case"
    static let savedDomain = "saved"

    /// Identifier form used both when indexing and when routing a hit.
    static func identifier(domain: String, id: String) -> String { "\(domain):\(id)" }

    /// Split a hit's `uniqueIdentifier` back into (domain, id).
    static func route(_ uniqueId: String) -> (domain: String, id: String)? {
        guard let sep = uniqueId.firstIndex(of: ":") else { return nil }
        let domain = String(uniqueId[..<sep])
        let id = String(uniqueId[uniqueId.index(after: sep)...])
        guard !domain.isEmpty, !id.isEmpty else { return nil }
        return (domain, id)
    }

    /// Replace the index for cases + saved items. Cheap enough to call after
    /// auth resolves and whenever either set changes.
    static func index(cases: [CaseSummary], saved: [SavedItem]) {
        var items: [CSSearchableItem] = []

        for c in cases {
            let attr = CSSearchableItemAttributeSet(contentType: UTType.text)
            attr.title = c.name
            attr.contentDescription = c.summary   // author-written, allowlisted
            items.append(CSSearchableItem(
                uniqueIdentifier: identifier(domain: caseDomain, id: c.id),
                domainIdentifier: caseDomain,
                attributeSet: attr))
        }

        for s in saved {
            let attr = CSSearchableItemAttributeSet(contentType: UTType.text)
            attr.title = s.displayName
            items.append(CSSearchableItem(
                uniqueIdentifier: identifier(domain: savedDomain, id: s.id),
                domainIdentifier: savedDomain,
                attributeSet: attr))
        }

        // Delete each domain first so removed cases/items don't linger, then
        // re-add the current set. deleteAll for our domains keeps this from
        // touching anything else's index.
        let index = CSSearchableIndex.default()
        index.deleteSearchableItems(withDomainIdentifiers: [caseDomain, savedDomain]) { _ in
            index.indexSearchableItems(items) { _ in }
        }
    }

    static func clear() {
        CSSearchableIndex.default().deleteSearchableItems(
            withDomainIdentifiers: [caseDomain, savedDomain]) { _ in }
    }
}
