/* collectors/sources/hp_chain_finance.c — on-chain and corporate-finance depth.
 *
 * An address balance is a headline. The penetrant records are the transaction
 * list, the UTXO set, the token transfers and the internal calls — because those
 * are what link one address to the next. On the traditional-finance side, GLEIF
 * is the same story: the repo looked up an LEI by name, while the same free API
 * publishes the direct parent, the ultimate parent, the direct children and the
 * ISINs attached to that LEI. That parent/child graph is regulator-verified
 * corporate ownership, which almost nothing else gives away for free. */
#include "lib/hpengine.h"

static const hp_source HP_CHAINFIN[] = {
  /* ── Bitcoin ───────────────────────────────────────────────────────────── */
  { .id = "MEMPOOL_ADDRESS", .name = "mempool.space — Bitcoin address summary",
    .name_ja = "mempool.space — BTCアドレス概要", .category = "cyber",
    .portal = "https://mempool.space", .record_type = "btc-address",
    .tags = "\"crypto\",\"bitcoin\"", .want = HP_BTC, .free_tier = 1,
    .url = "https://mempool.space/api/address/{q}",
    .title_keys = "address", .id_keys = "address",
    .description = "Chain and mempool statistics for a Bitcoin address — funded and "
      "spent txo counts and sums, confirmed balance and unconfirmed activity, direct "
      "from a full node with no API key" },

  { .id = "MEMPOOL_ADDRESS_TXS", .name = "mempool.space — Bitcoin address transactions",
    .name_ja = "mempool.space — BTC取引履歴", .category = "cyber",
    .portal = "https://mempool.space", .record_type = "btc-transaction",
    .tags = "\"crypto\",\"bitcoin\",\"flows\"", .want = HP_BTC, .free_tier = 1,
    .url = "https://mempool.space/api/address/{q}/txs",
    .title_keys = "txid", .id_keys = "txid", .date_keys = "status.block_time",
    .description = "The transaction history of a Bitcoin address with full inputs and "
      "outputs — the counterparty addresses, amounts, fees and block times that make "
      "flow tracing possible" },

  { .id = "MEMPOOL_ADDRESS_UTXO", .name = "mempool.space — Bitcoin UTXO set",
    .name_ja = "mempool.space — BTC未使用出力", .category = "cyber",
    .portal = "https://mempool.space", .record_type = "btc-utxo",
    .tags = "\"crypto\",\"bitcoin\"", .want = HP_BTC, .free_tier = 1,
    .url = "https://mempool.space/api/address/{q}/utxo",
    .title_keys = "txid", .id_keys = "txid", .date_keys = "status.block_time",
    .description = "Unspent outputs held by an address — the current holdings broken "
      "down per output, which shows whether funds sit in one consolidated UTXO or "
      "many small deposits" },

  { .id = "BLOCKSTREAM_ADDRESS", .name = "Blockstream Esplora — address record",
    .name_ja = "Blockstream — BTCアドレス記録", .category = "cyber",
    .portal = "https://blockstream.info", .record_type = "btc-address",
    .tags = "\"crypto\",\"bitcoin\"", .want = HP_BTC, .free_tier = 1,
    .url = "https://blockstream.info/api/address/{q}",
    .title_keys = "address", .id_keys = "address",
    .description = "Independent second Bitcoin node view of the same address — "
      "cross-checking two Esplora instances catches indexer lag and reorg artefacts "
      "before they become a wrong conclusion" },

  { .id = "BLOCKCHAIR_BTC_DASHBOARD", .name = "Blockchair — Bitcoin address dashboard",
    .name_ja = "Blockchair — BTCアドレス詳細", .category = "cyber",
    .portal = "https://blockchair.com", .record_type = "btc-address",
    .tags = "\"crypto\",\"bitcoin\"", .want = HP_BTC, .free_tier = 1,
    .url = "https://api.blockchair.com/bitcoin/dashboards/address/{q}?limit=50",
    .array_path = "data", .title_keys = "address.type", .id_keys = "address.script_hex",
    .description = "Blockchair's enriched address dashboard — first/last seen, "
      "received and spent totals, output type and the transaction id list, plus "
      "privacy-relevant metadata like address reuse" },

  /* ── EVM chains ────────────────────────────────────────────────────────── */
  { .id = "BLOCKSCOUT_ETH_ADDRESS", .name = "Blockscout — Ethereum address record",
    .name_ja = "Blockscout — ETHアドレス記録", .category = "cyber",
    .portal = "https://eth.blockscout.com", .record_type = "eth-address",
    .tags = "\"crypto\",\"ethereum\"", .want = HP_ETH, .free_tier = 1,
    .url = "https://eth.blockscout.com/api/v2/addresses/{q}",
    .title_keys = "name,hash", .id_keys = "hash",
    .description = "Ethereum address record without an API key — balance, contract "
      "flag, verified-contract name, implementation address for proxies, and public "
      "tags such as exchange or bridge labels" },

  { .id = "BLOCKSCOUT_ETH_TXS", .name = "Blockscout — Ethereum transactions",
    .name_ja = "Blockscout — ETH取引履歴", .category = "cyber",
    .portal = "https://eth.blockscout.com", .record_type = "eth-transaction",
    .tags = "\"crypto\",\"ethereum\",\"flows\"", .want = HP_ETH, .free_tier = 1,
    .url = "https://eth.blockscout.com/api/v2/addresses/{q}/transactions",
    .array_path = "items", .title_keys = "hash,method", .id_keys = "hash",
    .date_keys = "timestamp",
    .description = "Transactions to and from an address with decoded method names, "
      "counterparties, values, gas and status — decoded calls tell you what a wallet "
      "was doing, not just that it moved money" },

  { .id = "BLOCKSCOUT_ETH_TOKEN_TRANSFERS", .name = "Blockscout — token transfers",
    .name_ja = "Blockscout — トークン移転", .category = "cyber",
    .portal = "https://eth.blockscout.com", .record_type = "eth-token-transfer",
    .tags = "\"crypto\",\"ethereum\",\"tokens\"", .want = HP_ETH, .free_tier = 1,
    .url = "https://eth.blockscout.com/api/v2/addresses/{q}/token-transfers",
    .array_path = "items", .title_keys = "token.name,token.symbol",
    .id_keys = "tx_hash", .date_keys = "timestamp",
    .description = "ERC-20/721/1155 transfers involving an address — token contract, "
      "symbol, decimals, amount and counterparty. Stablecoin transfers are where most "
      "real value moves, and they are invisible in a native-balance check" },

  { .id = "BLOCKSCOUT_ETH_INTERNAL_TXS", .name = "Blockscout — internal transactions",
    .name_ja = "Blockscout — 内部取引", .category = "cyber",
    .portal = "https://eth.blockscout.com", .record_type = "eth-internal-tx",
    .tags = "\"crypto\",\"ethereum\",\"flows\"", .want = HP_ETH, .free_tier = 1,
    .url = "https://eth.blockscout.com/api/v2/addresses/{q}/internal-transactions",
    .array_path = "items", .title_keys = "type,transaction_hash",
    .id_keys = "transaction_hash", .date_keys = "timestamp",
    .description = "Contract-internal calls touching an address — the value movements "
      "produced by contract execution, which never appear in the normal transaction "
      "list and are how mixers and routers actually deliver funds" },

  { .id = "TRONGRID_ACCOUNT", .name = "TronGrid — TRON account record",
    .name_ja = "TronGrid — TRONアカウント", .category = "cyber",
    .portal = "https://tronscan.org", .record_type = "tron-account",
    .tags = "\"crypto\",\"tron\"", .free_tier = 1,
    .url = "https://api.trongrid.io/v1/accounts/{qn}",
    .array_path = "data", .title_keys = "address", .id_keys = "address",
    .date_keys = "create_time",
    .description = "TRON account state — TRX balance, TRC-20 token balances, "
      "permissions/multisig configuration and creation time. TRON carries a large "
      "share of USDT flows, which makes it a routine tracing target" },

  { .id = "SOLANA_SIGNATURES", .name = "Solana RPC — address signature history",
    .name_ja = "Solana — 取引署名履歴", .category = "cyber",
    .portal = "https://solana.com", .record_type = "solana-transaction",
    .tags = "\"crypto\",\"solana\"", .free_tier = 1,
    .url = "https://api.mainnet-beta.solana.com",
    .post_body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"getSignaturesForAddress\","
                 "\"params\":[\"{Q}\",{\"limit\":50}]}",
    .array_path = "result", .title_keys = "signature,memo", .id_keys = "signature",
    .date_keys = "blockTime",
    .description = "Recent transaction signatures for a Solana account with slot, "
      "block time, error status and memo — the entry point for tracing SPL token and "
      "NFT movement on a chain most tooling ignores" },

  { .id = "RANSOMWHERE_PAYMENTS", .name = "Ransomwhere — reported ransom payments",
    .name_ja = "Ransomwhere — 身代金支払い報告", .category = "cyber",
    .portal = "https://ransomwhe.re", .record_type = "ransom-payment",
    .tags = "\"crypto\",\"ransomware\"", .free_tier = 1,
    .url = "https://api.ransomwhe.re/export", .array_path = "result",
    .filter_query = 1, .title_keys = "family,address", .id_keys = "address",
    .description = "Crowdsourced ransomware payment addresses with the family they "
      "belong to and total received, filtered to the queried address or family — the "
      "public attribution layer over ransom wallets" },

  /* ── GLEIF: regulator-verified corporate ownership ─────────────────────── */
  { .id = "GLEIF_LEI_RECORD", .name = "GLEIF — LEI record",
    .name_ja = "GLEIF — LEIレコード", .category = "economy",
    .portal = "https://www.gleif.org", .record_type = "lei-record",
    .tags = "\"lei\",\"corporate\"", .free_tier = 1,
    .url = "https://api.gleif.org/api/v1/lei-records/{qU}",
    .title_keys = "data.attributes.entity.legalName.name", .id_keys = "data.id",
    .date_keys = "data.attributes.registration.initialRegistrationDate",
    .description = "The full LEI record — legal and transliterated names, legal form, "
      "legal and headquarters addresses, registration authority id, entity status and "
      "the validation source that verified it" },

  { .id = "GLEIF_DIRECT_PARENT", .name = "GLEIF — direct parent",
    .name_ja = "GLEIF — 直接親会社", .category = "economy",
    .portal = "https://www.gleif.org", .record_type = "lei-parent",
    .tags = "\"lei\",\"ownership\"", .free_tier = 1,
    .url = "https://api.gleif.org/api/v1/lei-records/{qU}/direct-parent",
    .title_keys = "data.attributes.entity.legalName.name", .id_keys = "data.id",
    .description = "The immediate accounting-consolidating parent of an entity, as "
      "reported to its LEI issuer — regulator-facing ownership, not a press-release "
      "org chart" },

  { .id = "GLEIF_ULTIMATE_PARENT", .name = "GLEIF — ultimate parent",
    .name_ja = "GLEIF — 最終親会社", .category = "economy",
    .portal = "https://www.gleif.org", .record_type = "lei-parent",
    .tags = "\"lei\",\"ownership\"", .free_tier = 1,
    .url = "https://api.gleif.org/api/v1/lei-records/{qU}/ultimate-parent",
    .title_keys = "data.attributes.entity.legalName.name", .id_keys = "data.id",
    .description = "The top of the consolidation chain above an entity — the single "
      "field that answers 'who ultimately owns this' for any LEI-registered company "
      "worldwide" },

  { .id = "GLEIF_DIRECT_CHILDREN", .name = "GLEIF — direct children",
    .name_ja = "GLEIF — 直接子会社", .category = "economy",
    .portal = "https://www.gleif.org", .record_type = "lei-child",
    .tags = "\"lei\",\"ownership\"", .free_tier = 1,
    .url = "https://api.gleif.org/api/v1/lei-records/{qU}/direct-children?page%5Bsize%5D=50",
    .array_path = "data", .title_keys = "attributes.entity.legalName.name",
    .id_keys = "id",
    .next_path = "links.next",
    .description = "Subsidiaries that report this entity as their direct parent — the "
      "group structure read downward, which surfaces subsidiaries in jurisdictions "
      "you would not have thought to search" },

  { .id = "GLEIF_ISIN_MAPPING", .name = "GLEIF — LEI to ISIN mapping",
    .name_ja = "GLEIF — LEI/ISIN対応", .category = "economy",
    .portal = "https://www.gleif.org", .record_type = "lei-isin",
    .tags = "\"lei\",\"securities\"", .free_tier = 1,
    .url = "https://api.gleif.org/api/v1/lei-records/{qU}/isins?page%5Bsize%5D=100",
    .array_path = "data", .title_keys = "attributes.isin", .id_keys = "id",
    .next_path = "links.next",
    .description = "Every ISIN issued by an entity — the bridge between a legal entity "
      "and its traded securities, which is what lets a holdings disclosure be matched "
      "back to a company" },

  { .id = "OPENFIGI_MAPPING", .name = "OpenFIGI — instrument identifier mapping",
    .name_ja = "OpenFIGI — 証券識別子マッピング", .category = "economy",
    .portal = "https://www.openfigi.com", .record_type = "financial-instrument",
    .tags = "\"securities\",\"identifiers\"", .free_tier = 1,
    .url = "https://api.openfigi.com/v3/mapping",
    .post_body = "[{\"idType\":\"ID_ISIN\",\"idValue\":\"{Q}\"}]",
    .array_path = "0.data", .title_keys = "name,securityDescription",
    .id_keys = "figi",
    .description = "Maps an ISIN, ticker or other identifier to FIGIs with the "
      "instrument name, exchange code, security type and market sector — the "
      "cross-vendor key for securities data" },

  { .id = "FRED_SERIES_SEARCH", .name = "FRED — economic series search",
    .name_ja = "FRED — 経済統計系列検索", .category = "economy",
    .portal = "https://fred.stlouisfed.org", .record_type = "economic-series",
    .tags = "\"macro\",\"statistics\"", .key_env = "FRED_KEY", .free_tier = 1,
    .url = "https://api.stlouisfed.org/fred/series/search?search_text={q}"
           "&api_key={key}&file_type=json&limit=40",
    .array_path = "seriess", .title_keys = "title", .id_keys = "id",
    .date_keys = "last_updated",
    .description = "St. Louis Fed series matching a term — frequency, units, "
      "seasonal adjustment, observation range and popularity, so a claim about a "
      "macro figure can be pinned to a specific published series" },

  { .id = "DEFILLAMA_PROTOCOL", .name = "DefiLlama — protocol TVL & chains",
    .name_ja = "DefiLlama — プロトコルTVL", .category = "economy",
    .portal = "https://defillama.com", .record_type = "defi-protocol",
    .tags = "\"crypto\",\"defi\"", .free_tier = 1,
    .url = "https://api.llama.fi/protocol/{ql}",
    .title_keys = "name,symbol", .id_keys = "id",
    .description = "A DeFi protocol's record — chains deployed on, TVL history, "
      "treasury and token addresses, audit links and the parent organisation, which "
      "is often a company with a real corporate registration" },
};

HP_REGISTER_TABLE(HP_CHAINFIN)
