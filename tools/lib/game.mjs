// Which game the tools work on, and where its files live:
//   games/<name>/       its config: game.json (ROM and features), patches.json, entries.json
//   generated/<name>/   the C recompiled from its ROM (never committed)
//   build/<name>/       its build, coverage.bin, disassembly and reports (never committed)
// The game is named by the GAME environment variable (CMake sets it), or --game <name>.
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

export const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');

export function listGames() {
  const dir = join(ROOT, 'games');
  return existsSync(dir) ? readdirSync(dir).filter((n) => existsSync(join(dir, n, 'game.json'))) : [];
}

function chosenGame() {
  const at = process.argv.indexOf('--game');
  const name = at >= 0 ? process.argv[at + 1] : process.env.GAME;
  if (name) return name;
  const all = listGames();
  if (all.length === 1) return all[0];
  throw new Error(`Which game? Set GAME or pass --game <name> (one of: ${all.join(', ') || 'none yet: run tools/new-game.mjs'})`);
}

export const GAME = chosenGame();
export const GAME_DIR = join(ROOT, 'games', GAME);
export const GEN_DIR = join(ROOT, 'generated', GAME);
export const BUILD_DIR = join(ROOT, 'build', GAME);

if (!existsSync(join(GAME_DIR, 'game.json'))) throw new Error(`No games/${GAME}/game.json: run tools/new-game.mjs with its ROM first`);
export const GAME_INFO = JSON.parse(readFileSync(join(GAME_DIR, 'game.json'), 'utf8'));

// The cartridges the game builds from: its own (game.json's rom, the main edition, which the
// addresses in patches.json and entries.json are written for) and any other edition of it
// (game.json's editions, e.g. another country's, with its own code). Each one:
//   { id, name, edition (the player's name for it), region (us, eu, jp), sha1, size, main }
export const EDITIONS = [
  { id: GAME_INFO.rom.id || 'main', name: GAME_INFO.rom.name, edition: GAME_INFO.rom.edition || GAME_INFO.title,
    region: GAME_INFO.rom.region || 'us', sha1: GAME_INFO.rom.sha1, size: GAME_INFO.rom.size, main: true },
  ...(GAME_INFO.editions || []).map((e) => ({ region: 'us', size: GAME_INFO.rom.size, ...e, main: false })),
];
export const MAIN_EDITION = EDITIONS[0];

// An edition's own file: `name` for the main one, else with "-<id>" before its extension
// ("coverage.bin" -> "coverage-jp.bin"), as runtime/src/bus.c's game_file_name.
export function editionFileName(edition, name) {
  if (edition.main) return name;
  const dot = name.lastIndexOf('.');
  return dot < 0 ? `${name}-${edition.id}` : `${name.slice(0, dot)}-${edition.id}${name.slice(dot)}`;
}

// The game's own config file, or `fallback` when it has none.
export function gameFile(name, fallback) {
  const path = join(GAME_DIR, name);
  return existsSync(path) ? JSON.parse(readFileSync(path, 'utf8')) : fallback;
}
