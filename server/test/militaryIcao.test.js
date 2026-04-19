import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  isMilitaryByIcao24,
  isMilitaryByCallsign,
  classifyMilitary,
} from '../src/collectors/_militaryIcao.js';

test('USAF range AE0000-AFFFFF is military', () => {
  assert.equal(isMilitaryByIcao24('ae1234'), true);
  assert.equal(isMilitaryByIcao24('AE1234'), true);
  assert.equal(isMilitaryByIcao24('af0000'), true);
  assert.equal(isMilitaryByIcao24('affffe'), true);
});

test('USAF range boundaries are inclusive', () => {
  assert.equal(isMilitaryByIcao24('ae0000'), true);  // exact lower bound
  assert.equal(isMilitaryByIcao24('afffff'), true);  // exact upper bound
  assert.equal(isMilitaryByIcao24('adffff'), false); // one below lower bound
  assert.equal(isMilitaryByIcao24('b00000'), false); // one above upper bound
});

test('JASDF range 86xxxx is military', () => {
  assert.equal(isMilitaryByIcao24('86f123'), true);
});

test('civilian ICAO24 is not military', () => {
  assert.equal(isMilitaryByIcao24('4c1b2a'), false);
  assert.equal(isMilitaryByIcao24('844abc'), false);
});

test('malformed or empty icao24 returns false', () => {
  assert.equal(isMilitaryByIcao24(''), false);
  assert.equal(isMilitaryByIcao24(null), false);
  assert.equal(isMilitaryByIcao24('zzzzzz'), false);
  assert.equal(isMilitaryByIcao24('ae12'), false);
});

test('RCH callsign is military', () => {
  assert.equal(isMilitaryByCallsign('RCH871'), true);
});

test('SAM callsign is military', () => {
  assert.equal(isMilitaryByCallsign('SAM100'), true);
});

test('JAL001 callsign is NOT military', () => {
  assert.equal(isMilitaryByCallsign('JAL001'), false);
});

test('empty callsign is not military', () => {
  assert.equal(isMilitaryByCallsign(''), false);
  assert.equal(isMilitaryByCallsign(null), false);
});

test('classifyMilitary returns reason=icao_range when hex matches', () => {
  const r = classifyMilitary({ icao24: 'ae1234', callsign: 'ANYTHING' });
  assert.equal(r.is_military, true);
  assert.equal(r.military_reason, 'icao_range');
});

test('classifyMilitary returns reason=callsign_prefix when only callsign matches', () => {
  const r = classifyMilitary({ icao24: '4c1b2a', callsign: 'RCH871' });
  assert.equal(r.is_military, true);
  assert.equal(r.military_reason, 'callsign_prefix');
});

test('classifyMilitary returns is_military=false for pure civilian', () => {
  const r = classifyMilitary({ icao24: '4c1b2a', callsign: 'ANA106' });
  assert.equal(r.is_military, false);
  assert.equal(r.military_reason, null);
});
