import { describe, it, expect, afterEach } from 'vitest';
import { render as rtlRender, screen, cleanup } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import React from 'react';
import SearchCard from '../SearchCard.jsx';

// SearchCard renders EntityChips, which calls useNavigate() to pivot on an
// entity — that throws outside a router, so every render here gets one.
const render = (ui) => rtlRender(<MemoryRouter>{ui}</MemoryRouter>);

// vite.config.js sets `globals: false`, so @testing-library/react never gets
// to auto-register its afterEach hook and every render stays in the document.
// Without this the second test sees the first test's card and asserts against
// it — which is how a passing suite would have "proved" a banner that is not
// there. Unmount explicitly.
afterEach(cleanup);

// What this protects, in the order the three bugs were found.
//
// 1. A run whose analysis stage never happened rendered identically to one
//    that succeeded: phase "completed", 100%, a synthesis paragraph, one
//    service. The backend now ships `degraded` + `stage_errors` and the card
//    has to actually SHOW them, or the honest reporting stops at the wire.
// 2. `r.data` arrives as a parsed JSON value (pipeline.c parses it), and the
//    card rendered it with String(), so every structured service payload
//    displayed as the literal text "[object Object]" — and, being 15
//    characters, passed the truncation check without a warning.
// 3. `r.sources` — the per-service upstream attribution osint_dispatch.c has
//    always built — was never rendered at all, which is what "services are not
//    attributed" looked like on screen even once routing worked.

function snap(over = {}) {
  return {
    request_id: 'r1',
    query: 'who owns example.com',
    phase: 'completed',
    progress_percent: 100,
    gpt_thinking: '',
    entities: [],
    discovered_entities: [],
    services: [],
    services_assigned: [],
    degraded: false,
    stage_errors: [],
    current_round: 0,
    max_rounds: 5,
    ...over,
  };
}

describe('SearchCard degradation reporting', () => {
  it('shows a degraded banner naming every failed stage', () => {
    render(
      <SearchCard
        query="q"
        snapshot={snap({
          degraded: true,
          stage_errors: [
            {
              stage: 'analysis',
              code: 'llm_unreachable',
              severity: 'error',
              detail: 'no LLM reachable at http://localhost:8080',
            },
            {
              stage: 'synthesis',
              code: 'llm_timeout',
              severity: 'error',
              detail: 'ran out its 60000 ms budget',
            },
          ],
        })}
      />,
    );
    expect(screen.getByText(/Degraded investigation/i)).toBeTruthy();
    expect(screen.getByText(/analysis · llm_unreachable/)).toBeTruthy();
    expect(screen.getByText(/synthesis · llm_timeout/)).toBeTruthy();
    // The banner must say plainly that this is not a complete investigation —
    // a code the reader has to look up is not the same as being told.
    expect(screen.getByText(/not a complete investigation/i)).toBeTruthy();
  });

  it('does not call a run degraded when it only carries notices', () => {
    // Every run on this registry bounds its service catalogue, so a notice
    // must not paint the card red or the warning becomes background noise.
    render(
      <SearchCard
        query="q"
        snapshot={snap({
          degraded: false,
          stage_errors: [
            {
              stage: 'analysis',
              code: 'service_catalogue_bounded',
              severity: 'notice',
              detail: 'shown 1535 of 1535',
            },
          ],
        })}
      />,
    );
    expect(screen.queryByText(/Degraded investigation/i)).toBeNull();
    expect(screen.getByText(/Run notices/i)).toBeTruthy();
    expect(screen.getByText(/analysis · service_catalogue_bounded/)).toBeTruthy();
  });

  it('renders a structured service payload as JSON, never "[object Object]"', () => {
    const { container } = render(
      <SearchCard
        query="q"
        snapshot={snap({
          results: {
            synthesis: '',
            services: [
              {
                name: 'DOMAIN_WHOIS',
                entity: 'example.com',
                success: true,
                record_count: 2,
                data: { record_count: 2, records: [{ registrar: 'IANA' }] },
                sources: [],
              },
            ],
          },
        })}
      />,
    );
    const text = container.textContent;
    expect(text).not.toContain('[object Object]');
    expect(text).toContain('registrar');
    expect(text).toContain('IANA');
  });

  it('renders per-service source attribution and record counts', () => {
    const { container } = render(
      <SearchCard
        query="q"
        snapshot={snap({
          services: [
            {
              name: 'DOMAIN_WHOIS',
              status: 'completed',
              entities: 'example.com',
              results_count: 1,
              status_message: 'done',
            },
          ],
          results: {
            synthesis: '',
            services: [
              {
                name: 'DOMAIN_WHOIS',
                entity: 'example.com',
                success: true,
                record_count: 2,
                data: null,
                sources: [
                  { name: 'rdap.verisign.com', status: 'ok', records: 2 },
                ],
              },
            ],
          },
        })}
      />,
    );
    // React splits these across text nodes, so assert on the rendered text as
    // a whole rather than on element boundaries.
    const text = container.textContent;
    // which upstream actually answered, and with how many rows...
    expect(text).toContain('rdap.verisign.com');
    expect(text).toContain('2 rec');
    expect(text).toContain('2 records stored');
    // ...and which entity the service was dispatched on.
    expect(text).toContain('(example.com)');
  });

  it('never prints an unmeasured per-host record count as zero', () => {
    // The server sends records:null when it could only attribute by HTTP host
    // — it knows which hosts were contacted, not which rows came from which.
    // `?? 0` turned that unknown into a confident "0 rec" on hosts that had in
    // fact returned data (and, before the server-side fix, into the service's
    // whole total repeated on every one of 60 hosts).
    const { container } = render(
      <SearchCard
        query="q"
        snapshot={snap({
          results: {
            synthesis: '',
            services: [
              {
                name: 'SOCIAL_EMAIL',
                entity: 'alice@example.org',
                success: true,
                record_count: 187,
                data: null,
                sources: [
                  { name: 'github.com', status: 'ok', records: null, requests: 3 },
                  { name: 'medium.com', status: 'error', records: null, requests: 1 },
                ],
              },
            ],
          },
        })}
      />,
    );
    const text = container.textContent;
    expect(text).toContain('github.com');
    expect(text).toContain('3 req');
    expect(text).toContain('1 req');
    expect(text).not.toContain('0 rec');
    // the real total still shows, where it is actually true
    expect(text).toContain('187 records stored');
  });
});
