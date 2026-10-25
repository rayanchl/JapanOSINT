#!/usr/bin/env python3
"""Split a giant beat by BODY TYPE: give every row of a table an explicit
`.collector` (and the category that body type is), derived from the row's NAME.

    python3 tools/split_beats_by_body_type.py plan  collectors/pivot/table/hp3b47_frgov.c ... > plan.tsv
    python3 tools/split_beats_by_body_type.py apply plan.tsv collectors/pivot/table/hp3b47_frgov.c ...
    python3 tools/split_beats_by_body_type.py manifest plan.tsv ../docs/candidate-sources-batch47.frgov.txt ...

Why (2026-10-24). Three tables held a third of the registry as one collector
each — hp3b47_frgov 9,619 rows, hp3b47_itgov 5,716, hp3b48_news 3,992 — every
row `category = "government"` (or "news"), `record_type = "government-notice"`,
so the Collectors pages, the facets and the iOS dashboard showed one bucket
where the rows themselves say "Mairie", "Centre communal d'action sociale",
"ISTITUTO COMPRENSIVO", "Ordine dei Medici", "(US)". Italy's 2,725 state
schools and 821 professional orders were filed under Government & public
administration. The hpengine default (collector = the table's file name) is
right for a beat that IS one body type; these were registers of everything a
country's public sector publishes.

How. `plan` reads the tables (the C is the maintained copy, never the
manifest), extracts each row's id and name, and applies ordered regex rules —
first match wins — on the body's name: France by the type prefix the
service-public.fr directory puts first ("Mairie - X", "Communauté de communes
- Y"), Italy by the name plus the IPA register category in parentheses
("Gestori di Pubblici Servizi"), the press by the "(CC)" country code the
batch-48 generator wrote into the name, mapped to seven world regions. A row
no rule types goes to the beat's honest residue (`itgov_altri`, 181 rows:
agencies, consortia, commissioner structures), never to a guessed type.
`apply` rewrites exactly one line per row — `.category = "…",` becomes
`.category = "<cat>", .collector = "<key>",` — and refuses a row it cannot
find in the plan or that already names a collector. `manifest` mirrors the
same two facts into the batch manifests (field 4 and a `collector=` opt), so a
future re-probe reads what the C says.

The keys need a line each in tools/gen_collectors_def.py (OVERRIDES for the
group, LABELS for the heading) — `make collectors-def` then regenerates
core/collectors.def and `make collectors-def-check` fails while any key has
none. Nothing about a row's URL, id or identity changes, so no emit measurement
is invalidated; the registry count is unchanged.

The rules are data about these three registers as read on 2026-10-24. A
re-run over a regenerated table (new rows) classifies the new rows the same
way; a rule that no longer matches anything is harmless.
"""
import re, sys, collections

REGION = {
 'news_northam': 'US CA',
 'news_latam': 'MX BR AR CL PE CO VE BO EC UY PY GT HN NI SV CR PA DO CU HT SR BS BB TT JM GY BZ PR',
 'news_europe_west': 'NO ES IT DE FR CH SE GB PT FI BE LU IE AT NL DK IS AD MT VA GR CY MC LI SM FO GI',
 'news_europe_east': 'RU UA HU PL RO CZ BY SK LV LT EE BG RS HR BA MK AL XK MD SI ME GE AM AZ KZ KG UZ TJ TM',
 'news_mena': 'PS IL AE DZ MA TN LB SA EG IR IQ SY YE KW QA BH OM JO LY MR SD TR',
 'news_africa': 'NG ZA GH BF GN BJ CD CM CI SN ML TG NE TD MG MZ AO ZM ZW KE UG RW ET SO DJ ER GM GW CV ST SZ LS BW NA MW TZ BI GA GQ CG SL LR MU SC KM SS CF',
 'news_asia_pacific': 'IN KR ID JP TW MY CN PK BD LK SG TH VN KH MM PH NP BT MN HK MO AU NZ FJ PW MH PG WS TO AF MV LA BN TL KP VU SB KI NR TV FM',
}
CC2REGION = {cc: k for k, v in REGION.items() for cc in v.split()}

def R(*pats): return re.compile('|'.join(pats), re.I)

# (collector, category, regex) — first match wins
FR_RULES = [
 ('frgov_ordres',      'legal',        R(r"ordre (des|de l')", r"de l'ordre", r"Chambre (départementale|interdépartementale|régionale) des (notaires|commissaires)", r"Compagnie nationale des conseils", r"experts[- ]comptables", r"Chambre de métiers", r"Chambre de commerce", r"\bCCI\b", r"\bCMA\b", r"Chambre d'agriculture")),
 ('frgov_sante',       'health',       R(r"sant[ée] (au|en|du) travail", r"santé travail", r"m[ée]decine (du|et santé au) travail", r"santé en milieu de travail", r"m[ée]dical", r"\bSamu\b", r"Centre hospitalier", r"\bCHU\b", r"\bCHR\b", r"cancer", r"\bDSRC\b", r"pharmacovigilance", r"protection maternelle", r"\bPMI\b", r"hygiène", r"drogues", r"biomédecine", r"praticiens hospitaliers", r"Agence de santé", r"santé mentale", r"santé publique", r"Joueurs Info Service", r"Santra Plus", r"\bAist\b", r"PREVY", r"Metrazif", r"Horizon Santé", r"Efficience santé", r"Ami Santé", r"Alsace santé", r"Action Santé", r"Agir ensemble pour la santé", r"Btp santé", r"port aux lions", r"Groupement interprofessionnel médico", r"Groupement des entreprises pour la santé", r"interentreprises de santé", r"Interentreprises", r"Caisse primaire d'assurance maladie", r"\bCPAM\b", r"Mutualité sociale agricole", r"\bMSA\b", r"sécurité sociale")),
 ('frgov_social',      'local-gov',    R(r"Centre communal d'action sociale", r"\bCCAS\b", r"Centre intercommunal d'action sociale", r"\bCIAS\b", r"personnes âgées", r"\bMDPH\b", r"personnes handicapées", r"de l'autonomie", r"\bMDA\b", r"Autonomie", r"des solidarités", r"\bMDS\b", r"Plateforme d'accompagnement", r"Point conseil budget", r"Point info famille", r"\bCLIC\b", r"Centre Local d.Information", r"Maison départementale", r"Maison Départementale", r"Citoyenneté", r"enfance", r"adoption", r"familles monoparentales", r"Hors La Rue", r"AMICALE DU NID", r"ASSOCIATION LAIQUE +PRADO", r"MLEZI MAORE", r"Agence AutonomY", r"Haut Comité pour le logement", r"\bADIL\b", r"\bAnah\b", r"information sur le logement", r"de l'habitat")),
 ('frgov_securite',    'public-safety',R(r"Police (municipale|pluricommunale|intercommunale)", r"police nationale", r"Service départemental d'incendie", r"\bSDIS\b", r"\bJRCC\b", r"\bCROSS\b", r"aide aux victimes", r"accès au droit", r"Maison de justice", r"Point justice", r"conciliation", r"pénitentiaire", r"privation de libert", r"\bOFPRA\b", r"réfugiés", r"Bureau d'enquête", r"Force d'intervention", r"douane", r"service national universel", r"armée", r"pupilles de l.air")),
 ('frgov_services_citoyens','government',R(r"France Rénov", r"accueil des salariés", r"^Point\b", r"Mission locale", r"Info ?Jeunes", r"information jeunesse", r"\bCRIJ\b", r"Bureau d.information et d.orientation", r"\bBIO\b", r"Centre d'information et d'orientation", r"\bCIO\b", r"Maison de l'emploi", r"\bMDEF?\b", r"\bMDECE\b", r"Cap emploi", r"Carif Oref", r"France Travail", r"France services", r"\bGreta\b", r"bénévoles", r"\bCRIB\b", r"information des associations", r"Guid'Asso", r"droits des femmes", r"Centre de ressources", r"illettrisme", r"Bureau d.information", r"Kiosque information")),
 ('frgov_enseignement','research',     R(r"Universit", r"École", r"Ecole", r"Institut national supérieur", r"\bINSPE\b", r"Conservatoire national des arts", r"\bCNAM\b", r"Télécom", r"Centrale", r"EURECOM", r"Arts et Métiers", r"Institut Mines", r"Institut supérieur", r"Campus", r"Collège de France", r"\bCNRS\b", r"recherche scientifique", r"Agence nationale de la recherche", r"Agence bibliographique", r"Centre informatique national", r"Institut des Hautes études", r"Institut d'éducation", r"Institut national d'enseignement", r"Institut Robert Badinter", r"Bureau des longitudes", r"Académie", r"Centre de recherche", r"Centre pour la recherche", r"Centre d'études", r"Groupe de météorologie", r"environnement industriel", r"Institut régional d'administration", r"Institut d'aménagement", r"formation de l'enseigne", r"Observatoire", r"Institut Franco", r"Centre Franco", r"agronomique", r"agro", r"Armeflhor")),
 ('frgov_environnement','environment', R(r"chasseurs", r"pêche", r"Parc naturel", r"Parcs nationaux", r"Conservatoire botanique", r"Conservatoire du littoral", r"\bSafer\b", r"aménagement foncier", r"Agence de l'eau", r"protection de la nature", r"climat", r"Délégation à la mer", r"Direction de la mer", r"\bDIRM\b", r"de la mer", r"affaires maritimes", r"marine marchande", r"\bCAUE\b", r"Conseil d'architecture", r"Conseils d'Architecture", r"environnement", r"écologie", r"littoral", r"Ambito", r"économie agricole", r"Office de développement")),
 ('frgov_culture',     'culture',      R(r"Institut français", r"Alliance française", r"Mus[ée]e", r"monuments nationaux", r"Centre national de la musique", r"Conservatoire national supérieur", r"affaires culturelles", r"patrimoine", r"château", r"Bibliothèque", r"Archives", r"culturelle", r"drame", r"Académie française")),
 ('frgov_mairies',     'local-gov',    R(r"^Mairie\b", r"^Commune\b", r"^Ville de", r"^Mairie Saint")),
 ('frgov_intercommunal','local-gov',   R(r"Communauté de communes", r"Communauté de Communes", r"Communauté d'agglomération", r"Communauté d'Agglomération", r"Communauté urbaine", r"Métropole", r"Agglomération", r"^Intercom", r"Syndicat", r"Pôle d'équilibre", r"^Communauté", r"^Pays de", r"^Grand port")),
 ('frgov_etat',        'government',   R(r"^Direction", r"Préfecture", r"^Sous", r"Conseil départemental", r"Conseil régional", r"Conseil économique", r"Collectivité", r"^Région", r"^Département", r"Centre de gestion", r"Laboratoire", r"^Service", r"^Délégation", r"^Secrétariat", r"^Bureau", r"^Office", r"^Agence", r"Établissement public", r"Institut d'émission", r"Haute autorité", r"Haut Conseil", r"Commission", r"Contrôleur", r"Inspection", r"^Mission", r"^Pôle", r"Assemblée", r"^Autorité", r"Conseil national", r"Conseil supérieur", r"Conseil de la culture", r"Centre technique", r"^Section", r"Crédit municipal", r"^Caisse", r"^Institut", r"impôts", r"statistique", r"Secrétariat", r"Département des", r"Conseil Départemental")),
]
IT_RULES = [
 ('itgov_sanita',      'health',       R(r"\bApsp\b", r"Emergenza Urgenza", r"Azienda Sociale", r"Solidale", r"\bARNAS\b", r"Azienda (Sanitaria|Ospedalier|Unit[aà]' Sanitaria|USL|Ulss|U\.L\.S\.S)", r"\bASL\b", r"\bASP\b", r"\bAUSL\b", r"\bUSL\b", r"\bIRCCS\b", r"Zooprofilattico", r"Ortopedico", r"Tumori", r"Oncologic", r"Policlinico", r"Ospedal", r"Casa di Riposo", r"Casa di Cura", r"\bIPAB\b", r"I\.P\.A\.B", r"\bIpab\b", r"Opera Pia", r"A\.P\.S\.P", r"Servizi alla Persona", r"Centro servizi (sociali|alla Persona)", r"Residenz", r"\bRSA\b", r"Beneficenza", r"AGENAS", r"Servizi Sanitari", r"della Salute", r"Superiore di Sanit", r"Croce Rossa", r"Anziani", r"Assistenza", r"Gaslini", r"Fisioterapici", r"Struttura Protetta", r"^Casa\b", r"^Istituto (Immacolata|Romano di San Michele|Rizza Rosso|Femminile|Costante Gris|Emilio Biazzi)", r"Soggiorno", r"Minori", r"Infanzia", r"Sanit")),
 ('itgov_universita',  'research',     R(r"Serics", r"Universit", r"Politecnico", r"Fond\. Univ", r"Conservatorio", r"Accademia", r"Scuola (Superiore|Normale|Internazionale)", r"\bSISSA\b", r"\bIMT\b", r"Istituto Nazionale di (Fisica|Astrofisica|Geofisica|Ricerca|Statistica|Studi|documentazione)", r"\bINFN\b", r"\bINGV\b", r"\bCNR\b", r"Consiglio Nazionale delle Ricerche", r"Diritto allo Studio", r"\bERSU\b", r"\bERDISU\b", r"\bADISU\b", r"Istituto (Storico|per La Storia|per Gli Studi|Italiano di Studi)", r"ISTAT", r"Ricerca", r"Studi Germanici", r"Numismatica", r"Collegio Ghislieri", r"Istituto Nazionale di Studi")),
 ('itgov_scuole',      'education',    R(r"^I\. C\.", r"^IST TEC", r"^P\.T\.P", r"Asilo", r"^ISTITUTO", r"^Istituto", r"^IST\.", r"^ISTITUTI", r"^I\.?C\.?S?\.?\b", r"^IIS", r"^I\.I\.S", r"^IISS", r"^ITS\b", r"^ITIS", r"^ITC", r"^ITE\b", r"^ITT", r"^IPS", r"^IPSIA", r"^IPSSAR", r"^LICEO", r"^Liceo", r"^SCUOLA", r"^Scuola", r"^DIREZIONE DIDATTICA", r"^Direzione Didattica", r"^CONVITTO", r"^Convitto", r"^CPIA", r"^Cpia", r"Istruzione", r"ISTRUZIONE", r"^Polo", r"^POLO", r"^Fondazione Its", r"Educandato", r"Circolo Didattico", r"^ISTITUZIONE MUSICALE", r"^Centro Provinciale", r"^Centro per L'Istruzione", r"Scolastic", r"Comprensiv", r"^I\.T\.", r"^I\.P\.", r"^L\.S\.", r"^L\.C\.", r"^S\.M\.S", r"^D\.D\.", r"^C\.P\.I\.A", r"^Omnicomprensivo", r"^ISTITUTO OMNICOMPRENSIVO")),
 ('itgov_ordini',      'legal',        R(r"Notarile", r"Consulenti del Lavoro", r"Consiglio Provinciale Ordine", r"^Ordine", r"^ORDINE", r"^Collegio", r"^COLLEGIO", r"degli Ordini", r"Degli Ordini", r"dell'Ordine", r"Consiglio Nazionale (dei|degli|delle) ", r"Spedizionieri Doganali", r"Fondazione Consiglio Nazionale", r"Mutua Assistenza per i Professionisti", r"Cassa Nazionale di Previdenza", r"Inarcassa", r"Maestri di Sci", r"Ordine dei", r"Ordine Dei")),
 ('itgov_camere',      'commerce',     R(r"Camera di Commercio", r"CAMERA DI COMMERCIO", r"Unioncamere", r"Camere di Commercio", r"Azienda Speciale della Camera")),
 ('itgov_comuni',      'local-gov',    R(r"^Frazione", r"^Comune\b", r"^COMUNE\b", r"^Citt[aà]' di", r"^Città di", r"^Municipio", r"^Municipalit", r"^Roma Capitale")),
 ('itgov_ambiente',    'environment',  R(r"Autorit.{1,3} di bacino", r"^Parco", r"^PARCO", r"Aree Protette", r"Area Marina", r"Riserva", r"Consorzio di Bonifica", r"Consorzio di bonifica", r"CONSORZIO DI BONIFICA", r"Bonifica", r"Ambito Territoriale (di )?Caccia", r"AMBITO TERRITORIALE (DI )?CACCIA", r"Protezione dell'Ambiente", r"\bARPA", r"Autorita' di Bacino", r"Irrigazione", r"Ambiente", r"Forestal", r"Parco Nazionale", r"Caccia")),
 ('itgov_cultura',     'culture',      R(r"d'Arte", r"Film Commission", r"Federazione Italiana", r"FEDERAZIONE ITALIANA", r"\bSport", r"Turistic", r"\bFIERE?\b", r"\bFiera\b", r"Teatr", r"Teatro", r"TEATRO", r"Museo", r"MUSEO", r"Orchestra", r"Bibliotec", r"Festival", r"Istituzione (Musicale|Dott|Antonio Vivaldi)", r"ISTITUZIONE", r"Dramma Antico", r"Fondazione (Cascina|Bruno Kessler)", r"Archivio", r"Pinacoteca", r"Sistema Bibliotecario", r"Cultura", r"Culturale", r"Diocesi")),
 ('itgov_territoriali','local-gov',    R(r"D'Ambito", r"^Anci\b", r"^Unione", r"^UNIONE", r"Comunit[aà]' Montana", r"Comunità Montana", r"^Comunit", r"^Provincia", r"^PROVINCIA", r"Citt[aà]' Metropolitana", r"Città Metropolitana", r"^Regione", r"^REGIONE", r"Consiglio Regionale", r"Giunta", r"Libero Consorzio", r"Consorzio (dei|di) Comuni", r"Consorzio Intercomunale", r"Amministrazione (Separata|Spearata|Seperata)", r"Ambito Territoriale Sociale", r"Ambito Sociale", r"^Ato\b", r"^ATO\b", r"^Ambito", r"Autorita' di Ambito", r"Autorita di Ambito", r"Consiglio di Bacino", r"Ente di gestione Ambito", r"^Gal\b", r"^GAL\b", r"Gruppo di Azione Locale", r"GRUPPO DI AZIONE LOCALE", r"G\.A\.L\.", r"Usi Civici", r"Uso Civico")),
 ('itgov_servizi_pubblici','utility',  R(r"Sistema Portuale", r"Smaltimento", r"Cimiter", r"FARMAC", r"S\.C\.R\.L", r"\bSC[AP]?R?L\b", r"\bSCPA\b", r"\bScarl\b", r"\bScrl\b", r"S\.?R\.?L\.?\b", r"S\.?P\.?A\.?\b", r"\bSpa\b", r"\bSrl\b", r"Societ", r"SOCIET", r"Servizi (Idrici|Ambientali|Pubblici)", r"SERVIZI", r"Autorita' (per Il Servizio|per il Servizio|di Sistema Portuale|di sistema portuale)", r"Agenzia del Trasporto", r"Trasport", r"Acque", r"Rifiuti", r"Energia", r"\bGas\b", r"Multiservizi", r"Azienda (Speciale|Municipalizzata|Trasporti|Servizi)", r"Idric", r"Gestione Integrata", r"in Liquidazione", r"IN LIQUIDAZIONE", r"Consorzio per L'Area di Sviluppo Industriale", r"Sviluppo Industriale", r"Porto", r"Aeroport", r"Interporto", r"Farmaci", r"Gestori di Pubblici Servizi")),
]

def classify(table, name, desc):
    if table == 'hp3b48_news':
        m = re.search(r'\((\w\w)\)(?: -|$)', name)
        cc = m.group(1) if m else None
        if cc is None or cc not in CC2REGION:
            return None, None, cc
        return CC2REGION[cc], 'news', cc
    rules = FR_RULES if table == 'hp3b47_frgov' else IT_RULES
    core = name
    if table == 'hp3b47_itgov':
        core = re.sub(r"\s*\((Pubbliche Amministrazioni|Gestori di Pubblici Servizi|Stazioni Appaltanti).*$", "", name)
    for key, cat, rx in rules:
        if rx.search(core):
            return key, cat, None
    if table == 'hp3b47_itgov' and 'Gestori di Pubblici Servizi' in name:
        return 'itgov_servizi_pubblici', 'utility', None
    return ('frgov_autres', 'government', None) if table == 'hp3b47_frgov' else ('itgov_altri', 'government', None)


def rows_of(path):
    """(id, name) per row of an hpengine table, adjacent string literals joined."""
    txt = open(path, encoding='utf-8').read()
    body = txt[txt.index('[] = {'):]
    for r in re.split(r'\n\s*\{\s*(?=\.id\s*=)', body)[1:]:
        def fld(name):
            m = re.search(r'\.' + name + r'\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+)', r)
            return ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))).replace('\\"', '"') if m else ''
        yield fld('id'), fld('name')


def cmd_plan(files):
    tally = collections.Counter()
    for f in files:
        table = f.split('/')[-1][:-2]
        for sid, name in rows_of(f):
            key, cat, cc = classify(table, name, '')
            if key is None:
                raise SystemExit(f'{table}: {sid}: no country code in the name ({cc!r}); add it to REGION')
            tally[(table, key, cat)] += 1
            print(f'{sid}\t{table}\t{key}\t{cat}')
    for (t, k, c), n in sorted(tally.items()):
        print(f'{t:14s} {k:26s} {c:14s} {n:6d}', file=sys.stderr)


def read_plan(path):
    plan = {}
    for l in open(path):
        sid, table, key, cat = l.rstrip('\n').split('\t'); plan[sid] = (table, key, cat)
    return plan


def cmd_apply(plan, files):
    for f in files:
        txt = open(f, encoding='utf-8').read()
        head, body = txt.split('[] = {', 1)
        parts = re.split(r'(\n\s*\{\s*(?=\.id\s*=))', body)
        out = [parts[0]]; n = 0; miss = []
        for i in range(1, len(parts), 2):
            sep, row = parts[i], parts[i + 1]
            sid = re.match(r'\.id\s*=\s*"([^"]+)"', row).group(1)
            if sid not in plan:
                miss.append(sid); out.append(sep + row); continue
            _, key, cat = plan[sid]
            if '.collector' in row:
                raise SystemExit(f'{f}: {sid} already names a collector')
            row2, k = re.subn(r'\.category = "[^"]*",', f'.category = "{cat}", .collector = "{key}",', row, count=1)
            if k != 1:
                raise SystemExit(f'{f}: {sid}: category line not found')
            n += 1; out.append(sep + row2)
        if miss:
            raise SystemExit(f'{f}: {len(miss)} rows not in plan: {miss[:3]}')
        open(f, 'w', encoding='utf-8').write(head + '[] = {' + ''.join(out))
        print(f'{f}: {n} rows edited', file=sys.stderr)


def cmd_manifest(plan, files):
    for f in files:
        out = []; n = 0; miss = 0
        for line in open(f, encoding='utf-8'):
            if line.startswith('#') or '|' not in line:
                out.append(line); continue
            fld = line.rstrip('\n').split('|')
            if fld[0] not in plan:
                miss += 1; out.append(line); continue
            _, key, cat = plan[fld[0]]
            fld[3] = cat
            while len(fld) < 13:
                fld.append('')
            fld[12] = (fld[12] + ';' if fld[12] else '') + f'collector={key}'
            out.append('|'.join(fld) + '\n'); n += 1
        open(f, 'w', encoding='utf-8').write(''.join(out))
        print(f'{f}: {n} rows mirrored, {miss} rows not in the tree (left as they are)', file=sys.stderr)


if __name__ == '__main__':
    if len(sys.argv) < 3 or sys.argv[1] not in ('plan', 'apply', 'manifest'):
        raise SystemExit(__doc__)
    if sys.argv[1] == 'plan':
        cmd_plan(sys.argv[2:])
    elif sys.argv[1] == 'apply':
        cmd_apply(read_plan(sys.argv[2]), sys.argv[3:])
    else:
        cmd_manifest(read_plan(sys.argv[2]), sys.argv[3:])
