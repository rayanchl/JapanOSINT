#!/usr/bin/env bash
# Remove the pure portal-probe stubs.
#
# Each of these emits exactly one row of {operator, reachable} with a title and
# summary that are string literals in the .c file. The only fetched fact is
# whether the portal answered — no measurement, ever, and no credential that
# would change that. They counted as DATA in every row-count sweep, which is
# what made "2,189 sources" overstate real coverage.
#
# Deliberately NOT removed (they have real extraction that has stopped
# matching — a fixable bug, not an empty stub):
#   tenki-jp, jr-central-delay, jr-east-delay, jr-west-delay,
#   shinkansen-status, bosai-volcano-cam
# Also NOT removed: the 9 key-gated services (they return real data with a
# credential) and grid-usage-realtime / pref-police-crime (real payload).
set -u
cd "$(dirname "$0")/../../collectors/sources" || exit 1
FILES="
blitzortung_lightning.c comiket_events.c earthquake_network_citizen.c
fsa_crypto_exchanges.c gas_outages.c jnto_arrivals.c jpo_jplatpat.c
jpo_trademarks.c jpx_quotes.c jr_boarding_stats.c kafun_pollen.c
kakaku_prices.c mext_schools.c mic_broadcast_towers.c mic_elections.c
michi_no_eki.c ndl_search.c nexco_roadwork.c nhk_relay_towers.c
nied_mowlas.c saibansyo_rulings.c steam_jp_users.c sumo_tournaments.c
teikoku_failures.c tepco_outage.c tsr_closures.c yahoo_chiebukuro.c
boj_stats.c docomo_mobaku.c jcg_navarea.c jma_pollen.c jma_uv.c
mhlw_health.c nict_atlas.c nitter_mirrors.c ntt_fiber.c pogo_raids_jp.c
tiktok_jp_discover.c vics_traffic.c vnet.c wantedly_bizreach.c
yahoo_auctions.c yahoo_crowd_map.c
jcab_notams.c jcg_msi.c jftc_mergers.c mlit_road_traffic.c tmp_protests.c
xrain_radar.c food_poisoning.c influenza_surveillance.c ndb_open.c
regional_grid_outages.c
"
n=0
for f in $FILES; do
  if [ -f "$f" ]; then rm -f "$f" && n=$((n+1)); else echo "already gone: $f"; fi
done
echo "removed $n probe stub files"
