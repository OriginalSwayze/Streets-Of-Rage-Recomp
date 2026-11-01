// Usage: node tools/new-game.mjs "<path to a Genesis ROM>" [--name <folder name>]
//
// Reads the cartridge header and adds the game to the kit: games/<name>/game.json (its ROM,
// title and features) and an empty patches.json, and remembers where the ROM is in
// local.json (never committed). A game that is already here only has its ROM path updated.
// Every feature that needs the game's own addresses starts off; the engine runs without them.
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2);
const romArg = args.find((a, k) => !a.startsWith('--') && args[k - 1] !== '--name');
const nameAt = args.indexOf('--name');
if (!romArg) {
  console.error('Usage: node tools/new-game.mjs "<path to a Genesis ROM>" [--name <folder name>]');
  process.exit(1);
}

const romPath = resolve(romArg);
const rom = readFileSync(romPath);
const text = (a, b) => rom.subarray(a, b).toString('latin1').replace(/\0/g, ' ').replace(/\s+/g, ' ').trim();
const fail = (why) => { console.error(`Not added: ${why}`); process.exit(1); };

// ---- The cartridge header ($100-$1FF) ------------------------------------------------
if (rom.subarray(0, 14).toString('latin1') === 'SEGADISCSYSTEM') fail('this is a Sega CD disc image: the engine runs cartridges only');
if (rom.length < 0x200 || !/^SEGA/.test(text(0x100, 0x110))) {
  fail(`${romPath} has no Genesis header ("SEGA ..." at $100). Interleaved .smd dumps need converting to .bin/.md first.`);
}
const header = {
  console: text(0x100, 0x110),
  domestic: text(0x120, 0x150),
  overseas: text(0x150, 0x180),
  serial: text(0x180, 0x18e),
  checksum: rom.readUInt16BE(0x18e).toString(16).toUpperCase().padStart(4, '0'),
  io: text(0x190, 0x1a0),
  sram: text(0x1b0, 0x1b2) === 'RA',
  region: text(0x1f0, 0x200),
};

// Hardware the engine does not have (yet): say so now, not after a broken build.
const problems = [];
if (rom.length > 0x400000) problems.push(`it is ${rom.length >> 20} MB: games over 4 MB use a bank-switching mapper (e.g. Super Street Fighter II) the engine does not have yet`);
if (/MK-1229|MK-1229 -00/.test(header.serial) || /VIRTUA RACING/.test(header.overseas)) problems.push('Virtua Racing uses the SVP chip, which the engine does not have');
if (/32X/.test(header.console)) problems.push('this is a 32X game: a second pair of CPUs the engine does not have');
if (problems.length) fail(problems.join('; '));

// ---- Names ------------------------------------------------------------------------------
const small = new Set(['of', 'the', 'and', 'a', 'an', 'in', 'on', 'to', 'vs', 'for']);
const title = (header.overseas || header.domestic)
  .toLowerCase()
  .split(' ')
  .map((w, k) => (k > 0 && small.has(w) ? w : w.replace(/^\w/, (c) => c.toUpperCase())))
  .join(' ')
  .replace(/\bIi\b/g, 'II').replace(/\bIii\b/g, 'III');
const slug = nameAt >= 0 ? args[nameAt + 1] : title.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
const exe = title.replace(/[^A-Za-z0-9]+/g, ' ').split(' ').filter(Boolean).map((w) => w[0].toUpperCase() + w.slice(1)).join('');
const sha1 = createHash('sha1').update(rom).digest('hex');

// ---- Add it (or find it) ------------------------------------------------------------------
const gamesDir = join(ROOT, 'games');
mkdirSync(gamesDir, { recursive: true });
let name = null;
for (const g of readdirSync(gamesDir)) {
  const file = join(gamesDir, g, 'game.json');
  if (existsSync(file) && JSON.parse(readFileSync(file, 'utf8')).rom.sha1 === sha1) name = g;
}
if (name) {
  console.log(`${name}: already in the kit, ROM path updated`);
} else {
  name = slug;
  const dir = join(gamesDir, name);
  if (existsSync(join(dir, 'game.json'))) fail(`games/${name}/ already holds a different ROM: pass --name <another name>`);
  mkdirSync(dir, { recursive: true });
  const game = {
    title,
    exe,
    rom: { name: `${title} (${header.region || '?'})`, serial: header.serial, size: rom.length, sha1, headerChecksum: header.checksum },
    features: { ram: false, widescreen: false, launcher: false },
    // Most games with a battery save keep it mapped from the start; a few over 2 MB map it
    // themselves through $A130F1, as Beyond Oasis does.
    sram: { present: header.sram, always: header.sram && rom.length <= 0x200000 },
  };
  writeFileSync(join(dir, 'game.json'), JSON.stringify(game, null, 2) + '\n');
  writeFileSync(join(dir, 'patches.json'), '{}\n');
  console.log(`${name}: added "${title}" (${header.serial}, ${rom.length >> 10} KB, region ${header.region}${header.sram ? ', battery save' : ''})`);
}

const localFile = join(ROOT, 'local.json');
const local = existsSync(localFile) ? JSON.parse(readFileSync(localFile, 'utf8')) : {};
local.roms = { ...(local.roms || {}), [name]: romPath };
writeFileSync(localFile, JSON.stringify(local, null, 2) + '\n');
console.log(`next: node tools/kit.mjs ${name}     (builds it, plays it for coverage and checks it)`);
