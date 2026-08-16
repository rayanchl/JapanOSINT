export const meta = {
  name: 'osint-batch17-global',
  description: 'Discover 300+ high-penetrancy sources across under-covered regions, verified to EMIT not merely to fetch',
  phases: [
    { title: 'Discover', detail: '10 regional specialists probe live endpoints' },
  ],
}

const RULES = `
REPO: C:\\Users\\rayan\\sources\\repos\\OSINTsaas
You have NETWORK. Use it. Every row you return must come from a request you made.

  curl -sS --max-time 20 -A "Mozilla/5.0 (compatible; JapanOSINT/1.0)" '<url>'
Add --compressed for gzipped APIs. Some hosts 403 without a browser UA.

RULE 1 - NEVER return an endpoint you did not personally fetch and read the body
of. Not one from documentation, not one whose sibling worked. Everything you
return is independently re-probed AND then actually run through the collector,
so an unverifiable row dies later anyway - it just wastes everyone's time first.

RULE 2 - THE ROW MUST BE ABLE TO PRODUCE INTEL, NOT JUST HTTP 200.
This is where the last batch lost 44% of its sources, so read carefully.

The collector turns your endpoint into rows with a generic parser. It finds the
record array, then for EACH record it needs a LABEL. It takes the first of:
  - a name-ish field: title, name, headline, subject, label, event_name,
    display_name, full_name, station, location, place, country, indicator,
    or the same in other languages (nome, nom, nombre, naam, navn, nazwa,
    titulo, titre, titel)
  - failing that, a timestamp field PLUS a numeric field
  - failing that, an id-ish field: id, uid, guid, uuid, _id, identifier, code, key
A record with NONE of those is dropped and the source emits nothing.

So before you return a row, LOOK AT ONE ACTUAL RECORD and confirm it has a
name-ish field or an id-ish field. If the records are bare strings, or are keyed
only on something like "doc_ctrl_num_7" with no id/name/code field, SAY SO in
the notes and do not return it.

ALSO: report the array path EXACTLY. The auto-detector picks the densest array
of objects, which on many APIs is METADATA, not records - a schema description,
a folder list, a bounding box, a facet list. If the records live at
"result.records" say json:result.records. If the document IS one record, say
json-object. Getting this wrong is the single biggest cause of a dead row.

HIGH PENETRANCY is the goal: the record BEHIND the search hit. Officers behind a
company, beneficial owners behind the officers, violations behind a facility,
awards and subawards under a vendor, docket entries behind a case, sanctions
behind a person. Where you find a per-record detail endpoint, probe it with a
REAL id and report it.

ALREADY IN THE TREE (10,130 sources) - check before spending a probe:
  grep -rn "HOSTNAME" C:/Users/rayan/sources/repos/OSINTsaas/native/collectors --include=*.c | head -3
CKAN/Socrata/OpenDataSoft/ArcGIS-Hub/uData open-data portals are heavily covered
already - only return one if it is a DEEP endpoint, not a catalogue listing.

KEYS: prefer keyless. A row you could only probe with a key you do not have is
not verified - do not return it.
`

const SCHEMA = {
  type: 'object',
  properties: {
    area: { type: 'string' },
    probed_total: { type: 'integer' },
    sources: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          id: { type: 'string', description: 'lowercase-kebab, globally unique' },
          url: { type: 'string' },
          kind: { type: 'string', description: 'EXACT: json:<dotted.path> | json-array | json-object | geojson | rss | atom | csv' },
          items: { type: 'integer' },
          http: { type: 'integer' },
          label_field: { type: 'string', description: 'the actual field name in ONE REAL RECORD that will serve as the label (a name-ish or id-ish key). Required - if you cannot name one, do not return the row.' },
          name: { type: 'string' },
          country: { type: 'string', description: 'ISO-2, or INT for multilateral' },
          category: { type: 'string' },
          lang: { type: 'string' },
          detail_url: { type: 'string', description: '{v} where the list-row id goes; empty if none' },
          detail_key: { type: 'string' },
          detail_verified: { type: 'boolean', description: 'true ONLY if you fetched it with a real id and saw a record' },
          description: { type: 'string', description: 'what fields an investigator actually gets' },
        },
        required: ['id', 'url', 'kind', 'items', 'http', 'label_field', 'name', 'country', 'category', 'description'],
      },
    },
    notes: { type: 'string', description: 'what failed and why, and anything you rejected for having no usable label' },
  },
  required: ['area', 'probed_total', 'sources'],
}

phase('Discover')

const AREAS = [
  { key: 'africa-anglophone', prompt: 'AFRICA (anglophone): Nigeria (CAC company search, NBS statistics, NEITI, budget office, BPP procurement), Kenya (BRS, KNBS, IFMIS/tenders, judiciary Kenya Law), Ghana (RGD, Ghana Open Data, PPA), South Africa (CIPC, SARS, Treasury tenders, StatsSA, courts SAFLII, municipal money), Uganda, Tanzania, Zambia, Rwanda, Botswana, Namibia, Ethiopia. Also AfricanLII/LII family (kenyalaw, zambialii, ulii, tanzlii, namiblii, seylii, ghalii, lawlibrary.org.za) - these are real case-law corpora with detail hops. Target company registries, procurement, courts, budget/spending, statistics.' },
  { key: 'africa-franco-north', prompt: 'AFRICA (francophone + North): Morocco (OMPIC, HCP statistics, marchespublics), Tunisia (INS, tuneps procurement, JORT), Algeria, Egypt (CAPMAS, EGX, GAFI), Senegal (ANSD, ARMP marches), Cote d Ivoire, Cameroon, Benin, Burkina Faso, Mali, DRC, Madagascar, Mauritius (companies registry, statistics). Also francophone LII (juricaf.org is excellent - francophone case law across many countries with detail hops), OHADA business-law registers. Target company registries, procurement, statistics, official gazettes, courts.' },
  { key: 'latam-southern', prompt: 'LATIN AMERICA (Brazil + Southern Cone): Brazil - this is the richest open-gov country in the world, go deep: Portal da Transparencia (servidores, despesas, convenios, sancoes CEIS/CNEP/CEPIM - real detail hops), ReceitaWS/BrasilAPI CNPJ, dados.gov.br, IBGE (localidades, censo, agregados), TSE elections, CNJ/DataJud courts, ANVISA, IBAMA, Comprasnet procurement, portal da transparencia sancoes. Argentina (datos.gob.ar, AFIP, BCRA, boletin oficial, compras), Chile (Mercado Publico API is excellent and keyless, SII, BCN leychile, transparencia), Uruguay, Paraguay (DNCP procurement API is very good), Bolivia. Prioritise Brazil and Chile - both have real detail hops.' },
  { key: 'latam-northern', prompt: 'LATIN AMERICA (Mexico, Andes, Central America, Caribbean): Mexico - datos.gob.mx, DOF diario oficial, CompraNet, SAT, INEGI (excellent API), Plataforma Nacional de Transparencia, RNPDNO. Colombia (datos.gov.co Socrata is covered - find DEEPER: SECOP procurement API, RUES company registry, SUIN-Juriscol, DANE), Peru (SUNAT, OSCE/SEACE procurement, INEI, El Peruano), Ecuador (SERCOP, SRI), Venezuela, Panama, Costa Rica, Guatemala, Honduras, Dominican Republic, Cuba, Jamaica, Trinidad. Target company registries, procurement, official gazettes, sanctions/debarment lists, statistics.' },
  { key: 'middle-east', prompt: 'MIDDLE EAST: UAE (Bayanat, DED trade licence lookups, ADGM/DIFC registries, Dubai Pulse), Saudi Arabia (Monshaat, Etimad procurement, GASTAT, Saudi Business Center, Absher-adjacent public APIs), Qatar, Kuwait, Bahrain, Oman, Israel (data.gov.il is CKAN but go DEEP: companies registrar dataset, courts, Knesset API which is excellent - bills/members/votes with detail hops, CBS statistics), Jordan, Lebanon, Iraq, Turkey (TUIK, EKAP procurement, Resmi Gazete, MERSIS, KAP disclosure). Target company registries, procurement, parliament/legislative, statistics, gazettes.' },
  { key: 'south-asia', prompt: 'SOUTH ASIA: India - very rich, go deep: data.gov.in, MCA company master data, GST taxpayer search, eProcurement CPPP, Supreme Court/eCourts APIs, RBI, SEBI, Lok Sabha/Rajya Sabha, ECI elections, PIB, NIC APIs, OGD platform, IndiaFilings-adjacent public endpoints, judgments (indiankanoon has an API). Pakistan (SECP, PBS, PPRA procurement), Bangladesh (RJSC, BBS, e-GP), Sri Lanka (dept of census, courts), Nepal (OCR company registry, PPMO bolpatra), Bhutan, Maldives, Afghanistan. Target company registries, courts, procurement, legislature, statistics.' },
  { key: 'southeast-asia', prompt: 'SOUTHEAST ASIA: Indonesia (data.go.id, AHU company registry, LPSE/LKPP procurement, BPS statistics, OJK, MA supreme court decisions - putusan.mahkamahagung.go.id is a huge corpus), Philippines (PhilGEPS procurement, SEC iView, PSA statistics, Supreme Court, DBM budget), Vietnam (muasamcong procurement, GSO statistics, business registration), Thailand (DBD company registry, egp procurement, NSO), Malaysia (SSM, MyProcurement, DOSM statistics, data.gov.my), Singapore (ACRA, data.gov.sg, GeBIZ, Singapore Statutes/judiciary), Myanmar, Cambodia, Laos, Brunei, Timor-Leste. Target company registries, procurement, courts, statistics.' },
  { key: 'east-asia-oceania', prompt: 'EAST ASIA (excluding Japan) + OCEANIA: South Korea (data.go.kr, DART corporate disclosure - excellent detail hops, NTS business registration, KOSIS statistics, Supreme Court, National Assembly API, procurement g2b), Taiwan (data.gov.tw, MOEA company registry - very good keyless API, judicial yuan, government procurement, DGBAS statistics), Hong Kong (data.gov.hk, Companies Registry, HKEX disclosure, judiciary), Mongolia, Macau. Oceania: Australia (ABN Lookup, ASIC, AusTender, data.gov.au, AustLII, APVMA, ACNC charities - go deep, not catalogue), New Zealand (NZBN company API is excellent and keyless, data.govt.nz, GETS tenders, NZLII, Stats NZ), Papua New Guinea, Fiji, Samoa, Vanuatu, Solomon Islands, Pacific Data Hub.' },
  { key: 'eastern-europe-central-asia', prompt: 'EASTERN EUROPE + CENTRAL ASIA + CAUCASUS: Ukraine (this is exceptionally rich and open: Prozorro procurement API with deep hops, YouControl-adjacent open registries, EDR company registry, court register reyestr.court.gov.ua, NAZK declarations - the asset declaration register is a major OSINT source, opendatabot-adjacent), Georgia (RS company registry, SPA procurement, court), Moldova, Armenia (e-register), Azerbaijan, Kazakhstan (goszakup procurement API is good, stat.gov.kz, adilet legal), Uzbekistan, Kyrgyzstan, Tajikistan, Belarus, Russia-adjacent open registries where lawfully public, Serbia (APR business registry), Bosnia, North Macedonia, Albania (open procurement), Montenegro, Kosovo. Prioritise Ukraine Prozorro/NAZK and Kazakhstan goszakup - all have real detail hops.' },
  { key: 'multilateral-global', prompt: 'MULTILATERAL / GLOBAL institutions - these cover every country at once and are high value: World Bank (projects, contracts awarded, debarred firms, indicators, documents - all with detail hops), IMF, UN (Comtrade, UNDP transparency, UNOPS, UN Global Marketplace suppliers, UNHCR, OCHA HDX, ReliefWeb), WHO (GHO, disease outbreak news), WTO, OECD, FAO, ILO, UNESCO, regional development banks (AfDB, AsDB, IDB - IDB has good project/contract APIs, EBRD, EIB, IsDB, CAF), Global Fund, GAVI, Green Climate Fund, sanctions and debarment (World Bank debarred, AfDB, AsDB, IDB, EBRD cross-debarment lists), OpenSanctions, aid transparency IATI datastore (very deep - activities/transactions/organisations), Open Contracting Data Standard aggregators. Target project/contract/award records with per-record detail hops and debarment/sanction lists.' },
]

const specs = (await parallel(AREAS.map(a => () =>
  agent(RULES + '\n\nYOUR REGION:\n' + a.prompt +
        '\n\nAim for 50-90 rows that will ACTUALLY EMIT. A row you confirmed has a label field is worth five you did not check. Report probed_total honestly including failures.',
        { label: 'discover:' + a.key, phase: 'Discover', schema: SCHEMA, effort: 'high' })
))).filter(Boolean)

const total = specs.reduce((n, s) => n + (s.sources || []).length, 0)
const probed = specs.reduce((n, s) => n + (s.probed_total || 0), 0)
const det = specs.reduce((n, s) => n + (s.sources || []).filter(x => x.detail_verified).length, 0)
const lab = specs.reduce((n, s) => n + (s.sources || []).filter(x => (x.label_field || '').trim()).length, 0)
log('discovered ' + total + ' from ' + probed + ' probes; ' + det + ' detail hops; ' + lab + ' with a named label field')

return {
  discovered: total,
  probed,
  with_detail_hop: det,
  with_label_field: lab,
  by_area: specs.map(s => ({ area: s.area, n: (s.sources || []).length, probed: s.probed_total, notes: s.notes })),
  sources: specs.flatMap(s => s.sources || []),
}
