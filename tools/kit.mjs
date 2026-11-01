// Usage: node tools/kit.mjs <game> [--frames N] [--rounds N]
//
// Everything after tools/new-game.mjs, in one go:
//   1. disassemble, configure (build/<game>) and build
//   2. play it without a window to find its code: Start through the menus, then random
//      play (N frames, default 20000); the runtime records every instruction it runs
//   3. recompile with that coverage and build again, while the native share still grows
//   4. check the build against the interpreter: the same inputs must give the same machine
//      state on every frame
//   5. write build/<game>/report/report.md with the numbers and screenshots
// The input script is fixed (seeded), so two runs of this give the same report.
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { join } from 'node:path';

const args = process.argv.slice(2);
const opt = (name, def) => { const k = args.indexOf(name); return k >= 0 ? Number(args[k + 1]) : def; };
const game = args.find((a, k) => !a.startsWith('--') && !(args[k - 1] || '').startsWith('--'));
if (!game) { console.error('Usage: node tools/kit.mjs <game> [--frames N] [--rounds N]'); process.exit(1); }
process.env.GAME = game;
const { ROOT, GAME_INFO, BUILD_DIR } = await import('./lib/game.mjs');
const { romPathFromConfig } = await import('./lib/rom.mjs');

const FRAMES = opt('--frames', 20000);
const ROUNDS = opt('--rounds', 3);
const rom = romPathFromConfig();
const exe = join(BUILD_DIR, `${GAME_INFO.exe}.exe`);
const reportDir = join(BUILD_DIR, 'report');

// ---- Tools: cmake and clang on PATH, or the toolchain this machine was set up with ----------
const env = { ...process.env, GAME: game };
const onPath = (cmd) => spawnSync(cmd, ['--version'], { env, shell: true }).status === 0;
if (!onPath('cmake')) {
  const tc = join(homedir(), '.local', 'share', 'retcomm', 'toolchains', 'cmake-clang-v1', 'latest', 'bin');
  if (existsSync(tc)) env.PATH = `${tc};${env.PATH}`;
  else { console.error('cmake not found: install CMake, Ninja and Clang (see README)'); process.exit(1); }
}
const run = (cmd, argv, what) => {
  process.stdout.write(`${what}... `);
  const r = spawnSync(cmd, argv, { cwd: ROOT, env, encoding: 'utf8', maxBuffer: 1 << 26 });
  if (r.status !== 0) { console.log('FAILED'); console.error((r.stdout || '') + (r.stderr || '')); process.exit(1); }
  console.log('ok');
  return r.stdout || '';
};
const node = (script) => run('node', [join('tools', script)], `${script.replace('.mjs', '')}`);

const disasmStats = (out) => Object.fromEntries(out.split('\n').map((l) => l.split(':').map((s) => s.trim())).filter((p) => p.length === 2 && p[1]));
const build = () => {
  if (!existsSync(join(BUILD_DIR, 'build.ninja'))) {
    run('cmake', ['-S', '.', '-B', BUILD_DIR, '-G', 'Ninja', `-DGAME=${game}`, '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++'], 'configure');
  }
  run('cmake', ['--build', BUILD_DIR], 'build');
};

// ---- The input script: Start through the title and menus, then seeded random play --------
function script(frames, seed) {
  let s = seed >>> 0;
  const rnd = (n) => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s % n; };
  const presses = [];
  for (let f = 600; f < 4000; f += 150) presses.push(`${f}:S:4`);          // logos, title, menus
  for (let f = 4000; f < frames; ) {
    const dir = ['U', 'D', 'L', 'R', 'UL', 'UR', 'DL', 'DR', 'R', 'R'][rnd(10)];
    const len = 20 + rnd(80);
    presses.push(`${f}:${dir}:${len}`);
    for (let k = 0; k < 3; k++) presses.push(`${f + rnd(len)}:${'ABC'[rnd(3)]}:${3 + rnd(6)}`);
    if (rnd(40) === 0) presses.push(`${f + len}:S:4`);                       // menus, continues
    f += len + rnd(10);
  }
  return presses.join(',');
}
const play = (frames, seed, extra = []) => {
  const r = spawnSync(exe, [rom, '--headless', String(frames), '--press', script(frames, seed), ...extra],
    { cwd: BUILD_DIR, env, encoding: 'utf8', maxBuffer: 1 << 26 });
  const native = /68000 time: ([\d.]+)% native/.exec(r.stdout || '');
  return { ok: r.status === 0, native: native ? Number(native[1]) : null, out: (r.stdout || '') + (r.stderr || '') };
};

// ---- 1-3: build, play, recompile with what was found ---------------------------------------
console.log(`${game}: ${GAME_INFO.title}`);
let stats = disasmStats(node('disasm.mjs'));
build();
// Each round plays differently (its own seed), so it can reach new parts of the game; what
// it found is recompiled in, and the rounds stop once one finds no new code.
for (let round = 1; round <= ROUNDS; round++) {
  process.stdout.write(`round ${round}: playing ${FRAMES} frames... `);
  const p = play(FRAMES, 1234 + round);
  if (!p.ok) { console.log('FAILED'); console.error(p.out); process.exit(1); }
  console.log(`${p.native}% native`);
  const before = stats.blocks;
  stats = disasmStats(node('disasm.mjs'));
  if (stats.blocks === before) break;                                       // nothing new found
  build();
}

// ---- 4: native against the interpreter (and the native share, on a fixed replay) ------------
// Widescreen off: it swaps some of the game's routines for C versions in the native build
// only, so with it on the two are meant to differ.
const verifyFrames = Math.min(FRAMES, 6000);
process.stdout.write(`verify: ${verifyFrames} frames against the interpreter... `);
const a = join(BUILD_DIR, 'verify_native.log'), b = join(BUILD_DIR, 'verify_interp.log');
const native = play(verifyFrames, 1235, ['--hash-log', a, '--widescreen', '0']).native;
play(verifyFrames, 1235, ['--hash-log', b, '--interpret', '--widescreen', '0']);
const la = readFileSync(a, 'utf8').split('\n'), lb = readFileSync(b, 'utf8').split('\n');
let firstDiff = -1;
for (let k = 0; k < Math.max(la.length, lb.length); k++) if (la[k] !== lb[k]) { firstDiff = k; break; }
console.log(firstDiff < 0 ? 'identical' : `DIFFERENT from frame ${firstDiff}`);

// ---- 5: screenshots and the report ----------------------------------------------------------
rmSync(reportDir, { recursive: true, force: true });
mkdirSync(reportDir, { recursive: true });
play(Math.min(FRAMES, 9000), 1235, ['--shots', '1500', reportDir]);
const shots = readdirSync(reportDir).filter((f) => f.endsWith('.png')).sort();
const report = [
  `# ${GAME_INFO.title}: recompilation report`,
  '',
  '| | |',
  '|---|---|',
  `| ROM | ${GAME_INFO.rom.name}, ${GAME_INFO.rom.size >> 10} KB, SHA-1 \`${GAME_INFO.rom.sha1}\` |`,
  `| Code found | ${stats['functions']} functions, ${stats['blocks']} blocks, ${stats['code bytes']} |`,
  `| Jumps not followed statically | ${stats['unresolved indirect jumps']} (their code is found by playing) |`,
  `| 68000 time running native | ${native}% (over the first ${verifyFrames} frames) |`,
  `| Against the interpreter | ${firstDiff < 0 ? `identical over ${verifyFrames} frames` : `**differs from frame ${firstDiff}**: see verify_*.log`} |`,
  `| Game-specific features | ${Object.entries(GAME_INFO.features).map(([k, v]) => `${k} ${v ? 'on' : 'off'}`).join(', ')} |`,
  '',
  '## Screenshots (every 1500 frames of the coverage play)',
  '',
  ...shots.map((f) => `![frame ${parseInt(f, 10)}](${f})`),
  '',
].join('\n');
writeFileSync(join(reportDir, 'report.md'), report);
console.log(`report: ${join(reportDir, 'report.md')}`);
console.log(`play it: ${exe}`);
process.exit(firstDiff < 0 ? 0 : 2);
