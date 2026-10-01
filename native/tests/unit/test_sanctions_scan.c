/* tests/unit/test_sanctions_scan.c — contract test for the CSV name screen in
 * collectors/sources/sanctions_world.c.
 *
 * WHAT IS BEING PROTECTED. A false positive in a sanctions screen is a real
 * person wrongly reported as designated, and the code path that produces one
 * is invisible in normal use: the run "succeeds", records are emitted, and
 * every one of them is a genuine line from the official file — just not a line
 * that carries the searched NAME. Screening the whole CSV row screens the
 * narrative columns too, so the query "Putin" returned 708 UK OFSI records
 * (Abramovich, Abakarov, …) whose Statement of Reasons happens to mention him.
 * Measured on the live ConList.csv on 2026-08-24: 708 rows contain "Putin"
 * anywhere, 11 contain it in the six NAME columns.
 *
 * sw_scan_lines() therefore takes a DELIMITER and a 1-based inclusive column
 * RANGE. It used to take a single count meaning "how many LEADING
 * COMMA-separated columns are the name", which could not describe the EU
 * consolidated export at ANY value: that file is semicolon-delimited and its
 * name fields sit at columns 17-20, so the EU row was pinned at "screen the
 * whole 113-column line".
 *
 * Every fixture below is REAL bytes, copied from the live exports:
 *   - the EU header + two rows from
 *     webgate.ec.europa.eu/fsd/fsf/public/files/csvFullSanctionsList_1_1
 *   - two rows from ofsistorage.blob.core.windows.net/.../ConList.csv
 * both fetched 2026-08-24. Fixtures invented by hand would not have caught
 * this: the defect is entirely about where the real files put their columns.
 *
 * It includes sanctions_world.c directly so it can reach the static
 * sw_scan_lines(); the harness links every object EXCEPT
 * obj/collectors/sources/sanctions_world.o and obj/main.o. See
 * tests/unit/run.sh. */

#include "../../collectors/sources/sanctions_world.c"

#include <assert.h>

/* ── capturing sink ──────────────────────────────────────────────────────── */
#define CAPMAX 64
static char g_names[CAPMAX][256];
static int  g_n = 0;

static int cap_emit(struct intel_sink *s, const intel_item *it) {
  (void)s;
  if (g_n >= CAPMAX) return -1;
  snprintf(g_names[g_n], sizeof g_names[0], "%s", it->title ? it->title : "");
  g_n++;
  return 1;
}

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}

/* Run the screen and report how many records it emitted. */
static int screen(const char *body, const char *q, char delim, int lo, int hi) {
  g_n = 0;
  intel_sink sink = { .ctx = NULL, .emit = cap_emit };
  return sw_scan_lines(&sink, body, q, "TEST_LIST", "sanctions-test",
                       "https://example.invalid/list", delim, lo, hi, 50);
}

/* ── real bytes ──────────────────────────────────────────────────────────── */

/* EU consolidated export. Columns 1-20 verbatim; the remaining 93 columns of
 * each row are trimmed after column 21 because the screen never looks past
 * col_hi. Column 7 is Entity_Remark and column 6 is
 * Entity_DesignationDetails — the narrative columns a whole-row screen also
 * matches. Columns 17-20 are NameAlias_LastName / _FirstName / _MiddleName /
 * _WholeName. Note the second row: the SAME entity (logical id 13) reappears
 * with a different alias, which is why restricting the screen to the name
 * columns cannot lose an alias — every alias is its own row. */
static const char *EU_BODY =
  "fileGenerationDate;Entity_LogicalId;Entity_EU_ReferenceNumber;"
  "Entity_UnitedNationId;Entity_DesignationDate;Entity_DesignationDetails;"
  "Entity_Remark;Entity_SubjectType;Entity_SubjectType_ClassificationCode;"
  "Entity_Regulation_Type;Entity_Regulation_OrganisationType;"
  "Entity_Regulation_PublicationDate;Entity_Regulation_EntryIntoForceDate;"
  "Entity_Regulation_NumberTitle;Entity_Regulation_Programme;"
  "Entity_Regulation_PublicationUrl;NameAlias_LastName;NameAlias_FirstName;"
  "NameAlias_MiddleName;NameAlias_WholeName;NameAlias_NameLanguage\n"
  "05/08/2026;13;EU.27.28;;;;(UNSC RESOLUTION 1483);P;person;regulation;"
  "commission;2003-07-08;2003-07-07;1210/2003 (OJ L169);IRQ;"
  "http://eur-lex.europa.eu/LexUriServ/LexUriServ.do?uri=OJ:L:2003:169:0006:0023:EN:PDF;"
  "Hussein Al-Tikriti;Saddam;;Saddam Hussein Al-Tikriti;\n"
  "05/08/2026;13;EU.27.28;;;;(UNSC RESOLUTION 1483);P;person;regulation;"
  "commission;2003-07-08;2003-07-07;1210/2003 (OJ L169);IRQ;"
  "http://eur-lex.europa.eu/LexUriServ/LexUriServ.do?uri=OJ:L:2003:169:0006:0023:EN:PDF;"
  ";;;Abu Ali;\n";

/* UK OFSI ConList.csv. Row 1 of the real file is a "Last Updated" banner, row
 * 2 is the header, then records. ABAKAROV is the false positive this whole
 * change exists to remove: "Putin" appears only in column 28, Other
 * Information — which is also a QUOTED field containing commas, so it doubles
 * as the check that quoting does not shift the column numbering. PUTIN himself
 * is the true positive that must survive. */
static const char *OFSI_BODY =
  "Last Updated,03/06/2026\n"
  "Name 6,Name 1,Name 2,Name 3,Name 4,Name 5,Title,Name Non-Latin Script,"
  "Non-Latin Script Type,Non-Latin Script Language,DOB,Town of Birth,"
  "Country of Birth,Nationality,Passport Number,Passport Details,"
  "National Identification Number,National Identification Details,Position,"
  "Address 1,Address 2,Address 3,Address 4,Address 5,Address 6,Post/Zip Code,"
  "Country,Other Information,Group Type,Alias Type\n"
  "ABAKAROV,Khizri,Magomedovich,,,,,,,,28/06/1960,Yuzhno Sakhalinsk,Russia,"
  "Russia,,,,,Member of the State Duma of the Russian Federation,"
  "1 Okhotny Ryad str,,,,,,103265,Russia,"
  "\"(UK Sanctions List Ref):RUS0296. (UK Statement of Reasons):Member of the "
  "State Duma of Russia who voted in favour of Federal Law No. 75577-8. In so "
  "doing, the member endorsed President Putin's decision to recognise the "
  "Donetsk People's Republic. (Gender):Male\",Individual,Primary name\n"
  "PUTIN,Vladimir,,,,,,,,,07/10/1952,St Petersburg (then Leningrad),Russia,"
  "Russia,,,,,President of the Russian Federation,,,,,,Moscow,,Russia,"
  "\"(UK Sanctions List Ref):RUS0251. (Gender):Male\",Individual,Primary name\n";

/* A row whose declared name columns are entirely empty, with a person's name
 * sitting in a narrative column instead. Under the old fall-back-to-the-whole-
 * row behaviour this was screened as if the narrative were a name. */
static const char *NAMELESS_BODY =
  "a,b,c,d,e,f,narrative\n"
  ",,,,,,\"vessel formerly operated on behalf of Vladimir Putin\"\n";

int main(void) {
  printf("sanctions scan test\n");

  /* ── EU: a semicolon file with the name at columns 17-20 ──────────────── */

  int n = screen(EU_BODY, "Saddam Hussein", ';', 17, 20);
  ok(n == 1 && g_n == 1, "EU: a listed person is found in columns 17-20");
  ok(g_n == 1 && strstr(g_names[0], "Saddam") != NULL,
     "EU: the emitted title is built from the real name columns");

  n = screen(EU_BODY, "Abu Ali", ';', 17, 20);
  ok(n == 1, "EU: an alias-only row matches on NameAlias_WholeName");

  /* The false positive. "UNSC RESOLUTION" is column 7, Entity_Remark. Under
   * the whole-row screen — the ONLY thing the old single-count field could
   * express for this file — both rows come back as sanctions hits on a query
   * that names nobody. */
  n = screen(EU_BODY, "UNSC RESOLUTION", ';', 17, 20);
  ok(n == 0, "EU: a narrative column is no longer screened as a name");
  n = screen(EU_BODY, "UNSC RESOLUTION", ';', 0, 0);
  ok(n >= 1, "EU: ...and the old whole-row screen really did match it");

  /* Semicolons matter: comma-splitting this file makes column 1 the entire
   * line, so columns 17-20 do not exist and the screen finds nothing. This is
   * why a delimiter had to be declarable and not just a column count. */
  n = screen(EU_BODY, "Saddam Hussein", ',', 17, 20);
  ok(n == 0, "EU: read with the wrong delimiter the name columns vanish");

  /* ── UK OFSI: a comma file with the name at columns 1-6 ───────────────── */

  n = screen(OFSI_BODY, "Putin", ',', 1, 6);
  ok(n == 1 && g_n == 1, "OFSI: only the row NAMED Putin is a hit");
  ok(g_n == 1 && strstr(g_names[0], "PUTIN") != NULL,
     "OFSI: and it is the right row");

  /* The measured 708-vs-11 in miniature: ABAKAROV mentions Putin only in the
   * quoted Other Information column. */
  n = screen(OFSI_BODY, "Putin", ',', 0, 0);
  ok(n == 2, "OFSI: the whole-row screen also flags the person merely MENTIONING him");

  n = screen(OFSI_BODY, "Abakarov Khizri", ',', 1, 6);
  ok(n == 1, "OFSI: a name split across Name 6 / Name 1 still matches");

  /* Commas inside the quoted narrative column must not shift the column
   * numbering — if they did, columns 1-6 would drift into the narrative and
   * the false positive would come straight back. */
  n = screen(OFSI_BODY, "Donetsk", ',', 1, 6);
  ok(n == 0, "OFSI: commas inside a quoted field do not shift the name columns");

  /* ── a row with nothing in its name columns is not screened ───────────── */

  /* Deliberate behaviour change. The old code fell back to screening the whole
   * row whenever the name columns came out empty, which reopened the exact
   * false positive the column range closes. When we KNOW where names live, a
   * row with nothing there carries no name. */
  n = screen(NAMELESS_BODY, "Vladimir Putin", ',', 1, 6);
  ok(n == 0, "a row with empty name columns is skipped, not screened whole");
  n = screen(NAMELESS_BODY, "Vladimir Putin", ',', 0, 0);
  ok(n == 1, "...whereas an UNDECLARED layout still screens the whole row");

  /* ── the undeclared case is unchanged ─────────────────────────────────── */

  /* AU_DFAT is still on this path: dfat.gov.au refuses every connection from
   * the build host, so its layout is unknown and the row keeps lo=0. That must
   * keep behaving as it always did — over-matching, never missing. */
  n = screen(OFSI_BODY, "Vladimir", ',', 0, 0);
  ok(n >= 1, "an undeclared layout screens the whole row, as before");

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
