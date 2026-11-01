// Usage: node tools/setup.mjs --game <name> "<path to that game's ROM>"
// Verifies the ROM against games/<name>/game.json and remembers its location in local.json.
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { verifyRom, rememberRomPath, ROM_INFO } from './lib/rom.mjs';

const arg = process.argv.slice(2).find((a, k, all) => !a.startsWith('--') && all[k - 1] !== '--game');
if (!arg) {
  console.error(`Usage: node tools/setup.mjs --game <name> "<path to ${ROM_INFO.name} ROM>"`);
  process.exit(1);
}
const romPath = resolve(arg);
const err = verifyRom(readFileSync(romPath));
if (err) {
  console.error(`ROM rejected: ${err}`);
  process.exit(1);
}
rememberRomPath(romPath);
console.log(`OK: ${ROM_INFO.name} verified, path saved to local.json`);
