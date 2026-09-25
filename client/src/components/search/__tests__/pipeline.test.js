import { describe, it, expect } from 'vitest';
import { pipelineStages, targetStageIndex, SearchLinks, isJapanese, stageErrorHeadline } from '../pipeline.js';

describe('pipelineStages', () => {
  it('paints a failed analysis stage as error on a completed run', () => {
    const rows = pipelineStages({ phase: 'completed', progress_percent: 100,
      stage_errors: [{ stage: 'analysis', code: 'llm_unreachable', severity: 'error' }] });
    expect(rows.find((r) => r.stage.key === 'gpt_analyzing').state).toBe('error');
    expect(rows.find((r) => r.stage.key === 'aggregating').state).toBe('done');
  });
  it('treats a notice-only run as fully done', () => {
    const rows = pipelineStages({ phase: 'completed', progress_percent: 100,
      stage_errors: [{ stage: 'analysis', code: 'service_catalogue_bounded', severity: 'notice' }] });
    expect(rows.every((r) => r.state === 'done')).toBe(true);
  });
  it('maps unlisted phases onto their diagram row and falls back to percent', () => {
    expect(targetStageIndex({ phase: 'services_launching', progress_percent: 30 })).toBe(3);
    expect(targetStageIndex({ phase: 'something_new', progress_percent: 92 })).toBe(6);
    expect(targetStageIndex({ phase: 'error', progress_percent: 17 })).toBe(1);
  });
});

describe('SearchLinks.sourcesOf', () => {
  it('never turns an unmeasured per-host count into zero', () => {
    const snap = { results: { services: [
      { name: 'SOCIAL_EMAIL', sources: [{ name: 'github.com', status: 'ok', records: null, requests: 3 }] },
      { name: 'SOCIAL_EMAIL', sources: [{ name: 'github.com', status: 'error', records: null, requests: 1 }] },
    ] } };
    const [s] = SearchLinks.sourcesOf('SOCIAL_EMAIL', snap);
    expect(s.records).toBeNull();
    expect(s.requests).toBe(4);
    expect(s.status).toBe('error');
  });
});

describe('helpers', () => {
  it('detects Japanese script', () => {
    expect(isJapanese('東京')).toBe(true);
    expect(isJapanese('tokyo')).toBe(false);
  });
  it('never hides an unknown stage code', () => {
    expect(stageErrorHeadline('brand_new_code')).toBe('brand new code');
  });
});
