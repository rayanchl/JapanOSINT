import Foundation

// Codable mirrors of the backend's progress_tracker snapshot + entity API.
// All fields optional/tolerant — the snapshot grows across pipeline phases.

struct SearchStartResponse: Codable, Sendable {
    let request_id: String
    let status: String?
    let query: String?
}

struct SuggestEnvelope: Codable, Sendable { let suggestions: [String] }

struct EntityRef: Codable, Hashable, Sendable, Identifiable {
    let value: String
    let type: String
    let discovered_by: String?
    var id: String { "\(type)|\(value)" }
}

struct ServiceProgress: Codable, Hashable, Sendable, Identifiable {
    let name: String
    let status: String
    let results_count: Int?
    let status_message: String?
    var id: String { name }
}

struct ServiceResult: Codable, Hashable, Sendable, Identifiable {
    let name: String
    let entity: String?
    let success: Bool?
    let confidence: Double?
    /// The server sends `data` as an arbitrary JSON value (often a nested
    /// object), not a string. Decode tolerantly: keep a string as-is,
    /// otherwise re-serialize whatever JSON arrived so a shape change here
    /// can never throw and drop the whole snapshot.
    let data: String?
    let error: String?
    var id: String { "\(name)|\(entity ?? "")" }

    enum CodingKeys: String, CodingKey {
        case name, entity, success, confidence, data, error
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        name = (try? c.decode(String.self, forKey: .name)) ?? ""
        entity = try? c.decode(String.self, forKey: .entity)
        success = try? c.decode(Bool.self, forKey: .success)
        confidence = try? c.decode(Double.self, forKey: .confidence)
        error = try? c.decode(String.self, forKey: .error)
        if let s = try? c.decode(String.self, forKey: .data) {
            data = s
        } else if let raw = try? c.decode(JSONValue.self, forKey: .data) {
            data = raw.jsonString
        } else {
            data = nil
        }
    }
}

/// Minimal tolerant JSON value — lets us accept (and re-serialize) any shape
/// the backend sends for loosely-typed fields without failing the decode.
enum JSONValue: Codable, Hashable, Sendable {
    case string(String), number(Double), bool(Bool)
    case object([String: JSONValue]), array([JSONValue]), null

    init(from decoder: Decoder) throws {
        let c = try decoder.singleValueContainer()
        if c.decodeNil() { self = .null }
        else if let v = try? c.decode(Bool.self) { self = .bool(v) }
        else if let v = try? c.decode(Double.self) { self = .number(v) }
        else if let v = try? c.decode(String.self) { self = .string(v) }
        else if let v = try? c.decode([String: JSONValue].self) { self = .object(v) }
        else if let v = try? c.decode([JSONValue].self) { self = .array(v) }
        else { self = .null }
    }
    func encode(to encoder: Encoder) throws {
        var c = encoder.singleValueContainer()
        switch self {
        case .string(let v): try c.encode(v)
        case .number(let v): try c.encode(v)
        case .bool(let v):   try c.encode(v)
        case .object(let v): try c.encode(v)
        case .array(let v):  try c.encode(v)
        case .null:          try c.encodeNil()
        }
    }
    var jsonString: String {
        guard let d = try? JSONEncoder().encode(self),
              let s = String(data: d, encoding: .utf8) else { return "" }
        return s
    }
}

struct ResultsBlob: Codable, Hashable, Sendable {
    let synthesis: String?
    let services: [ServiceResult]?

    enum CodingKeys: String, CodingKey { case synthesis, services }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        synthesis = try? c.decode(String.self, forKey: .synthesis)
        // Never let a malformed service entry throw away the whole snapshot.
        services = try? c.decode([ServiceResult].self, forKey: .services)
    }
}

struct SearchSnapshot: Codable, Hashable, Sendable {
    let request_id: String
    let query: String?
    let phase: String?
    let progress_percent: Double?
    let gpt_thinking: String?
    let entities: [EntityRef]?
    let discovered_entities: [EntityRef]?
    let services: [ServiceProgress]?
    let current_round: Int?
    let max_rounds: Int?
    let results: ResultsBlob?
    let done: Bool?
}

struct EntityHit: Codable, Hashable, Sendable, Identifiable {
    let entity_id: String
    let type: String
    let value: String
    let mention_count: Int?
    var id: String { entity_id }
}
struct EntitySearchEnvelope: Codable, Sendable { let results: [EntityHit] }

struct EntityProfile: Codable, Hashable, Sendable {
    let entity_id: String
    let type: String
    let value: String
    let name_ja: String?
    let name_romaji: String?
    let aliases: [String]?
    let mention_count: Int?
    let first_seen_at: String?
    let last_seen_at: String?
}

struct EntityNode: Codable, Hashable, Sendable, Identifiable {
    let id: String
    let type: String
    let value: String
    let label: String?
    let mention_count: Int?
}
struct EntityEdge: Codable, Hashable, Sendable {
    let source: String
    let target: String
    let relationship: String
    let weight: Double?
}
struct EntityGraph: Codable, Hashable, Sendable {
    let nodes: [EntityNode]
    let edges: [EntityEdge]
}

struct EntityMention: Codable, Hashable, Sendable, Identifiable {
    let item_uid: String
    let source_id: String?
    let surface: String?
    let field: String?
    let title: String?
    let summary: String?
    let link: String?
    let published_at: String?
    let created_at: String?
    var id: String { "\(item_uid)|\(field ?? "")" }
}
struct MentionsEnvelope: Codable, Sendable { let mentions: [EntityMention] }
