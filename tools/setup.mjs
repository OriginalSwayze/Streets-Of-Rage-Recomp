// Usage: node tools/setup.mjs --game <name> "<path to that game's ROM>"
// Verifies the ROM against games/<name>/game.json (any of its editions) and remembers its
// location in local.json.
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { verifyRom, identifyRom, rememberRomPath, ROM_INFO } from './lib/rom.mjs';

const arg = process.argv.slice(2).find((a, k, all) => !a.startsWith('--') && all[k - 1] !== '--game');
if (!arg) {
  console.error(`Usage: node tools/setup.mjs --game <name> "<path to ${ROM_INFO.name} ROM>"`);
  process.exit(1);
}
const romPath = resolve(arg);
const rom = readFileSync(romPath);
const err = verifyRom(rom);
if (err) {
  console.error(`ROM rejected: ${err}`);
  process.exit(1);
}
const edition = identifyRom(rom);
rememberRomPath(romPath, edition);
console.log(`OK: ${edition.name} verified, path saved to local.json`);
