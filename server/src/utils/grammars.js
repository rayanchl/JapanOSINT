// server/src/utils/grammars.js
//
// Loads the verbatim OSINTsaas GBNF grammars copied into server/grammars/.
// These are fed to llama.cpp's `grammar` parameter (via llmClient.complete)
// so the ported OSINT pipeline + entity extractor constrain output exactly
// as the tuned C backend did. Contents are read once and cached.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const GRAMMAR_DIR = new URL('../../grammars/', import.meta.url);
const cache = new Map();

/**
 * @param {'osint_analysis'|'entity_extraction'|'page_analysis'|'suggestions'} name
 * @returns {string} inline GBNF content, or '' if missing (caller falls back
 *                    to ungrammared generation rather than crashing).
 */
export function loadGrammar(name) {
  if (cache.has(name)) return cache.get(name);
  let content = '';
  try {
    content = readFileSync(fileURLToPath(new URL(`${name}.gbnf`, GRAMMAR_DIR)), 'utf8');
  } catch {
    content = '';
  }
  cache.set(name, content);
  return content;
}
