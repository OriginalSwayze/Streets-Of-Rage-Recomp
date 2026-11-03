// Extra trusted entry points for the tracer: hand-listed addresses in
// games/<name>/entries.json, code seen running in earlier play (games/<name>/coverage.txt,
// committed, so a fresh build has it), and code the runtime saw running here
// (build/<name>/coverage.bin).
import { readFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import { GAME_DIR, BUILD_DIR, MAIN_EDITION, editionFileName } from './game.mjs';

// coverage.txt: one hex address, or a "start-end" range of even addresses, per line.
export function loadCoverageList() {
  const file = join(GAME_DIR, 'coverage.txt'), out = [];
  if (!existsSync(file)) return out;
  for (const line of readFileSync(file, 'utf8').split('\n')) {
    const m = /^([0-9A-Fa-f]+)(?:-([0-9A-Fa-f]+))?/.exec(line.trim());
    if (m) for (let a = parseInt(m[1], 16); a <= parseInt(m[2] || m[1], 16); a += 2) out.push(a);
  }
  return out;
}

// The main edition's, or with `config` (tools/lib/editions.mjs configFor) another edition's:
// its mapped entries and coverage, and its own coverage-<id>.bin.
export function loadExtraEntries(edition = MAIN_EDITION, config = null) {
  const entries = [], covered = config ? [...config.coverage] : loadCoverageList();
  if (config) for (const s of config.entries) entries.push(parseInt(s, 16));
  const entriesFile = join(GAME_DIR, 'entries.json');
  if (!config && existsSync(entriesFile)) for (const s of JSON.parse(readFileSync(entriesFile, 'utf8'))) entries.push(parseInt(s, 16));
  const cov = join(BUILD_DIR, editionFileName(edition, 'coverage.bin'));
  if (existsSync(cov)) {
    // One bit per even address: byte k bit b covers address k*16 + b*2.
    const bits = readFileSync(cov);
    for (let k = 0; k < bits.length; k++) {
      if (!bits[k]) continue;
      for (let b = 0; b < 8; b++) if (bits[k] & (1 << b)) covered.push(k * 16 + b * 2);
    }
  }
  return { entries, covered };
}
