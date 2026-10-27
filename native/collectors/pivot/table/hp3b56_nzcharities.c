/* collectors/pivot/table/hp3b56_nzcharities.c — batch 56: nzcharities — the
 * rest of the New Zealand Charities Register, beside the organisations.
 *
 * `nz-charities-register` (collectors/sources/reg2_nz_charities.c) reads the
 * register's 46,144 organisations. The same keyless OData v2 service
 * (Charities Services / DIA) publishes 23 entity sets, and the register's own
 * web page shows three more things about every charity that the organisation
 * record does not carry: its officers, its annual returns and its group. All
 * three are bulk sets on the same service, measured 2026-10-25 with
 * `$inlinecount=allpages`:
 *
 *   Officers       589,390   1.2 MB per 1,000, 7–13 s a page (the deepest too)
 *   AnnualReturn   430,815   9.1 MB per 1,000, 14–18 s a page
 *   Groups             357   one request
 *
 * Officers and annual returns are far larger than one run should read, so both
 * CYCLE (`page_resume`): ascending on their own id, so a record published
 * mid-walk lands at the tail where the walk has not been yet, and each run
 * reads the next slice. Both sets are fully dated (`ModifiedOn` is null on 0
 * of either), but a window on it would not do: a bulk update stamps one day
 * on most of a set (2020-10-08 and 2023-10-27 recur across both), and a
 * record cap is per read, so no window can take such a day whole.
 *
 * What is deliberately NOT read, each measured:
 *   * AnnualReturnCloseRelationshipTransaction — published EMPTY (0 rows); a
 *     row would read "emits nothing" for ever.
 *   * vOfficerOrganisations — the same 589,390 officers joined with their
 *     charity's fields, i.e. this file's Officers plus the organisation row.
 *   * GrpOrgLatestReturns (41,202) — the latest return per entity, a subset of
 *     AnnualReturn; its all-returns sibling times out inside the service.
 *   * Activities / Sectors / Beneficiaries / AreaOfOperations / SourceOfFunds
 *     — lookups whose names vOrganisations already resolves onto each charity.
 *   * The register page's "Charity Documents" exist only on the web page, one
 *     page per charity; that is a separate decision, not a bulk set.
 *
 * OData v2 states its total as the STRING `d.__count`; the engine reads it
 * (pw_odata_v2_count, hptest 58), so every notice here names the real
 * remainder. `$skip=0` is bound in each URL, as an offset walk's first page
 * must be (CLAUDE.md, the Lechner INSPIRE case). The rows share the
 * `government` collector with the organisations row, so the register stays
 * one beat.
 */
#include "../../../lib/hpengine.h"

static const hp_source HP3B56_NZCHARITIES[] = {
  { .id = "NZ_CHARITY_OFFICERS_ALL",
    .name = "NZ Charities Register — every officer (OData)",
    .name_ja = "ニュージーランド慈善団体登録 全役員",
    .collector = "government",
    .category = "charity", .portal = "https://www.charities.govt.nz",
    .record_type = "charity-officer",
    .tags = "\"newzealand\",\"charities\",\"officers\",\"directors\",\"banned\",\"odata\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://www.odata.charities.govt.nz/Officers?$format=json&$orderby=OfficerId&$top=1000&$skip=0&$inlinecount=allpages",
    .array_path = "d.results",
    .id_keys = "OfficerId",
    /* A body corporate officer has no person's name: 2 of 589,390 carry no
     * FullName at all. */
    .title_keys = "FullName,BodyCorporateName",
    .date_keys = "PositionAppointmentDate",
    .interval = 86400,
    .page_param = "$skip",
    .page_size = 1000,
    .page_zero_based = 1,
    /* 100 pages = 100,000 officers, ~120 MB, a run; the set comes round in
     * about six runs. */
    .page_max = 100,
    .page_resume = 1,
    .timeout_ms = 120000,
    .description = "Every officer of every New Zealand charity, current and past, "
      "from the Charities Services OData register: full name or body corporate, "
      "position, appointment and last dates, status, the OrganisationId that "
      "links each to its charity, and the statutory flags — banned director, "
      "undischarged bankrupt, convicted, convicted of a terrorism offence, "
      "disqualified, subject to a property order. 589,390 rows, read over a "
      "cycle." },

  { .id = "NZ_CHARITY_ANNUAL_RETURNS",
    .name = "NZ Charities Register — every annual return (OData)",
    .name_ja = "ニュージーランド慈善団体登録 年次報告（財務）全件",
    .collector = "government",
    .category = "charity", .portal = "https://www.charities.govt.nz",
    .record_type = "charity-annual-return",
    .tags = "\"newzealand\",\"charities\",\"annual-returns\",\"financials\",\"odata\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://www.odata.charities.govt.nz/AnnualReturn?$format=json&$orderby=AnnualReturnId&$top=1000&$skip=0&$inlinecount=allpages",
    .array_path = "d.results",
    .id_keys = "AnnualReturnId",
    /* A return carries no name, and title_keys chooses one field, it cannot
     * compose two: the title is the year-end date. The charity is the
     * record's OrganisationId (or GroupId for a group's return). */
    .title_keys = "YearEnded",
    .date_keys = "YearEnded",
    .interval = 86400,
    .page_param = "$skip",
    .page_size = 1000,
    .page_zero_based = 1,
    /* 30 pages = 30,000 returns, ~275 MB and ~9 minutes, a run; the set comes
     * round in about fifteen runs. A return changes once a year. */
    .page_max = 30,
    .page_resume = 1,
    .timeout_ms = 120000,
    .description = "Every annual return filed with New Zealand's Charities "
      "Services, 244 fields each: year ended, reporting tier, income by source "
      "(donations, bequests, government grants and contracts, trading, "
      "investments), expenditure (salaries, grants paid inside and outside NZ, "
      "service provision), assets, liabilities and reserves, paid staff and "
      "volunteer hours. 430,815 returns, read over a cycle." },

  { .id = "NZ_CHARITY_GROUPS",
    .name = "NZ Charities Register — charity groups (OData)",
    .name_ja = "ニュージーランド慈善団体登録 グループ",
    .collector = "government",
    .category = "charity", .portal = "https://www.charities.govt.nz",
    .record_type = "charity-group",
    .tags = "\"newzealand\",\"charities\",\"groups\",\"odata\"",
    .mode = HP_JSON, .want = HP_ANY, .free_tier = 1,
    .url = "https://www.odata.charities.govt.nz/Groups?$format=json&$orderby=GroupId&$top=1000&$skip=0&$inlinecount=allpages",
    .array_path = "d.results",
    .id_keys = "GroupId",
    .title_keys = "name",
    .date_keys = "DateRegistered",
    .interval = 86400,
    .page_param = "$skip",
    .page_size = 1000,
    .page_zero_based = 1,
    .timeout_ms = 60000,
    .description = "The charity groups registered with New Zealand's Charities "
      "Services — a parent and its member charities registered as one — with "
      "their registration number, status, dates, addresses, purposes and "
      "overseas operations. 357 groups; an organisation's GroupId links it to "
      "its group." },
};

HP_REGISTER_TABLE(HP3B56_NZCHARITIES)
