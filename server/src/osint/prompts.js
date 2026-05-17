// server/src/osint/prompts.js
//
// Verbatim JS port of the tuned OSINTsaas prompt builders
// (backend/src/llama/llama_client.c + llama_analyzer.c). The strings are
// copied byte-for-byte — these prompts were heavily tuned and must not drift.
// Structure is enforced by the GBNF grammars in server/grammars/ via
// llmClient.complete (native llama.cpp /completion + `grammar`).
//
// Reference (read-only): /Users/rayan/OSINTsaas/backend/src/llama/llama_client.c

// ENTITY_TYPES_PROMPT — verbatim from backend/include/entity_types.h
const ENTITY_TYPES_PROMPT =
  'Entity types to identify:\n' +
  '- CORE: email, ip, domain, phone, username, person, company, address, url\n' +
  '- TRANSPORT: vehicle, flight, vessel, container\n' +
  '- FINANCIAL: crypto, iban, swift, credit_card, bank_account\n' +
  '- NETWORK: dns, asn, mac, subnet, port\n' +
  '- LOCATION: coordinates, timezone\n' +
  '- IDENTITY: ssn, passport, license, national_id, tax_id\n' +
  '- TECHNICAL: hash, cve, api_key, jwt, uuid, file_path, registry\n' +
  '- DEVICE: imei, imsi, serial, barcode, qr\n' +
  '- OTHER: image, keyword, unknown\n';

/**
 * Phase 1 analysis prompt. Verbatim port of
 * llama_client_create_analysis_prompt(). `servicesList` is the categorised
 * service list from dispatcher.getServicesList().
 */
export function createAnalysisPrompt(query, servicesList) {
  const list = servicesList || '(service list unavailable)';
  return (
    `Extract OSINT entities from: ${query}\n\n` +
    'CRITICAL: Output compact JSON on a SINGLE LINE with NO extra whitespace, newlines, or formatting.\n\n' +
    'CRITICAL: The entity VALUE must contain ONLY the identifier itself, NEVER context words!\n' +
    '- WRONG: {"value": "weather at location of ip adress 43.45.3.23", "type": "ip"}\n' +
    '- RIGHT: {"value": "43.45.3.23", "type": "ip"}\n' +
    '- WRONG: {"value": "find info on john@test.com", "type": "email"}\n' +
    '- RIGHT: {"value": "john@test.com", "type": "email"}\n\n' +
    'NOTE: Ignore spelling errors in queries (e.g., "adress" = "address", "emal" = "email").\n' +
    'Extract the actual identifier regardless of surrounding typos or casual language.\n\n' +
    'CRITICAL: Extract ALL entities found in the query. If query contains BOTH a person name AND an IP address, you MUST return BOTH in the entities array.\n' +
    'CRITICAL: Comma-separated items are SEPARATE entities. Each item between commas should be its own entity.\n' +
    'IP addresses are ANY dotted decimal format (e.g., 8.8.8.8, 45.33.32.1, 192.168.1.1).\n' +
    'License plates can be alphanumeric (e.g., ABC123, 5334DE2434, XYZ789). Extract the plate code, not the vehicle description.\n\n' +
    'PHONE vs IP DISTINCTION (CRITICAL):\n' +
    '- IP addresses ALWAYS have exactly 4 dot-separated octets (e.g., 8.8.8.8, 192.168.1.1, 10.0.0.1)\n' +
    '- Phone numbers are 9-15 CONTINUOUS digits, may start with 0 or +, NO dots between digits\n' +
    '- Number with NO dots and 9+ digits = PHONE, not IP\n' +
    '- Examples: 0781218793 = PHONE, 078.121.87.93 = IP\n\n' +
    "CONTEXT WORDS ARE NOT ENTITIES: Words like 'weather', 'map', 'nearby', 'location', 'shodan', 'search', 'lookup', 'find', 'carrier' are CONTEXT, not entities.\n" +
    'Do NOT create keyword entities for these context words.\n' +
    "For 'weather at IP X' queries: Output ONLY the IP entity with IP_GEOLOCATION, set chain_reason:true.\n" +
    'Phase 2 will extract coordinates and call weather services.\n\n' +
    ENTITY_TYPES_PROMPT +
    'Output JSON with ALL entities found using the types above.\n\n' +
    '=== CHAIN_REASON DECISION (REQUIRED BOOLEAN) ===\n' +
    'chain_reason is REQUIRED and must be true or false:\n' +
    'Set chain_reason: true when Phase 2 follow-up investigation is valuable:\n' +
    '- Weather/map/nearby queries + IP: true (coords enable WEATHER_SERVICE, map services)\n' +
    '- Person/company queries: true (may reveal employees, accounts, employers)\n' +
    '- Email breach queries: true (may reveal linked accounts, employers)\n' +
    "- Domain WHOIS 'who owns' queries: true (may reveal owner name/company)\n" +
    '- Vehicle/phone lookups: true (may reveal owner name)\n' +
    '- Flight queries: true (may reveal airports for geolocation)\n\n' +
    'Set chain_reason: false for simple informational queries:\n' +
    "- 'what is X' or 'lookup X': false (purely informational)\n" +
    '- Hash/CVE lookups: false (technical data, no personal info)\n' +
    '- Simple DNS record queries: false (no personal data to chain)\n\n' +
    `Available services:\n${list}\n\n` +
    'Examples (chain_reason: true - follow-up valuable):\n' +
    'Query: weather at 1.2.3.4\n' +
    '{"entities":[{"value":"1.2.3.4","type":"ip","confidence":"high","services":["IP_GEOLOCATION"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP for weather","chain_reason":true}\n\n' +
    'Query: shodan search 8.8.8.8\n' +
    '{"entities":[{"value":"8.8.8.8","type":"ip","confidence":"high","services":["SHODAN_SEARCH"]}],"recommended_services":["SHODAN_SEARCH"],"complexity":"low","analysis":"IP for Shodan","chain_reason":false}\n\n' +
    'Query: john@example.com\n' +
    '{"entities":[{"value":"john@example.com","type":"email","confidence":"high","services":["BREACH_CHECKER","EMAIL_REPUTATION"]}],"recommended_services":["BREACH_CHECKER"],"complexity":"low","analysis":"Email found","chain_reason":true}\n\n' +
    'Query: 8.8.8.8\n' +
    '{"entities":[{"value":"8.8.8.8","type":"ip","confidence":"high","services":["IP_GEOLOCATION","REVERSE_DNS","THREAT_INTEL"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"Public IP address","chain_reason":true}\n\n' +
    'Query: 45.33.32.1 WHOIS lookup\n' +
    '{"entities":[{"value":"45.33.32.1","type":"ip","confidence":"high","services":["IP_GEOLOCATION","DOMAIN_WHOIS","ASN_LOOKUP"]}],"recommended_services":["IP_GEOLOCATION","DOMAIN_WHOIS"],"complexity":"low","analysis":"IP with WHOIS request","chain_reason":true}\n\n' +
    'Query: John Smith\n' +
    '{"entities":[{"value":"John Smith","type":"person","confidence":"high","services":["PERSON_SEARCH","PEOPLE_FINDER","LINKEDIN_LOOKUP"]}],"recommended_services":["PERSON_SEARCH"],"complexity":"low","analysis":"Person name found","chain_reason":true}\n\n' +
    'Query: red sedan plate ABC123\n' +
    '{"entities":[{"value":"ABC123","type":"vehicle","confidence":"high","services":["LICENSE_PLATE_LOOKUP","VEHICLE_LOOKUP"]}],"recommended_services":["LICENSE_PLATE_LOOKUP"],"complexity":"low","analysis":"License plate found","chain_reason":true}\n\n' +
    'Query: blue truck plate 5334DE2434\n' +
    '{"entities":[{"value":"5334DE2434","type":"vehicle","confidence":"high","services":["LICENSE_PLATE_LOOKUP","VEHICLE_LOOKUP"]}],"recommended_services":["LICENSE_PLATE_LOOKUP"],"complexity":"low","analysis":"Alphanumeric plate","chain_reason":true}\n\n' +
    'Query: +1 555-123-4567\n' +
    '{"entities":[{"value":"+1 555-123-4567","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["PHONE_LOOKUP"],"complexity":"low","analysis":"Phone number","chain_reason":true}\n\n' +
    'Query: 0893847546\n' +
    '{"entities":[{"value":"0893847546","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["PHONE_LOOKUP"],"complexity":"low","analysis":"Phone number","chain_reason":true}\n\n' +
    'Query: phone number 0781218793\n' +
    '{"entities":[{"value":"0781218793","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["PHONE_LOOKUP"],"complexity":"low","analysis":"Phone number extracted","chain_reason":true}\n\n' +
    'Query: call this number 0612345678\n' +
    '{"entities":[{"value":"0612345678","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["PHONE_LOOKUP"],"complexity":"low","analysis":"Phone extracted from context","chain_reason":true}\n\n' +
    'Query: phone 0781218793\n' +
    '{"entities":[{"value":"0781218793","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["PHONE_LOOKUP"],"complexity":"low","analysis":"Phone number","chain_reason":true}\n\n' +
    'Query: my phone is 06 12 34 56 78\n' +
    '{"entities":[{"value":"0612345678","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["PHONE_LOOKUP"],"complexity":"low","analysis":"Phone with spaces normalized","chain_reason":true}\n\n' +
    'Query: 0781218793 carrier lookup\n' +
    '{"entities":[{"value":"0781218793","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["CARRIER_LOOKUP"],"complexity":"low","analysis":"Phone carrier lookup","chain_reason":true}\n\n' +
    'Query: carrier for 0612345678\n' +
    '{"entities":[{"value":"0612345678","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["CARRIER_LOOKUP"],"complexity":"low","analysis":"Phone carrier query","chain_reason":true}\n\n' +
    'Query: lookup carrier 0893847546\n' +
    '{"entities":[{"value":"0893847546","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["CARRIER_LOOKUP"],"complexity":"low","analysis":"Carrier lookup request","chain_reason":true}\n\n' +
    'Query: john smith, jane doe, mike wilson\n' +
    '{"entities":[{"value":"john smith","type":"person","confidence":"high","services":["PERSON_SEARCH","PEOPLE_FINDER"]},{"value":"jane doe","type":"person","confidence":"high","services":["PERSON_SEARCH","PEOPLE_FINDER"]},{"value":"mike wilson","type":"person","confidence":"high","services":["PERSON_SEARCH","PEOPLE_FINDER"]}],"recommended_services":["PERSON_SEARCH"],"complexity":"medium","analysis":"Multiple persons comma-separated","chain_reason":true}\n\n' +
    'Query: john barker, 0782327674, red sedan plate 5334de2434, johb isabella\n' +
    '{"entities":[{"value":"john barker","type":"person","confidence":"high","services":["PERSON_SEARCH","PEOPLE_FINDER"]},{"value":"0782327674","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]},{"value":"5334de2434","type":"vehicle","confidence":"high","services":["LICENSE_PLATE_LOOKUP","VEHICLE_LOOKUP"]},{"value":"johb isabella","type":"person","confidence":"high","services":["PERSON_SEARCH","PEOPLE_FINDER"]}],"recommended_services":["PERSON_SEARCH","PHONE_LOOKUP","LICENSE_PLATE_LOOKUP"],"complexity":"high","analysis":"Multiple entities","chain_reason":true}\n\n' +
    'Query: john_user and 0612345678\n' +
    '{"entities":[{"value":"john_user","type":"username","confidence":"high","services":["SHERLOCK_SEARCH","WHATSMYNAME_SEARCH"]},{"value":"0612345678","type":"phone","confidence":"high","services":["PHONE_LOOKUP","CARRIER_LOOKUP"]}],"recommended_services":["SHERLOCK_SEARCH","PHONE_LOOKUP"],"complexity":"medium","analysis":"Username and phone","chain_reason":true}\n\n' +
    'Query: John Doe and 192.168.1.1\n' +
    '{"entities":[{"value":"John Doe","type":"person","confidence":"high","services":["PERSON_SEARCH","PEOPLE_FINDER"]},{"value":"192.168.1.1","type":"ip","confidence":"high","services":["IP_GEOLOCATION","REVERSE_DNS"]}],"recommended_services":["PERSON_SEARCH","IP_GEOLOCATION"],"complexity":"medium","analysis":"Person and IP","chain_reason":true}\n\n' +
    'Query: weather at 34.3.3.54 location\n' +
    '{"entities":[{"value":"34.3.3.54","type":"ip","confidence":"high","services":["IP_GEOLOCATION","REVERSE_DNS"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP for weather lookup","chain_reason":true}\n\n' +
    'Query: flight AA123\n' +
    '{"entities":[{"value":"AA123","type":"flight","confidence":"high","services":["FLIGHT_TRACKER"]}],"recommended_services":["FLIGHT_TRACKER"],"complexity":"low","analysis":"Flight number","chain_reason":true}\n\n' +
    'Query: find info about server 10.0.0.1\n' +
    '{"entities":[{"value":"10.0.0.1","type":"ip","confidence":"high","services":["IP_GEOLOCATION","REVERSE_DNS","ASN_LOOKUP"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP address extraction","chain_reason":true}\n\n' +
    'Query: weather at 45.33.32.1\n' +
    '{"entities":[{"value":"45.33.32.1","type":"ip","confidence":"high","services":["IP_GEOLOCATION"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP for weather location","chain_reason":true}\n\n' +
    'Query: map location of IP 192.168.1.1\n' +
    '{"entities":[{"value":"192.168.1.1","type":"ip","confidence":"high","services":["IP_GEOLOCATION"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP for map","chain_reason":true}\n\n' +
    'Query: nearby places around IP 8.8.4.4\n' +
    '{"entities":[{"value":"8.8.4.4","type":"ip","confidence":"high","services":["IP_GEOLOCATION"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP for nearby search","chain_reason":true}\n\n' +
    'Query: who owns example.com\n' +
    '{"entities":[{"value":"example.com","type":"domain","confidence":"high","services":["DOMAIN_WHOIS","DNS_RECORDS"]}],"recommended_services":["DOMAIN_WHOIS"],"complexity":"low","analysis":"Domain ownership query","chain_reason":true}\n\n' +
    'Query: employees at TechCorp Inc\n' +
    '{"entities":[{"value":"TechCorp Inc","type":"company","confidence":"high","services":["COMPANY_LOOKUP","OPENCORPORATES_SEARCH"]}],"recommended_services":["COMPANY_LOOKUP"],"complexity":"low","analysis":"Company employee search","chain_reason":true}\n\n' +
    'Query: breach history of john@corp.com\n' +
    '{"entities":[{"value":"john@corp.com","type":"email","confidence":"high","services":["BREACH_CHECKER","DEHASHED_SEARCH"]}],"recommended_services":["BREACH_CHECKER"],"complexity":"low","analysis":"Email breach check","chain_reason":true}\n\n' +
    'Examples (chain_reason: false - no follow-up needed):\n' +
    'Query: what is 8.8.8.8\n' +
    '{"entities":[{"value":"8.8.8.8","type":"ip","confidence":"high","services":["IP_GEOLOCATION","REVERSE_DNS"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"Simple IP lookup","chain_reason":false}\n\n' +
    'Query: lookup domain google.com\n' +
    '{"entities":[{"value":"google.com","type":"domain","confidence":"high","services":["DOMAIN_WHOIS","DNS_RECORDS"]}],"recommended_services":["DOMAIN_WHOIS"],"complexity":"low","analysis":"Domain info request","chain_reason":false}\n\n' +
    'Query: dns records for example.org\n' +
    '{"entities":[{"value":"example.org","type":"domain","confidence":"high","services":["DNS_RECORDS"]}],"recommended_services":["DNS_RECORDS"],"complexity":"low","analysis":"DNS query","chain_reason":false}\n\n' +
    'Query: hash sha256 abc123def456\n' +
    '{"entities":[{"value":"abc123def456","type":"hash","confidence":"high","services":["HASH_LOOKUP","MALWARE_ANALYSIS"]}],"recommended_services":["HASH_LOOKUP"],"complexity":"low","analysis":"Hash lookup","chain_reason":false}\n\n' +
    'Examples with typos/casual language (extract identifier ONLY, ignore surrounding words):\n' +
    'Query: weather at location of ip adress 43.45.3.23\n' +
    '{"entities":[{"value":"43.45.3.23","type":"ip","confidence":"high","services":["IP_GEOLOCATION"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP for weather","chain_reason":true}\n\n' +
    'Query: can u find info on the email addy john.doe@gmail.com please\n' +
    '{"entities":[{"value":"john.doe@gmail.com","type":"email","confidence":"high","services":["BREACH_CHECKER","EMAIL_REPUTATION"]}],"recommended_services":["BREACH_CHECKER"],"complexity":"low","analysis":"Email lookup","chain_reason":true}\n\n' +
    'Query: whats the location of this ip 192.168.1.1 and weather there\n' +
    '{"entities":[{"value":"192.168.1.1","type":"ip","confidence":"high","services":["IP_GEOLOCATION"]}],"recommended_services":["IP_GEOLOCATION"],"complexity":"low","analysis":"IP for location and weather","chain_reason":true}\n\n' +
    'Query: pls lookup this domain google.co.uk thx\n' +
    '{"entities":[{"value":"google.co.uk","type":"domain","confidence":"high","services":["DOMAIN_WHOIS","DNS_RECORDS"]}],"recommended_services":["DOMAIN_WHOIS"],"complexity":"low","analysis":"Domain lookup","chain_reason":false}\n\n' +
    'Query: check if emal test@corp.net was in any data breaches\n' +
    '{"entities":[{"value":"test@corp.net","type":"email","confidence":"high","services":["BREACH_CHECKER","DEHASHED_SEARCH"]}],"recommended_services":["BREACH_CHECKER"],"complexity":"low","analysis":"Email breach check","chain_reason":true}\n\n' +
    'Now output JSON for the query:\n'
  );
}

/**
 * Suggestions prompt. Verbatim port of
 * llama_client_create_suggestions_prompt(). Used via the chat path
 * (suggestions.gbnf constrains the 9-item array).
 */
export function createSuggestionsPrompt(query) {
  return (
    'You are an OSINT search assistant. Output ONLY a JSON array with 9 search suggestions.\n' +
    'Do not explain or add any text before or after the JSON array.\n\n' +
    'Examples:\n\n' +
    'Query: "John Smith"\n' +
    '["John Smith LinkedIn professional profile","John Smith Facebook social media","John Smith public records court documents","John Smith email breach databases","John Smith company affiliations","John Smith Twitter account","John Smith username search Sherlock","John Smith academic publications","John Smith phone reverse lookup"]\n\n' +
    'Query: "contact@example.com"\n' +
    '["contact@example.com breach HaveIBeenPwned","contact@example.com email reputation","contact@example.com social accounts Holehe","example.com WHOIS domain","example.com DNS records","example.com SSL certificates","example.com company info","contact@example.com paste sites","contact@example.com username profiles"]\n\n' +
    'Query: "192.168.1.1"\n' +
    '["192.168.1.1 geolocation ISP","192.168.1.1 Shodan ports","192.168.1.1 reverse DNS","192.168.1.1 ASN lookup","192.168.1.1 threat intelligence","192.168.1.1 Censys scan","192.168.1.1 WHOIS network","192.168.1.1 historical data","192.168.1.1 port scanning"]\n\n' +
    `Query: "${query}"\n`
  );
}

/**
 * Phase 2 chaining prompt. Verbatim port of
 * llama_client_create_phase2_prompt().
 */
export function createPhase2Prompt(originalQuery, resultsJson, servicesList) {
  const list = servicesList || '(service list unavailable)';
  return (
    'You are an OSINT service orchestrator analyzing Phase 1 results.\n\n' +
    `ORIGINAL QUERY: "${originalQuery}"\n\n` +
    `PHASE 1 RESULTS:\n${resultsJson}\n\n` +
    'TASK: Determine if additional services should be called based on:\n' +
    '1. The original query intent (what did the user actually want?)\n' +
    '2. Data extracted in Phase 1 (coordinates, domains, emails, etc.)\n\n' +
    'SERVICE CHAINING RULES - CRITICAL:\n' +
    "- WEATHER_SERVICE: If query mentions 'weather' and IP_GEOLOCATION returned lat/lon,\n" +
    "  extract coordinates and pass as 'lat,lon' format (e.g., '37.38,-122.08').\n" +
    "- GEOCODING/REVERSE_GEOCODING: If query mentions 'map', 'nearby', 'location',\n" +
    '  extract coordinates from IP_GEOLOCATION results.\n' +
    '- DOMAIN_WHOIS: Extract domain from email addresses or URLs found in results.\n' +
    '- EMAIL_HARVESTER: Extract domain from email to find more addresses at same org.\n' +
    '- BREACH_CHECKER: Pass email addresses found in social lookups or person searches.\n' +
    '- COMPANY_LOOKUP: Extract employer/company names from PERSON_SEARCH or breach results.\n' +
    '- SEC_EDGAR_SEARCH: Extract CIK numbers from COMPANY_LOOKUP results for SEC filings.\n' +
    '- SUBDOMAIN_FINDER: Extract base domain from URLs or emails to find subdomains.\n' +
    '- PERSON_SEARCH: Extract names from social profile results or breach data.\n' +
    '- THREAT_INTEL: Pass IP addresses resolved from domains or found in logs.\n' +
    '- ASN_LOOKUP: Pass IP addresses to get network/ISP information.\n\n' +
    'IMPORTANT: Extract VALUES from Phase 1 results. Do NOT re-pass original query!\n\n' +
    ENTITY_TYPES_PROMPT +
    '\nExtract entities of these types from the results above.\n\n' +
    `AVAILABLE SERVICES:\n${list}\n\n` +
    'EXAMPLES:\n\n' +
    'IMPORTANT: source_service is REQUIRED - it indicates which service\'s results the entity came from!\n\n' +
    '1. Weather from IP:\n' +
    "Query: 'weather at 8.8.8.8'\n" +
    'Phase 1 IP_GEOLOCATION: {"latitude": 37.38, "longitude": -122.08}\n' +
    'Output: {"needs_newphase":true,"reason":"Extract coords for weather",' +
    '"chain_services":[{"service":"WEATHER_SERVICE","entity":"37.38,-122.08","entity_type":"coords","source_service":"IP_GEOLOCATION"}]}\n\n' +
    '2. Email breach to company:\n' +
    "Query: 'investigate john@acme.com'\n" +
    'Phase 1 BREACH_CHECKER: {"breaches":["LinkedIn2021"],"employer":"Acme Corp"}\n' +
    'Output: {"needs_newphase":true,"reason":"Investigate employer from breach",' +
    '"chain_services":[{"service":"COMPANY_LOOKUP","entity":"Acme Corp","entity_type":"company","source_service":"BREACH_CHECKER"}]}\n\n' +
    '3. Domain WHOIS to registrant:\n' +
    "Query: 'who owns evil.com'\n" +
    'Phase 1 DOMAIN_WHOIS: {"registrant_email":"admin@privacyguard.io"}\n' +
    'Output: {"needs_newphase":true,"reason":"Harvest emails from registrant domain",' +
    '"chain_services":[{"service":"EMAIL_HARVESTER","entity":"privacyguard.io","entity_type":"domain","source_service":"DOMAIN_WHOIS"}]}\n\n' +
    '4. Username to breach:\n' +
    "Query: 'find accounts for hacker123'\n" +
    'Phase 1 SHERLOCK_SEARCH: {"found_email":"hacker123@gmail.com"}\n' +
    'Output: {"needs_newphase":true,"reason":"Check discovered email for breaches",' +
    '"chain_services":[{"service":"BREACH_CHECKER","entity":"hacker123@gmail.com","entity_type":"email","source_service":"SHERLOCK_SEARCH"}]}\n\n' +
    '5. Company to SEC filings:\n' +
    "Query: 'investigate TechCorp financial'\n" +
    'Phase 1 COMPANY_LOOKUP: {"cik":"0001234567","name":"TechCorp Inc"}\n' +
    'Output: {"needs_newphase":true,"reason":"Get SEC filings for company",' +
    '"chain_services":[{"service":"SEC_EDGAR_SEARCH","entity":"0001234567","entity_type":"cik","source_service":"COMPANY_LOOKUP"}]}\n\n' +
    '6. Phone to person:\n' +
    "Query: 'who owns +1-555-123-4567'\n" +
    'Phase 1 PHONE_LOOKUP: {"carrier":"Verizon","possible_name":"John Doe"}\n' +
    'Output: {"needs_newphase":true,"reason":"Search for identified person",' +
    '"chain_services":[{"service":"PERSON_SEARCH","entity":"John Doe","entity_type":"person","source_service":"PHONE_LOOKUP"}]}\n\n' +
    '7. Map/nearby from IP:\n' +
    "Query: 'map location of 192.168.1.1'\n" +
    'Phase 1 IP_GEOLOCATION: {"latitude": 40.71, "longitude": -74.00, "city": "New York"}\n' +
    'Output: {"needs_newphase":true,"reason":"Extract coords for map/location services",' +
    '"chain_services":[{"service":"REVERSE_GEOCODING","entity":"40.71,-74.00","entity_type":"coords","source_service":"IP_GEOLOCATION"}]}\n\n' +
    '8. Phone to social accounts:\n' +
    "Query: 'find accounts for 0612345678'\n" +
    'Phase 1 PHONE_LOOKUP: {"carrier":"Orange","country":"France","line_type":"mobile"}\n' +
    'Output: {"needs_newphase":true,"reason":"Search social accounts linked to phone",' +
    '"chain_services":[{"service":"SOCIAL_ANALYZER","entity":"0612345678","entity_type":"phone","source_service":"PHONE_LOOKUP"}]}\n\n' +
    '9. Phone revealed email:\n' +
    "Query: 'investigate phone 0781218793'\n" +
    'Phase 1 PHONE_LOOKUP: {"associated_email":"john.smith@gmail.com","carrier":"SFR"}\n' +
    'Output: {"needs_newphase":true,"reason":"Check discovered email for breaches",' +
    '"chain_services":[{"service":"BREACH_CHECKER","entity":"john.smith@gmail.com","entity_type":"email","source_service":"PHONE_LOOKUP"}]}\n\n' +
    '10. Phone found in breach data:\n' +
    "Query: 'investigate john@corp.com'\n" +
    'Phase 1 BREACH_CHECKER: {"breaches":["Facebook2019"],"phone_exposed":"+33612345678"}\n' +
    'Output: {"needs_newphase":true,"reason":"Investigate exposed phone number",' +
    '"chain_services":[{"service":"PHONE_LOOKUP","entity":"+33612345678","entity_type":"phone","source_service":"BREACH_CHECKER"},' +
    '{"service":"CARRIER_LOOKUP","entity":"+33612345678","entity_type":"phone","source_service":"BREACH_CHECKER"}]}\n\n' +
    '11. Vehicle lookup to owner:\n' +
    "Query: 'who owns plate ABC123'\n" +
    'Phase 1 LICENSE_PLATE_LOOKUP: {"plate":"ABC123","owner_name":"Marie Dupont","city":"Paris"}\n' +
    'Output: {"needs_newphase":true,"reason":"Search for vehicle owner",' +
    '"chain_services":[{"service":"PERSON_SEARCH","entity":"Marie Dupont","entity_type":"person","source_service":"LICENSE_PLATE_LOOKUP"}]}\n\n' +
    '12. Flight to airport geolocation:\n' +
    "Query: 'track flight AF123'\n" +
    'Phase 1 FLIGHT_TRACKER: {"flight":"AF123","origin":"CDG","destination":"JFK","origin_coords":{"lat":49.01,"lon":2.55}}\n' +
    'Output: {"needs_newphase":true,"reason":"Get location details for airports",' +
    '"chain_services":[{"service":"REVERSE_GEOCODING","entity":"49.01,2.55","entity_type":"coords","source_service":"FLIGHT_TRACKER"}]}\n\n' +
    '13. Person search found phone:\n' +
    "Query: 'find info on John Smith'\n" +
    'Phase 1 PERSON_SEARCH: {"name":"John Smith","phone":"+1-555-987-6543","email":"jsmith@example.com"}\n' +
    'Output: {"needs_newphase":true,"reason":"Investigate discovered phone and email",' +
    '"chain_services":[{"service":"PHONE_LOOKUP","entity":"+1-555-987-6543","entity_type":"phone","source_service":"PERSON_SEARCH"},' +
    '{"service":"BREACH_CHECKER","entity":"jsmith@example.com","entity_type":"email","source_service":"PERSON_SEARCH"}]}\n\n' +
    '14. Username found multiple accounts:\n' +
    "Query: 'search username darkuser99'\n" +
    'Phase 1 SHERLOCK_SEARCH: {"profiles":["twitter.com/darkuser99","github.com/darkuser99"],"possible_email":"darkuser99@proton.me"}\n' +
    'Output: {"needs_newphase":true,"reason":"Check email and analyze social profiles",' +
    '"chain_services":[{"service":"BREACH_CHECKER","entity":"darkuser99@proton.me","entity_type":"email","source_service":"SHERLOCK_SEARCH"},' +
    '{"service":"GITHUB_USER_LOOKUP","entity":"darkuser99","entity_type":"username","source_service":"SHERLOCK_SEARCH"}]}\n\n' +
    '15. Vessel to port/company:\n' +
    "Query: 'track vessel IMO 9074729'\n" +
    'Phase 1 VESSEL_TRACKER: {"imo":"9074729","name":"Ever Given","owner":"Evergreen Marine","destination_port":"Rotterdam"}\n' +
    'Output: {"needs_newphase":true,"reason":"Investigate vessel owner company",' +
    '"chain_services":[{"service":"COMPANY_LOOKUP","entity":"Evergreen Marine","entity_type":"company","source_service":"VESSEL_TRACKER"}]}\n\n' +
    '16. Email domain to subdomains:\n' +
    "Query: 'investigate contact@suspicious.io'\n" +
    'Phase 1 BREACH_CHECKER: {"email":"contact@suspicious.io","breaches":[],"domain":"suspicious.io"}\n' +
    'Output: {"needs_newphase":true,"reason":"Investigate email domain for more intel",' +
    '"chain_services":[{"service":"SUBDOMAIN_FINDER","entity":"suspicious.io","entity_type":"domain","source_service":"BREACH_CHECKER"},' +
    '{"service":"DOMAIN_WHOIS","entity":"suspicious.io","entity_type":"domain","source_service":"BREACH_CHECKER"}]}\n\n' +
    '17. Crypto address to transactions:\n' +
    "Query: 'investigate wallet 0x742d35Cc6634C0532925a3b844Bc9e7595f'\n" +
    'Phase 1 CRYPTO_TRACKER: {"address":"0x742d35Cc6634C0532925a3b844Bc9e7595f","balance":"1.5 ETH","exchange_deposit":"Binance"}\n' +
    'Output: {"needs_newphase":true,"reason":"Track exchange connections",' +
    '"chain_services":[{"service":"EXCHANGE_FLOW","entity":"0x742d35Cc6634C0532925a3b844Bc9e7595f","entity_type":"crypto","source_service":"CRYPTO_TRACKER"}]}\n\n' +
    '=== RECURSIVE CHAINING (Phase 2 can trigger Phase 3) ===\n' +
    '18. Person found employer -> investigate company:\n' +
    "Query: 'investigate John Doe'\n" +
    'Phase 2 PERSON_SEARCH: {"employer":"TechCorp Inc","linkedin":"linkedin.com/in/johndoe"}\n' +
    'Output: {"needs_newphase":true,"reason":"Investigate discovered employer",' +
    '"chain_services":[{"service":"COMPANY_LOOKUP","entity":"TechCorp Inc","entity_type":"company","source_service":"PERSON_SEARCH"}]}\n\n' +
    '19. Breach revealed email -> check more breaches:\n' +
    "Query: 'investigate user hacker123'\n" +
    'Phase 2 SHERLOCK_SEARCH: {"emails_found":["hacker123@proton.me","h4ck3r@gmail.com"]}\n' +
    'Output: {"needs_newphase":true,"reason":"Check discovered emails for breaches",' +
    '"chain_services":[{"service":"BREACH_CHECKER","entity":"hacker123@proton.me","entity_type":"email","source_service":"SHERLOCK_SEARCH"},' +
    '{"service":"BREACH_CHECKER","entity":"h4ck3r@gmail.com","entity_type":"email","source_service":"SHERLOCK_SEARCH"}]}\n\n' +
    '20. Company revealed executives -> search persons:\n' +
    "Query: 'investigate Acme Corp'\n" +
    'Phase 2 COMPANY_LOOKUP: {"executives":["Jane Smith (CEO)","Bob Wilson (CFO)"]}\n' +
    'Output: {"needs_newphase":true,"reason":"Search discovered executives",' +
    '"chain_services":[{"service":"PERSON_SEARCH","entity":"Jane Smith","entity_type":"person","source_service":"COMPANY_LOOKUP"},' +
    '{"service":"PERSON_SEARCH","entity":"Bob Wilson","entity_type":"person","source_service":"COMPANY_LOOKUP"}]}\n\n' +
    '21. Phone investigation revealed address -> geocode:\n' +
    "Query: 'investigate 0612345678'\n" +
    'Phase 2 PHONE_LOOKUP: {"owner":"Pierre Martin","address":"123 Rue de Paris, Lyon, France"}\n' +
    'Output: {"needs_newphase":true,"reason":"Geocode discovered address",' +
    '"chain_services":[{"service":"GEOCODING","entity":"123 Rue de Paris, Lyon, France","entity_type":"address","source_service":"PHONE_LOOKUP"},' +
    '{"service":"PERSON_SEARCH","entity":"Pierre Martin","entity_type":"person","source_service":"PHONE_LOOKUP"}]}\n\n' +
    '=== NEGATIVE EXAMPLES (needs_newphase: false) ===\n' +
    '22. Simple IP lookup - no chainable data:\n' +
    "Query: 'what is 8.8.8.8'\n" +
    'Phase 1 IP_GEOLOCATION: {"ip":"8.8.8.8","country":"US","city":"Mountain View"}\n' +
    'Output: {"needs_newphase":false,"reason":"Simple lookup complete, no follow-up needed","chain_services":[]}\n\n' +
    '23. Domain lookup - informational only:\n' +
    "Query: 'lookup domain google.com'\n" +
    'Phase 1 DOMAIN_WHOIS: {"registrar":"MarkMonitor","created":"1997-09-15"}\n' +
    'Output: {"needs_newphase":false,"reason":"Domain info retrieved, no personal data to chain","chain_services":[]}\n\n' +
    '24. DNS records - technical data:\n' +
    "Query: 'dns records for example.org'\n" +
    'Phase 1 DNS_RECORDS: {"A":["93.184.216.34"],"MX":["mail.example.org"]}\n' +
    'Output: {"needs_newphase":false,"reason":"Technical DNS data, no entities to investigate","chain_services":[]}\n\n' +
    '25. Hash lookup - no personal info:\n' +
    "Query: 'hash sha256 abc123def456'\n" +
    'Phase 1 HASH_LOOKUP: {"malware":false,"file_type":"PDF","detections":0}\n' +
    'Output: {"needs_newphase":false,"reason":"Hash analysis complete, no chainable entities","chain_services":[]}\n\n' +
    '26. Weather already retrieved - chain complete:\n' +
    "Query: 'weather at 8.8.8.8'\n" +
    'Phase 2 WEATHER_SERVICE: {"temperature":72,"conditions":"Sunny","location":"Mountain View"}\n' +
    'Output: {"needs_newphase":false,"reason":"Weather data retrieved, investigation complete","chain_services":[]}\n\n' +
    '27. Phone lookup complete - no additional entities:\n' +
    "Query: 'carrier for 0612345678'\n" +
    'Phase 1 CARRIER_LOOKUP: {"carrier":"Orange","line_type":"mobile","country":"France"}\n' +
    'Output: {"needs_newphase":false,"reason":"Carrier info retrieved, no personal data found","chain_services":[]}\n\n' +
    '28. Simple phone info - no owner found:\n' +
    "Query: 'lookup 0781218793'\n" +
    'Phase 1 PHONE_LOOKUP: {"valid":true,"carrier":"SFR","type":"mobile","owner":null}\n' +
    'Output: {"needs_newphase":false,"reason":"Phone validated but no chainable entities found","chain_services":[]}\n\n' +
    'OUTPUT FORMAT (JSON):\n' +
    '{\n' +
    '  "needs_newphase": true/false,\n' +
    '  "reason": "why follow-up is/isn\'t needed",\n' +
    '  "chain_services": [\n' +
    '    {"service": "SERVICE_NAME", "entity": "extracted value", "entity_type": "type", "source_service": "SERVICE_THAT_FOUND_ENTITY"}\n' +
    '  ]\n' +
    '}\n\n' +
    'NOTE: source_service is REQUIRED for each chain_services entry - it tracks which Phase 1 service discovered the entity.\n\n' +
    'Set needs_newphase=false and empty chain_services when:\n' +
    '- The original query intent has been satisfied (weather retrieved, info found)\n' +
    '- Results contain only technical/informational data (DNS, hashes, simple lookups)\n' +
    '- No new personal entities (names, emails, companies) were discovered\n' +
    '- All discovered entities have already been investigated\n\n' +
    'Output JSON:'
  );
}
