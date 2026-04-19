/**
 * Military aircraft detection — ICAO24 hex ranges + callsign prefix fallback.
 * Ranges sourced from publicly-documented FAA/ICAO allocations and
 * hobbyist trackers (ADSBExchange, adsb.lol).
 */

// Inclusive [start, end] 24-bit hex ranges, lowercase.
const MILITARY_RANGES = [
  // United States
  { start: 0xae0000, end: 0xafffff, note: 'USAF / USN / USA' },
  // United Kingdom
  { start: 0x43c000, end: 0x43cfff, note: 'RAF' },
  // Canada
  { start: 0xc00000, end: 0xc0ffff, note: 'CAF (subset)' },
  // Japan
  { start: 0x868000, end: 0x86ffff, note: 'JASDF / JMSDF / JGSDF' },
  // Australia
  { start: 0x7cf800, end: 0x7cffff, note: 'RAAF' },
  // Germany
  { start: 0x3ea000, end: 0x3ebfff, note: 'Luftwaffe' },
  // France
  { start: 0x3b7000, end: 0x3b7fff, note: "Armee de l'Air" },
  // Italy
  { start: 0x33ff00, end: 0x33ffff, note: 'AMI' },
  // Spain
  { start: 0x3443c0, end: 0x3443ff, note: 'Ejercito del Aire' },
  // Netherlands
  { start: 0x484800, end: 0x4848ff, note: 'RNLAF' },
  // South Korea
  { start: 0x71be00, end: 0x71beff, note: 'ROKAF' },
];

// Callsign prefixes (case-insensitive). Anchored to start; must be followed
// by a digit to avoid matching civil callsigns that happen to start with
// the same letters.
const CALLSIGN_RE = /^(RCH|CNV|EVAC|SAM|JFR|JAPAN|PAT|REACH|DUKE|SHARK|NAVY|RESCUE|CONVOY|HKY|VADER|RAID|PACK)\d/i;

export function isMilitaryByIcao24(icao24) {
  if (!icao24 || typeof icao24 !== 'string') return false;
  const hex = icao24.trim().toLowerCase();
  if (!/^[0-9a-f]{6}$/.test(hex)) return false;
  const n = parseInt(hex, 16);
  for (const r of MILITARY_RANGES) {
    if (n >= r.start && n <= r.end) return true;
  }
  return false;
}

export function isMilitaryByCallsign(callsign) {
  if (!callsign || typeof callsign !== 'string') return false;
  return CALLSIGN_RE.test(callsign.trim());
}

/**
 * Returns { is_military, military_reason } for the given aircraft identifiers.
 * ICAO24 range is checked first (hardware-assigned, higher confidence);
 * callsign is a fallback for aircraft whose hex falls outside known military blocks.
 */
export function classifyMilitary({ icao24, callsign }) {
  if (isMilitaryByIcao24(icao24)) {
    return { is_military: true, military_reason: 'icao_range' };
  }
  if (isMilitaryByCallsign(callsign)) {
    return { is_military: true, military_reason: 'callsign_prefix' };
  }
  return { is_military: false, military_reason: null };
}
