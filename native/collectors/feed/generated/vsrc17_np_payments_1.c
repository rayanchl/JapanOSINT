/* Verified-live np_payments sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(np_bipad_incident_relief, "np-bipad-incident-relief", "Nepal BIPAD - relief distribution to named beneficiaries", "Nepal BIPAD - relief distribution to named beneficiaries",
  "np_payments", "payments",
  "https://bipadportal.gov.np/api/v1/incident-relief/?format=json&limit=50",
  "results",
  "en", "[\"np\",\"payments\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Named-beneficiary payment records: nameOfBeneficiary (e.g. Purna Bahadur Prajapati and Gyan Maya Prajapati), numberOfBeneficiaryFamily, reliefAmountNpr, dateOfReliefDistribution, causeOfIncident and totals benefited by sex, minority and Dalit status. Ties public money to named recipients.");
