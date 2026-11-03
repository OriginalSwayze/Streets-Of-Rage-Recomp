// Where the main edition's code addresses (games/<name>/patches.json, entries.json and
// coverage.txt) are in the game's other editions (game.json's editions). The other
// countries' cartridges moved the code but mostly kept it: tools/map-editions.mjs finds
// each address by the bytes around it in the main ROM and saves the result in
// games/<name>/editions.json (addresses only, no bytes of any game), so a build needs only
// the ROMs it builds.
//
// An edition in game.json may say what the mapping cannot know:
//   absent      main-edition addresses that edition does not have (left out of its patches)
//   patches     patches of its own, at its own addresses (merged after the mapped ones)
//   entries     entry points of its own
//   sameCodeAs  another edition whose code it shares (a patched copy of that cartridge):
//               its mapping, absent, patches and entries are used
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { decode } from './m68k.mjs';
import { GAME_DIR, EDITIONS, gameFile } from './game.mjs';
import { loadCoverageList } from './inputs.mjs';

const isHex = (s) => typeof s === 'string' && /^[0-9A-F]{6}$/.test(s);
export const hex6 = (a) => a.toString(16).toUpperCase().padStart(6, '0');

// Every hex address in a config object (keys and values).
export function addressesIn(obj, out = new Set()) {
  const add = (v) => {
    if (isHex(v)) out.add(parseInt(v, 16));
    else if (Array.isArray(v)) v.forEach(add);
    else if (v && typeof v === 'object') addressesIn(v, out);
  };
  for (const [k, v] of Object.entries(obj)) {
    if (k === 'comment') continue;
    if (isHex(k)) out.add(parseInt(k, 16));
    add(v);
  }
  return out;
}

const uniqueHit = (target, pat, at) => {
  const first = target.indexOf(pat);
  return first < 0 || target.indexOf(pat, first + 1) >= 0 ? -1 : first + at;
};

// The instruction at `a` in the main ROM is the one at `b` in the target: the same bytes
// (for a branch or call, whose distance can change, the same opcode).
export function sameInstruction(main, a, target, b) {
  if (b < 0 || b + 10 > target.length) return false;
  const i = decode(main, a);
  if (!i) return false;
  if (main.subarray(a, a + i.len).equals(target.subarray(b, b + i.len))) return true;
  return i.target !== undefined && main.readUInt16BE(a) === target.readUInt16BE(b);
}

// Map of main-ROM address -> target address for `list`; the ones not found are left out.
export function mapAddresses(main, target, list) {
  const found = new Map();
  for (const a of list) {
    // Windows from the address forward (the code before it may be another routine that
    // changed), then centred on it, growing until the bytes occur once.
    for (const len of [12, 16, 24, 32, 48, 64]) {
      let hit = uniqueHit(target, main.subarray(a, a + len), 0);
      if (hit < 0) hit = uniqueHit(target, main.subarray(a - len / 2, a + len / 2), len / 2);
      if (hit >= 0) { found.set(a, hit); break; }
    }
  }
  for (const a of list) {
    if (found.has(a)) continue;
    // Repeated code: from the nearest address found, at its main-ROM distance.
    const near = [...found.keys()].filter((b) => Math.abs(b - a) < 0x1000).sort((x, y) => Math.abs(x - a) - Math.abs(y - a));
    for (const b of near) {
      const c = found.get(b) + (a - b);
      if (sameInstruction(main, a, target, c)) { found.set(a, c); break; }
    }
  }
  for (const [a, b] of found) if (!sameInstruction(main, a, target, b)) found.delete(a);
  return found;
}

// coverage.txt's addresses in the target: each run of code seen running is placed where the
// same bytes are (first at the distance the previous run moved by, which most code shares).
// A misplaced one would only add code that never runs: the tracer decodes what is there.
export function mapCoverage(main, target, addresses) {
  const out = [];
  let delta = 0;
  const same = (a, b, n) => b >= 0 && b + n <= target.length && main.subarray(a, a + n).equals(target.subarray(b, b + n));
  for (let k = 0; k < addresses.length; ) {
    let j = k;
    while (j + 1 < addresses.length && addresses[j + 1] === addresses[j] + 2) j++;
    const a = addresses[k], n = Math.max(addresses[j] + 2 - a, 16);
    let b = same(a, a + delta, n) ? a + delta : -1;
    if (b < 0) for (const len of [16, 24, 32, 48]) if ((b = uniqueHit(target, main.subarray(a, a + Math.max(len, n)), 0)) >= 0) break;
    if (b >= 0) {
      delta = b - a;
      for (let i = k; i <= j; i++) out.push(addresses[i] + delta);
    }
    k = j + 1;
  }
  return out;
}

// A config object with every main-ROM address replaced by the edition's; keys and array items
// whose address it lacks (absent) are left out.
export function remapConfig(obj, map, absent) {
  const conv = (s) => (isHex(s) && map.has(parseInt(s, 16)) ? hex6(map.get(parseInt(s, 16))) : s);
  const gone = (s) => isHex(s) && absent.has(parseInt(s, 16));
  const walk = (v) => {
    if (Array.isArray(v)) return v.filter((x) => !gone(x)).map(walk);
    if (v && typeof v === 'object') {
      return Object.fromEntries(Object.entries(v).filter(([k, x]) => k !== 'comment' && !gone(k) && !gone(x)).map(([k, x]) => [conv(k), walk(x)]));
    }
    return conv(v);
  };
  return walk(obj);
}

// Merges an edition's own patches into the mapped ones (objects key by key, lists appended).
export function mergeConfig(base, extra) {
  if (Array.isArray(base) && Array.isArray(extra)) return [...base, ...extra];
  if (base && extra && typeof base === 'object' && typeof extra === 'object') {
    const out = { ...base };
    for (const [k, v] of Object.entries(extra)) out[k] = k in out ? mergeConfig(out[k], v) : v;
    return out;
  }
  return extra;
}

// The edition whose mapping an edition uses (itself, or the one it shares its code with).
export const codeEdition = (e) => (e.sameCodeAs ? EDITIONS.find((x) => x.id === e.sameCodeAs) : e);

const MAPPED = join(GAME_DIR, 'editions.json');

// { patches, entries, coverage } for an edition: the main edition's own files, or its saved
// mapping with its own additions.
export function configFor(edition) {
  const patches = gameFile('patches.json', {}), entries = gameFile('entries.json', []);
  if (edition.main) return { patches, entries, coverage: loadCoverageList() };
  const code = codeEdition(edition);
  const all = existsSync(MAPPED) ? JSON.parse(readFileSync(MAPPED, 'utf8')) : {};
  const m = all[code.id];
  if (!m) throw new Error(`no addresses for the ${code.name} edition in ${MAPPED}: run node tools/map-editions.mjs --game <name> with both ROMs set up`);
  const coverage = [];
  for (const s of m.coverage || []) {
    const [from, to = from] = s.split('-').map((x) => parseInt(x, 16));
    for (let a = from; a <= to; a += 2) coverage.push(a);
  }
  return {
    patches: mergeConfig(m.patches, code.patches || {}),
    entries: [...m.entries, ...(code.entries || [])],
    coverage,
  };
}
