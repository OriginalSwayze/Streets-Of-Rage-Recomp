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

// The game's own config file, or `fallback` when it has none.
export function gameFile(name, fallback) {
  const path = join(GAME_DIR, name);
  return existsSync(path) ? JSON.parse(readFileSync(path, 'utf8')) : fallback;
}
