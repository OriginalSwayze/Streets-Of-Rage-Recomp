// Loads and verifies the user's own ROM. The ROM is never copied into the repo:
// local.json (gitignored) only stores where each game's ROM lives on disk.
import { createHash } from 'node:crypto';
import { readFileSync, existsSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, GAME, GAME_INFO } from './game.mjs';

export { ROOT };
export const ROM_INFO = GAME_INFO.rom;

export function verifyRom(buf) {
  if (buf.length !== ROM_INFO.size) {
    return `wrong size: ${buf.length} bytes, expected ${ROM_INFO.size}`;
  }
  const sha1 = createHash('sha1').update(buf).digest('hex');
  if (sha1 !== ROM_INFO.sha1) {
    return `wrong SHA-1: ${sha1}, expected ${ROM_INFO.sha1} (${ROM_INFO.name})`;
  }
  return null;
}

const LOCAL = join(ROOT, 'local.json');
const readLocal = () => (existsSync(LOCAL) ? JSON.parse(readFileSync(LOCAL, 'utf8')) : {});

export function romPathFromConfig() {
  const path = (readLocal().roms || {})[GAME];
  if (!path) throw new Error(`No ROM configured for ${GAME}. Run: node tools/new-game.mjs "<path to ${ROM_INFO.name} ROM>"`);
  return path;
}

export function rememberRomPath(path) {
  const local = readLocal();
  local.roms = { ...(local.roms || {}), [GAME]: path };
  writeFileSync(LOCAL, JSON.stringify(local, null, 2) + '\n');
}

export function loadRom() {
  const path = romPathFromConfig();
  const buf = readFileSync(path);
  const err = verifyRom(buf);
  if (err) throw new Error(`${path}: ${err}`);
  return buf;
}
