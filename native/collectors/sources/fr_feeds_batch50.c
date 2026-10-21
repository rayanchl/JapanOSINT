/* collectors/sources/fr_feeds_batch50.c
 * Batch 50 — 261 France government / institutional RSS-Atom feeds.
 *
 * Every row was fetched live on 2026-10-20: HTTP 200, an <rss>/<feed>/<rdf> root,
 * and at least one <item>/<entry>. URLs already registered anywhere in the tree
 * were dropped before probing.
 *
 * 173 of these were found by AUTODISCOVERY: the official homepage (from
 * Wikidata P856, or a host already in the tree) was fetched and its own
 * <link rel="alternate" type="application/rss+xml"> declarations were read —
 * the site was asked what it serves rather than probed at guessed paths.
 * Closed gov.uk bodies (gov.uk organisations API govuk_status=closed) poll weekly.
 * Per-row probe counts: docs/verified-sources-batch50.tsv. */
#include "_source_macros.inc"

/* ── fr_government ───────────────────────────────────────────────────── */

RSSX(fr_b50_42network_org, "fr-b50-42network-org",
     "42", "42",
     "fr_government", "government",
     "https://www.42network.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:42\",\"government\",\"batch:50\"]",
     10800,
     "42 — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_academie_tourisme_fr, "fr-b50-academie-tourisme-fr",
     "Academy of Tourism", "Académie du Tourisme",
     "fr_government", "government",
     "https://academie-tourisme.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Academy of Tourism\",\"government\",\"batch:50\"]",
     10800,
     "Academy of Tourism — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_afghanistan_uni_org, "fr-b50-afghanistan-uni-org",
     "Roshan Afghanistan University - RAU", "Roshan Afghanistan University",
     "fr_government", "government",
     "https://www.afghanistan-uni.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Roshan Afghanistan University - RAU\",\"government\",\"batch:50\"]",
     10800,
     "Roshan Afghanistan University - RAU — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ajaccio_corsica, "fr-b50-ajaccio-corsica",
     "Ajaccio", "Ajaccio",
     "fr_government", "government",
     "https://ajaccio.corsica/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Ajaccio\",\"government\",\"batch:50\"]",
     10800,
     "Ajaccio — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_amicalecombree_fr_format_feed_type_atom, "fr-b50-amicalecombree-fr-format-feed-type-atom",
     "Accueil", "Institution libre de Combrée",
     "fr_government", "government",
     "https://amicalecombree.fr/?format=feed&type=atom",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Accueil\",\"government\",\"batch:50\"]",
     10800,
     "Accueil — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 2 entries at probe")

RSSX(fr_b50_amicalecombree_fr_format_feed_type_rss, "fr-b50-amicalecombree-fr-format-feed-type-rss",
     "Accueil", "Institution libre de Combrée",
     "fr_government", "government",
     "https://amicalecombree.fr/?format=feed&type=rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Accueil\",\"government\",\"batch:50\"]",
     10800,
     "Accueil — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 2 entries at probe")

RSSX(fr_b50_anps_fr, "fr-b50-anps-fr",
     "National Association for First Aid", "Association nationale des premiers secours",
     "fr_government", "government",
     "https://www.anps.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:National Association for First Aid\",\"government\",\"batch:50\"]",
     10800,
     "National Association for First Aid — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_anses_fr, "fr-b50-anses-fr",
     "ANSES Food Safety", "ANSES Food Safety",
     "fr_government", "government",
     "https://www.anses.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:ANSES Food Safety\",\"government\",\"batch:50\"]",
     10800,
     "ANSES Food Safety — government feed (10 entries at probe)")

RSSX(fr_b50_anses_fr_fr, "fr-b50-anses-fr-fr",
     "France ANSES Food Safety", "France ANSES Food Safety",
     "fr_government", "government",
     "https://www.anses.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France ANSES Food Safety\",\"government\",\"batch:50\"]",
     10800,
     "France ANSES Food Safety — government feed (10 entries at probe)")

RSSX(fr_b50_apprenance_formation_org, "fr-b50-apprenance-formation-org",
     "Apprenance, Formation, Digital", "Équipe Apprenance, Formation, Digital",
     "fr_government", "government",
     "https://apprenance-formation.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Apprenance, Formation, Digital\",\"government\",\"batch:50\"]",
     10800,
     "Apprenance, Formation, Digital — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 12 entries at probe")

RSSX(fr_b50_atelierblanchesalant_fr, "fr-b50-atelierblanchesalant-fr",
     "Atelier Blanche Salant — Atelier Blanche Salant Paul Weaver RSS Feed", "atelier Blanche Salant — Atelier Blanche Salant Paul Weaver RSS Feed",
     "fr_government", "government",
     "http://atelierblanchesalant.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Atelier Blanche Salant — Atelier Blanche Salant Paul Weaver RSS Feed\",\"government\",\"batch:50\"]",
     10800,
     "Atelier Blanche Salant — Atelier Blanche Salant Paul Weaver RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_autoritedelaconcurrence_fr, "fr-b50-autoritedelaconcurrence-fr",
     "Competition Authority", "Competition Authority",
     "fr_government", "government",
     "https://www.autoritedelaconcurrence.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Competition Authority\",\"government\",\"batch:50\"]",
     10800,
     "Competition Authority — government feed (10 entries at probe)")

RSSX(fr_b50_avicenne_eu, "fr-b50-avicenne-eu",
     "Institut Avicenne des Sciences Humaines", "Institut Avicenne de Sciences Humaines",
     "fr_government", "government",
     "http://avicenne.eu/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut Avicenne des Sciences Humaines\",\"government\",\"batch:50\"]",
     10800,
     "Institut Avicenne des Sciences Humaines — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_beaucaire_fr, "fr-b50-beaucaire-fr",
     "Beaucaire", "Beaucaire",
     "fr_government", "government",
     "https://www.beaucaire.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Beaucaire\",\"government\",\"batch:50\"]",
     10800,
     "Beaucaire — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(fr_b50_boisbuchet_org, "fr-b50-boisbuchet-org",
     "domaine de Boisbuchet", "domaine de Boisbuchet",
     "fr_government", "government",
     "https://www.boisbuchet.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:domaine de Boisbuchet\",\"government\",\"batch:50\"]",
     10800,
     "domaine de Boisbuchet — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_brest_bs_com, "fr-b50-brest-bs-com",
     "ESC Bretagne Brest", "École supérieure de commerce Bretagne Brest",
     "fr_government", "government",
     "https://brest-bs.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:ESC Bretagne Brest\",\"government\",\"batch:50\"]",
     10800,
     "ESC Bretagne Brest — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_brignoles_fr, "fr-b50-brignoles-fr",
     "Brignoles", "Brignoles",
     "fr_government", "government",
     "https://www.brignoles.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Brignoles\",\"government\",\"batch:50\"]",
     10800,
     "Brignoles — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_buccotherm_com_blog_feed, "fr-b50-buccotherm-com-blog-feed",
     "Laboratoire Odost (France) — Buccotherm - RSS", "Laboratoire Odost — Buccotherm - RSS",
     "fr_government", "government",
     "https://www.buccotherm.com/blog-feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Laboratoire Odost (France) — Buccotherm - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Laboratoire Odost (France) — Buccotherm - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 8 entries at probe")

RSSX(fr_b50_calandretacostapavada_net, "fr-b50-calandretacostapavada-net",
     "Calandreta Costa Pavada", "Calandreta Costa Pavada",
     "fr_government", "government",
     "https://calandretacostapavada.net/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Calandreta Costa Pavada\",\"government\",\"batch:50\"]",
     10800,
     "Calandreta Costa Pavada — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_carquefou_fr_post_type_affichage_liste, "fr-b50-carquefou-fr-post-type-affichage-liste",
     "Carquefou — Flux RSS des affichages listes", "Carquefou — Flux RSS des affichages listes",
     "fr_government", "government",
     "https://www.carquefou.fr/feed/?post_type=affichage_liste",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Carquefou — Flux RSS des affichages listes\",\"government\",\"batch:50\"]",
     10800,
     "Carquefou — Flux RSS des affichages listes — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_carquefou_fr_post_type_agenda, "fr-b50-carquefou-fr-post-type-agenda",
     "Carquefou — Flux RSS de l'agenda", "Carquefou — Flux RSS de l'agenda",
     "fr_government", "government",
     "https://www.carquefou.fr/feed/?post_type=agenda",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Carquefou — Flux RSS de l'agenda\",\"government\",\"batch:50\"]",
     10800,
     "Carquefou — Flux RSS de l'agenda — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_carquefou_fr_post_type_base_documentaire, "fr-b50-carquefou-fr-post-type-base-documentaire",
     "Carquefou — Flux RSS de la base documentaire", "Carquefou — Flux RSS de la base documentaire",
     "fr_government", "government",
     "https://www.carquefou.fr/feed/?post_type=base_documentaire",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Carquefou — Flux RSS de la base documentaire\",\"government\",\"batch:50\"]",
     10800,
     "Carquefou — Flux RSS de la base documentaire — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_carquefou_fr_post_type_contacts, "fr-b50-carquefou-fr-post-type-contacts",
     "Carquefou — Flux RSS de l'annuaire de contact", "Carquefou — Flux RSS de l'annuaire de contact",
     "fr_government", "government",
     "https://www.carquefou.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Carquefou — Flux RSS de l'annuaire de contact\",\"government\",\"batch:50\"]",
     10800,
     "Carquefou — Flux RSS de l'annuaire de contact — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_carquefou_fr_post_type_post, "fr-b50-carquefou-fr-post-type-post",
     "Carquefou — Flux RSS des actualités", "Carquefou — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.carquefou.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Carquefou — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Carquefou — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_cca_martinique_com, "fr-b50-cca-martinique-com",
     "Accueil", "Campus Caribéen des arts",
     "fr_government", "government",
     "https://cca-martinique.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Accueil\",\"government\",\"batch:50\"]",
     10800,
     "Accueil — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_celsa_fr, "fr-b50-celsa-fr",
     "CELSA Paris", "École des hautes études en sciences de l'information et de la communication – Celsa",
     "fr_government", "government",
     "https://www.celsa.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CELSA Paris\",\"government\",\"batch:50\"]",
     10800,
     "CELSA Paris — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_cerfav_fr, "fr-b50-cerfav-fr",
     "European Center for Research and Training in Glass Arts", "centre européen de recherches et de formation aux arts verriers",
     "fr_government", "government",
     "https://cerfav.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:European Center for Research and Training in Glass Arts\",\"government\",\"batch:50\"]",
     10800,
     "European Center for Research and Training in Glass Arts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_chemille_en_anjou_fr_post_type_contacts, "fr-b50-chemille-en-anjou-fr-post-type-contacts",
     "Chemillé-en-Anjou — Flux RSS des contacts", "Chemillé-en-Anjou — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.chemille-en-anjou.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Chemillé-en-Anjou — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Chemillé-en-Anjou — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_chemille_en_anjou_fr_post_type_events, "fr-b50-chemille-en-anjou-fr-post-type-events",
     "Chemillé-en-Anjou — Flux RSS des évènements", "Chemillé-en-Anjou — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.chemille-en-anjou.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Chemillé-en-Anjou — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Chemillé-en-Anjou — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_chemille_en_anjou_fr_post_type_post, "fr-b50-chemille-en-anjou-fr-post-type-post",
     "Chemillé-en-Anjou — Flux RSS des actualités", "Chemillé-en-Anjou — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.chemille-en-anjou.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Chemillé-en-Anjou — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Chemillé-en-Anjou — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_citedelagastronomie_dijon_fr, "fr-b50-citedelagastronomie-dijon-fr",
     "International city of gastronomy and wine", "Cité internationale de la gastronomie et du vin",
     "fr_government", "government",
     "https://www.citedelagastronomie-dijon.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:International city of gastronomy and wine\",\"government\",\"batch:50\"]",
     10800,
     "International city of gastronomy and wine — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 1 entries at probe")

RSSX(fr_b50_cnil_fr, "fr-b50-cnil-fr",
     "France CNIL Data Protection", "France CNIL Data Protection",
     "fr_government", "government",
     "https://www.cnil.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France CNIL Data Protection\",\"government\",\"batch:50\"]",
     10800,
     "France CNIL Data Protection — government feed (10 entries at probe)")

RSSX(fr_b50_conservatoire_toulouse_fr, "fr-b50-conservatoire-toulouse-fr",
     "Toulouse Regional Conservatory", "conservatoire à rayonnement régional de Toulouse",
     "fr_government", "government",
     "https://conservatoire.toulouse.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Toulouse Regional Conservatory\",\"government\",\"batch:50\"]",
     10800,
     "Toulouse Regional Conservatory — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_dladl_unistra_fr, "fr-b50-dladl-unistra-fr",
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS global", "département de linguistique appliquée et de didactique des langues — Flux RSS global",
     "fr_government", "government",
     "https://dladl.unistra.fr/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS global\",\"government\",\"batch:50\"]",
     10800,
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS global — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 14 entries at probe")

RSSX(fr_b50_dladl_unistra_fr_actualites, "fr-b50-dladl-unistra-fr-actualites",
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des actualités uniquement", "département de linguistique appliquée et de didactique des langues — Flux RSS des actualités uniquement",
     "fr_government", "government",
     "https://dladl.unistra.fr/actualites/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des actualités uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des actualités uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 4 entries at probe")

RSSX(fr_b50_dladl_unistra_fr_agenda, "fr-b50-dladl-unistra-fr-agenda",
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS de l'agenda uniquement", "département de linguistique appliquée et de didactique des langues — Flux RSS de l'agenda uniquement",
     "fr_government", "government",
     "https://dladl.unistra.fr/agenda/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS de l'agenda uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS de l'agenda uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 3 entries at probe")

RSSX(fr_b50_dladl_unistra_fr_focus, "fr-b50-dladl-unistra-fr-focus",
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des focus uniquement", "département de linguistique appliquée et de didactique des langues — Flux RSS des focus uniquement",
     "fr_government", "government",
     "https://dladl.unistra.fr/focus/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des focus uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des focus uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 3 entries at probe")

RSSX(fr_b50_dladl_unistra_fr_publications, "fr-b50-dladl-unistra-fr-publications",
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des publications uniquement", "département de linguistique appliquée et de didactique des langues — Flux RSS des publications uniquement",
     "fr_government", "government",
     "https://dladl.unistra.fr/publications/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des publications uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Université de Strasbourg, Département de Linguistique Appliquée et de Didactique des Langues Vivantes — Flux RSS des publications uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 4 entries at probe")

RSSX(fr_b50_douanier_rousseau_paysdelaloire_e_lyco_fr, "fr-b50-douanier-rousseau-paysdelaloire-e-lyco-fr",
     "Lycée Douanier Rousseau", "lycée Douanier Rousseau",
     "fr_government", "government",
     "https://douanier-rousseau.paysdelaloire.e-lyco.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Lycée Douanier Rousseau\",\"government\",\"batch:50\"]",
     10800,
     "Lycée Douanier Rousseau — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ebs_paris_fr, "fr-b50-ebs-paris-fr",
     "European Business School Paris", "European Business School Paris",
     "fr_government", "government",
     "https://www.ebs-paris.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:European Business School Paris\",\"government\",\"batch:50\"]",
     10800,
     "European Business School Paris — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 1 entries at probe")

RSSX(fr_b50_ecolecamondo_fr, "fr-b50-ecolecamondo-fr",
     "École Camondo", "École Camondo",
     "fr_government", "government",
     "https://www.ecolecamondo.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:École Camondo\",\"government\",\"batch:50\"]",
     10800,
     "École Camondo — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ecoleql_fr_blog_feed, "fr-b50-ecoleql-fr-blog-feed",
     "Ecole de la Qualité Logicielle — EcoleQL - RSS", "EQL & Afcepf — EcoleQL - RSS",
     "fr_government", "government",
     "https://www.ecoleql.fr/blog-feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Ecole de la Qualité Logicielle — EcoleQL - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Ecole de la Qualité Logicielle — EcoleQL - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 11 entries at probe")

RSSX(fr_b50_efoa_fr, "fr-b50-efoa-fr",
     "EFOA", "EFOA",
     "fr_government", "government",
     "https://www.efoa.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:EFOA\",\"government\",\"batch:50\"]",
     10800,
     "EFOA — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ejdg_fr, "fr-b50-ejdg-fr",
     "École de Journalisme de Grenoble", "école de journalisme de Grenoble",
     "fr_government", "government",
     "http://www.ejdg.fr/index.php/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:École de Journalisme de Grenoble\",\"government\",\"batch:50\"]",
     10800,
     "École de Journalisme de Grenoble — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_elysee_fr, "fr-b50-elysee-fr",
     "Elysee Palace", "Elysee Palace",
     "fr_government", "government",
     "https://www.elysee.fr/feed",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Elysee Palace\",\"government\",\"batch:50\"]",
     10800,
     "Elysee Palace — government feed (50 entries at probe)")

RSSX(fr_b50_emlv_fr, "fr-b50-emlv-fr",
     "Ecole de Management Léonard De Vinci", "École de management Léonard-de-Vinci",
     "fr_government", "government",
     "https://www.emlv.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Ecole de Management Léonard De Vinci\",\"government\",\"batch:50\"]",
     10800,
     "Ecole de Management Léonard De Vinci — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 15 entries at probe")

RSSX(fr_b50_energies_renouvelables_org, "fr-b50-energies-renouvelables-org",
     "Observatoire des énergies renouvelables", "Observ'ER",
     "fr_government", "government",
     "https://energies-renouvelables.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Observatoire des énergies renouvelables\",\"government\",\"batch:50\"]",
     10800,
     "Observatoire des énergies renouvelables — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 2 entries at probe")

RSSX(fr_b50_enoes_com, "fr-b50-enoes-com",
     "ENOES", "École de l'expertise comptable et de l'audit",
     "fr_government", "government",
     "https://www.enoes.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:ENOES\",\"government\",\"batch:50\"]",
     10800,
     "ENOES — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_enscmu_uha_fr, "fr-b50-enscmu-uha-fr",
     "École nationale supérieure de chimie de Mulhouse — ENSCMu | École Nationale Supérieure de Chimie de Mulhouse »", "École nationale supérieure de chimie de Mulhouse — ENSCMu | École Nationale Supérieure de Chimie de Mulhouse »",
     "fr_government", "government",
     "https://www.enscmu.uha.fr/index.php/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:École nationale supérieure de chimie de Mulhouse — ENSCMu | École Nationale Supérieure de Chimie de Mulhouse »\",\"government\",\"batch:50\"]",
     10800,
     "École nationale supérieure de chimie de Mulhouse — ENSCMu | École Nationale Supérieure de Chimie de Mulhouse » — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_epita_fr, "fr-b50-epita-fr",
     "IA Institut", "IA Institut",
     "fr_government", "government",
     "https://www.epita.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:IA Institut\",\"government\",\"batch:50\"]",
     10800,
     "IA Institut — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_esa_n_info_blog_feed, "fr-b50-esa-n-info-blog-feed",
     "Ecole Supérieure d'Art du Nord-Pas de Calais — ESÄ⎪Dk-Tg - RSS", "École supérieure d'art du Nord-Pas-de-Calais — ESÄ⎪Dk-Tg - RSS",
     "fr_government", "government",
     "https://www.esa-n.info/blog-feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Ecole Supérieure d'Art du Nord-Pas de Calais — ESÄ⎪Dk-Tg - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Ecole Supérieure d'Art du Nord-Pas de Calais — ESÄ⎪Dk-Tg - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(fr_b50_esa_quimper_fr, "fr-b50-esa-quimper-fr",
     "Quimper School of Fine Arts", "École européenne supérieure d'art de Bretagne (Quimper)",
     "fr_government", "government",
     "http://www.esa-quimper.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Quimper School of Fine Arts\",\"government\",\"batch:50\"]",
     10800,
     "Quimper School of Fine Arts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 4 entries at probe")

RSSX(fr_b50_esaaix_fr_spip_page_backend, "fr-b50-esaaix-fr-spip-page-backend",
     "Aix-en-Provence School of Art — Syndiquer tout le site", "École supérieure d'art d'Aix-en-Provence — Syndiquer tout le site",
     "fr_government", "government",
     "https://www.esaaix.fr/spip.php?page=backend",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Aix-en-Provence School of Art — Syndiquer tout le site\",\"government\",\"batch:50\"]",
     10800,
     "Aix-en-Provence School of Art — Syndiquer tout le site — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_esaavignon_eu, "fr-b50-esaavignon-eu",
     "École des beaux-arts d'Avignon", "Beaux-Arts d'Avignon",
     "fr_government", "government",
     "https://esaavignon.eu/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:École des beaux-arts d'Avignon\",\"government\",\"batch:50\"]",
     10800,
     "École des beaux-arts d'Avignon — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_esci_paris_eu, "fr-b50-esci-paris-eu",
     "École supérieure de commerce international", "L’ESCI",
     "fr_government", "government",
     "https://esci-paris.eu/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:École supérieure de commerce international\",\"government\",\"batch:50\"]",
     10800,
     "École supérieure de commerce international — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_esci_paris_eu_podcast, "fr-b50-esci-paris-eu-podcast",
     "École supérieure de commerce international — Flux RSS du podcast", "L’ESCI — Flux RSS du podcast",
     "fr_government", "government",
     "https://esci-paris.eu/feed/podcast",
     "fr", "[\"gov\",\"country:fr\",\"publisher:École supérieure de commerce international — Flux RSS du podcast\",\"government\",\"batch:50\"]",
     10800,
     "École supérieure de commerce international — Flux RSS du podcast — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 8 entries at probe")

RSSX(fr_b50_esj_lille_fr, "fr-b50-esj-lille-fr",
     "Lille school of journalism - École supérieure de journalisme de Lille", "École supérieure de journalisme de Lille",
     "fr_government", "government",
     "https://esj-lille.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Lille school of journalism - École supérieure de journalisme de Lille\",\"government\",\"batch:50\"]",
     10800,
     "Lille school of journalism - École supérieure de journalisme de Lille — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 8 entries at probe")

RSSX(fr_b50_etna_io, "fr-b50-etna-io",
     "École des technologies numériques avancées", "École des technologies numériques avancées",
     "fr_government", "government",
     "https://etna.io/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:École des technologies numériques avancées\",\"government\",\"batch:50\"]",
     10800,
     "École des technologies numériques avancées — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_fenelonsaintemarie_org, "fr-b50-fenelonsaintemarie-org",
     "Groupe scolaire Fénelon Sainte-Marie", "Groupe Scolaire",
     "fr_government", "government",
     "https://www.fenelonsaintemarie.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Groupe scolaire Fénelon Sainte-Marie\",\"government\",\"batch:50\"]",
     10800,
     "Groupe scolaire Fénelon Sainte-Marie — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_fonds_clinatec_fr, "fr-b50-fonds-clinatec-fr",
     "Brain Tech Laboratory", "Accueil",
     "fr_government", "government",
     "https://fonds-clinatec.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Brain Tech Laboratory\",\"government\",\"batch:50\"]",
     10800,
     "Brain Tech Laboratory — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_foyerdecachan_org, "fr-b50-foyerdecachan-org",
     "Foyer de Cachan", "foyer de Cachan",
     "fr_government", "government",
     "https://www.foyerdecachan.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Foyer de Cachan\",\"government\",\"batch:50\"]",
     10800,
     "Foyer de Cachan — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_france24_com_fr_afrique, "fr-b50-france24-com-fr-afrique",
     "France 24 FR Africa", "France 24 FR Africa",
     "fr_government", "government",
     "https://www.france24.com/fr/afrique/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Africa\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Africa — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_am_riques, "fr-b50-france24-com-fr-am-riques",
     "France 24 FR Americas", "France 24 FR Americas",
     "fr_government", "government",
     "https://www.france24.com/fr/amériques/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Americas\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Americas — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_asie_pacifique, "fr-b50-france24-com-fr-asie-pacifique",
     "France 24 FR Asia-Pacific", "France 24 FR Asia-Pacific",
     "fr_government", "government",
     "https://www.france24.com/fr/asie-pacifique/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Asia-Pacific\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Asia-Pacific — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_co_tech, "fr-b50-france24-com-fr-co-tech",
     "France 24 FR Economy Tech", "France 24 FR Economy Tech",
     "fr_government", "government",
     "https://www.france24.com/fr/éco-tech/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Economy Tech\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Economy Tech — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_culture, "fr-b50-france24-com-fr-culture",
     "France 24 FR Culture", "France 24 FR Culture",
     "fr_government", "government",
     "https://www.france24.com/fr/culture/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Culture\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Culture — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_europe, "fr-b50-france24-com-fr-europe",
     "France 24 FR Europe", "France 24 FR Europe",
     "fr_government", "government",
     "https://www.france24.com/fr/europe/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Europe\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Europe — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_france, "fr-b50-france24-com-fr-france",
     "France 24 FR France", "France 24 FR France",
     "fr_government", "government",
     "https://www.france24.com/fr/france/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR France\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR France — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_moyen_orient, "fr-b50-france24-com-fr-moyen-orient",
     "France 24 FR Middle East", "France 24 FR Middle East",
     "fr_government", "government",
     "https://www.france24.com/fr/moyen-orient/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Middle East\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Middle East — government feed (30 entries at probe)")

RSSX(fr_b50_france24_com_fr_sports, "fr-b50-france24-com-fr-sports",
     "France 24 FR Sports", "France 24 FR Sports",
     "fr_government", "government",
     "https://www.france24.com/fr/sports/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France 24 FR Sports\",\"government\",\"batch:50\"]",
     10800,
     "France 24 FR Sports — government feed (30 entries at probe)")

RSSX(fr_b50_frapscentre_org, "fr-b50-frapscentre-org",
     "Fédération Régionale des Acteurs en Promotion de la Santé", "Accueil",
     "fr_government", "government",
     "https://frapscentre.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Fédération Régionale des Acteurs en Promotion de la Santé\",\"government\",\"batch:50\"]",
     10800,
     "Fédération Régionale des Acteurs en Promotion de la Santé — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_fresnes94_fr, "fr-b50-fresnes94-fr",
     "Fresnes", "Fresnes",
     "fr_government", "government",
     "https://www.fresnes94.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Fresnes\",\"government\",\"batch:50\"]",
     10800,
     "Fresnes — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 3 entries at probe")

RSSX(fr_b50_galilee_univ_paris13_fr, "fr-b50-galilee-univ-paris13-fr",
     "Institut Galilée", "Institut Galilée",
     "fr_government", "government",
     "https://galilee.univ-paris13.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut Galilée\",\"government\",\"batch:50\"]",
     10800,
     "Institut Galilée — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_gip_cei_com, "fr-b50-gip-cei-com",
     "GIP CEI", "GIP CEI",
     "fr_government", "government",
     "https://gip-cei.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:GIP CEI\",\"government\",\"batch:50\"]",
     10800,
     "GIP CEI — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_grenoblecognition_fr, "fr-b50-grenoblecognition-fr",
     "Autonomie, Gérontologie, E-santé, Imagerie et Société", "Home",
     "fr_government", "government",
     "https://grenoblecognition.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Autonomie, Gérontologie, E-santé, Imagerie et Société\",\"government\",\"batch:50\"]",
     10800,
     "Autonomie, Gérontologie, E-santé, Imagerie et Société — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_groupe_espi_fr, "fr-b50-groupe-espi-fr",
     "ESPI Marseille", "Campus ESPI Marseille",
     "fr_government", "government",
     "https://groupe-espi.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:ESPI Marseille\",\"government\",\"batch:50\"]",
     10800,
     "ESPI Marseille — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_hatvp_fr_post_type_presse, "fr-b50-hatvp-fr-post-type-presse",
     "Accueil", "Accueil",
     "fr_government", "government",
     "https://www.hatvp.fr/feed/?post_type=presse",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Accueil\",\"government\",\"batch:50\"]",
     10800,
     "Accueil — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_hatvp_fr_post_type_ri_actualites, "fr-b50-hatvp-fr-post-type-ri-actualites",
     "Accueil", "Accueil",
     "fr_government", "government",
     "https://www.hatvp.fr/feed/?post_type=ri_actualites",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Accueil\",\"government\",\"batch:50\"]",
     10800,
     "Accueil — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_iat_cnam_fr_adminsite_webservices_export_rss_jsp_nombre_10_code_rubrique_1602766, "fr-b50-iat-cnam-fr-adminsite-webservices-export-rss-jsp-nombre-10-code-rubrique-1602766",
     "Institut aérotechnique — Fil RSS des dix dernières actualités", "Institut aérotechnique — Fil RSS des dix dernières actualités",
     "fr_government", "government",
     "https://iat.cnam.fr/adminsite/webservices/export_rss.jsp?NOMBRE=10&CODE_RUBRIQUE=1602766768314&LANGUE=0",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut aérotechnique — Fil RSS des dix dernières actualités\",\"government\",\"batch:50\"]",
     10800,
     "Institut aérotechnique — Fil RSS des dix dernières actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_iesip_fr, "fr-b50-iesip-fr",
     "Institut d'Enseignement Supérieur Islamique de Paris — IESIP", "Institut d'Enseignement Supérieur Islamique de Paris — IESIP",
     "fr_government", "government",
     "https://www.iesip.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut d'Enseignement Supérieur Islamique de Paris — IESIP\",\"government\",\"batch:50\"]",
     10800,
     "Institut d'Enseignement Supérieur Islamique de Paris — IESIP — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 8 entries at probe")

RSSX(fr_b50_ifpfrance_org, "fr-b50-ifpfrance-org",
     "Institut de Formation Politique", "Institut de formation politique",
     "fr_government", "government",
     "https://ifpfrance.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut de Formation Politique\",\"government\",\"batch:50\"]",
     10800,
     "Institut de Formation Politique — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_inbp_com, "fr-b50-inbp-com",
     "INBP", "Institut national de la boulangerie pâtisserie",
     "fr_government", "government",
     "https://www.inbp.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:INBP\",\"government\",\"batch:50\"]",
     10800,
     "INBP — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 3 entries at probe")

RSSX(fr_b50_inserm_fr, "fr-b50-inserm-fr",
     "INSERM Medical Research", "INSERM Medical Research",
     "fr_government", "government",
     "https://www.inserm.fr/feed",
     "fr", "[\"gov\",\"country:fr\",\"publisher:INSERM Medical Research\",\"government\",\"batch:50\"]",
     10800,
     "INSERM Medical Research — government feed (10 entries at probe)")

RSSX(fr_b50_institut_entreprise_fr, "fr-b50-institut-entreprise-fr",
     "Institut de l'Entreprise", "Institut de l'entreprise",
     "fr_government", "government",
     "https://www.institut-entreprise.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut de l'Entreprise\",\"government\",\"batch:50\"]",
     10800,
     "Institut de l'Entreprise — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_institutionjeanpaul2_fr, "fr-b50-institutionjeanpaul2-fr",
     "Institution Jean-Paul-II", "Institution Jean-Paul-II",
     "fr_government", "government",
     "https://www.institutionjeanpaul2.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institution Jean-Paul-II\",\"government\",\"batch:50\"]",
     10800,
     "Institution Jean-Paul-II — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_intec_cnam_fr_adminsite_webservices_export_rss_jsp_nombre_10_code_rubrique_intec, "fr-b50-intec-cnam-fr-adminsite-webservices-export-rss-jsp-nombre-10-code-rubrique-intec",
     "Institut National des Techniques Économiques et Comptables — Fil RSS des dix dernières actualités", "Institut national des techniques économiques et comptables — Fil RSS des dix dernières actualités",
     "fr_government", "government",
     "https://intec.cnam.fr/adminsite/webservices/export_rss.jsp?NOMBRE=10&CODE_RUBRIQUE=INTEC&LANGUE=0",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut National des Techniques Économiques et Comptables — Fil RSS des dix dernières actualités\",\"government\",\"batch:50\"]",
     10800,
     "Institut National des Techniques Économiques et Comptables — Fil RSS des dix dernières actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_interfor_fr, "fr-b50-interfor-fr",
     "Centre de formation alternance et continue à Amiens", "Interfor",
     "fr_government", "government",
     "https://interfor.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Centre de formation alternance et continue à Amiens\",\"government\",\"batch:50\"]",
     10800,
     "Centre de formation alternance et continue à Amiens — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ipl_fr, "fr-b50-ipl-fr",
     "Institut polytechnique de Lyon", "Institut polytechnique de Lyon",
     "fr_government", "government",
     "https://www.ipl.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut polytechnique de Lyon\",\"government\",\"batch:50\"]",
     10800,
     "Institut polytechnique de Lyon — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_irsn_fr, "fr-b50-irsn-fr",
     "France IRSN Nuclear Safety", "France IRSN Nuclear Safety",
     "fr_government", "government",
     "https://www.irsn.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:France IRSN Nuclear Safety\",\"government\",\"batch:50\"]",
     10800,
     "France IRSN Nuclear Safety — government feed (10 entries at probe)")

RSSX(fr_b50_isblue_fr, "fr-b50-isblue-fr",
     "ISblue — Ecole Universitaire de Recherche en sciences & technologies", "ISblue — Ecole Universitaire de Recherche en sciences & technologies",
     "fr_government", "government",
     "https://isblue.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:ISblue — Ecole Universitaire de Recherche en sciences & technologies\",\"government\",\"batch:50\"]",
     10800,
     "ISblue — Ecole Universitaire de Recherche en sciences & technologies — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_isep_fr, "fr-b50-isep-fr",
     "Institut supérieur d'électronique de Paris", "Institut supérieur d'électronique de Paris",
     "fr_government", "government",
     "https://www.isep.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut supérieur d'électronique de Paris\",\"government\",\"batch:50\"]",
     10800,
     "Institut supérieur d'électronique de Paris — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ismans_cesi_fr, "fr-b50-ismans-cesi-fr",
     "Institut Supérieur des Matériaux du Mans", "Institut supérieur des matériaux et mécaniques avancées",
     "fr_government", "government",
     "https://ismans.cesi.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut Supérieur des Matériaux du Mans\",\"government\",\"batch:50\"]",
     10800,
     "Institut Supérieur des Matériaux du Mans — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ites_unistra_fr, "fr-b50-ites-unistra-fr",
     "Earth and Environment Institute of Strasbourg — Flux RSS global", "Institut Terre et Environnement de Strasbourg — Flux RSS global",
     "fr_government", "government",
     "https://ites.unistra.fr/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Earth and Environment Institute of Strasbourg — Flux RSS global\",\"government\",\"batch:50\"]",
     10800,
     "Earth and Environment Institute of Strasbourg — Flux RSS global — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 30 entries at probe")

RSSX(fr_b50_ites_unistra_fr_actualites, "fr-b50-ites-unistra-fr-actualites",
     "Earth and Environment Institute of Strasbourg — Flux RSS des actualités uniquement", "Institut Terre et Environnement de Strasbourg — Flux RSS des actualités uniquement",
     "fr_government", "government",
     "https://ites.unistra.fr/actualites/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Earth and Environment Institute of Strasbourg — Flux RSS des actualités uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Earth and Environment Institute of Strasbourg — Flux RSS des actualités uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 30 entries at probe")

RSSX(fr_b50_ites_unistra_fr_agenda, "fr-b50-ites-unistra-fr-agenda",
     "Earth and Environment Institute of Strasbourg — Flux RSS de l'agenda uniquement", "Institut Terre et Environnement de Strasbourg — Flux RSS de l'agenda uniquement",
     "fr_government", "government",
     "https://ites.unistra.fr/agenda/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Earth and Environment Institute of Strasbourg — Flux RSS de l'agenda uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Earth and Environment Institute of Strasbourg — Flux RSS de l'agenda uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 30 entries at probe")

RSSX(fr_b50_ites_unistra_fr_focus, "fr-b50-ites-unistra-fr-focus",
     "Earth and Environment Institute of Strasbourg — Flux RSS des focus uniquement", "Institut Terre et Environnement de Strasbourg — Flux RSS des focus uniquement",
     "fr_government", "government",
     "https://ites.unistra.fr/focus/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Earth and Environment Institute of Strasbourg — Flux RSS des focus uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Earth and Environment Institute of Strasbourg — Flux RSS des focus uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 18 entries at probe")

RSSX(fr_b50_ites_unistra_fr_publications, "fr-b50-ites-unistra-fr-publications",
     "Earth and Environment Institute of Strasbourg — Flux RSS des publications uniquement", "Institut Terre et Environnement de Strasbourg — Flux RSS des publications uniquement",
     "fr_government", "government",
     "https://ites.unistra.fr/publications/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Earth and Environment Institute of Strasbourg — Flux RSS des publications uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Earth and Environment Institute of Strasbourg — Flux RSS des publications uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 30 entries at probe")

RSSX(fr_b50_iut_u_pec_fr_adminsite_webservices_export_rss_jsp_nombre_10_code_rubrique_iutcre, "fr-b50-iut-u-pec-fr-adminsite-webservices-export-rss-jsp-nombre-10-code-rubrique-iutcre",
     "Bienvenue à l'IUT de Créteil-Vitry — Fil RSS des dix dernières actualités", "Institut universitaire de technologie de Créteil-Vitry — Fil RSS des dix dernières actualités",
     "fr_government", "government",
     "https://iut.u-pec.fr/adminsite/webservices/export_rss.jsp?NOMBRE=10&CODE_RUBRIQUE=IUTCRET&LANGUE=0",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Bienvenue à l'IUT de Créteil-Vitry — Fil RSS des dix dernières actualités\",\"government\",\"batch:50\"]",
     10800,
     "Bienvenue à l'IUT de Créteil-Vitry — Fil RSS des dix dernières actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_justice_gouv_fr, "fr-b50-justice-gouv-fr",
     "Ministry of Justice", "Ministry of Justice",
     "fr_government", "government",
     "https://www.justice.gouv.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Ministry of Justice\",\"government\",\"batch:50\"]",
     10800,
     "Ministry of Justice — government feed (10 entries at probe)")

RSSX(fr_b50_landerneau_bzh_post_type_contacts, "fr-b50-landerneau-bzh-post-type-contacts",
     "Landerneau — Flux RSS des contacts", "Landerneau — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.landerneau.bzh/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Landerneau — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Landerneau — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_landerneau_bzh_post_type_events, "fr-b50-landerneau-bzh-post-type-events",
     "Landerneau — Flux RSS des évènements", "Landerneau — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.landerneau.bzh/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Landerneau — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Landerneau — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_landerneau_bzh_post_type_post, "fr-b50-landerneau-bzh-post-type-post",
     "Landerneau — Flux RSS des actualités", "Landerneau — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.landerneau.bzh/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Landerneau — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Landerneau — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 17 entries at probe")

RSSX(fr_b50_lapec_univ_avignon_fr, "fr-b50-lapec-univ-avignon-fr",
     "Laboratoire de Physiologie Expérimentale Cardiovasculaire — Laboratoire de Physiologie Expérimentale Cardiovasculaire »", "Laboratoire de Physiologie Expérimentale Cardiovasculaire — Laboratoire de Physiologie Expérimentale Cardiovasculaire »",
     "fr_government", "government",
     "https://lapec.univ-avignon.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Laboratoire de Physiologie Expérimentale Cardiovasculaire — Laboratoire de Physiologie Expérimentale Cardiovasculaire »\",\"government\",\"batch:50\"]",
     10800,
     "Laboratoire de Physiologie Expérimentale Cardiovasculaire — Laboratoire de Physiologie Expérimentale Cardiovasculaire » — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_lapommeraye_fr, "fr-b50-lapommeraye-fr",
     "La Pommeraye", "La Pommeraye",
     "fr_government", "government",
     "https://www.lapommeraye.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:La Pommeraye\",\"government\",\"batch:50\"]",
     10800,
     "La Pommeraye — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 50 entries at probe")

RSSX(fr_b50_lecolefrancaise_fr, "fr-b50-lecolefrancaise-fr",
     "L'École Française", "L'École Française",
     "fr_government", "government",
     "https://lecolefrancaise.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:L'École Française\",\"government\",\"batch:50\"]",
     10800,
     "L'École Française — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 12 entries at probe")

RSSX(fr_b50_lefigaro_fr_figaro_culture, "fr-b50-lefigaro-fr-figaro-culture",
     "Le Figaro Culture", "Le Figaro Culture",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_culture.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Culture\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Culture — government feed (16 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_economie, "fr-b50-lefigaro-fr-figaro-economie",
     "Le Figaro Economy", "Le Figaro Economy",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_economie.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Economy\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Economy — government feed (20 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_flash_eco, "fr-b50-lefigaro-fr-figaro-flash-eco",
     "Le Figaro Flash Economy", "Le Figaro Flash Economy",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_flash-eco.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Flash Economy\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Flash Economy — government feed (20 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_high_tech, "fr-b50-lefigaro-fr-figaro-high-tech",
     "Le Figaro Tech", "Le Figaro Tech",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_high-tech.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Tech\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Tech — government feed (20 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_international, "fr-b50-lefigaro-fr-figaro-international",
     "Le Figaro International", "Le Figaro International",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_international.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro International\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro International — government feed (20 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_politique, "fr-b50-lefigaro-fr-figaro-politique",
     "Le Figaro Politics", "Le Figaro Politics",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_politique.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Politics\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Politics — government feed (20 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_sante, "fr-b50-lefigaro-fr-figaro-sante",
     "Le Figaro Health", "Le Figaro Health",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_sante.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Health\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Health — government feed (20 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_sciences, "fr-b50-lefigaro-fr-figaro-sciences",
     "Le Figaro Science", "Le Figaro Science",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_sciences.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Science\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Science — government feed (20 entries at probe)")

RSSX(fr_b50_lefigaro_fr_figaro_sport, "fr-b50-lefigaro-fr-figaro-sport",
     "Le Figaro Sport", "Le Figaro Sport",
     "fr_government", "government",
     "https://www.lefigaro.fr/rss/figaro_sport.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Figaro Sport\",\"government\",\"batch:50\"]",
     10800,
     "Le Figaro Sport — government feed (20 entries at probe)")

RSSX(fr_b50_legalfamily_fr, "fr-b50-legalfamily-fr",
     "LegalFamily", "Formations continues homologuées CNB pour avocats",
     "fr_government", "government",
     "https://legalfamily.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:LegalFamily\",\"government\",\"batch:50\"]",
     10800,
     "LegalFamily — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_lemonde_fr_culture_rss_full, "fr-b50-lemonde-fr-culture-rss-full",
     "Le Monde Culture", "Le Monde Culture",
     "fr_government", "government",
     "https://www.lemonde.fr/culture/rss_full.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Monde Culture\",\"government\",\"batch:50\"]",
     10800,
     "Le Monde Culture — government feed (20 entries at probe)")

RSSX(fr_b50_lemonde_fr_economie_rss_full, "fr-b50-lemonde-fr-economie-rss-full",
     "Le Monde Economy", "Le Monde Economy",
     "fr_government", "government",
     "https://www.lemonde.fr/economie/rss_full.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Monde Economy\",\"government\",\"batch:50\"]",
     10800,
     "Le Monde Economy — government feed (20 entries at probe)")

RSSX(fr_b50_lemonde_fr_en_continu, "fr-b50-lemonde-fr-en-continu",
     "Le Monde Live", "Le Monde Live",
     "fr_government", "government",
     "https://www.lemonde.fr/rss/en_continu.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Monde Live\",\"government\",\"batch:50\"]",
     10800,
     "Le Monde Live — government feed (60 entries at probe)")

RSSX(fr_b50_lemonde_fr_international_rss_full, "fr-b50-lemonde-fr-international-rss-full",
     "Le Monde International", "Le Monde International",
     "fr_government", "government",
     "https://www.lemonde.fr/international/rss_full.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Monde International\",\"government\",\"batch:50\"]",
     10800,
     "Le Monde International — government feed (20 entries at probe)")

RSSX(fr_b50_lemonde_fr_politique_rss_full, "fr-b50-lemonde-fr-politique-rss-full",
     "Le Monde Politics", "Le Monde Politics",
     "fr_government", "government",
     "https://www.lemonde.fr/politique/rss_full.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Monde Politics\",\"government\",\"batch:50\"]",
     10800,
     "Le Monde Politics — government feed (20 entries at probe)")

RSSX(fr_b50_lemonde_fr_sciences_rss_full, "fr-b50-lemonde-fr-sciences-rss-full",
     "Le Monde Science", "Le Monde Science",
     "fr_government", "government",
     "https://www.lemonde.fr/sciences/rss_full.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Monde Science\",\"government\",\"batch:50\"]",
     10800,
     "Le Monde Science — government feed (20 entries at probe)")

RSSX(fr_b50_lemonde_fr_sport_rss_full, "fr-b50-lemonde-fr-sport-rss-full",
     "Le Monde Sport", "Le Monde Sport",
     "fr_government", "government",
     "https://www.lemonde.fr/sport/rss_full.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Monde Sport\",\"government\",\"batch:50\"]",
     10800,
     "Le Monde Sport — government feed (20 entries at probe)")

RSSX(fr_b50_levalentin_education, "fr-b50-levalentin-education",
     "EPLEFPA Le Valentin – Valence", "lycée d'enseignement général et technologique agricole Le Valentin de Bourg-lès-Valence",
     "fr_government", "government",
     "https://levalentin.education/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:EPLEFPA Le Valentin – Valence\",\"government\",\"batch:50\"]",
     10800,
     "EPLEFPA Le Valentin – Valence — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 12 entries at probe")

RSSX(fr_b50_live_virtualabs_fr_videos, "fr-b50-live-virtualabs-fr-videos",
     "Le hangar de Virtu — Le hangar de Virtu - Videos feed", "Le hangar de Virtu — Le hangar de Virtu - Videos feed",
     "fr_government", "government",
     "https://live.virtualabs.fr/feeds/videos.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le hangar de Virtu — Le hangar de Virtu - Videos feed\",\"government\",\"batch:50\"]",
     10800,
     "Le hangar de Virtu — Le hangar de Virtu - Videos feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 8 entries at probe")

RSSX(fr_b50_loire_authion_fr_post_type_affichage_liste, "fr-b50-loire-authion-fr-post-type-affichage-liste",
     "Loire-Authion — Flux RSS des affichages listes", "Loire-Authion — Flux RSS des affichages listes",
     "fr_government", "government",
     "https://www.loire-authion.fr/feed/?post_type=affichage_liste",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Loire-Authion — Flux RSS des affichages listes\",\"government\",\"batch:50\"]",
     10800,
     "Loire-Authion — Flux RSS des affichages listes — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_loire_authion_fr_post_type_agenda, "fr-b50-loire-authion-fr-post-type-agenda",
     "Loire-Authion — Flux RSS de l'agenda", "Loire-Authion — Flux RSS de l'agenda",
     "fr_government", "government",
     "https://www.loire-authion.fr/feed/?post_type=agenda",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Loire-Authion — Flux RSS de l'agenda\",\"government\",\"batch:50\"]",
     10800,
     "Loire-Authion — Flux RSS de l'agenda — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_loire_authion_fr_post_type_contacts, "fr-b50-loire-authion-fr-post-type-contacts",
     "Loire-Authion — Flux RSS de l'annuaire de contact", "Loire-Authion — Flux RSS de l'annuaire de contact",
     "fr_government", "government",
     "https://www.loire-authion.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Loire-Authion — Flux RSS de l'annuaire de contact\",\"government\",\"batch:50\"]",
     10800,
     "Loire-Authion — Flux RSS de l'annuaire de contact — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_loire_authion_fr_post_type_navigation_profils, "fr-b50-loire-authion-fr-post-type-navigation-profils",
     "Loire-Authion — Flux RSS des différents profils", "Loire-Authion — Flux RSS des différents profils",
     "fr_government", "government",
     "https://www.loire-authion.fr/feed/?post_type=navigation_profils",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Loire-Authion — Flux RSS des différents profils\",\"government\",\"batch:50\"]",
     10800,
     "Loire-Authion — Flux RSS des différents profils — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 7 entries at probe")

RSSX(fr_b50_loire_authion_fr_post_type_post, "fr-b50-loire-authion-fr-post-type-post",
     "Loire-Authion — Flux RSS des actualités", "Loire-Authion — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.loire-authion.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Loire-Authion — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Loire-Authion — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_lorraine_inp_fr, "fr-b50-lorraine-inp-fr",
     "Lorraine INP", "Institut national polytechnique de Lorraine",
     "fr_government", "government",
     "https://www.lorraine-inp.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Lorraine INP\",\"government\",\"batch:50\"]",
     10800,
     "Lorraine INP — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_lunel_com, "fr-b50-lunel-com",
     "Lunel", "Lunel",
     "fr_government", "government",
     "https://www.lunel.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Lunel\",\"government\",\"batch:50\"]",
     10800,
     "Lunel — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_lvts_fr, "fr-b50-lvts-fr",
     "Laboratory For Vascular Translational Science", "Laboratoire de recherche vasculaire translationnelle",
     "fr_government", "government",
     "https://lvts.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Laboratory For Vascular Translational Science\",\"government\",\"batch:50\"]",
     10800,
     "Laboratory For Vascular Translational Science — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 500 entries at probe")

RSSX(fr_b50_mairie_balma_fr_post_type_contacts, "fr-b50-mairie-balma-fr-post-type-contacts",
     "Balma — Flux RSS des contacts", "Balma — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.mairie-balma.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Balma — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Balma — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_mairie_balma_fr_post_type_events, "fr-b50-mairie-balma-fr-post-type-events",
     "Balma — Flux RSS des évènements", "Balma — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.mairie-balma.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Balma — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Balma — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_mairie_balma_fr_post_type_post, "fr-b50-mairie-balma-fr-post-type-post",
     "Balma — Flux RSS des actualités", "Balma — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.mairie-balma.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Balma — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Balma — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 14 entries at probe")

RSSX(fr_b50_mairie_begles_fr, "fr-b50-mairie-begles-fr",
     "Bègles", "Bègles",
     "fr_government", "government",
     "https://www.mairie-begles.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Bègles\",\"government\",\"batch:50\"]",
     10800,
     "Bègles — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_mairie_orly_fr_post_type_contacts, "fr-b50-mairie-orly-fr-post-type-contacts",
     "Orly — Flux RSS des contacts", "Orly — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.mairie-orly.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Orly — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Orly — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 30 entries at probe")

RSSX(fr_b50_mairie_orly_fr_post_type_events, "fr-b50-mairie-orly-fr-post-type-events",
     "Orly — Flux RSS des évènements", "Orly — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.mairie-orly.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Orly — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Orly — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 30 entries at probe")

RSSX(fr_b50_mairie_orly_fr_post_type_post, "fr-b50-mairie-orly-fr-post-type-post",
     "Orly — Flux RSS des actualités", "Orly — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.mairie-orly.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Orly — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Orly — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 30 entries at probe")

RSSX(fr_b50_mairie_toul_fr_spip_page_backend, "fr-b50-mairie-toul-fr-spip-page-backend",
     "Toul — Syndiquer tout le site", "Toul — Syndiquer tout le site",
     "fr_government", "government",
     "http://www.mairie-toul.fr/spip.php?page=backend",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Toul — Syndiquer tout le site\",\"government\",\"batch:50\"]",
     10800,
     "Toul — Syndiquer tout le site — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_mauges_sur_loire_fr_post_type_contacts, "fr-b50-mauges-sur-loire-fr-post-type-contacts",
     "Mauges-sur-Loire — Flux RSS des contacts", "Mauges-sur-Loire — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.mauges-sur-loire.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Mauges-sur-Loire — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Mauges-sur-Loire — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_mauges_sur_loire_fr_post_type_events, "fr-b50-mauges-sur-loire-fr-post-type-events",
     "Mauges-sur-Loire — Flux RSS des évènements", "Mauges-sur-Loire — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.mauges-sur-loire.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Mauges-sur-Loire — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Mauges-sur-Loire — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_mauges_sur_loire_fr_post_type_post, "fr-b50-mauges-sur-loire-fr-post-type-post",
     "Mauges-sur-Loire — Flux RSS des actualités", "Mauges-sur-Loire — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.mauges-sur-loire.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Mauges-sur-Loire — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Mauges-sur-Loire — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_metropole_rennes_fr, "fr-b50-metropole-rennes-fr",
     "Rennes — Rennes Ville et Métropole", "Rennes — Rennes Ville et Métropole",
     "fr_government", "government",
     "https://metropole.rennes.fr/index.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Rennes — Rennes Ville et Métropole\",\"government\",\"batch:50\"]",
     10800,
     "Rennes — Rennes Ville et Métropole — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 100 entries at probe")

RSSX(fr_b50_montrevaultsurevre_fr, "fr-b50-montrevaultsurevre-fr",
     "Montrevault-sur-Èvre", "Montrevault-sur-Èvre",
     "fr_government", "government",
     "https://www.montrevaultsurevre.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Montrevault-sur-Èvre\",\"government\",\"batch:50\"]",
     10800,
     "Montrevault-sur-Èvre — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_morsang_fr_post_type_contacts, "fr-b50-morsang-fr-post-type-contacts",
     "Morsang-sur-Orge — Flux RSS des contacts", "Morsang-sur-Orge — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.morsang.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Morsang-sur-Orge — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Morsang-sur-Orge — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_morsang_fr_post_type_events, "fr-b50-morsang-fr-post-type-events",
     "Morsang-sur-Orge — Flux RSS des évènements", "Morsang-sur-Orge — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.morsang.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Morsang-sur-Orge — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Morsang-sur-Orge — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_morsang_fr_post_type_post, "fr-b50-morsang-fr-post-type-post",
     "Morsang-sur-Orge — Flux RSS des actualités", "Morsang-sur-Orge — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.morsang.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Morsang-sur-Orge — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Morsang-sur-Orge — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 39 entries at probe")

RSSX(fr_b50_parisinstitute_org, "fr-b50-parisinstitute-org",
     "Paris Institute for Critical Thinking — Paris Institute articles", "Paris Institute for Critical Thinking — Paris Institute articles",
     "fr_government", "government",
     "https://parisinstitute.org/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Paris Institute for Critical Thinking — Paris Institute articles\",\"government\",\"batch:50\"]",
     10800,
     "Paris Institute for Critical Thinking — Paris Institute articles — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 50 entries at probe")

RSSX(fr_b50_peertube_easter_fr_videos, "fr-b50-peertube-easter-fr-videos",
     "PeerTube — PeerTube - Videos feed", "PeerTube — PeerTube - Videos feed",
     "fr_government", "government",
     "https://peertube.easter.fr/feeds/videos.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:PeerTube — PeerTube - Videos feed\",\"government\",\"batch:50\"]",
     10800,
     "PeerTube — PeerTube - Videos feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(fr_b50_prim_univ_tours_fr_adminsite_webservices_export_rss_jsp_nombre_10_code_rubrique, "fr-b50-prim-univ-tours-fr-adminsite-webservices-export-rss-jsp-nombre-10-code-rubrique",
     "Pratiques et ressources de l’information et des médiations — Fil RSS des dix dernières actualités", "PRIM — Fil RSS des dix dernières actualités",
     "fr_government", "government",
     "https://prim.univ-tours.fr/adminsite/webservices/export_rss.jsp?NOMBRE=10&CODE_RUBRIQUE=PRIM&LANGUE=0",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Pratiques et ressources de l’information et des médiations — Fil RSS des dix dernières actualités\",\"government\",\"batch:50\"]",
     10800,
     "Pratiques et ressources de l’information et des médiations — Fil RSS des dix dernières actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_region_haute_normandie_com, "fr-b50-region-haute-normandie-com",
     "Upper Normandy", "Haute-Normandie",
     "fr_government", "government",
     "https://www.region-haute-normandie.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Upper Normandy\",\"government\",\"batch:50\"]",
     10800,
     "Upper Normandy — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_rfi_fr_en, "fr-b50-rfi-fr-en",
     "RFI EN", "RFI EN",
     "fr_government", "government",
     "https://www.rfi.fr/en/rss",
     "en", "[\"gov\",\"country:fr\",\"publisher:RFI EN\",\"government\",\"batch:50\"]",
     10800,
     "RFI EN — government feed (22 entries at probe)")

RSSX(fr_b50_rfi_fr_fr_am_riques, "fr-b50-rfi-fr-fr-am-riques",
     "RFI FR Americas", "RFI FR Americas",
     "fr_government", "government",
     "https://www.rfi.fr/fr/amériques/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:RFI FR Americas\",\"government\",\"batch:50\"]",
     10800,
     "RFI FR Americas — government feed (30 entries at probe)")

RSSX(fr_b50_rfi_fr_fr_asie_pacifique, "fr-b50-rfi-fr-fr-asie-pacifique",
     "RFI FR Asia-Pacific", "RFI FR Asia-Pacific",
     "fr_government", "government",
     "https://www.rfi.fr/fr/asie-pacifique/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:RFI FR Asia-Pacific\",\"government\",\"batch:50\"]",
     10800,
     "RFI FR Asia-Pacific — government feed (30 entries at probe)")

RSSX(fr_b50_rfi_fr_fr_conomie, "fr-b50-rfi-fr-fr-conomie",
     "RFI FR Economy", "RFI FR Economy",
     "fr_government", "government",
     "https://www.rfi.fr/fr/économie/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:RFI FR Economy\",\"government\",\"batch:50\"]",
     10800,
     "RFI FR Economy — government feed (30 entries at probe)")

RSSX(fr_b50_rfi_fr_fr_culture, "fr-b50-rfi-fr-fr-culture",
     "RFI FR Culture", "RFI FR Culture",
     "fr_government", "government",
     "https://www.rfi.fr/fr/culture/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:RFI FR Culture\",\"government\",\"batch:50\"]",
     10800,
     "RFI FR Culture — government feed (15 entries at probe)")

RSSX(fr_b50_rfi_fr_fr_europe, "fr-b50-rfi-fr-fr-europe",
     "RFI FR Europe", "RFI FR Europe",
     "fr_government", "government",
     "https://www.rfi.fr/fr/europe/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:RFI FR Europe\",\"government\",\"batch:50\"]",
     10800,
     "RFI FR Europe — government feed (30 entries at probe)")

RSSX(fr_b50_rfi_fr_fr_france, "fr-b50-rfi-fr-fr-france",
     "RFI FR France", "RFI FR France",
     "fr_government", "government",
     "https://www.rfi.fr/fr/france/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:RFI FR France\",\"government\",\"batch:50\"]",
     10800,
     "RFI FR France — government feed (30 entries at probe)")

RSSX(fr_b50_rfi_fr_fr_sports, "fr-b50-rfi-fr-fr-sports",
     "RFI FR Sports", "RFI FR Sports",
     "fr_government", "government",
     "https://www.rfi.fr/fr/sports/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:RFI FR Sports\",\"government\",\"batch:50\"]",
     10800,
     "RFI FR Sports — government feed (15 entries at probe)")

RSSX(fr_b50_saint_ouen_fr, "fr-b50-saint-ouen-fr",
     "Saint-Ouen-sur-Seine", "Saint-Ouen-sur-Seine",
     "fr_government", "government",
     "https://www.saint-ouen.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Saint-Ouen-sur-Seine\",\"government\",\"batch:50\"]",
     10800,
     "Saint-Ouen-sur-Seine — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_saint_serge_net, "fr-b50-saint-serge-net",
     "St. Sergius Institute", "Institut Saint-Serge",
     "fr_government", "government",
     "http://www.saint-serge.net/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:St. Sergius Institute\",\"government\",\"batch:50\"]",
     10800,
     "St. Sergius Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 15 entries at probe")

RSSX(fr_b50_sainte_luce_loire_com_post_type_affichage_liste, "fr-b50-sainte-luce-loire-com-post-type-affichage-liste",
     "Sainte-Luce-sur-Loire — Flux RSS des affichages listes", "Sainte-Luce-sur-Loire — Flux RSS des affichages listes",
     "fr_government", "government",
     "https://www.sainte-luce-loire.com/feed/?post_type=affichage_liste",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sainte-Luce-sur-Loire — Flux RSS des affichages listes\",\"government\",\"batch:50\"]",
     10800,
     "Sainte-Luce-sur-Loire — Flux RSS des affichages listes — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 77 entries at probe")

RSSX(fr_b50_sainte_luce_loire_com_post_type_agenda, "fr-b50-sainte-luce-loire-com-post-type-agenda",
     "Sainte-Luce-sur-Loire — Flux RSS de l'agenda", "Sainte-Luce-sur-Loire — Flux RSS de l'agenda",
     "fr_government", "government",
     "https://www.sainte-luce-loire.com/feed/?post_type=agenda",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sainte-Luce-sur-Loire — Flux RSS de l'agenda\",\"government\",\"batch:50\"]",
     10800,
     "Sainte-Luce-sur-Loire — Flux RSS de l'agenda — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 100 entries at probe")

RSSX(fr_b50_sainte_luce_loire_com_post_type_base_documentaire, "fr-b50-sainte-luce-loire-com-post-type-base-documentaire",
     "Sainte-Luce-sur-Loire — Flux RSS de la base documentaire", "Sainte-Luce-sur-Loire — Flux RSS de la base documentaire",
     "fr_government", "government",
     "https://www.sainte-luce-loire.com/feed/?post_type=base_documentaire",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sainte-Luce-sur-Loire — Flux RSS de la base documentaire\",\"government\",\"batch:50\"]",
     10800,
     "Sainte-Luce-sur-Loire — Flux RSS de la base documentaire — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 100 entries at probe")

RSSX(fr_b50_sainte_luce_loire_com_post_type_contacts, "fr-b50-sainte-luce-loire-com-post-type-contacts",
     "Sainte-Luce-sur-Loire — Flux RSS de l'annuaire de contact", "Sainte-Luce-sur-Loire — Flux RSS de l'annuaire de contact",
     "fr_government", "government",
     "https://www.sainte-luce-loire.com/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sainte-Luce-sur-Loire — Flux RSS de l'annuaire de contact\",\"government\",\"batch:50\"]",
     10800,
     "Sainte-Luce-sur-Loire — Flux RSS de l'annuaire de contact — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 100 entries at probe")

RSSX(fr_b50_sainte_luce_loire_com_post_type_post, "fr-b50-sainte-luce-loire-com-post-type-post",
     "Sainte-Luce-sur-Loire — Flux RSS des actualités", "Sainte-Luce-sur-Loire — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.sainte-luce-loire.com/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sainte-Luce-sur-Loire — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Sainte-Luce-sur-Loire — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 100 entries at probe")

RSSX(fr_b50_saintleu_re, "fr-b50-saintleu-re",
     "Saint-Leu", "Saint-Leu",
     "fr_government", "government",
     "https://www.saintleu.re/feed",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Saint-Leu\",\"government\",\"batch:50\"]",
     10800,
     "Saint-Leu — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_saran_fr, "fr-b50-saran-fr",
     "Saran — Ville de Saran RSS", "Saran — Ville de Saran RSS",
     "fr_government", "government",
     "https://www.saran.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Saran — Ville de Saran RSS\",\"government\",\"batch:50\"]",
     10800,
     "Saran — Ville de Saran RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 5 entries at probe")

RSSX(fr_b50_saran_fr_calendrier_saranrss_2026_10, "fr-b50-saran-fr-calendrier-saranrss-2026-10",
     "Saran — Agenda de Saran", "Saran — Agenda de Saran",
     "fr_government", "government",
     "https://www.saran.fr/calendrier/saranrss.rss/2026-10",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Saran — Agenda de Saran\",\"government\",\"batch:50\"]",
     10800,
     "Saran — Agenda de Saran — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(fr_b50_sciencespo_saintgermainenlaye_fr, "fr-b50-sciencespo-saintgermainenlaye-fr",
     "Institut d'études politiques de Saint-Germain-en-Laye", "Institut d'études politiques de Saint-Germain-en-Laye",
     "fr_government", "government",
     "https://www.sciencespo-saintgermainenlaye.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut d'études politiques de Saint-Germain-en-Laye\",\"government\",\"batch:50\"]",
     10800,
     "Institut d'études politiques de Saint-Germain-en-Laye — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_seismology_epos_france_fr, "fr-b50-seismology-epos-france-fr",
     "EPOS-FRANCE Seismology", "EPOS-FRANCE Seismology",
     "fr_government", "government",
     "https://seismology.epos-france.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:EPOS-FRANCE Seismology\",\"government\",\"batch:50\"]",
     10800,
     "EPOS-FRANCE Seismology — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 7 entries at probe")

RSSX(fr_b50_senat_fr_presse, "fr-b50-senat-fr-presse",
     "French Senate Press", "French Senate Press",
     "fr_government", "government",
     "https://www.senat.fr/rss/presse.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:French Senate Press\",\"government\",\"batch:50\"]",
     10800,
     "French Senate Press — government feed (5 entries at probe)")

RSSX(fr_b50_senat_fr_rapports, "fr-b50-senat-fr-rapports",
     "French Senate Reports", "French Senate Reports",
     "fr_government", "government",
     "https://www.senat.fr/rss/rapports.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:French Senate Reports\",\"government\",\"batch:50\"]",
     10800,
     "French Senate Reports — government feed (36 entries at probe)")

RSSX(fr_b50_senat_fr_textes, "fr-b50-senat-fr-textes",
     "French Senate Texts", "French Senate Texts",
     "fr_government", "government",
     "https://www.senat.fr/rss/textes.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:French Senate Texts\",\"government\",\"batch:50\"]",
     10800,
     "French Senate Texts — government feed (38 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss1, "fr-b50-senat-fr-themes-therss1",
     "Sénat Agriculture", "Sénat Agriculture",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss1.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Agriculture\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Agriculture — government feed (121 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss10, "fr-b50-senat-fr-themes-therss10",
     "Sénat Local Government", "Sénat Local Government",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss10.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Local Government\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Local Government — government feed (174 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss11, "fr-b50-senat-fr-themes-therss11",
     "Sénat Institutions", "Sénat Institutions",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss11.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Institutions\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Institutions — government feed (46 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss12, "fr-b50-senat-fr-themes-therss12",
     "Sénat Justice", "Sénat Justice",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss12.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Justice\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Justice — government feed (55 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss13, "fr-b50-senat-fr-themes-therss13",
     "Sénat Law", "Sénat Law",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss13.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Law\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Law — government feed (62 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss14, "fr-b50-senat-fr-themes-therss14",
     "Sénat Overseas", "Sénat Overseas",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss14.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Overseas\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Overseas — government feed (104 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss15, "fr-b50-senat-fr-themes-therss15",
     "Sénat Research", "Sénat Research",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss15.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Research\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Research — government feed (42 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss16, "fr-b50-senat-fr-themes-therss16",
     "Sénat Security", "Sénat Security",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss16.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Security\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Security — government feed (59 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss17, "fr-b50-senat-fr-themes-therss17",
     "Sénat Social Affairs", "Sénat Social Affairs",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss17.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Social Affairs\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Social Affairs — government feed (330 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss18, "fr-b50-senat-fr-themes-therss18",
     "Sénat Transport", "Sénat Transport",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss18.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Transport\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Transport — government feed (75 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss19, "fr-b50-senat-fr-themes-therss19",
     "Sénat Digital", "Sénat Digital",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss19.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Digital\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Digital — government feed (19 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss2, "fr-b50-senat-fr-themes-therss2",
     "Sénat Budget", "Sénat Budget",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss2.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Budget\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Budget — government feed (60 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss20, "fr-b50-senat-fr-themes-therss20",
     "Sénat Europe", "Sénat Europe",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss20.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Europe\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Europe — government feed (209 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss3, "fr-b50-senat-fr-themes-therss3",
     "Sénat Culture Education", "Sénat Culture Education",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss3.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Culture Education\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Culture Education — government feed (418 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss4, "fr-b50-senat-fr-themes-therss4",
     "Sénat Foreign Affairs", "Sénat Foreign Affairs",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss4.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Foreign Affairs\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Foreign Affairs — government feed (112 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss5, "fr-b50-senat-fr-themes-therss5",
     "Sénat Defence", "Sénat Defence",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss5.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Defence\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Defence — government feed (76 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss7, "fr-b50-senat-fr-themes-therss7",
     "Sénat Economy", "Sénat Economy",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss7.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Economy\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Economy — government feed (94 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss8, "fr-b50-senat-fr-themes-therss8",
     "Sénat Environment", "Sénat Environment",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss8.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Environment\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Environment — government feed (113 entries at probe)")

RSSX(fr_b50_senat_fr_themes_therss9, "fr-b50-senat-fr-themes-therss9",
     "Sénat Finance", "Sénat Finance",
     "fr_government", "government",
     "https://www.senat.fr/themes/rss/therss9.rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sénat Finance\",\"government\",\"batch:50\"]",
     10800,
     "Sénat Finance — government feed (201 entries at probe)")

RSSX(fr_b50_service_public_pf_dsp, "fr-b50-service-public-pf-dsp",
     "Directorate of Health of French Polynesia", "Direction de la santé de Polynésie française",
     "fr_government", "government",
     "https://www.service-public.pf/dsp/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Directorate of Health of French Polynesia\",\"government\",\"batch:50\"]",
     10800,
     "Directorate of Health of French Polynesia — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_sfu_paris_fr, "fr-b50-sfu-paris-fr",
     "Sigmund Freud Privatuniversität - Standort Paris", "SFU",
     "fr_government", "government",
     "https://sfu-paris.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sigmund Freud Privatuniversität - Standort Paris\",\"government\",\"batch:50\"]",
     10800,
     "Sigmund Freud Privatuniversität - Standort Paris — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_site_ac_aix_marseille_fr_lyc_thiers_spip_spip_page_backend, "fr-b50-site-ac-aix-marseille-fr-lyc-thiers-spip-spip-page-backend",
     "Lycée Thiers — Syndiquer tout le site", "lycée Thiers — Syndiquer tout le site",
     "fr_government", "government",
     "https://www.site.ac-aix-marseille.fr/lyc-thiers/spip/spip.php?page=backend",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Lycée Thiers — Syndiquer tout le site\",\"government\",\"batch:50\"]",
     10800,
     "Lycée Thiers — Syndiquer tout le site — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_staubinlasalle_fr, "fr-b50-staubinlasalle-fr",
     "ACCUEIL", "lycée La Baronnerie",
     "fr_government", "government",
     "https://www.staubinlasalle.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:ACCUEIL\",\"government\",\"batch:50\"]",
     10800,
     "ACCUEIL — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_stcharles_stecroix_org, "fr-b50-stcharles-stecroix-org",
     "Accueil — Ensemble scolaire Saint-Charles Sainte Croix - Le Mans » Flu", "lycée Sainte-Croix — Ensemble scolaire Saint-Charles Sainte Croix - Le Mans » Flu",
     "fr_government", "government",
     "https://stcharles-stecroix.org/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Accueil — Ensemble scolaire Saint-Charles Sainte Croix - Le Mans » Flu\",\"government\",\"batch:50\"]",
     10800,
     "Accueil — Ensemble scolaire Saint-Charles Sainte Croix - Le Mans » Flu — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 12 entries at probe")

RSSX(fr_b50_theocatho_unistra_fr, "fr-b50-theocatho-unistra-fr",
     "Institut de droit canonique — Flux RSS global", "Institut de droit canonique — Flux RSS global",
     "fr_government", "government",
     "https://theocatho.unistra.fr/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut de droit canonique — Flux RSS global\",\"government\",\"batch:50\"]",
     10800,
     "Institut de droit canonique — Flux RSS global — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 13 entries at probe")

RSSX(fr_b50_theocatho_unistra_fr_actualites, "fr-b50-theocatho-unistra-fr-actualites",
     "Institut de droit canonique — Flux RSS des actualités uniquement", "Institut de droit canonique — Flux RSS des actualités uniquement",
     "fr_government", "government",
     "https://theocatho.unistra.fr/actualites/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut de droit canonique — Flux RSS des actualités uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Institut de droit canonique — Flux RSS des actualités uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 12 entries at probe")

RSSX(fr_b50_theocatho_unistra_fr_agenda, "fr-b50-theocatho-unistra-fr-agenda",
     "Institut de droit canonique — Flux RSS de l'agenda uniquement", "Institut de droit canonique — Flux RSS de l'agenda uniquement",
     "fr_government", "government",
     "https://theocatho.unistra.fr/agenda/feed.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Institut de droit canonique — Flux RSS de l'agenda uniquement\",\"government\",\"batch:50\"]",
     10800,
     "Institut de droit canonique — Flux RSS de l'agenda uniquement — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 1 entries at probe")

RSSX(fr_b50_toulouse_dominicains_com, "fr-b50-toulouse-dominicains-com",
     "Couvent Saint-Thomas-d'Aquin", "couvent Saint-Thomas d'Aquin",
     "fr_government", "government",
     "https://toulouse.dominicains.com/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Couvent Saint-Thomas-d'Aquin\",\"government\",\"batch:50\"]",
     10800,
     "Couvent Saint-Thomas-d'Aquin — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_transport_data_gouv_fr, "fr-b50-transport-data-gouv-fr",
     "Le Point d’Accès National aux données ouvertes de transport — Resources feed", "Le Point d’Accès National aux données ouvertes de transport — Resources feed",
     "fr_government", "government",
     "https://transport.data.gouv.fr/atom.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Le Point d’Accès National aux données ouvertes de transport — Resources feed\",\"government\",\"batch:50\"]",
     10800,
     "Le Point d’Accès National aux données ouvertes de transport — Resources feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 791 entries at probe")

RSSX(fr_b50_travail_emploi_gouv_fr, "fr-b50-travail-emploi-gouv-fr",
     "Ministry of Labour", "Ministry of Labour",
     "fr_government", "government",
     "https://travail-emploi.gouv.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Ministry of Labour\",\"government\",\"batch:50\"]",
     10800,
     "Ministry of Labour — government feed (50 entries at probe)")

RSSX(fr_b50_udp_sorbonne_fr, "fr-b50-udp-sorbonne-fr",
     "Accueil", "université des patients",
     "fr_government", "government",
     "https://www.udp-sorbonne.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Accueil\",\"government\",\"batch:50\"]",
     10800,
     "Accueil — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_utbm_fr, "fr-b50-utbm-fr",
     "Université de technologie de Belfort-Montbéliard", "université de technologie de Belfort-Montbéliard",
     "fr_government", "government",
     "https://www.utbm.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Université de technologie de Belfort-Montbéliard\",\"government\",\"batch:50\"]",
     10800,
     "Université de technologie de Belfort-Montbéliard — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ville_avrille_fr_post_type_contacts, "fr-b50-ville-avrille-fr-post-type-contacts",
     "Avrillé — Flux RSS des contacts", "Avrillé — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.ville-avrille.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Avrillé — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Avrillé — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_avrille_fr_post_type_events, "fr-b50-ville-avrille-fr-post-type-events",
     "Avrillé — Flux RSS des évènements", "Avrillé — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.ville-avrille.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Avrillé — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Avrillé — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_avrille_fr_post_type_jobs, "fr-b50-ville-avrille-fr-post-type-jobs",
     "Avrillé — Flux RSS des offres d'emploi", "Avrillé — Flux RSS des offres d'emploi",
     "fr_government", "government",
     "https://www.ville-avrille.fr/feed/?post_type=jobs",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Avrillé — Flux RSS des offres d'emploi\",\"government\",\"batch:50\"]",
     10800,
     "Avrillé — Flux RSS des offres d'emploi — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ville_avrille_fr_post_type_post, "fr-b50-ville-avrille-fr-post-type-post",
     "Avrillé — Flux RSS des actualités", "Avrillé — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.ville-avrille.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Avrillé — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Avrillé — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_biscarrosse_fr_post_type_contacts, "fr-b50-ville-biscarrosse-fr-post-type-contacts",
     "Biscarrosse — Flux RSS des contacts", "Biscarrosse — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.ville-biscarrosse.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Biscarrosse — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Biscarrosse — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_biscarrosse_fr_post_type_events, "fr-b50-ville-biscarrosse-fr-post-type-events",
     "Biscarrosse — Flux RSS des évènements", "Biscarrosse — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.ville-biscarrosse.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Biscarrosse — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Biscarrosse — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_biscarrosse_fr_post_type_post, "fr-b50-ville-biscarrosse-fr-post-type-post",
     "Biscarrosse — Flux RSS des actualités", "Biscarrosse — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.ville-biscarrosse.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Biscarrosse — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Biscarrosse — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 18 entries at probe")

RSSX(fr_b50_ville_bressuire_fr_post_type_contacts, "fr-b50-ville-bressuire-fr-post-type-contacts",
     "Bressuire — Flux RSS des contacts", "Bressuire — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.ville-bressuire.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Bressuire — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Bressuire — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_bressuire_fr_post_type_events, "fr-b50-ville-bressuire-fr-post-type-events",
     "Bressuire — Flux RSS des évènements", "Bressuire — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.ville-bressuire.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Bressuire — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Bressuire — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_bressuire_fr_post_type_post, "fr-b50-ville-bressuire-fr-post-type-post",
     "Bressuire — Flux RSS des actualités", "Bressuire — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.ville-bressuire.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Bressuire — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Bressuire — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_cagnes_fr, "fr-b50-ville-cagnes-fr",
     "Cagnes-sur-Mer", "Cagnes-sur-Mer",
     "fr_government", "government",
     "https://ville.cagnes.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Cagnes-sur-Mer\",\"government\",\"batch:50\"]",
     10800,
     "Cagnes-sur-Mer — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 1 entries at probe")

RSSX(fr_b50_ville_cesson_sevigne_fr_post_type_contacts, "fr-b50-ville-cesson-sevigne-fr-post-type-contacts",
     "Cesson-Sévigné — Flux RSS des contacts", "Cesson-Sévigné — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.ville-cesson-sevigne.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Cesson-Sévigné — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Cesson-Sévigné — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_cesson_sevigne_fr_post_type_events, "fr-b50-ville-cesson-sevigne-fr-post-type-events",
     "Cesson-Sévigné — Flux RSS des évènements", "Cesson-Sévigné — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.ville-cesson-sevigne.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Cesson-Sévigné — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Cesson-Sévigné — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_cesson_sevigne_fr_post_type_post, "fr-b50-ville-cesson-sevigne-fr-post-type-post",
     "Cesson-Sévigné — Flux RSS des actualités", "Cesson-Sévigné — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.ville-cesson-sevigne.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Cesson-Sévigné — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Cesson-Sévigné — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(fr_b50_ville_chilly_mazarin_fr, "fr-b50-ville-chilly-mazarin-fr",
     "Chilly-Mazarin", "Chilly-Mazarin",
     "fr_government", "government",
     "https://www.ville-chilly-mazarin.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Chilly-Mazarin\",\"government\",\"batch:50\"]",
     10800,
     "Chilly-Mazarin — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 100 entries at probe")

RSSX(fr_b50_ville_cormeilles95_fr_post_type_contacts, "fr-b50-ville-cormeilles95-fr-post-type-contacts",
     "Cormeilles-en-Parisis — Flux RSS des contacts", "Cormeilles-en-Parisis — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.ville-cormeilles95.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Cormeilles-en-Parisis — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Cormeilles-en-Parisis — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_cormeilles95_fr_post_type_events, "fr-b50-ville-cormeilles95-fr-post-type-events",
     "Cormeilles-en-Parisis — Flux RSS des évènements", "Cormeilles-en-Parisis — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.ville-cormeilles95.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Cormeilles-en-Parisis — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Cormeilles-en-Parisis — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_cormeilles95_fr_post_type_post, "fr-b50-ville-cormeilles95-fr-post-type-post",
     "Cormeilles-en-Parisis — Flux RSS des actualités", "Cormeilles-en-Parisis — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.ville-cormeilles95.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Cormeilles-en-Parisis — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Cormeilles-en-Parisis — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_creteil_fr, "fr-b50-ville-creteil-fr",
     "Créteil", "Créteil",
     "fr_government", "government",
     "https://www.ville-creteil.fr/rss",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Créteil\",\"government\",\"batch:50\"]",
     10800,
     "Créteil — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 20 entries at probe")

RSSX(fr_b50_ville_grasse_fr_post_type_contacts, "fr-b50-ville-grasse-fr-post-type-contacts",
     "Grasse — Flux RSS des contacts", "Grasse — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.ville-grasse.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Grasse — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Grasse — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_grasse_fr_post_type_events, "fr-b50-ville-grasse-fr-post-type-events",
     "Grasse — Flux RSS des évènements", "Grasse — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.ville-grasse.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Grasse — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Grasse — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 8 entries at probe")

RSSX(fr_b50_ville_grasse_fr_post_type_post, "fr-b50-ville-grasse-fr-post-type-post",
     "Grasse — Flux RSS des actualités", "Grasse — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.ville-grasse.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Grasse — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Grasse — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 5 entries at probe")

RSSX(fr_b50_ville_hayange_fr, "fr-b50-ville-hayange-fr",
     "Hayange", "Hayange",
     "fr_government", "government",
     "https://ville-hayange.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Hayange\",\"government\",\"batch:50\"]",
     10800,
     "Hayange — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ville_petitbourg_fr, "fr-b50-ville-petitbourg-fr",
     "Petit-Bourg — Bienvenue sur le site officiel de la Ville de Petit-Bourg (G", "Petit-Bourg — Bienvenue sur le site officiel de la Ville de Petit-Bourg (G",
     "fr_government", "government",
     "https://www.ville-petitbourg.fr/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Petit-Bourg — Bienvenue sur le site officiel de la Ville de Petit-Bourg (G\",\"government\",\"batch:50\"]",
     10800,
     "Petit-Bourg — Bienvenue sur le site officiel de la Ville de Petit-Bourg (G — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 50 entries at probe")

RSSX(fr_b50_ville_saintemarie_re, "fr-b50-ville-saintemarie-re",
     "Sainte-Marie", "Sainte-Marie",
     "fr_government", "government",
     "https://www.ville-saintemarie.re/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Sainte-Marie\",\"government\",\"batch:50\"]",
     10800,
     "Sainte-Marie — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

RSSX(fr_b50_ville_viroflay_fr_post_type_contacts, "fr-b50-ville-viroflay-fr-post-type-contacts",
     "Viroflay — Flux RSS des contacts", "Viroflay — Flux RSS des contacts",
     "fr_government", "government",
     "https://www.ville-viroflay.fr/feed/?post_type=contacts",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Viroflay — Flux RSS des contacts\",\"government\",\"batch:50\"]",
     10800,
     "Viroflay — Flux RSS des contacts — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_viroflay_fr_post_type_events, "fr-b50-ville-viroflay-fr-post-type-events",
     "Viroflay — Flux RSS des évènements", "Viroflay — Flux RSS des évènements",
     "fr_government", "government",
     "https://www.ville-viroflay.fr/feed/?post_type=events",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Viroflay — Flux RSS des évènements\",\"government\",\"batch:50\"]",
     10800,
     "Viroflay — Flux RSS des évènements — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 40 entries at probe")

RSSX(fr_b50_ville_viroflay_fr_post_type_post, "fr-b50-ville-viroflay-fr-post-type-post",
     "Viroflay — Flux RSS des actualités", "Viroflay — Flux RSS des actualités",
     "fr_government", "government",
     "https://www.ville-viroflay.fr/feed/?post_type=post",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Viroflay — Flux RSS des actualités\",\"government\",\"batch:50\"]",
     10800,
     "Viroflay — Flux RSS des actualités — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 16 entries at probe")

RSSX(fr_b50_wpvit_efb_fr, "fr-b50-wpvit-efb-fr",
     "Vocational Training School of the Paris Court of Appeal", "École de formation professionnelle des barreaux de la cour d'appel de Paris",
     "fr_government", "government",
     "https://wpvit.efb.fr/index.php/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Vocational Training School of the Paris Court of Appeal\",\"government\",\"batch:50\"]",
     10800,
     "Vocational Training School of the Paris Court of Appeal — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 10 entries at probe")

/* ── fr_research ─────────────────────────────────────────────────────── */

RSSX(fr_b50_ceea_edu, "fr-b50-ceea-edu",
     "Conservatoire européen d'écriture audiovisuelle", "Conservatoire européen d'écriture audiovisuelle",
     "fr_research", "science",
     "https://ceea.edu/feed/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:Conservatoire européen d'écriture audiovisuelle\",\"science\",\"batch:50\"]",
     10800,
     "Conservatoire européen d'écriture audiovisuelle — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 99 entries at probe")

RSSX(fr_b50_cnes_fr, "fr-b50-cnes-fr",
     "CNES Space Agency", "CNES Space Agency",
     "fr_research", "science",
     "https://www.cnes.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNES Space Agency\",\"science\",\"batch:50\"]",
     10800,
     "CNES Space Agency — science feed (10 entries at probe)")

RSSX(fr_b50_cnrs_fr, "fr-b50-cnrs-fr",
     "CNRS National", "CNRS National",
     "fr_research", "science",
     "https://www.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS National\",\"science\",\"batch:50\"]",
     10800,
     "CNRS National — science feed (10 entries at probe)")

RSSX(fr_b50_inc_cnrs_fr, "fr-b50-inc-cnrs-fr",
     "CNRS Chemistry (INC)", "CNRS Chemistry (INC)",
     "fr_research", "science",
     "https://www.inc.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Chemistry (INC)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Chemistry (INC) — science feed (10 entries at probe)")

RSSX(fr_b50_inc_cnrs_fr_fr, "fr-b50-inc-cnrs-fr-fr",
     "CNRS INC Chemistry", "CNRS INC",
     "fr_research", "science",
     "https://www.inc.cnrs.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS INC Chemistry\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INC Chemistry — science feed (10 entries at probe)")

RSSX(fr_b50_inee_cnrs_fr, "fr-b50-inee-cnrs-fr",
     "CNRS Ecology (INEE)", "CNRS Ecology (INEE)",
     "fr_research", "science",
     "https://www.inee.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Ecology (INEE)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Ecology (INEE) — science feed (2 entries at probe)")

RSSX(fr_b50_inee_cnrs_fr_fr, "fr-b50-inee-cnrs-fr-fr",
     "CNRS INEE Ecology", "CNRS INEE",
     "fr_research", "science",
     "https://www.inee.cnrs.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS INEE Ecology\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INEE Ecology — science feed (2 entries at probe)")

RSSX(fr_b50_inp_cnrs_fr, "fr-b50-inp-cnrs-fr",
     "CNRS Physics (INP)", "CNRS Physics (INP)",
     "fr_research", "science",
     "https://www.inp.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Physics (INP)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Physics (INP) — science feed (8 entries at probe)")

RSSX(fr_b50_inp_cnrs_fr_fr, "fr-b50-inp-cnrs-fr-fr",
     "CNRS INP Physics", "CNRS INP",
     "fr_research", "science",
     "https://www.inp.cnrs.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS INP Physics\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INP Physics — science feed (8 entries at probe)")

RSSX(fr_b50_ins2i_cnrs_fr, "fr-b50-ins2i-cnrs-fr",
     "CNRS Computing (INS2I)", "CNRS Computing (INS2I)",
     "fr_research", "science",
     "https://www.ins2i.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Computing (INS2I)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Computing (INS2I) — science feed (4 entries at probe)")

RSSX(fr_b50_ins2i_cnrs_fr_fr, "fr-b50-ins2i-cnrs-fr-fr",
     "CNRS INS2I Computer Science", "CNRS INS2I",
     "fr_research", "science",
     "https://ins2i.cnrs.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS INS2I Computer Science\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INS2I Computer Science — science feed (4 entries at probe)")

RSSX(fr_b50_insb_cnrs_fr, "fr-b50-insb-cnrs-fr",
     "CNRS Biology (INSB)", "CNRS Biology (INSB)",
     "fr_research", "science",
     "https://www.insb.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Biology (INSB)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Biology (INSB) — science feed (3 entries at probe)")

RSSX(fr_b50_insb_cnrs_fr_fr, "fr-b50-insb-cnrs-fr-fr",
     "CNRS INSB Biology", "CNRS INSB",
     "fr_research", "science",
     "https://insb.cnrs.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS INSB Biology\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INSB Biology — science feed (3 entries at probe)")

RSSX(fr_b50_inserm_fr_2, "fr-b50-inserm-fr-2",
     "INSERM", "INSERM",
     "fr_research", "science",
     "https://www.inserm.fr/rss/",
     "fr", "[\"gov\",\"country:fr\",\"publisher:INSERM\",\"science\",\"batch:50\"]",
     10800,
     "INSERM — science feed (10 entries at probe)")

RSSX(fr_b50_inshs_cnrs_fr, "fr-b50-inshs-cnrs-fr",
     "CNRS Humanities (INSHS)", "CNRS Humanities (INSHS)",
     "fr_research", "science",
     "https://www.inshs.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Humanities (INSHS)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Humanities (INSHS) — science feed (5 entries at probe)")

RSSX(fr_b50_inshs_cnrs_fr_fr, "fr-b50-inshs-cnrs-fr-fr",
     "CNRS INSHS Social Sciences", "CNRS INSHS",
     "fr_research", "science",
     "https://www.inshs.cnrs.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS INSHS Social Sciences\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INSHS Social Sciences — science feed (5 entries at probe)")

RSSX(fr_b50_insis_cnrs_fr, "fr-b50-insis-cnrs-fr",
     "CNRS Engineering (INSIS)", "CNRS Engineering (INSIS)",
     "fr_research", "science",
     "https://www.insis.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Engineering (INSIS)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Engineering (INSIS) — science feed (4 entries at probe)")

RSSX(fr_b50_insmi_cnrs_fr, "fr-b50-insmi-cnrs-fr",
     "CNRS Mathematics (INSMI)", "CNRS Mathematics (INSMI)",
     "fr_research", "science",
     "https://www.insmi.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Mathematics (INSMI)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Mathematics (INSMI) — science feed (5 entries at probe)")

RSSX(fr_b50_insmi_cnrs_fr_fr, "fr-b50-insmi-cnrs-fr-fr",
     "CNRS INSMI Mathematics", "CNRS INSMI",
     "fr_research", "science",
     "https://insmi.cnrs.fr/fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS INSMI Mathematics\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INSMI Mathematics — science feed (5 entries at probe)")

RSSX(fr_b50_insu_cnrs_fr, "fr-b50-insu-cnrs-fr",
     "CNRS Earth Sciences (INSU)", "CNRS Earth Sciences (INSU)",
     "fr_research", "science",
     "https://www.insu.cnrs.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:CNRS Earth Sciences (INSU)\",\"science\",\"batch:50\"]",
     10800,
     "CNRS Earth Sciences (INSU) — science feed (10 entries at probe)")

RSSX(fr_b50_insu_cnrs_fr_en, "fr-b50-insu-cnrs-fr-en",
     "CNRS INSU Earth Sciences", "CNRS INSU",
     "fr_research", "science",
     "https://www.insu.cnrs.fr/en/rss.xml",
     "en", "[\"gov\",\"country:fr\",\"publisher:CNRS INSU Earth Sciences\",\"science\",\"batch:50\"]",
     10800,
     "CNRS INSU Earth Sciences — science feed (3 entries at probe)")

RSSX(fr_b50_ird_fr, "fr-b50-ird-fr",
     "IRD Development Research", "IRD Development Research",
     "fr_research", "science",
     "https://www.ird.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:IRD Development Research\",\"science\",\"batch:50\"]",
     10800,
     "IRD Development Research — science feed (10 entries at probe)")

RSSX(fr_b50_onera_fr, "fr-b50-onera-fr",
     "ONERA Aerospace Research", "ONERA Aerospace Research",
     "fr_research", "science",
     "https://www.onera.fr/rss.xml",
     "fr", "[\"gov\",\"country:fr\",\"publisher:ONERA Aerospace Research\",\"science\",\"batch:50\"]",
     10800,
     "ONERA Aerospace Research — science feed (3 entries at probe)")

/* ── news ────────────────────────────────────────────────────────────── */

RSSX(fr_b50_bfmtv_com_news_24_7, "fr-b50-bfmtv-com-news-24-7",
     "BFM TV", "BFM TV",
     "news", "media",
     "https://www.bfmtv.com/rss/news-24-7/",
     "fr", "[\"news\",\"country:fr\",\"publisher:BFM TV\",\"media\",\"batch:50\"]",
     3600,
     "BFM TV — news feed (30 entries at probe)")

RSSX(fr_b50_liberation_fr_arc_outboundfeeds_rss_all, "fr-b50-liberation-fr-arc-outboundfeeds-rss-all",
     "Liberation", "Liberation",
     "news", "media",
     "https://www.liberation.fr/arc/outboundfeeds/rss-all/",
     "fr", "[\"news\",\"country:fr\",\"publisher:Liberation\",\"media\",\"batch:50\"]",
     3600,
     "Liberation — news feed (50 entries at probe)")
