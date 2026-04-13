# JapanOSINT - Intelligence Map Platform

Real-time geospatial intelligence map aggregating 150+ Japanese public data sources onto a single interactive map.

## Features

- **Interactive Map** — MapLibre GL dark-themed map with 12+ data layers (earthquakes, weather, transit, air quality, radiation, cameras, population, land prices, river levels, crime, buildings, social media)
- **150+ Data Sources** — JMA, ODPT, e-Stat, PLATEAU, SORAMAME, NRA, MLIT, GSI, RESAS, and more
- **Source Dashboard** — Real-time monitoring of all sources with status, type (API/Dataset/Scraped/Web Request), usage stats, and data flow visualization
- **Real-time Feeds** — WebSocket push for earthquake alerts, weather warnings, and transport updates
- **Dark OSINT Theme** — Cyberpunk-inspired command center aesthetic

## Architecture

```
client/          React + Vite + MapLibre GL + Deck.gl + Tailwind
server/          Express + SQLite + node-cron + WebSocket
  collectors/    Data fetchers for each source (JMA, ODPT, etc.)
  routes/        REST API endpoints (/api/sources, /api/layers, /api/data)
  utils/         Database, scheduler, source registry (150+ sources)
```

## Quick Start

```bash
npm run install:all   # Install all dependencies
npm run dev           # Start both server (4000) and client (3000)
```

- Map UI: http://localhost:3000
- Source Dashboard: http://localhost:3000/sources
- API: http://localhost:4000/api/sources

## Data Source Categories

| Category | Sources | Examples |
|----------|---------|---------|
| Environment | 20+ | JMA earthquakes, weather, AMeDAS, Himawari satellite |
| Transport | 15+ | ODPT trains/buses, Shinkansen status, flight tracking |
| Geospatial | 10+ | PLATEAU 3D buildings, GSI tiles, active fault maps |
| Statistics | 15+ | e-Stat population mesh, RESAS, census data |
| Economy | 10+ | MLIT land prices, rental data, J-REIT |
| Safety | 10+ | Police crime maps, hazard maps, shelter locations |
| Infrastructure | 15+ | River levels, XRAIN radar, power grid, EV charging |
| Cyber/IoT | 8+ | Shodan, NICTER darknet, JPCERT alerts |
| Social | 10+ | Twitter/X, Flickr, YouTube |
| Satellite | 8+ | Tellus, Sentinel, Himawari-9, ALOS |
| Health | 5+ | NDB, influenza surveillance, pharmacy map |
| Commercial | 8+ | Docomo, Agoop, Zenrin, HERE Maps |
| Ocean/Marine | 7+ | AIS vessels, wave data, sea temperature |

## License

MIT
