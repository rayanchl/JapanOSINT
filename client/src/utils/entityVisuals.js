// Single source of truth for entity-type rendering (chips, highlighter,
// graph, profile). Mirrors the OSINTsaas EntityReviewPanel colour/icon maps,
// reskinned to JapanOSINT Tailwind tokens. Covers the 45-type taxonomy from
// the analysis grammar; unknown types fall back to a neutral style.

const MAP = {
  email:    { color: 'bg-blue-500/15 text-blue-300 border-blue-500/30', dot: '#60a5fa', label: 'Email' },
  ip:       { color: 'bg-green-500/15 text-green-300 border-green-500/30', dot: '#4ade80', label: 'IP' },
  domain:   { color: 'bg-purple-500/15 text-purple-300 border-purple-500/30', dot: '#c084fc', label: 'Domain' },
  url:      { color: 'bg-purple-500/15 text-purple-300 border-purple-500/30', dot: '#c084fc', label: 'URL' },
  dns:      { color: 'bg-purple-500/15 text-purple-300 border-purple-500/30', dot: '#c084fc', label: 'DNS' },
  phone:    { color: 'bg-yellow-500/15 text-yellow-300 border-yellow-500/30', dot: '#facc15', label: 'Phone' },
  username: { color: 'bg-pink-500/15 text-pink-300 border-pink-500/30', dot: '#f472b6', label: 'Username' },
  person:   { color: 'bg-indigo-500/15 text-indigo-300 border-indigo-500/30', dot: '#818cf8', label: 'Person' },
  company:  { color: 'bg-orange-500/15 text-orange-300 border-orange-500/30', dot: '#fb923c', label: 'Company' },
  address:  { color: 'bg-rose-500/15 text-rose-300 border-rose-500/30', dot: '#fb7185', label: 'Address' },
  coordinates: { color: 'bg-rose-500/15 text-rose-300 border-rose-500/30', dot: '#fb7185', label: 'Coords' },
  vehicle:  { color: 'bg-amber-500/15 text-amber-300 border-amber-500/30', dot: '#fbbf24', label: 'Vehicle' },
  flight:   { color: 'bg-sky-500/15 text-sky-300 border-sky-500/30', dot: '#38bdf8', label: 'Flight' },
  vessel:   { color: 'bg-cyan-500/15 text-cyan-300 border-cyan-500/30', dot: '#22d3ee', label: 'Vessel' },
  container:{ color: 'bg-cyan-500/15 text-cyan-300 border-cyan-500/30', dot: '#22d3ee', label: 'Container' },
  crypto:   { color: 'bg-emerald-500/15 text-emerald-300 border-emerald-500/30', dot: '#34d399', label: 'Crypto' },
  asn:      { color: 'bg-teal-500/15 text-teal-300 border-teal-500/30', dot: '#2dd4bf', label: 'ASN' },
  hash:     { color: 'bg-slate-500/15 text-slate-300 border-slate-500/30', dot: '#94a3b8', label: 'Hash' },
  cve:      { color: 'bg-red-500/15 text-red-300 border-red-500/30', dot: '#f87171', label: 'CVE' },
  keyword:  { color: 'bg-gray-500/15 text-gray-300 border-gray-500/30', dot: '#9ca3af', label: 'Keyword' },
};
const FALLBACK = { color: 'bg-gray-500/15 text-gray-300 border-gray-500/30', dot: '#9ca3af', label: 'Entity' };

export function entityVisual(type) {
  return MAP[String(type || '').toLowerCase()] || { ...FALLBACK, label: type || 'Entity' };
}
