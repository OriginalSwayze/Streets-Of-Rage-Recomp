// Loads and verifies the user's own ROMs. No ROM is ever copied into the repo:
// local.json (gitignored) only stores where each one lives on disk: roms.<game> for the
// game's main edition, roms.<game>-<id> for another edition of it.
import { createHash } from 'node:crypto';
import { readFileSync, existsSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, GAME, GAME_INFO, EDITIONS, MAIN_EDITION } from './game.mjs';

export { ROOT };
export const ROM_INFO = GAME_INFO.rom;

// The edition of this game a ROM image is, or null.
export function identifyRom(buf) {
  const sha1 = createHash('sha1').update(buf).digest('hex');
  return EDITIONS.find((e) => e.size === buf.length && e.sha1 === sha1) || null;
}

// Why a ROM image is none of this game's editions, or null.
export function verifyRom(buf) {
  if (identifyRom(buf)) return null;
  if (!EDITIONS.some((e) => e.size === buf.length)) return `wrong size: ${buf.length} bytes, expected ${ROM_INFO.size}`;
  const sha1 = createHash('sha1').update(buf).digest('hex');
  return `wrong SHA-1: ${sha1}, not one of: ${EDITIONS.map((e) => `${e.sha1} (${e.name})`).join(', ')}`;
}

const LOCAL = join(ROOT, 'local.json');
const readLocal = () => (existsSync(LOCAL) ? JSON.parse(readFileSync(LOCAL, 'utf8')) : {});
const romKey = (edition) => (edition.main ? GAME : `${GAME}-${edition.id}`);

export function romPathFromConfig(edition = MAIN_EDITION) {
  const path = (readLocal().roms || {})[romKey(edition)];
  if (!path) throw new Error(`No ROM configured for ${GAME}. Run: node tools/new-game.mjs "<path to ${edition.name} ROM>"`);
  return path;
}

// Remembers a ROM's path under the edition it is (the main one if not given).
export function rememberRomPath(path, edition = MAIN_EDITION) {
  const local = readLocal();
  local.roms = { ...(local.roms || {}), [romKey(edition)]: path };
  writeFileSync(LOCAL, JSON.stringify(local, null, 2) + '\n');
}

// The main edition's ROM (the tools that read the addresses it is written for).
export function loadRom() {
  const path = romPathFromConfig();
  const buf = readFileSync(path);
  if (identifyRom(buf) !== MAIN_EDITION) throw new Error(`${path}: ${verifyRom(buf) || `this is ${identifyRom(buf).name}, not ${MAIN_EDITION.name}`}`);
  return buf;
}

// [{ edition, path, rom }] for every edition whose ROM is set up, in game.json order.
export function loadRoms() {
  const roms = readLocal().roms || {}, out = [];
  for (const edition of EDITIONS) {
    const path = roms[romKey(edition)];
    if (!path) continue;
    const rom = readFileSync(path);
    if (identifyRom(rom) !== edition) throw new Error(`${path}: ${verifyRom(rom) || `this is ${identifyRom(rom).name}, not ${edition.name}`}`);
    out.push({ edition, path, rom });
  }
  if (!out.length) throw new Error(`No ROM configured for ${GAME}. Run: node tools/setup.mjs --game ${GAME} "<path to its ROM>"`);
  return out;
}
