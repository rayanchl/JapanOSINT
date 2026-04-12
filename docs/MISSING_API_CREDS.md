# Missing / Required API Credentials

This project references the following API credentials via `process.env`, but
none are currently provided (there is no `.env` or `.env.example` in the repo).
Collectors fall back to `''` when the env var is unset, so they run in a
degraded / empty state until the credential is supplied.

## Credentials used by collectors

| Env var | Provider | Used by | Purpose |
|---|---|---|---|
| `MARINETRAFFIC_API_KEY` | MarineTraffic | `collectors/maritimeAis.js`, `collectors/marineTraffic.js` | Live vessel AIS positions |
| `VESSELFINDER_API_KEY` | VesselFinder | `collectors/maritimeAis.js`, `collectors/vesselFinder.js` | Live vessel AIS positions |
| `FACEBOOK_ACCESS_TOKEN` | Meta Graph API | `collectors/facebookGeo.js` | Geo-tagged Facebook data |
| `SENTINELHUB_CLIENT_ID` | Sentinel Hub | `collectors/sentinelHub.js` | Satellite imagery (OAuth client) |
| `SENTINELHUB_CLIENT_SECRET` | Sentinel Hub | `collectors/sentinelHub.js` | Satellite imagery (OAuth client) |
| `WIGLE_API_KEY` | WiGLE | `collectors/wifiNetworks.js` | Wi-Fi network geolocation |
| `OPENSKY_USER` | OpenSky Network | `collectors/flightAdsb.js` | ADS-B flight tracking (basic auth) |
| `OPENSKY_PASS` | OpenSky Network | `collectors/flightAdsb.js` | ADS-B flight tracking (basic auth) |
| `TWITTER_BEARER_TOKEN` | Twitter / X API v2 | `collectors/twitterGeo.js` | Geo-tagged tweets |
| `RESAS_API_KEY` | RESAS (METI) | `collectors/resasPopulation.js`, `collectors/resasIndustry.js`, `collectors/resasTourism.js` | Regional economy / tourism / industry stats |
| `ESTAT_APP_ID` | e-Stat | `collectors/estatCensus.js` | Census data |
| `ESTAT_API_KEY` | e-Stat | `collectors/estatPopulation.js` | Population mesh (note: different var name than `ESTAT_APP_ID`; likely should be unified) |
| `SHODAN_API_KEY` | Shodan | `collectors/shodanIot.js` | Exposed IoT / ICS devices |
| `OPENCELLID_KEY` | OpenCelliD | `collectors/cellTowers.js` | Cell tower locations |
| `HOTPEPPER_API_KEY` | Recruit HotPepper Gourmet | `collectors/tabelogRestaurants.js` | Restaurant listings |
| `AERODATABOX_KEY` | AeroDataBox (RapidAPI) | `collectors/hanedaFlights.js`, `collectors/naritaFlights.js` | Airport arrivals/departures |

## Config-only (not secret, but required for data)

| Env var | Used by | Purpose |
|---|---|---|
| `GOOGLE_MYMAPS_IDS` | `collectors/googleMyMaps.js` | Comma-separated list of Google My Maps `mid`s to import |

## Infrastructure (already have sane defaults)

- `PORT` — server port (defaults to `4000`)
- `OVERPASS_URL` — OSM Overpass endpoint (defaults to `https://overpass-api.de/api/interpreter`)

## Notes / follow-ups

- `ESTAT_APP_ID` and `ESTAT_API_KEY` refer to the same e-Stat credential but
  use different variable names in two collectors — worth consolidating.
- No `.env.example` file exists; adding one with the keys above would make
  onboarding easier.
- All credentials are optional at runtime (`|| ''` fallback). Collectors
  that require them will return empty data when the key is missing rather
  than failing loudly.
