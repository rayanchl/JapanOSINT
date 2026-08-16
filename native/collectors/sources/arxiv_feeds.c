/* arXiv research-category feeds (deterministic real RSS at rss.arxiv.org). Early-signal scientific/technical intelligence, via rss_collect. */
#include "source.h"
#include "lib/rss_atom.h"

#include "_source_macros.inc"

RSSX(arxiv_cs_cr, "arxiv-cs-cr", "arXiv cs.CR — Cryptography and Security", "arXiv cs.CR", "osint", "news",
  "https://rss.arxiv.org/rss/cs.CR", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.cr\"]", 21600,
  "arXiv cs.CR new submissions — Cryptography and Security");

RSSX(arxiv_cs_ai, "arxiv-cs-ai", "arXiv cs.AI — Artificial Intelligence", "arXiv cs.AI", "osint", "news",
  "https://rss.arxiv.org/rss/cs.AI", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ai\"]", 21600,
  "arXiv cs.AI new submissions — Artificial Intelligence");

RSSX(arxiv_cs_lg, "arxiv-cs-lg", "arXiv cs.LG — Machine Learning", "arXiv cs.LG", "osint", "news",
  "https://rss.arxiv.org/rss/cs.LG", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.lg\"]", 21600,
  "arXiv cs.LG new submissions — Machine Learning");

RSSX(arxiv_cs_cl, "arxiv-cs-cl", "arXiv cs.CL — Computation and Language (NLP)", "arXiv cs.CL", "osint", "news",
  "https://rss.arxiv.org/rss/cs.CL", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.cl\"]", 21600,
  "arXiv cs.CL new submissions — Computation and Language (NLP)");

RSSX(arxiv_cs_cv, "arxiv-cs-cv", "arXiv cs.CV — Computer Vision", "arXiv cs.CV", "osint", "news",
  "https://rss.arxiv.org/rss/cs.CV", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.cv\"]", 21600,
  "arXiv cs.CV new submissions — Computer Vision");

RSSX(arxiv_cs_ni, "arxiv-cs-ni", "arXiv cs.NI — Networking and Internet Architecture", "arXiv cs.NI", "osint", "news",
  "https://rss.arxiv.org/rss/cs.NI", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ni\"]", 21600,
  "arXiv cs.NI new submissions — Networking and Internet Architecture");

RSSX(arxiv_cs_cy, "arxiv-cs-cy", "arXiv cs.CY — Computers and Society", "arXiv cs.CY", "osint", "news",
  "https://rss.arxiv.org/rss/cs.CY", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.cy\"]", 21600,
  "arXiv cs.CY new submissions — Computers and Society");

RSSX(arxiv_cs_dc, "arxiv-cs-dc", "arXiv cs.DC — Distributed and Cluster Computing", "arXiv cs.DC", "osint", "news",
  "https://rss.arxiv.org/rss/cs.DC", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.dc\"]", 21600,
  "arXiv cs.DC new submissions — Distributed and Cluster Computing");

RSSX(arxiv_cs_se, "arxiv-cs-se", "arXiv cs.SE — Software Engineering", "arXiv cs.SE", "osint", "news",
  "https://rss.arxiv.org/rss/cs.SE", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.se\"]", 21600,
  "arXiv cs.SE new submissions — Software Engineering");

RSSX(arxiv_cs_ro, "arxiv-cs-ro", "arXiv cs.RO — Robotics", "arXiv cs.RO", "osint", "news",
  "https://rss.arxiv.org/rss/cs.RO", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ro\"]", 21600,
  "arXiv cs.RO new submissions — Robotics");

RSSX(arxiv_cs_db, "arxiv-cs-db", "arXiv cs.DB — Databases", "arXiv cs.DB", "osint", "news",
  "https://rss.arxiv.org/rss/cs.DB", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.db\"]", 21600,
  "arXiv cs.DB new submissions — Databases");

RSSX(arxiv_cs_ir, "arxiv-cs-ir", "arXiv cs.IR — Information Retrieval", "arXiv cs.IR", "osint", "news",
  "https://rss.arxiv.org/rss/cs.IR", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ir\"]", 21600,
  "arXiv cs.IR new submissions — Information Retrieval");

RSSX(arxiv_cs_gt, "arxiv-cs-gt", "arXiv cs.GT — Computer Science and Game Theory", "arXiv cs.GT", "osint", "news",
  "https://rss.arxiv.org/rss/cs.GT", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.gt\"]", 21600,
  "arXiv cs.GT new submissions — Computer Science and Game Theory");

RSSX(arxiv_cs_ma, "arxiv-cs-ma", "arXiv cs.MA — Multiagent Systems", "arXiv cs.MA", "osint", "news",
  "https://rss.arxiv.org/rss/cs.MA", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ma\"]", 21600,
  "arXiv cs.MA new submissions — Multiagent Systems");

RSSX(arxiv_cs_os, "arxiv-cs-os", "arXiv cs.OS — Operating Systems", "arXiv cs.OS", "osint", "news",
  "https://rss.arxiv.org/rss/cs.OS", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.os\"]", 21600,
  "arXiv cs.OS new submissions — Operating Systems");

RSSX(arxiv_cs_pl, "arxiv-cs-pl", "arXiv cs.PL — Programming Languages", "arXiv cs.PL", "osint", "news",
  "https://rss.arxiv.org/rss/cs.PL", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.pl\"]", 21600,
  "arXiv cs.PL new submissions — Programming Languages");

RSSX(arxiv_cs_ar, "arxiv-cs-ar", "arXiv cs.AR — Hardware Architecture", "arXiv cs.AR", "osint", "news",
  "https://rss.arxiv.org/rss/cs.AR", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ar\"]", 21600,
  "arXiv cs.AR new submissions — Hardware Architecture");

RSSX(arxiv_cs_ne, "arxiv-cs-ne", "arXiv cs.NE — Neural and Evolutionary Computing", "arXiv cs.NE", "osint", "news",
  "https://rss.arxiv.org/rss/cs.NE", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ne\"]", 21600,
  "arXiv cs.NE new submissions — Neural and Evolutionary Computing");

RSSX(arxiv_cs_si, "arxiv-cs-si", "arXiv cs.SI — Social and Information Networks", "arXiv cs.SI", "osint", "news",
  "https://rss.arxiv.org/rss/cs.SI", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.si\"]", 21600,
  "arXiv cs.SI new submissions — Social and Information Networks");

RSSX(arxiv_cs_et, "arxiv-cs-et", "arXiv cs.ET — Emerging Technologies", "arXiv cs.ET", "osint", "news",
  "https://rss.arxiv.org/rss/cs.ET", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.et\"]", 21600,
  "arXiv cs.ET new submissions — Emerging Technologies");

RSSX(arxiv_cs_ds, "arxiv-cs-ds", "arXiv cs.DS — Data Structures and Algorithms", "arXiv cs.DS", "osint", "news",
  "https://rss.arxiv.org/rss/cs.DS", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.ds\"]", 21600,
  "arXiv cs.DS new submissions — Data Structures and Algorithms");

RSSX(arxiv_cs_hc, "arxiv-cs-hc", "arXiv cs.HC — Human-Computer Interaction", "arXiv cs.HC", "osint", "news",
  "https://rss.arxiv.org/rss/cs.HC", "en", "[\"research\",\"arxiv\",\"preprint\",\"cs.hc\"]", 21600,
  "arXiv cs.HC new submissions — Human-Computer Interaction");

RSSX(arxiv_eess_sp, "arxiv-eess-sp", "arXiv eess.SP — Signal Processing", "arXiv eess.SP", "osint", "news",
  "https://rss.arxiv.org/rss/eess.SP", "en", "[\"research\",\"arxiv\",\"preprint\",\"eess.sp\"]", 21600,
  "arXiv eess.SP new submissions — Signal Processing");

RSSX(arxiv_eess_sy, "arxiv-eess-sy", "arXiv eess.SY — Systems and Control", "arXiv eess.SY", "osint", "news",
  "https://rss.arxiv.org/rss/eess.SY", "en", "[\"research\",\"arxiv\",\"preprint\",\"eess.sy\"]", 21600,
  "arXiv eess.SY new submissions — Systems and Control");

RSSX(arxiv_stat_ml, "arxiv-stat-ml", "arXiv stat.ML — Statistics - Machine Learning", "arXiv stat.ML", "osint", "news",
  "https://rss.arxiv.org/rss/stat.ML", "en", "[\"research\",\"arxiv\",\"preprint\",\"stat.ml\"]", 21600,
  "arXiv stat.ML new submissions — Statistics - Machine Learning");

RSSX(arxiv_quant_ph, "arxiv-quant-ph", "arXiv quant-ph — Quantum Physics", "arXiv quant-ph", "osint", "news",
  "https://rss.arxiv.org/rss/quant-ph", "en", "[\"research\",\"arxiv\",\"preprint\",\"quant-ph\"]", 21600,
  "arXiv quant-ph new submissions — Quantum Physics");

RSSX(arxiv_physics_soc_ph, "arxiv-physics-soc-ph", "arXiv physics.soc-ph — Physics and Society", "arXiv physics.soc-ph", "osint", "news",
  "https://rss.arxiv.org/rss/physics.soc-ph", "en", "[\"research\",\"arxiv\",\"preprint\",\"physics.soc-ph\"]", 21600,
  "arXiv physics.soc-ph new submissions — Physics and Society");

RSSX(arxiv_math_oc, "arxiv-math-oc", "arXiv math.OC — Optimization and Control", "arXiv math.OC", "osint", "news",
  "https://rss.arxiv.org/rss/math.OC", "en", "[\"research\",\"arxiv\",\"preprint\",\"math.oc\"]", 21600,
  "arXiv math.OC new submissions — Optimization and Control");

RSSX(arxiv_econ_gn, "arxiv-econ-gn", "arXiv econ.GN — General Economics", "arXiv econ.GN", "osint", "news",
  "https://rss.arxiv.org/rss/econ.GN", "en", "[\"research\",\"arxiv\",\"preprint\",\"econ.gn\"]", 21600,
  "arXiv econ.GN new submissions — General Economics");

RSSX(arxiv_q_fin_gn, "arxiv-q-fin-gn", "arXiv q-fin.GN — General Finance", "arXiv q-fin.GN", "osint", "news",
  "https://rss.arxiv.org/rss/q-fin.GN", "en", "[\"research\",\"arxiv\",\"preprint\",\"q-fin.gn\"]", 21600,
  "arXiv q-fin.GN new submissions — General Finance");

RSSX(arxiv_astro_ph_ep, "arxiv-astro-ph-ep", "arXiv astro-ph.EP — Earth and Planetary Astrophysics", "arXiv astro-ph.EP", "osint", "news",
  "https://rss.arxiv.org/rss/astro-ph.EP", "en", "[\"research\",\"arxiv\",\"preprint\",\"astro-ph.ep\"]", 21600,
  "arXiv astro-ph.EP new submissions — Earth and Planetary Astrophysics");

RSSX(arxiv_astro_ph_im, "arxiv-astro-ph-im", "arXiv astro-ph.IM — Astrophysics Instrumentation", "arXiv astro-ph.IM", "osint", "news",
  "https://rss.arxiv.org/rss/astro-ph.IM", "en", "[\"research\",\"arxiv\",\"preprint\",\"astro-ph.im\"]", 21600,
  "arXiv astro-ph.IM new submissions — Astrophysics Instrumentation");

RSSX(arxiv_physics_geo_ph, "arxiv-physics-geo-ph", "arXiv physics.geo-ph — Geophysics", "arXiv physics.geo-ph", "osint", "news",
  "https://rss.arxiv.org/rss/physics.geo-ph", "en", "[\"research\",\"arxiv\",\"preprint\",\"physics.geo-ph\"]", 21600,
  "arXiv physics.geo-ph new submissions — Geophysics");

RSSX(arxiv_physics_space_ph, "arxiv-physics-space-ph", "arXiv physics.space-ph — Space Physics", "arXiv physics.space-ph", "osint", "news",
  "https://rss.arxiv.org/rss/physics.space-ph", "en", "[\"research\",\"arxiv\",\"preprint\",\"physics.space-ph\"]", 21600,
  "arXiv physics.space-ph new submissions — Space Physics");

RSSX(arxiv_physics_ao_ph, "arxiv-physics-ao-ph", "arXiv physics.ao-ph — Atmospheric and Oceanic Physics", "arXiv physics.ao-ph", "osint", "news",
  "https://rss.arxiv.org/rss/physics.ao-ph", "en", "[\"research\",\"arxiv\",\"preprint\",\"physics.ao-ph\"]", 21600,
  "arXiv physics.ao-ph new submissions — Atmospheric and Oceanic Physics");

RSSX(arxiv_physics_med_ph, "arxiv-physics-med-ph", "arXiv physics.med-ph — Medical Physics", "arXiv physics.med-ph", "osint", "news",
  "https://rss.arxiv.org/rss/physics.med-ph", "en", "[\"research\",\"arxiv\",\"preprint\",\"physics.med-ph\"]", 21600,
  "arXiv physics.med-ph new submissions — Medical Physics");

RSSX(arxiv_q_bio_pe, "arxiv-q-bio-pe", "arXiv q-bio.PE — Populations and Evolution", "arXiv q-bio.PE", "osint", "news",
  "https://rss.arxiv.org/rss/q-bio.PE", "en", "[\"research\",\"arxiv\",\"preprint\",\"q-bio.pe\"]", 21600,
  "arXiv q-bio.PE new submissions — Populations and Evolution");

RSSX(arxiv_nucl_ex, "arxiv-nucl-ex", "arXiv nucl-ex — Nuclear Experiment", "arXiv nucl-ex", "osint", "news",
  "https://rss.arxiv.org/rss/nucl-ex", "en", "[\"research\",\"arxiv\",\"preprint\",\"nucl-ex\"]", 21600,
  "arXiv nucl-ex new submissions — Nuclear Experiment");

RSSX(arxiv_cond_mat_mtrl_sci, "arxiv-cond-mat-mtrl-sci", "arXiv cond-mat.mtrl-sci — Materials Science", "arXiv cond-mat.mtrl-sci", "osint", "news",
  "https://rss.arxiv.org/rss/cond-mat.mtrl-sci", "en", "[\"research\",\"arxiv\",\"preprint\",\"cond-mat.mtrl-sci\"]", 21600,
  "arXiv cond-mat.mtrl-sci new submissions — Materials Science");

RSSX(arxiv_physics_optics, "arxiv-physics-optics", "arXiv physics.optics — Optics", "arXiv physics.optics", "osint", "news",
  "https://rss.arxiv.org/rss/physics.optics", "en", "[\"research\",\"arxiv\",\"preprint\",\"physics.optics\"]", 21600,
  "arXiv physics.optics new submissions — Optics");
