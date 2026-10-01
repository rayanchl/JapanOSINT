/* collectors/sources/hp_people_ident.c — person and account depth.
 *
 * Username enumeration ("does this handle exist on 600 sites") was already
 * covered. What was missing is the record behind a confirmed account. A GitHub
 * account, for instance, publishes its public SSH and GPG keys — and a GPG key
 * carries the email addresses its owner attached to it. Push events carry commit
 * emails and timestamps that map a working day to a timezone. Gists carry
 * pasted config. That is the difference between "account exists" and "here is
 * the person".
 *
 * Wikidata, VIAF, LittleSis and OpenCorporates' officer index add the other
 * half: linking a person to organisations, offices and relationships. */
#include "lib/hpengine.h"

#define GH_HDRS { "Accept: application/vnd.github+json", NULL }

static const hp_source HP_PEOPLE[] = {
  /* ── GitHub: one account, eight records ────────────────────────────────── */
  { .id = "GH_USER_PROFILE", .name = "GitHub — user profile",
    .name_ja = "GitHub — ユーザープロファイル", .category = "social",
    .portal = "https://github.com", .record_type = "github-user",
    .tags = "\"github\",\"people\"", .free_tier = 1, .headers = GH_HDRS,
    .url = "https://api.github.com/users/{qn}",
    .title_keys = "login,name", .id_keys = "id", .date_keys = "created_at",
    .description = "GitHub profile record — display name, company, blog, location, "
      "public email, hireable flag, follower counts and account creation date. The "
      "company and location fields are self-reported and frequently accurate" },

  { .id = "GH_USER_SSH_KEYS", .name = "GitHub — public SSH keys",
    .name_ja = "GitHub — 公開SSH鍵", .category = "cyber",
    .portal = "https://github.com", .record_type = "github-ssh-key",
    .tags = "\"github\",\"keys\",\"identity\"", .free_tier = 1, .headers = GH_HDRS,
    .url = "https://api.github.com/users/{qn}/keys",
    .title_keys = "key", .id_keys = "id",
    .description = "The public SSH keys a GitHub account has uploaded. Key material "
      "is a hard cross-platform identifier: the same key on another host, a leaked "
      "authorized_keys file or a Git commit signature ties the accounts together" },

  { .id = "GH_USER_GPG_KEYS", .name = "GitHub — public GPG keys",
    .name_ja = "GitHub — 公開GPG鍵", .category = "cyber",
    .portal = "https://github.com", .record_type = "github-gpg-key",
    .tags = "\"github\",\"keys\",\"identity\"", .free_tier = 1, .headers = GH_HDRS,
    .url = "https://api.github.com/users/{qn}/gpg_keys",
    .title_keys = "key_id", .id_keys = "key_id", .date_keys = "created_at",
    .description = "GPG keys registered to a GitHub account, including the email "
      "identities embedded in each key — one of the few places a developer's private "
      "or corporate address is published deliberately" },

  { .id = "GH_USER_REPOS", .name = "GitHub — repositories",
    .name_ja = "GitHub — リポジトリ一覧", .category = "social",
    .portal = "https://github.com", .record_type = "github-repo",
    .tags = "\"github\",\"code\"", .free_tier = 1, .headers = GH_HDRS,
    .url = "https://api.github.com/users/{qn}/repos?per_page=50&sort=pushed",
    .title_keys = "full_name,name", .id_keys = "id", .date_keys = "pushed_at",
    .page_param = "page", .page_start = 1,
    .description = "Repositories owned by an account, most recently pushed first — "
      "language, fork/parent, licence, homepage and topics. Fork parents expose which "
      "internal or upstream projects the person actually works on" },

  { .id = "GH_USER_EVENTS", .name = "GitHub — public activity timeline",
    .name_ja = "GitHub — 公開アクティビティ", .category = "social",
    .portal = "https://github.com", .record_type = "github-event",
    .tags = "\"github\",\"timeline\"", .free_tier = 1, .headers = GH_HDRS,
    .url = "https://api.github.com/users/{qn}/events/public?per_page=50",
    .title_keys = "type,repo.name", .id_keys = "id", .date_keys = "created_at",
    .page_param = "page", .page_start = 1,
    .description = "The account's public event stream — pushes (with commit messages "
      "and author emails), issue comments, releases and stars, each timestamped. A "
      "timeline like this reveals working hours, and therefore timezone" },

  { .id = "GH_USER_GISTS", .name = "GitHub — public gists",
    .name_ja = "GitHub — 公開Gist", .category = "cyber",
    .portal = "https://gist.github.com", .record_type = "github-gist",
    .tags = "\"github\",\"leaks\"", .free_tier = 1, .headers = GH_HDRS,
    .url = "https://api.github.com/users/{qn}/gists?per_page=50",
    .title_keys = "description,id", .id_keys = "id", .date_keys = "created_at",
    .page_param = "page", .page_start = 1,
    .description = "Gists published by an account — filenames, languages and sizes. "
      "Gists are where configuration, logs and credentials get pasted 'temporarily' "
      "and then forgotten" },

  { .id = "GH_USER_ORGS", .name = "GitHub — organisation memberships",
    .name_ja = "GitHub — 所属Organization", .category = "social",
    .portal = "https://github.com", .record_type = "github-org",
    .tags = "\"github\",\"employment\"", .free_tier = 1, .headers = GH_HDRS,
    .url = "https://api.github.com/users/{qn}/orgs",
    .title_keys = "login,description", .id_keys = "id",
    .page_param = "page", .page_start = 1,
    .description = "Publicly visible organisation memberships of an account — often "
      "the most current employment signal available for an engineer, and a route into "
      "the employer's own repositories" },

  { .id = "GH_COMMIT_EMAIL_SEARCH", .name = "GitHub — commits by author email",
    .name_ja = "GitHub — コミット著者メール検索", .category = "cyber",
    .portal = "https://github.com", .record_type = "github-commit",
    .tags = "\"github\",\"email\",\"identity\"", .want = HP_EMAIL,
    .key_env = "GITHUB_TOKEN", .free_tier = 1,
    .url = "https://api.github.com/search/commits?q=author-email:{q}&per_page=40",
    .headers = { "Accept: application/vnd.github+json", "Authorization: Bearer {key}", NULL },
    .array_path = "items", .title_keys = "commit.message,repository.full_name",
    .id_keys = "sha", .date_keys = "commit.author.date",
    .description = "Commits authored by a specific email address across public "
      "GitHub — turns a leaked or guessed corporate address into the repositories, "
      "aliases and co-authors of the person behind it" },

  /* ── Other code hosts ──────────────────────────────────────────────────── */
  { .id = "GITLAB_USER_LOOKUP", .name = "GitLab — user lookup",
    .name_ja = "GitLab — ユーザー照会", .category = "social",
    .portal = "https://gitlab.com", .record_type = "gitlab-user",
    .tags = "\"gitlab\",\"people\"", .free_tier = 1,
    .url = "https://gitlab.com/api/v4/users?username={qn}",
    .title_keys = "name,username", .id_keys = "id", .date_keys = "created_at",
    .description = "GitLab.com account record for a username — display name, avatar, "
      "bio, organisation, job title, location and public email where set" },

  { .id = "CODEBERG_USER_SEARCH", .name = "Codeberg — user search",
    .name_ja = "Codeberg — ユーザー検索", .category = "social",
    .portal = "https://codeberg.org", .record_type = "forgejo-user",
    .tags = "\"codeberg\",\"people\"", .free_tier = 1,
    .url = "https://codeberg.org/api/v1/users/search?q={q}&limit=25",
    .array_path = "data", .title_keys = "login,full_name", .id_keys = "id",
    .date_keys = "created",
    .description = "Accounts on Codeberg (Forgejo) — the host developers move to when "
      "they leave GitHub, so a handle absent there and present here is itself a "
      "signal" },

  /* ── Forums & Q&A ──────────────────────────────────────────────────────── */
  { .id = "HN_USER_PROFILE", .name = "Hacker News — user profile",
    .name_ja = "Hacker News — ユーザー", .category = "social",
    .portal = "https://news.ycombinator.com", .record_type = "hn-user",
    .tags = "\"hackernews\",\"people\"", .free_tier = 1,
    .url = "https://hacker-news.firebaseio.com/v0/user/{qn}.json",
    .title_keys = "id", .id_keys = "id", .date_keys = "created",
    .description = "Hacker News account record — karma, account creation time and the "
      "free-text 'about' field, which very often contains a real name, employer, "
      "email or personal site" },

  { .id = "HN_AUTHOR_ACTIVITY", .name = "Hacker News — posts & comments by author",
    .name_ja = "Hacker News — 投稿/コメント履歴", .category = "social",
    .portal = "https://news.ycombinator.com", .record_type = "hn-item",
    .tags = "\"hackernews\",\"timeline\"", .free_tier = 1,
    .url = "https://hn.algolia.com/api/v1/search_by_date?tags=author_{qn}&hitsPerPage=40",
    .array_path = "hits", .title_keys = "title,comment_text,story_title",
    .id_keys = "objectID", .date_keys = "created_at",
    .description = "Everything an HN account has written, newest first — the full text "
      "of comments and submissions, which is where technical staff describe their "
      "employer's stack and incidents in detail" },

  { .id = "STACKEXCHANGE_USERS", .name = "Stack Exchange — user search",
    .name_ja = "Stack Exchange — ユーザー検索", .category = "social",
    .portal = "https://stackexchange.com", .record_type = "stackexchange-user",
    .tags = "\"stackoverflow\",\"people\"", .free_tier = 1,
    .url = "https://api.stackexchange.com/2.3/users?inname={q}&site=stackoverflow"
           "&pagesize=40&order=desc&sort=reputation",
    .array_path = "items", .title_keys = "display_name", .id_keys = "user_id",
    .date_keys = "creation_date",
    .description = "Stack Overflow accounts matching a name — reputation, location, "
      "website, and the account age that lets you tell a long-lived professional "
      "identity from a throwaway" },

  { .id = "KEYBASE_DISCOVER", .name = "Keybase — cross-platform proof discovery",
    .name_ja = "Keybase — 他サービス紐付け", .category = "social",
    .portal = "https://keybase.io", .record_type = "keybase-identity",
    .tags = "\"keybase\",\"identity\",\"proofs\"", .free_tier = 1,
    .url = "https://keybase.io/_/api/1.0/user/discover.json?github={qn}&twitter={qn}"
           "&reddit={qn}&hackernews={qn}&flatten=1",
    .title_keys = "matches.0.basics.username",
    .description = "Reverse-resolves a handle on GitHub, X, Reddit or HN to a Keybase "
      "identity and, through it, to every other platform the same person "
      "cryptographically proved they own" },

  { .id = "GRAVATAR_PROFILE", .name = "Gravatar — profile record",
    .name_ja = "Gravatar — プロファイル", .category = "social",
    .portal = "https://gravatar.com", .record_type = "gravatar-profile",
    .tags = "\"gravatar\",\"identity\"", .free_tier = 1,
    .url = "https://en.gravatar.com/{qn}.json",
    .array_path = "entry", .title_keys = "displayName,name.formatted",
    .id_keys = "hash",
    .description = "Gravatar profile behind a username or email hash — display name, "
      "'about me', linked accounts, verified services, photos and personal URLs, all "
      "keyed to an email address" },

  /* ── Fediverse / Bluesky ───────────────────────────────────────────────── */
  { .id = "BLUESKY_PROFILE", .name = "Bluesky — actor profile",
    .name_ja = "Bluesky — プロファイル", .category = "social",
    .portal = "https://bsky.app", .record_type = "bluesky-profile",
    .tags = "\"bluesky\",\"people\"", .free_tier = 1,
    .url = "https://public.api.bsky.app/xrpc/app.bsky.actor.getProfile?actor={qn}",
    .title_keys = "displayName,handle", .id_keys = "did", .date_keys = "createdAt",
    .description = "Bluesky profile record — DID, handle (which is usually a domain "
      "the person controls), display name, description, follower counts and pinned "
      "post. The DID survives handle changes" },

  { .id = "BLUESKY_POST_SEARCH", .name = "Bluesky — post search",
    .name_ja = "Bluesky — 投稿検索", .category = "social",
    .portal = "https://bsky.app", .record_type = "bluesky-post",
    .tags = "\"bluesky\",\"posts\"", .free_tier = 1,
    .url = "https://public.api.bsky.app/xrpc/app.bsky.feed.searchPosts?q={q}&limit=40",
    .array_path = "posts", .title_keys = "record.text,author.handle",
    .id_keys = "cid", .date_keys = "record.createdAt",
    .description = "Public Bluesky posts matching a term — author DID and handle, "
      "text, language, reply/quote context and engagement counts, with no "
      "authentication required" },

  { .id = "MASTODON_ACCOUNT_SEARCH", .name = "Mastodon — account search",
    .name_ja = "Mastodon — アカウント検索", .category = "social",
    .portal = "https://mastodon.social", .record_type = "mastodon-account",
    .tags = "\"mastodon\",\"fediverse\"", .free_tier = 1,
    .url = "https://mastodon.social/api/v2/search?q={q}&type=accounts&limit=25&resolve=true",
    .array_path = "accounts", .title_keys = "acct,display_name", .id_keys = "id",
    .date_keys = "created_at",
    .description = "Fediverse accounts resolvable from one large instance — full "
      "acct@domain, display name, note, fields (which often carry verified personal "
      "links) and post/follower counts" },

  { .id = "LEMMY_USER_SEARCH", .name = "Lemmy — user & community search",
    .name_ja = "Lemmy — ユーザー検索", .category = "social",
    .portal = "https://lemmy.world", .record_type = "lemmy-user",
    .tags = "\"lemmy\",\"fediverse\"", .free_tier = 1,
    .url = "https://lemmy.world/api/v3/search?q={q}&type_=Users&limit=25",
    .array_path = "users", .title_keys = "person.name,person.display_name",
    .id_keys = "person.id", .date_keys = "person.published",
    .description = "Lemmy accounts matching a handle — home instance, bio, "
      "registration date and post/comment counts across the federated network" },

  { .id = "TELEGRAM_CHANNEL_PREVIEW", .name = "Telegram — public channel preview",
    .name_ja = "Telegram — 公開チャンネル", .category = "social",
    .type = "scraped", .mode = HP_HTML,
    .portal = "https://t.me", .record_type = "telegram-post",
    .tags = "\"telegram\",\"channels\"", .free_tier = 1,
    .url = "https://t.me/s/{qn}", .base = "https://t.me",
    .description = "The server-rendered preview of a public Telegram channel — recent "
      "post links and text without an API key or phone number. Channel previews are "
      "the only keyless window into Telegram" },

  /* ── Identity graphs ───────────────────────────────────────────────────── */
  { .id = "WIKIDATA_ENTITY_CLAIMS", .name = "Wikidata — full entity claims",
    .name_ja = "Wikidata — 全クレーム取得", .category = "investigation",
    .portal = "https://www.wikidata.org", .record_type = "wikidata-entity",
    .tags = "\"wikidata\",\"identity\"", .free_tier = 1,
    .url = "https://www.wikidata.org/w/api.php?action=wbgetentities&ids={qU}"
           "&props=claims%7Clabels%7Cdescriptions%7Csitelinks%7Caliases&format=json",
    .array_path = "entities", .title_keys = "labels.en.value", .id_keys = "id",
    .description = "Every statement on a Wikidata item — employer, position held, "
      "board memberships, spouse, education, national identifiers (VIAF, ORCID, ISNI, "
      "company numbers) and the sitelinks that expand into full biographies" },

  { .id = "WIKIDATA_SPARQL_PERSON", .name = "Wikidata SPARQL — person & role graph",
    .name_ja = "Wikidata SPARQL — 人物関係グラフ", .category = "investigation",
    .portal = "https://query.wikidata.org", .record_type = "wikidata-person",
    .tags = "\"wikidata\",\"sparql\",\"relationships\"", .free_tier = 1,
    .url = "https://query.wikidata.org/sparql?format=json&query="
           "SELECT%20%3Fitem%20%3FitemLabel%20%3FroleLabel%20%3ForgLabel%20WHERE%20%7B"
           "%3Fitem%20rdfs%3Alabel%20%22{q}%22%40en%20.%20OPTIONAL%7B%3Fitem%20wdt%3AP39%20%3Frole%7D%20"
           "OPTIONAL%7B%3Fitem%20wdt%3AP108%20%3Forg%7D%20SERVICE%20wikibase%3Alabel%20"
           "%7Bbd%3AserviceParam%20wikibase%3Alanguage%20%22en%22%7D%7D%20LIMIT%2040",
    .headers = { "Accept: application/sparql-results+json", NULL },
    .array_path = "results.bindings", .title_keys = "itemLabel.value",
    .id_keys = "item.value",
    .description = "A live SPARQL query returning the positions held and employers "
      "recorded for a person's exact label — the structured relationship layer that "
      "keyword search over Wikidata cannot express" },

  { .id = "VIAF_AUTOSUGGEST", .name = "VIAF — authority file identities",
    .name_ja = "VIAF — 典拠識別子", .category = "research",
    .portal = "https://viaf.org", .record_type = "authority-record",
    .tags = "\"authority\",\"identity\"", .free_tier = 1,
    .url = "https://viaf.org/viaf/AutoSuggest?query={q}",
    .array_path = "result", .title_keys = "term,displayForm", .id_keys = "viafid",
    .description = "VIAF authority records — the cross-walk between a person's name "
      "as held by the Library of Congress, BnF, DNB, NDL Japan and others, plus their "
      "LC/ISNI/Wikidata identifiers" },

  { .id = "LITTLESIS_RELATIONSHIPS", .name = "LittleSis — power network relationships",
    .name_ja = "LittleSis — 権力ネットワーク", .category = "investigation",
    .portal = "https://littlesis.org", .record_type = "power-relationship",
    .tags = "\"relationships\",\"elites\"", .free_tier = 1,
    .url = "https://littlesis.org/api/entities/search?q={q}",
    .array_path = "data", .title_keys = "attributes.name", .id_keys = "id",
    .detail_url = "https://littlesis.org/api/entities/{v}/relationships",
    .detail_key = "id",
    .description = "LittleSis entities matching a name, then the relationships behind "
      "each — board seats, donations, ownership, government positions, family ties — "
      "with amounts and date ranges per edge" },

  { .id = "OPENCORPORATES_OFFICERS", .name = "OpenCorporates — officer search",
    .name_ja = "OpenCorporates — 役員検索", .category = "government",
    .portal = "https://opencorporates.com", .record_type = "company-officer",
    .tags = "\"officers\",\"global\"", .key_env = "OPENCORPORATES_API_KEY",
    .free_tier = 0,
    .url = "https://api.opencorporates.com/v0.4/officers/search?q={q}"
           "&api_token={key}&per_page=40",
    .array_path = "results.officers", .title_keys = "officer.name",
    .id_keys = "officer.id", .date_keys = "officer.start_date",
    .description = "Cross-jurisdiction officer search over 140+ company registries — "
      "every company a person is or was an officer of, with role, dates and "
      "jurisdiction. The single best global directorship pivot" },
};

HP_REGISTER_TABLE(HP_PEOPLE)
